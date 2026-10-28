#ifndef VISION_H
#define VISION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* IMAGE YUV420 PLANAIRE COMPACTE, sans en-tête ni octets de remplissage.
 * Y décrit la luminosité de chaque pixel. U et V décrivent sa couleur,
 * avec une valeur partagée par un carré de 2 × 2 pixels.
 * Mémoire : [Y : w*h octets][U : w*h/4 octets][V : w*h/4 octets].
 * Les pointeurs y/u/v désignent des parties du même bloc : seul buf est libéré. */
typedef struct {
    int width;                 /* Largeur en pixels ; nécessairement paire. */
    int height;                /* Hauteur en pixels ; nécessairement paire. */
    uint8_t *buf;              /* Adresse du bloc unique alloué dynamiquement. */
    size_t size;               /* Taille du bloc : largeur × hauteur × 3/2. */
    uint8_t *y;                /* Début du plan de luminance. */
    uint8_t *u;                /* Début du premier plan de chrominance. */
    uint8_t *v;                /* Début du second plan de chrominance. */
} Frame;

/* Une cible est ici une zone de pixels respectant des seuils, sans IA. */
typedef struct {
    int use_color;             /* 0 : luminosité seule ; 1 : luminosité et couleur. */
    int y_min;                 /* Luminosité minimale acceptée, entre 0 et 255. */
    int u_min, u_max;          /* Intervalle accepté pour la composante U. */
    int v_min, v_max;          /* Intervalle accepté pour la composante V. */
    int min_area;              /* Surface minimale, en pixels ÉCHANTILLONNÉS. */
    int step;                  /* Pas du parcours : 2 = une ligne/colonne sur deux. */
} VisionParams;

/* Résultat de la détection d'une seule image, indépendant de l'image précédente. */
typedef struct {
    bool found;                /* false : aucun groupe suffisamment grand. */
    float x, y;                /* Centre moyen en pixels ; origine en haut à gauche. */
    int area;                  /* Nombre de points retenus sur la grille échantillonnée. */
} Target;

/* Prépare une structure neuve et alloue son bloc ; renvoie 0 ou -1.
 * Ne pas rappeler sur une image encore allouée sans frame_free() auparavant. */
int frame_alloc(Frame *f, int width, int height);
/* Libère le bloc unique et remet tous les champs à zéro. */
void frame_free(Frame *f);
/* Remplit un bloc déjà alloué : 1 = image lue ; 0 = fin normale du flux ;
 * -1 = erreur ou image incomplète. Les octets arrivent depuis un fichier ou tube. */
int frame_read(FILE *in, Frame *f);

/* Cherche le plus grand groupe connecté sur la grille échantillonnée.
 * Écrit toujours le résultat dans *out ; out->found indique la présence.
 * Renvoie 0 même sans cible, et -1 en cas d'entrée invalide ou manque de mémoire. */
int vision_detect(const Frame *f, const VisionParams *p, Target *out);

#endif
