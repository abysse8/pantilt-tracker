/* RESPONSABLE : C (commande) */
#include "control.h"

void control_update(const ControlParams *p, const Target *t,
                    int img_w, int img_h,
                    float pan_center, float tilt_center,
                    ControlState *st)
{
    /* TODO(C)
     * Cible trouvée : lost_count = 0 ;
     *   ex = t->x - img_w/2  (pixels), idem ey ;
     *   écart dans la zone morte -> ne rien changer sur cet axe ;
     *   sinon dpan = pan_dir * kp * ex * hfov_deg / img_w, borné à ±max_step_deg.
     * Cible perdue : garder la position pendant lost_frames_hold images,
     *   puis revenir lentement vers (pan_center, tilt_center).
     * Écrire tests/test_control.c AVANT de brancher le matériel. */
    (void)p;
    (void)t;
    (void)img_w;
    (void)img_h;
    (void)pan_center;
    (void)tilt_center;
    (void)st;
}
