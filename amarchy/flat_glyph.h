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
#define FLAT_W_LEFT 0x0A                /* scroller arrow images (SYSIA_Which LEFTIMAGE..DOWNIMAGE): clone path only */
#define FLAT_W_UP 0x0B
#define FLAT_W_RIGHT 0x0C
#define FLAT_W_DOWN 0x0D
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
 * at (ox, oy); w, h are the image's Width and Height. side = flat_side_px of the screen's resolution (1 for square pixels):
 * the scroller arrows widen with it so they keep their shape on hires. */
int flat_glyph_draw(const flat_state *s, unsigned long which, unsigned long method, unsigned long ids, int ox, int oy,
                    int w, int h, int frame_pen, int side, flat_fill_fn fill, void *ctx);

/* FLAT-C8: the flat frame of one frameiclass IM_DRAW / IM_DRAWFRAME call: with fill != 0 the box is one solid fill,
 * NO outline: pen_button (pen_back when 0), pen_line when ids == IDS_SELECTED (1) (FLAT-BUTTON-FILL); with fill == 0 only a
 * full 1 px outline in pen_line. Returns 1 when drawn, 0
 * when the box is under 3x3 or fill_fn is null (the caller then runs the stock routine). */
int flat_frame_draw(const flat_state *s, int ids, int ox, int oy, int w, int h, int fill, int role, flat_fill_fn fn, void *ctx);

/* FLAT-BUTTON-ROUND: rd(ctx, x, y) reads the pen already on the surface (ReadPixel; negative = unreadable). */
typedef int (*flat_read_fn)(void *ctx, int x, int y);
/* The rounded corners of a button box of size w x h, window-relative: each cut pixel is {x, y, sx, sy} where (x, y) is inside the box
 * and (sx, sy) the pixel just outside it, diagonally beyond the corner (the colour the cut takes). A 1 px cut at each corner; a box at
 * least FLAT_ROUND_TALL (12) px high also cuts the two pixels beside each corner along the edges (their outside pixel is straight
 * above / below / left / right of them). Returns the number of cuts (4 or 12), 0 when the box is under 3 x 3. out holds 12. */
#define FLAT_ROUND_TALL 12
#define FLAT_ROUND_MAX 12
int flat_button_corners(int w, int h, int out[][4]);
/* FLAT-SLIDER-ROUND: the knob gets the button radius. When the fill (x0,y0)-(x1,y1) is exactly the knob rectangle, the cut pixels
 * (absolute x, y) that take the track pen are written to out (FLAT_ROUND_MAX at most) and their count returned, else 0. The corner
 * rule is flat_button_corners (the same cuts as a flat button of the knob's size). */
int flat_scroller_cuts(int cx, int cy, int cw, int ch, int horiz, int vert, unsigned hpot, unsigned vpot, unsigned hbody, unsigned vbody,
                       int x0, int y0, int x1, int y1, int out[][2]);
/* flat_frame_draw plus rounded corners: the outside pens are read with rd BEFORE the box is painted, then each cut pixel takes its
 * sampled pen (an unreadable pixel is left as drawn). rd null: no rounding, same as flat_frame_draw. */
int flat_frame_draw_round(const flat_state *s, int ids, int ox, int oy, int w, int h, int fill, int role, flat_fill_fn fn,
                          flat_read_fn rd, void *ctx);

/* FLAT-OUTER-EDGE: the four 1 px strips of the outer edge of a window of size w x h, as fill(ctx, x0, y0, x1, y1, pen)
 * calls in pen_outline: top row y = 0, bottom y = h - 1, left x = 0, right x = w - 1 (window-relative; nothing inside).
 * Returns the number of strips drawn: 4, or 0 when pen_outline is 0 (no outline), a window under 2 x 2, or fn is null.
 * Left runs y = 1 .. h - 1, right y = 1 .. h - 2 and bottom x = 1 .. w - 1, so no pixel is drawn twice. */
int flat_outline_rects(const flat_state *s, int w, int h, flat_fill_fn fn, void *ctx);

/* 1 when (x0,y0)-(x1,y1) is exactly one of the four strips flat_outline_rects paints (the same geometry, which is the shape
 * Intuition's border redraw fills with RectFill), else 0. Used by the graphics RectFill patch to pick out Intuition's
 * outer-edge fills. */
int flat_outer_edge_match(int w, int h, int x0, int y0, int x1, int y1);

/* FLAT-ASPECT-BORDERS: the width in pixels of a vertical 1-pixel-looking line on a mode whose DisplayInfo Resolution is
 * rx x ry ticks per pixel. Non-square modes with taller pixels (PAL/NTSC hires non-interlace 22 x 44, super hires
 * 11 x 44) give ry / rx rounded (2, 4), so the left and right frame lines look as thick as the 1 px top and bottom
 * lines. Square modes, wide-pixel modes, RTG and an unknown mode (rx or ry 0) give 1. */
int flat_side_px(int rx, int ry);

/* The rect the RectFill patch fills for one of Intuition's outer strips: when (x0,y0)-(x1,y1) is exactly one strip
 * (flat_outer_edge_match), writes it to out[] with the left strip widened rightwards and the right strip widened
 * leftwards to px columns (px clamped to 1 .. w / 2), top and bottom unchanged, and returns 1; else returns 0. */
int flat_outer_edge_fill(int w, int h, int x0, int y0, int x1, int y1, int px, int out[4]);


/* FLAT-GLYPH-OS-SHAPES: the clone of the system's own gadget image. The OS side draws the stock image offscreen with a private
 * DrawInfo whose pen i is the marker i + 1 (FLAT_CLONE_MARK(i)); flat_clone_map turns that grid into box (0) and glyph (1):
 * the bevel ring (SHINE / SHADOW / BACKGROUND markers on the outermost 1 px) and the ground (the marker filling most of the
 * inside, 0 = untouched) become the box, every other pixel is one glyph pixel. */
#define FLAT_CLONE_MARK(i) ((i) + 1)
/* dri_Pens indices (intuition/screens.h): SHINEPEN 3, SHADOWPEN 4, BACKGROUNDPEN 7 */
#define FLAT_CLONE_BACK FLAT_CLONE_MARK(7)
#define FLAT_CLONE_SHINE FLAT_CLONE_MARK(3)
#define FLAT_CLONE_SHADOW FLAT_CLONE_MARK(4)
#define FLAT_CLONE_MARKS 16
#define FLAT_CLONE_SCRATCH (2 * 32 * 32)
#define FLAT_CLONE_MAX 32           /* the two grids (FLAT_CLONE_SCRATCH bytes) are the caller's, never the calling task's stack */
enum { FLAT_CLONE_NO = 0, FLAT_CLONE_OK = 1, FLAT_CLONE_FAIL = 2 };    /* not routed / drawn / render failed (use the block drawing) */
typedef int (*flat_render_fn)(void *ctx, unsigned long ids, uint8_t *grid, int w, int h);
void flat_glyph_roles(unsigned long which, int state, int *glyph_role, int *box_role);
int flat_glyph_window_gadget(unsigned long which);
/* 1 for the images that sit in the title bar (close, depth, zoom, iconify): their box is the title band. Size and the scroller
 * arrows sit in the border, whose colour is the stock BACKGROUNDPEN. */
int flat_glyph_on_band(unsigned long which);
/* The clone path's own route: flat on, not held, method OM_DRAW, and a window-border image (flat_glyph_window_gadget). */
int flat_clone_route(const flat_state *s, unsigned long which, unsigned long method);
int flat_clone_map(const uint8_t *grid, int w, int h, uint8_t *out);
int flat_clone_pen(const flat_state *s, int role, int frame_pen);
int flat_clone_draw(const flat_state *s, unsigned long which, unsigned long method, unsigned long ids, int ox, int oy,
                    int w, int h, int frame_pen, int text_pen, flat_render_fn render, void *rctx, flat_fill_fn fill, void *fctx, uint8_t *scratch);
/* scratch: FLAT_CLONE_SCRATCH bytes owned by the caller for the call. text_pen: >= 0 replaces the pen of the normal (active, unselected) glyph, the pen the title text uses on the band; -1 keeps flat_glyph_pen. */
#endif
