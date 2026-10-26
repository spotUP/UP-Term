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

/* pixels s takes */
int mtc_width(mtc *c, const char *s);
/* pixels the first len bytes of s take (whole code points) */
int mtc_width_n(mtc *c, const char *s, int len);
/* bytes of the first len bytes of s, whole code points, that fit maxw pixels (a wrapped line's length) */
int mtc_prefix(mtc *c, const char *s, int len, int maxw);
/* s cut to fit maxw pixels into out (n bytes): s whole when it fits, else the longest head that with "..."
 * fits ("." when even that is too wide for more; "" when nothing fits). Returns the pixel width of out. */
int mtc_fit(mtc *c, const char *s, int maxw, char *out, int n);

/* ---- TrueType scaling: px pixels an em, advances rounded to whole pixels ---- */
int mtc_scale(int units, int upem, int px);

typedef struct mtc_ttf {
    ttf_font *f;
    int px, upem, asc, line; /* line: the height of a coverage box, baseline row asc */
    long scale;              /* pixels per font unit, 16.16 */
    int base;
    void *(*alloc)(unsigned long);
    void (*release)(void *);
} mtc_ttf;

int mtc_ttf_init(mtc_ttf *t, ttf_font *f, int px);
/* the source over t: its alloc and release are the caller's (AllocVec on the Amiga, malloc on the host) */
void mtc_ttf_source(mtc_src *s, mtc_ttf *t, void *(*alloc)(unsigned long), void (*release)(void *));

#endif
