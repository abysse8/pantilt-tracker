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
    /* Les arguments sont : largeur, hauteur et nombre d'images à produire.
     * Ce générateur de test attend de petits entiers valides, pas une saisie libre. */
    int w = atoi(argv[1]), h = atoi(argv[2]), n = atoi(argv[3]);
    if (w <= 0 || h <= 0 || w % 2 || h % 2 || n <= 0) {
        fprintf(stderr, "dimensions paires et nombre d'images positifs requis\n");
        return 2;
    }

    /* Même disposition mémoire que Frame : un plan Y, puis deux plans quatre
     * fois plus petits. On réutilise le bloc pour toutes les images. */
    size_t ysize = (size_t)w * h, csize = ysize / 4;
    unsigned char *buf = malloc(ysize + 2 * csize);
    if (!buf) {
        perror("malloc");
        return 1;
    }

    /* Chaque passage produit une image complète. stdout reste exclusivement
     * binaire ; les positions attendues sont écrites séparément sur stderr. */
    for (int k = 0; k < n; k++) {
        /* Fond gris : luminosité 60, chrominances neutres à 128.
         * Sans réinitialisation, les anciennes positions laisseraient une trace. */
        memset(buf, 60, ysize);
        memset(buf + ysize, 128, 2 * csize);

        /* cos/sin font parcourir une ellipse au centre de la cible.
         * L'amplitude vaut 30 % de chaque dimension, pour rester dans l'image. */
        double a = 2.0 * M_PI * k / n;
        int cx = w / 2 + (int)(0.3 * w * cos(a));
        int cy = h / 2 + (int)(0.3 * h * sin(a));
        /* Dessiner un disque de rayon 10. Tester la distance au carré évite
         * une racine carrée ; tester les bords empêche d'écrire hors du bloc. */
        for (int y = cy - 10; y <= cy + 10; y++)
            for (int x = cx - 10; x <= cx + 10; x++)
                if (x >= 0 && x < w && y >= 0 && y < h &&
                    (x - cx) * (x - cx) + (y - cy) * (y - cy) <= 100)
                    buf[(size_t)y * w + x] = 255;

        /* Écrire tous les octets tels quels : aucun en-tête texte ne doit être
         * ajouté au flux, sinon le lecteur ne retrouverait plus les plans. */
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
