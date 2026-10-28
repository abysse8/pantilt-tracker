/* TESTS DES MODULES A, B ET D.
 * Les échecs volontairement provoqués affichent des messages : c'est normal.
 * Un assert qui échoue, en revanche, interrompt le programme avec un code d'erreur.
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "log.h"

static void vision_tests(void)
{
    /* Préparer une petite image, puis vérifier l'interdiction d'une largeur impaire. */
    Frame f={0};
    assert(frame_alloc(&f,15,16)==-1);
    assert(frame_alloc(&f,64,32)==0);
    /* Couleur neutre partout, luminosité faible : aucune cible au départ. */
    memset(f.buf,128,f.size); memset(f.y,40,64*32);
    VisionParams p={.y_min=200,.u_max=255,.v_max=255,.min_area=4,.step=1};
    Target t;
    assert(vision_detect(&f,&p,&t)==0 && !t.found);
    /* Un rectangle de 24 pixels et un distracteur isolé : le détecteur doit
     * sélectionner le rectangle et calculer son centre, pas celui de l'ensemble. */
    for(int y=10;y<14;y++) for(int x=20;x<26;x++) f.y[y*64+x]=255;
    f.y[0]=255;
    assert(vision_detect(&f,&p,&t)==0 && t.found && t.area==24);
    assert(fabsf(t.x-22.5f)<0.001f && fabsf(t.y-11.5f)<0.001f);
    /* Activer les seuils couleur doit rejeter le gris neutre. Modifier ensuite
     * U et V pour faire entrer le rectangle dans les intervalles acceptés. */
    p.use_color=1; p.u_min=80; p.u_max=110; p.v_min=180; p.v_max=240;
    assert(vision_detect(&f,&p,&t)==0 && !t.found);
    memset(f.u,100,64*32/4); memset(f.v,220,64*32/4);
    assert(vision_detect(&f,&p,&t)==0 && t.found);
    /* Un point sur deux par axe divise ici la surface échantillonnée par quatre.
     * Le seuil de surface s'applique à ce nombre réduit, pas aux pixels d'origine. */
    p.step=2;
    assert(vision_detect(&f,&p,&t)==0 && t.area==6);
    p.min_area=7;
    assert(vision_detect(&f,&p,&t)==0 && !t.found);
    p.step=0;
    assert(vision_detect(&f,&p,&t)==-1);
    /* tmpfile fournit un fichier temporaire automatiquement supprimé à la fermeture.
     * Tester une image complète, la fin normale du fichier puis une image tronquée. */
    FILE *fp=tmpfile(); assert(fp);
    assert(fwrite(f.buf,1,f.size,fp)==f.size); rewind(fp);
    assert(frame_read(fp,&f)==1 && frame_read(fp,&f)==0); fclose(fp);
    fp=tmpfile(); assert(fp); fputc(1,fp); rewind(fp);
    assert(frame_read(fp,&f)==-1); fclose(fp);
    frame_free(&f); frame_free(&f);
}

/* Créer un vrai fichier à partir d'une petite chaîne permet de tester
 * le parseur public, y compris fopen/fgets, sans conserver de fichiers de test. */
static int load_text(const char *text, Config *cfg)
{
    /* mkstemp remplace XXXXXX par un nom unique et ouvre le fichier sans collision. */
    char path[]="/tmp/pantilt-test-XXXXXX";
    int fd=mkstemp(path); assert(fd>=0);
    FILE *fp=fdopen(fd,"w"); assert(fp);
    assert(fputs(text,fp)>=0 && fclose(fp)==0);
    int result=config_load(path,cfg); unlink(path); return result;
}

static void config_tests(void)
{
    Config c; config_defaults(&c);
    /* Vérifier aussi que le fichier d'exemple livré dans le dépôt est valide. */
    assert(config_load("tracker.conf",&c)==0);
    assert(load_text("# test\nwidth = 128 # aligned\ny_min=210\nkp = 1.5\ni2c_addr = 0x41\nlog_path =\n",&c)==0);
    assert(c.width==128 && c.vision.y_min==210 && c.control.kp==1.5f && c.i2c_addr==0x41 && !c.log_path[0]);
    /* Une entrée par famille d'erreurs : clé inconnue, dimensions, NaN,
     * suffixe parasite, direction nulle, adresse, voies identiques, butées,
     * seuils inversés, dépassement numérique, séparateur et chemin invalide. */
    const char *bad[]={"unknown=1\n","width=63\n","height=3\n","kp=nan\n","kp=2x\n",
      "step=0\n","pan_dir=0\n","i2c_addr=0x80\n","pan_channel=1\n",
      "pan_min_deg=160\n","pan_min_us=3000\n","u_min=255\nu_max=0\n",
      "width=9999999999999999999\n","width 640\n","log_path = a b\n"};
    for(size_t k=0;k<sizeof bad/sizeof *bad;k++) {
        config_defaults(&c); Config old=c;
        assert(load_text(bad[k],&c)==-1);
        /* L'échec ne doit modifier aucun champ : chargement transactionnel. */
        assert(memcmp(&c,&old,sizeof c)==0);
    }
    assert(config_load("/does/not/exist",&c)==-1);
}

static void servo_tests(void)
{
    /* Simulation du bus, mais vraies bornes et vraies validations du pilote. */
    Config c; config_defaults(&c); Pca9685 p;
    assert(pca9685_open(&p,NULL,0x40)==0);
    /* Des angles hors course sont bornés ; une valeur NaN est refusée. */
    assert(servo_write(&p,&c.pan,-50)==0 && c.pan.angle==c.pan.min_deg);
    assert(servo_write(&p,&c.pan,999)==0 && c.pan.angle==c.pan.max_deg);
    assert(servo_write(&p,&c.pan,NAN)==-1);
    /* Les paramètres invalides doivent échouer même sans périphérique réel. */
    assert(pca9685_set_pulse_us(&p,16,1500)==-1);
    assert(pca9685_set_pulse_us(&p,0,399)==-1);
    assert(pca9685_set_pulse_us(&p,0,1500)==0);
    assert(pca9685_off(&p)==0); pca9685_close(&p);
    assert(pca9685_open(&p,"/does/not/exist",0x40)==-1 && p.fd==-1);
}

static void log_tests(void)
{
    /* Tester d'abord le journal désactivé, puis le contenu exact d'une ligne CSV. */
    Logger l; Target t={.found=true,.x=12,.y=16,.area=8}; ControlState s={.pan=135,.tilt=90};
    assert(log_open(&l,"")==0);
    assert(log_row(&l,0,0,&t,&s)==0 && log_close(&l)==0);
    l.fp=tmpfile(); assert(l.fp);
    assert(log_row(&l,2,10,&t,&s)==0); rewind(l.fp);
    char line[256]; assert(fgets(line,sizeof line,l.fp));
    assert(strstr(line,"2,10.000,1,12.000,16.000,8,135.000,90.000"));
    /* La seconde fermeture doit rester sans effet : éviter les doubles fclose. */
    assert(log_close(&l)==0 && log_close(&l)==0);
    assert(log_open(&l,"/does/not/exist/log")==-1);
}

int main(void)
{
    vision_tests(); config_tests(); servo_tests(); log_tests(); puts("modules: OK");
}
