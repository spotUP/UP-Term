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

#define ICL_MAXW 1024           /* widest box blended (a window title across a wide screen); wider ones run stock */
#define ICL_MAXH 24             /* tallest box blended */
#define ICL_MAXPX (320 * ICL_MAXH)  /* most pixels in one box (the buffers): 320 x 24, a 960 x 8 title, a 480 x 16 title */

/* rp->DrawMode bits (graphics/rastport.h), repeated so the host needs no Amiga headers */
#define ICL_JAM2 1
#define ICL_COMPLEMENT 2
#define ICL_INVERSVID 4

#define ICL_COV_ONE 96          /* an empty pixel in one inner corner of the glyph (two orthogonal glyph neighbours) */
#define ICL_COV_TWO 160         /* an empty pixel in two or more such corners */

/* Does this Text call get antialiased? ui-antialias is on (aa_on != 0; any task), the mode JAM1 or JAM2 with no COMPLEMENT
 * or INVERSVID, APen != BPen, the box (x, y, w, h) non-empty, at most ICL_MAXW wide,
 * ICL_MAXH tall and ICL_MAXPX pixels, and wholly inside the window's
 * inner area il..ir, it..ib (inclusive, rastport coordinates). 1 = yes. */
int icl_accept(int aa_on, int mode, int apen, int bpen, int x, int y, int w, int h, int il, int it, int ir, int ib);

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
    const void *screen;             /* S57-ICL-PEN-RECOLOUR: the screen the obtained pens belong to, 0 = none (as flat_pens) */
    unsigned epoch;                 /* its ipd_epoch */
} icl_pencache;

void icl_cache_clear(icl_pencache *c);
/* the cached pen, -1 on a miss (absent, or made for other colours) */
int icl_cache_find(const icl_pencache *c, int fgpen, int bgpen, int k, uint32_t fg_rgb, uint32_t bg_rgb);
/* 1 when icl_cache_put for this key can store (same key present, or a free slot). With 0 the caller must not ObtainBestPen:
 * an obtained pen that cannot be cached would be released while its pixels are drawn (S57-ICL-PEN-RECOLOUR). */
int icl_cache_has_room(const icl_pencache *c, int fgpen, int bgpen, int k);
/* Store a result. An entry of the same pens and step is replaced: *old_pen gets its pen when that one was obtained
 * (the caller releases it), else -1. Returns 0 when the cache is full (the caller releases pen itself). */
int icl_cache_put(icl_pencache *c, int fgpen, int bgpen, int k, uint32_t fg_rgb, uint32_t bg_rgb, int pen, int obtained,
                  int *old_pen);

/* S57-ICL-PEN-RECOLOUR: the shade pens stay obtained across Text calls, so no other client is handed (and recolours) a pen number
 * that glyph pixels already on screen use. Like flat_pens they belong to one screen identity (screen pointer + ipd_epoch).
 * Bind at the start of a label call: on another identity the cache is forgotten WITHOUT any ReleasePen (the pens went with the
 * old ColorMap; ReleasePen on a freed map waits for ever on its dead PalExtra semaphore, s57 boot hang). Returns 1 when forgotten. */
int icl_cache_bind(icl_pencache *c, const void *screen, unsigned epoch);
/* S57-ICL-PINK-SHADE: after icl_cache_bind said "same identity", ask the ColorMap whether the obtained pens are still held
 * (ip_any_free). A Workbench reopen at the same Screen address before the next pens_sync keeps screen and epoch equal, yet the
 * cached numbers name pens of the OLD map: free on the new one, showing its default colours (pink). If any is free the cache is
 * forgotten WITHOUT ReleasePen (identity kept, so the next bind does not clear pens obtained afterwards). Returns 1 when forgotten. */
int icl_cache_verify(icl_pencache *c, int (*is_free)(void *ctx, int pen), void *ctx);
/* Give the pens back (shutdown): write the obtained pens into out[ICL_CACHE] (each once) and empty the cache, but only when
 * `screen`/`epoch` is the identity they were obtained on (the caller releases them on that screen's map); on any other identity,
 * or no screen (0), they are forgotten and 0 is returned. */
int icl_cache_release(icl_pencache *c, const void *screen, unsigned epoch, int *out);
/* The obtained pens into out[] (each once), cache emptied. Only for a caller that holds the live map. */
int icl_cache_drain(icl_pencache *c, int *out);

/* TE-10.6a: one pen cache per screen slot (same lookup rule as flat_pens_tab, flat_ident_slot). Switching windows between screens
 * binds another slot instead of forgetting the first screen's obtained pens. Equal to IP_SCREENS (checked in tests/test_iclabel.c). */
#define ICL_TAB_SLOTS 40
typedef struct icl_pentab {
    icl_pencache slot[ICL_TAB_SLOTS];
} icl_pentab;
void icl_tab_init(icl_pentab *t);
/* The cache to use for (screen, epoch): its own, else a free one bound to the identity; 0 when full or screen is 0. */
icl_pencache *icl_tab_for(icl_pentab *t, const void *screen, unsigned epoch);
/* The cache holding exactly that identity, else 0. */
const icl_pencache *icl_tab_find(const icl_pentab *t, const void *screen, unsigned epoch);
/* Forget (WITHOUT release) every slot whose identity is_open says is gone; returns the count. */
int icl_tab_reap(icl_pentab *t, int (*is_open)(void *ctx, const void *screen, unsigned epoch), void *ctx);

#endif
