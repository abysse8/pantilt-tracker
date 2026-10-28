/* MODULE D — LECTURE ET VALIDATION DU FICHIER DE CONFIGURATION.
 * Format volontairement simple : une clé et une valeur par ligne, sans JSON.
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

/* Point de départ commun : tout paramètre absent du fichier garde sa valeur.
 * Les exemples de servos restent à confirmer sur les références du matériel. */
void config_defaults(Config *c)
{
    /* memset initialise aussi les chaînes vides et les champs d'état.
     * Les littéraux composés ci-dessous remplissent les structures imbriquées. */
    memset(c,0,sizeof *c);
    c->width=640; c->height=480;
    strcpy(c->i2c_dev,"/dev/i2c-1"); c->i2c_addr=0x40;
    c->vision=(VisionParams){.y_min=240,.u_max=255,.v_max=255,.min_area=20,.step=2};
    c->control=(ControlParams){.kp=2,.hfov_deg=66,.vfov_deg=41,
        .deadband_px=8,.max_speed_deg_s=20,.pan_dir=1,.tilt_dir=1};
    /* Exemples de correspondance angle/impulsion, pas des mesures de vos servos.
     * Les butées ne permettent ici que ±10° autour d'un centre illustratif. */
    c->pan=(Servo){.channel=0,.range_deg=270,.min_us=500,.max_us=2500,
        .min_deg=125,.max_deg=145,.center_deg=135};
    c->tilt=(Servo){.channel=1,.range_deg=180,.min_us=500,.max_us=2500,
        .min_deg=80,.max_deg=100,.center_deg=90};
}

/* Table de description des clés, pour éviter une longue suite de strcmp.
 * offset = décalage du champ depuis le début de Config, en octets.
 * type = entier ('i'), flottant ('f') ou chaîne ('s').
 * low/high donnent l'intervalle numérique ; high donne la capacité d'une chaîne. */
typedef struct { const char *key; size_t offset; char type; double low,high; } Field;
/* offsetof est un outil standard C : le compilateur calcule la position
 * du champ, y compris dans une sous-structure comme vision.y_min.
 * sizeof n'évalue pas le pointeur fictif ((Config *)0) : il mesure seulement
 * le type du tableau. Aucun accès mémoire à l'adresse zéro n'a lieu. */
#define I(key,member,lo,hi) {key,offsetof(Config,member),'i',lo,hi}
#define F(key,member,lo,hi) {key,offsetof(Config,member),'f',lo,hi}
#define S(key,member) {key,offsetof(Config,member),'s',0,sizeof(((Config *)0)->member)}
static const Field fields[]={
    I("width",width,64,4096), I("height",height,2,2160),
    I("i2c_addr",i2c_addr,8,0x77), S("i2c_dev",i2c_dev), S("log_path",log_path),
    I("use_color",vision.use_color,0,1), I("y_min",vision.y_min,0,255),
    I("u_min",vision.u_min,0,255), I("u_max",vision.u_max,0,255),
    I("v_min",vision.v_min,0,255), I("v_max",vision.v_max,0,255),
    I("min_area",vision.min_area,1,4096*2160), I("step",vision.step,1,16),
    F("kp",control.kp,0.001,20), F("hfov_deg",control.hfov_deg,1,179),
    F("vfov_deg",control.vfov_deg,1,179), F("deadband_px",control.deadband_px,0,1000),
    F("max_speed_deg_s",control.max_speed_deg_s,0.1,90),
    I("pan_dir",control.pan_dir,-1,1), I("tilt_dir",control.tilt_dir,-1,1),
/* La macro AXIS décrit les mêmes sept paramètres pour pan et tilt.
 * #a transforme le nom passé à la macro en texte ; les chaînes côte à côte
 * sont concaténées par C, par exemple "pan" "_channel" devient "pan_channel". */
#define AXIS(a) \
    I(#a "_channel",a.channel,0,15), F(#a "_range_deg",a.range_deg,1,360), \
    I(#a "_min_us",a.min_us,400,2600), I(#a "_max_us",a.max_us,400,2600), \
    F(#a "_min_deg",a.min_deg,0,360), F(#a "_max_deg",a.max_deg,0,360), \
    F(#a "_center_deg",a.center_deg,0,360)
    AXIS(pan), AXIS(tilt)
};

/* Supprime les espaces au début et à la fin, directement dans la ligne.
 * Le pointeur renvoyé peut viser l'intérieur du tableau d'origine.
 * isspace reçoit un unsigned char pour éviter les valeurs négatives interdites. */
static char *trim(char *s)
{
    while(isspace((unsigned char)*s)) s++;
    char *end=s+strlen(s);
    while(end>s && isspace((unsigned char)end[-1])) *--end='\0';
    return s;
}

/* Les bornes de chaque champ ne suffisent pas : leur ordre doit être cohérent.
 * La position initiale doit être comprise dans les butées et la course déclarée. */
static int valid_servo(const Servo *s)
{
    return s->min_us<s->max_us && s->min_deg<=s->center_deg &&
        s->center_deg<=s->max_deg && s->max_deg<=s->range_deg;
}

int config_load(const char *path, Config *cfg)
{
    FILE *fp=fopen(path,"r");
    if(!fp) { perror(path); return -1; }
    /* Travailler sur une copie locale : une erreur à la dernière ligne ne doit
     * pas laisser la moitié de la configuration appliquée. */
    Config candidate=*cfg;
    char line[512]; int number=0; const char *reason=NULL;
    /* Une case par clé : refuser les doublons évite une valeur discrètement
     * remplacée plus bas dans un long fichier. sizeof donne la taille du tableau. */
    unsigned char seen[sizeof fields/sizeof *fields]={0};
    while(fgets(line,sizeof line,fp)) {
        number++;
        /* Une ligne qui dépasse le tampon est refusée au lieu d'être coupée
         * et interprétée comme deux paramètres différents. */
        if(!strchr(line,'\n') && !feof(fp)) { reason="ligne trop longue"; break; }
        /* Autoriser les commentaires après une valeur : y_min=240 # lampe.
         * strtok n'est pas nécessaire : on remplace les séparateurs par '\0'. */
        char *comment=strchr(line,'#'); if(comment) *comment=0;
        char *key=trim(line); if(!*key) continue;
        char *eq=strchr(key,'=');
        if(!eq) { reason="attendu cle = valeur"; break; }
        *eq=0; key=trim(key); char *value=trim(eq+1);
        /* Retrouver la description de la clé dans la table. Les noms sont
         * sensibles à la casse : une faute de frappe est une erreur explicite. */
        size_t j;
        for(j=0;j<sizeof fields/sizeof *fields;j++) if(!strcmp(fields[j].key,key)) break;
        if(j==sizeof fields/sizeof *fields) { reason="cle inconnue"; break; }
        if(seen[j]++) { reason="cle dupliquee"; break; }
        /* Un pointeur char* avance d'un octet : base + offset atteint le champ.
         * Son type réel sera rétabli uniquement après validation de la valeur. */
        const Field *f=&fields[j]; char *dest=(char *)&candidate+f->offset;
        /* La capacité inclut l'octet final '\0'. Dans cette version simple,
         * les chemins avec espaces ne sont pas pris en charge. */
        if(f->type=='s') {
            if(strlen(value)>=(size_t)f->high || strpbrk(value," \t\r\n")) {
                reason="chemin trop long ou contenant des espaces"; break;
            }
            strcpy(dest,value); continue;
        }
        /* strtol/strtod indiquent où la lecture s'arrête grâce à end.
         * Vérifier *end refuse une saisie comme « 2abc ». errno détecte notamment
         * les dépassements, et isfinite refuse les valeurs NaN et infinies.
         * La base 0 de strtol autorise l'hexadécimal, utile pour 0x40. */
        char *end; errno=0; double num;
        if(f->type=='i') num=(double)strtol(value,&end,0);
        else num=strtod(value,&end);
        if(end==value || *end || errno || !isfinite(num) || num<f->low || num>f->high) {
            reason="valeur numerique invalide ou hors limites"; break;
        }
        if(f->type=='i') *(int *)dest=(int)num;
        else *(float *)dest=(float)num;
    }
    /* La lecture peut s'arrêter sur erreur et pas seulement en fin de fichier.
     * La fermeture est également vérifiée avant de valider la copie. */
    if(ferror(fp)) reason="erreur de lecture";
    if(fclose(fp)) reason="erreur de fermeture";
    /* Contraintes entre champs : image compatible avec le flux prévu,
     * directions ±1, seuils ordonnés, voies distinctes et servos cohérents.
     * Ce contrôle se fait après lecture : l'ordre des lignes n'a pas d'importance. */
    if(!reason && (candidate.width%64 || candidate.height%2 ||
       abs(candidate.control.pan_dir)!=1 || abs(candidate.control.tilt_dir)!=1 ||
       candidate.vision.u_min>candidate.vision.u_max || candidate.vision.v_min>candidate.vision.v_max ||
       candidate.pan.channel==candidate.tilt.channel || !candidate.i2c_dev[0] ||
       !valid_servo(&candidate.pan) || !valid_servo(&candidate.tilt)))
        reason="configuration incoherente (dimensions, directions, seuils ou servos)";
    if(reason) { fprintf(stderr,"%s:%d: %s\n",path,number,reason); return -1; }
    /* Validation finale : c'est le seul endroit qui modifie la configuration
     * de l'appelant. Toutes les erreurs précédentes ont laissé *cfg intact. */
    *cfg=candidate; return 0;
}
