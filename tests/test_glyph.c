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

/* Every CP437 high byte maps back to itself (the reverse table is sorted
 * for a binary search; a wrong order or a drifted entry loses glyphs). */
static void cp437_round_trips_every_high_byte(void)
{
    const vt_u16 *t = vt_cp437_table();
    int i;
    for (i = 0; i < 128; i++) {
        vt_glyph g = vt_map_glyph(t[i], VT_ENC_CP437);
        CHECK_INT(g.kind, VT_GLYPH_FONT);
        CHECK_INT(g.code, 0x80 + i);
    }
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

/* The cells an outline font may draw instead: stand-ins and the
 * replacement; never what the font or the line drawing shows itself. */
static void only_stand_ins_and_the_replacement_are_not_native(void)
{
    CHECK(vt_glyph_native('A', VT_ENC_LATIN1));
    CHECK(vt_glyph_native(0xE5, VT_ENC_LATIN1));
    CHECK(vt_glyph_native(0x2500, VT_ENC_LATIN1));  /* drawn line */
    CHECK(vt_glyph_native(0x2588, VT_ENC_LATIN1));  /* drawn block */
    CHECK(vt_glyph_native(0x2502, VT_ENC_CP437));   /* the IBM font's own */
    CHECK(!vt_glyph_native(0x2014, VT_ENC_LATIN1)); /* em dash: '-' stands in */
    CHECK(!vt_glyph_native(0xE0A0, VT_ENC_LATIN1)); /* a Nerd Font icon: '?' */
    CHECK(!vt_glyph_native(0x4E2D, VT_ENC_LATIN1)); /* CJK: '?' */
    CHECK(!vt_glyph_native(0x2022, VT_ENC_CP437));  /* bullet: CP437's middle dot stands in */
    CHECK(vt_glyph_native(0xE5, VT_ENC_CP437));     /* CP437 has a-ring */
}

/* An emoji or a plane-15 icon no bitmap font has: a replacement box (the
 * renderer sizes it to the cell's width), not a '?'; and not native, so an
 * outline font is asked first. */
static void beyond_the_bmp_is_a_replacement_box(void)
{
    vt_glyph g = vt_map_glyph(0x1F600, VT_ENC_LATIN1);
    CHECK_INT(g.kind, VT_GLYPH_MISSING);
    CHECK(!vt_glyph_native(0x1F600, VT_ENC_LATIN1));
    g = vt_map_glyph(0xF0001, VT_ENC_CP437);
    CHECK_INT(g.kind, VT_GLYPH_MISSING);
    g = vt_map_glyph(0x4E2D, VT_ENC_LATIN1); /* the BMP keeps its '?' */
    CHECK_INT(g.kind, VT_GLYPH_FONT);
    CHECK_INT(g.code, '?');
}

/* A letter and its combining marks draw as the precomposed letter where
 * one exists (the Latin-1 font has e acute, not U+0301); marks with no
 * composition stay to be drawn over; selectors and joiners draw nothing. */
static void combining_marks_compose_for_the_font(void)
{
    vt_u32 cp[6];
    cp[0] = 'e';
    cp[1] = 0x301;
    CHECK_INT(vt_compose_cell(cp, 2), 1);
    CHECK_INT(cp[0], 0xE9);
    CHECK_INT(vt_map_glyph(cp[0], VT_ENC_LATIN1).code, 0xE9);
    cp[0] = 'e'; /* Vietnamese: e, dot below, circumflex */
    cp[1] = 0x323;
    cp[2] = 0x302;
    CHECK_INT(vt_compose_cell(cp, 3), 1);
    CHECK_INT(cp[0], 0x1EC7);
    cp[0] = 'x';
    cp[1] = 0x301;
    cp[2] = 0xFE0F;
    CHECK_INT(vt_compose_cell(cp, 3), 2);
    CHECK_INT(cp[0], 'x');
    CHECK_INT(cp[1], 0x301);
    cp[0] = 0x2764;
    cp[1] = 0xFE0F;
    cp[2] = 0x200D;
    CHECK_INT(vt_compose_cell(cp, 3), 1);
    CHECK_INT(cp[0], 0x2764);
    cp[0] = 0x1F600;
    cp[1] = 0x301;
    CHECK_INT(vt_compose_cell(cp, 2), 2);
}

void suite_glyph(void)
{
    beyond_the_bmp_is_a_replacement_box();
    combining_marks_compose_for_the_font();
    only_stand_ins_and_the_replacement_are_not_native();
    latin1_font_draws_latin1_itself();
    box_drawing_becomes_lines();
    blocks_become_rectangles();
    cp437_font_uses_its_own_line_glyphs();
    cp437_round_trips_every_high_byte();
    unknown_code_points_get_a_stand_in();
}
