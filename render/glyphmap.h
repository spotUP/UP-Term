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

/* Arm weight 0-3 of a VT_GLYPH_BOX code, for arm 0 up, 1 right, 2 down, 3 left. */
#define VT_BOX_ARM(code, arm) (((code) >> ((arm) * 2)) & 3)

#endif
