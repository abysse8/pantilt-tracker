#ifndef CONFIG_H
#define CONFIG_H

#include "control.h"
#include "servo.h"
#include "vision.h"

/* Configuration complète de l'application, transmise aux différents modules.
 * Chaque sous-structure rassemble les paramètres d'une responsabilité.
 * Les tableaux de caractères contiennent les chemins, terminés par '\0'. */
typedef struct {
    int width, height;         /* Dimensions attendues des images, en pixels. */
    char i2c_dev[64];          /* Fichier du bus Linux, par exemple /dev/i2c-1. */
    int i2c_addr;              /* Adresse de la carte sur ce bus, souvent 0x40. */
    char log_path[256];        /* Chemin du CSV ; chaîne vide = journal désactivé. */
    VisionParams vision;      /* Seuils permettant de reconnaître la cible. */
    ControlParams control;    /* Réactivité, vitesse et sens des mouvements. */
    Servo pan, tilt;          /* Description et état de chacun des deux servos. */
} Config;

/* Remplit toute la structure, y compris les champs non présents dans le fichier. */
void config_defaults(Config *cfg);
/* Lit un fichier texte « cle = valeur », en ignorant les lignes vides et #.
 * Renvoie 0 si tout est valide ; sinon -1 avec une explication sur stderr.
 * Le chargement est transactionnel : une erreur laisse *cfg inchangé.
 * Les contraintes entre plusieurs paramètres sont contrôlées en fin de lecture. */
int config_load(const char *path, Config *cfg);

#endif
