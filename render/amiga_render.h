/* The Amiga renderer: draws a vt_term into an Intuition window's RastPort.
 *
 * The engine calls vr_damage / vr_scroll through its callbacks; this file
 * turns cells into Text() runs, RectFill() backgrounds and drawn line and
 * block glyphs (render/glyphmap), and keeps the cursor. It goes through the
 * RastPort only, so it works on every screen (planar, AGA, RTG) and under
 * covering windows; the planar fast path of retro32-term's term-engine.c
 * is a later speed-up behind the same functions (ledger R1).
 */
#ifndef AMIGA_RENDER_H
#define AMIGA_RENDER_H

#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/text.h>
#include "../engine/vtengine.h"
#include "glyphmap.h"

#define VR_EXACT_SLOTS 256  /* a power of two */
#define VR_EXACT_MAX 160

typedef struct vr_render {
    struct Window *win;
    struct RastPort *rp;
    struct TextFont *font;
    struct ColorMap *cm;
    vt_term *t;
    enum vt_font_enc enc;
    WORD ox, oy;          /* top left of the text area in the window */
    WORD cw, ch, base;    /* cell size, baseline */
    WORD cols, rows;      /* what fits in the window now */
    UBYTE pen_default_fg, pen_default_bg;
    UBYTE pens[256];      /* palette index -> screen pen (xterm, pcansi) */
    UBYTE have[256];      /* 1: pens[i] obtained, 2: failed */
    LONG obtained[256];   /* ObtainBestPen results to release */
    UBYTE *rgb_pens;      /* 32768 rgb555 -> pen+1 cache, 0 = unknown (palette screens) */
    LONG rgb_obtained[64];/* pens obtained for direct colours, released in vr_free */
    WORD n_rgb;
    /* true-colour screen: direct colours through two exclusive scratch
     * pens, A and B, and the ink each holds now (0: none yet) */
    BYTE truecolor;
    LONG scratch[2];
    ULONG scratch_ink[2];
    /* true-colour screen: pens obtained for exact direct colours, a hash
     * on 0xRRGGBB (key 0 = empty, else 0x01RRGGBB); at most VR_EXACT_MAX,
     * so other programs keep pens too */
    ULONG exact_key[VR_EXACT_SLOTS];
    UBYTE exact_pen[VR_EXACT_SLOTS];
    WORD n_exact;
    LONG dflt_obtained[2];  /* the pens vr_set_defaults obtained, -1 none */
    /* fonts for SGR 11-19 (1-9) and Fraktur, SGR 20 (10); 0 = the primary */
    struct TextFont *alt_font[11];
    /* blinking: a blinking cell was drawn; frames counted; the off phases */
    BYTE has_blink, blink_slow_off, blink_fast_off;
    ULONG blink_frames;
    WORD cursor_x, cursor_y;
    BYTE cursor_drawn;
    BYTE hidden;          /* the window is too small to draw into */
    /* scrollback view: screen row y shows grid row y - view (0 = live) */
    WORD view;
    /* Amiga layout requests (CSI t / u / x / y), -1 = automatic:
     * text rows, text columns, left and top offset in pixels */
    WORD lay_rows, lay_cols, lay_x, lay_y;
    /* planar fast path (retro32-term's technique): the font's 8-pixel
     * glyphs, one byte per row, extracted once; NULL when the font is not
     * 8 pixels wide */
    UBYTE *glyphs;
    /* profile counters, read by the debug build */
    ULONG n_direct, n_text;
    /* selection, inclusive, rows as grid row + vt_lines_scrolled() at the
     * time (so it stays on its text while output scrolls) */
    BYTE sel;
    WORD sel_ax, sel_bx;
    LONG sel_ay, sel_by;
} vr_render;

/* Default colours from RGB (0xRRGGBB; VR_KEEP leaves the screen's text /
 * background pen): XCON:'s DARK, FG and BG options. Call after vr_init,
 * then vr_redraw. */
#define VR_KEEP 0xFFFFFFFFUL
void vr_set_defaults(vr_render *r, ULONG fg_rgb, ULONG bg_rgb);

/* Blinking cells: call once per frame; 1 while any blink on screen. */
int  vr_blink_tick(vr_render *r);
/* The cursor blinks (?12 or a blinking DECSCUSR shape). */
int  vr_cursor_blinks(vr_render *r);
/* The font SGR 11-19 (n 1-9) or 20 (n 10, Fraktur) draws with; the caller
 * keeps it open. Ignored unless its cells are the primary font's size. */
void vr_set_alt_font(vr_render *r, int n, struct TextFont *font);

/* The colour a screen pen shows now, 0xRRGGBB. */
ULONG vr_pen_rgb(vr_render *r, UBYTE pen);
/* The palette or the default colours changed (OSC 4 / 10-12): the pens
 * chosen for palette entries are given back and chosen again. */
void vr_palette_changed(vr_render *r);

void vr_init(vr_render *r, struct Window *win, struct TextFont *font, vt_term *t,
             enum vt_font_enc enc);
void vr_free(vr_render *r);
/* Recompute the text area from the window size; returns 1 when cols/rows
 * changed (the caller then calls vt_resize and vr_redraw). */
int  vr_layout(vr_render *r);
void vr_redraw(vr_render *r);
void vr_damage(vr_render *r, int x0, int y0, int x1, int y1);
void vr_scroll(vr_render *r, int top, int bottom, int n);
/* Hide / show the cursor around a batch of output. */
void vr_cursor_off(vr_render *r);
void vr_cursor_on(vr_render *r);
/* Cell under a window pixel position; returns 0 outside the text area. */
int  vr_cell_at(const vr_render *r, WORD mx, WORD my, int *x, int *y);
/* Show the grid `lines` rows back into the scrollback (0 = live output);
 * clamps and redraws. Engine damage is not drawn while the view is back. */
void vr_set_view(vr_render *r, int lines);
/* Selection: set (a to b, grid coordinates now) or clear; redraws what
 * changed. vr_selection gives it back in grid coordinates of now. */
void vr_select(vr_render *r, int on, int ax, int ay, int bx, int by);
int  vr_selection(const vr_render *r, int *ax, int *ay, int *bx, int *by);

#endif
