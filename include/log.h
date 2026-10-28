#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#include "control.h"
#include "vision.h"

typedef struct {
    FILE *fp;
} Logger;

/* path = NULL ou "" : journal désactivé, les appels suivants ne font rien. */
int log_open(Logger *lg, const char *path);
/* Une ligne CSV par image : frame,t_ms,found,x,y,area,pan,tilt */
void log_row(Logger *lg, long frame, double t_ms, const Target *t,
             const ControlState *st);
void log_close(Logger *lg);

#endif
