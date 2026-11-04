/* richaa_core -- RL20 SELECTA-AA-RICHUI: blended rounded corners for the boxes of the rich Amarchy menu (amarchy-gui): the
 * selected row, the welcome window's Close / Open Help buttons. The box is filled as before; each pixel of a corner then takes
 * the box colour mixed over the colour around it at the pixel's coverage of the rounded rectangle. Text and its sizes already
 * go through aa_ink (menu_text.c); this file adds the box edges. It reuses roundaa_core (raa_radius, raa_cover, raa_nearest: RL19,
 * Selecta mui_aa_round_inset, MIT) and ce_mix (render/emoji.c). Pure (no AmigaOS): tests/test_richaa.c. */
#ifndef AMARCHY_RICHAA_CORE_H
#define AMARCHY_RICHAA_CORE_H

#include "roundaa_core.h"

#define RICH_MIN_DEPTH 4       /* below 16 colours the pens cannot show a shade: the box keeps its stock square corners */

/* Writes the colour of one pixel (screen coordinates); the host decides colour or nearest pen */
typedef void (*rich_put_fn)(void *ctx, int x, int y, unsigned long rgb);

/* Blends the four corners of the filled box (x, y, w, h) of colour fg over bg. Returns the pixels written (corner pixels with
 * coverage under 255, those with coverage 0 included: they take bg back); 0 and nothing written when the box is under 3 x 3 or
 * the screen is under RICH_MIN_DEPTH deep. rich_aa_calls counts every call that wrote (the tests' and the rig's proof it ran). */
int rich_corners(int depth, int x, int y, int w, int h, unsigned long fg, unsigned long bg, rich_put_fn put, void *ctx);

extern unsigned long rich_aa_calls;

#endif
