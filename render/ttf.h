/* render/ttf: UP-Term's own TrueType reader and rasterizer, for the outline
 * glyph worker (render/outline.c) in place of ttf.library, whose 0.8.5 cannot
 * reach code points past U+FFFF (render/glyphcode.h). Plan
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

#endif
