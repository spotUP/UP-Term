#include "ttf.h"

/* Raster coordinates are 26.6 fixed point (64 subpixels a pixel). One row
 * strip of a line deposits d (its height in subpixels, signed by direction)
 * times twice the area right of it, so a covered pixel sums to FULL. */
#define SUB 64
#define FULL (SUB * SUB * 2)
#define MAX_GRID 512
#define MAX_DEPTH 4      /* composite nesting */
#define MAX_SPAN 16384   /* longest line piece in subpixels: dx * dy stays in 32 bits */
#define MAX_COORD 65535L /* font units after a composite's transform */
#define FLAT 16          /* a curve is a line when |p0 - 2 p1 + p2| <= FLAT (1/16 px off) */
#define MAX_SPLIT 8

#define TAG_HEAD 0x68656164UL
#define TAG_MAXP 0x6D617870UL
#define TAG_HHEA 0x68686561UL
#define TAG_HMTX 0x686D7478UL
#define TAG_LOCA 0x6C6F6361UL
#define TAG_GLYF 0x676C7966UL
#define TAG_CMAP 0x636D6170UL

struct ttf_font {
    ttf_io io;
    unsigned long glyf_off, glyf_len;
    unsigned char *loca;
    int loca_long;
    unsigned char *hmtx;
    int nhm;
    unsigned char *cmap;
    unsigned long cmap_len;
    int cmap_fmt;
    int upem, nglyphs;
    long *acc;           /* the render accumulator, kept between renders */
    unsigned long acc_n;
};

/* a composite's transform: a b c d in 2.14, dx dy in font units */
typedef struct xform {
    long a, b, c, d, dx, dy;
} xform;

typedef struct rast {
    long *acc;
    int w, h, stride;
    long xs, ys;         /* pixels per font unit, 16.16 */
    long ox, by;         /* the origin, 26.6 */
} rast;

static unsigned be16(const unsigned char *p)
{
    return ((unsigned)p[0] << 8) | p[1];
}

static long s16(const unsigned char *p)
{
    long v = (long)be16(p);
    return v >= 0x8000L ? v - 65536L : v;
}

static unsigned long be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) |
           (unsigned long)p[3];
}

static long labs_(long v)
{
    return v < 0 ? -v : v;
}

/* v >> 14 rounded toward zero for either sign (C89 leaves a negative shift to the compiler) */
static long sh14(long v)
{
    return v >= 0 ? v >> 14 : -((-v) >> 14);
}

/* n / d rounded toward zero, d != 0 */
static long sdiv(long n, long d)
{
    long q = labs_(n) / labs_(d);
    return (n < 0) != (d < 0) ? -q : q;
}

/* a font unit coordinate (|v| <= MAX_COORD) at s pixels a unit (16.16, < 1.0) in 26.6 */
static long scale(long v, long s)
{
    unsigned long m = ((unsigned long)labs_(v) * (unsigned long)s + 512UL) >> 10;
    return v < 0 ? -(long)m : (long)m;
}

static int rd(const ttf_font *f, unsigned long off, void *buf, long len)
{
    return f->io.read(f->io.ctx, off, buf, len) == len ? TTF_OK : TTF_TRUNCATED;
}

static void *grab(const ttf_font *f, unsigned long size)
{
    return f->io.alloc(f->io.ctx, size ? size : 1);
}

static void drop(const ttf_font *f, void *p)
{
    if (p)
        f->io.free(f->io.ctx, p);
}

/* reads len bytes at off into a new block, *out */
static int load(ttf_font *f, unsigned long off, unsigned long len, unsigned char **out)
{
    int err;
    *out = (unsigned char *)grab(f, len);
    if (!*out)
        return TTF_NO_MEMORY;
    err = rd(f, off, *out, (long)len);
    if (err) {
        drop(f, *out);
        *out = 0;
    }
    return err;
}

void ttf_close(ttf_font *f)
{
    if (!f)
        return;
    drop(f, f->loca);
    drop(f, f->hmtx);
    drop(f, f->cmap);
    drop(f, f->acc);
    f->io.free(f->io.ctx, f);
}

/* the best Unicode subtable: format 12, else format 4 */
static int pick_cmap(ttf_font *f, unsigned long base, unsigned long len)
{
    unsigned char hd[8], *recs;
    unsigned long best_off = 0, best_len = 0;
    int nt, i, best = 0, err;
    if (len < 4)
        return TTF_BAD_TABLE;
    if ((err = rd(f, base, hd, 4)) != TTF_OK)
        return err;
    nt = (int)be16(hd + 2);
    if (4UL + (unsigned long)nt * 8 > len)
        return TTF_BAD_TABLE;
    if ((err = load(f, base + 4, (unsigned long)nt * 8, &recs)) != TTF_OK)
        return err;
    for (i = 0; i < nt; i++) {
        unsigned pid = be16(recs + i * 8), eid = be16(recs + i * 8 + 2);
        unsigned long off = be32(recs + i * 8 + 4), sl;
        int fmt;
        if (!(pid == 0 || (pid == 3 && (eid == 1 || eid == 10))) || off + 8 > len)
            continue;
        if ((err = rd(f, base + off, hd, 8)) != TTF_OK) {
            drop(f, recs);
            return err;
        }
        fmt = (int)be16(hd);
        if (fmt == 12)
            sl = be32(hd + 4);
        else if (fmt == 4)
            sl = be16(hd + 2);
        else
            continue;
        if (fmt > best && sl <= len - off) {
            best = fmt;
            best_off = off;
            best_len = sl;
        }
    }
    drop(f, recs);
    if (!best)
        return TTF_NO_CMAP;
    if (best == 12 ? best_len < 16 : best_len < 16)
        return TTF_BAD_TABLE;
    if ((err = load(f, base + best_off, best_len, &f->cmap)) != TTF_OK)
        return err;
    f->cmap_len = best_len;
    f->cmap_fmt = best;
    if (best == 12) {
        if (16 + be32(f->cmap + 12) * 12 > best_len)
            return TTF_BAD_TABLE;
    } else {
        unsigned seg2 = be16(f->cmap + 6);
        if ((seg2 & 1) || 16UL + 4UL * seg2 > best_len)
            return TTF_BAD_TABLE;
    }
    return TTF_OK;
}

static int open_tables(ttf_font *f)
{
    static const unsigned long want[7] = { TAG_HEAD, TAG_MAXP, TAG_HHEA, TAG_HMTX,
                                           TAG_LOCA, TAG_GLYF, TAG_CMAP };
    unsigned long off[7], len[7], ver, need;
    unsigned char hd[54], *dir;
    int n, i, k, err;
    if ((err = rd(f, 0, hd, 12)) != TTF_OK)
        return err;
    ver = be32(hd);
    if (ver != 0x00010000UL && ver != 0x74727565UL) /* "true": Apple's tag */
        return TTF_NOT_TTF;
    n = (int)be16(hd + 4);
    if ((err = load(f, 12, (unsigned long)n * 16, &dir)) != TTF_OK)
        return err;
    for (k = 0; k < 7; k++)
        off[k] = len[k] = 0;
    for (i = 0; i < n; i++)
        for (k = 0; k < 7; k++)
            if (be32(dir + i * 16) == want[k]) {
                off[k] = be32(dir + i * 16 + 8);
                len[k] = be32(dir + i * 16 + 12);
            }
    drop(f, dir);
    for (k = 0; k < 7; k++)
        if (!off[k])
            return TTF_NO_TABLE;

    if (len[0] < 54)
        return TTF_BAD_TABLE;
    if ((err = rd(f, off[0], hd, 54)) != TTF_OK)
        return err;
    f->upem = (int)be16(hd + 18);
    f->loca_long = (int)s16(hd + 50);
    if (f->upem < 16 || f->upem > 16384 || (f->loca_long != 0 && f->loca_long != 1))
        return TTF_BAD_TABLE;

    if (len[1] < 6)
        return TTF_BAD_TABLE;
    if ((err = rd(f, off[1], hd, 6)) != TTF_OK)
        return err;
    f->nglyphs = (int)be16(hd + 4);

    if (len[2] < 36)
        return TTF_BAD_TABLE;
    if ((err = rd(f, off[2], hd, 36)) != TTF_OK)
        return err;
    f->nhm = (int)be16(hd + 34);
    if (!f->nglyphs || !f->nhm || f->nhm > f->nglyphs || (unsigned long)f->nhm * 4 > len[3])
        return TTF_BAD_TABLE;
    if ((err = load(f, off[3], (unsigned long)f->nhm * 4, &f->hmtx)) != TTF_OK)
        return err;

    need = ((unsigned long)f->nglyphs + 1) * (f->loca_long ? 4 : 2);
    if (need > len[4])
        return TTF_BAD_TABLE;
    if ((err = load(f, off[4], need, &f->loca)) != TTF_OK)
        return err;
    f->glyf_off = off[5];
    f->glyf_len = len[5];
    return pick_cmap(f, off[6], len[6]);
}

int ttf_open(const ttf_io *io, ttf_font **out)
{
    ttf_font *f;
    int err;
    *out = 0;
    f = (ttf_font *)io->alloc(io->ctx, sizeof *f);
    if (!f)
        return TTF_NO_MEMORY;
    f->io = *io;
    if ((err = open_tables(f)) != TTF_OK) {
        ttf_close(f);
        return err;
    }
    *out = f;
    return TTF_OK;
}

int ttf_units_per_em(const ttf_font *f)
{
    return f->upem;
}

int ttf_num_glyphs(const ttf_font *f)
{
    return f->nglyphs;
}

int ttf_advance(const ttf_font *f, unsigned gid)
{
    if (gid >= (unsigned)f->nglyphs)
        return 0;
    if (gid >= (unsigned)f->nhm)
        gid = (unsigned)f->nhm - 1;
    return (int)be16(f->hmtx + gid * 4);
}

unsigned ttf_glyph_index(const ttf_font *f, unsigned long cp)
{
    const unsigned char *t = f->cmap;
    if (f->cmap_fmt == 12) {
        long lo = 0, hi = (long)be32(t + 12) - 1;
        while (lo <= hi) {
            long mid = (lo + hi) / 2;
            const unsigned char *g = t + 16 + mid * 12;
            if (cp < be32(g))
                hi = mid - 1;
            else if (cp > be32(g + 4))
                lo = mid + 1;
            else {
                unsigned long gid = be32(g + 8) + (cp - be32(g));
                return gid < (unsigned long)f->nglyphs ? (unsigned)gid : 0;
            }
        }
        return 0;
    } else {
        unsigned seg = be16(t + 6) / 2;
        long lo = 0, hi = (long)seg - 1, i = -1;
        if (cp > 0xFFFFUL)
            return 0;
        while (lo <= hi) { /* the first segment whose end is at or past cp */
            long mid = (lo + hi) / 2;
            if (be16(t + 14 + mid * 2) >= cp) {
                i = mid;
                hi = mid - 1;
            } else
                lo = mid + 1;
        }
        if (i >= 0) {
            unsigned long start = be16(t + 16 + seg * 2 + i * 2);
            unsigned delta = be16(t + 16 + seg * 4 + i * 2);
            unsigned long ro_at = 16UL + seg * 6 + (unsigned long)i * 2;
            unsigned ro = be16(t + ro_at), g;
            if (cp < start)
                return 0;
            if (!ro)
                g = (unsigned)((cp + delta) & 0xFFFFUL);
            else {
                unsigned long at = ro_at + ro + (cp - start) * 2;
                if (at + 2 > f->cmap_len)
                    return 0;
                g = be16(t + at);
                if (g)
                    g = (g + delta) & 0xFFFFU;
            }
            return g < (unsigned)f->nglyphs ? g : 0;
        }
        return 0;
    }
}

static unsigned long loca_at(const ttf_font *f, unsigned i)
{
    return f->loca_long ? be32(f->loca + i * 4) : (unsigned long)be16(f->loca + i * 2) * 2;
}

/* one line piece, inside 0 <= x <= w and short: its signed area into the rows it crosses */
static void draw_line(rast *r, long x0, long y0, long x1, long y1)
{
    long dir = 1, dx, dy, row, rend;
    if (y0 == y1)
        return;
    if (y0 > y1) {
        long t = x0;
        x0 = x1;
        x1 = t;
        t = y0;
        y0 = y1;
        y1 = t;
        dir = -1;
    }
    dx = x1 - x0;
    dy = y1 - y0;
    row = y0 >= 0 ? y0 / SUB : 0;
    rend = (y1 + SUB - 1) / SUB;
    if (y1 <= 0)
        return;
    if (rend > r->h)
        rend = r->h;
    for (; row < rend; row++) {
        long ya = row * SUB > y0 ? row * SUB : y0, yb = (row + 1) * SUB < y1 ? (row + 1) * SUB : y1;
        long xa, xb, xs, xe, d, prev = 0, i, i0, i1;
        long *a = r->acc + row * r->stride;
        if (yb <= ya)
            continue;
        xa = x0 + sdiv(dx * (ya - y0), dy);
        xb = x0 + sdiv(dx * (yb - y0), dy);
        xs = xa < xb ? xa : xb;
        xe = xa < xb ? xb : xa;
        d = (yb - ya) * dir;
        i0 = xs / SUB;
        i1 = (xe + SUB - 1) / SUB;
        /* pixel i holds d times the mean over it of how much of the line lies left:
         * the difference of twice the integral H2 at its edges */
        for (i = i0; i <= i1; i++) {
            long t, h0, h1, c;
            for (t = 0, h0 = h1 = 0; t < 2; t++) {
                long at = (i + t) * SUB, h;
                if (at <= xs)
                    h = 0;
                else if (at >= xe)
                    h = 2 * at - xs - xe;
                else
                    h = (at - xs) * (at - xs) / (xe - xs);
                if (t)
                    h1 = h;
                else
                    h0 = h;
            }
            c = d * (h1 - h0);
            a[i] += c - prev;
            prev = c;
        }
    }
}

/* a line piece in x: left of the grid it is a line on x = 0, right of it nothing */
static void clip_line(rast *r, long x0, long y0, long x1, long y1)
{
    long wx = (long)r->w * SUB;
    if (x0 >= wx && x1 >= wx)
        return;
    if (x0 <= 0 && x1 <= 0) {
        draw_line(r, 0, y0, 0, y1);
        return;
    }
    if ((x0 < 0) != (x1 < 0)) {
        long yc = y0 + sdiv((y1 - y0) * -x0, x1 - x0);
        clip_line(r, x0, y0, 0, yc);
        clip_line(r, 0, yc, x1, y1);
        return;
    }
    if ((x0 > wx) != (x1 > wx)) {
        long yc = y0 + sdiv((y1 - y0) * (wx - x0), x1 - x0);
        clip_line(r, x0, y0, wx, yc);
        clip_line(r, wx, yc, x1, y1);
        return;
    }
    draw_line(r, x0, y0, x1, y1);
}

static void add_line(rast *r, long x0, long y0, long x1, long y1)
{
    if (labs_(x1 - x0) > MAX_SPAN || labs_(y1 - y0) > MAX_SPAN) {
        long mx = (x0 + x1) / 2, my = (y0 + y1) / 2;
        add_line(r, x0, y0, mx, my);
        add_line(r, mx, my, x1, y1);
        return;
    }
    clip_line(r, x0, y0, x1, y1);
}

static void add_quad(rast *r, long x0, long y0, long x1, long y1, long x2, long y2, int depth)
{
    long ax = x0 - 2 * x1 + x2, ay = y0 - 2 * y1 + y2;
    if (depth >= MAX_SPLIT || (labs_(ax) <= FLAT && labs_(ay) <= FLAT)) {
        add_line(r, x0, y0, x2, y2);
        return;
    } else {
        long ax1 = (x0 + x1) / 2, ay1 = (y0 + y1) / 2, bx1 = (x1 + x2) / 2, by1 = (y1 + y2) / 2;
        long mx = (ax1 + bx1) / 2, my = (ay1 + by1) / 2;
        add_quad(r, x0, y0, ax1, ay1, mx, my, depth + 1);
        add_quad(r, mx, my, bx1, by1, x2, y2, depth + 1);
    }
}

/* one closed contour of n points (raster coordinates); bit 0 of fl: on the curve */
static void add_contour(rast *r, const long *x, const long *y, const unsigned char *fl, int n)
{
    long sx, sy, cx, cy, qx = 0, qy = 0;
    int k, j, begin, ctrl = 0;
    if (n < 2)
        return;
    for (k = 0; k < n && !(fl[k] & 1); k++)
        ;
    if (k < n) {
        sx = x[k];
        sy = y[k];
        begin = k + 1;
    } else { /* no point on the curve: start between the last and the first */
        sx = (x[n - 1] + x[0]) / 2;
        sy = (y[n - 1] + y[0]) / 2;
        begin = 0;
    }
    cx = sx;
    cy = sy;
    for (j = 0; j < n; j++) {
        int i = (begin + j) % n;
        if (fl[i] & 1) {
            if (ctrl)
                add_quad(r, cx, cy, qx, qy, x[i], y[i], 0);
            else
                add_line(r, cx, cy, x[i], y[i]);
            cx = x[i];
            cy = y[i];
            ctrl = 0;
        } else {
            if (ctrl) {
                long mx = (qx + x[i]) / 2, my = (qy + y[i]) / 2;
                add_quad(r, cx, cy, qx, qy, mx, my, 0);
                cx = mx;
                cy = my;
            }
            qx = x[i];
            qy = y[i];
            ctrl = 1;
        }
    }
    if (ctrl)
        add_quad(r, cx, cy, qx, qy, sx, sy, 0);
    else
        add_line(r, cx, cy, sx, sy);
}

static int draw_simple(ttf_font *f, rast *r, const unsigned char *g, unsigned long len, int nc,
                       const xform *t)
{
    unsigned long p, npts, i;
    long *x, *y, v;
    unsigned char *fl;
    int c, start;
    if (!nc)
        return TTF_OK;
    if (10UL + (unsigned long)nc * 2 + 2 > len)
        return TTF_BAD_TABLE;
    npts = (unsigned long)be16(g + 10 + (nc - 1) * 2) + 1;
    p = 10UL + (unsigned long)nc * 2;
    p += 2 + be16(g + p);
    if (p > len)
        return TTF_BAD_TABLE;
    x = (long *)grab(f, npts * (2 * sizeof(long) + 1));
    if (!x)
        return TTF_NO_MEMORY;
    y = x + npts;
    fl = (unsigned char *)(y + npts);
    for (i = 0; i < npts;) {
        unsigned char b;
        if (p >= len)
            goto bad;
        b = g[p++];
        fl[i++] = b;
        if (b & 8) {
            unsigned rep;
            if (p >= len)
                goto bad;
            for (rep = g[p++]; rep && i < npts; rep--)
                fl[i++] = b;
        }
    }
    for (i = 0, v = 0; i < npts; i++) {
        if (fl[i] & 2) {
            if (p >= len)
                goto bad;
            v += fl[i] & 0x10 ? (long)g[p] : -(long)g[p];
            p++;
        } else if (!(fl[i] & 0x10)) {
            if (p + 2 > len)
                goto bad;
            v += s16(g + p);
            p += 2;
        }
        x[i] = v;
    }
    for (i = 0, v = 0; i < npts; i++) {
        if (fl[i] & 4) {
            if (p >= len)
                goto bad;
            v += fl[i] & 0x20 ? (long)g[p] : -(long)g[p];
            p++;
        } else if (!(fl[i] & 0x20)) {
            if (p + 2 > len)
                goto bad;
            v += s16(g + p);
            p += 2;
        }
        y[i] = v;
    }
    for (i = 0; i < npts; i++) {
        long fx = sh14(t->a * x[i]) + sh14(t->c * y[i]) + t->dx;
        long fy = sh14(t->b * x[i]) + sh14(t->d * y[i]) + t->dy;
        if (labs_(fx) > MAX_COORD || labs_(fy) > MAX_COORD) {
            drop(f, x);
            return TTF_TOO_BIG;
        }
        x[i] = r->ox + scale(fx, r->xs);
        y[i] = r->by - scale(fy, r->ys);
    }
    for (c = 0, start = 0; c < nc; c++) {
        int end = (int)be16(g + 10 + c * 2);
        if (end < start || (unsigned long)end >= npts)
            goto bad;
        add_contour(r, x + start, y + start, fl + start, end - start + 1);
        start = end + 1;
    }
    drop(f, x);
    return TTF_OK;
bad:
    drop(f, x);
    return TTF_BAD_TABLE;
}

static int draw_glyph(ttf_font *f, rast *r, unsigned gid, const xform *t, int depth);

static int draw_composite(ttf_font *f, rast *r, const unsigned char *g, unsigned long len,
                          const xform *t, int depth)
{
    unsigned long p = 10;
    unsigned flags;
    if (depth >= MAX_DEPTH)
        return TTF_BAD_TABLE;
    do {
        xform l, n;
        unsigned cg;
        int err;
        if (p + 4 > len)
            return TTF_BAD_TABLE;
        flags = be16(g + p);
        cg = be16(g + p + 2);
        p += 4;
        if (!(flags & 2)) /* anchored by point numbers: not supported */
            return TTF_BAD_TABLE;
        l.a = l.d = 16384;
        l.b = l.c = 0;
        if (flags & 1) {
            if (p + 4 > len)
                return TTF_BAD_TABLE;
            l.dx = s16(g + p);
            l.dy = s16(g + p + 2);
            p += 4;
        } else {
            if (p + 2 > len)
                return TTF_BAD_TABLE;
            l.dx = g[p] >= 128 ? (long)g[p] - 256 : (long)g[p];
            l.dy = g[p + 1] >= 128 ? (long)g[p + 1] - 256 : (long)g[p + 1];
            p += 2;
        }
        if (flags & 8) {
            if (p + 2 > len)
                return TTF_BAD_TABLE;
            l.a = l.d = s16(g + p);
            p += 2;
        } else if (flags & 0x40) {
            if (p + 4 > len)
                return TTF_BAD_TABLE;
            l.a = s16(g + p);
            l.d = s16(g + p + 2);
            p += 4;
        } else if (flags & 0x80) {
            if (p + 8 > len)
                return TTF_BAD_TABLE;
            l.a = s16(g + p);
            l.b = s16(g + p + 2);
            l.c = s16(g + p + 4);
            l.d = s16(g + p + 6);
            p += 8;
        }
        n.a = sh14(t->a * l.a) + sh14(t->c * l.b);
        n.b = sh14(t->b * l.a) + sh14(t->d * l.b);
        n.c = sh14(t->a * l.c) + sh14(t->c * l.d);
        n.d = sh14(t->b * l.c) + sh14(t->d * l.d);
        n.dx = sh14(t->a * l.dx) + sh14(t->c * l.dy) + t->dx;
        n.dy = sh14(t->b * l.dx) + sh14(t->d * l.dy) + t->dy;
        if (labs_(n.a) > 32768L || labs_(n.b) > 32768L || labs_(n.c) > 32768L ||
            labs_(n.d) > 32768L || labs_(n.dx) > MAX_COORD || labs_(n.dy) > MAX_COORD)
            return TTF_TOO_BIG;
        if ((err = draw_glyph(f, r, cg, &n, depth + 1)) != TTF_OK)
            return err;
    } while (flags & 0x20);
    return TTF_OK;
}

static int draw_glyph(ttf_font *f, rast *r, unsigned gid, const xform *t, int depth)
{
    unsigned long o0, o1;
    unsigned char *g;
    int err, nc;
    if (gid >= (unsigned)f->nglyphs)
        return TTF_BAD_TABLE;
    o0 = loca_at(f, gid);
    o1 = loca_at(f, gid + 1);
    if (o1 < o0 || o1 > f->glyf_len)
        return TTF_BAD_TABLE;
    if (o1 == o0)
        return TTF_OK;
    if (o1 - o0 < 10)
        return TTF_BAD_TABLE;
    if ((err = load(f, f->glyf_off + o0, o1 - o0, &g)) != TTF_OK)
        return err;
    nc = (int)s16(g);
    if (nc >= 0)
        err = draw_simple(f, r, g, o1 - o0, nc, t);
    else
        err = draw_composite(f, r, g, o1 - o0, t, depth);
    drop(f, g);
    return err;
}

int ttf_render(ttf_font *f, unsigned gid, long xscale, long yscale, int ox, int baseline,
               unsigned char *cov, int w, int h)
{
    static const xform id = { 16384, 0, 0, 16384, 0, 0 };
    unsigned long need, i;
    rast r;
    int err, any = 0, x, y;
    if (w < 1 || h < 1 || w > MAX_GRID || h > MAX_GRID || xscale <= 0 || yscale <= 0 ||
        xscale >= 65536L || yscale >= 65536L)
        return -TTF_TOO_BIG;
    need = (unsigned long)(w + 2) * (unsigned long)h;
    if (f->acc_n < need) {
        drop(f, f->acc);
        f->acc_n = 0;
        f->acc = (long *)grab(f, need * sizeof(long));
        if (!f->acc)
            return -TTF_NO_MEMORY;
        f->acc_n = need;
    }
    for (i = 0; i < need; i++)
        f->acc[i] = 0;
    r.acc = f->acc;
    r.w = w;
    r.h = h;
    r.stride = w + 2;
    r.xs = xscale;
    r.ys = yscale;
    r.ox = (long)ox * SUB;
    r.by = (long)baseline * SUB;
    if ((err = draw_glyph(f, &r, gid, &id, 0)) != TTF_OK)
        return -err;
    for (y = 0; y < h; y++) {
        const long *a = r.acc + (long)y * r.stride;
        unsigned char *o = cov + (long)y * w;
        long sum = 0;
        for (x = 0; x < w; x++) {
            long v, c;
            sum += a[x];
            v = labs_(sum);
            if (v > FULL)
                v = FULL;
            v = (v * 255 + FULL / 2) / FULL;
            if (v)
                any = 1;
            c = o[x] + v;
            o[x] = (unsigned char)(c > 255 ? 255 : c);
        }
    }
    return any;
}

void ttf_threshold(const unsigned char *cov, int w, int h, unsigned char *mask, int bpr)
{
    ttf_threshold_at(cov, w, h, mask, bpr, 128);
}

void ttf_threshold_at(const unsigned char *cov, int w, int h, unsigned char *mask, int bpr, int level)
{
    int x, y, b;
    for (y = 0; y < h; y++) {
        unsigned char *m = mask + (long)y * bpr;
        for (b = 0; b < bpr; b++)
            m[b] = 0;
        for (x = 0; x < w && x / 8 < bpr; x++)
            if (cov[(long)y * w + x] >= level)
                m[x / 8] |= (unsigned char)(0x80 >> (x & 7));
    }
}

void ttf_embolden(const unsigned char *cov, int w, int h, unsigned char *out)
{
    int x, y;
    for (y = 0; y < h; y++) {
        const unsigned char *c = cov + (long)y * w;
        unsigned char *o = out + (long)y * w;
        for (x = w - 1; x > 0; x--)
            o[x] = c[x] > c[x - 1] ? c[x] : c[x - 1];
        if (w > 0)
            o[0] = c[0];
    }
}
