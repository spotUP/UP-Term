/* Indexed canvas for RIPscrip (RIP-3, D19/D20). Standalone: libc only. One byte
 * per pixel, a palette of 0xRRGGBB entries, and one dirty rectangle the host
 * reads and clears. Drawing primitives clip to the canvas; every write widens
 * the dirty rectangle (a palette change dirties all of it, so a renderer
 * redraws with the live palette). */
#include <stdlib.h>
#include <string.h>
#include "rip.h"

#define CV_MAX 1024

static void dirty_add(struct cv *c, int x0, int y0, int x1, int y1)
{
    struct cv_rect_s *d = &c->dirty;
    if (d->x1 <= d->x0) {
        d->x0 = x0, d->y0 = y0, d->x1 = x1, d->y1 = y1;
        return;
    }
    if (x0 < d->x0)
        d->x0 = x0;
    if (y0 < d->y0)
        d->y0 = y0;
    if (x1 > d->x1)
        d->x1 = x1;
    if (y1 > d->y1)
        d->y1 = y1;
}

struct cv *cv_open(int w, int h, int npal)
{
    struct cv *c;
    if (w <= 0 || h <= 0 || w > CV_MAX || h > CV_MAX || npal < 1 || npal > 256)
        return NULL;
    c = calloc(1, sizeof(*c));
    if (!c)
        return NULL;
    c->pix = calloc((size_t)w * (size_t)h, 1);
    if (!c->pix) {
        free(c);
        return NULL;
    }
    c->w = w;
    c->h = h;
    c->npal = npal;
    c->own = 1;
    return c;
}

struct cv *cv_open_pix(int w, int h, int npal, unsigned char *pix)
{
    struct cv *c;
    if (!pix || w <= 0 || h <= 0 || w > CV_MAX || h > CV_MAX || npal < 1 || npal > 256)
        return NULL;
    c = calloc(1, sizeof(*c));
    if (!c)
        return NULL;
    memset(pix, 0, (size_t)w * (size_t)h);
    c->pix = pix;
    c->w = w;
    c->h = h;
    c->npal = npal;
    return c;
}

void cv_close(struct cv *c)
{
    if (c) {
        if (c->own)
            free(c->pix);
        free(c);
    }
}

void cv_set_pal(struct cv *c, int idx, unsigned long rgb)
{
    if (idx < 0 || idx >= c->npal || c->pal[idx] == rgb)
        return;
    c->pal[idx] = rgb;
    dirty_add(c, 0, 0, c->w, c->h);
}

int cv_get(const struct cv *c, int x, int y)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h)
        return -1;
    return c->pix[y * c->w + x];
}

void cv_pixel(struct cv *c, int x, int y, int idx)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h)
        return;
    c->pix[y * c->w + x] = (unsigned char)idx;
    dirty_add(c, x, y, x + 1, y + 1);
}

void cv_hspan(struct cv *c, int x0, int x1, int y, int idx)
{
    if (y < 0 || y >= c->h)
        return;
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (x0 < 0)
        x0 = 0;
    if (x1 >= c->w)
        x1 = c->w - 1;
    if (x1 < x0)
        return;
    memset(c->pix + y * c->w + x0, idx, (size_t)(x1 - x0 + 1));
    dirty_add(c, x0, y, x1 + 1, y + 1);
}

void cv_rect(struct cv *c, int x0, int y0, int x1, int y1, int idx)
{
    int y;
    if (y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    for (y = y0 < 0 ? 0 : y0; y <= y1 && y < c->h; y++)
        cv_hspan(c, x0, x1, y, idx);
}

struct cv_rect_s cv_take_dirty(struct cv *c)
{
    struct cv_rect_s d = c->dirty;
    memset(&c->dirty, 0, sizeof(c->dirty));
    if (d.x1 <= d.x0 || d.y1 <= d.y0)
        memset(&d, 0, sizeof(d));
    return d;
}

int cv_flush(struct cv *c)
{
    if (c->dirty.x1 <= c->dirty.x0)
        return 0;
    c->serial++;
    return 1;
}

int cv_map(int cv_w, int cv_h, int win_w, int win_h, struct cv_map_s *m)
{
    if (cv_w <= 0 || cv_h <= 0 || win_w <= 0 || win_h <= 0)
        return 0;
    if (win_w >= cv_w && win_h >= cv_h) {
        m->out_w = cv_w, m->out_h = cv_h, m->num = 1, m->den = 1;
    } else if ((long)win_w * cv_h <= (long)win_h * cv_w) { /* width is the tight side */
        m->num = win_w, m->den = cv_w;
        m->out_w = win_w;
        m->out_h = (int)((long)cv_h * win_w / cv_w);
    } else {
        m->num = win_h, m->den = cv_h;
        m->out_h = win_h;
        m->out_w = (int)((long)cv_w * win_h / cv_h);
    }
    if (m->out_w < 1)
        m->out_w = 1;
    if (m->out_h < 1)
        m->out_h = 1;
    return 1;
}
