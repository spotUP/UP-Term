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
    CHECK(aa_ink_shade_target(tab, 0) == tab[64]);
    CHECK(aa_ink_shade_target(tab, 1) == tab[128]);
    CHECK(aa_ink_shade_target(tab, 2) == tab[192]);
    CHECK(aa_ink_level(0) == 64 && aa_ink_level(1) == 128 && aa_ink_level(2) == 192 && aa_ink_level(3) == 255);
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
