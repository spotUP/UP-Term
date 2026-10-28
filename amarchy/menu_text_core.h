/* menu_text_core -- the pure layout of the Amarchy menu's text (plan 2026-10-26-amarchy-gui-aa-font.md, A2):
 * the width of a string, the ellipsis that fits one into a pixel width, and the per-code-point glyph cache,
 * apart from AmigaOS so the host tests them (tests/test_menu_text.c). amarchy/menu_text.c adds the Amiga side:
 * the font files, the bitmap font and the drawing.
 *
 * Text is UTF-8. A glyph source answers an advance (pixels) and, for TrueType, one coverage box per code point;
 * both are cached for the open size, so a width call after the first renders nothing. */
#ifndef MENU_TEXT_CORE_H
#define MENU_TEXT_CORE_H

#include "../render/ttf.h"

#define MTC_SLOTS 512 /* direct-mapped by code point */

/* one cached glyph: cov is w x h bytes (columns trimmed to the ink), drawn with its left edge at the pen x +
 * left and its top h-rows-line at the line top; w 0 for a blank glyph */
typedef struct mtc_glyph {
    unsigned long cp;
    int have_adv, have_cov;
    int adv;        /* pixels to the next glyph */
    int left, w, h; /* the coverage box */
    unsigned char *cov;
} mtc_glyph;

typedef struct mtc_src {
    void *u;
    int (*advance)(void *u, unsigned long cp);                      /* pixels */
    int (*render)(void *u, unsigned long cp, mtc_glyph *g);         /* fills left, w, h, cov (malloc'd); 0 = blank */
    void *(*alloc)(unsigned long n);
    void (*release)(void *p);
} mtc_src;

typedef struct mtc {
    mtc_src src;
    mtc_glyph slot[MTC_SLOTS];
    unsigned long advances; /* advance lookups answered by the source (cache misses) */
    unsigned long renders;  /* glyphs the source rendered */
    unsigned long hits;     /* lookups answered from the cache */
} mtc;

void mtc_init(mtc *c, const mtc_src *src);
void mtc_free(mtc *c);

/* the next code point of UTF-8 s (a malformed byte is itself); *n bytes used, at least 1 unless *s is 0 */
unsigned long mtc_next(const char *s, int *n);

int mtc_advance(mtc *c, unsigned long cp);
/* the glyph with its coverage (cached); 0 when the source has no coverage (bitmap) */
const mtc_glyph *mtc_glyph_of(mtc *c, unsigned long cp);

/* tmp (bw x bh coverage bytes, alloc'd; freed here) cut to its inked columns into g; pad = the columns of tmp
 * left of the pen x. 0 for a blank glyph. */
int mtc_cov_trim(unsigned char *tmp, int bw, int bh, int pad, mtc_glyph *g, void *(*alloc)(unsigned long),
                 void (*rel)(void *));
/* A bitmap font's glyph as Text() drew it into one plane (bpr bytes a row, bit 7 leftmost; the pen x pad
 * pixels in) as coverage 255 per set bit, trimmed as mtc_cov_trim: a true-colour screen then draws bitmap text
 * in the theme's own colours through the same coverage path as TrueType, not on the nearest pens. */
int mtc_cov_from_plane(const unsigned char *plane, int bpr, int bw, int bh, int pad, mtc_glyph *g,
                       void *(*alloc)(unsigned long), void (*rel)(void *));

/* pixels s takes */
int mtc_width(mtc *c, const char *s);
/* pixels the first len bytes of s take (whole code points) */
int mtc_width_n(mtc *c, const char *s, int len);
/* bytes of the first len bytes of s, whole code points, that fit maxw pixels (a wrapped line's length) */
int mtc_prefix(mtc *c, const char *s, int len, int maxw);
/* s cut to fit maxw pixels into out (n bytes): s whole when it fits, else the longest head that with "..."
 * fits ("." when even that is too wide for more; "" when nothing fits). Returns the pixel width of out. */
int mtc_fit(mtc *c, const char *s, int maxw, char *out, int n);
/* Like mtc_fit, but s[keep..] (a row's ": Value") is never cut while it fits: the head s[0..keep) is cut to
 * "..." instead. When the tail alone is wider than maxw it is cut as mtc_fit does. */
int mtc_fit_tail(mtc *c, const char *s, int keep, int maxw, char *out, int n);

/* ---- TrueType scaling: px pixels an em, advances rounded to whole pixels ---- */
int mtc_scale(int units, int upem, int px);

typedef struct mtc_ttf {
    ttf_font *f;
    int px, py, upem, asc, line; /* line: the height of a coverage box, baseline row asc */
    long scale, yscale;      /* pixels per font unit across and down, 16.16 */
    int base;
    void *(*alloc)(unsigned long);
    void (*release)(void *);
} mtc_ttf;

int mtc_ttf_init(mtc_ttf *t, ttf_font *f, int px, int rx, int ry);
/* the source over t: its alloc and release are the caller's (AllocVec on the Amiga, malloc on the host) */
void mtc_ttf_source(mtc_src *s, mtc_ttf *t, void *(*alloc)(unsigned long), void (*release)(void *));

#endif
