/* MODULE C — COMMANDE PROPORTIONNELLE DU SUIVI.
 * La caméra est solidaire de la tourelle : tourner vers la cible réduit son
 * écart au centre. Le servo possède déjà sa propre commande interne de position.
 */
#include <limits.h>
#include <math.h>
#include "control.h"

/* Calcule un incrément d'angle pour UN axe, sans toucher au matériel.
 * error : écart en pixels ; fov : champ de vision en degrés ; dt : secondes. */
static float correction(float error, float fov, int pixels,
                        const ControlParams *p, int direction, float dt)
{
    /* Zone morte : une petite erreur est tolérée pour ne pas poursuivre
     * indéfiniment le bruit de détection autour du centre. */
    if(fabsf(error)<=p->deadband_px) return 0;
    /* Conversion approximative pixels → degrés, puis commande proportionnelle.
     * Unités : (s^-1) * pixels * degrés / pixels = degrés/seconde.
     * direction inverse le signe si le servo est monté dans l'autre sens. */
    float speed=direction*p->kp*error*fov/pixels;
    /* Saturation symétrique : ni trop vite à gauche, ni trop vite à droite.
     * fminf/fmaxf sont les versions flottantes de minimum/maximum. */
    speed=fmaxf(-p->max_speed_deg_s,fminf(p->max_speed_deg_s,speed));
    /* Intégration sur le temps réellement écoulé : à 60 images/s les pas sont
     * deux fois plus petits qu'à 30 images/s, à erreur constante. */
    return speed*dt;
}

void control_update(const ControlParams *p, const Target *t,
                    int img_w, int img_h, float dt, ControlState *st)
{
    /* Refuser un temps non fini, nul ou négatif : sinon la commande pourrait
     * propager NaN (« pas un nombre ») jusqu'au calcul des impulsions. */
    if(img_w<=0 || img_h<=0 || !isfinite(dt) || dt<=0) return;
    /* La perte d'une cible ne donne pas de nouvelle direction fiable.
     * On compte les images perdues, mais on conserve les deux consignes. */
    if(!t->found) {
        /* Éviter le dépassement d'un entier si la perte dure très longtemps. */
        if(st->lost_count<INT_MAX) st->lost_count++;
        return;
    }
    if(!isfinite(t->x) || !isfinite(t->y)) return;
    /* Une détection valide réinitialise la durée de perte. Le centre de
     * l'image est (largeur/2, hauteur/2), dans la même convention que le détecteur. */
    st->lost_count=0;
    st->pan+=correction(t->x-img_w/2.0f,p->hfov_deg,img_w,p,p->pan_dir,dt);
    st->tilt+=correction(t->y-img_h/2.0f,p->vfov_deg,img_h,p,p->tilt_dir,dt);
}
