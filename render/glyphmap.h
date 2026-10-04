/* Unicode cell -> what an Amiga font can show. Portable C89, host-tested.
 *
 * Amiga bitmap fonts are 8-bit: Latin-1 for the system fonts (topaz,
 * xen...), CP437 for the IBM fonts BBS users load. Box drawing and block
 * elements are not in the Latin-1 fonts at all, so the renderer draws them
 * from line and rectangle descriptions instead, which also keeps them
 * joined at any font size. */
#ifndef GLYPHMAP_H
#define GLYPHMAP_H
#include "../engine/vtengine.h"

enum vt_font_enc { VT_ENC_LATIN1 = 0, VT_ENC_CP437 = 1 };

enum vt_glyph_kind {
    VT_GLYPH_FONT = 0,  /* draw font character `code` */
    VT_GLYPH_BOX,       /* draw lines: `code` is the arm byte (see glyph_tables.inc) */
    VT_GLYPH_BLOCK,     /* draw rectangles / shade: `code` is the block byte */
    VT_GLYPH_DIAGONAL,  /* `code` 1 = /, 2 = \\, 3 = X */
    VT_GLYPH_DIAMOND,   /* the DEC graphics diamond */
    VT_GLYPH_HLINE,     /* a light horizontal line `code` eighths down (DEC scan lines) */
    VT_GLYPH_MISSING    /* a replacement box (a character beyond the BMP no font has);
                         * the renderer sets `code` to the cells it spans, 1 or 2 */
};

typedef struct vt_glyph {
    vt_u8 kind;
    vt_u8 code;
} vt_glyph;

/* The replacement for a code point the font cannot show is font '?'; for
 * one beyond the BMP (an emoji, an icon) a box the width of the cell. */
vt_glyph vt_map_glyph(vt_u32 cp, enum vt_font_enc enc);

/* 1 when vt_map_glyph shows cp as itself (the font's glyph, or a drawn line
 * or block); 0 when it gives a stand-in ('-' for an en dash) or the
 * replacement '?'. Those cells are the ones an outline font can do better
 * (render/outline, plan 2026-10-03-outline-fonts.md). */
int vt_glyph_native(vt_u32 cp, enum vt_font_enc enc);

/* What a cell's code points (vt_cell_text: the character, then its marks)
 * draw: the character composed with each mark that has a precomposed form
 * (e + U+0301 is U+00E9, one glyph the font may have), then the marks left
 * to draw over it. Variation selectors, joiners and other invisible
 * controls are dropped. cp is rewritten in place; returns the new count,
 * cp[0] the character to draw. */
int vt_compose_cell(vt_u32 *cp, int n);

/* A glyph source that answers with a mask: cp over `cells` cells (1 or 2)
 * as the cell's height of rows, *bpr bytes each, set bits the ink; 0 when
 * it has no glyph for cp. */
typedef const vt_u8 *(*vt_mask_source)(void *src, vt_u32 cp, int cells, int *bpr);

/* The sources asked for what the font cannot show itself, in this order:
 * the outline font the profile names (font-fallback, F1: the user's
 * choice, Nerd Font icons), then GNU Unifont for the BMP (render/unifont,
 * U2). A source left 0 is skipped. */
typedef struct vt_fallback {
    enum vt_font_enc enc;
    vt_mask_source outline;
    void *outline_src;
    vt_mask_source unifont;
    void *unifont_src;
} vt_fallback;

/* The mask cp draws from over `cells` cells: 0 when the font shows cp
 * itself (vt_glyph_native) or no source has it -- then vt_map_glyph's
 * stand-in or replacement. */
const vt_u8 *vt_fallback_glyph(const vt_fallback *f, vt_u32 cp, int cells, int *bpr);

/* What a cell with a character past ASCII draws (the renderer's rows and
 * cursor): cp[VT_CLUSTER_CPS] gets its code points composed
 * (vt_compose_cell: cp[0] the character, then the marks to draw over it),
 * *ncp how many; the mask from vt_fallback_glyph over the cell's width
 * (the engine's width table: 2 for a wide character), or 0 and *g what
 * vt_map_glyph gives. */
const vt_u8 *vt_cell_glyph(const vt_fallback *f, vt_term *t, const vt_cell *c, vt_u32 *cp, int *ncp,
                           int *bpr, vt_glyph *g);

/* UTF-8 text as a Latin-1 string an Intuition title can show: each code
 * point as vt_map_glyph gives it for a Latin-1 font (so a stand-in, never a
 * byte of the sequence), '?' for one drawn as lines or blocks or not
 * decodable. At most max - 1 characters and a NUL; returns the length. */
int vt_latin1_text(const char *utf8, char *out, int max);

/* Arm weight 0-3 of a VT_GLYPH_BOX code, for arm 0 up, 1 right, 2 down, 3 left. */
#define VT_BOX_ARM(code, arm) (((code) >> ((arm) * 2)) & 3)

#endif
