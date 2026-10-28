/* MODULE B — SIGNAUX DES SERVOS VIA LA PCA9685.
 * Ce fichier gère le protocole I²C et les conversions ; il ne mesure ni la
 * position réelle ni le courant. La puissance des servos vient d'une alimentation séparée.
 */
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include "servo.h"

/* PCA9685 : l'oscillateur interne vaut nominalement 25 MHz.
 * Le prescaler 121 divise cette horloge par 122, et un cycle compte 4096 pas.
 * Fréquence : 25 000 000 / (122*4096) ≈ 50,0288 Hz.
 * Période nominale : 19 988,48 µs. L'horloge réelle peut différer : mesurer
 * les impulsions sur le matériel avant de conclure à une précision absolue. */
/* Envoi d'un registre suivi de ses valeurs, dans une seule transaction I²C.
 * fd < 0 est la convention de simulation : valider sans écrire physiquement. */
static int registers(Pca9685 *pca, const uint8_t *data, size_t n)
{
    if(pca->fd<0) return 0;
    ssize_t r;
    /* EINTR signifie qu'un signal a interrompu l'appel avant sa réussite.
     * On retente alors. Une écriture partielle est une erreur, car on ne peut
     * pas reprendre au milieu d'un bloc I²C comme dans un fichier ordinaire. */
    do { r=write(pca->fd,data,n); } while(r<0 && errno==EINTR);
    if(r!=(ssize_t)n) { if(r>=0) errno=EIO; perror("PCA9685 write"); return -1; }
    return 0;
}

int pca9685_off(Pca9685 *pca)
{
    /* 0xFA..0xFD : registres ALL_LED, appliqués à toutes les voies.
     * Le bit 4 du dernier octet impose l'arrêt complet des impulsions.
     * L'alimentation des servos reste présente : seul leur signal est coupé. */
    const uint8_t data[]={0xFA,0,0,0,0x10};
    return registers(pca,data,sizeof data);
}

int pca9685_open(Pca9685 *pca, const char *dev, int addr)
{
    /* Initialiser avant tout échec possible : close ne recevra pas un fd inconnu. */
    pca->fd=-1;
    if(addr<0x08 || addr>0x77) return -1;
    if(!dev) return 0;
    /* Sous Linux, le bus I²C s'ouvre comme un fichier spécial.
     * O_CLOEXEC empêche de transmettre ce descripteur à un programme exécuté ensuite. */
    pca->fd=open(dev,O_RDWR|O_CLOEXEC);
    if(pca->fd<0) { perror(dev); return -1; }
    /* ioctl configure ici l'adresse du périphérique visé sur le bus partagé. */
    if(ioctl(pca->fd,I2C_SLAVE,addr)<0) { perror("I2C_SLAVE"); goto fail; }
    /* Séquence imposée par le composant :
     * 1. MODE1 = sommeil + incrément automatique des adresses de registre.
     * 2. PRE_SCALE = 121 ; la fréquence se règle pendant le sommeil.
     * 3. MODE2 = sorties actives haut/bas, adaptées au signal servo.
     * 4. Couper toutes les voies puis réveiller l'oscillateur.
     * Les valeurs hexadécimales viennent de la fiche technique NXP. */
    const uint8_t sleep_mode[]={0x00,0x30}, freq[]={0xFE,121};
    const uint8_t output_mode[]={0x01,0x04}, wake[]={0x00,0x20};
    if(registers(pca,sleep_mode,2) || registers(pca,freq,2) ||
       registers(pca,output_mode,2) || pca9685_off(pca) || registers(pca,wake,2)) goto fail;
    /* Attendre 1 ms pour que l'oscillateur se stabilise (minimum requis : 500 µs).
     * nanosleep fournit le temps restant si un signal interrompt cette attente. */
    struct timespec delay={.tv_nsec=1000000};
    while(nanosleep(&delay,&delay)<0 && errno==EINTR) {}
    /* Bit RESTART + incrément automatique : lancer les compteurs PWM. */
    const uint8_t restart[]={0x00,0xA0};
    if(registers(pca,restart,2)) goto fail;
    return 0;
fail:
    /* Point commun aux erreurs d'initialisation : ne pas laisser le fd ouvert. */
    pca9685_close(pca); return -1;
}

int pca9685_set_pulse_us(Pca9685 *pca, int channel, int pulse_us)
{
    if(channel<0 || channel>15 || pulse_us<400 || pulse_us>2600) return -1;
    /* Un pas de compteur dure 122/25 µs. Arrondir au pas le plus proche.
     * Exemple : 1500 µs correspondent à environ 307 pas sur 4096. */
    int ticks=(int)lround(pulse_us*25.0/122.0);
    /* Chaque voie possède quatre registres, à partir de 0x06 + 4*voie :
     * instant ON sur 12 bits, puis instant OFF sur 12 bits.
     * ON=0 ; OFF=ticks. On envoie d'abord les huit bits faibles, puis les quatre
     * bits hauts : &255 et &15 extraient ces morceaux de l'entier. */
    const uint8_t data[]={(uint8_t)(0x06+4*channel),0,0,
        (uint8_t)(ticks&255),(uint8_t)((ticks>>8)&15)};
    return registers(pca,data,sizeof data);
}

void pca9685_close(Pca9685 *pca)
{
    /* Fermer un fd ne coupe pas la PCA9685 : main.c appelle off auparavant. */
    if(pca->fd>=0) close(pca->fd);
    pca->fd=-1;
}

int servo_write(Pca9685 *pca, Servo *s, float deg)
{
    /* Valider avant les divisions et conversions entières : NaN et une course
     * nulle rendraient le calcul des impulsions invalide. */
    if(!isfinite(deg) || !isfinite(s->range_deg) || s->range_deg<=0 ||
       !isfinite(s->min_deg) || !isfinite(s->max_deg) ||
       s->min_deg<0 || s->max_deg>s->range_deg || s->min_deg>s->max_deg ||
       s->min_us<400 || s->max_us>2600 || s->min_us>=s->max_us) return -1;
    /* Les butées protègent le montage indépendamment du contrôleur.
     * La conversion est une interpolation linéaire :
     * impulsion = minimum + fraction_de_course * amplitude_des_impulsions. */
    deg=fmaxf(s->min_deg,fminf(s->max_deg,deg));
    int pulse=(int)lroundf(s->min_us+deg/s->range_deg*(s->max_us-s->min_us));
    if(pca9685_set_pulse_us(pca,s->channel,pulse)) return -1;
    /* Mémoriser seulement une commande envoyée avec succès. En simulation,
     * l'envoi est virtuel, mais les validations et conversions sont les mêmes. */
    s->angle=deg;
    return 0;
}
