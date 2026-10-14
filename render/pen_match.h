/* pen_match -- a colour on a screen of few pens (W-T7): the pen that looks nearest, or two pens mixed by an
 * ordered dither when no single pen comes close. Apart from AmigaOS so the host tests it (tests/test_pens.c);
 * render/amiga_render.c (few_pick) is its one user.
 *
 * Distance: "redmean", the weighted RGB distance of T. Riemersma (CompuPhase, "Colour metric",
 * https://www.compuphase.com/cmetric.htm): integer only, so it costs a 68020 without an FPU nothing, and close
 * to a perceptual distance for its price (OKLab needs cube roots). On top of it a colour that has chroma (its
 * red-green and yellow-blue opponents) pays for the chroma a candidate lacks in its hue: on the 16-colour Workbench the
 * xterm cube's light pastels were nearest to pen 6, #d2d2e1, by plain distance -- 26-31 % of UPDemo's
 * rotozoomer drawn grey (owner, 2026-10-13).
 *
 * Dither: Joel Yliluoma's positional dithering with an arbitrary palette, algorithm 1
 * (https://bisqwit.iki.fi/story/howto/dither/jy/): the best mix of two pens, scored as its distance to the
 * colour plus a tenth of the two pens' own distance (weighted by how uneven the mix is), so two far-apart pens
 * (black and white for a grey) lose to a nearer pair. The mix is taken in linear light, as a screen shows two
 * pens side by side. The pattern is the 8 x 8 Bayer matrix: 65 levels, and one byte a pixel row of an 8-pixel
 * cell, so the planar fast path still writes one byte a plane a row (amiga_render.c block_run). */
#ifndef PEN_MATCH_H
#define PEN_MATCH_H

#define PM_LEVELS 64     /* a pattern level: lev of the 64 pixels of an 8 x 8 tile in pen b, the rest in pen a */
#define PM_MAXPENS 32    /* the palette sizes this is meant for: few pens */
#define PM_CHROMA_K 4    /* the price of a step of chroma lost, against redmean's units */
#define PM_SOLID_OK 1200 /* a single pen this close (redmean, about 15 levels of one channel) is not dithered */

/* a choice: the pen for a single-colour drawing (text), and the mix for an area: pen b on lev / 64 of the
 * pixels, pen a on the rest (lev 0: pen `solid` alone) */
typedef struct pm_pick {
    int solid;
    int a, b, lev;
} pm_pick;

/* sRGB to linear light, 0..4095 */
static const unsigned short pm_lin[256] = {
    0, 1, 2, 4, 5, 6, 7, 9, 10, 11, 12, 14, 15, 16, 18, 20,
    21, 23, 25, 27, 29, 31, 33, 35, 37, 40, 42, 45, 48, 50, 53, 56,
    59, 62, 66, 69, 72, 76, 79, 83, 87, 91, 95, 99, 103, 107, 112, 116,
    121, 126, 131, 136, 141, 146, 151, 156, 162, 168, 173, 179, 185, 191, 197, 204,
    210, 216, 223, 230, 237, 244, 251, 258, 265, 273, 280, 288, 296, 304, 312, 320,
    329, 337, 346, 354, 363, 372, 381, 390, 400, 409, 419, 428, 438, 448, 458, 469,
    479, 490, 500, 511, 522, 533, 544, 555, 567, 578, 590, 602, 614, 626, 639, 651,
    664, 676, 689, 702, 715, 729, 742, 756, 769, 783, 797, 811, 826, 840, 855, 869,
    884, 899, 914, 930, 945, 961, 976, 992, 1008, 1025, 1041, 1058, 1074, 1091, 1108, 1125,
    1142, 1160, 1178, 1195, 1213, 1231, 1250, 1268, 1287, 1305, 1324, 1343, 1363, 1382, 1402, 1421,
    1441, 1461, 1482, 1502, 1523, 1544, 1565, 1586, 1607, 1628, 1650, 1672, 1694, 1716, 1738, 1761,
    1784, 1806, 1829, 1853, 1876, 1900, 1923, 1947, 1971, 1996, 2020, 2045, 2070, 2095, 2120, 2145,
    2171, 2197, 2223, 2249, 2275, 2302, 2328, 2355, 2382, 2410, 2437, 2465, 2493, 2521, 2549, 2577,
    2606, 2635, 2664, 2693, 2722, 2752, 2782, 2812, 2842, 2872, 2903, 2934, 2965, 2996, 3027, 3059,
    3091, 3123, 3155, 3187, 3220, 3253, 3286, 3319, 3352, 3386, 3420, 3454, 3488, 3523, 3557, 3592,
    3627, 3663, 3698, 3734, 3770, 3806, 3843, 3879, 3916, 3953, 3991, 4028, 4066, 4104, 4143, 4181,
};

/* the 8 x 8 Bayer matrix: pixel (x, y) of a tile at level lev is pen b when pm_bayer[y][x] < lev */
static const unsigned char pm_bayer[8][8] = {
    { 0, 32, 8, 40, 2, 34, 10, 42 },   { 48, 16, 56, 24, 50, 18, 58, 26 },
    { 12, 44, 4, 36, 14, 46, 6, 38 },  { 60, 28, 52, 20, 62, 30, 54, 22 },
    { 3, 35, 11, 43, 1, 33, 9, 41 },   { 51, 19, 59, 27, 49, 17, 57, 25 },
    { 15, 47, 7, 39, 13, 45, 5, 37 },  { 63, 31, 55, 23, 61, 29, 53, 21 }
};

/* row y of the tile at level lev as a byte, the leftmost pixel the top bit: set where pen b goes */
static unsigned char pm_pattern(int lev, int y)
{
    unsigned char v = 0;
    int x;
    for (x = 0; x < 8; x++)
        if (pm_bayer[y & 7][x] < lev)
            v = (unsigned char)(v | (0x80 >> x));
    return v;
}

/* linear light back to sRGB: the nearest table entry */
static int pm_srgb(long lin)
{
    int lo = 0, hi = 255;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (pm_lin[mid] < lin)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo > 0 && lin - pm_lin[lo - 1] < pm_lin[lo] - lin)
        lo--;
    return lo;
}

/* a colour's chroma as a vector on two opponent axes, red-green and yellow-blue */
static void pm_opp(unsigned long c, long *u, long *v)
{
    long r = (long)((c >> 16) & 255), g = (long)((c >> 8) & 255), b = (long)(c & 255);
    *u = r - g;
    *v = (r + g) / 2 - b;
}

static long pm_isqrt(long n)
{
    long x = 0, bit = 1L << 20;
    while (bit > n)
        bit >>= 2;
    while (bit) {
        if (n >= x + bit) {
            n -= x + bit;
            x = (x >> 1) + bit;
        } else {
            x >>= 1;
        }
        bit >>= 2;
    }
    return x;
}

/* how far c looks from the wanted colour t: redmean, plus the chroma of t that c lacks in t's own hue (c's
 * chroma projected on t's: a grey has none, a pen of another hue little) */
static long pm_dist(unsigned long t, unsigned long c)
{
    long r1 = (long)((t >> 16) & 255), g1 = (long)((t >> 8) & 255), b1 = (long)(t & 255);
    long dr = r1 - (long)((c >> 16) & 255), dg = g1 - (long)((c >> 8) & 255), db = b1 - (long)(c & 255);
    long rm = (r1 + (long)((c >> 16) & 255)) / 2, tu, tv, cu, cv, tn, lost;
    long d = (((512 + rm) * dr * dr) >> 8) + 4 * dg * dg + (((767 - rm) * db * db) >> 8);
    pm_opp(t, &tu, &tv);
    tn = pm_isqrt(tu * tu + tv * tv);
    if (!tn)
        return d;
    pm_opp(c, &cu, &cv);
    lost = tn - (tu * cu + tv * cv) / tn;
    return lost > 0 ? d + PM_CHROMA_K * lost * lost : d;
}

/* the colour pens a and b show at level lev, side by side (linear light) */
static unsigned long pm_mix(unsigned long a, unsigned long b, int lev)
{
    unsigned long out = 0;
    int s;
    for (s = 16; s >= 0; s -= 8) {
        long la = pm_lin[(a >> s) & 255], lb = pm_lin[(b >> s) & 255];
        out |= (unsigned long)pm_srgb(la + ((lb - la) * lev + PM_LEVELS / 2) / PM_LEVELS) << s;
    }
    return out;
}

/* Choose for colour t among pal[0..n) (0xRRGGBB), only the pens whose bit is set in `usable`. pick->solid -1:
 * no pen usable. dither 0: the single pen only (lev 0). */
static void pm_match(const unsigned long *pal, int n, unsigned long usable, unsigned long t, int dither,
                     pm_pick *pick)
{
    long best = -1, bestd = -1, d;
    int i, j;
    pick->solid = pick->a = pick->b = -1;
    pick->lev = 0;
    if (n > PM_MAXPENS)
        n = PM_MAXPENS;
    for (i = 0; i < n; i++) {
        if (!((usable >> i) & 1))
            continue;
        d = pm_dist(t, pal[i]);
        if (best < 0 || d < best) {
            best = d;
            pick->solid = i;
        }
    }
    pick->a = pick->b = pick->solid;
    if (!dither || best <= PM_SOLID_OK)
        return;
    bestd = best;
    for (i = 0; i < n; i++) {
        long ta[3], ab[3], num = 0, den = 0, ab_d;
        int c, lev, l;
        if (!((usable >> i) & 1))
            continue;
        for (j = i + 1; j < n; j++) {
            if (!((usable >> j) & 1) || pal[i] == pal[j])
                continue;
            /* the level nearest t along a..b in linear light, then its neighbours by the full score */
            num = den = 0;
            for (c = 0; c < 3; c++) {
                long la = pm_lin[(pal[i] >> (16 - 8 * c)) & 255];
                ta[c] = pm_lin[(t >> (16 - 8 * c)) & 255] - la;
                ab[c] = pm_lin[(pal[j] >> (16 - 8 * c)) & 255] - la;
                num += ta[c] * ab[c] / 16;
                den += ab[c] * ab[c] / 16;
            }
            lev = den ? (int)((num * PM_LEVELS + den / 2) / den) : 0;
            if (lev < 1 - 2 || lev > PM_LEVELS - 1 + 2)
                continue; /* t lies past one end: that end alone is nearer */
            ab_d = pm_dist(pal[i], pal[j]);
            for (l = lev - 2; l <= lev + 2; l++) {
                long off;
                if (l < 1 || l > PM_LEVELS - 1)
                    continue;
                off = l > PM_LEVELS / 2 ? l - PM_LEVELS / 2 : PM_LEVELS / 2 - l;
                d = pm_dist(t, pm_mix(pal[i], pal[j], l)) + ab_d * (off + PM_LEVELS / 2) / (10 * PM_LEVELS);
                if (d < bestd) {
                    bestd = d;
                    pick->a = i;
                    pick->b = j;
                    pick->lev = l;
                }
            }
        }
    }
}

#endif
