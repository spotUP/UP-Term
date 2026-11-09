/* pic_pens -- a picture of more colours than the screen has free pens (a 32-colour screen: 12 pens free, the rest
 * held by the terminal's own 16 colours and the system). Apart from AmigaOS so the host tests it
 * (tests/test_picpens.c); render/amiga_render.c (img_colours) is one user, DCTelnet's VtImgColours the other.
 *
 * The cause it removes: pens handed to a picture's palette first-come, in raster order, gave the picture the
 * colours of its top rows and the nearest of those to everything else (black drawn green, white grey).
 *
 *   pp_weights  how many pixels use each palette entry
 *   pp_pick     the K colours that cover most of the picture: a weighted median cut of the palette; a box whose
 *               mean a fixed pen already shows closely gets no pen of its own
 *   pp_plan_make  for every palette entry the two candidates (fixed pens + picked pens) that mix to it best and
 *               the mix level: pen_match.h's two-pen match, worked once per entry
 *   pp_pen_at   the candidate a pixel (x, y) takes: the 8 x 8 Bayer pattern of pen_match.h, so the mean of a
 *               flat area is the entry's colour and the picture keeps its shades
 * The pixels are the picture's palette indices (an RGB picture would feed pp_pick through a 5-bit histogram
 * of its pixels in the same entry list; the dither works per entry either way). */
#ifndef PIC_PENS_H
#define PIC_PENS_H

#include "pen_match.h"

#define PP_MAXCOL 256      /* a palette: indices are bytes */
#define PP_MAXBOX 64       /* boxes of one cut: more than the pens a pen_match.h palette holds */
#define PP_NEAR 300        /* a box mean this close (pen_match.h pm_dist) to a fixed pen needs no pen */

typedef struct pp_plan {
    unsigned char a[PP_MAXCOL], b[PP_MAXCOL], lev[PP_MAXCOL]; /* per palette entry: candidates a, b, level */
} pp_plan;

/* w[i]: the pixels of pix[0..n) that are palette entry i (0..npal) */
PM_MAYBE_UNUSED static void pp_weights(const unsigned char *pix, long n, int npal, unsigned long *w)
{
    long i;
    int c;
    for (c = 0; c < npal; c++)
        w[c] = 0;
    for (i = 0; i < n; i++)
        if (pix[i] < npal)
            w[pix[i]]++;
}

/* one cut of a colour list into boxes: nbox boxes over the order ord, box j = ord[lo[j] .. hi[j]) */
typedef struct pp_cut {
    int ord[PP_MAXCOL];
    int lo[PP_MAXBOX], hi[PP_MAXBOX];
    int nbox;
} pp_cut;

static int pp_chan(unsigned long c, int ch)
{
    return (int)((c >> (16 - 8 * ch)) & 255);
}

/* the box's longest channel and its range */
static int pp_longest(const unsigned long *rgb, const pp_cut *c, int j, int *range)
{
    int ch, best = 0, bestr = -1, i;
    for (ch = 0; ch < 3; ch++) {
        int mn = 255, mx = 0;
        for (i = c->lo[j]; i < c->hi[j]; i++) {
            int v = pp_chan(rgb[c->ord[i]], ch);
            if (v < mn)
                mn = v;
            if (v > mx)
                mx = v;
        }
        if (mx - mn > bestr) {
            bestr = mx - mn;
            best = ch;
        }
    }
    *range = bestr;
    return best;
}

static unsigned long pp_box_weight(const unsigned long *w, const pp_cut *c, int j)
{
    unsigned long s = 0;
    int i;
    for (i = c->lo[j]; i < c->hi[j]; i++)
        s += w[c->ord[i]];
    return s;
}

/* n entries cut into at most want boxes: the box with the most weight times colour range splits at its weighted
 * median along its longest channel */
static void pp_cut_make(const unsigned long *rgb, const unsigned long *w, int n, int want, pp_cut *c)
{
    int i;
    for (i = 0; i < n; i++)
        c->ord[i] = i;
    c->nbox = 1;
    c->lo[0] = 0;
    c->hi[0] = n;
    while (c->nbox < want) {
        int j, pick = -1, ch, range, k, m;
        unsigned long score, bests = 0, half, run;
        for (j = 0; j < c->nbox; j++) {
            if (c->hi[j] - c->lo[j] < 2)
                continue;
            pp_longest(rgb, c, j, &range);
            if (range <= 0)
                continue;
            score = pp_box_weight(w, c, j) * (unsigned long)range;
            if (pick < 0 || score > bests) {
                pick = j;
                bests = score;
            }
        }
        if (pick < 0)
            break;
        ch = pp_longest(rgb, c, pick, &range);
        for (k = c->lo[pick] + 1; k < c->hi[pick]; k++) { /* insertion sort on the channel */
            int o = c->ord[k], v = pp_chan(rgb[o], ch);
            m = k - 1;
            while (m >= c->lo[pick] && pp_chan(rgb[c->ord[m]], ch) > v) {
                c->ord[m + 1] = c->ord[m];
                m--;
            }
            c->ord[m + 1] = o;
        }
        half = (pp_box_weight(w, c, pick) + 1) / 2;
        run = 0;
        for (k = c->lo[pick]; k < c->hi[pick] - 1; k++) {
            run += w[c->ord[k]];
            if (run >= half)
                break;
        }
        c->lo[c->nbox] = k + 1;
        c->hi[c->nbox] = c->hi[pick];
        c->hi[pick] = k + 1;
        c->nbox++;
    }
}

static unsigned long pp_box_mean(const unsigned long *rgb, const unsigned long *w, const pp_cut *c, int j)
{
    unsigned long sum[3] = { 0, 0, 0 }, tot = 0, out = 0;
    int i, ch;
    for (i = c->lo[j]; i < c->hi[j]; i++) {
        unsigned long wt = w[c->ord[i]];
        for (ch = 0; ch < 3; ch++)
            sum[ch] += wt * (unsigned long)pp_chan(rgb[c->ord[i]], ch);
        tot += wt;
    }
    if (!tot)
        return 0;
    for (ch = 0; ch < 3; ch++)
        out |= ((sum[ch] + tot / 2) / tot) << (16 - 8 * ch);
    return out;
}

/* Lloyd's iterations over the entries: every entry belongs to the nearest of the centroids and the fixed pens; a
 * centroid moves to the mean of its entries. A median cut mixes colours a fixed pen shows already into the mean of
 * a box (black and a teal in one box give a dark teal); here those entries go to the fixed pen and the mean is the
 * rest's. Writes the centroids that kept pixels, the heaviest first; returns how many (at most k). */
static int pp_refine(const unsigned long *erg, const unsigned long *ew, int en, unsigned long *cen, int m,
                     const unsigned long *fixed, int nfixed, int k, unsigned long *out)
{
    unsigned long sum[PP_MAXBOX][3], tot[PP_MAXBOX];
    int it, i, j, f, ch, got = 0;
    for (it = 0; it < 4; it++) {
        for (j = 0; j < m; j++) {
            tot[j] = 0;
            sum[j][0] = sum[j][1] = sum[j][2] = 0;
        }
        for (i = 0; i < en; i++) {
            pm_target tt;
            long best = -1, d;
            int who = -1;
            pm_aim(erg[i], &tt);
            for (f = 0; f < nfixed; f++) {
                d = pm_dist(&tt, fixed[f]);
                if (best < 0 || d < best) {
                    best = d;
                    who = -1;
                }
            }
            for (j = 0; j < m; j++) {
                d = pm_dist(&tt, cen[j]);
                if (best < 0 || d < best) {
                    best = d;
                    who = j;
                }
            }
            if (who < 0)
                continue;
            tot[who] += ew[i];
            for (ch = 0; ch < 3; ch++)
                sum[who][ch] += ew[i] * (unsigned long)pp_chan(erg[i], ch);
        }
        for (j = 0; j < m; j++)
            if (tot[j]) {
                unsigned long c = 0;
                for (ch = 0; ch < 3; ch++)
                    c |= ((sum[j][ch] + tot[j] / 2) / tot[j]) << (16 - 8 * ch);
                cen[j] = c;
            }
    }
    while (got < k) {
        int best = -1;
        for (j = 0; j < m; j++)
            if (tot[j] && (best < 0 || tot[j] > tot[best]))
                best = j;
        if (best < 0)
            break;
        out[got++] = cen[best];
        tot[best] = 0;
    }
    return got;
}

/* The colours to give pens to. rgb[i], weight[i] (i < n): the picture's colours and their pixel counts (entries
 * of weight 0 are skipped). fixed[0..nfixed): colours the screen shows anyway (the terminal's pens). Writes at
 * most k colours (0xRRGGBB) to out, the heaviest box first; returns how many. A picture of k or fewer used
 * colours gets exactly those (less any a fixed pen shows closely). */
PM_MAYBE_UNUSED static int pp_pick(const unsigned long *rgb, const unsigned long *weight, int n,
                                   const unsigned long *fixed, int nfixed, int k, unsigned long *out)
{
    static pp_cut cut; /* 1.4 KB: off the caller's stack (the renderer draws on a small one) */
    unsigned long erg[PP_MAXCOL], ew[PP_MAXCOL], mean[PP_MAXBOX], bw[PP_MAXBOX];
    int en = 0, i, j, want, nb;
    if (k <= 0)
        return 0;
    if (n > PP_MAXCOL)
        n = PP_MAXCOL;
    for (i = 0; i < n; i++)
        if (weight[i]) {
            erg[en] = rgb[i];
            ew[en++] = weight[i];
        }
    if (!en)
        return 0;
    want = k;
    for (;;) {
        int keep = 0;
        if (want > en)
            want = en;
        if (want > PP_MAXBOX)
            want = PP_MAXBOX;
        pp_cut_make(erg, ew, en, want, &cut);
        nb = cut.nbox;
        for (j = 0; j < nb; j++) {
            pm_target tt;
            int shown = 0;
            mean[j] = pp_box_mean(erg, ew, &cut, j);
            bw[j] = pp_box_weight(ew, &cut, j);
            pm_aim(mean[j], &tt);
            for (i = 0; i < nfixed && !shown; i++)
                if (pm_dist(&tt, fixed[i]) < PP_NEAR)
                    shown = 1;
            if (shown)
                bw[j] = 0; /* a fixed pen shows it */
            else
                keep++;
        }
        if (keep >= k || want >= en || want >= PP_MAXBOX || nb < want)
            break;
        want += k - keep;
    }
    {
        unsigned long cen[PP_MAXBOX];
        int m = 0;
        for (j = 0; j < nb; j++)
            if (bw[j])
                cen[m++] = mean[j];
        return pp_refine(erg, ew, en, cen, m, fixed, nfixed, k, out);
    }
}

/* The mix of every palette entry over the candidate pens: cand[0..ncand) (0xRRGGBB, the fixed pens then the
 * picked ones; at most PM_MAXPENS). scratch is a pm_palette the caller owns (4.6 KB). Entry 0 is the picture's
 * unset pixels: left alone. */
PM_MAYBE_UNUSED static void pp_plan_make(pp_plan *pl, pm_palette *scratch, const unsigned long *pal, int npal,
                                         const unsigned long *cand, int ncand)
{
    unsigned long usable;
    int i;
    if (ncand > PM_MAXPENS)
        ncand = PM_MAXPENS;
    usable = ncand >= 32 ? 0xFFFFFFFFUL : ((1UL << ncand) - 1);
    scratch->n = 0;
    pm_palette_set(scratch, cand, ncand);
    for (i = 1; i < npal && i < PP_MAXCOL; i++) {
        pm_pick pk;
        pm_match(scratch, usable, pal[i], 1, &pk);
        pl->a[i] = (unsigned char)(pk.a < 0 ? 0 : pk.a);
        pl->b[i] = (unsigned char)(pk.b < 0 ? pl->a[i] : pk.b);
        pl->lev[i] = (unsigned char)(pk.a == pk.b ? 0 : pk.lev);
    }
}

/* the candidate entry i takes at picture pixel (x, y): b on lev of the 64 pixels of an 8 x 8 tile */
PM_MAYBE_UNUSED static int pp_pen_at(const pp_plan *pl, int i, int x, int y)
{
    return pl->lev[i] && pm_bayer[y & 7][x & 7] < pl->lev[i] ? pl->b[i] : pl->a[i];
}

/* ---- a picture that keeps its own colours (vt_image_view.rgb: 3 bytes a pixel) ----
 *
 *   pp_hist     the colours of the picture, 4 bits a channel: the heaviest bins (at most PP_MAXCOL) as (mean
 *               colour, pixel count), from at most about 65 000 sampled pixels; feeds pp_pick like a palette. Also
 *               the mean colour of every bin sampled (the others: the bin's middle) for pp_bin_fill.
 *   pp_bin_fill the two candidates and the level for one bin, worked on first use (a picture costs at most 4096)
 *   pp_pen_bin  the candidate a pixel (x, y) takes: the same 8 x 8 pattern as pp_pen_at
 * Every pixel is dithered ONCE from its own colour onto {fixed pens + picked pens}. */
#define PP_BINS 4096
#define PP_UNSET 255 /* a bin plan's level before pp_bin_fill */

typedef struct pp_hist_work {
    unsigned long n[PP_BINS], s[3][PP_BINS];
} pp_hist_work;

static int pp_bin_of(const unsigned char *p)
{
    return ((p[0] >> 4) << 8) | ((p[1] >> 4) << 4) | (p[2] >> 4);
}

static int pp_log2(unsigned long v)
{
    int lg = 0;
    while (v > 1) {
        v >>= 1;
        lg++;
    }
    return lg;
}

/* idx[i] == 0: an unset (transparent) pixel, left out. bincol[bin * 3 + c]: mean colour of bin. Returns the entries
 * written to orgb / ow (at most PP_MAXCOL). */
PM_MAYBE_UNUSED static int pp_hist(const unsigned char *idx, const unsigned char *rgb, long npix, pp_hist_work *wk,
                                   unsigned long *orgb, unsigned long *ow, unsigned char *bincol)
{
    long i, step = (npix / 65536L) | 1L;
    int cnt[33], b, c, got = 0, th = 0, acc = 0;
    for (b = 0; b < PP_BINS; b++) {
        wk->n[b] = wk->s[0][b] = wk->s[1][b] = wk->s[2][b] = 0;
        bincol[b * 3] = (unsigned char)(((b >> 8) & 15) * 17);
        bincol[b * 3 + 1] = (unsigned char)(((b >> 4) & 15) * 17);
        bincol[b * 3 + 2] = (unsigned char)((b & 15) * 17);
    }
    for (i = 0; i < npix; i += step) {
        int bn;
        if (!idx[i])
            continue;
        bn = pp_bin_of(rgb + i * 3);
        wk->n[bn]++;
        for (c = 0; c < 3; c++)
            wk->s[c][bn] += rgb[i * 3 + c];
    }
    for (b = 0; b <= 32; b++)
        cnt[b] = 0;
    for (b = 0; b < PP_BINS; b++)
        if (wk->n[b]) {
            cnt[pp_log2(wk->n[b])]++;
            for (c = 0; c < 3; c++)
                bincol[b * 3 + c] = (unsigned char)((wk->s[c][b] + wk->n[b] / 2) / wk->n[b]);
        }
    for (b = 32; b >= 0; b--) { /* the lowest weight class (log2) from which at most PP_MAXCOL bins are taken */
        if (acc + cnt[b] > PP_MAXCOL)
            break;
        acc += cnt[b];
        th = b;
    }
    for (b = 0; b < PP_BINS && got < PP_MAXCOL; b++) {
        if (!wk->n[b] || pp_log2(wk->n[b]) < th)
            continue;
        orgb[got] = ((unsigned long)bincol[b * 3] << 16) | ((unsigned long)bincol[b * 3 + 1] << 8) |
                    bincol[b * 3 + 2];
        ow[got++] = wk->n[b];
    }
    return got;
}

/* plan3[bin * 3 .. +2] = candidate a, candidate b, level; filled on first use from the bin's colour */
PM_MAYBE_UNUSED static void pp_bin_fill(unsigned char *plan3, const unsigned char *bincol, const pm_palette *pal,
                                        unsigned long usable, int bin)
{
    pm_pick pk;
    unsigned long rgb = ((unsigned long)bincol[bin * 3] << 16) | ((unsigned long)bincol[bin * 3 + 1] << 8) |
                        bincol[bin * 3 + 2];
    pm_match(pal, usable, rgb, 1, &pk);
    plan3[bin * 3] = (unsigned char)(pk.a < 0 ? 0 : pk.a);
    plan3[bin * 3 + 1] = (unsigned char)(pk.b < 0 ? plan3[bin * 3] : pk.b);
    plan3[bin * 3 + 2] = (unsigned char)(pk.a == pk.b ? 0 : pk.lev);
}

/* the candidate pixel (x, y) with colour rgb[0..2] takes */
PM_MAYBE_UNUSED static int pp_pen_bin(unsigned char *plan3, const unsigned char *bincol, const pm_palette *pal,
                                      unsigned long usable, const unsigned char *rgb, int x, int y)
{
    int bin = pp_bin_of(rgb);
    unsigned char *e = plan3 + bin * 3;
    if (e[2] == PP_UNSET)
        pp_bin_fill(plan3, bincol, pal, usable, bin);
    return e[2] && pm_bayer[y & 7][x & 7] < e[2] ? e[1] : e[0];
}

#endif
