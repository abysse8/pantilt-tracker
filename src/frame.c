/* MODULE A — STOCKAGE ET LECTURE DES IMAGES BRUTES.
 * Aucun décodage vidéo ici : le producteur doit fournir du YUV420 compact.
 */
#include <stdint.h>
#include <stdlib.h>
#include "vision.h"

int frame_alloc(Frame *f, int width, int height)
{
    /* Littéral composé C : initialiser tous les champs à zéro en une fois.
     * Cela laisse une structure propre même si l'allocation échoue. */
    *f=(Frame){0};
    /* Limiter les dimensions pour borner la mémoire demandée.
     * YUV420 exige des dimensions paires : U/V ont demi-largeur et demi-hauteur. */
    if(width<=0 || height<=0 || width>4096 || height>2160 || width%2 || height%2)
        return -1;
    /* Un octet Y par pixel + un octet U et V pour quatre pixels.
     * Le type size_t est prévu pour compter des octets et indexer la mémoire. */
    size_t pixels=(size_t)width*height;
    f->size=pixels+pixels/2;
    f->buf=malloc(f->size);
    if(!f->buf) return -1;
    /* Une allocation unique suffit. y/u/v sont des vues dans le même bloc,
     * obtenues par arithmétique de pointeurs, et non trois allocations séparées. */
    f->width=width; f->height=height;
    f->y=f->buf; f->u=f->y+pixels; f->v=f->u+pixels/4;
    return 0;
}

void frame_free(Frame *f)
{
    /* Libérer seulement buf : libérer aussi u/v serait une erreur mémoire.
     * Remettre à zéro permet d'appeler frame_free une deuxième fois sans danger. */
    free(f->buf); *f=(Frame){0};
}

int frame_read(FILE *in, Frame *f)
{
    /* fread demande exactement la taille d'une image, jamais une ligne de texte.
     * Les octets peuvent venir d'un fichier enregistré ou du tube de rpicam-vid. */
    size_t got=fread(f->buf,1,f->size,in);
    if(got==f->size) return 1;
    /* Zéro octet à la fin du fichier est normal. Quelques octets seulement
     * signalent une image tronquée : on ne doit pas analyser un mélange d'images. */
    if(got==0 && feof(in) && !ferror(in)) return 0;
    return -1;
}
