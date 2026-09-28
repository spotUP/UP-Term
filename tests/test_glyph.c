/* render/glyphmap: what each code point becomes on an Amiga font. */
#include "harness.h"
#include "../render/glyphmap.h"

static void latin1_font_draws_latin1_itself(void)
{
    vt_glyph g = vt_map_glyph(0xE5, VT_ENC_LATIN1);
    CHECK_INT(g.kind, VT_GLYPH_FONT);
    CHECK_INT(g.code, 0xE5);
    g = vt_map_glyph('A', VT_ENC_LATIN1);
    CHECK_INT(g.code, 'A');
}

static void box_drawing_becomes_lines(void)
{
    vt_glyph g = vt_map_glyph(0x250C, VT_ENC_LATIN1); /* light down and right */
    CHECK_INT(g.kind, VT_GLYPH_BOX);
    CHECK_INT(VT_BOX_ARM(g.code, 0), 0);
    CHECK_INT(VT_BOX_ARM(g.code, 1), 1);
    CHECK_INT(VT_BOX_ARM(g.code, 2), 1);
    CHECK_INT(VT_BOX_ARM(g.code, 3), 0);
    g = vt_map_glyph(0x256C, VT_ENC_LATIN1); /* double vertical and horizontal */
    CHECK_INT(g.code, 0xFF);
    g = vt_map_glyph(0x2541, VT_ENC_LATIN1); /* down heavy and up horizontal light */
    CHECK_INT(VT_BOX_ARM(g.code, 2), 2);
    CHECK_INT(VT_BOX_ARM(g.code, 0), 1);
    CHECK_INT(VT_BOX_ARM(g.code, 1), 1);
    g = vt_map_glyph(0x2573, VT_ENC_LATIN1);
    CHECK_INT(g.kind, VT_GLYPH_DIAGONAL);
    CHECK_INT(g.code, 3);
}

static void blocks_become_rectangles(void)
{
    vt_glyph g = vt_map_glyph(0x2584, VT_ENC_LATIN1); /* lower half */
    CHECK_INT(g.kind, VT_GLYPH_BLOCK);
    CHECK_INT(g.code, (1 << 4) | 4);
    g = vt_map_glyph(0x2588, VT_ENC_LATIN1);
    CHECK_INT(g.code, 8);
    g = vt_map_glyph(0x2592, VT_ENC_LATIN1);
    CHECK_INT(g.code, 0x82);
    g = vt_map_glyph(0x259E, VT_ENC_LATIN1); /* upper right and lower left */
    CHECK_INT(g.code, 0x40 | 2 | 4);
}

static void cp437_font_uses_its_own_line_glyphs(void)
{
    vt_glyph g = vt_map_glyph(0x2554, VT_ENC_CP437);
    CHECK_INT(g.kind, VT_GLYPH_FONT);
    CHECK_INT(g.code, 0xC9);
    g = vt_map_glyph(0x2591, VT_ENC_CP437);
    CHECK_INT(g.code, 0xB0);
    g = vt_map_glyph(0x2572, VT_ENC_CP437); /* not in CP437: drawn */
    CHECK_INT(g.kind, VT_GLYPH_DIAGONAL);
}

static void unknown_code_points_get_a_stand_in(void)
{
    vt_glyph g = vt_map_glyph(0x201C, VT_ENC_LATIN1);
    CHECK_INT(g.code, '"');
    g = vt_map_glyph(0x2022, VT_ENC_LATIN1);
    CHECK_INT(g.code, 0xB7);
    g = vt_map_glyph(0x2022, VT_ENC_CP437); /* the middle dot is CP437 0xFA */
    CHECK_INT(g.code, 0xFA);
    g = vt_map_glyph(0x4E2D, VT_ENC_LATIN1);
    CHECK_INT(g.code, '?');
    g = vt_map_glyph(0x23BB, VT_ENC_LATIN1);
    CHECK_INT(g.kind, VT_GLYPH_HLINE);
    CHECK_INT(g.code, 2);
}

void suite_glyph(void)
{
    latin1_font_draws_latin1_itself();
    box_drawing_becomes_lines();
    blocks_become_rectangles();
    cp437_font_uses_its_own_line_glyphs();
    unknown_code_points_get_a_stand_in();
}
