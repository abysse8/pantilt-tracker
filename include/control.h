#ifndef CONTROL_H
#define CONTROL_H
#include "vision.h"

/* PARAMÈTRES DU SUIVI : ils sont lus dans tracker.conf.
 * La caméra tourne avec les servos. L'objectif est donc de réduire l'écart
 * entre la cible et le centre de l'image, sans calculer de position en 3D. */
typedef struct {
    float kp;                  /* Gain proportionnel en s⁻¹ : règle la réactivité. */
    float hfov_deg, vfov_deg;   /* Champs de vision horizontal/vertical en degrés. */
    float deadband_px;         /* Petit écart ignoré pour éviter les tremblements. */
    float max_speed_deg_s;     /* Vitesse maximale autorisée, en degrés/seconde. */
    int pan_dir, tilt_dir;     /* +1 ou -1 : sens physique de chaque montage. */
} ControlParams;

/* ÉTAT MÉMORISÉ ENTRE DEUX IMAGES.
 * Ces angles sont des consignes : aucun capteur ne confirme l'angle réel.
 * main.c les remplace par les angles bornés effectivement envoyés aux servos. */
typedef struct {
    float pan, tilt;           /* Consignes gauche/droite et haut/bas, en degrés. */
    int lost_count;            /* Nombre d'images consécutives sans cible. */
} ControlState;

/* Met à jour st à partir d'une observation t ; dt est exprimé en secondes.
 * Cette fonction ne parle à aucun matériel : elle est testable sur un PC.
 * Une cible perdue conserve les angles ; aucun balayage n'est déclenché.
 * Les butées mécaniques sont ensuite appliquées par servo_write(). */
void control_update(const ControlParams *p, const Target *t,
                    int img_w, int img_h, float dt, ControlState *st);
#endif
