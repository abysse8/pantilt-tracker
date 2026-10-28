/* MODULE D — JOURNAL CSV.
 * Il permet de retrouver les détections et consignes après un essai, sans
 * confondre les consignes demandées avec des mesures d'angle réelles.
 */
#include "log.h"

int log_open(Logger *lg, const char *path)
{
    /* NULL représente le journal désactivé ; toutes les fonctions le gèrent. */
    lg->fp=NULL;
    if(!path || !*path) return 0;
    /* Le mode « w » crée le fichier ou remplace son ancien contenu.
     * Le dossier parent doit déjà exister ; l'échec remonte à main.c. */
    lg->fp=fopen(path,"w");
    if(!lg->fp) { perror(path); return -1; }
    /* L'en-tête rend le CSV exploitable dans un tableur ou un script. */
    if(fputs("frame,t_ms,found,x,y,area,pan,tilt\n",lg->fp)==EOF) {
        perror("CSV header"); log_close(lg); return -1;
    }
    return 0;
}

int log_row(Logger *lg, long frame, double t_ms, const Target *t,
            const ControlState *st)
{
    if(!lg->fp) return 0;
    /* Une ligne correspond à une image. %.3f garde trois chiffres après
     * la virgule décimale (un point dans le fichier CSV).
     * fprintf renvoie un nombre négatif si l'écriture échoue. */
    return fprintf(lg->fp,"%ld,%.3f,%d,%.3f,%.3f,%d,%.3f,%.3f\n",
        frame,t_ms,t->found,t->x,t->y,t->area,st->pan,st->tilt)<0 ? -1 : 0;
}

int log_close(Logger *lg)
{
    if(!lg->fp) return 0;
    /* On détache d'abord le pointeur de la structure pour éviter une seconde
     * fermeture. fclose peut échouer en vidant les écritures mises en tampon :
     * il faut donc tester son résultat, même si les fprintf avaient réussi. */
    FILE *fp=lg->fp; lg->fp=NULL;
    if(fclose(fp)) { perror("CSV close"); return -1; }
    return 0;
}
