/* Boucle principale : images sur stdin -> détection -> commande -> servos.
 *
 *   rpicam-vid -t 0 -n --codec yuv420 --width 640 --height 480 --framerate 30 -o - \
 *       | ./tracker -c tracker.conf
 *
 *   ./tracker -n -c tracker.conf < data/test.yuv     (sans servos, sur PC)
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "control.h"
#include "log.h"
#include "servo.h"
#include "vision.h"

static volatile sig_atomic_t stop_requested = 0;

static void on_signal(int sig)
{
    (void)sig;
    stop_requested = 1;
}

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [-c fichier.conf] [-n]\n"
            "  -c  fichier de configuration\n"
            "  -n  simulation : aucun accès I2C, servos non commandés\n"
            "Les images YUV420 sont lues sur l'entrée standard.\n",
            prog);
}

int main(int argc, char **argv)
{
    const char *config_path = NULL;
    int dry_run = 0;
    int opt;

    while ((opt = getopt(argc, argv, "c:nh")) != -1) {
        switch (opt) {
        case 'c': config_path = optarg; break;
        case 'n': dry_run = 1; break;
        default: usage(argv[0]); return opt == 'h' ? 0 : 2;
        }
    }

    Config cfg;
    config_defaults(&cfg);
    if (config_path && config_load(config_path, &cfg) != 0)
        return 1;

    Frame frame;
    if (frame_alloc(&frame, cfg.width, cfg.height) != 0) {
        fprintf(stderr, "allocation de l'image %dx%d impossible\n", cfg.width, cfg.height);
        return 1;
    }

    Pca9685 pca;
    if (pca9685_open(&pca, dry_run ? NULL : cfg.i2c_dev, cfg.i2c_addr) != 0) {
        frame_free(&frame);
        return 1;
    }

    Logger lg;
    if (log_open(&lg, cfg.log_path) != 0) {
        pca9685_close(&pca);
        frame_free(&frame);
        return 1;
    }

    struct sigaction sa = { .sa_handler = on_signal };
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    ControlState st = { .pan = cfg.pan.center_deg, .tilt = cfg.tilt.center_deg };
    servo_write(&pca, &cfg.pan, st.pan);
    servo_write(&pca, &cfg.tilt, st.tilt);

    int status = 0;
    long n = 0;
    double t0 = now_ms();

    while (!stop_requested) {
        int r = frame_read(stdin, &frame);
        if (r == 0)
            break;
        if (r < 0) {
            if (!stop_requested) {
                fprintf(stderr, "lecture d'image échouée (image %ld)\n", n);
                status = 1;
            }
            break;
        }

        Target t;
        vision_detect(&frame, &cfg.vision, &t);
        control_update(&cfg.control, &t, frame.width, frame.height,
                       cfg.pan.center_deg, cfg.tilt.center_deg, &st);

        if (servo_write(&pca, &cfg.pan, st.pan) != 0 ||
            servo_write(&pca, &cfg.tilt, st.tilt) != 0) {
            fprintf(stderr, "écriture servo échouée (image %ld)\n", n);
            status = 1;
            break;
        }

        log_row(&lg, n, now_ms() - t0, &t, &st);
        n++;
    }

    double elapsed = (now_ms() - t0) / 1000.0;
    fprintf(stderr, "%ld images en %.1f s (%.1f images/s)\n",
            n, elapsed, elapsed > 0 ? n / elapsed : 0.0);

    servo_write(&pca, &cfg.pan, cfg.pan.center_deg);
    servo_write(&pca, &cfg.tilt, cfg.tilt.center_deg);
    usleep(300 * 1000);
    pca9685_off(&pca);

    log_close(&lg);
    pca9685_close(&pca);
    frame_free(&frame);
    return status;
}
