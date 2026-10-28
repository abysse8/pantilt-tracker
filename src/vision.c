/* RESPONSABLE : A (vision) */
#include "vision.h"

void vision_detect(const Frame *f, const VisionParams *p, Target *out)
{
    /* TODO(A)
     * Étape 1 : parcourir le plan Y (pas de p->step), garder les pixels
     *   avec y >= p->y_min, accumuler somme des x, somme des y, compte.
     *   Centroïde = sommes / compte ; found si compte >= p->min_area.
     * Étape 2 (use_color) : pour le pixel (x, y), la chrominance est
     *   u[(y/2) * (w/2) + x/2] et v[même indice] ; tester les seuils U/V.
     * Tester sur des images enregistrées (voir README), pas en direct. */
    (void)f;
    (void)p;
    out->found = false;
    out->x = out->y = 0.0f;
    out->area = 0;
}
