/* Indexed canvas for RIPscrip (RIP-3, D19/D20). Standalone: libc only. One byte
 * per pixel, a palette of 0xRRGGBB entries, and one dirty rectangle the host
 * reads and clears. Drawing primitives clip to the canvas; every write widens
 * the dirty rectangle (a palette change dirties all of it, so a renderer
 * redraws with the live palette). */
#include <stdlib.h>
#include <string.h>
#include "rip.h"

#define CV_MAX CV_ROWS_MAX

/* Widen c->dirty by a NON-EMPTY box on locals: cv_rect_union's result without a struct copy and a
 * call (on a 68000 those cost ~470 cycles a pixel). */
#define DIRTY_BOX(c, X0, Y0, X1, Y1)                                                                   \
    do {                                                                                               \
        struct cv_rect_s *d_ = &(c)->dirty;                                                            \
        if (d_->x1 <= d_->x0) {                                                                        \
            d_->x0 = (X0), d_->y0 = (Y0), d_->x1 = (X1), d_->y1 = (Y1);                                \
        } else {                                                                                       \
            if ((X0) < d_->x0) d_->x0 = (X0);                                                          \
            if ((Y0) < d_->y0) d_->y0 = (Y0);                                                          \
            if ((X1) > d_->x1) d_->x1 = (X1);                                                          \
            if ((Y1) > d_->y1) d_->y1 = (Y1);                                                          \
        }                                                                                              \
    } while (0)

void cv_rect_union(struct cv_rect_s *d, const struct cv_rect_s *r)
{
    if (r->x1 <= r->x0 || r->y1 <= r->y0)
        return;
    if (d->x1 <= d->x0) {
        *d = *r;
        return;
    }
    if (r->x0 < d->x0)
        d->x0 = r->x0;
    if (r->y0 < d->y0)
        d->y0 = r->y0;
    if (r->x1 > d->x1)
        d->x1 = r->x1;
    if (r->y1 > d->y1)
        d->y1 = r->y1;
}

void cv_dirty(struct cv *c, int x0, int y0, int x1, int y1)
{
    if (x1 <= x0 || y1 <= y0)
        return;
    DIRTY_BOX(c, x0, y0, x1, y1);
}

static void fill_rows(struct cv *c)
{
    int y;
    unsigned char *p = c->pix;
    for (y = 0; y < c->h; y++, p += c->w)
        c->row[y] = p;
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
    fill_rows(c);
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
    fill_rows(c);
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
    DIRTY_BOX(c, 0, 0, c->w, c->h);
}

/* one unsigned compare tests both ends: a negative int is a huge unsigned */
int cv_get(const struct cv *c, int x, int y)
{
    if ((unsigned)x >= (unsigned)c->w || (unsigned)y >= (unsigned)c->h)
        return -1;
    return c->row[y][x];
}

void cv_pixel(struct cv *c, int x, int y, int idx)
{
    struct cv_rect_s *d;
    if ((unsigned)x >= (unsigned)c->w || (unsigned)y >= (unsigned)c->h)
        return;
    c->row[y][x] = (unsigned char)idx;
    d = &c->dirty;
    if (d->x1 <= d->x0) {
        d->x0 = x, d->y0 = y, d->x1 = x + 1, d->y1 = y + 1;
        return;
    }
    if (x < d->x0)
        d->x0 = x;
    else if (x >= d->x1)
        d->x1 = x + 1;
    if (y < d->y0)
        d->y0 = y;
    else if (y >= d->y1)
        d->y1 = y + 1;
}

void cv_hspan(struct cv *c, int x0, int x1, int y, int idx)
{
    if ((unsigned)y >= (unsigned)c->h)
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
    memset(c->row[y] + x0, idx, (size_t)(x1 - x0 + 1));
    x1++;
    DIRTY_BOX(c, x0, y, x1, y + 1);
}

void cv_hrow(struct cv *c, int x0, int y, const unsigned char *src, int n)
{
    int x1 = x0 + n - 1;
    if ((unsigned)y >= (unsigned)c->h || n <= 0)
        return;
    if (x0 < 0) {
        src -= x0;
        x0 = 0;
    }
    if (x1 >= c->w)
        x1 = c->w - 1;
    if (x1 < x0)
        return;
    memcpy(c->row[y] + x0, src, (size_t)(x1 - x0 + 1));
    x1++;
    DIRTY_BOX(c, x0, y, x1, y + 1);
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

void cv_blit(struct cv *c, int x, int y, const unsigned char *src, int w, int h, int minterm,
             int cx0, int cy0, int cx1, int cy1)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            int px = x + i, py = y + j, d, v = src[j * w + i];
            if (px < cx0 || px > cx1 || py < cy0 || py > cy1)
                continue;
            d = cv_get(c, px, py);
            if (d < 0)
                continue;
            switch (minterm) {
            case CV_XOR: v = d ^ v; break;
            case CV_OR: v = d | v; break;
            case CV_AND: v = d & v; break;
            case CV_NOT: v = ~v & 15; break;
            default: break;
            }
            cv_pixel(c, px, py, v);
        }
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
