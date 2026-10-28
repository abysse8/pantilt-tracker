/* TEST DU PROTOCOLE I²C AVEC APPELS SYSTÈME SIMULÉS.
 * L'éditeur de liens remplace open/ioctl/write/close/nanosleep par nos fonctions
 * __wrap_*. Le pilote reste inchangé, mais on observe les octets qu'il envoie.
 * Ce test valide le protocole demandé, pas la réaction d'une vraie carte. */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <linux/i2c-dev.h>
#include "config.h"

/* Mémoriser les transactions pour inspecter leur contenu et leur ordre.
 * Le descripteur fictif 88 identifie exclusivement notre faux périphérique. */
static uint8_t sent[64][8];
static size_t sizes[64];
static int writes, closes, fail_ioctl, fail_write, short_write;

/* L'éditeur de liens permet encore d'appeler les vraies fonctions avec
 * le préfixe __real_ : utile aux fichiers internes de l'outil de couverture. */
int __real_open(const char *path,int flags,...);
ssize_t __real_write(int fd,const void *buffer,size_t n);
int __real_close(int fd);

int __wrap_open(const char *path, int flags, ...)
{
    if(!strcmp(path,"/mock/i2c")) return 88;
    /* Les outils de couverture écrivent aussi des fichiers : laisser passer
     * leurs appels au système, et ne simuler que /mock/i2c. */
    /* open est variadique : le paramètre mode n'existe qu'en cas de création.
     * va_list permet de récupérer cet argument facultatif sans changer l'interface. */
    if(flags&O_CREAT) {
        va_list args; va_start(args,flags); int mode=va_arg(args,int); va_end(args);
        return __real_open(path,flags,mode);
    }
    return __real_open(path,flags);
}
int __wrap_ioctl(int fd, unsigned long request, ...)
{
    /* Vérifier que le pilote sélectionne l'adresse I²C attendue. */
    assert(fd==88 && request==I2C_SLAVE);
    va_list args; va_start(args,request); assert(va_arg(args,int)==0x40); va_end(args);
    if(fail_ioctl) { errno=EIO; return -1; } return 0;
}
ssize_t __wrap_write(int fd,const void *buffer,size_t n)
{
    if(fd!=88) return __real_write(fd,buffer,n);
    assert(writes<64 && n<=8);
    /* Injection de pannes : simuler une erreur franche ou une écriture partielle.
     * Les deux doivent remonter jusqu'à l'appelant du pilote. */
    if(fail_write) { errno=EIO; return -1; }
    if(short_write) return (ssize_t)n-1;
    /* Sans panne, enregistrer les octets au lieu de les envoyer physiquement. */
    memcpy(sent[writes],buffer,n); sizes[writes++]=n;
    return (ssize_t)n;
}
int __wrap_close(int fd) { if(fd!=88) return __real_close(fd); closes++; return 0; }
int __wrap_nanosleep(const struct timespec *req, struct timespec *rem)
{
    (void)rem; assert(req->tv_nsec>=500000); return 0;
}

int main(void)
{
    Pca9685 p;
    assert(pca9685_open(&p,"/mock/i2c",0x40)==0);
    /* L'initialisation exige six transactions, dans cet ordre précis. */
    assert(writes==6);
    assert(sent[0][0]==0 && sent[0][1]==0x30); /* Sommeil et incrément automatique. */
    assert(sent[1][0]==0xFE && sent[1][1]==121);
    assert(sent[2][0]==1 && sent[2][1]==4);
    assert(sizes[3]==5 && sent[3][0]==0xFA && sent[3][4]==0x10); /* Toutes les voies arrêtées. */
    assert(sent[4][0]==0 && sent[4][1]==0x20);
    assert(sent[5][0]==0 && sent[5][1]==0xA0);
    assert(pca9685_set_pulse_us(&p,1,1500)==0);
    assert(sizes[6]==5 && sent[6][0]==0x0A && sent[6][1]==0 &&
        sent[6][2]==0 && sent[6][3]==0x33 && sent[6][4]==1); /* 307 pas : 1500 µs avec le réglage nominal. */
    /* Le centre d'une course 270° est 135° : avec 500..2500 µs,
     * on attend aussi 1500 µs. Vérifier la conversion angle → impulsion. */
    Config c; config_defaults(&c);
    assert(servo_write(&p,&c.pan,135)==0);
    assert(sent[7][0]==6 && sent[7][3]==0x33 && sent[7][4]==1);
    assert(pca9685_off(&p)==0 && sent[8][4]==0x10);
    /* Après une erreur, l'angle mémorisé doit rester la dernière commande réussie.
     * Les autres scénarios vérifient la fermeture du bus si l'ouverture échoue. */
    fail_write=1; float previous=c.pan.angle;
    assert(servo_write(&p,&c.pan,140)==-1 && c.pan.angle==previous);
    pca9685_close(&p); assert(closes==1 && p.fd==-1);
    assert(pca9685_open(&p,"/mock/i2c",0x40)==-1 && p.fd==-1 && closes==2);
    fail_write=0; fail_ioctl=1;
    assert(pca9685_open(&p,"/mock/i2c",0x40)==-1 && p.fd==-1 && closes==3);
    fail_ioctl=0; short_write=1;
    assert(pca9685_open(&p,"/mock/i2c",0x40)==-1 && p.fd==-1 && closes==4);
    assert(pca9685_open(&p,NULL,0x80)==-1);
    puts("servo register protocol + injected I/O errors: OK (mock hardware)");
}
