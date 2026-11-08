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
enum { FLAT_ROLE_BACK = 0, FLAT_ROLE_TEXT = 1, FLAT_ROLE_LINE = 2, FLAT_ROLE_FRAME = 3, FLAT_ROLE_KNOB = 4 };
/* FLAT_ROLE_KNOB (S32-ARROW-KNOB-COLOUR): the scroller knob's state pen (flat_knob_state_pen: accent in an active window, inactive bar tone
 * otherwise), passed in as knob_pen; pen_line when knob_pen < 0. The scroller arrows and the size gadget draw in it, pressed or not. */
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
                    int w, int h, int frame_pen, int knob_pen, int side, flat_fill_fn fill, void *ctx);

/* FLAT-C8: the flat frame of one frameiclass IM_DRAW / IM_DRAWFRAME call: with fill != 0 the box is one solid fill,
 * NO outline: pen_button (pen_back when 0), pen_line when ids == IDS_SELECTED (1) (FLAT-BUTTON-FILL); with fill == 0 only a
 * full 1 px outline in pen_line. Returns 1 when drawn, 0
 * when the box is under 3x3 or fill_fn is null (the caller then runs the stock routine). */
int flat_frame_draw(const flat_state *s, int ids, int ox, int oy, int w, int h, int fill, int role, flat_fill_fn fn, void *ctx);

/* 0c DOPUS-ENV-UNSTYLED: the flat result of one dopus5.library DrawBox / DrawFieldBox call (FLAT-D4) under flat pens. Both
 * stock routines draw the bevel edges only, never the inside (the caller has drawn or will draw the label and contents), so
 * the flat equivalent is flat_frame_draw's edges-only form: a full outline in pen_line, s->side columns wide at the sides.
 * x0..y1 is the inclusive struct Rectangle the routine got. Returns 1 when drawn, 0 when the box is under 3x3 or fn is
 * null (the caller then draws nothing, as before). */
int flat_dopus_box_draw(const flat_state *s, int x0, int y0, int x1, int y1, flat_fill_fn fn, void *ctx);

/* FLAT-BUTTON-ROUND: rd(ctx, x, y) reads the pen already on the surface (ReadPixel; negative = unreadable). */
typedef int (*flat_read_fn)(void *ctx, int x, int y);
/* The rounded corners of a button box of size w x h, window-relative: each cut pixel is {x, y, sx, sy} where (x, y) is inside the box
 * and (sx, sy) the pixel just outside it, diagonally beyond the corner (the colour the cut takes). A 1 px cut at each corner; a box at
 * least FLAT_ROUND_TALL (12) px high also cuts the two pixels beside each corner along the edges (their outside pixel is straight
 * above / below / left / right of them). Returns the number of cuts (4 or 12), 0 when the box is under 3 x 3. out holds 12. */
#define FLAT_ROUND_TALL 12
#define FLAT_ROUND_MAX 12
int flat_button_corners(int w, int h, int out[][4]);
/* Cut i of flat_button_corners(w, h) into out (same order and values), 1 when it exists, else 0. The draw hooks use this one at a time
 * (S62-FLAT-RF-TX-BUDGET: no FLAT_ROUND_MAX x 4 array on the stack of the drawing task). */
int flat_button_corner(int w, int h, int i, int out[4]);
/* FLAT-SLIDER-ROUND: the knob gets the button radius. When the fill (x0,y0)-(x1,y1) is exactly the knob rectangle, the cut pixels
 * (absolute x, y) that take the track pen are written to out (FLAT_ROUND_MAX at most) and their count returned, else 0. The corner
 * rule is flat_button_corners (the same cuts as a flat button of the knob's size). */
int flat_scroller_cuts(int cx, int cy, int cw, int ch, int horiz, int vert, unsigned hpot, unsigned vpot, unsigned hbody, unsigned vbody,
                       int x0, int y0, int x1, int y1, int out[][2]);
/* flat_scroller_cuts with the knob rectangle given (S56-XCON-KNOB-BOX: the box flat_scroller_knob_img reads from Intuition). */
int flat_scroller_cuts_box(int kx, int ky, int kw, int kh, int x0, int y0, int x1, int y1, int out[][2]);
/* Cut i (0 .. FLAT_ROUND_MAX-1) of the knob box as flat_scroller_cuts_box tests it: 1 and out = (x, y) when cut i exists and lies
 * inside the fill, else 0. flat_scroller_cuts_box lists exactly the cuts this accepts, in index order. */
int flat_scroller_cut_at(int kx, int ky, int kw, int kh, int x0, int y0, int x1, int y1, int i, int out[2]);
/* flat_frame_draw plus rounded corners: the outside pens are read with rd BEFORE the box is painted, then each cut pixel takes its
 * sampled pen (an unreadable pixel is left as drawn). rd null: no rounding, same as flat_frame_draw. */
int flat_frame_draw_round(const flat_state *s, int ids, int ox, int oy, int w, int h, int fill, int role, flat_fill_fn fn,
                          flat_read_fn rd, void *ctx);
/* S56-LOCALE-KNOB: a SMARTKNOBIMAGE prop (3.2 GadTools listview scrollers) draws its knob through a frameiclass IM_DRAWFRAME, not a
 * RectFill, so scroller_pen never sees it. flat_smart_knob says whether image is that knob: prop gadget, AUTOKNOB and SMARTKNOBIMAGE set,
 * and image is the BOOPSI frame Intuition hangs off the AUTOKNOB Image's NextImage. flat_knob_frame_draw paints it like a Workbench
 * window knob: a box in flat_knob_state_pen (accent active, inactive bar tone) with the rounded corners of flat_frame_draw_round. */
int flat_smart_knob(int is_prop, unsigned prop_flags, const void *knob_next, const void *image);
int flat_knob_frame_draw(const flat_state *s, int depth, int active, int ox, int oy, int w, int h, flat_fill_fn fn, flat_read_fn rd, void *ctx);

/* FLAT-OUTER-EDGE: the four 1 px strips of the outer edge of a window of size w x h, as fill(ctx, x0, y0, x1, y1, pen)
 * calls in pen_outline: top row y = 0, bottom y = h - 1, left x = 0, right x = w - 1 (window-relative; nothing inside).
 * Returns the number of strips drawn: 4, or 0 when pen_outline is 0 (no outline), a window under 2 x 2, or fn is null.
 * Left runs y = 1 .. h - 1, right y = 1 .. h - 2 and bottom x = 1 .. w - 1, so no pixel is drawn twice. */
int flat_outline_rects(const flat_state *s, int w, int h, flat_fill_fn fn, void *ctx);

/* 1 when (x0,y0)-(x1,y1) is exactly one of the four strips flat_outline_rects paints (the same geometry, which is the shape
 * Intuition's border redraw fills with RectFill), else 0. Used by the graphics RectFill patch to pick out Intuition's
 * outer-edge fills. */
int flat_outer_edge_match(int w, int h, int x0, int y0, int x1, int y1);

/* The rect the RectFill patch fills for one of Intuition's outer strips: when (x0,y0)-(x1,y1) is exactly one strip
 * (flat_outer_edge_match), writes it to out[] with the left strip widened rightwards and the right strip widened
 * leftwards to px columns (px clamped to 1 .. w / 2), top and bottom unchanged, and returns 1; else returns 0. */
int flat_outer_edge_fill(int w, int h, int x0, int y0, int x1, int y1, int px, int out[4]);

/* S32-ARROW-OUTLINE: the parts of the box (bx0,by0)-(bx1,by1) (window-relative, inclusive) that lie on the outer ring of a
 * w x h window, as the strips flat_outer_edge_fill paints them (left/right strips px columns wide). Calls fill(ctx, rect, pen)
 * for each of up to 4 rects and returns their count (0 when the box misses the ring). A gadget box reaching the last column or
 * row overlaps the ring; the caller refills these with the edge pen after the box fill. The rects go to a callback, not an
 * array, so the caller's stack frame stays inside the flat budget. */
int flat_outer_ring_overlap(int w, int h, int px, int bx0, int by0, int bx1, int by1, flat_fill_fn fill, void *ctx, int pen);


/* FLAT-GLYPH-OS-SHAPES: the clone of the system's own gadget image. The OS side draws the stock image offscreen with a private
 * DrawInfo whose pen i is the marker i + 1 (FLAT_CLONE_MARK(i)); flat_clone_map turns that grid into box (0) and glyph (1):
 * the bevel ring (SHINE / SHADOW / BACKGROUND markers on the outermost 1 px), the ground (the marker filling most of the
 * inside, 0 = untouched) and every BACKGROUND pixel (a hole, e.g. inside the depth image's back window) become the box,
 * every other pixel is one glyph pixel. */
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
/* S32 CLONE-PRIVATE-IMAGE: OS 3.2 sysiclass (intuition 47) renders its imagery ONCE, at OM_NEW, into private planar data with
 * the SYSIA_DrawInfo pens of that moment; IM_DRAW only blits that data and never reads imp_DrInfo. The clone therefore draws
 * a private image made with the marker DrawInfo, one per (which, width, height), kept in a small table. flat_clone_slot finds
 * the entry for the key: its index, n when it must be appended (n < cap), or -1 when the table is full. */
#define FLAT_CLONE_OBJS 16
#define FLAT_CLONE_DEPTH 8          /* the marker DrawInfo's dri_Depth: the private image is baked deep enough for every marker */
typedef struct flat_clone_key { unsigned long which; int w, h; } flat_clone_key;
int flat_clone_slot(const flat_clone_key *keys, int n, int cap, unsigned long which, int w, int h);
int flat_clone_map(const uint8_t *grid, int w, int h, uint8_t *out);
/* S32-CLONE-CLIP: the draw offset that puts the private image's column 0 at grid column 0. 3.2 sysiclass gives the depth and
 * zoom images LeftEdge -1 (close and size 0): drawn at offset 0 the image's first column fell off the bitmap, the glyph sat
 * 1 px left and its right bevel became a glyph line (rig 23, s32-evidence/clone-clip-hunt.md). */
int flat_clone_offset(int edge);
int flat_clone_pen(const flat_state *s, int role, int frame_pen, int knob_pen);
/* S32-ARROW-KNOB-COLOUR: the one place both draw paths settle the box role: a knob-pen glyph on a box of the same pen would vanish, so that box takes pen_back. */
int flat_glyph_box_role(const flat_state *s, int grole, int brole, int frame_pen, int knob_pen);
int flat_glyph_knob_gadget(unsigned long which);
/* S32-TITLE-STRIP: how many rows of a gadget box (top at window row box_top, height h) lie on the title band, which is window rows 0 .. border_top-2 (the bar's last row is its edge, the background colour): clamp(border_top - 1 - box_top, 0, h). */
int flat_clone_band_rows(int box_top, int h, int border_top);
int flat_clone_draw(const flat_state *s, unsigned long which, unsigned long method, unsigned long ids, int ox, int oy,
                    int w, int h, int frame_pen, int knob_pen, int text_pen, int band_rows, int edge_pen, flat_render_fn render, void *rctx, flat_fill_fn fill, void *fctx, uint8_t *scratch);
/* The arguments of flat_clone_draw as one struct: the draw hook passes a pointer (7 arguments, not 18), because every argument is
 * pushed on the stack of whatever task draws the window, and the flat budget caps that frame. flat_clone_draw is the same call with
 * the fields spelled out (the unit tests' form). */
typedef struct flat_clone_in {
    unsigned long which, method, ids;
    int ox, oy, w, h, frame_pen, knob_pen, text_pen, band_rows, edge_pen;
} flat_clone_in;
int flat_clone_draw_in(const flat_state *s, const flat_clone_in *in, flat_render_fn render, void *rctx, flat_fill_fn fill, void *fctx, uint8_t *scratch);
/* scratch: FLAT_CLONE_SCRATCH bytes owned by the caller for the call. band_rows: box rows y < band_rows are filled with the frame/box pen, the rest with edge_pen (>= 0; -1 keeps the box pen); band_rows >= h paints the whole box as before. text_pen: >= 0 replaces the pen of every unselected glyph (active, inactive, disabled: S32-INACTIVE-BAR), the pen the title text uses on the band or the inactive tone of an inactive window arrow; -1 keeps flat_glyph_pen. */
#endif
