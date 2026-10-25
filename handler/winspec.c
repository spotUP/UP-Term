#include "winspec.h"

int winspec_size(int want, int origin, int extent)
{
    int room;
    if (want != 0)
        return want;
    room = extent - origin;
    return room > 0 ? room : extent;
}

/* The UP-Term ground (light grey on black) unless the spec chose colours; the
 * personality (CON: is the amiga dialect) is escape-sequence protocol and does
 * not decide the look. Returns 1 when it set *fg / *bg. */
int winspec_ground(int colours, unsigned long *fg, unsigned long *bg)
{
    if (colours)
        return 0;
    *fg = 0xC0C0C0UL;
    *bg = 0x000000UL;
    return 1;
}

static int ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'a' && x <= 'z') x = (char)(x - 32);
        if (y >= 'a' && y <= 'z') y = (char)(y - 32);
        if (x != y) return 0;
    }
    return *a == *b;
}

static int num(const char **p)
{
    int v = 0, any = 0;
    while (**p >= '0' && **p <= '9') {
        if (v < 100000) v = v * 10 + (**p - '0');
        (*p)++;
        any = 1;
    }
    return any ? v : -1;
}

int winspec_option(const char *field, winspec_opts *o)
{
    if (ieq(field, "CENTRE") || ieq(field, "CENTER")) {
        o->centre = 1;
        return 1;
    }
    if (ieq(field, "FRONT")) {
        o->front = 1;
        return 1;
    }
    if (ieq(field, "CLOSEINACTIVE")) {
        o->close_inactive = 1;
        return 1;
    }
    if ((field[0] | 32) == 'c' && (field[1] | 32) == 'e' && (field[2] | 32) == 'l' &&
        (field[3] | 32) == 'l' && (field[4] | 32) == 's' && (field[5] == ' ' || field[5] == '\t')) {
        const char *p = field + 5;
        int cols, rows;
        while (*p == ' ' || *p == '\t') p++;
        cols = num(&p);
        if (cols < 1 || (*p != 'x' && *p != 'X'))
            return 0;
        p++;
        rows = num(&p);
        if (rows < 1 || *p)
            return 0;
        o->cols = cols;
        o->rows = rows;
        return 1;
    }
    return 0;
}

void winspec_box(const winspec_opts *o, int cw, int ch, int bl, int bt, int br, int bb,
                 int sw, int sh, int *x, int *y, int *w, int *h)
{
    if (o->cols > 0 && o->rows > 0) {
        *w = o->cols * cw + bl + br;
        *h = o->rows * ch + bt + bb;
    }
    if (*w > sw) *w = sw;
    if (*h > sh) *h = sh;
    if (o->centre) {
        *x = (sw - *w) / 2;
        *y = (sh - *h) / 2;
    }
    if (*x + *w > sw) *x = sw - *w;
    if (*y + *h > sh) *y = sh - *h;
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
}

const char *winspec_wa_title(int borderless, const char *title)
{
    return borderless ? 0 : title;
}
