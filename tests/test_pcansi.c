/* pcansi personality: ANSI.SYS / BBS art, as DCTelnet's term-engine.c draws it. */
#include "harness.h"

static void bold_is_bright_and_blink_is_ice(void)
{
    vt_term *t = h_new(10, 1, VT_PCANSI);
    vt_u16 f, b;
    h_put(t, "\033[1;30ma\033[0;5;44mb\033[0mc");
    vt_resolve_colors(t, h_cell(t, 0, 0), &f, &b);
    CHECK_INT(f, 8); /* 1;30 is the dark grey BBS art shadows with */
    CHECK_INT(b, 0);
    vt_resolve_colors(t, h_cell(t, 1, 0), &f, &b);
    CHECK_INT(f, 7);
    CHECK_INT(b, 12);
    vt_resolve_colors(t, h_cell(t, 2, 0), &f, &b);
    CHECK_INT(f, 7);
    CHECK_INT(b, 0);
    vt_free(t);
}

/* iCE: blink is a bright background, so an erase or a scroll after
 * ESC[5m fills with the bright background, as a printed cell has it
 * (found by tools/te_diff on HTF-SHES.ANS against term-engine). */
static void erase_after_blink_fills_with_the_bright_background(void)
{
    vt_term *t = h_new(10, 2, VT_PCANSI);
    vt_u16 f, b;
    h_put(t, "\033[5ma\033[K");
    vt_resolve_colors(t, h_cell(t, 0, 0), &f, &b);
    CHECK_INT(b, 8);
    vt_resolve_colors(t, h_cell(t, 5, 0), &f, &b);
    CHECK_INT(b, 8); /* EL */
    h_put(t, "\r\n\n");
    vt_resolve_colors(t, h_cell(t, 5, 1), &f, &b);
    CHECK_INT(b, 8); /* the row the scroll vacated */
    vt_free(t);
}

/* Art uses 0x0B as the CP437 male sign (IPH-PLT.ANS, GOODBYE.ANS); a
 * line feed there shifts every line below it. */
/* ANSI.SYS swaps the attribute byte for inverse, and erases with it: a
 * row scrolled in under ESC[7m is in the foreground colour
 * (ICEHOUSE_84TTk2M.TXT against term-engine). */
static void erase_under_inverse_fills_with_the_foreground(void)
{
    vt_term *t = h_new(10, 2, VT_PCANSI);
    vt_u16 f, b;
    h_put(t, "\033[1;33;44;7m\033[K\r\n\n");
    vt_resolve_colors(t, h_cell(t, 0, 1), &f, &b);
    CHECK_INT(f, 4);
    CHECK_INT(b, 11);
    vt_free(t);
}

/* ESC[1;1T scrolls down by the first parameter, as term-engine and
 * SyncTERM do (__z9hnNn8 in the BBS ad corpus). */
static void scroll_down_takes_the_first_of_two_parameters(void)
{
    vt_term *t = h_new(4, 2, VT_PCANSI);
    h_put(t, "ab\033[1;1T");
    CHECK_INT(h_cell(t, 0, 0)->ch, ' ');
    CHECK_INT(h_cell(t, 0, 1)->ch, 'a');
    vt_free(t);
}

/* BBS art saves with ESC[s, changes colour, restores with ESC[u and
 * draws on in the new colour (LSP-CS.ANS against term-engine). */
static void restore_position_keeps_the_colour(void)
{
    vt_term *t = h_new(10, 2, VT_PCANSI);
    vt_u16 f, b;
    h_put(t, "\033[s\033[1;33mX\033[uY");
    vt_resolve_colors(t, h_cell(t, 0, 0), &f, &b);
    CHECK_INT(h_cell(t, 0, 0)->ch, 'Y');
    CHECK_INT(f, 11);
    vt_free(t);
}

static void vertical_tab_does_not_move_the_cursor(void)
{
    vt_term *t = h_new(10, 3, VT_PCANSI);
    int x, y;
    h_put(t, "ab\013c");
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 3);
    CHECK_INT(y, 0);
    vt_free(t);
}

static void erase_display_homes_the_cursor(void)
{
    vt_term *t = h_new(10, 3, VT_PCANSI);
    int x, y;
    h_put(t, "\033[2;4H\033[2J");
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 0);
    CHECK_INT(y, 0);
    vt_free(t);
}

static void high_bytes_are_cp437(void)
{
    vt_term *t = h_new(10, 1, VT_PCANSI);
    h_put(t, "\xc9\xcd\xbb\xb0\xdb");
    CHECK_INT(h_cell(t, 0, 0)->ch, 0x2554);
    CHECK_INT(h_cell(t, 1, 0)->ch, 0x2550);
    CHECK_INT(h_cell(t, 2, 0)->ch, 0x2557);
    CHECK_INT(h_cell(t, 3, 0)->ch, 0x2591);
    CHECK_INT(h_cell(t, 4, 0)->ch, 0x2588);
    vt_free(t);
}

static void eight_bit_csi_is_honoured(void)
{
    vt_term *t = h_new(10, 1, VT_PCANSI);
    h_put(t, "ab\x9b" "1DX");
    CHECK_STR(h_row(t, 0), "aX");
    vt_free(t);
}

static void form_feed_clears_and_homes(void)
{
    vt_term *t = h_new(10, 2, VT_PCANSI);
    h_put(t, "abc\r\ndef\014X");
    CHECK_STR(h_screen(t), "X");
    vt_free(t);
}

static void save_restore_with_s_and_u(void)
{
    vt_term *t = h_new(10, 2, VT_PCANSI);
    h_put(t, "\033[2;5H\033[s\033[1;1H\033[uX");
    CHECK_INT(h_cell(t, 4, 1)->ch, 'X');
    vt_free(t);
}

static void eighty_column_art_wraps_deferred(void)
{
    vt_term *t = h_new(4, 3, VT_PCANSI);
    h_put(t, "abcd\r\nefgh");
    CHECK_STR(h_screen(t), "abcd|efgh");
    vt_free(t);
}

static void aixterm_bright_sets_bold_like_dctelnet(void)
{
    vt_term *t = h_new(10, 1, VT_PCANSI);
    vt_u16 f, b;
    h_put(t, "\033[91ma\033[32mb\033[0;104mc");
    vt_resolve_colors(t, h_cell(t, 1, 0), &f, &b);
    CHECK_INT(f, 10); /* stays bright until 22/0 */
    vt_resolve_colors(t, h_cell(t, 2, 0), &f, &b);
    CHECK_INT(b, 12);
    vt_free(t);
}

static void extended_colours_are_consumed(void)
{
    vt_term *t = h_new(10, 1, VT_PCANSI);
    h_put(t, "\033[38;5;196;1ma\033[0;38:5:9mb");
    CHECK(h_cell(t, 0, 0)->attr & VT_ATTR_BOLD);
    CHECK_INT(h_cell(t, 0, 0)->fg, VT_COLOR_DEFAULT);
    CHECK_INT(h_cell(t, 1, 0)->fg, VT_COLOR_DEFAULT);
    vt_free(t);
}

static void del_is_the_house_glyph(void)
{
    vt_term *t = h_new(10, 1, VT_PCANSI);
    h_put(t, "\177");
    CHECK_INT(h_cell(t, 0, 0)->ch, 0x2302);
    vt_free(t);
}

void suite_pcansi(void)
{
    aixterm_bright_sets_bold_like_dctelnet();
    extended_colours_are_consumed();
    del_is_the_house_glyph();
    bold_is_bright_and_blink_is_ice();
    erase_after_blink_fills_with_the_bright_background();
    vertical_tab_does_not_move_the_cursor();
    restore_position_keeps_the_colour();
    scroll_down_takes_the_first_of_two_parameters();
    erase_under_inverse_fills_with_the_foreground();
    erase_display_homes_the_cursor();
    high_bytes_are_cp437();
    eight_bit_csi_is_honoured();
    form_feed_clears_and_homes();
    save_restore_with_s_and_u();
    eighty_column_art_wraps_deferred();
}
