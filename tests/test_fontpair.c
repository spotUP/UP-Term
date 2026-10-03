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

void suite_fontpair(void)
{
    a_face_knows_its_pair();
    topaz_becomes_topaz_pro_on_square_pixels();
    ibm_8_and_16_swap();
    the_tall_font_stays_when_25_rows_do_not_fit();
    other_fonts_are_left_alone();
}
