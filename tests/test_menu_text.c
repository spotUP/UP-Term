/* The Amarchy menu text layout (amarchy/menu_text_core.c; plan 2026-10-26-amarchy-gui-aa-font.md A2): string
 * width, ellipsis fitting, the per-code-point glyph cache. A fake glyph source stands in for a font. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../amarchy/menu_text_core.h"
#include "../render/ttf.h"
#include "data/symnerd_subset.h"
#include "harness.h"

static unsigned long fake_adv_calls, fake_render_calls;

/* 'M' 9 wide, '.' 3, U+00E9 7, everything else 6 */
static int f_adv(void *u, unsigned long cp)
{
    (void)u;
    fake_adv_calls++;
    return cp == 'M' ? 9 : cp == '.' ? 3 : cp == 0xE9 ? 7 : 6;
}

static int f_render(void *u, unsigned long cp, mtc_glyph *g)
{
    (void)u; (void)cp;
    fake_render_calls++;
    g->left = 0;
    g->w = 2;
    g->h = 3;
    g->cov = (unsigned char *)malloc(6);
    memset(g->cov, 255, 6);
    return 1;
}

static void *f_alloc(unsigned long n) { return malloc(n); }
static void f_rel(void *p) { free(p); }

static void open_fake(mtc *c)
{
    mtc_src s;
    s.u = 0;
    s.advance = f_adv;
    s.render = f_render;
    s.alloc = f_alloc;
    s.release = f_rel;
    fake_adv_calls = fake_render_calls = 0;
    mtc_init(c, &s);
}

static void test_width(void)
{
    static mtc c;
    open_fake(&c);
    CHECK_INT(mtc_width(&c, ""), 0);
    CHECK_INT(mtc_width(&c, "ab"), 12);
    CHECK_INT(mtc_width(&c, "aM."), 6 + 9 + 3);
    CHECK_INT(mtc_width(&c, "\xC3\xA9x"), 7 + 6); /* UTF-8 e-acute is one glyph */
    mtc_free(&c);
}

static void test_fit(void)
{
    static mtc c;
    char out[64];
    int w;
    open_fake(&c);
    w = mtc_fit(&c, "abcd", 24, out, sizeof out); /* fits exactly */
    CHECK_STR(out, "abcd");
    CHECK_INT(w, 24);
    w = mtc_fit(&c, "abcdef", 24, out, sizeof out); /* 36 wide: head + "..." (9) in 24 = 2 letters */
    CHECK_STR(out, "ab...");
    CHECK_INT(w, 12 + 9);
    CHECK(w <= 24);
    w = mtc_fit(&c, "\xC3\xA9\xC3\xA9\xC3\xA9", 17, out, sizeof out); /* 21 wide: one e-acute + "..." = 16 */
    CHECK_STR(out, "\xC3\xA9...");
    CHECK_INT(w, 16);
    w = mtc_fit(&c, "abcdef", 7, out, sizeof out); /* room for two dots only */
    CHECK_STR(out, "..");
    CHECK_INT(w, 6);
    w = mtc_fit(&c, "abcdef", 2, out, sizeof out);
    CHECK_STR(out, "");
    CHECK_INT(w, 0);
    mtc_free(&c);
}

/* the wrapped-line and prefix widths gui.c uses for the viewer and the message */
static void test_prefix(void)
{
    static mtc c;
    const char *t = "ab\xC3\xA9" "cd"; /* 6 + 6 + 7 + 6 + 6 */
    open_fake(&c);
    CHECK_INT(mtc_width_n(&c, t, 3), 12);                /* a whole code point only: the cut e-acute is left out */
    CHECK_INT(mtc_width_n(&c, t, 4), 6 + 6 + 7);
    CHECK_INT(mtc_prefix(&c, t, 6, 100), 6);             /* all fit */
    CHECK_INT(mtc_prefix(&c, t, 6, 18), 2);              /* ab = 12, the e-acute would be 19 */
    CHECK_INT(mtc_prefix(&c, t, 6, 19), 4);              /* ab + e-acute = 2 + 2 bytes */
    CHECK_INT(mtc_prefix(&c, t, 3, 100), 2);             /* the cut inside the e-acute stops before it */
    CHECK_INT(mtc_prefix(&c, t, 6, 3), 0);
    mtc_free(&c);
}

/* amarchy/gui.c measures and draws every label through menu_text (A3): none of the bitmap-font calls are left
 * in it, and its width callback is mt_width. Fails when a TextLength/TextFit/Text call comes back. */
static void test_gui_uses_menu_text(void)
{
    static char src[300000];
    size_t n;
    FILE *f = fopen("amarchy/gui.c", "rb");
    CHECK(f != 0);
    if (!f)
        return;
    n = fread(src, 1, sizeof src - 1, f);
    fclose(f);
    src[n] = 0;
    CHECK(strstr(src, "TextLength(") == 0);
    CHECK(strstr(src, "TextFit(") == 0);
    CHECK(strstr(src, " Text(") == 0 && strstr(src, "\tText(") == 0);
    CHECK(strstr(src, "return mt_width((menu_text *)ctx, s);") != 0);
    CHECK(strstr(src, "mt_draw(g->mt") != 0);
    CHECK(strstr(src, "mt_fit(g->mt") != 0);
    CHECK(strstr(src, "mt_prefix(g->mt") != 0);
    CHECK(strstr(src, "mt_aa_glyphs(g->mt)") != 0);
}

/* the second width call of the same text asks the source for nothing; draw-side coverage is cached per code point */
static void test_cache(void)
{
    static mtc c;
    const mtc_glyph *g;
    open_fake(&c);
    mtc_width(&c, "abab");
    CHECK_INT(fake_adv_calls, 2); /* a and b once each */
    CHECK_INT(c.advances, 2);
    CHECK_INT(c.hits, 2);
    mtc_width(&c, "abab");
    CHECK_INT(fake_adv_calls, 2);
    CHECK_INT(c.advances, 2);
    g = mtc_glyph_of(&c, 'a');
    CHECK(g && g->w == 2);
    CHECK_INT(fake_render_calls, 1);
    mtc_glyph_of(&c, 'a');
    CHECK_INT(fake_render_calls, 1);
    CHECK_INT(c.renders, 1);
    /* a code point that maps to a's slot evicts it: the next 'a' renders again */
    mtc_glyph_of(&c, 'a' + MTC_SLOTS);
    mtc_glyph_of(&c, 'a');
    CHECK_INT(fake_render_calls, 3);
    mtc_free(&c);
}

static void test_scale(void)
{
    CHECK_INT(mtc_scale(1000, 2048, 16), 8);   /* 7.8 */
    CHECK_INT(mtc_scale(1024, 2048, 15), 8);   /* 7.5 rounds up */
    CHECK_INT(mtc_scale(0, 2048, 16), 0);
    CHECK_INT(mtc_scale(-1000, 2048, 16), -8);
    CHECK_INT(mtc_scale(5, 0, 16), 0);
}

typedef struct mem_font { const unsigned char *d; long n; } mem_font;

static long mf_read(void *ctx, unsigned long off, void *buf, long len)
{
    mem_font *m = (mem_font *)ctx;
    if (off >= (unsigned long)m->n)
        return 0;
    if (off + (unsigned long)len > (unsigned long)m->n)
        len = (long)((unsigned long)m->n - off);
    memcpy(buf, m->d + off, (size_t)len);
    return len;
}

static void *mf_alloc(void *ctx, unsigned long n) { (void)ctx; return calloc(1, n); }
static void mf_free(void *ctx, void *p) { (void)ctx; free(p); }

/* On a tall-pixel screen (640x256 PAL hires: a pixel is twice as tall as wide) the TrueType em down is fewer rows
 * than the em across: rig 7 showed JBMono:14 with rows twice the bitmap's and the window two rows deep. The width
 * is the same on every screen; a square screen is unchanged. */
static void test_ttf_tall_pixels(void)
{
    mem_font mf;
    ttf_io io;
    ttf_font *f = 0;
    mtc_ttf sq, tall, unknown;
    mf.d = symnerd_subset;
    mf.n = SYMNERD_SUBSET_LEN;
    io.read = mf_read;
    io.alloc = mf_alloc;
    io.free = mf_free;
    io.ctx = &mf;
    CHECK_INT(ttf_open(&io, &f), TTF_OK);
    CHECK_INT(mtc_ttf_init(&sq, f, 16, 22, 22), 1);
    CHECK_INT(mtc_ttf_init(&tall, f, 16, 22, 44), 1);
    CHECK_INT(mtc_ttf_init(&unknown, f, 16, 0, 0), 1);
    CHECK_INT(sq.line, unknown.line);                     /* unknown aspect = square */
    CHECK_INT(sq.line >= 16, 1);
    CHECK_INT(tall.line < sq.line, 1);                    /* about half the rows */
    CHECK_INT(tall.line >= 8, 1);
    CHECK_INT(tall.base < sq.base, 1);
    CHECK_INT(tall.scale == sq.scale, 1);                 /* across: unchanged */
    CHECK_INT(tall.yscale * 2 == tall.scale, 1);          /* down: half */
    ttf_close(f);
}

void suite_menu_text(void)
{
    test_ttf_tall_pixels();
    test_width();
    test_fit();
    test_cache();
    test_scale();
    test_prefix();
    test_gui_uses_menu_text();
}
