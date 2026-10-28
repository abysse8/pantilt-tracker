#ifndef SERVO_H
#define SERVO_H

/* Carte PCA9685 (16 voies) sur le bus I2C du Raspberry Pi. */
typedef struct {
    int fd;         /* -1 en mode simulation (--dry-run) */
} Pca9685;

/* Un servo branché sur une voie de la carte. */
typedef struct {
    int channel;
    float range_deg;    /* course mécanique : 180 ou 270 */
    int min_us, max_us; /* impulsions pour 0° et range_deg (500 / 2500) */
    float min_deg, max_deg; /* butées logicielles, plus strictes que la mécanique */
    float center_deg;
    float angle;        /* dernier angle envoyé */
} Servo;

/* addr vaut normalement 0x40. dev = NULL : mode simulation, rien n'est écrit. */
int pca9685_open(Pca9685 *pca, const char *dev, int addr);
int pca9685_set_pulse_us(Pca9685 *pca, int channel, int pulse_us);
/* Coupe toutes les sorties PWM (servos relâchés). */
int pca9685_off(Pca9685 *pca);
void pca9685_close(Pca9685 *pca);

/* Borne l'angle entre min_deg et max_deg, convertit en impulsion et l'envoie. */
int servo_write(Pca9685 *pca, Servo *s, float deg);

#endif
