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

void suite_pcansi(void)
{
    bold_is_bright_and_blink_is_ice();
    erase_display_homes_the_cursor();
    high_bytes_are_cp437();
    eight_bit_csi_is_honoured();
    form_feed_clears_and_homes();
    save_restore_with_s_and_u();
    eighty_column_art_wraps_deferred();
}
