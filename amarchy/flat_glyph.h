/* amarchy-flat, flat window and menu glyphs: pure geometry (plan 2026-10-29-amarchy-flat-ui-patch.md FLAT-C2; the glyphs
 * are solid filled blocks only, FLAT-SOLID-FILL). For one SYSIA_Which value, a box size and a state
 * the plan is a list of filled rectangles with a pen role. Rectangles only (1 pixel wide ones included), so the host
 * canvas and graphics.library RectFill give identical pixels; no diagonal Draw. No OS calls. */
#ifndef AMARCHY_FLAT_GLYPH_H
#define AMARCHY_FLAT_GLYPH_H

#include <stdint.h>
#include "flat_core.h"

/* SYSIA_Which values (intuition/imageclass.h) that C2 draws; every other value goes to the old hook. */
#define FLAT_W_DEPTH 0x00
#define FLAT_W_ZOOM 0x01
#define FLAT_W_SIZE 0x02
#define FLAT_W_CLOSE 0x03
#define FLAT_W_MENUCHECK 0x10
#define FLAT_W_AMIGAKEY 0x11
#define FLAT_W_ICONIFY 0x16
#define FLAT_W_MENUSUB 0x1C

#define FLAT_IM_DRAW 0x202UL    /* the only method C2 draws; IM_DRAWFRAME and the rest go to the old hook */

enum { FLAT_G_NORMAL = 0, FLAT_G_SELECTED = 1, FLAT_G_INACTIVE = 2 };
enum { FLAT_ROLE_BACK = 0, FLAT_ROLE_TEXT = 1, FLAT_ROLE_LINE = 2, FLAT_ROLE_FRAME = 3 };
/* FLAT_ROLE_FRAME: the pen of the frame the gadget sits on (window border, inactive border, screen bar block); drawn
 * as pen_back when flat_glyph_draw gets frame_pen < 0. Only the window gadgets use it as their box background. */

typedef struct flat_prim {
    int16_t x0, y0, x1, y1;     /* inclusive, relative to the box's top left */
    uint8_t role;               /* FLAT_ROLE_* */
} flat_prim;

#define FLAT_GLYPH_MAX_PRIMS 40
#define FLAT_GLYPH_MIN_SIZE 7   /* a box smaller than this in either direction is left to the old hook */

/* 1 when C2 draws this SYSIA_Which. */
int flat_glyph_known(unsigned long which);

/* intuition IDS_* image state -> FLAT_G_*: IDS_SELECTED and IDS_INACTIVESELECTED(6) read as selected / inactive. */
int flat_glyph_state(unsigned long ids);

/* The rectangles of one glyph: the first is always the whole box in the state's background role. Returns the count,
 * 0 for an unknown which or a box below FLAT_GLYPH_MIN_SIZE (cap must be >= FLAT_GLYPH_MAX_PRIMS). */
int flat_glyph_plan(unsigned long which, int w, int h, int state, flat_prim *out, int cap);

/* The pen index of a role from the shared state block. */
int flat_glyph_pen(const flat_state *s, int role);

/* Does the dispatcher draw this call itself? Needs the switch on, not held, IM_DRAW and a known which. */
int flat_glyph_route(const flat_state *s, unsigned long which, unsigned long method);

/* fill(ctx, x0, y0, x1, y1, pen): the OS side's SetAPen + RectFill, a canvas in the host test. */
typedef void (*flat_fill_fn)(void *ctx, int x0, int y0, int x1, int y1, int pen);

/* The whole dispatcher decision and draw. Returns 1 when the glyph was drawn (the caller returns TRUE and does not
 * call the old hook), 0 when the call must go to the old hook unchanged (route refused, box too small). The box is
 * at (ox, oy); w, h are the image's Width and Height. */
int flat_glyph_draw(const flat_state *s, unsigned long which, unsigned long method, unsigned long ids, int ox, int oy,
                    int w, int h, int frame_pen, flat_fill_fn fill, void *ctx);

/* FLAT-C8: the flat frame of one frameiclass IM_DRAW / IM_DRAWFRAME call: with fill != 0 the box is one solid fill,
 * NO outline: pen_button (pen_back when 0), pen_line when ids == IDS_SELECTED (1) (FLAT-BUTTON-FILL); with fill == 0 only a
 * full 1 px outline in pen_line. Returns 1 when drawn, 0
 * when the box is under 3x3 or fill_fn is null (the caller then runs the stock routine). */
int flat_frame_draw(const flat_state *s, int ids, int ox, int oy, int w, int h, int fill, flat_fill_fn fn, void *ctx);

/* FLAT-OUTER-EDGE: the four 1 px strips of the outer edge of a window of size w x h, as fill(ctx, x0, y0, x1, y1, pen)
 * calls in pen_outline: top row y = 0, bottom y = h - 1, left x = 0, right x = w - 1 (window-relative; nothing inside).
 * Returns the number of strips drawn: 4, or 0 when pen_outline is 0 (no outline), a window under 2 x 2, or fn is null.
 * Left runs y = 1 .. h - 1, right y = 1 .. h - 2 and bottom x = 1 .. w - 1, so no pixel is drawn twice. */
int flat_outline_rects(const flat_state *s, int w, int h, flat_fill_fn fn, void *ctx);

/* 1 when (x0,y0)-(x1,y1) is exactly one of the four strips flat_outline_rects paints (the same geometry, which is the shape
 * Intuition's border redraw fills with RectFill), else 0. Used by the graphics RectFill patch to pick out Intuition's
 * outer-edge fills. */
int flat_outer_edge_match(int w, int h, int x0, int y0, int x1, int y1);

#endif
