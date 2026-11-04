/* RL21 icon label antialiasing core (amarchy/iclabel_core.c): call filter, glyph mask, corner coverage, true-colour
 * and pen blends, shade pen cache. */
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "../amarchy/iclabel_core.h"
#include "../render/aa_ink_rule.h"

static void t_accept(void)
{
    /* a Workbench label inside a drawer of inner area 4..195, 11..97 */
    CHECK(icl_accept("Workbench", ICL_JAM2, 1, 0, 20, 40, 48, 8, 4, 11, 195, 97));
    CHECK(icl_accept("dopus", 0, 2, 0, 20, 40, 48, 8, 4, 11, 195, 97));
    CHECK(!icl_accept("Shell Process", ICL_JAM2, 1, 0, 20, 40, 48, 8, 4, 11, 195, 97));
    CHECK(!icl_accept("Workbench", ICL_JAM2 | ICL_COMPLEMENT, 1, 0, 20, 40, 48, 8, 4, 11, 195, 97));
    CHECK(!icl_accept("Workbench", ICL_INVERSVID, 1, 0, 20, 40, 48, 8, 4, 11, 195, 97));
    CHECK(!icl_accept("Workbench", ICL_JAM2, 1, 1, 20, 40, 48, 8, 4, 11, 195, 97));
    /* title bar text (y 2) and a box past the right border stay stock */
    CHECK(!icl_accept("Workbench", ICL_JAM2, 1, 0, 20, 2, 48, 8, 4, 11, 195, 97));
    CHECK(!icl_accept("Workbench", ICL_JAM2, 1, 0, 160, 40, 48, 8, 4, 11, 195, 97));
    CHECK(!icl_accept("Workbench", ICL_JAM2, 1, 0, 0, 0, ICL_MAXW + 8, 8, 0, 0, 2000, 2000));
    CHECK(!icl_accept(NULL, ICL_JAM2, 1, 0, 20, 40, 48, 8, 4, 11, 195, 97));
}

static void t_mask(void)
{
    uint32_t before[4] = { 7, 7, 7, 7 }, after[4] = { 1, 0, 1, 7 };
    uint32_t rtg[3] = { 0x000008, 0xF8F8F8, 0x101010 };
    uint8_t m[4];
    CHECK_INT(icl_mask(1, 1, before, after, 1, 0, 4, m), 2);
    CHECK(m[0] == 1 && m[1] == 0 && m[2] == 1 && m[3] == 0);
    /* JAM1: anything Text changed is glyph */
    CHECK_INT(icl_mask(0, 1, before, after, 1, 0, 4, m), 3);
    CHECK(m[3] == 0);
    /* RTG JAM2 with 16-bit rounding: nearer to black (APen) is glyph */
    CHECK_INT(icl_mask(1, 0, NULL, rtg, 0x000000, 0xFFFFFF, 3, m), 2);
    CHECK(m[0] == 1 && m[1] == 0 && m[2] == 1);
}

/* a 4x4 diagonal stroke:  X...  .X..  ..X.  ...X  */
static const uint8_t diag[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

static void t_coverage(void)
{
    uint8_t cov[16];
    /* the empty pixels beside the stroke sit in its inner corners: (1,0) has W+S, (0,1) has N+E ... */
    CHECK_INT(icl_coverage(diag, 4, 4, cov), 6);
    CHECK_INT(cov[0], 255);
    CHECK_INT(cov[1], ICL_COV_ONE);
    CHECK_INT(cov[4], ICL_COV_ONE);
    CHECK_INT(cov[3], 0);
    CHECK_INT(cov[12], 0);
    {
        /* a pixel between two strokes (corner on both sides) gets ICL_COV_TWO */
        static const uint8_t two[9] = { 1, 0, 1, 0, 0, 0, 1, 0, 1 };
        static const uint8_t plus[9] = { 0, 1, 0, 1, 0, 1, 0, 1, 0 };
        CHECK_INT(icl_coverage(two, 3, 3, cov), 0);
        CHECK_INT(icl_coverage(plus, 3, 3, cov), 5);
        CHECK_INT(cov[4], ICL_COV_TWO);
        CHECK_INT(cov[0], ICL_COV_ONE);
    }
    {
        /* straight vertical and horizontal edges stay hard */
        static const uint8_t bar[9] = { 0, 1, 0, 0, 1, 0, 0, 1, 0 };
        CHECK_INT(icl_coverage(bar, 3, 3, cov), 0);
    }
}

static void t_blend_rgb(void)
{
    uint8_t cov[16];
    uint32_t after[16], out[16];
    uint32_t fg;
    int i;
    for (i = 0; i < 16; i++)
        after[i] = diag[i] ? 0x000000u : 0xAAAAAAu;
    icl_coverage(diag, 4, 4, cov);
    CHECK(icl_fg(diag, after, 16, &fg));
    CHECK_INT((int)fg, 0);
    CHECK_INT(icl_blend_rgb(cov, after, fg, 16, out), 6);
    /* the edge pixel is an intermediate grey: ce_mix(black, AAAAAA, 96) */
    CHECK_INT((int)out[1], (int)ce_mix(0x000000, 0xAAAAAA, ICL_COV_ONE));
    CHECK((out[1] & 0xFF) > 0 && (out[1] & 0xFF) < 0xAA);
    CHECK_INT((int)out[0], 0);
    CHECK_INT((int)out[3], 0xAAAAAA);
    {
        static const uint8_t none[4] = { 0, 0, 0, 0 };
        CHECK(!icl_fg(none, after, 4, &fg));
    }
}

static int fake_pen(void *u, int fgpen, int bgpen, int k)
{
    int *calls = (int *)u;
    (*calls)++;
    (void)fgpen;
    (void)bgpen;
    return 10 + k;
}

static void t_pens(void)
{
    uint8_t cov[16];
    uint32_t after[16], out[16];
    int i, calls = 0, n = aa_ink_shades_for_depth(8);
    for (i = 0; i < 16; i++)
        after[i] = diag[i] ? 1u : 0u;
    icl_coverage(diag, 4, 4, cov);
    CHECK_INT(icl_blend_pens(cov, after, 1, n, fake_pen, &calls, 16, out), 6);
    CHECK_INT(calls, 6);
    CHECK_INT((int)out[1], 10 + aa_ink_index(ICL_COV_ONE, n));
    CHECK_INT((int)out[0], 1);
    CHECK_INT((int)out[3], 0);
    /* the shade target sits between fg and bg */
    CHECK_INT((int)icl_pen_target(0x000000, 0xFFFFFF, 2, 4), (int)ce_mix(0, 0xFFFFFF, aa_ink_level(2, 4)));
    /* accept only a pen closer than fg and bg, else round */
    CHECK_INT(icl_pen_choice(2, 4, 9, 10, 50, 50, 1, 0), 9);
    CHECK_INT(icl_pen_choice(2, 4, 9, 60, 50, 80, 1, 0), 0);
    CHECK_INT(icl_pen_choice(3, 4, 9, 60, 50, 80, 1, 0), 1);
    CHECK_INT(icl_pen_choice(3, 4, -1, 0, 50, 80, 1, 0), 1);
}

static void t_cache(void)
{
    icl_pencache c;
    int old, i;
    icl_cache_clear(&c);
    CHECK_INT(icl_cache_find(&c, 1, 0, 2, 0, 0xAAAAAA), -1);
    CHECK(icl_cache_put(&c, 1, 0, 2, 0, 0xAAAAAA, 9, 1, &old));
    CHECK_INT(old, -1);
    CHECK_INT(icl_cache_find(&c, 1, 0, 2, 0, 0xAAAAAA), 9);
    /* palette change: a miss, and replacing hands back the old obtained pen */
    CHECK_INT(icl_cache_find(&c, 1, 0, 2, 0, 0xBBBBBB), -1);
    CHECK(icl_cache_put(&c, 1, 0, 2, 0, 0xBBBBBB, 12, 1, &old));
    CHECK_INT(old, 9);
    CHECK_INT(icl_cache_find(&c, 1, 0, 2, 0, 0xBBBBBB), 12);
    for (i = 0; i < ICL_CACHE - 1; i++)
        CHECK(icl_cache_put(&c, 2, i, 1, 0, 0, 20, 0, &old));
    CHECK(!icl_cache_put(&c, 3, 3, 3, 0, 0, 21, 1, &old));
}

void suite_iclabel(void)
{
    t_accept();
    t_mask();
    t_coverage();
    t_blend_rgb();
    t_pens();
    t_cache();
}
