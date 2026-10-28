/* RESPONSABLE : C. Tests de control_update, sans matériel : make test */
#include <math.h>
#include <stdio.h>

#include "config.h"
#include "control.h"

static int failures = 0;

#define CHECK(cond)                                                      \
    do {                                                                 \
        if (!(cond)) {                                                   \
            fprintf(stderr, "%s:%d: échec : %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                  \
        }                                                                \
    } while (0)

static void test_centered_target_does_not_move(const Config *cfg)
{
    ControlState st = { .pan = 135.0f, .tilt = 90.0f };
    Target t = { .found = true, .x = 320.0f, .y = 240.0f, .area = 50 };
    control_update(&cfg->control, &t, 640, 480, 135.0f, 90.0f, &st);
    CHECK(fabsf(st.pan - 135.0f) < 1e-3f);
    CHECK(fabsf(st.tilt - 90.0f) < 1e-3f);
}

static void test_target_right_moves_pan(const Config *cfg)
{
    ControlState st = { .pan = 135.0f, .tilt = 90.0f };
    Target t = { .found = true, .x = 600.0f, .y = 240.0f, .area = 50 };
    control_update(&cfg->control, &t, 640, 480, 135.0f, 90.0f, &st);
    CHECK(st.pan != 135.0f);
    CHECK(fabsf(st.pan - 135.0f) <= cfg->control.max_step_deg + 1e-3f);
}

/* TODO(C) : zone morte, cible perdue (maintien puis retour au centre),
 * signe de pan_dir / tilt_dir. */

int main(void)
{
    Config cfg;
    config_defaults(&cfg);

    test_centered_target_does_not_move(&cfg);
    test_target_right_moves_pan(&cfg);

    if (failures) {
        fprintf(stderr, "%d test(s) en échec\n", failures);
        return 1;
    }
    printf("tests control : OK\n");
    return 0;
}
