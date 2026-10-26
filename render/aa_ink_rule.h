/* aa_ink_rule -- the pure rules of render/aa_ink.c, apart from AmigaOS so the host tests them (tests/test_pens.c). */
#ifndef AA_INK_RULE_H
#define AA_INK_RULE_H

#include "emoji.h" /* vt_u32, ce_mix */

/* One 0x00RRGGBB table entry: 32 bits wherever the host puts long (the cybergraphics table is read as 32-bit entries). */
typedef unsigned int aa_ink_rgb;

/* the rules are static so each includer gets its own; one that needs only some must not trip -Wunused-function */
#ifdef __GNUC__
#define AA_INK_RULE static __attribute__((unused))
#else
#define AA_INK_RULE static
#endif

/* The coverage at which pass i (0..3) draws: 64, 128, 192, 255. */
AA_INK_RULE int aa_ink_level(int i)
{
    return 64 * (i + 1) - (i == 3);
}

/* fg over bg at each coverage 0..255 */
AA_INK_RULE void aa_ink_build_tab(aa_ink_rgb *tab, vt_u32 fg, vt_u32 bg)
{
    int i;
    for (i = 0; i < 256; i++)
        tab[i] = ce_mix(fg, bg, i);
}

/* The colour shade i (0..2: 1/4, 1/2, 3/4 of the way to fg) wants */
AA_INK_RULE vt_u32 aa_ink_shade_target(const aa_ink_rgb *tab, int i)
{
    return tab[64 * (i + 1)];
}

/* A pen the screen found nearest a shade is used only when it shows the shade closer than the text pen and the
 * background pen do (distances from pm_dist; smaller is closer). */
AA_INK_RULE int aa_ink_shade_accept(long d_pen, long d_fg, long d_bg)
{
    return d_pen < d_fg && d_pen < d_bg;
}

#endif
