#ifndef VISION_H
#define VISION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Une image YUV420 planaire, telle que l'écrit `rpicam-vid --codec yuv420`.
 * Plans à la suite dans buf : Y (w*h), puis U (w/2*h/2), puis V (w/2*h/2). */
typedef struct {
    int width;
    int height;
    uint8_t *buf;   /* bloc unique alloué par frame_alloc */
    size_t size;    /* w*h*3/2 */
    uint8_t *y;     /* pointeurs dans buf */
    uint8_t *u;
    uint8_t *v;
} Frame;

typedef struct {
    int use_color;  /* 0 : point le plus lumineux, 1 : seuils U/V */
    int y_min;      /* luminance minimale d'un pixel cible */
    int u_min, u_max;
    int v_min, v_max;
    int min_area;   /* nombre de pixels minimal pour valider la cible */
    int step;       /* sous-échantillonnage : 1 = tous les pixels, 2 = un sur deux... */
} VisionParams;

typedef struct {
    bool found;
    float x, y;     /* centroïde en pixels, origine en haut à gauche */
    int area;       /* pixels retenus (après sous-échantillonnage) */
} Target;

int frame_alloc(Frame *f, int width, int height);
void frame_free(Frame *f);
/* 1 : image lue, 0 : fin du flux, -1 : erreur ou image tronquée */
int frame_read(FILE *in, Frame *f);

void vision_detect(const Frame *f, const VisionParams *p, Target *out);

#endif
