/* RESPONSABLE : D (configuration, journal, intégration) */
#include "log.h"

int log_open(Logger *lg, const char *path)
{
    /* TODO(D) : si path non vide, fopen en écriture (erreur -> perror, -1)
     * et écrire l'en-tête CSV. */
    (void)path;
    lg->fp = NULL;
    return 0;
}

void log_row(Logger *lg, long frame, double t_ms, const Target *t,
             const ControlState *st)
{
    if (!lg->fp)
        return;
    /* TODO(D) */
    (void)frame;
    (void)t_ms;
    (void)t;
    (void)st;
}

void log_close(Logger *lg)
{
    /* TODO(D) : fclose si ouvert */
    lg->fp = NULL;
}
