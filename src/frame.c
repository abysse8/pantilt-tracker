#include <stdlib.h>

#include "vision.h"

int frame_alloc(Frame *f, int width, int height)
{
    if (width <= 0 || height <= 0 || width % 2 || height % 2)
        return -1;

    f->width = width;
    f->height = height;
    f->size = (size_t)width * height * 3 / 2;
    f->buf = malloc(f->size);
    if (!f->buf)
        return -1;

    f->y = f->buf;
    f->u = f->y + (size_t)width * height;
    f->v = f->u + (size_t)(width / 2) * (height / 2);
    return 0;
}

void frame_free(Frame *f)
{
    free(f->buf);
    f->buf = f->y = f->u = f->v = NULL;
}

int frame_read(FILE *in, Frame *f)
{
    size_t got = fread(f->buf, 1, f->size, in);
    if (got == f->size)
        return 1;
    if (got == 0 && feof(in))
        return 0;
    return -1;
}
