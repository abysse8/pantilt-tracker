#ifndef CONFIG_H
#define CONFIG_H

#include "control.h"
#include "servo.h"
#include "vision.h"

typedef struct {
    int width, height;
    char i2c_dev[64];
    int i2c_addr;
    char log_path[256];
    VisionParams vision;
    ControlParams control;
    Servo pan, tilt;
} Config;

void config_defaults(Config *cfg);
/* Fichier texte "cle = valeur", lignes vides et # ignorés.
 * Retourne 0, ou -1 avec un message sur stderr (fichier absent, clé inconnue,
 * valeur invalide : indiquer le numéro de ligne). */
int config_load(const char *path, Config *cfg);

#endif
