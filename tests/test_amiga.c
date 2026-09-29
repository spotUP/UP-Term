/* amiga personality: the ROM console.device dialect (RKM Devices, console
 * chapter; see the conformance matrix). */
#include "harness.h"

static void window_status_request_reports_bounds(void)
{
    vt_term *t = h_new(77, 23, VT_AMIGA);
    h_put(t, "\x9b" "0 q");
    CHECK_STR(h_reply, "\x9b" "1;1;23;77 r");
    vt_free(t);
}

static void private_layout_sequences_reach_the_host(void)
{
    vt_term *t = h_new(80, 24, VT_AMIGA);
    h_put(t, "\x9b" "20t");
    CHECK_INT(h_layout_which, VT_LAYOUT_PAGE_LENGTH);
    CHECK_INT(h_layout_value, 20);
    h_put(t, "\x9b" "u");
    CHECK_INT(h_layout_which, VT_LAYOUT_LINE_LENGTH);
    CHECK_INT(h_layout_value, -1);
    h_put(t, "\x9b" "8x");
    CHECK_INT(h_layout_which, VT_LAYOUT_LEFT_OFFSET);
    h_put(t, "\x9b" "4y");
    CHECK_INT(h_layout_which, VT_LAYOUT_TOP_OFFSET);
    vt_free(t);
}

static void raw_events_set_and_reset(void)
{
    vt_term *t = h_new(80, 24, VT_AMIGA);
    h_put(t, "\x9b" "1;2;12{");
    CHECK_INT(vt_raw_events(t), (1L << 1) | (1L << 2) | (1L << 12));
    h_put(t, "\x9b" "2}");
    CHECK_INT(vt_raw_events(t), (1L << 1) | (1L << 12));
    vt_free(t);
}

static void cursor_rendition(void)
{
    vt_term *t = h_new(80, 24, VT_AMIGA);
    h_put(t, "\x9b" "0 p");
    CHECK_INT(vt_modes(t) & VT_MODE_CURSOR_VISIBLE, 0);
    h_put(t, "\x9b" " p");
    CHECK(vt_modes(t) & VT_MODE_CURSOR_VISIBLE);
    vt_free(t);
}

static void linefeed_starts_a_new_line(void)
{
    vt_term *t = h_new(10, 3, VT_AMIGA);
    h_put(t, "abc\ndef");
    CHECK_STR(h_screen(t), "abc|def");
    vt_free(t);
}

static void pens_are_screen_pens(void)
{
    vt_term *t = h_new(10, 1, VT_AMIGA);
    vt_u16 f, b;
    h_put(t, "a\x9b" "1;33;41mb");
    vt_resolve_colors(t, h_cell(t, 0, 0), &f, &b);
    CHECK_INT(f, 1);
    CHECK_INT(b, 0);
    vt_resolve_colors(t, h_cell(t, 1, 0), &f, &b);
    CHECK_INT(f, 3); /* bold does not brighten: it is a font style */
    CHECK_INT(b, 1);
    vt_free(t);
}

static void form_feed_clears_the_window(void)
{
    vt_term *t = h_new(10, 2, VT_AMIGA);
    h_put(t, "abc\ndef\014X");
    CHECK_STR(h_screen(t), "X");
    vt_free(t);
}

static void shift_out_sets_the_high_bit(void)
{
    vt_term *t = h_new(10, 1, VT_AMIGA);
    h_put(t, "a\016a\017a");
    CHECK_INT(h_cell(t, 0, 0)->ch, 'a');
    CHECK_INT(h_cell(t, 1, 0)->ch, 0xE1); /* 'a' | 0x80 */
    CHECK_INT(h_cell(t, 2, 0)->ch, 'a');
    vt_free(t);
}

static void del_is_a_glyph(void)
{
    vt_term *t = h_new(10, 1, VT_AMIGA);
    h_put(t, "a\177b");
    CHECK_INT(h_cell(t, 1, 0)->ch, 0x7F);
    CHECK_INT(h_cell(t, 2, 0)->ch, 'b');
    vt_free(t);
}

static void vertical_tab_moves_up(void)
{
    vt_term *t = h_new(10, 3, VT_AMIGA);
    h_put(t, "\n\nab\013c");
    CHECK_STR(h_screen(t), "|  c|ab");
    vt_free(t);
}

static void global_background_as_a_prefixed_item(void)
{
    vt_term *t = h_new(10, 1, VT_AMIGA);
    vt_u16 f, b;
    h_put(t, "\x9b" "1;33;40;>2m\x9b" "K");
    vt_resolve_colors(t, h_cell(t, 5, 0), &f, &b);
    CHECK_INT(b, 2); /* vacated cells take the global background */
    h_put(t, "x");
    CHECK_INT(h_cell(t, 0, 0)->fg, 3);
    CHECK_INT(h_cell(t, 0, 0)->bg, 0);
    vt_free(t);
}

static void set_default_style_makes_sgr0_return_to_it(void)
{
    vt_term *t = h_new(10, 1, VT_AMIGA);
    h_put(t, "\x9b" "32;43m\x9b" " s\x9b" "31m\x9b" "0mx");
    CHECK_INT(h_cell(t, 0, 0)->fg, 2);
    CHECK_INT(h_cell(t, 0, 0)->bg, 3);
    h_put(t, "\033c\x9b" "0my"); /* RIS restores the factory defaults */
    CHECK_INT(h_cell(t, 0, 0)->fg, VT_COLOR_DEFAULT);
    vt_free(t);
}

static void ctc_sets_and_clears_tabs(void)
{
    vt_term *t = h_new(20, 2, VT_AMIGA); /* 2 rows: the last column wraps at once */
    h_put(t, "\x9b" "5W\x9b" "1;4H\x9b" "0W\r\tX");
    CHECK_INT(h_cell(t, 3, 0)->ch, 'X');
    h_put(t, "\x9b" "1;4H\x9b" "2W\r\tY");
    CHECK_INT(h_cell(t, 19, 0)->ch, 'Y');
    vt_free(t);
}

static void cursor_report_uses_the_8bit_csi(void)
{
    vt_term *t = h_new(40, 20, VT_AMIGA);
    h_put(t, "\x9b" "12;7H\x9b" "6n");
    CHECK_STR(h_reply, "\x9b" "12;7R"); /* row;column (matrix erratum E3) */
    vt_free(t);
}

static void xterm_only_sequences_are_inert(void)
{
    vt_term *t = h_new(10, 3, VT_AMIGA);
    h_put(t, "ab\x9b" "2;3r\x9b" "sc");
    CHECK_STR(h_screen(t), "abc");
    vt_free(t);
}

/* Cursor motion as measured on the ROM console (tests/probes, rig
 * 2026-09-29): tools/probe_compare.py checks all 50 cases on the rig; these
 * pin the rules on the host. */
static void rom_measured_cursor_motion(void)
{
    vt_term *t = h_new(79, 25, VT_AMIGA);
    int x, y;
    h_put(t, "\x9b" "2;1H\b");          /* BS at column 1: into the row above */
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 0);
    CHECK_INT(x, 78);
    h_put(t, "\x9b" "1;1H\x9b" "999C"); /* CUF runs on through the rows */
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 12);
    CHECK_INT(x, 51);
    h_put(t, "\x9b" "25;70H\x9b" "20C"); /* and stops on the bottom row */
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 24);
    CHECK_INT(x, 10);
    h_put(t, "\x9b" "1;1H");
    h_put(t, "0123456789012345678901234567890123456789012345678901234567890123456789012345678");
    vt_cursor(t, &x, &y);                /* 79 characters: the wrap is immediate */
    CHECK_INT(y, 1);
    CHECK_INT(x, 0);
    h_put(t, "\x9b" "1;4H\x9b" "5G\0337\x9b" "3;3H\0338");
    vt_cursor(t, &x, &y);                /* no CHA, no ESC 7 / ESC 8 */
    CHECK_INT(y, 2);
    CHECK_INT(x, 2);
    h_put(t, "\x9b" "2;3H\x9b" "L");    /* IL keeps the column */
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 2);
    vt_free(t);
}

void suite_amiga(void)
{
    rom_measured_cursor_motion();
    shift_out_sets_the_high_bit();
    del_is_a_glyph();
    vertical_tab_moves_up();
    global_background_as_a_prefixed_item();
    set_default_style_makes_sgr0_return_to_it();
    ctc_sets_and_clears_tabs();
    cursor_report_uses_the_8bit_csi();
    xterm_only_sequences_are_inert();
    window_status_request_reports_bounds();
    private_layout_sequences_reach_the_host();
    raw_events_set_and_reset();
    cursor_rendition();
    linefeed_starts_a_new_line();
    pens_are_screen_pens();
    form_feed_clears_the_window();
}
