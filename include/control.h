#ifndef CONTROL_H
#define CONTROL_H

#include "vision.h"

/* La caméra est fixée sur la tourelle : on veut ramener la cible au centre
 * de l'image. Pas de calibration ni de cinématique inverse. */
typedef struct {
    float kp;               /* fraction de l'écart corrigée par image, 0 < kp < 1 */
    float hfov_deg, vfov_deg; /* champ de vision de la caméra */
    float deadband_px;      /* écart ignoré autour du centre */
    float max_step_deg;     /* pas maximal par image (limite de vitesse) */
    int pan_dir, tilt_dir;  /* +1 ou -1 selon le montage */
    int lost_frames_hold;   /* images sans cible avant de revenir au centre */
} ControlParams;

typedef struct {
    float pan, tilt;        /* consignes en degrés */
    int lost_count;
} ControlState;

/* Calcule les nouvelles consignes à partir de la cible détectée.
 * Fonction pure : testable sur un PC, sans caméra ni servo. */
void control_update(const ControlParams *p, const Target *t,
                    int img_w, int img_h,
                    float pan_center, float tilt_center,
                    ControlState *st);

#endif
