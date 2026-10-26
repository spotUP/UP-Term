/* The Amarchy menu text layout (amarchy/menu_text_core.c; plan 2026-10-26-amarchy-gui-aa-font.md A2): string
 * width, ellipsis fitting, the per-code-point glyph cache. A fake glyph source stands in for a font. */
#include <stdlib.h>
#include <string.h>
#include "../amarchy/menu_text_core.h"
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

void suite_menu_text(void)
{
    test_width();
    test_fit();
    test_cache();
    test_scale();
}
