/* render/chips: the blitter scroll's plan and the CPU's permission to write
 * beside it (CC1); the sprite cursor's geometry, image, colours and the
 * fallback to the planes (CC2). */
#include "harness.h"
#include "../render/chips.h"

/* ---- CC1 ----------------------------------------------------------------- */

static void a_scroll_up_copies_the_rest_and_owes_the_bottom_rows(void)
{
    vc_scroll s;
    vc_scroll_plan(0, 24, 3, &s);
    CHECK_INT(s.copy, 21);
    CHECK_INT(s.src, 3);
    CHECK_INT(s.dst, 0);
    CHECK_INT(s.vac0, 21);
    CHECK_INT(s.vac1, 24);
    CHECK_INT(s.busy0, 0);
    CHECK_INT(s.busy1, 24);
}

static void a_region_scroll_down_owes_its_top_rows_and_leaves_the_rest_free(void)
{
    vc_scroll s;
    vc_scroll_plan(2, 10, -2, &s); /* a status line below row 10 stays outside */
    CHECK_INT(s.copy, 6);
    CHECK_INT(s.src, 2);
    CHECK_INT(s.dst, 4);
    CHECK_INT(s.vac0, 2);
    CHECK_INT(s.vac1, 4);
    CHECK_INT(s.busy0, 2);
    CHECK_INT(s.busy1, 10);
}

static void a_scroll_past_the_region_copies_nothing_and_owes_it_all(void)
{
    vc_scroll s;
    vc_scroll_plan(0, 20, 25, &s);
    CHECK_INT(s.copy, 0);
    CHECK_INT(s.vac0, 0);
    CHECK_INT(s.vac1, 20);
    CHECK_INT(s.busy1 - s.busy0, 0); /* no blit: the painter need not wait */
    vc_scroll_plan(0, 20, 0, &s);
    CHECK_INT(s.vac1 - s.vac0, 0);
}

static void the_cpu_waits_only_for_a_blit_it_could_collide_with(void)
{
    CHECK(vc_cpu_may_write(VC_BLIT_IDLE, 0, 0, 10, 20));
    /* the copy on pixel rows 16..176: a status row at 176 is free */
    CHECK(vc_cpu_may_write(VC_BLIT_SCROLL, 16, 176, 176, 184));
    CHECK(vc_cpu_may_write(VC_BLIT_SCROLL, 16, 176, 8, 16));
    CHECK(!vc_cpu_may_write(VC_BLIT_SCROLL, 16, 176, 168, 177)); /* the vacated rows are in its source */
    CHECK(!vc_cpu_may_write(VC_BLIT_SCROLL, 16, 176, 15, 17));
    /* any other graphics call: its blit's rectangle is not known */
    CHECK(!vc_cpu_may_write(VC_BLIT_ANY, 16, 176, 300, 308));
}

static void the_blit_scroll_needs_a_pass_and_writable_planes(void)
{
    CHECK(vc_scroll_by_blit(1, 1, 0));
    CHECK(!vc_scroll_by_blit(0, 1, 0)); /* outside a pass nothing pays the owed rows */
    CHECK(!vc_scroll_by_blit(1, 0, 0)); /* covered, RTG, not an 8-pixel font: ScrollRaster */
    CHECK(!vc_scroll_by_blit(1, 1, 1));
}

/* ---- CC2 ----------------------------------------------------------------- */

static void the_cursor_shapes_cover_the_cell_its_bottom_or_its_left(void)
{
    int dx, dy, w, h;
    vc_cursor_rect(2, 8, 8, &dx, &dy, &w, &h);
    CHECK(dx == 0 && dy == 0 && w == 8 && h == 8);
    vc_cursor_rect(4, 8, 8, &dx, &dy, &w, &h);
    CHECK(dx == 0 && dy == 6 && w == 8 && h == 2);
    vc_cursor_rect(5, 16, 8, &dx, &dy, &w, &h); /* a double-width row's cell */
    CHECK(dx == 0 && dy == 0 && w == 2 && h == 8);
}

static void a_hires_cell_is_four_lores_sprite_pixels_and_eight_with_hires_sprites(void)
{
    vc_sprite_geom g;
    CHECK(vc_sprite_geom_of(70, 140, 0, 8, 8, &g));
    CHECK_INT(g.sw, 4);
    CHECK_INT(g.sh, 8);
    CHECK(vc_sprite_geom_of(70, 70, 0, 8, 8, &g));
    CHECK_INT(g.sw, 8);
    CHECK(vc_sprite_geom_of(35, 35, 0, 8, 16, &g));
    CHECK_INT(g.sw, 8);
    CHECK(vc_sprite_geom_of(70, 140, 1, 8, 16, &g)); /* laced: a sprite line over two rows */
    CHECK_INT(g.sh, 8);
}

static void a_cell_that_is_not_whole_sprite_pixels_or_lines_falls_back(void)
{
    vc_sprite_geom g;
    CHECK(!vc_sprite_geom_of(35, 140, 0, 6, 8, &g));  /* 1.5 lores pixels */
    CHECK(!vc_sprite_geom_of(70, 140, 1, 8, 9, &g));  /* an odd height on a laced screen */
    CHECK(!vc_sprite_geom_of(140, 35, 0, 8, 8, &g));  /* 32 pixels: wider than the image */
    CHECK(!vc_sprite_geom_of(70, 140, 0, 8, 65, &g)); /* taller than the image */
    CHECK(!vc_sprite_geom_of(0, 140, 0, 8, 8, &g));   /* an unknown pixel speed (RTG) */
}

static void the_position_is_lores_pixels_and_whole_lines(void)
{
    vc_sprite_geom g;
    long sx, sy;
    vc_sprite_geom_of(70, 140, 0, 8, 8, &g);
    CHECK(vc_sprite_pos(&g, 100, 33, &sx, &sy));
    CHECK_INT(sx, 50);
    CHECK_INT(sy, 33);
    CHECK(!vc_sprite_pos(&g, 101, 33, &sx, &sy)); /* half a lores pixel: the planes draw it */
    CHECK(!vc_sprite_pos(&g, -2, 0, &sx, &sy));
    vc_sprite_geom_of(140, 140, 1, 8, 16, &g);
    CHECK(vc_sprite_pos(&g, 17, 40, &sx, &sy));
    CHECK_INT(sx, 17);
    CHECK_INT(sy, 20);
    CHECK(!vc_sprite_pos(&g, 17, 41, &sx, &sy));
}

static void a_block_shows_the_glyph_in_colour_two_on_colour_one(void)
{
    static const unsigned char glyph[8] = { 0x18, 0x24, 0x42, 0x7E, 0x42, 0x42, 0x00, 0x00 }; /* A */
    unsigned short a[VC_SPRITE_H], b[VC_SPRITE_H];
    vc_sprite_geom g;
    vc_sprite_geom_of(70, 70, 0, 8, 8, &g);
    CHECK(vc_sprite_image(&g, 0, 0, 8, 8, glyph, a, b));
    CHECK_INT(b[0], 0x1800);         /* the glyph's pixels, colour 2 */
    CHECK_INT(a[0], 0xE700);         /* the rest of the cell, colour 1 */
    CHECK_INT(a[3] | b[3], 0xFF00);  /* the whole cell, nothing beyond it */
    CHECK_INT(b[3], 0x7E00);
    CHECK_INT(a[7], 0xFF00);
}

static void an_underline_is_two_lines_and_a_bar_one_lores_pixel(void)
{
    unsigned short a[VC_SPRITE_H], b[VC_SPRITE_H];
    vc_sprite_geom g;
    int i, set = 0;
    vc_sprite_geom_of(70, 140, 0, 8, 8, &g);
    CHECK(vc_sprite_image(&g, 0, 6, 8, 2, 0, a, b));
    for (i = 0; i < 6; i++)
        set |= a[i] | b[i];
    CHECK_INT(set, 0);
    CHECK_INT(a[6], 0xF000); /* four lores pixels: the cell */
    CHECK_INT(a[7], 0xF000);
    CHECK(vc_sprite_image(&g, 0, 0, 2, 8, 0, a, b));
    CHECK_INT(a[0], 0x8000); /* the bar's two hires pixels */
    CHECK_INT(b[0], 0);
}

static void a_coarse_sprite_shows_a_blank_cell_but_not_a_glyph(void)
{
    static const unsigned char blank[8] = { 0 };
    static const unsigned char glyph[8] = { 0, 0x10, 0, 0, 0, 0, 0, 0 };
    unsigned short a[VC_SPRITE_H], b[VC_SPRITE_H];
    vc_sprite_geom g;
    vc_sprite_geom_of(70, 140, 0, 8, 8, &g);
    CHECK(vc_sprite_image(&g, 0, 0, 8, 8, blank, a, b));
    CHECK_INT(a[4], 0xF000);
    CHECK(!vc_sprite_image(&g, 0, 0, 8, 8, glyph, a, b));
    vc_sprite_geom_of(70, 70, 1, 8, 8, &g); /* laced: two rows a line */
    CHECK(!vc_sprite_image(&g, 0, 0, 8, 8, glyph, a, b));
    CHECK(vc_sprite_image(&g, 0, 0, 8, 8, blank, a, b));
    CHECK_INT(g.sh, 4);
    CHECK_INT(a[3], 0xFF00);
}

static void the_sprite_colours_must_not_be_text_colours(void)
{
    CHECK_INT(vc_sprite_colour_reg(2, 16, 16, 4, 32), 21); /* the RKM's table: sprites 2/3, 21-23 */
    CHECK_INT(vc_sprite_colour_reg(7, 16, 16, 3, 32), 29);
    CHECK_INT(vc_sprite_colour_reg(2, 16, 16, 5, 32), -1); /* 32 colours: 21 is a pen of the text */
    CHECK_INT(vc_sprite_colour_reg(2, 16, 16, 4, 16), -1); /* a colour map without those entries */
    CHECK_INT(vc_sprite_colour_reg(3, 16, 240, 7, 256), 245); /* AGA: the odd sprites' bank moved */
    CHECK_INT(vc_sprite_colour_reg(2, 240, 240, 8, 256), -1);
}

static void the_planes_draw_the_cursor_whenever_the_sprite_cannot(void)
{
    vc_cursor_env e;
    e.native = e.visible = e.geom = e.pos = e.plain_cell = e.image = e.colours = e.have_sprite = 1;
    CHECK_INT(vc_cursor_choice(&e), VC_CUR_SPRITE);
    e.have_sprite = 0;
    CHECK_INT(vc_cursor_choice(&e), VC_CUR_NONE);
    e.colours = 0;
    CHECK_INT(vc_cursor_choice(&e), VC_CUR_COLOURS);
    e.image = 0;
    CHECK_INT(vc_cursor_choice(&e), VC_CUR_IMAGE);
    e.plain_cell = 0;
    CHECK_INT(vc_cursor_choice(&e), VC_CUR_CELL);
    e.pos = 0;
    CHECK_INT(vc_cursor_choice(&e), VC_CUR_POS);
    e.geom = 0;
    CHECK_INT(vc_cursor_choice(&e), VC_CUR_GEOM);
    e.visible = 0;
    CHECK_INT(vc_cursor_choice(&e), VC_CUR_HIDDEN);
    e.native = 0;
    CHECK_INT(vc_cursor_choice(&e), VC_CUR_RTG);
}

void suite_chips(void)
{
    a_scroll_up_copies_the_rest_and_owes_the_bottom_rows();
    a_region_scroll_down_owes_its_top_rows_and_leaves_the_rest_free();
    a_scroll_past_the_region_copies_nothing_and_owes_it_all();
    the_cpu_waits_only_for_a_blit_it_could_collide_with();
    the_blit_scroll_needs_a_pass_and_writable_planes();
    the_cursor_shapes_cover_the_cell_its_bottom_or_its_left();
    a_hires_cell_is_four_lores_sprite_pixels_and_eight_with_hires_sprites();
    a_cell_that_is_not_whole_sprite_pixels_or_lines_falls_back();
    the_position_is_lores_pixels_and_whole_lines();
    a_block_shows_the_glyph_in_colour_two_on_colour_one();
    an_underline_is_two_lines_and_a_bar_one_lores_pixel();
    a_coarse_sprite_shows_a_blank_cell_but_not_a_glyph();
    the_sprite_colours_must_not_be_text_colours();
    the_planes_draw_the_cursor_whenever_the_sprite_cannot();
}
