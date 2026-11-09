/* RIPscrip drawing model (RIP-4). Pure C like the rest of the RIP core: libc and
 * engine/rip.h only, integer arithmetic only (no libm, so the Amiga build needs no
 * maths library). Geometry follows Borland BGI, the library RIPterm drew with: angles
 * run counterclockwise from three o'clock, a thick line is three pixels across, a
 * circle ignores the line style. Exact pixel results of arcs, ellipses and pies are
 * UNVERIFIED against BGI itself (no reference output available): they are polylines
 * through rounded parametric points, pinned in the tests by their extreme points
 * only. The pattern tables are from the BGI documentation, also UNVERIFIED here.
 * Drawing clips to the canvas; the RIP viewport (`v`) belongs to RIP-5. */
#include <stdlib.h>
#include <string.h>
#include "rip.h"

static const short sin_tab[91] = {
    0, 71, 143, 214, 286, 357, 428, 499, 570, 641, 711, 782, 852, 921, 991, 1060, 1129, 1198, 1266, 1334, 1401,
    1468, 1534, 1600, 1666, 1731, 1796, 1860, 1923, 1986, 2048, 2110, 2171, 2231, 2290, 2349, 2408, 2465, 2522,
    2578, 2633, 2687, 2741, 2793, 2845, 2896, 2946, 2996, 3044, 3091, 3138, 3183, 3228, 3271, 3314, 3355, 3396,
    3435, 3474, 3511, 3547, 3582, 3617, 3650, 3681, 3712, 3742, 3770, 3798, 3824, 3849, 3873, 3896, 3917, 3937,
    3956, 3974, 3991, 4006, 4021, 4034, 4046, 4056, 4065, 4074, 4080, 4086, 4090, 4094, 4095, 4096
};

/* sin of an angle in 1/16 degrees, scaled by 4096 */
static long sin16(long a)
{
    long d, f, q;
    a %= 360 * 16;
    if (a < 0)
        a += 360 * 16;
    q = a / (90 * 16);
    a %= 90 * 16;
    if (q & 1)
        a = 90 * 16 - a;
    d = a / 16;
    f = a % 16;
    {
        long v = d >= 90 ? sin_tab[90] : sin_tab[d] + (sin_tab[d + 1] - sin_tab[d]) * f / 16;
        return q >= 2 ? -v : v;
    }
}
static long cos16(long a) { return sin16(a + 90 * 16); }

static const unsigned char fill_tab[12][8] = {
    { 0, 0, 0, 0, 0, 0, 0, 0 },                         /* 0 empty: background */
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, /* 1 solid */
    { 0xFF, 0xFF, 0, 0, 0xFF, 0xFF, 0, 0 },             /* 2 line */
    { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 }, /* 3 light slash */
    { 0xE0, 0xC1, 0x83, 0x07, 0x0E, 0x1C, 0x38, 0x70 }, /* 4 slash */
    { 0xF0, 0x78, 0x3C, 0x1E, 0x0F, 0x87, 0xC3, 0xE1 }, /* 5 backslash */
    { 0xA5, 0xD2, 0x69, 0xB4, 0x5A, 0x2D, 0x96, 0x4B }, /* 6 light backslash */
    { 0xFF, 0x88, 0x88, 0x88, 0xFF, 0x88, 0x88, 0x88 }, /* 7 hatch */
    { 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81 }, /* 8 cross hatch */
    { 0xCC, 0x33, 0xCC, 0x33, 0xCC, 0x33, 0xCC, 0x33 }, /* 9 interleave */
    { 0x80, 0, 0x08, 0, 0x80, 0, 0x08, 0 },             /* 10 wide dot */
    { 0x88, 0, 0x22, 0, 0x88, 0, 0x22, 0 }              /* 11 close dot */
};

void rip_gfx_init(struct rip_gfx *g)
{
    memset(g, 0, sizeof(*g));
    g->color = 15;
    g->lstyle = 0;
    g->lpat = 0xFFFF;
    g->thick = 1;
    g->fpat = 1;
    g->fcolor = 15;
    g->vx0 = 0;
    g->vy0 = 0;
    g->vx1 = CV_RIP_W - 1;
    g->vy1 = CV_RIP_H - 1;
    g->font = 0;
    g->fdir = 0;
    g->fsize = 1;
}

static const unsigned char font8[256][8] = {
#include "font8x8.inc"
};

unsigned char rip_font8_row(int ch, int r)
{
    return font8[ch & 255][r & 7];
}

static void gput(const struct rip_gfx *g, struct cv *cv, int x, int y, int col)
{
    if (x < g->vx0 || x > g->vx1 || y < g->vy0 || y > g->vy1)
        return;
    if (g->wmode == 1) {
        int old = cv_get(cv, x, y);
        if (old < 0)
            return;
        col ^= old;
    }
    cv_pixel(cv, x, y, col);
}

struct stroke_ctx { const struct rip_gfx *g; struct cv *cv; };

static void line(const struct rip_gfx *g, struct cv *cv, int x0, int y0, int x1, int y1, unsigned pat);

static void stroke_seg(void *ud, int x0, int y0, int x1, int y1)
{
    const struct stroke_ctx *sc = ud;
    line(sc->g, sc->cv, x0, y0, x1, y1, 0xFFFF);
}

static int fill_bit(const struct rip_gfx *g, int x, int y)
{
    const unsigned char *p = g->fpat == 12 ? g->upat : fill_tab[g->fpat < 0 || g->fpat > 11 ? 1 : g->fpat];
    return (p[y & 7] >> (7 - (x & 7))) & 1;
}

static void fill_span(const struct rip_gfx *g, struct cv *cv, int x0, int x1, int y)
{
    int x;
    for (x = x0; x <= x1; x++)
        gput(g, cv, x, y, fill_bit(g, x, y) ? g->fcolor : 0);
}

static unsigned line_pattern(const struct rip_gfx *g)
{
    static const unsigned pat[4] = { 0xFFFF, 0xCCCC, 0xF1F8, 0xF8F8 };
    return g->lstyle == 4 ? g->lpat & 0xFFFF : pat[g->lstyle & 3];
}

static void pen(const struct rip_gfx *g, struct cv *cv, int x, int y, int xmajor)
{
    gput(g, cv, x, y, g->color);
    if (g->thick == 3) {
        if (xmajor) {
            gput(g, cv, x, y - 1, g->color);
            gput(g, cv, x, y + 1, g->color);
        } else {
            gput(g, cv, x - 1, y, g->color);
            gput(g, cv, x + 1, y, g->color);
        }
    }
}

/* Bresenham; pat 0xFFFF solid, else bit 15 first, one bit a pixel along the line */
static void line(const struct rip_gfx *g, struct cv *cv, int x0, int y0, int x1, int y1, unsigned pat)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy, k = 0, xmajor = dx >= dy;
    for (;;) {
        if ((pat >> (15 - (k & 15))) & 1)
            pen(g, cv, x0, y0, xmajor);
        k++;
        if (x0 == x1 && y0 == y1)
            break;
        {
            int e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx) {
                err += dx;
                y0 += sy;
            }
        }
    }
}

static void styled_line(const struct rip_gfx *g, struct cv *cv, int x0, int y0, int x1, int y1)
{
    line(g, cv, x0, y0, x1, y1, line_pattern(g));
}

static void rect_outline(const struct rip_gfx *g, struct cv *cv, int x0, int y0, int x1, int y1)
{
    styled_line(g, cv, x0, y0, x1, y0);
    styled_line(g, cv, x1, y0, x1, y1);
    styled_line(g, cv, x1, y1, x0, y1);
    styled_line(g, cv, x0, y1, x0, y0);
}

/* the points of an elliptical arc from sa to ea degrees (counterclockwise from three
 * o'clock), centre (cx, cy); a full turn when sa == ea == 0 or the span is 360.
 * Returns the count (at most max). */
static int arc_points(int cx, int cy, int sa, int ea, int rx, int ry, int *pt, int max)
{
    long span, a0, i, n;
    int r = rx > ry ? rx : ry, k = 0;
    sa %= 360;
    ea %= 360;
    if (sa < 0)
        sa += 360;
    if (ea < 0)
        ea += 360;
    span = ea - sa;
    if (span <= 0)
        span += 360;
    if (sa == 0 && ea == 0)
        span = 360;
    n = span;
    if (span * r / 24 > n)
        n = span * r / 24;
    if (n > max - 1)
        n = max - 1;
    a0 = (long)sa * 16;
    for (i = 0; i <= n; i++) {
        long a = a0 + span * 16 * i / n;
        pt[2 * k] = cx + (int)((long)rx * cos16(a) / 4096 + (cos16(a) >= 0 ? 0 : 0));
        pt[2 * k + 1] = cy - (int)((long)ry * sin16(a) / 4096);
        /* rounded, not truncated */
        pt[2 * k] = cx + (int)(((long)rx * cos16(a) + (cos16(a) >= 0 ? 2048 : -2048)) / 4096);
        pt[2 * k + 1] = cy - (int)(((long)ry * sin16(a) + (sin16(a) >= 0 ? 2048 : -2048)) / 4096);
        k++;
    }
    return k;
}

#define ARC_MAX 4096

static void poly_line(const struct rip_gfx *g, struct cv *cv, const int *pt, int n)
{
    int i;
    struct rip_gfx s = *g;
    s.lstyle = 0; /* arcs and ellipses are solid */
    for (i = 1; i < n; i++)
        line(&s, cv, pt[2 * i - 2], pt[2 * i - 1], pt[2 * i], pt[2 * i + 1], 0xFFFF);
    if (n == 1)
        pen(&s, cv, pt[0], pt[1], 1);
}

/* even-odd scanline fill of a closed polygon with the fill style */
static void fill_poly(const struct rip_gfx *g, struct cv *cv, const int *pt, int n)
{
    int ymin, ymax, y, i, k;
    int *xs;
    if (n < 3)
        return;
    xs = malloc((size_t)n * sizeof(int));
    if (!xs)
        return;
    ymin = ymax = pt[1];
    for (i = 1; i < n; i++) {
        if (pt[2 * i + 1] < ymin)
            ymin = pt[2 * i + 1];
        if (pt[2 * i + 1] > ymax)
            ymax = pt[2 * i + 1];
    }
    if (ymin < 0)
        ymin = 0;
    if (ymax >= cv->h)
        ymax = cv->h - 1;
    for (y = ymin; y <= ymax; y++) {
        k = 0;
        for (i = 0; i < n; i++) {
            int j = (i + 1) % n, ya = pt[2 * i + 1], yb = pt[2 * j + 1], xa = pt[2 * i], xb = pt[2 * j];
            if (ya == yb)
                continue;
            if (ya > yb) {
                int t = ya;
                ya = yb;
                yb = t;
                t = xa;
                xa = xb;
                xb = t;
            }
            if (y < ya || y >= yb)
                continue;
            xs[k++] = xa + (int)(((long)(y - ya) * (xb - xa) + (xb >= xa ? (yb - ya) / 2 : -((yb - ya) / 2))) / (yb - ya));
        }
        for (i = 1; i < k; i++) { /* insertion sort */
            int v = xs[i], j = i - 1;
            while (j >= 0 && xs[j] > v) {
                xs[j + 1] = xs[j];
                j--;
            }
            xs[j + 1] = v;
        }
        for (i = 0; i + 1 < k; i += 2)
            fill_span(g, cv, xs[i], xs[i + 1], y);
    }
    free(xs);
}

/* flood fill from (x, y) over pixels that are not `border`, 4-connected */
static void flood(const struct rip_gfx *g, struct cv *cv, int x, int y, int border)
{
    unsigned char *seen;
    short *stk;
    int sp = 0, cap = 4096;
    if (cv_get(cv, x, y) < 0 || cv_get(cv, x, y) == border)
        return;
    seen = calloc((size_t)cv->w * (size_t)cv->h, 1);
    stk = malloc((size_t)cap * 2 * sizeof(short));
    if (!seen || !stk) {
        free(seen);
        free(stk);
        return;
    }
    stk[0] = (short)x;
    stk[1] = (short)y;
    sp = 1;
    while (sp) {
        int px, py, xl, xr, i, dir;
        sp--;
        px = stk[2 * sp];
        py = stk[2 * sp + 1];
        if (seen[py * cv->w + px] || cv_get(cv, px, py) == border)
            continue;
        xl = xr = px;
        while (xl > 0 && !seen[py * cv->w + xl - 1] && cv_get(cv, xl - 1, py) != border)
            xl--;
        while (xr < cv->w - 1 && !seen[py * cv->w + xr + 1] && cv_get(cv, xr + 1, py) != border)
            xr++;
        for (i = xl; i <= xr; i++)
            seen[py * cv->w + i] = 1;
        for (dir = -1; dir <= 1; dir += 2) {
            int ny = py + dir, in = 0;
            if (ny < 0 || ny >= cv->h)
                continue;
            for (i = xl; i <= xr; i++) {
                int open = !seen[ny * cv->w + i] && cv_get(cv, i, ny) != border;
                if (open && !in && sp < cap) {
                    stk[2 * sp] = (short)i;
                    stk[2 * sp + 1] = (short)ny;
                    sp++;
                }
                in = open;
            }
        }
        /* paint after scanning neighbours: they read the old colours */
        fill_span(g, cv, xl, xr, py);
    }
    free(seen);
    free(stk);
}

static int is_pair_cmd(int ch) { return ch == 'P' || ch == 'p' || ch == 'l'; }

int rip_gfx_handles(const struct rip_cmd *c)
{
    if (c->level != 0)
        return 0;
    switch (c->ch) {
    case 'c': case 'W': case '=': case 'S': case 's': case 'm': case 'X': case 'L': case 'R': case 'B':
    case 'C': case 'O': case 'o': case 'A': case 'V': case 'I': case 'i': case 'Z': case 'P': case 'p':
    case 'l': case 'F': case 'E': case '#': case 'v': case 'Y': case 'T': case '@': case '*':
        return 1;
    }
    return 0;
}

int rip_gfx_exec(struct rip_gfx *g, struct cv *cv, const struct rip_cmd *c)
{
    const long *a = c->arg;
    int n = c->nargs, *pt, np;
    if (!rip_gfx_handles(c))
        return 0;
    if (is_pair_cmd(c->ch) && (n < 1 || n < 1 + 2 * (int)a[0]))
        return 1;
    switch (c->ch) {
    case 'c': g->color = (int)(a[0] & 15); break;
    case 'W': g->wmode = a[0] == 1 ? 1 : 0; break;
    case '=':
        g->lstyle = (int)(a[0] > 4 ? 0 : a[0]);
        g->lpat = (unsigned)a[1] & 0xFFFF;
        g->thick = a[2] == 3 ? 3 : 1;
        break;
    case 'S': g->fpat = (int)(a[0] > 12 ? 1 : a[0]); g->fcolor = (int)(a[1] & 15); break;
    case 's': {
        int i;
        for (i = 0; i < 8; i++)
            g->upat[i] = (unsigned char)a[i];
        g->fpat = 12;
        g->fcolor = (int)(a[8] & 15);
        break;
    }
    case 'm': g->px = (int)a[0]; g->py = (int)a[1]; break;
    case 'X': gput(g, cv, (int)a[0], (int)a[1], g->color); break;
    case 'L': styled_line(g, cv, (int)a[0], (int)a[1], (int)a[2], (int)a[3]); break;
    case 'R': {
        int x0 = (int)a[0], y0 = (int)a[1], x1 = (int)a[2], y1 = (int)a[3], t;
        if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
        if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
        rect_outline(g, cv, x0, y0, x1, y1);
        break;
    }
    case 'B': { /* a bar: the fill style over the box, no outline */
        int x0 = (int)a[0], y0 = (int)a[1], x1 = (int)a[2], y1 = (int)a[3], y, t;
        if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
        if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 >= cv->w) x1 = cv->w - 1;
        if (y1 >= cv->h) y1 = cv->h - 1;
        for (y = y0; y <= y1; y++)
            fill_span(g, cv, x0, x1, y);
        break;
    }
    case '#': case 'E':
        if (c->ch == 'E') /* erase the viewport */
            cv_rect(cv, g->vx0, g->vy0, g->vx1, g->vy1, 0);
        break;
    case '*': /* reset windows: the viewport is the whole canvas again, cleared */
        g->vx0 = 0;
        g->vy0 = 0;
        g->vx1 = cv->w - 1;
        g->vy1 = cv->h - 1;
        cv_rect(cv, 0, 0, cv->w - 1, cv->h - 1, 0);
        break;
    case 'v': { /* viewport, clamped to the canvas */
        int x0 = (int)a[0], y0 = (int)a[1], x1 = (int)a[2], y1 = (int)a[3], t;
        if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
        if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
        g->vx0 = x0 < 0 ? 0 : x0;
        g->vy0 = y0 < 0 ? 0 : y0;
        g->vx1 = x1 >= cv->w ? cv->w - 1 : x1;
        g->vy1 = y1 >= cv->h ? cv->h - 1 : y1;
        break;
    }
    case 'Y':
        g->font = (int)a[0];
        g->fdir = a[1] == 1 ? 1 : 0;
        g->fsize = (int)(a[2] < 1 ? 1 : a[2] > 10 ? 10 : a[2]);
        break;
    case 'T': case '@': {
        int i, r, k, s = g->fsize, x, y, sx, sy;
        if (c->ch == '@') {
            g->px = (int)a[0];
            g->py = (int)a[1];
        }
        if (g->stroke && rip_font_is_stroke(g->font)) { /* RIP-3b: Hershey strokes through the line primitive */
            struct rip_gfx pen1 = *g;
            struct stroke_ctx sc;
            pen1.thick = 1;
            pen1.lstyle = 0;
            sc.g = &pen1;
            sc.cv = cv;
            for (i = 0; i < c->ntext; i++) {
                int adv = rip_font_glyph(g->font, c->text[i], s, g->fdir, g->px, g->py, stroke_seg, &sc);
                if (g->fdir)
                    g->py -= adv;
                else
                    g->px += adv;
            }
            break;
        }
        for (i = 0; i < c->ntext; i++) {
            for (r = 0; r < 8; r++) {
                unsigned row = font8[c->text[i]][r];
                for (k = 0; k < 8; k++) {
                    if (!((row >> (7 - k)) & 1))
                        continue;
                    /* across: glyph column k to the right; up: rotated a quarter turn
                     * counter-clockwise, the glyph's top toward the left */
                    x = g->fdir ? g->px + r * s : g->px + k * s;
                    y = g->fdir ? g->py - (k + 1) * s : g->py + r * s;
                    for (sy = 0; sy < s; sy++)
                        for (sx = 0; sx < s; sx++)
                            gput(g, cv, x + sx, y + sy, g->color);
                }
            }
            if (g->fdir)
                g->py -= 8 * s;
            else
                g->px += 8 * s;
        }
        break;
    }
    case 'C': case 'O': case 'o': case 'A': case 'V': case 'I': case 'i': {
        int cx = (int)a[0], cy = (int)a[1], sa = 0, ea = 0, rx, ry, pie = 0, fillit = 0;
        if (c->ch == 'C') { rx = ry = (int)a[2]; }
        else if (c->ch == 'o') { rx = (int)a[2]; ry = (int)a[3]; fillit = 1; }
        else if (c->ch == 'A' || c->ch == 'I') { sa = (int)a[2]; ea = (int)a[3]; rx = ry = (int)a[4]; pie = c->ch == 'I'; }
        else { sa = (int)a[2]; ea = (int)a[3]; rx = (int)a[4]; ry = (int)a[5]; pie = c->ch == 'i'; }
        if (rx < 0) rx = 0;
        if (ry < 0) ry = 0;
        pt = malloc((size_t)(ARC_MAX + 2) * 2 * sizeof(int));
        if (!pt)
            return 1;
        np = arc_points(cx, cy, sa, ea, rx, ry, pt, ARC_MAX);
        if (pie || fillit) {
            if (pie) { /* the centre closes the wedge */
                pt[2 * np] = cx;
                pt[2 * np + 1] = cy;
                np++;
            }
            fill_poly(g, cv, pt, np);
            if (pie) {
                poly_line(g, cv, pt, np);
                line(g, cv, cx, cy, pt[0], pt[1], 0xFFFF);
            } else {
                poly_line(g, cv, pt, np);
            }
        } else {
            poly_line(g, cv, pt, np);
        }
        free(pt);
        break;
    }
    case 'Z': { /* cubic Bezier, a[8] segments */
        long cnt = a[8] < 1 ? 1 : a[8] > 1000 ? 1000 : a[8], i;
        int lx = (int)a[0], ly = (int)a[1];
        for (i = 1; i <= cnt; i++) {
            long u = i * 1024 / cnt, b = 1024 - u;
            long c0 = b * b / 1024 * b / 1024, c1 = 3 * (b * b / 1024) * u / 1024;
            long c2 = 3 * (b * u / 1024) * u / 1024, c3 = u * u / 1024 * u / 1024;
            int x = (int)((c0 * a[0] + c1 * a[2] + c2 * a[4] + c3 * a[6] + 512) / 1024);
            int y = (int)((c0 * a[1] + c1 * a[3] + c2 * a[5] + c3 * a[7] + 512) / 1024);
            styled_line(g, cv, lx, ly, x, y);
            lx = x;
            ly = y;
        }
        break;
    }
    case 'P': case 'p': case 'l': {
        int cnt = (int)a[0], i;
        if (cnt < 1)
            break;
        pt = malloc((size_t)cnt * 2 * sizeof(int));
        if (!pt)
            return 1;
        for (i = 0; i < 2 * cnt; i++)
            pt[i] = (int)a[1 + i];
        if (c->ch == 'p')
            fill_poly(g, cv, pt, cnt);
        for (i = 1; i < cnt; i++)
            styled_line(g, cv, pt[2 * i - 2], pt[2 * i - 1], pt[2 * i], pt[2 * i + 1]);
        if (c->ch != 'l' && cnt > 2)
            styled_line(g, cv, pt[2 * cnt - 2], pt[2 * cnt - 1], pt[0], pt[1]);
        free(pt);
        break;
    }
    case 'F': {
        /* the seed is flooded with the fill style up to pixels of the border colour */
        flood(g, cv, (int)a[0], (int)a[1], (int)(a[2] & 15));
        break;
    }
    }
    return 1;
}
