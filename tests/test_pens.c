/* The renderer's pen request (render/pen_rule.h) against a model of a colour map: free entries take a colour's
 * exact value, a full map shares its nearest pen. */
#include "harness.h"
#include "../render/pen_rule.h"
#include "../render/glyph_pen.h"
#include "../render/aa_ink_rule.h"

#define MAP 8
static unsigned long col[MAP];
static int used[MAP];

static long dist(unsigned long a, unsigned long b)
{
    long dr = (long)((a >> 16) & 255) - (long)((b >> 16) & 255), dg = (long)((a >> 8) & 255) - (long)((b >> 8) & 255),
         db = (long)(a & 255) - (long)(b & 255);
    return dr * dr + dg * dg + db * db;
}

/* ObtainBestPen on the model: an entry of that very colour, else a free entry set to it (exact), or the nearest
 * entry in use (not exact) */
static long best(void *u, unsigned long rgb, int exact)
{
    int i, k = -1;
    (void)u;
    for (i = 0; i < MAP; i++)
        if (used[i] && col[i] == rgb)
            return i;
    if (exact) {
        for (i = 0; i < MAP; i++)
            if (!used[i]) {
                used[i] = 1;
                col[i] = rgb;
                return i;
            }
        return -1;
    }
    for (i = 0; i < MAP; i++)
        if (used[i] && (k < 0 || dist(col[i], rgb) < dist(col[k], rgb)))
            k = i;
    return k;
}

static void workbench(int n)
{
    static const unsigned long wb[4] = { 0xAAAAAA, 0x000000, 0xFFFFFF, 0xEE9900 };
    int i;
    for (i = 0; i < MAP; i++) {
        used[i] = i < n;
        col[i] = i < 4 ? wb[i] : 0x112233 * (unsigned long)i;
    }
}

/* a 256-colour Workbench with free entries: the cube's (215,135,0) is its own pen, not the Workbench orange */
static void cube_colour_near_a_workbench_pen_gets_its_own_pen(void)
{
    long p;
    workbench(4);
    p = vr_obtain_rule(best, 0, 0xD78700);
    CHECK(p >= 4);
    CHECK(col[p] == 0xD78700);
    CHECK(vr_obtain_rule(best, 0, 0xEE9900) == 3); /* the Workbench colour itself: its pen */
}

/* a full map: the nearest pen, as before */
static void full_map_shares_the_nearest_pen(void)
{
    workbench(MAP);
    CHECK(vr_obtain_rule(best, 0, 0xD78700) == 3);
}

/* black on black (owner 2026-10-24): Amiga-personality text in pen 4 or 5 on a 4-colour screen draws as pen 0 or 1,
 * and the glyphs must still not take the background's pen */
static void text_pen_past_a_4_colour_screen_is_not_drawn_in_the_background(void)
{
    unsigned fg = vr_screen_pen(4, 2), bg = vr_screen_pen(0, 2);
    CHECK(fg == 0);                                         /* the pen graphics really draws */
    CHECK(vr_screen_pen(5, 2) == 1);
    CHECK(vr_screen_pen(13, 3) == 5);
    CHECK(vr_screen_pen(200, 8) == 200);                    /* every pen of a 256-colour screen is its own */
    CHECK(vr_screen_pen(200, 24) == 200);
    CHECK(vr_glyph_pen(fg, bg, 1, 1, 0) == 1);              /* the plain text pen instead */
    CHECK(vr_glyph_pen(vr_screen_pen(4, 2), vr_screen_pen(1, 2), 1, 1, 0) == 0); /* on the text pen: the background's */
}

/* the cell asked for one colour twice (or the same pen on purpose): left alone; distinct pens untouched */
static void glyph_pen_kept_when_it_differs_or_was_asked_equal(void)
{
    CHECK(vr_glyph_pen(3, 0, 1, 1, 0) == 3);
    CHECK(vr_glyph_pen(2, 2, 0, 1, 0) == 2);
}

/* render/aa_ink.c's rules, the shared coverage-to-ink path (terminal glyphs and amarchy-gui's text): the table
 * runs from bg (coverage 0) to fg (255), the shades aim at 1/4, 1/2, 3/4, the passes sit at 64/128/192/255, and a
 * nearest pen is taken only when it beats both the text pen and the background pen. */
static void aa_ink_rules(void)
{
    aa_ink_rgb tab[256];
    aa_ink_build_tab(tab, 0xFFFFFF, 0x000000);
    CHECK(tab[0] == 0x000000);
    CHECK(tab[255] == 0xFFFFFF);
    CHECK(tab[128] == ce_mix(0xFFFFFF, 0, 128));
    /* 3 shades is the old ramp: 64, 128, 192, 255 */
    CHECK(aa_ink_shade_target(tab, 0, 3) == tab[64]);
    CHECK(aa_ink_shade_target(tab, 1, 3) == tab[128]);
    CHECK(aa_ink_shade_target(tab, 2, 3) == tab[192]);
    CHECK(aa_ink_level(1, 3) == 64 && aa_ink_level(2, 3) == 128 && aa_ink_level(3, 3) == 192 && aa_ink_level(4, 3) == 255);
    /* the depth picks the count: 4 on AGA and <= 8 bits, 8 deeper (more than the old 3) */
    CHECK(aa_ink_shades_for_depth(8) == 4 && aa_ink_shades_for_depth(5) == 4 && aa_ink_shades_for_depth(15) == 8
          && aa_ink_shades_for_depth(24) == 8);
    CHECK(aa_ink_shades_for_depth(8) > 3 && aa_ink_shades_for_depth(24) <= AA_INK_MAX_SHADES);
    /* step index = Selecta's (cov*(steps-1)+127)/255 with steps = n+2 */
    CHECK(aa_ink_index(0, 3) == 0 && aa_ink_index(255, 3) == 4 && aa_ink_index(127, 3) == 2 && aa_ink_index(32, 3) == 1);
    CHECK(aa_ink_index(31, 3) == 0 && aa_ink_index(96, 3) == 2 && aa_ink_index(95, 3) == 1);
    CHECK(aa_ink_index(0, 4) == 0 && aa_ink_index(255, 4) == 5 && aa_ink_index(51, 4) == 1 && aa_ink_index(50, 4) == 1
          && aa_ink_index(25, 4) == 0 && aa_ink_index(26, 4) == 1 && aa_ink_index(127, 4) == 2 && aa_ink_index(128, 4) == 3);
    CHECK(aa_ink_index(0, 8) == 0 && aa_ink_index(255, 8) == 9 && aa_ink_index(127, 8) == 4 && aa_ink_index(15, 8) == 1
          && aa_ink_index(14, 8) == 0 && aa_ink_index(-5, 8) == 0 && aa_ink_index(999, 8) == 9);
    {
        int n, c, k;
        for (n = 3; n <= 8; n++)
            for (c = 0; c < 256; c++) { /* the passes (threshold between neighbours) draw the step index says, +-0 */
                int want = aa_ink_index(c, n), got = 0;
                for (k = 1; k <= n + 1; k++)
                    if (c >= aa_ink_threshold(k - 1, k, n))
                        got = k;
                if (got != want) {
                    printf("pass/index mismatch n=%d c=%d got=%d want=%d\n", n, c, got, want);
                    CHECK(0);
                }
            }
    }
    /* colour mix of each step: the table at its level, n = 4 white over black */
    CHECK(aa_ink_shade_target(tab, 0, 4) == tab[aa_ink_level(1, 4)] && (aa_ink_shade_target(tab, 0, 4) & 255) == 51);
    CHECK((aa_ink_shade_target(tab, 3, 4) & 255) == 204 && (aa_ink_shade_target(tab, 7, 8) & 255) == 227);
    /* revert check: the old 3-step mapping (coverage/64 passes) puts 127 at step 1, the ramp at step 2 of 3 shades,
     * and 4 shades distinguish 51 from 25 where 3 shades cannot */
    CHECK(aa_ink_index(127, 3) == 2 && 127 / 64 == 1); /* old mapping: 1 */
    CHECK(aa_ink_index(51, 4) != aa_ink_index(25, 4) && aa_ink_shades_for_depth(8) != 3);
    CHECK(aa_ink_shade_accept(10, 50, 60));   /* closer than both: used */
    CHECK(!aa_ink_shade_accept(50, 50, 60));  /* as far as the text pen: the text pen draws it */
    CHECK(!aa_ink_shade_accept(60, 70, 60));  /* as far as the background pen */
    CHECK(!aa_ink_shade_accept(90, 50, 60));
}

void suite_pens(void)
{
    aa_ink_rules();
    text_pen_past_a_4_colour_screen_is_not_drawn_in_the_background();
    glyph_pen_kept_when_it_differs_or_was_asked_equal();
    cube_colour_near_a_workbench_pen_gets_its_own_pen();
    full_map_shares_the_nearest_pen();
}
