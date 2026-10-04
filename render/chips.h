/* chips.h -- the custom chips' share of the renderer (ledger S1, CC1 and
 * CC2): the decisions and the arithmetic, apart from the OS calls, so the
 * host tests run them (tests/test_chips.c). amiga_render.c makes the calls.
 *
 * CC1, the blitter beside the CPU: on an unobscured planar window a scroll
 * is one BltBitMap copy (no ScrollRaster, no clear); the rows it vacates
 * are owed and the CPU painter fills them in the same pass. The CPU writes
 * a row without a WaitBlit when the only blit that may still run is that
 * copy and the row is outside it; the pass ends with one WaitBlit, so a
 * barrier (WaitForChar) answers only after every pixel is there.
 *
 * CC2, a hardware sprite as the cursor: no cursor erase and draw in the
 * bitplanes each flush. The sprite is the cell (or the underline's or
 * bar's strip) in the cursor colour, the glyph under a block cursor in the
 * background colour; anything it cannot show exactly falls back to the
 * cursor in the planes. */
#ifndef VT_CHIPS_H
#define VT_CHIPS_H

/* ---- CC1 ----------------------------------------------------------------- */

/* Text rows [top, bottom) up by n (down when n < 0). */
typedef struct vc_scroll {
    int copy;         /* rows the blit copies; 0: the whole region is vacated */
    int src, dst;     /* the copy's first source and destination row */
    int vac0, vac1;   /* the vacated rows [vac0, vac1): owed to the painter */
    int busy0, busy1; /* the rows the copy reads or writes; empty without a copy */
} vc_scroll;

void vc_scroll_plan(int top, int bottom, int n, vc_scroll *s);

/* What the blitter may still be doing for this renderer. */
#define VC_BLIT_IDLE 0   /* nothing: the last WaitBlit came after every blit */
#define VC_BLIT_SCROLL 1 /* only the scroll copy, on the pixel rows given */
#define VC_BLIT_ANY 2    /* a graphics call ran: anything, anywhere */

/* May the CPU write pixel rows [y0, y1) now without a WaitBlit first? */
int vc_cpu_may_write(int state, long busy_y0, long busy_y1, long y0, long y1);

/* A scroll through the blitter copy and the painter's fill: inside a render
 * pass (which pays the owed rows and ends with a WaitBlit), the bitplanes
 * writable (planar, chip RAM, unobscured, an 8-pixel font), no RGB
 * background (a planar screen has none). */
int vc_scroll_by_blit(int in_pass, int planes_ok, int rgb_bg);

/* ---- CC2 ----------------------------------------------------------------- */

/* The cursor inside its cell (DECSCUSR): a block, an underline (the two
 * bottom rows: styles 3, 4) or a bar (the two left columns: 5, 6). cw is
 * the cell's width on screen (twice the font's on a double-width row). */
void vc_cursor_rect(int style, int cw, int ch, int *dx, int *dy, int *w, int *h);

/* A cell of cw x ch screen pixels as sprite pixels and lines. screen_ns and
 * sprite_ns are the pixels' widths in nanoseconds (lores 140, hires 70,
 * super hires 35); a laced screen shows a sprite line over two rows. */
typedef struct vc_sprite_geom {
    int sw, sh;          /* the cell: sprite pixels, sprite lines */
    int screen_ns, sprite_ns;
    int lace;
} vc_sprite_geom;

#define VC_SPRITE_W 16   /* the image's width (the sprite engine pads wider sprites) */
#define VC_SPRITE_H 64   /* the most lines an image has */

/* 1 when the cell is a whole number of sprite pixels and lines and fits
 * the image. */
int vc_sprite_geom_of(int screen_ns, int sprite_ns, int lace, int cw, int ch, vc_sprite_geom *g);

/* MoveSprite's position for screen pixel (x, y) of the ViewPort: since V39
 * graphics.library takes the ViewPort's own resolution and converts (seen
 * on the stock rig 2026-10-04: lores units, as RKM ch. 28 describes for the
 * hardware, put the cursor at half its x on a hires screen). 0 when the
 * cell's corner falls between two lores positions (OCS/ECS sprites move in
 * lores steps) or on an odd laced line: the planes draw the cursor there. */
#define VC_POS_NS 140
int vc_sprite_pos(const vc_sprite_geom *g, long x, long y, long *sx, long *sy);

/* The image, VC_SPRITE_W pixels a line (bit 15 the left pixel), planes a
 * (colour bit 0) and b (bit 1), g->sh lines: the rectangle (rx, ry, rw, rh
 * in screen pixels inside the cell) in colour 1, and inside it the glyph's
 * set pixels in colour 2 (glyph: one byte a screen row, bit 7 the left
 * pixel, 8 pixels wide; 0 for none). 0 when the glyph cannot be shown --
 * the sprite's pixels or lines are coarser than the screen's and a set
 * pixel lies in the rectangle: then the planes draw the cursor. */
int vc_sprite_image(const vc_sprite_geom *g, int rx, int ry, int rw, int rh, const unsigned char *glyph,
                    unsigned short *a, unsigned short *b);

/* The colour register of the sprite's colour 1 (colour 2 is the next):
 * base + 4 * (num / 2) + 1, the even sprites' base for an even num (16 but
 * on AGA when moved, VTAG_SPEVEN_BASE_GET). -1 when those registers are
 * bitplane colours of a screen `depth` planes deep (they would recolour
 * text) or beyond its colour map's `count` entries. */
int vc_sprite_colour_reg(int num, int even_base, int odd_base, int depth, int count);

/* Why the cursor is not a sprite, or VC_CUR_SPRITE when it is. In the
 * order the checks run: the cheap and permanent ones first. */
#define VC_CUR_SPRITE 0
#define VC_CUR_RTG 1       /* not a chip-RAM planar display */
#define VC_CUR_HIDDEN 2    /* covered, inactive, or its screen not in front */
#define VC_CUR_GEOM 3      /* the cell is not whole sprite pixels / lines */
#define VC_CUR_POS 4       /* the cell's corner is between two positions */
#define VC_CUR_CELL 5      /* a double-size row, a glyph not from the bitmap font */
#define VC_CUR_IMAGE 6     /* the glyph cannot be shown at the sprite's pixels */
#define VC_CUR_COLOURS 7   /* the sprite's colours are text colours */
#define VC_CUR_NONE 8      /* no sprite free */
typedef struct vc_cursor_env {
    int native, visible, geom, pos, plain_cell, image, colours, have_sprite;
} vc_cursor_env;
int vc_cursor_choice(const vc_cursor_env *e);

#endif
