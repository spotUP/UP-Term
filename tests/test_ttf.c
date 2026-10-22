/* render/ttf: the in-handler TrueType rasterizer draws the Symbols Nerd
 * Font subset as FreeType does (tests/data/ttf_golden.h, no hinting), reads
 * code points past U+FFFF through cmap format 12, and refuses a damaged
 * font without crashing or leaking. Plan
 * thoughts/shared/plans/2026-10-22-handler-rasterizer.md, R1-R2. */
#include <stdlib.h>
#include <string.h>
#include "harness.h"
#include "../render/ttf.h"
#include "data/symnerd_subset.h"
#include "data/ttf_golden.h"

/* a font in memory; fail_after counts allocations left before one fails (-1 never) */
typedef struct mem {
    const unsigned char *d;
    long n;
    long fail_after, live, allocs;
} mem;

static long m_read(void *ctx, unsigned long off, void *buf, long len)
{
    mem *m = (mem *)ctx;
    if ((long)off > m->n)
        return -1;
    if (len > m->n - (long)off)
        len = m->n - (long)off;
    memcpy(buf, m->d + off, (size_t)len);
    return len;
}

static void *m_alloc(void *ctx, unsigned long size)
{
    mem *m = (mem *)ctx;
    void *p;
    if (m->fail_after == 0)
        return 0;
    if (m->fail_after > 0)
        m->fail_after--;
    p = calloc(1, size);
    if (p) {
        m->live++;
        m->allocs++;
    }
    return p;
}

static void m_free(void *ctx, void *p)
{
    ((mem *)ctx)->live--;
    free(p);
}

static int open_mem(mem *m, const unsigned char *d, long n, ttf_font **f)
{
    ttf_io io;
    m->d = d;
    m->n = n;
    m->fail_after = -1;
    m->live = m->allocs = 0;
    io.read = m_read;
    io.alloc = m_alloc;
    io.free = m_free;
    io.ctx = m;
    return ttf_open(&io, f);
}

static unsigned rd16(const unsigned char *p)
{
    return ((unsigned)p[0] << 8) | p[1];
}

static unsigned long rd32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) |
           p[3];
}

static void wr16(unsigned char *p, long v)
{
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

static void wr32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static const unsigned long nerd_cps[11] = { 0xE0A0UL,  0xE615UL,  0xEA74UL,  0xEBCFUL,
                                            0xF011UL,  0xF021UL,  0xF003BUL, 0xF09D1UL,
                                            0xF0249UL, 0xF14DEUL, 0xF0B4CUL };

static void test_subset_font(void)
{
    mem m;
    ttf_font *f;
    int i;
    CHECK_INT(open_mem(&m, symnerd_subset, SYMNERD_SUBSET_LEN, &f), TTF_OK);
    if (!f)
        return;
    CHECK_INT(ttf_units_per_em(f), 2048);
    for (i = 0; i < 11; i++) {
        unsigned g = ttf_glyph_index(f, nerd_cps[i]);
        CHECK(g != 0);
        CHECK_INT(ttf_advance(f, g), 2048);
    }
    CHECK_INT(ttf_glyph_index(f, 0x2588UL), 0); /* not in the font */
    CHECK_INT(ttf_glyph_index(f, 0x10FFFFUL), 0);
    ttf_close(f);
    CHECK_INT(m.live, 0);
}

/* every golden within 24 levels a pixel and at most 2% of 1-bit pixels apart */
static void test_goldens(void)
{
    static unsigned char cov[16 * 32];
    mem m;
    ttf_font *f;
    int i, worst = 0;
    long worst_bits = 0, total_bits = 0;
    if (open_mem(&m, symnerd_subset, SYMNERD_SUBSET_LEN, &f) != TTF_OK) {
        CHECK(0);
        return;
    }
    for (i = 0; i < TTF_GOLDENS; i++) {
        const ttf_golden *g = &ttf_goldens[i];
        long sc = (long)g->w * 65536L / ttf_units_per_em(f);
        long ysc = (long)g->h * 65536L / ttf_units_per_em(f);
        int n = g->w * g->h, k, maxd = 0, bits = 0, r;
        memset(cov, 0, sizeof cov);
        r = ttf_render(f, ttf_glyph_index(f, g->cp), sc, ysc, 0, g->baseline, cov, g->w, g->h);
        CHECK_INT(r, 1);
        for (k = 0; k < n; k++) {
            int d = cov[k] - g->cov[k];
            if (d < 0)
                d = -d;
            if (d > maxd)
                maxd = d;
            if ((cov[k] >= 128) != (g->cov[k] >= 128))
                bits++;
        }
        if (maxd > 24 || bits * 50 > n)
            printf("  golden U+%04lX %dx%d: max diff %d, %d of %d bits differ\n", g->cp, g->w,
                   g->h, maxd, bits, n);
        CHECK(maxd <= 24);
        CHECK(bits * 50 <= n);
        if (maxd > worst)
            worst = maxd;
        if (bits * 1000L / n > worst_bits)
            worst_bits = bits * 1000L / n;
        total_bits += bits;
    }
    printf("  ttf goldens: worst pixel diff %d, worst 1-bit mismatch %ld.%ld%%, %ld bits in all\n",
           worst, worst_bits / 10, worst_bits % 10, total_bits);
    ttf_close(f);
    CHECK_INT(m.live, 0);
}

/* with the format 12 records, (0,4) and (3,10), hidden, format 4 answers the BMP alone */
/* R6.1: the shape kept on tall pixels (plan 2026-10-22-handler-rasterizer.md) */
static void test_fit(void)
{
    ttf_fit_out o;
    /* square pixels, a 1-em box in an 8 x 8 cell: the cell scale */
    ttf_fit(2048, 2048, 8, 8, 6, 22, 22, &o);
    CHECK_INT(o.xscale, (8L << 16) / 2048);
    CHECK_INT(o.yscale, o.xscale);
    CHECK_INT(o.bw, 8);
    CHECK_INT(o.bh, 8);
    CHECK_INT(o.baseline, 6);
    /* PAL hires (pixel twice as tall): half the rows, centred */
    ttf_fit(2048, 2048, 8, 8, 6, 22, 44, &o);
    CHECK_INT(o.bw, 8);
    CHECK_INT(o.bh, 4);
    CHECK_INT(o.xscale, (8L << 16) / 2048);
    CHECK_INT(o.yscale, o.xscale / 2);
    CHECK_INT(o.baseline, 2 + 3);
    /* two cells on PAL hires: the em fills the rows, twice as many pixels across */
    ttf_fit(2048, 2048, 16, 8, 6, 22, 44, &o);
    CHECK_INT(o.bh, 8);
    CHECK_INT(o.bw, 16);
    CHECK_INT(o.yscale, (8L << 16) / 2048);
    CHECK_INT(o.xscale, 2 * o.yscale);
    CHECK_INT(o.baseline, 6);
    /* a narrow box on square pixels: full height, narrower than the cell */
    ttf_fit(1229, 2048, 8, 8, 6, 1, 1, &o);
    CHECK_INT(o.bh, 8);
    CHECK_INT(o.bw, 5);
    /* the extremes: no overflow, the box inside the cell */
    ttf_fit(131070L, 65535, 512, 512, 400, 1, 255, &o);
    CHECK(o.bw <= 512 && o.bh <= 512 && o.bw > 0 && o.bh > 0);
    CHECK(o.xscale > 0 && o.yscale > 0);
    ttf_fit(131070L, 65535, 512, 512, 400, 255, 1, &o);
    CHECK(o.bw <= 512 && o.bh <= 512 && o.bw > 0 && o.bh > 0);
}

static void test_format4_fallback(void)
{
    static unsigned char d[SYMNERD_SUBSET_LEN];
    mem m, m12;
    ttf_font *f, *f12;
    unsigned long cmap = 0;
    int i, nt, hidden = 0;
    memcpy(d, symnerd_subset, sizeof d);
    for (i = 0; i < (int)rd16(d + 4); i++)
        if (rd32(d + 12 + i * 16) == 0x636D6170UL)
            cmap = rd32(d + 12 + i * 16 + 8);
    CHECK(cmap != 0);
    nt = (int)rd16(d + cmap + 2);
    for (i = 0; i < nt; i++) {
        unsigned char *r = d + cmap + 4 + i * 8;
        if (rd16(d + cmap + rd32(r + 4)) == 12) {
            wr16(r, 7);
            hidden++;
        }
    }
    CHECK_INT(hidden, 2);
    CHECK_INT(open_mem(&m, d, sizeof d, &f), TTF_OK);
    CHECK_INT(open_mem(&m12, symnerd_subset, SYMNERD_SUBSET_LEN, &f12), TTF_OK);
    if (!f || !f12)
        return;
    for (i = 0; i < 11; i++) {
        if (nerd_cps[i] <= 0xFFFFUL) {
            CHECK(ttf_glyph_index(f, nerd_cps[i]) != 0);
            CHECK_INT(ttf_glyph_index(f, nerd_cps[i]), ttf_glyph_index(f12, nerd_cps[i]));
        } else
            CHECK_INT(ttf_glyph_index(f, nerd_cps[i]), 0);
    }
    ttf_close(f);
    ttf_close(f12);
}

/* --- a synthetic font: upem 1024, so at scale 512 one pixel is 128 units --- */

#define S_HEAD 1
#define S_CMAP 2
#define MAXG 8

typedef struct pt {
    int x, y, on;
} pt;

static unsigned char sfont[4096];
static unsigned char glyph[MAXG][256];
static int glen[MAXG];
static int nglyph;

/* a simple glyph: contours of pts, ends[] the last point of each */
static void simple(int gid, const pt *p, const int *ends, int nc)
{
    unsigned char *g = glyph[gid];
    int n = ends[nc - 1] + 1, i, o, px = 0, py = 0;
    memset(g, 0, 256);
    wr16(g, nc);
    for (i = 0; i < nc; i++)
        wr16(g + 10 + i * 2, ends[i]);
    o = 10 + nc * 2;
    wr16(g + o, 0);
    o += 2;
    for (i = 0; i < n; i++)
        g[o++] = (unsigned char)(p[i].on ? 1 : 0); /* 16-bit deltas, no repeats */
    for (i = 0; i < n; i++, o += 2) {
        wr16(g + o, p[i].x - px);
        px = p[i].x;
    }
    for (i = 0; i < n; i++, o += 2) {
        wr16(g + o, p[i].y - py);
        py = p[i].y;
    }
    glen[gid] = (o + 3) & ~3;
}

/* a one-component composite: word offsets, scale (2.14) when nonzero */
static void composite(int gid, int child, int dx, int dy, int scale, unsigned flags)
{
    unsigned char *g = glyph[gid];
    int o = 10;
    memset(g, 0, 256);
    wr16(g, -1);
    wr16(g + o, (long)(flags | (scale ? 8 : 0)));
    wr16(g + o + 2, child);
    wr16(g + o + 4, dx);
    wr16(g + o + 6, dy);
    o += 8;
    if (scale) {
        wr16(g + o, scale);
        o += 2;
    }
    glen[gid] = (o + 3) & ~3;
}

static long build(int omit)
{
    static const unsigned long tags[7] = { 0x636D6170UL, 0x676C7966UL, 0x68656164UL, 0x68686561UL,
                                           0x686D7478UL, 0x6C6F6361UL, 0x6D617870UL };
    unsigned long off[7], len[7];
    long o = 12 + 7 * 16, at;
    int i, t, nt = 0;
    memset(sfont, 0, sizeof sfont);
    wr32(sfont, 0x00010000UL);
    for (t = 0; t < 7; t++) {
        unsigned char *p = sfont + o;
        off[t] = (unsigned long)o;
        switch (t) {
        case 0: /* cmap: one format 4 subtable, U+0041.. to gid 1.. */
            wr16(p + 2, 1);
            wr16(p + 4, 3);
            wr16(p + 6, 1);
            wr32(p + 8, 12);
            p += 12;
            wr16(p, 4);
            wr16(p + 2, 32);
            wr16(p + 6, 4);
            wr16(p + 14, 0x41 + nglyph - 2);
            wr16(p + 16, 0xFFFF);
            wr16(p + 20, 0x41);
            wr16(p + 22, 0xFFFF);
            wr16(p + 24, 1 - 0x41);
            wr16(p + 26, 1);
            len[t] = 12 + 32;
            break;
        case 1: /* glyf */
            for (i = 0, at = 0; i < nglyph; i++) {
                memcpy(p + at, glyph[i], (size_t)glen[i]);
                at += glen[i];
            }
            len[t] = (unsigned long)at;
            break;
        case 2: /* head */
            wr32(p, 0x00010000UL);
            wr16(p + 18, 1024);
            wr16(p + 50, 1);
            len[t] = 54;
            break;
        case 3: /* hhea */
            wr16(p + 34, nglyph);
            len[t] = 36;
            break;
        case 4: /* hmtx */
            for (i = 0; i < nglyph; i++)
                wr16(p + i * 4, 1024);
            len[t] = (unsigned long)nglyph * 4;
            break;
        case 5: /* loca, long */
            for (i = 0, at = 0; i <= nglyph; i++) {
                wr32(p + i * 4, (unsigned long)at);
                if (i < nglyph)
                    at += glen[i];
            }
            len[t] = ((unsigned long)nglyph + 1) * 4;
            break;
        default: /* maxp */
            wr32(p, 0x00005000UL);
            wr16(p + 4, nglyph);
            len[t] = 6;
            break;
        }
        o += (long)((len[t] + 3) & ~3UL);
    }
    for (t = 0; t < 7; t++) {
        unsigned char *r;
        if ((omit & S_CMAP) && t == 0)
            continue;
        if ((omit & S_HEAD) && t == 2)
            continue;
        r = sfont + 12 + nt * 16;
        wr32(r, tags[t]);
        wr32(r + 8, off[t]);
        wr32(r + 12, len[t]);
        nt++;
    }
    wr16(sfont + 4, nt);
    return o;
}

/* glyph 1: the square x 2..6 px, y 0..4 px (rows 8..11 at baseline 12) */
static const pt square[4] = { { 256, 0, 1 }, { 256, 512, 1 }, { 768, 512, 1 }, { 768, 0, 1 } };

static void base_glyphs(void)
{
    static const int e4[1] = { 3 };
    static const pt half[4] = { { 320, 0, 1 }, { 320, 512, 1 }, { 768, 512, 1 }, { 768, 0, 1 } };
    /* corners all off the curve: the implied points are the edge midpoints */
    static const pt round[4] = { { 768, 512, 0 }, { 768, 1024, 0 }, { 1280, 1024, 0 },
                                 { 1280, 512, 0 } };
    static const pt tri[3] = { { 256, 0, 1 }, { 256, 1024, 1 }, { 1280, 0, 1 } };
    static const int e3[1] = { 2 };
    nglyph = 8;
    glen[0] = 0;
    simple(1, square, e4, 1);
    simple(2, half, e4, 1);
    simple(3, round, e4, 1);
    simple(4, tri, e3, 1);
    composite(5, 1, 512, 0, 0, 0x0003);    /* 4 px right */
    composite(6, 1, 0, 0, 0x2000, 0x0003); /* half size */
    composite(7, 7, 0, 0, 0, 0x0003);      /* itself */
}

static int synth_render(ttf_font *f, unsigned gid, unsigned char *cov)
{
    memset(cov, 0, 256);
    return ttf_render(f, gid, 512, 512, 0, 12, cov, 16, 16);
}

static long cov_sum(const unsigned char *cov)
{
    long s = 0;
    int i;
    for (i = 0; i < 256; i++)
        s += cov[i];
    return s;
}

static void test_synthetic(void)
{
    unsigned char cov[256];
    mem m;
    ttf_font *f;
    long n, s;
    int x, y, ok;
    base_glyphs();
    n = build(0);
    CHECK_INT(open_mem(&m, sfont, n, &f), TTF_OK);
    if (!f)
        return;
    CHECK_INT(ttf_glyph_index(f, 0x41), 1);
    CHECK_INT(ttf_glyph_index(f, 0x47), 7);
    CHECK_INT(ttf_glyph_index(f, 0x48), 0);
    CHECK_INT(ttf_glyph_index(f, 0x1F600UL), 0);

    CHECK_INT(synth_render(f, 0, cov), 0); /* empty glyph */
    CHECK_INT(cov_sum(cov), 0);

    CHECK_INT(synth_render(f, 1, cov), 1);
    for (y = 0, ok = 1; y < 16; y++)
        for (x = 0; x < 16; x++)
            if (cov[y * 16 + x] != (x >= 2 && x < 6 && y >= 8 && y < 12 ? 255 : 0))
                ok = 0;
    CHECK(ok);

    synth_render(f, 2, cov); /* left edge at 2.5 px */
    CHECK_INT(cov[9 * 16 + 2], 128);
    CHECK_INT(cov[9 * 16 + 3], 255);
    CHECK_INT(cov[9 * 16 + 1], 0);

    synth_render(f, 5, cov);
    CHECK_INT(cov[8 * 16 + 6], 255);
    CHECK_INT(cov[8 * 16 + 9], 255);
    CHECK_INT(cov[8 * 16 + 5], 0);
    CHECK_INT(cov_sum(cov), 16 * 255);

    synth_render(f, 6, cov); /* x 1..3, y 0..2 px */
    CHECK_INT(cov[10 * 16 + 1], 255);
    CHECK_INT(cov[11 * 16 + 2], 255);
    CHECK_INT(cov_sum(cov), 4 * 255);

    CHECK_INT(synth_render(f, 7, cov), -TTF_BAD_TABLE); /* the depth limit, no hang */

    /* half side s = 2 px with all points off: the curve's area is 10/3 s^2 =
     * 13.33 px. Each arc is flattened to 4 chords (the flatness of FreeType's
     * gray rasterizer, which the goldens hold to), which cut 1/16 of each
     * arc's s^2/3 bulge: 13.0 px, 3315 */
    synth_render(f, 3, cov);
    s = cov_sum(cov);
    printf("  ttf off-curve square: %ld (flattened 3315, curve 3400)\n", s);
    CHECK(s >= 3282 && s <= 3348);

    synth_render(f, 4, cov); /* legs 8 px: 32 px */
    s = cov_sum(cov);
    printf("  ttf triangle: %ld (ideal 8160)\n", s);
    CHECK(s >= 8079 && s <= 8241);

    /* coverage adds to what is there */
    memset(cov, 100, 256);
    ttf_render(f, 1, 512, 512, 0, 12, cov, 16, 16);
    CHECK_INT(cov[8 * 16 + 2], 255);
    CHECK_INT(cov[0], 100);

    CHECK_INT(ttf_render(f, 1, 512, 512, 0, 12, cov, 513, 1), -TTF_TOO_BIG);
    CHECK_INT(ttf_render(f, 1, 512, 512, 0, 12, cov, 0, 16), -TTF_TOO_BIG);
    CHECK_INT(ttf_render(f, 1, 65536L, 512, 0, 12, cov, 16, 16), -TTF_TOO_BIG);
    CHECK_INT(ttf_render(f, 1, 0, 512, 0, 12, cov, 16, 16), -TTF_TOO_BIG);
    CHECK_INT(ttf_render(f, 99, 512, 512, 0, 12, cov, 16, 16), -TTF_BAD_TABLE);
    ttf_close(f);
    CHECK_INT(m.live, 0);

    /* a component anchored by point numbers is refused */
    composite(5, 1, 0, 0, 0, 0x0001);
    n = build(0);
    CHECK_INT(open_mem(&m, sfont, n, &f), TTF_OK);
    if (f) {
        CHECK_INT(synth_render(f, 5, cov), -TTF_BAD_TABLE);
        ttf_close(f);
    }

    /* a scaled offset past the fixed-point range */
    composite(5, 1, 32000, 0, 0x7FFF, 0x0003);
    composite(6, 5, 32000, 0, 0x7FFF, 0x0003);
    n = build(0);
    CHECK_INT(open_mem(&m, sfont, n, &f), TTF_OK);
    if (f) {
        CHECK_INT(synth_render(f, 6, cov), -TTF_TOO_BIG);
        ttf_close(f);
    }
}

static void test_refused(void)
{
    static const unsigned char otto[12] = { 'O', 'T', 'T', 'O', 0, 0 };
    static const unsigned char ttc[12] = { 't', 't', 'c', 'f', 0, 1 };
    mem m;
    ttf_font *f;
    long n, cut;
    int fails = 0, nomem = 0, k;
    CHECK_INT(open_mem(&m, otto, 12, &f), TTF_NOT_TTF);
    CHECK(f == 0);
    CHECK_INT(open_mem(&m, ttc, 12, &f), TTF_NOT_TTF);
    CHECK_INT(open_mem(&m, otto, 3, &f), TTF_TRUNCATED);

    base_glyphs();
    n = build(S_CMAP);
    CHECK_INT(open_mem(&m, sfont, n, &f), TTF_NO_TABLE);
    CHECK_INT(m.live, 0);
    n = build(S_HEAD);
    CHECK_INT(open_mem(&m, sfont, n, &f), TTF_NO_TABLE);

    /* every truncation either opens or fails cleanly; what opens renders or fails cleanly */
    for (cut = 0; cut < SYMNERD_SUBSET_LEN; cut += 7) {
        int e = open_mem(&m, symnerd_subset, cut, &f);
        if (e == TTF_OK) {
            unsigned char cov[16 * 32];
            int i;
            for (i = 0; i < 11; i++) {
                memset(cov, 0, sizeof cov);
                ttf_render(f, ttf_glyph_index(f, nerd_cps[i]), 512, 1024, 0, 25, cov, 16, 32);
            }
            ttf_close(f);
        } else {
            CHECK(f == 0);
            fails++;
        }
        if (m.live) {
            printf("  ttf cut %ld leaks %ld blocks\n", cut, m.live);
            CHECK_INT(m.live, 0);
        }
    }
    CHECK(fails > 0);

    /* every allocation failing in turn: NO_MEMORY and nothing left behind */
    for (k = 0; k < 40; k++) {
        unsigned char cov[8 * 16];
        int e;
        ttf_io io;
        m.d = symnerd_subset;
        m.n = SYMNERD_SUBSET_LEN;
        m.fail_after = k;
        m.live = m.allocs = 0;
        io.read = m_read;
        io.alloc = m_alloc;
        io.free = m_free;
        io.ctx = &m;
        e = ttf_open(&io, &f);
        if (e == TTF_OK) {
            memset(cov, 0, sizeof cov);
            e = ttf_render(f, ttf_glyph_index(f, 0xF0249UL), 256, 512, 0, 12, cov, 8, 16);
            if (e == -TTF_NO_MEMORY)
                nomem++;
            else
                CHECK_INT(e, 1);
            ttf_close(f);
        } else {
            CHECK_INT(e, TTF_NO_MEMORY);
            nomem++;
        }
        CHECK_INT(m.live, 0);
    }
    CHECK(nomem > 3);
}

static void test_threshold(void)
{
    unsigned char cov[10 * 2] = { 0 }, mask[4] = { 0xAA, 0xAA, 0xAA, 0xAA };
    cov[0] = 128;
    cov[1] = 127;
    cov[7] = 255;
    cov[8] = 200;
    cov[10 + 9] = 128;
    ttf_threshold(cov, 10, 2, mask, 2);
    CHECK_INT(mask[0], 0x81);
    CHECK_INT(mask[1], 0x80);
    CHECK_INT(mask[2], 0x00);
    CHECK_INT(mask[3], 0x40);
}

/* plan R4.3: the shade passes threshold at their own levels */
static void test_threshold_at(void)
{
    unsigned char cov[4] = { 31, 32, 160, 255 }, mask[1];
    ttf_threshold_at(cov, 4, 1, mask, 1, 32);
    CHECK_INT(mask[0], 0x70);
    ttf_threshold_at(cov, 4, 1, mask, 1, 224);
    CHECK_INT(mask[0], 0x10);
}

/* bold coverage: each pixel the larger of itself and its left neighbour */
static void test_embolden(void)
{
    unsigned char cov[2 * 4] = { 0, 200, 50, 0, 90, 0, 0, 10 }, out[2 * 4];
    ttf_embolden(cov, 4, 2, out);
    CHECK_INT(out[0], 0);
    CHECK_INT(out[1], 200);
    CHECK_INT(out[2], 200);
    CHECK_INT(out[3], 50);
    CHECK_INT(out[4], 90); /* row 2 starts fresh: nothing from row 1's last pixel */
    CHECK_INT(out[5], 90);
    CHECK_INT(out[6], 0);
    CHECK_INT(out[7], 10);
}

void suite_ttf(void)
{
    test_subset_font();
    test_goldens();
    test_format4_fallback();
    test_fit();
    test_synthetic();
    test_refused();
    test_threshold();
    test_threshold_at();
    test_embolden();
}
