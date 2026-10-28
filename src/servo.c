/* RESPONSABLE : B (actionneurs) */
#include <stdio.h>

#include "servo.h"

/* Registres PCA9685 (datasheet NXP) :
 *   MODE1 = 0x00 (bit SLEEP = 0x10, AI auto-incrément = 0x20, RESTART = 0x80)
 *   PRESCALE = 0xFE, modifiable seulement en SLEEP
 *     prescale = round(25 MHz / (4096 * 50 Hz)) - 1 = 121
 *   LEDn_ON_L = 0x06 + 4*n, puis ON_H, OFF_L, OFF_H
 *     ON = 0, OFF = pulse_us * 4096 / 20000  (1500 us -> 307)
 *   ALL_LED_OFF_H = 0xFD, bit 4 = tout couper
 * Accès Linux : open("/dev/i2c-1"), ioctl(fd, I2C_SLAVE, addr),
 *   puis write(fd, {registre, valeur...}, n). Voir <linux/i2c-dev.h>. */

int pca9685_open(Pca9685 *pca, const char *dev, int addr)
{
    (void)addr;
    pca->fd = -1;
    if (!dev)
        return 0; /* simulation */
    /* TODO(B) : ouvrir, sélectionner l'adresse, régler 50 Hz, réveiller. */
    fprintf(stderr, "pca9685_open: pas encore implémenté (utiliser -n)\n");
    return -1;
}

int pca9685_set_pulse_us(Pca9685 *pca, int channel, int pulse_us)
{
    if (pca->fd < 0)
        return 0;
    /* TODO(B) : écrire les 4 registres de la voie. Refuser channel hors 0..15
     * et pulse_us hors 400..2600. */
    (void)channel;
    (void)pulse_us;
    return -1;
}

int pca9685_off(Pca9685 *pca)
{
    if (pca->fd < 0)
        return 0;
    /* TODO(B) */
    return -1;
}

void pca9685_close(Pca9685 *pca)
{
    /* TODO(B) : close(fd) si fd >= 0 */
    pca->fd = -1;
}

int servo_write(Pca9685 *pca, Servo *s, float deg)
{
    /* TODO(B) : borner deg dans [min_deg, max_deg], convertir
     *   us = min_us + deg / range_deg * (max_us - min_us), mémoriser s->angle. */
    s->angle = deg;
    return pca9685_set_pulse_us(pca, s->channel, s->min_us);
}
