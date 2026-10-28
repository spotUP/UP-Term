/* render/fontpair: the font a screen gets for its pixel shape. */
#include <string.h>
#include "harness.h"
#include "../render/fontpair.h"

static fontpair_choice ch;

static void topaz_becomes_topaz_pro_on_square_pixels(void)
{
    fontpair_choose("topaz.font", 8, 1, 1, &ch);
    CHECK_STR(ch.name, "TopazPro.font");
    CHECK_INT(ch.size, 16);
    CHECK_INT(ch.changed, 1);
    fontpair_choose("TOPAZ", 8, 1, 1, &ch);        /* any case, no suffix */
    CHECK_STR(ch.name, "TopazPro.font");
    fontpair_choose("topaz.font", 11, 1, 1, &ch);  /* another design: a size chosen stays */
    CHECK_STR(ch.name, "topaz.font");
    CHECK_INT(ch.size, 11);
    CHECK_INT(ch.changed, 0);
    fontpair_choose("TopazPro.font", 16, 0, 1, &ch); /* back on tall pixels */
    CHECK_STR(ch.name, "topaz.font");
    CHECK_INT(ch.size, 8);
    CHECK_INT(ch.changed, 1);
}

static void ibm_8_and_16_swap(void)
{
    fontpair_choose("IBM.font", 8, 1, 1, &ch);
    CHECK_STR(ch.name, "IBM.font");
    CHECK_INT(ch.size, 16);
    fontpair_choose("IBM.font", 16, 0, 1, &ch);
    CHECK_INT(ch.size, 8);
    fontpair_choose("IBM.font", 11, 1, 1, &ch);    /* not in a pair: as asked */
    CHECK_INT(ch.size, 11);
    CHECK_INT(ch.changed, 0);
}

static void the_tall_font_stays_when_25_rows_do_not_fit(void)
{
    fontpair_choose("topaz.font", 8, 1, 0, &ch);
    CHECK_STR(ch.name, "topaz.font");
    CHECK_INT(ch.size, 8);
    CHECK_INT(ch.changed, 0);
}

/* 25 rows of 16-pixel cells = 400, two (bar + 1) = 2 x 12 for a 11 pixel bar, 4 spare: 428 lines */
static void square_fits_needs_25_rows_of_16_pixels(void)
{
    CHECK_INT(fontpair_square_fits(256, 11), 0);
    CHECK_INT(fontpair_square_fits(512, 11), 1);
    CHECK_INT(fontpair_square_fits(427, 11), 0);
    CHECK_INT(fontpair_square_fits(428, 11), 1);
}

static void other_fonts_are_left_alone(void)
{
    fontpair_choose("courier.font", 13, 1, 1, &ch);
    CHECK_STR(ch.name, "courier.font");
    CHECK_INT(ch.size, 13);
    CHECK_INT(ch.changed, 0);
    fontpair_choose("topazx.font", 8, 1, 1, &ch);  /* a prefix is not the name */
    CHECK_STR(ch.name, "topazx.font");
}

/* /font-size steps through a face: from topaz 11 up is TopazPro 16 on
 * square pixels -- the pair is the face's, whatever size is drawn */
static void a_face_knows_its_pair(void)
{
    fontpair_face("topaz.font", 1, 1, &ch);
    CHECK_STR(ch.name, "TopazPro.font");
    CHECK_INT(ch.size, 16);
    CHECK_INT(ch.changed, 1);
    fontpair_face("topaz", 0, 1, &ch);             /* tall pixels: the face's own */
    CHECK_INT(ch.changed, 0);
    fontpair_face("courier.font", 1, 1, &ch);
    CHECK_INT(ch.changed, 0);
    CHECK_INT(ch.size, 0);
}

/* Owner 2026-10-23: the Amarchy menu uses Omarchy's font, JetBrains Mono 16, on square pixels; the terminal
 * keeps TopazPro 16; tall pixels keep topaz 8 for both. */
static void menu_uses_jetbrains_mono_on_square_screens(void)
{
    fontpair_choice m[FONTPAIR_MENU_MAX];
    CHECK_INT(fontpair_menu_choices(0, 0, 1, 1, m), 2);
    CHECK_STR(m[0].name, "JetBrainsMono.font");
    CHECK_INT(m[0].size, 16);
    CHECK_STR(m[1].name, "TopazPro.font");        /* the fallback is the terminal's pick */
    CHECK_INT(m[1].size, 16);
}

static void a_preferred_menu_font_comes_first_and_the_built_in_choices_stay_as_fallbacks(void)
{
    fontpair_choice m[FONTPAIR_MENU_MAX];
    CHECK_INT(fontpair_menu_choices("IBM.font", 16, 1, 1, m), 3);
    CHECK_STR(m[0].name, "IBM.font");
    CHECK_INT(m[0].size, 16);
    CHECK_STR(m[1].name, "JetBrainsMono.font");
    CHECK_STR(m[2].name, "TopazPro.font");
    CHECK_INT(fontpair_menu_choices("", 16, 1, 1, m), 2);       /* no preference: nothing changes */
    CHECK_INT(fontpair_menu_choices("IBM.font", 0, 0, 1, m), 1);
    CHECK_STR(m[0].name, "topaz.font");
}

/* Owner 2026-10-28: the drawn menu's text was not antialiased, because the default JetBrains Mono only came
 * as the 1-bit 16 strike. The default choice now names the TrueType size, so mt_open draws FONTS:JetBrainsMono.ttf
 * antialiased first; a preferred font keeps its own size as TrueType; the topaz fallbacks stay bitmap only. */
static void menu_jetbrains_mono_opens_antialiased_as_truetype_first(void)
{
    fontpair_choice m[FONTPAIR_MENU_MAX];
    CHECK_INT(fontpair_menu_choices(0, 0, 1, 1, m), 2);
    CHECK_STR(m[0].name, "JetBrainsMono.font");
    CHECK_INT(m[0].ttf_px, 12);
    CHECK_INT(m[1].ttf_px, 0);
    CHECK_INT(fontpair_menu_choices("Hack.font", 14, 1, 1, m), 3);
    CHECK_INT(m[0].ttf_px, 14);
    CHECK_INT(m[1].ttf_px, 12);
    CHECK_INT(fontpair_menu_choices(0, 0, 0, 1, m), 1);
    CHECK_INT(m[0].ttf_px, 0);
}

static void menu_keeps_topaz_8_on_tall_pixels_and_when_25_rows_do_not_fit(void)
{
    fontpair_choice m[FONTPAIR_MENU_MAX];
    CHECK_INT(fontpair_menu_choices(0, 0, 0, 1, m), 1);
    CHECK_STR(m[0].name, "topaz.font");
    CHECK_INT(m[0].size, 8);
    CHECK_INT(fontpair_menu_choices(0, 0, 1, 0, m), 1);
    CHECK_STR(m[0].name, "topaz.font");
    CHECK_INT(m[0].size, 8);
}

static void the_terminal_stays_on_topaz_pro_16_on_square_screens(void)
{
    fontpair_choose("topaz.font", 8, 1, 1, &ch);
    CHECK_STR(ch.name, "TopazPro.font");
    CHECK_INT(ch.size, 16);
}

void suite_fontpair(void)
{
    square_fits_needs_25_rows_of_16_pixels();
    a_face_knows_its_pair();
    topaz_becomes_topaz_pro_on_square_pixels();
    ibm_8_and_16_swap();
    the_tall_font_stays_when_25_rows_do_not_fit();
    other_fonts_are_left_alone();
    menu_uses_jetbrains_mono_on_square_screens();
    a_preferred_menu_font_comes_first_and_the_built_in_choices_stay_as_fallbacks();
    menu_jetbrains_mono_opens_antialiased_as_truetype_first();
    menu_keeps_topaz_8_on_tall_pixels_and_when_25_rows_do_not_fit();
    the_terminal_stays_on_topaz_pro_16_on_square_screens();
}
