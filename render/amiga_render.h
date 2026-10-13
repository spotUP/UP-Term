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
#include "outline.h"
#include "unifont.h"
#include "emoji.h"

#define VR_EXACT_SLOTS 256  /* a power of two */
#define VR_EXACT_MAX 160
#define VR_IMG_SLOTS 512    /* image colours' pens, a power of two */
#define VR_IMG_MAX 256

/* An image's colours on this screen, 3,840 bytes: in every window's
 * vr_render they put a console.device STANDARD unit over DV4's 64 KB
 * (65,608 bytes on a clean 3.1 Workbench, 2026-10-11), though no unit
 * there had drawn an image; now allocated at a window's first image.
 * Palette screens: pens obtained for the images' colours, a hash on
 * 0xRRGGBB (key, pen), and the image's palette index -> pen (map).
 * True-colour screens: cybergraphics' WriteLUTPixelArray draws the
 * indices through ctab. */
struct vr_images {
    ULONG key[VR_IMG_SLOTS];
    UBYTE pen[VR_IMG_SLOTS];
    UBYTE map[256];
    ULONG ctab[256];
};

#define VR_BACK_RECTS 32
#define VR_STRIP 128   /* the cells block_run writes at a time */

/* The back bitmap (a friend of the window's, at least its size), its
 * RastPort, and the rectangles drawn into it since the last blit (pixels,
 * inclusive: x0 y0 x1 y1). */
struct vr_back {
    struct BitMap *bm;
    struct RastPort rp;
    WORD w, h;
    WORD n;
    WORD rect[VR_BACK_RECTS][4];
    /* A planar (AGA) window's frame (W-T4): planes of its own in any memory
     * (fast RAM where there is some), written by the CPU paths alone, and
     * copied into the screen's planes a row at a time (every plane of a row
     * before the next row) at frame end. Window pixel (x, y) is bit
     * phase + x of row y (phase: the window's screen x mod 32, so a copy
     * moves whole longwords). pmem 0 until a window's first planar frame. */
    UBYTE *pmem;
    ULONG psize;
    UBYTE *planes[8];
    LONG bpr;
    WORD depth, phase;
    UBYTE planar;         /* this pass composes in these planes, not in bm */
    UBYTE copy_mask;      /* the planes a copy writes: the pass's write mask before and after */
    UBYTE in_rows;        /* inside draw_rows: cur is the cells it is drawing */
    WORD nd;              /* the cells drawn into the planes since the last copy (draw_rows' x0 y0 x1 y1) */
    WORD dmg[VR_BACK_RECTS][4];
    WORD cur[4];
    /* block_run's cells (top and bottom pen; on a few-pen screen also the
     * dither's second pen and level, 0: solid) and one plane's two rows of bits */
    UBYTE tp[VR_STRIP], bp[VR_STRIP];
    UBYTE tb[VR_STRIP], bb[VR_STRIP], tl[VR_STRIP], bl[VR_STRIP];
    UBYTE strip[2][VR_STRIP + 1];
};

/* A screen of few pens (W-T7, 16 or fewer: a 4-plane Workbench): a palette
 * colour no pen shows closely fills its areas (backgrounds, blocks) with
 * two pens in an 8 x 8 ordered pattern (render/pen_match.h). Per palette
 * index the mix (pen b on lev / 64 of the pixels, pen a on the rest; lev 0
 * none) and which of a, b this window holds a pen for (1 a, 2 b: the
 * ReleasePen it owes); the pattern's rows per level; a chip-RAM template the
 * pattern fills in place are drawn from (BltTemplate). Made at the first
 * colour such a screen has no exact pen for, freed by vr_free. */
struct vr_dither {
    UBYTE a[256], b[256], lev[256], held[256];
    UBYTE pat[64][8];
    UBYTE *tmpl;
    ULONG tmpl_size;
};

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
    /* blinking: a blinking cell was drawn; the off phases; the microseconds
     * each rate's phase has lasted (vr_blink_tick) */
    BYTE has_blink, blink_slow_off, blink_fast_off;
    ULONG blink_slow_us, blink_fast_us;
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
    UBYTE view_stale;      /* live output reached the grid while the view was back: the screen is not the scrollback's */
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
    struct BitMap *chip_bm;       /* planes_ok's bitmap check, made once a bitmap (S1) */
    PLANEPTR chip_plane0;
    UBYTE chip_ok;
    /* the outline font for the cells the bitmap font cannot show (F1),
     * 0 for none; the owner opens and closes it (vr_set_outline) */
    struct vo_font *outline;
    ULONG n_outline;      /* cells drawn from it or from unifont */
    /* GNU Unifont's pages for the BMP the font and the outline font lack
     * (U2), 0 for none; the owner keeps it (vr_set_unifont) */
    struct uf_cache *unifont;
    /* colour emoji (U4): the painter on this window's target (an RTG screen
     * of 15 bits or more, cybergraphics' WriteLUTPixelArray) and the colour
     * pages the owner keeps (vr_set_emoji), emoji.store 0 for none */
    ce_painter emoji;
    vt_fallback fb;       /* the sources in their order (glyphmap.h), from the three above */
    /* selection, inclusive, rows as grid row + vt_lines_scrolled() at the
     * time (so it stays on its text while output scrolls) */
    BYTE sel;
    WORD sel_ax, sel_bx;
    LONG sel_ay, sel_by;
    /* images (sixel; draw_images): their colour tables (vr_images) made at
     * the first image this window draws, freed with its pens by vr_free;
     * the pixels turned into pens in img_buf. The last image's table is
     * kept (img_serial, img_bgrgb) */
    struct vr_images *img;
    WORD n_img_pens;
    UBYTE *img_buf;
    ULONG img_buf_size;
    struct Library *cgx;
    LONG img_serial;
    ULONG img_bgrgb;
    UBYTE img_planes;     /* the planes the last image's pens use */
    ULONG n_img_runs;     /* image runs drawn (the debug build's counter) */
    /* A synchronized frame composed off screen (vr_compose_begin, W-T3):
     * made at a window's first such frame (a window's own block stays
     * small: device/upcon_device.c's bound), freed by vr_free; the window's
     * RastPort while r->rp is the back one (0: not composing); the
     * engine's count of frames already shown (vt_sync_frames). */
    struct vr_back *back;
    struct RastPort *front_rp;
    long sync_seen;
    /* A planar composed pass (W-T4): the CPU paths write back->planes, and
     * anything only graphics.library can draw sets cpu_fail -- that pass is
     * then drawn in place. n_copies: the frames copied to the window. */
    UBYTE cpu, cpu_fail;
    ULONG n_copies;
    /* W-T7: a screen of 16 pens or fewer, and its dithers (0 until needed) */
    UBYTE few;
    struct vr_dither *dith;
} vr_render;

/* Default colours from RGB (0xRRGGBB; VR_KEEP leaves the screen's text /
 * background pen): XCON:'s DARK, FG and BG options. Call after vr_init,
 * then vr_redraw. */
#define VR_KEEP 0xFFFFFFFFUL
void vr_set_defaults(vr_render *r, ULONG fg_rgb, ULONG bg_rgb);

/* Blinking (SGR 5 slow, SGR 6 rapid, and a blinking cursor at the slow
 * rate): each phase, on or off, lasts this long. ECMA-48 (5th ed., 8.3.117):
 * 5 "slowly blinking (less then 150 per minute)", 6 "rapidly blinking (150
 * per minute or more)". Slow: 1 s a blink, 60 a minute; rapid: 300 ms, 200 a
 * minute. In time, not frames: the frame clock's interval follows what a
 * frame costs (pace.h, 20 to 160 ms), so a count of frames blinked at the
 * rate of the drawing (W-T2). */
#define VR_BLINK_SLOW_US 500000UL
#define VR_BLINK_FAST_US 150000UL
/* Blinking cells: call once per frame with the microseconds since the last
 * call; 1 while any blink on screen. */
int  vr_blink_tick(vr_render *r, ULONG us);
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
/* The Unifont page cache for glyphs neither font has (0: none); the caller
 * keeps it and redraws. Its cell follows vr_set_font. */
void vr_set_unifont(vr_render *r, struct uf_cache *c);
/* The colour emoji pages and scratch (0: none); the caller keeps them and
 * redraws. Drawn only where the screen can (vr_can_colour), else
 * Unifont's glyph. */
void vr_set_emoji(vr_render *r, struct ce_store *s);
/* 1 when this window's screen can show colour emoji (RTG, 15 bits or
 * more, cybergraphics): the owner then gives it a store. */
int vr_can_colour(const vr_render *r);
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
/* Around the engine's flush in a render pass (inside the mask): when the
 * program ended a synchronized-output frame (?2026) since the last pass,
 * the pass draws into an off-screen bitmap and vr_compose_end puts what it
 * drew into the window in one blit per rectangle, so the frame appears at
 * once instead of row by row as it is drawn (W-T3: UPDemo's effects swept
 * down the window over 12-27 ms on the 68060's RTG screen). A planar (AGA)
 * window not covered by another composes in planes of its own that the CPU
 * writes (fast RAM: no RectFill, no chip bus), copied into the screen's
 * planes row by row once the beam is off the rows copied (a blit would go
 * plane by plane: wrong colours while it runs); a frame with cells only
 * graphics.library draws is drawn in place (W-T4). 1 when the pass is
 * composed. */
int  vr_compose_begin(vr_render *r);
void vr_compose_end(vr_render *r);
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
/* Hide / show the cursor around a batch of output. */
void vr_cursor_off(vr_render *r);
void vr_cursor_on(vr_render *r);
/* Show the grid `lines` rows back into the scrollback (0 = live output);
 * clamps and redraws. Engine damage is not drawn while the view is back. */
void vr_set_view(vr_render *r, int lines);
/* Selection: set (a to b, grid coordinates now) or clear; redraws what
 * changed. vr_selection gives it back in grid coordinates of now. */
void vr_select(vr_render *r, int on, int ax, int ay, int bx, int by);
int  vr_selection(const vr_render *r, int *ax, int *ay, int *bx, int *by);

#endif
