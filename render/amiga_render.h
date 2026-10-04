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
#include <graphics/sprite.h>
#include "../engine/vtengine.h"
#include "glyphmap.h"
#include "outline.h"

#define VR_EXACT_SLOTS 256  /* a power of two */
#define VR_EXACT_MAX 160
#define VR_IMG_SLOTS 512    /* image colours' pens, a power of two */
#define VR_IMG_MAX 256

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
    BYTE bell_flash;      /* the visual bell's reversed frame */
    /* Speed on planar screens (ledger S1, after the ROM console and CCON):
     * mask is the planes the pens drawn so far occupy -- every other plane
     * of the text area holds zeros, so a render pass draws and scrolls
     * only these (rp->Mask; vr_mask_begin / vr_mask_end around the pass).
     * It widens with each new pen and starts again at a full redraw.
     * blank: the text area is known to be all default background, so a
     * scroll moves nothing and is skipped. */
    UBYTE mask, mask_on, planar, blank;
    UBYTE cursor_flip;    /* the drawn cursor is an inversion in these planes (0: it is not) */
    UBYTE full_pass;      /* inside a draw of the whole grid on a planar screen: `seen` becomes the mask */
    UBYTE in_pass, bs_valid, bs_ok; /* a render pass is on; the default blank's style is known for it, and plain */
    UBYTE seen;           /* the pens drawn since that pass began */
    ULONG bg_ink;         /* the ink of a default blank cell: what `blank` and the skipped fills mean */
    ULONG pad_ink;        /* what the strips of the text area beside the grid hold */
    UBYTE was_blank;      /* inside draw_rows: blank when it began (its blank runs need no fill) */
    WORD cursor_x, cursor_y;
    BYTE cursor_drawn;
    BYTE cursor_colorful;  /* the cursor cell was filled with the profile colour */
    ULONG cursor_ink;      /* the profile's cursor colour (a pen, or VR_INK_RGB on
                              * true colour), or VR_KEEP: the inverted cell */
    ULONG sel_ink[2];      /* the profile's selection foreground and background: a
                            * pen, or VR_INK_RGB on true colour, else VR_KEEP --
                            * which keeps the swapped colours of the cell */
    /* The pens the two setters obtained for those inks themselves, -1 none:
     * released by the setter that replaces them or by vr_free, nowhere
     * else. An ink can be a pen another table owns (an exact true-colour
     * pen belongs to exact_pen[]), so the ink alone does not say whose it is. */
    LONG cursor_pen;
    LONG sel_pen[2];
    BYTE hidden;          /* nothing is drawn: the window is too small, or off */
    BYTE off;             /* a tab that is not the active one: the grid goes on, nothing drawn */
    WORD inset_top;       /* pixels above the text kept free (the tab bar) */
    /* scrollback view: screen row y shows grid row y - view (0 = live) */
    WORD view;
    WORD jump;             /* jump scroll: screen row s shows grid row s + jump (vr_scroll) */
    WORD jump_step;        /* the spare rows the next jump leaves; doubles while scrolls keep coming */
    UBYTE scrolled_pass;   /* this render pass scrolled the whole screen */
    /* Amiga layout requests (CSI t / u / x / y), -1 = automatic:
     * text rows, text columns, left and top offset in pixels */
    WORD lay_rows, lay_cols, lay_x, lay_y;
    /* planar fast path (retro32-term's technique): the font's 8-pixel
     * glyphs, one byte per row, extracted once; NULL when the font is not
     * 8 pixels wide */
    UBYTE *glyphs;
    /* profile counters, read by the debug build */
    ULONG n_direct, n_text;
    /* CC1 (render/chips.h): what the blitter may still be doing for this
     * renderer (VC_BLIT_*), and on which pixel rows of the window when it
     * is the scroll copy; the rows a blitter scroll vacated, owed to the
     * painter -- screen text rows [owe0, owe1) in the pen owe_pen. */
    UBYTE bp, owe_pen;
    WORD bp_y0, bp_y1;
    WORD owe0, owe1;
    ULONG n_blit_scroll, n_owed_fill; /* the debug build's counters: scrolls by blit, owed rows filled */
    /* CC2: the cursor as a hardware sprite (vr_cursor_on). spr_num is the
     * sprite's number, -1 none; spr_es the two images (the one shown and
     * the next), spr_cur which is shown, spr_key what it shows; spr_vis
     * the sprite is on screen as the cursor; spr_none no sprite was free
     * (not asked again until the next screen); spr_reg the colour
     * register of its colour 1, spr_rgb the colours set there, spr_old
     * what they were (given back on release); spr_env the last
     * vc_cursor_choice, for the watch (vr_cursor_watch). */
    BYTE spr_num, spr_cur, spr_vis, spr_none;
    struct ExtSprite *spr_es[2];
    ULONG spr_key[2];
    WORD spr_w;            /* the sprite engine's width the images were made for */
    WORD spr_reg;
    ULONG spr_rgb[2], spr_old[2][3];
    WORD spr_x, spr_y;     /* its MoveSprite position */
    BYTE spr_env;
    WORD spr_sns;          /* the screen's pixel in ns, 0: no sprite cursor here (vr_init) */
    BYTE spr_lace;         /* the screen is laced */
    WORD spr_ns;           /* the sprite pixel the image was made for, in ns */
    ULONG n_spr_moves, n_spr_images, n_plane_cursor; /* the debug build's counters */
    struct BitMap *chip_bm;       /* planes_ok's bitmap check, made once a bitmap (S1) */
    PLANEPTR chip_plane0;
    UBYTE chip_ok;
    /* the outline font for the cells the bitmap font cannot show (F1),
     * 0 for none; the owner opens and closes it (vr_set_outline) */
    struct vo_font *outline;
    ULONG n_outline;      /* cells drawn from it */
    /* selection, inclusive, rows as grid row + vt_lines_scrolled() at the
     * time (so it stays on its text while output scrolls) */
    BYTE sel;
    WORD sel_ax, sel_bx;
    LONG sel_ay, sel_by;
    /* images (sixel; draw_images). Palette screens: pens obtained for the
     * images' colours, a hash on 0xRRGGBB, released by vr_free; the pixels
     * turned into pens in img_buf. True-colour screens: cybergraphics'
     * WriteLUTPixelArray draws the indices through img_ctab. The last
     * image's table is kept (img_serial, img_bgrgb) */
    ULONG img_key[VR_IMG_SLOTS];
    UBYTE img_pen[VR_IMG_SLOTS];
    WORD n_img_pens;
    UBYTE *img_buf;
    ULONG img_buf_size;
    struct Library *cgx;
    LONG img_serial;
    ULONG img_bgrgb;
    UBYTE img_planes;     /* the planes the last image's pens use */
    UBYTE img_map[256];
    ULONG img_ctab[256];
    ULONG n_img_runs;     /* image runs drawn (the debug build's counter) */
} vr_render;

/* Default colours from RGB (0xRRGGBB; VR_KEEP leaves the screen's text /
 * background pen): XCON:'s DARK, FG and BG options. Call after vr_init,
 * then vr_redraw. */
#define VR_KEEP 0xFFFFFFFFUL
void vr_set_defaults(vr_render *r, ULONG fg_rgb, ULONG bg_rgb);

/* Blinking cells: call once per frame; 1 while any blink on screen. */
int  vr_blink_tick(vr_render *r);
/* The visual bell: every cell reversed for one frame; call once per frame,
 * 1 when the frame just ended (a re-render is due). The terminal's own
 * reverse-video mode is not touched. */
void vr_bell_flash(vr_render *r);
int  vr_flash_tick(vr_render *r);
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

/* Another fixed-width font for the text: cell size, baseline, the
 * rastport's font and the planar glyphs follow; the caller resizes the
 * grid (vr_layout) and redraws. */
void vr_set_font(vr_render *r, struct TextFont *font);
/* The outline font for glyphs the bitmap font lacks (0: none); the caller
 * keeps it open and redraws. Its cell follows vr_set_font. */
void vr_set_outline(vr_render *r, struct vo_font *f);
/* A tab's renderer: off (another tab is shown) draws nothing at all; the
 * caller redraws when it is on again. */
void vr_set_off(vr_render *r, int off);
/* Pixels above the text kept free (the tab bar); the caller lays out and
 * redraws. */
void vr_set_inset(vr_render *r, WORD top);

void vr_init(vr_render *r, struct Window *win, struct TextFont *font, vt_term *t,
             enum vt_font_enc enc);
void vr_free(vr_render *r);
/* Recompute the text area from the window size; returns 1 when cols/rows
 * changed (the caller then calls vt_resize and vr_redraw). */
int  vr_layout(vr_render *r);
/* Around a render pass (the cursor off, the engine's flush, the cursor
 * on): drawing limited to the planes in use. Outside it everything draws
 * at full depth. */
void vr_mask_begin(vr_render *r);
void vr_mask_end(vr_render *r);
void vr_redraw(vr_render *r);
void vr_damage(vr_render *r, int x0, int y0, int x1, int y1);
/* Jump scroll's end: the screen exactly as the grid again (output stopped). */
void vr_settle(vr_render *r);
void vr_scroll(vr_render *r, int top, int bottom, int n);
/* The cursor's colour (0xRRGGBB) for a block cursor: the cell is filled
 * with it and the glyph drawn in the background colour, as xterm does with
 * its cursor colour. VR_KEEP keeps the inverted cell (the default).
 * Underline and bar shapes stay inverted. */
void vr_set_cursor_color(vr_render *r, ULONG rgb);
/* The profile's selection foreground and background, 0xRRGGBB. VR_KEEP for
 * either (both by default) leaves that half of the swap in place, so the
 * plain look is a selected cell with its two colours exchanged. */
void vr_set_selection_colors(vr_render *r, ULONG fg_rgb, ULONG bg_rgb);
/* Hide / show the cursor around a batch of output. A sprite cursor
 * (CC2) is not in the planes: off leaves it where it is, on moves it --
 * vr_cursor_hide takes it off the screen (the blink, a tab that goes). */
void vr_cursor_off(vr_render *r);
void vr_cursor_on(vr_render *r);
void vr_cursor_hide(vr_render *r);
/* While the cursor is a sprite: 1, and the frame clock keeps coming --
 * the sprite is above every window and screen, so a window moved over
 * this one, another window made active or another screen brought to the
 * front must take it back to the planes; this looks each frame. */
int  vr_cursor_watch(vr_render *r);
/* Show the grid `lines` rows back into the scrollback (0 = live output);
 * clamps and redraws. Engine damage is not drawn while the view is back. */
void vr_set_view(vr_render *r, int lines);
/* Selection: set (a to b, grid coordinates now) or clear; redraws what
 * changed. vr_selection gives it back in grid coordinates of now. */
void vr_select(vr_render *r, int on, int ax, int ay, int bx, int by);
int  vr_selection(const vr_render *r, int *ax, int *ay, int *bx, int *by);

#endif
