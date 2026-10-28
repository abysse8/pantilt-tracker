#ifndef SERVO_H
#define SERVO_H

/* Une carte PCA9685 fournit 16 signaux PWM, commandés par le bus I²C.
 * Elle ne fournit pas à elle seule l'énergie nécessaire aux moteurs. */
typedef struct {
    int fd;                    /* Descripteur Linux ouvert ; -1 en simulation. */
} Pca9685;

/* Correspondance entre un angle logique et la durée de son impulsion.
 * Les valeurs doivent correspondre au servo réellement utilisé : une course
 * 180° ou 270° ne se déduit pas de la couleur de la carte de commande. */
typedef struct {
    int channel;               /* Numéro de connecteur sur la carte : 0 à 15. */
    float range_deg;           /* Course totale utilisée pour la conversion. */
    int min_us, max_us;        /* Durées pour 0° et range_deg, en microsecondes. */
    float min_deg, max_deg;    /* Butées logicielles, à l'intérieur de la course. */
    float center_deg;          /* Position initiale choisie pour ce montage. */
    float angle;               /* Dernière consigne envoyée avec succès. */
} Servo;

/* Ouvre le bus, sélectionne addr et configure environ 50 Hz.
 * dev == NULL active la simulation : aucune communication physique.
 * Renvoie 0 en cas de succès, -1 sinon ; le descripteur reste fermé si échec. */
int pca9685_open(Pca9685 *pca, const char *dev, int addr);
/* Programme une voie. Les voies et durées invalides sont aussi refusées
 * en simulation, pour que les tests gardent un comportement représentatif. */
int pca9685_set_pulse_us(Pca9685 *pca, int channel, int pulse_us);
/* Coupe les signaux sur toutes les voies : le support peut perdre son maintien.
 * Renvoie -1 si la communication échoue ; ce n'est pas un arrêt matériel garanti. */
int pca9685_off(Pca9685 *pca);
/* Ferme seulement le fichier du bus ; ne remplace pas pca9685_off(). */
void pca9685_close(Pca9685 *pca);

/* Borne l'angle, le convertit en impulsion et l'envoie.
 * L'état s->angle n'est modifié qu'après réussite de l'envoi. */
int servo_write(Pca9685 *pca, Servo *s, float deg);

#endif
