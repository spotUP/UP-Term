/* render/ttf: UP-Term's own TrueType reader and rasterizer, for the outline
 * glyph worker (render/outline.c) in place of ttf.library, whose 0.8.5 cannot
 * reach code points past U+FFFF (it ignores OT_GlyphCode_32). Plan
 * thoughts/shared/plans/2026-10-22-handler-rasterizer.md.
 *
 * The font is read through callbacks, not loaded whole: the Symbols Nerd Font
 * is 2.6 MB. Kept in memory per font: loca, hmtx and the chosen cmap
 * subtable (format 12, else format 4); glyf is read glyph by glyph. Simple
 * and composite glyphs, no hinting (instructions are skipped).
 *
 * The rasterizer adds each outline edge's signed area to an accumulator and
 * sums each row (the font-rs method), in fixed point only: a 68020 without
 * an FPU stays usable. Coverage is 0..255 per pixel, non-zero winding.
 * Portable C89, no OS calls (tests/test_ttf.c).
 */
#ifndef TTF_H
#define TTF_H

/* bytes read at off into buf, len wanted; fewer (or < 0) is an error */
typedef long (*ttf_read_fn)(void *ctx, unsigned long off, void *buf, long len);
/* size bytes, cleared; 0 when there is no memory */
typedef void *(*ttf_alloc_fn)(void *ctx, unsigned long size);
typedef void (*ttf_free_fn)(void *ctx, void *p);

typedef struct ttf_io {
    ttf_read_fn read;
    ttf_alloc_fn alloc;
    ttf_free_fn free;
    void *ctx;
} ttf_io;

typedef struct ttf_font ttf_font;

enum {
    TTF_OK = 0,
    TTF_NOT_TTF,     /* not a TrueType file (CFF "OTTO", a collection, or no sfnt header) */
    TTF_TRUNCATED,   /* a read came back short */
    TTF_NO_TABLE,    /* head, maxp, hhea, hmtx, loca, glyf or cmap is missing */
    TTF_BAD_TABLE,   /* a table's contents point outside it */
    TTF_NO_CMAP,     /* no Unicode cmap of format 4 or 12 */
    TTF_NO_MEMORY,
    TTF_TOO_BIG      /* render: the grid or the scale is past the fixed-point range */
};

/* *out is the font on TTF_OK (io is copied), else 0 */
int ttf_open(const ttf_io *io, ttf_font **out);
void ttf_close(ttf_font *f);

/* the glyph for code point cp; 0 (.notdef) when the font has none */
unsigned ttf_glyph_index(const ttf_font *f, unsigned long cp);
/* gid's advance width in font units */
int ttf_advance(const ttf_font *f, unsigned gid);
int ttf_units_per_em(const ttf_font *f);
/* the line's top and bottom (hhea ascender, descender; the latter negative) in font units */
int ttf_ascender(const ttf_font *f);
int ttf_descender(const ttf_font *f);
int ttf_num_glyphs(const ttf_font *f);

/* gid's outline into cov, w x h bytes (row 0 the top), which the caller
 * clears: 0..255 per pixel, added to what is there and clamped. xscale and
 * yscale: pixels per font unit in 16.16. The glyph's origin is ox pixels
 * from the left and on row baseline (the row below the origin is
 * baseline). 1 when a pixel was covered, 0 for a blank or empty glyph, a
 * negative TTF_* on an error. w and h are at most 512. */
int ttf_render(ttf_font *f, unsigned gid, long xscale, long yscale, int ox, int baseline,
               unsigned char *cov, int w, int h);

/* coverage to a 1-bit mask (bpr bytes a row, most significant bit the left
 * pixel): a level of 128 or more sets the bit; mask is written whole */
void ttf_threshold(const unsigned char *cov, int w, int h, unsigned char *mask, int bpr);

/* ttf_threshold at another level: a coverage of level or more sets the bit
 * (the antialiased draw's shade passes, plan R4.3) */
void ttf_threshold_at(const unsigned char *cov, int w, int h, unsigned char *mask, int bpr, int level);

/* A box units_w x upem font units drawn into w x h pixels whose pixels are
 * rx x ry ticks wide and tall (DisplayInfo Resolution; 1 x 2 a PAL hires
 * pixel), its shape kept on the screen: the scale is the smaller of the two
 * that fit (plan 2026-10-22-handler-rasterizer.md R6.1). xscale, yscale as
 * ttf_render takes them; bw x bh the box in pixels (one of them w or h);
 * baseline the row for ttf_render: the box centred down the cell, base (the
 * cell's baseline with the em filling h rows) scaled into it. No float;
 * w, h at most 512, units_w at most 2 x 65535, any tick count. */
typedef struct ttf_fit_out {
    long xscale, yscale;
    int bw, bh, baseline;
} ttf_fit_out;
void ttf_fit(long units_w, int upem, int w, int h, int base, int rx, int ry, ttf_fit_out *out);

/* Where glyph gid goes in a span w pixels wide and h rows tall, the
 * bitmap font's baseline on row base, pixels rx x ry ticks (plan
 * 2026-10-22-handler-rasterizer.md R6.1, R6.3). TTF_SHAPE: the glyph's own
 * advance x one em fitted into the span with its shape kept (ttf_fit) and
 * centred across, the font's line centred down it (base only when the font
 * has no usable line). TTF_FILL (box drawing, blocks, powerline separators): the
 * advance stretched across the span and the font's line (hhea ascender to
 * descender) down the h rows, its baseline from the font, not from base;
 * base only when the font has no usable line. xscale, yscale, baseline as
 * ttf_render takes them; ox its origin's column. */
enum { TTF_SHAPE, TTF_FILL };
typedef struct ttf_place_out {
    long xscale, yscale;
    int ox, baseline;
} ttf_place_out;
void ttf_place(const ttf_font *f, unsigned gid, int how, int w, int h, int base, int rx, int ry,
               ttf_place_out *out);

/* cov one pixel bolder into out (both w x h bytes): each pixel the larger
 * of itself and its left neighbour, as Text() bold draws the glyph again
 * one pixel right */
void ttf_embolden(const unsigned char *cov, int w, int h, unsigned char *out);

#endif
