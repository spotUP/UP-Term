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

/* The shade ramp. N shade inks sit between the background (step 0) and the text (step N+1), N+2 steps in all.
 * The step maths follows Selecta's mui_aa_ramp_index / mui_aa_ramp_rgb (MIT, Copyright (c) 2026 Johan
 * Samuelsson). Our old ramp was N = 3. */
#define AA_INK_MAX_SHADES 8
#define AA_INK_SHADES_LOW 4  /* native screens of 8 bits or less (AGA): pens are scarce */
#define AA_INK_SHADES_DEEP 8 /* deeper pen screens */

/* The shade count a pen screen of this depth gets */
AA_INK_RULE int aa_ink_shades_for_depth(int depth)
{
    return depth <= 8 ? AA_INK_SHADES_LOW : AA_INK_SHADES_DEEP;
}

/* The step (0..n+1) a coverage 0..255 falls on: the nearest of the n+2 evenly spaced steps */
AA_INK_RULE int aa_ink_index(int coverage, int n)
{
    if (coverage < 0)
        coverage = 0;
    if (coverage > 255)
        coverage = 255;
    return (coverage * (n + 1) + 127) / 255;
}

/* The coverage step k (0..n+1) stands for: 255 at the top, else k/(n+1) of 256 (n = 3: 64, 128, 192, 255) */
AA_INK_RULE int aa_ink_level(int k, int n)
{
    int c = k * 256 / (n + 1);
    return c > 255 ? 255 : c;
}

/* The coverage at which a pass for step k draws over the pass for the last step lo below it. Neighbouring steps
 * meet where aa_ink_index changes (so the passes draw what the index says); a step the screen cannot show is
 * shared by its neighbours, halfway between the levels of lo and k. */
AA_INK_RULE int aa_ink_threshold(int lo, int k, int n)
{
    if (lo == k - 1)
        return (255 * k - 127 + n) / (n + 1);
    return (aa_ink_level(lo, n) + aa_ink_level(k, n) + 1) / 2;
}

/* fg over bg at each coverage 0..255 */
AA_INK_RULE void aa_ink_build_tab(aa_ink_rgb *tab, vt_u32 fg, vt_u32 bg)
{
    int i;
    for (i = 0; i < 256; i++)
        tab[i] = ce_mix(fg, bg, i);
}

/* The colour shade i (0..n-1: step i+1 of n+2) wants: the table at that step's level */
AA_INK_RULE vt_u32 aa_ink_shade_target(const aa_ink_rgb *tab, int i, int n)
{
    return tab[aa_ink_level(i + 1, n)];
}

/* A pen the screen found nearest a shade is used only when it shows the shade closer than the text pen and the
 * background pen do (distances from pm_dist; smaller is closer). */
AA_INK_RULE int aa_ink_shade_accept(long d_pen, long d_fg, long d_bg)
{
    return d_pen < d_fg && d_pen < d_bg;
}

#endif
