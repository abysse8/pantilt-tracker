/* Génère une séquence YUV420 synthétique : fond gris, disque blanc qui décrit
 * un cercle. Permet de tester vision et commande sans caméra.
 *   ./bin/gen_frames 640 480 90 > data/cercle.yuv */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s largeur hauteur nb_images > sortie.yuv\n", argv[0]);
        return 2;
    }
    int w = atoi(argv[1]), h = atoi(argv[2]), n = atoi(argv[3]);
    if (w <= 0 || h <= 0 || w % 2 || h % 2 || n <= 0) {
        fprintf(stderr, "dimensions paires et nombre d'images positifs requis\n");
        return 2;
    }

    size_t ysize = (size_t)w * h, csize = ysize / 4;
    unsigned char *buf = malloc(ysize + 2 * csize);
    if (!buf) {
        perror("malloc");
        return 1;
    }

    for (int k = 0; k < n; k++) {
        memset(buf, 60, ysize);
        memset(buf + ysize, 128, 2 * csize);

        double a = 2.0 * M_PI * k / n;
        int cx = w / 2 + (int)(0.3 * w * cos(a));
        int cy = h / 2 + (int)(0.3 * h * sin(a));
        for (int y = cy - 10; y <= cy + 10; y++)
            for (int x = cx - 10; x <= cx + 10; x++)
                if (x >= 0 && x < w && y >= 0 && y < h &&
                    (x - cx) * (x - cx) + (y - cy) * (y - cy) <= 100)
                    buf[(size_t)y * w + x] = 255;

        if (fwrite(buf, 1, ysize + 2 * csize, stdout) != ysize + 2 * csize) {
            perror("fwrite");
            free(buf);
            return 1;
        }
        /* centre attendu, pour comparer avec la détection */
        fprintf(stderr, "%d,%d,%d\n", k, cx, cy);
    }

    free(buf);
    return 0;
}
