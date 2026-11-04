/* iclabel_core -- RL21 SELECTA-AA-ICONTEXT: the pure part of the antialiased Workbench and DOpus icon labels
 * (thoughts/shared/research/s57-evidence/rl21-icon-label-aa.md). amarchy-flat's Text patch (flat.c flat_tx_c) reads the
 * label's box before and after the stock Text, and this file decides: which calls qualify, which pixels are glyph
 * (the mask), how much of each edge pixel the glyph covers (corner fill), and the colour or pen each pixel gets.
 * No OS calls: tests/test_iclabel.c drives every function on the host.
 *
 * Pixels are 32-bit values: 0x00RRGGBB on a true-colour screen (cybergraphics ReadPixelArray), a pen number on a pen
 * screen (ReadPixelArray8). The blend follows Selecta's mui_aa_draw ramp (MIT) through render/aa_ink_rule.h. */
#ifndef AMARCHY_ICLABEL_CORE_H
#define AMARCHY_ICLABEL_CORE_H

#include <stdint.h>

#define ICL_MAXW 320            /* widest label box blended (topaz 8: 40 characters); wider ones run stock */
#define ICL_MAXH 24             /* tallest label box blended */
#define ICL_MAXPX (ICL_MAXW * ICL_MAXH)

/* rp->DrawMode bits (graphics/rastport.h), repeated so the host needs no Amiga headers */
#define ICL_JAM2 1
#define ICL_COMPLEMENT 2
#define ICL_INVERSVID 4

#define ICL_COV_ONE 96          /* an empty pixel in one inner corner of the glyph (two orthogonal glyph neighbours) */
#define ICL_COV_TWO 160         /* an empty pixel in two or more such corners */

/* Does this Text call get antialiased? The task is "Workbench" or "dopus", the mode JAM1 or JAM2 with no COMPLEMENT
 * or INVERSVID, APen != BPen, the box (x, y, w, h) non-empty, at most ICL_MAXW x ICL_MAXH, and wholly inside the window's
 * inner area il..ir, it..ib (inclusive, rastport coordinates). 1 = yes. */
int icl_accept(const char *task, int mode, int apen, int bpen, int x, int y, int w, int h, int il, int it, int ir, int ib);

/* The glyph mask of a box of n pixels. JAM2 (jam2 != 0): a pixel is glyph when after is APen; exact != 0 compares the
 * values (pens), exact == 0 takes the nearer of apen and bpen (a 15/16-bit RTG screen reads the pens back rounded).
 * JAM1: a pixel is glyph when after differs from before (before = the backdrop). mask[i] = 1 or 0; returns the glyph count. */
int icl_mask(int jam2, int exact, const uint32_t *before, const uint32_t *after, uint32_t apen, uint32_t bpen, int n,
             uint8_t *mask);

/* Coverage from filling corners: glyph pixels 255; an empty pixel with glyph neighbours N+E, N+W, S+E or S+W gets
 * ICL_COV_ONE for one such corner, ICL_COV_TWO for two or more; any other empty pixel 0. Returns the partial count. */
int icl_coverage(const uint8_t *mask, int w, int h, uint8_t *cov);

/* The text colour as the screen shows it: after at the first glyph pixel. 0 = no glyph pixel (nothing to blend). */
int icl_fg(const uint8_t *mask, const uint32_t *after, int n, uint32_t *fg);

/* True colour: each partial pixel becomes ce_mix(fg, its own background (after), cov); every other pixel keeps after.
 * out may be after. Returns the pixels blended. */
int icl_blend_rgb(const uint8_t *cov, const uint32_t *after, uint32_t fg, int n, uint32_t *out);

/* Pen screens. The colour a shade step k (1..n) of fg over bg stands for */
uint32_t icl_pen_target(uint32_t fg_rgb, uint32_t bg_rgb, int k, int n);

/* The pen a shade gets: the pen the screen found nearest (pen, at pm_dist d_pen from the target) when it shows the
 * shade closer than fg (d_fg) and bg (d_bg) do (aa_ink_shade_accept); else the shade rounds to fg (upper half of the
 * ramp) or bg. */
int icl_pen_choice(int k, int n, int pen, long d_pen, long d_fg, long d_bg, int fgpen, int bgpen);

/* The pen for (fg pen, bg pen, step k); the caller resolves a miss (cache, ObtainBestPen, icl_pen_choice) */
typedef int (*icl_pen_fn)(void *u, int fgpen, int bgpen, int k);

/* Pen blend: each partial pixel's step k = aa_ink_index(cov, n); k = 0 keeps its pen, k = n + 1 is fgpen, any other
 * step asks pen_for (bg = its own pen). out may be after. Returns the pixels whose pen changed. */
int icl_blend_pens(const uint8_t *cov, const uint32_t *after, int fgpen, int n, icl_pen_fn pen_for, void *u, int count,
                   uint32_t *out);

/* The shade pen cache: one entry per (fg pen, bg pen, step) and the colours it was made for, so a palette change
 * re-obtains. obtained = the pen came from ObtainBestPen and is released on unload. */
#define ICL_CACHE 32
typedef struct icl_pen_entry {
    int16_t fgpen, bgpen, k, pen;
    uint32_t fg_rgb, bg_rgb;
    uint8_t obtained, used;
} icl_pen_entry;
typedef struct icl_pencache {
    icl_pen_entry e[ICL_CACHE];
} icl_pencache;

void icl_cache_clear(icl_pencache *c);
/* the cached pen, -1 on a miss (absent, or made for other colours) */
int icl_cache_find(const icl_pencache *c, int fgpen, int bgpen, int k, uint32_t fg_rgb, uint32_t bg_rgb);
/* Store a result. An entry of the same pens and step is replaced: *old_pen gets its pen when that one was obtained
 * (the caller releases it), else -1. Returns 0 when the cache is full (the caller releases pen itself). */
int icl_cache_put(icl_pencache *c, int fgpen, int bgpen, int k, uint32_t fg_rgb, uint32_t bg_rgb, int pen, int obtained,
                  int *old_pen);

#endif
