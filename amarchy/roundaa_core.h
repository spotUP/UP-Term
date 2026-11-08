/* roundaa_core -- RL19 SELECTA-AA-ROUND: antialiased rounded corners for Amarchy's own filled boxes (flat buttons, cycle and
 * scroller knobs). The box is drawn as before; each pixel of a corner then takes the pen that looks nearest to the box colour
 * mixed over the colour around it at the pixel's coverage of the rounded rectangle.
 *
 * The maths follows Selecta's mui_aa_round_inset (MusicPlayer plugins/ui/mui/map/mui_aa.c, MIT, Copyright (c) 2026 Johan
 * Samuelsson): the circle through doubled integer coordinates. Reused here, not copied again: ce_mix (render/emoji.c) for the
 * mix, pm_aim / pm_dist (render/pen_match.h) for the nearest pen, flat_frame_draw_round (flat_glyph.c) for the stock round box.
 * Pure (no AmigaOS), so the host tests it (tests/test_flat_core.c). */
#ifndef ROUNDAA_CORE_H
#define ROUNDAA_CORE_H

#include "flat_glyph.h"

#define RAA_MAX_RADIUS 4
#define RAA_MAX_PENS 256

/* Corner radius in pixels of a box of this height: 2 below FLAT_ROUND_TALL, else 4 */
int raa_radius(int h);
/* Selecta mui_aa_round_inset: the pixels row y (0..h-1) of a w x h rounded box leaves empty at each end (0 = full row) */
int raa_round_inset(int w, int h, int r, int y);
/* Coverage 0..255 of pixel (x, y) of a w x h box with corner radius r by the rounded rectangle (4 x 4 samples per pixel) */
int raa_cover(int w, int h, int r, int x, int y);
/* The partly covered pixels of the top left r x r corner square (r <= 4): bit (y * 4 + x) set when raa_cover(w, h, r, x, y) < 255.
 * The cover is the same in all four mirrored corners, so a caller tests corner pixel (dx, dy) of any corner with one shift and
 * makes no raa_cover call per pixel (S62-FLAT-RF-TX-BUDGET: dim_fill on the drawing task's stack). */
unsigned raa_partial_mask(int w, int h, int r);
/* 0xRRGGBB of screen pen p, as the host answers it */
typedef unsigned long (*raa_rgb_fn)(void *ctx, int pen);
/* The pen of 0..npens-1 whose colour looks nearest to rgb (pen_match's redmean + chroma distance) */
int raa_nearest(raa_rgb_fn rgb, void *ctx, int npens, unsigned long want);
/* flat_frame_draw_round, then the antialiased corners. Falls back to flat_frame_draw_round alone (return as it) when rd or rgb is
 * null, the box is not filled, has no readable pixel beyond a corner, or npens is 0 or over
 * RAA_MAX_PENS. Returns the number of corner pixels it blended in *blended when that is not null. */
/* raa_frame_draw in two halves a caller runs one after the other (S62-FLAT-RF-TX-BUDGET: neither frame is under the other, and
 * the arguments travel as one pointer, so the RectFill hook's border-button fill keeps its job in a static and its stack small).
 * raa_frame_box reads the pen beside each corner, then paints flat_frame_draw_round. It returns RAA_BOX_NONE when that drew
 * nothing, RAA_BOX_PLAIN when the box is drawn and the corners stay as drawn (the fallbacks raa_frame_draw names), or
 * RAA_BOX_BLEND: j->bg then holds corner c's pen in bits 8c .. 8c+7 for raa_frame_corners, which blends the corner pixels. */
typedef struct raa_job {
    const flat_state *s;
    flat_fill_fn fn;
    flat_read_fn rd;
    raa_rgb_fn rgb;
    void *ctx;
    int ids, ox, oy, w, h, fill, role, npens;
    unsigned long bg;                         /* out of raa_frame_box */
} raa_job;
#define RAA_BOX_NONE  0
#define RAA_BOX_PLAIN 1
#define RAA_BOX_BLEND 2
int raa_frame_box(raa_job *j);
void raa_frame_corners(const raa_job *j, int *blended);
int raa_frame_draw(const flat_state *s, int ids, int ox, int oy, int w, int h, int fill, int role, flat_fill_fn fn, flat_read_fn rd,
                   raa_rgb_fn rgb, int npens, void *ctx, int *blended);
/* flat_knob_frame_draw (the Workbench scroller knob: state pen box, plain role) through raa_frame_draw */
int raa_knob_frame_draw(const flat_state *s, int depth, int active, int ox, int oy, int w, int h, flat_fill_fn fn, flat_read_fn rd,
                        raa_rgb_fn rgb, int npens, void *ctx, int *blended);

#endif
