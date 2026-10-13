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

enum gf_kind { GF_TEXT, GF_RECT, GF_SCROLL, GF_BLT, GF_MOVE, GF_CGXFILL, GF_SETRGB, GF_ALLOC, GF_OTHER };

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
struct TextFont *gf_font(void);
int gf_pixel(int x, int y);
/* The glyph at cell (col, row), ' ' for anything that is not a glyph. */
char gf_cell(int col, int row);
/* The pen under the top left pixel of cell (col, row): the last fill's, or the background pen of the Text that drew it. */
int gf_bgpen(int col, int row);
/* The window's rows as text, each cols long, joined with '|'. */
const char *gf_screen(void);
void gf_clear_log(void);
int gf_count(enum gf_kind k);
/* Calls of that kind since the last gf_clear_log. */
const gf_call *gf_find(enum gf_kind k, int nth);

#endif
