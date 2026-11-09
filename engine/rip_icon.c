/* rip_icon.c -- RIPscrip clipboard (1C copy, 1P paste, 1G copy region) and icons (1I, .ICN
 * files). Pure: no vt_term; the file read comes in through struct rip_icon_src. */
#include <stdlib.h>
#include <string.h>
#include "rip.h"
#include "dlname.h"

static long g_icons;

long rip_icons_drawn(void)
{
    return g_icons;
}

int rip_icon_name(const unsigned char *text, int ntext, char *out, int max)
{
    char raw[64];
    int i, dot = 0;
    while (ntext > 0 && text[ntext - 1] == ' ')
        ntext--;
    if (ntext <= 0 || ntext + 5 > max || ntext >= (int)sizeof(raw))
        return 0;
    for (i = 0; i < ntext; i++) {
        raw[i] = (char)text[i];
        if (text[i] == '.')
            dot = 1;
    }
    /* the shared sanitiser (dlname.h) repairs a path to its base name; an icon request
     * that needed repair is refused instead */
    if (!dl_clean_name(raw, ntext, out, max, 0) || (int)strlen(out) != ntext)
        return 0;
    if (!dot)
        strcat(out, ".ICN");
    return 1;
}

int rip_icon_decode(const unsigned char *d, long n, unsigned char **pix, int *w, int *h)
{
    int iw, ih, rb, x, y, p;
    unsigned char *o;
    if (!d || n < 4 || n > RIP_ICON_FILE_MAX)
        return 0;
    iw = d[0] | (d[1] << 8);
    ih = d[2] | (d[3] << 8);
    if (iw < 1 || ih < 1 || iw > CV_RIP_W || ih > CV_RIP_H)
        return 0;
    rb = (iw + 7) / 8;
    if (4L + (long)ih * 4 * rb > n)
        return 0;
    o = (unsigned char *)malloc((size_t)iw * (size_t)ih);
    if (!o)
        return 0;
    for (y = 0; y < ih; y++) {
        const unsigned char *row = d + 4 + (long)y * 4 * rb;
        for (x = 0; x < iw; x++) {
            int v = 0;
            for (p = 0; p < 4; p++)
                if (row[p * rb + (x >> 3)] & (0x80 >> (x & 7)))
                    v |= 1 << p;
            o[y * iw + x] = (unsigned char)v;
        }
    }
    *pix = o;
    *w = iw;
    *h = ih;
    return 1;
}

int rip_clip_handles(const struct rip_cmd *c)
{
    return c->level == 1 && (c->ch == 'C' || c->ch == 'P' || c->ch == 'G' || c->ch == 'I');
}

void rip_clip_free(struct rip_clip *clip)
{
    free(clip->pix);
    clip->pix = 0;
    clip->w = clip->h = 0;
}

static int mode_of(long m)
{
    return m >= 0 && m <= 4 ? (int)m : CV_COPY;
}

/* the box x0..x1, y0..y1 ordered and cut to the canvas; 0 when nothing is left */
static int box(const struct cv *cv, long *x0, long *y0, long *x1, long *y1)
{
    long t;
    if (*x0 > *x1) t = *x0, *x0 = *x1, *x1 = t;
    if (*y0 > *y1) t = *y0, *y1 = *y1, *y0 = t;
    if (*x0 < 0) *x0 = 0;
    if (*y0 < 0) *y0 = 0;
    if (*x1 >= cv->w) *x1 = cv->w - 1;
    if (*y1 >= cv->h) *y1 = cv->h - 1;
    return *x0 <= *x1 && *y0 <= *y1;
}

/* the canvas box x0..x1, y0..y1 into a new buffer; 0 when empty or out of memory */
static unsigned char *grab(const struct cv *cv, long x0, long y0, long x1, long y1, int *w, int *h)
{
    int i, j;
    unsigned char *o;
    if (!box(cv, &x0, &y0, &x1, &y1))
        return 0;
    *w = (int)(x1 - x0 + 1);
    *h = (int)(y1 - y0 + 1);
    o = (unsigned char *)malloc((size_t)*w * (size_t)*h);
    if (!o)
        return 0;
    for (j = 0; j < *h; j++)
        for (i = 0; i < *w; i++)
            o[j * *w + i] = (unsigned char)cv_get(cv, (int)x0 + i, (int)y0 + j);
    return o;
}

static void ripclip_store(struct rip_clip *clip, unsigned char *pix, int w, int h)
{
    free(clip->pix);
    clip->pix = pix;
    clip->w = w;
    clip->h = h;
}

int rip_clip_exec(struct rip_clip *clip, const struct rip_gfx *g, struct cv *cv,
                  const struct rip_cmd *c, const struct rip_icon_src *src)
{
    const long *a = c->arg;
    unsigned char *pix, *buf = 0;
    long n = 0, x0, y0, x1, y1;
    int w, h;
    char name[64];
    if (!rip_clip_handles(c))
        return 0;
    switch (c->ch) {
    case 'C': /* x0 y0 x1 y1 res: into the clipboard */
        if (c->nargs < 4)
            return 1;
        x0 = a[0], y0 = a[1], x1 = a[2], y1 = a[3];
        pix = grab(cv, x0, y0, x1, y1, &w, &h);
        if (pix)
            ripclip_store(clip, pix, w, h);
        return 1;
    case 'P': /* x y mode res: the clipboard back */
        if (c->nargs < 3 || !clip->pix)
            return 1;
        cv_blit(cv, (int)a[0], (int)a[1], clip->pix, clip->w, clip->h, mode_of(a[2]),
                g->vx0, g->vy0, g->vx1, g->vy1);
        return 1;
    case 'G': /* x0 y0 x1 y1 res dest_line: the box moves to the same x, row dest_line */
        if (c->nargs < 6)
            return 1;
        x0 = a[0], y0 = a[1], x1 = a[2], y1 = a[3];
        pix = grab(cv, x0, y0, x1, y1, &w, &h);
        if (pix) {
            long bx0 = x0 < x1 ? x0 : x1;
            cv_blit(cv, (int)(bx0 < 0 ? 0 : bx0), (int)a[5], pix, w, h, CV_COPY,
                    g->vx0, g->vy0, g->vx1, g->vy1);
            free(pix);
        }
        return 1;
    default: /* 'I': x y mode clipflag res name */
        if (c->nargs < 4 || !src || !src->load)
            return 1;
        if (!rip_icon_name(c->text, c->ntext, name, (int)sizeof(name)))
            return 1;
        if (!src->load(src->ud, name, &buf, &n))
            return 1;
        if (rip_icon_decode(buf, n, &pix, &w, &h)) {
            cv_blit(cv, (int)a[0], (int)a[1], pix, w, h, mode_of(a[2]),
                    g->vx0, g->vy0, g->vx1, g->vy1);
            g_icons++;
            if (a[3] == 1)
                ripclip_store(clip, pix, w, h);
            else
                free(pix);
        }
        if (src->release)
            src->release(src->ud, buf);
        return 1;
    }
}
