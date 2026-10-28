/* MODULE A — RECHERCHE DE LA CIBLE.
 * Étapes : seuils → groupes de pixels voisins → plus grand groupe → centre moyen.
 * Temps de calcul proportionnel au nombre de points échantillonnés.
 */
#include <stdlib.h>
#include <string.h>
#include "vision.h"

/* Test d'un pixel : ce petit filtre transforme l'image en « candidat / fond ».
 * La position (x,y) désigne toujours l'image complète, pas la grille réduite. */
static int matches(const Frame *f, const VisionParams *p, int x, int y)
{
    /* Une image est un tableau à une dimension : indice = ligne*largeur+colonne.
     * Le cast en size_t adapte le calcul au type utilisé pour les tailles mémoire. */
    if (f->y[(size_t)y*f->width+x] < p->y_min) return 0;
    if (!p->use_color) return 1;
    /* En YUV420, quatre pixels voisins partagent une paire U/V.
     * Les divisions entières par 2 retrouvent la bonne case des plans couleur. */
    size_t i=(size_t)(y/2)*(f->width/2)+x/2;
    return f->u[i]>=p->u_min && f->u[i]<=p->u_max &&
           f->v[i]>=p->v_min && f->v[i]<=p->v_max;
}

/* On segmente chaque image séparément : cette démo ne mémorise pas l'identité
 * d'une cible. Si un autre groupe devient plus grand, il sera sélectionné. */
int vision_detect(const Frame *f, const VisionParams *p, Target *out)
{
    /* Par défaut : aucune cible. Même en cas d'erreur, pas de résultat périmé. */
    *out=(Target){0};
    if (!f->buf || f->width<=0 || f->height<=0 || p->step<=0 ||
        p->step>16 || p->min_area<=0) return -1;
    /* Division arrondie vers le haut : garder aussi les points du dernier bord.
     * Avec step=2, on traite environ quatre fois moins de points. */
    int cols=(f->width+p->step-1)/p->step;
    int rows=(f->height+p->step-1)/p->step;
    size_t count=(size_t)cols*rows;
    /* mask contient 1 pour un candidat, 0 pour le fond ou un point déjà visité.
     * queue est la file du parcours en largeur (BFS) : on y ajoute les voisins
     * trouvés et on les traite dans leur ordre d'arrivée.
     * Au pire, un seul groupe occupe toute l'image : prévoir count places. */
    unsigned char *mask=malloc(count);
    size_t *queue=malloc(count*sizeof *queue);
    if (!mask || !queue) { free(mask); free(queue); return -1; }
    /* Première passe : appliquer les seuils à toute la grille réduite. */
    for(int y=0;y<rows;y++) for(int x=0;x<cols;x++)
        mask[(size_t)y*cols+x]=(unsigned char)matches(f,p,x*p->step,y*p->step);

    /* Deuxième passe : chaque point encore marqué démarre un nouveau groupe.
     * « Connecté » signifie ici relié par haut/bas/gauche/droite, sans diagonales. */
    for(size_t seed=0;seed<count;seed++) {
        if (!mask[seed]) continue;
        /* head = prochain élément à lire ; tail = prochaine place libre.
         * Effacer le point dès son ajout évite de l'ajouter plusieurs fois. */
        size_t head=0, tail=1; queue[0]=seed; mask[seed]=0;
        double sx=0, sy=0;
        while(head<tail) {
            size_t i=queue[head++];
            int x=(int)(i%(size_t)cols), y=(int)(i/(size_t)cols);
            /* Revenir aux coordonnées de l'image d'origine avant de sommer.
             * Le centre final sera ainsi utilisable directement par control.c. */
            sx+=x*p->step; sy+=y*p->step;
            /* Ces deux tableaux décrivent les quatre déplacements possibles.
             * Les tests de bornes empêchent toute lecture hors du tableau. */
            const int dx[]={-1,1,0,0}, dy[]={0,0,-1,1};
            for(int k=0;k<4;k++) {
                int nx=x+dx[k], ny=y+dy[k];
                if(nx<0 || nx>=cols || ny<0 || ny>=rows) continue;
                size_t next=(size_t)ny*cols+nx;
                if(mask[next]) { mask[next]=0; queue[tail++]=next; }
            }
        }
        /* tail est aussi le nombre de points du groupe. On ignore les petits
         * reflets et on ne remplace le résultat que par un groupe plus grand.
         * En cas d'égalité, le premier groupe rencontré est conservé. */
        if(tail>=(size_t)p->min_area && tail>(size_t)out->area) {
            out->found=true; out->area=(int)tail;
            out->x=(float)(sx/tail); out->y=(float)(sy/tail);
        }
    }
    /* Chaque malloc doit avoir son free : les images arrivent en continu,
     * donc une petite fuite par image finirait par épuiser la mémoire. */
    free(queue); free(mask); return 0;
}
