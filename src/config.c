/* RESPONSABLE : D (configuration, journal, intégration) */
#include <stdio.h>
#include <string.h>

#include "config.h"

void config_defaults(Config *cfg)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->width = 640;
    cfg->height = 480;
    strcpy(cfg->i2c_dev, "/dev/i2c-1");
    cfg->i2c_addr = 0x40;

    cfg->vision = (VisionParams){
        .use_color = 0, .y_min = 240,
        .u_min = 0, .u_max = 255, .v_min = 0, .v_max = 255,
        .min_area = 20, .step = 2,
    };
    cfg->control = (ControlParams){
        .kp = 0.4f, .hfov_deg = 66.0f, .vfov_deg = 41.0f, /* Camera Module 3 */
        .deadband_px = 8.0f, .max_step_deg = 4.0f,
        .pan_dir = 1, .tilt_dir = 1, .lost_frames_hold = 30,
    };
    /* Kit Yahboom : pan 270°, tilt 180°, 500–2500 us. Butées à affiner. */
    cfg->pan = (Servo){
        .channel = 0, .range_deg = 270.0f, .min_us = 500, .max_us = 2500,
        .min_deg = 45.0f, .max_deg = 225.0f, .center_deg = 135.0f,
    };
    cfg->tilt = (Servo){
        .channel = 1, .range_deg = 180.0f, .min_us = 500, .max_us = 2500,
        .min_deg = 45.0f, .max_deg = 135.0f, .center_deg = 90.0f,
    };
}

int config_load(const char *path, Config *cfg)
{
    /* TODO(D) : fopen, fgets ligne par ligne, ignorer # et lignes vides,
     * sscanf "%63s = %63s", reconnaître chaque clé de tracker.conf.
     * Clé inconnue ou valeur invalide -> message avec numéro de ligne, -1. */
    (void)cfg;
    fprintf(stderr, "config_load(%s): pas encore implémenté, valeurs par défaut\n", path);
    return 0;
}
