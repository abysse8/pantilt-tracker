#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include "control.h"
#include "vision.h"

/* FILE est le type de flux fourni par la bibliothèque standard du C.
 * fp == NULL signifie qu'aucun journal n'est ouvert. */
typedef struct {
    FILE *fp;
} Logger;

/* Ouvre le CSV en écriture : un ancien fichier de même nom sera remplacé.
 * path == NULL ou "" désactive le journal. Renvoie 0 ou -1 en cas d'erreur. */
int log_open(Logger *lg, const char *path);
/* Écrit une observation : numéro, temps en ms, détection, centre, surface,
 * puis consignes pan/tilt en degrés. Renvoie 0 ou -1 si l'écriture échoue.
 * L'en-tête conserve des noms courts : frame,t_ms,found,x,y,area,pan,tilt. */
int log_row(Logger *lg, long frame, double t_ms, const Target *t,
             const ControlState *st);
/* Ferme le fichier et vérifie aussi les erreurs d'écriture différée.
 * Un journal déjà fermé peut être refermé sans conséquence. */
int log_close(Logger *lg);

#endif
