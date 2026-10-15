/* A fake graphics.library for the host: render/amiga_render.c compiled against the NDK's structs
 * and these stubs (tests/test_render.c). Every call the renderer makes on the RastPort is logged;
 * Text, RectFill and ScrollRaster also draw into a pixel canvas, so a test can ask what the
 * window shows and which calls put it there. Nothing here is exec, DOS or a real screen. */
#ifndef GFX_FAKE_H
#define GFX_FAKE_H

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/text.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>

enum gf_kind { GF_TEXT, GF_RECT, GF_SCROLL, GF_BLT, GF_MOVE, GF_CGXFILL, GF_SETRGB, GF_ALLOC, GF_TOF, GF_OTHER };

typedef struct gf_call {
    enum gf_kind kind;
    long a, b, c, d;      /* BLT (BltBitMapRastPort): dest x y, w h; ALLOC: the back bitmap's w h; CGXFILL: x y w h; TEXT: x, y (top left of the cell), count, -; RECT: x0 y0 x1 y1; SCROLL: dx dy y0 y1; MOVE: x y */
    char str[16];         /* TEXT: the first bytes drawn */
    unsigned long drmd;   /* the draw mode at the call */
} gf_call;

#define GF_LOG_MAX 4096
extern gf_call gf_log[GF_LOG_MAX];
extern int gf_n;

/* The canvas value of a pixel: 0 untouched, 32..255 a glyph's code (Text), GF_FILL | pen a fill. */
#define GF_FILL 0x1000
/* A FillPixelArray fill (cybergraphics) paints GF_CGX | 0xRRGGBB, not a pen. */
#define GF_CGX 0x10000000
#define GF_FLIP 0x40000   /* a COMPLEMENT fill flips this bit */

/* A window of w x h pixels with a font of cw x ch (baseline base) and its screen, no borders. */
void gf_open(int w, int h, int cw, int ch, int base);
void gf_close(void);
/* 1: AllocBitMap refuses the renderer's back bitmap (no memory for it), until gf_close. */
extern int gf_no_back;
struct Window *gf_window(void);
/* A true-colour screen (24 bits) with cybergraphics.library 41 and no pen left for ObtainBestPen, as a
 * screen that has used them up: a direct colour is then the renderer's VR_INK_RGB case (scratch pens, or
 * FillPixelArray for a fill). Call after gf_open, before vr_init. Off by default. */
void gf_truecolor(int on);
/* gf_truecolor(2): the same screen with its colour map used up (rig 2's 24-bit Workbench, W-T13): ObtainPen
 * finds no pen of its own, ObtainBestPen exact finds none and not exact returns the nearest entry (pen 7,
 * counted in gf_pens_held). A direct colour is then FillPixelArray's alone; a glyph's pen is the nearest. */
/* A full colour map of n entries 0xRRGGBB (W-T7): ObtainBestPen exact finds an entry of the colour or fails,
 * not exact the nearest entry by plain RGB distance; GetRGB32 reads it back; gf_pens_held counts the pens
 * obtained and not yet released. Call after gf_open (which clears it), before vr_init. */
void gf_palette(const unsigned long *rgb, int n);
extern int gf_pens_held;
/* A planar screen (W-T4): the window's bitmap is `depth` real bitplanes in chip RAM (TypeOfMem says so),
 * BMF_STANDARD, the window at screen pixel (left, top) on it, one unobscured ClipRect over it, and the font
 * 8 pixels wide with glyph data (so the renderer's planar fast paths run). RectFill, Text, ScrollRaster and
 * Draw write the planes under the RastPort's write mask; gf_pixel reads the pen back from them. Call after
 * gf_open, before vr_init. */
void gf_planar(int depth, int left, int top);
/* The window's borders (W-T10): BorderLeft/Top/Right/Bottom set, their pixels marked GF_BORDER in the canvas and
 * the last pen (every plane set) in the planes; gf_border_hits counts those changed since -- the renderer must
 * draw only inside the borders. Call after gf_open (and gf_planar), before vr_init. */
#define GF_BORDER 0x2000
void gf_borders(int l, int t, int r, int b);
int gf_border_hits(void);
/* Another window over part of this one (a second ClipRect): the layer must clip, nothing may write the
 * planes directly. */
void gf_cover(int on);
/* The raster line VBeamPos reports (WaitTOF records GF_TOF and sets it to 0, the top of the next frame). The
 * View starts the display at line 44, the screen at its top, not interlaced. */
extern long gf_beam;
struct TextFont *gf_font(void);
int gf_pixel(int x, int y);
/* The glyph at cell (col, row), ' ' for anything that is not a glyph. */
char gf_cell(int col, int row);
/* The pen under the top left pixel of cell (col, row): the last fill's, or the background pen of the Text that drew it. */
int gf_bgpen(int col, int row);
int gf_bg(int x, int y); /* the background pen under window pixel (x, y): a planar screen's pen there */
/* The window's rows as text, each cols long, joined with '|'. */
const char *gf_screen(void);
void gf_clear_log(void);
int gf_count(enum gf_kind k);
/* Calls of that kind since the last gf_clear_log. */
const gf_call *gf_find(enum gf_kind k, int nth);

#endif
