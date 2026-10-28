/* MODULE D — POINT D'ENTRÉE ET INTÉGRATION.
 * Le programme reçoit des images brutes sur stdin, par exemple depuis rpicam-vid.
 * Il réunit les modules sans confondre calcul de commande et accès au matériel.
 * Exemple sans matériel : ./bin/tracker -n -r 30 -c tracker.conf < video.yuv
 */
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "config.h"
#include "log.h"

/* Le gestionnaire de signal ne doit faire qu'une opération simple.
 * sig_atomic_t permet de partager ce drapeau avec le code interrompu ;
 * volatile oblige le programme à relire sa valeur dans la boucle. */
static volatile sig_atomic_t stop_requested;
static void on_signal(int sig) { (void)sig; stop_requested=1; }
/* Horloge monotone : elle mesure une durée sans sauter lors d'une correction
 * de l'heure système. Les nanosecondes sont converties en secondes décimales. */
static double now_s(void)
{
    struct timespec ts;
    if(clock_gettime(CLOCK_MONOTONIC,&ts)) { perror("clock_gettime"); exit(1); }
    return ts.tv_sec+ts.tv_nsec/1e9;
}

static void usage(const char *prog)
{
    fprintf(stderr,"Usage: %s [-n | -a] [-c config] [-r replay_fps]\n"
        "  -n simulation, no I2C (default)\n"
        "  -a enable physical servos; verified configuration required\n"
        "  -r fixed replay time step (1..120 fps); requires -n\n"
        "Input: packed planar YUV420 on stdin. Ctrl-C disables PWM.\n",prog);
}

int main(int argc,char **argv)
{
    /* Charger les valeurs par défaut avant les éventuelles valeurs du fichier. */
    Config cfg; config_defaults(&cfg);
    const char *config_path=NULL;
    int armed=0, opt; float replay_fps=0;
    /* getopt lit les options de la ligne de commande.
     * Un deux-points dans « c:nar:h » signifie que c/r attendent un argument.
     * Sans -a, les mouvements sont uniquement calculés, jamais envoyés sur I²C. */
    while((opt=getopt(argc,argv,"c:nar:h"))!=-1) {
        switch(opt) {
        case 'c': config_path=optarg; break;
        case 'n': armed=0; break;
        case 'a': armed=1; break;
        /* Un fichier se lit beaucoup plus vite qu'une caméra. En rejeu,
         * l'utilisateur donne donc la cadence d'enregistrement, pas celle du PC. */
        case 'r': {
            char *end; errno=0; replay_fps=strtof(optarg,&end);
            if(errno || end==optarg || *end || !isfinite(replay_fps) || replay_fps<1 || replay_fps>120) {
                usage(argv[0]); return 2;
            }
            break;
        }
        default: usage(argv[0]); return opt=='h' ? 0 : 2;
        }
    }
    /* Refuser les arguments inattendus et le rejeu rapide avec servos actifs. */
    if(optind!=argc || (armed && replay_fps)) { usage(argv[0]); return 2; }
    if(config_path && config_load(config_path,&cfg)) return 1;
    /* SIGINT = Ctrl-C ; SIGTERM = demande d'arrêt du système ou d'un test.
     * Le gestionnaire pose seulement le drapeau ; la fermeture se fait ensuite
     * dans le chemin normal, avec vérification des erreurs. */
    struct sigaction sa={.sa_handler=on_signal}; sigemptyset(&sa.sa_mask);
    if(sigaction(SIGINT,&sa,NULL) || sigaction(SIGTERM,&sa,NULL)) { perror("sigaction"); return 1; }
    /* Ouvrir les ressources dans cet ordre : image, journal, carte.
     * À chaque échec, refermer celles déjà ouvertes avant de quitter. */
    Frame frame;
    if(frame_alloc(&frame,cfg.width,cfg.height)) { fprintf(stderr,"Frame allocation failed\n"); return 1; }
    Logger lg;
    if(log_open(&lg,cfg.log_path)) { frame_free(&frame); return 1; }
    Pca9685 pca;
    if(pca9685_open(&pca,armed?cfg.i2c_dev:NULL,cfg.i2c_addr)) {
        log_close(&lg); frame_free(&frame); return 1;
    }
    fprintf(stderr,"%s\n",armed?"HARDWARE: servo commands enabled":"SIMULATION: no I2C writes");
    /* État initial : centres déclarés par la configuration. Une première
     * commande physique peut déplacer le support ; vérifier le montage avant -a. */
    ControlState st={.pan=cfg.pan.center_deg,.tilt=cfg.tilt.center_deg};
    int status=0; long n=0;
    double start=now_s(), previous=start;
    /* poll observe le descripteur Linux, pas le tampon de FILE. Désactiver
     * ce tampon évite d'attendre alors que fread possède déjà des octets.
     * Le producteur vidéo reste un programme séparé relié par un tube. */
    setvbuf(stdin,NULL,_IONBF,0);
    if(stop_requested) goto cleanup;
    if(servo_write(&pca,&cfg.pan,st.pan) || servo_write(&pca,&cfg.tilt,st.tilt)) {
        status=1; goto cleanup;
    }
    /* BOUCLE D'UNE IMAGE : attendre → lire → détecter → calculer → envoyer → noter. */
    while(!stop_requested) {
        /* Attendre au maximum 100 ms avant de revérifier la demande d'arrêt.
         * Cela permet de quitter même si la caméra ne produit aucune image. */
        struct pollfd input={.fd=STDIN_FILENO,.events=POLLIN};
        int ready=poll(&input,1,100);
        if(ready<0) { if(errno==EINTR) continue; perror("poll"); status=1; break; }
        if(!ready) continue;
        /* Une fin normale de flux quitte la boucle sans erreur ; une image
         * incomplète est refusée, sauf si elle accompagne une demande d'arrêt. */
        int r=frame_read(stdin,&frame);
        if(r==0) break;
        if(r<0) {
            if(!stop_requested) { fprintf(stderr,"Truncated/unreadable frame %ld\n",n); status=1; }
            break;
        }
        if(stop_requested) break;
        double stamp=now_s();
        /* Une grande pause caméra ne doit pas devenir un grand saut d'angle.
         * En direct, limiter dt à 0,1 s ; en rejeu, utiliser le pas fixe demandé. */
        float dt=replay_fps ? 1/replay_fps : (float)fmin(stamp-previous,0.1);
        previous=stamp;
        /* La cible est recalculée à chaque image, puis le contrôleur met à jour
         * les consignes. La vérification du signal évite un envoi après arrêt. */
        Target target;
        if(vision_detect(&frame,&cfg.vision,&target)) { fprintf(stderr,"Detection failed\n"); status=1; break; }
        control_update(&cfg.control,&target,cfg.width,cfg.height,dt,&st);
        if(stop_requested) break;
        if(servo_write(&pca,&cfg.pan,st.pan) || servo_write(&pca,&cfg.tilt,st.tilt)) { status=1; break; }
        /* servo_write borne les consignes. Réinjecter ces valeurs dans l'état
         * évite d'accumuler une consigne imaginaire au-delà d'une butée. */
        st.pan=cfg.pan.angle; st.tilt=cfg.tilt.angle;
        /* En rejeu, le temps du journal est celui de la vidéo ; en direct,
         * c'est le temps mesuré depuis le démarrage du programme. */
        double ms=replay_fps ? n*1000.0/replay_fps : (stamp-start)*1000;
        if(log_row(&lg,n,ms,&target,&st)) { fprintf(stderr,"CSV write failed\n"); status=1; break; }
        n++;
    }
cleanup:
    /* CHEMIN DE SORTIE COMMUN : arrêter les signaux puis libérer les ressources.
     * Ne pas recentrer : cela pourrait faire balayer un obstacle par le support.
     * La coupure des PWM peut relâcher le montage ; ce n'est pas un arrêt matériel.
     * Une erreur pendant la fermeture doit aussi rendre le code de sortie non nul. */
    if(pca9685_off(&pca)) status=1;
    pca9685_close(&pca);
    if(log_close(&lg)) status=1;
    frame_free(&frame);
    fprintf(stderr,"%ld frames processed; PWM disabled on exit\n",n);
    return status;
}
