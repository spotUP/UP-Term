/* amiga personality: the ROM console.device dialect (RKM Devices, console
 * chapter; see the conformance matrix). */
#include "harness.h"

/* The ROM console knows none of the ECMA-48 extras: they stay unhandled
 * (and draw nothing) in the amiga personality. */
static void amiga_ignores_the_ecma_extras(void)
{
    vt_term *t = h_new(4, 1, VT_AMIGA);
    h_put(t, "\033[53;73;11ma");
    CHECK_INT(h_cell(t, 0, 0)->attr & (VT_ATTR_OVERLINE | VT_ATTR_SUPER), 0);
    CHECK_INT(vt_cell_font(t, h_cell(t, 0, 0)), 0);
    CHECK_INT(vt_unhandled(t, 0, 0, 0), 3);
    vt_free(t);
}

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
    vt_color f, b;
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
    vt_color f, b;
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

/* The ROM console (3.1 and 3.2.3): a TAB at the last column wraps only when
 * a TAB took the cursor there. Placed at the last column (a CUP past the
 * edge), the first TAB stays and the second goes to the next line's first
 * stop (rig 3.2, romprobe ht-at-end: CSI 1;75H TAB TAB in a 61-column
 * window -> 2;9; UP-Term gave 2;17). */
static void a_tab_at_the_last_column_wraps_only_after_a_tab(void)
{
    vt_term *t = h_new(61, 5, VT_AMIGA);
    int x, y;
    h_put(t, "\x9b" "1;75H\t\t");      /* CUP clamps to column 61 */
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 1);
    CHECK_INT(x, 8);
    vt_free(t);
    t = h_new(80, 5, VT_AMIGA);
    h_put(t, "\x9b" "1;75H\t\t");      /* the 3.1 rig's case: TAB to 80, TAB wraps */
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 1);
    CHECK_INT(x, 8);
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

/* Reflow on resize (vt_set_reflow), as the ROM's character-mapped units
 * re-wrap linked lines. Measured on the SNIPMAP unit (rig 2026-09-30, all
 * four Kickstarts): 100 characters in 45 columns leave the cursor at 3;11;
 * widened to 90 columns the window shows 90 + 10 and the cursor is at 2;11. */
static const char hundred[] =
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "012345678901234567890123456789";

static void reflow_widening_rewraps_as_the_rom_does(void)
{
    vt_term *t = h_new(45, 10, VT_AMIGA);
    int x, y;
    vt_set_reflow(t, 1);
    h_put(t, hundred);
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 2);
    CHECK_INT(x, 10);
    vt_resize(t, 90, 10);
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 1);                   /* 2;11 in the console's terms */
    CHECK_INT(x, 10);
    CHECK_INT(strlen(h_row(t, 0)), 90);
    CHECK_STR(h_row(t, 1), "0123456789");
    CHECK(vt_row_wrapped(t, 0));
    CHECK(!vt_row_wrapped(t, 1));
    CHECK_STR(h_row(t, 2), "");
    vt_free(t);
}

static void reflow_there_and_back_restores_the_layout(void)
{
    vt_term *t = h_new(45, 10, VT_AMIGA);
    char before[512];
    int x, y;
    vt_set_reflow(t, 1);
    h_put(t, "\x9b" "32mgreen\x9b" "0m\n");
    h_put(t, hundred);
    strcpy(before, h_screen(t));
    vt_resize(t, 90, 10);
    vt_resize(t, 45, 10);
    CHECK_STR(h_screen(t), before);
    CHECK(vt_row_wrapped(t, 1));
    CHECK(vt_row_wrapped(t, 2));
    CHECK(!vt_row_wrapped(t, 3));
    CHECK_INT(h_cell(t, 0, 0)->fg, 2); /* attributes travel with their cells */
    CHECK_INT(h_cell(t, 5, 0)->fg, VT_COLOR_DEFAULT);
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 3);
    CHECK_INT(x, 10);
    vt_resize(t, 30, 10);              /* narrower than it started: 30+30+30+10 */
    CHECK_STR(h_row(t, 4), "0123456789");
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 4);
    CHECK_INT(x, 10);
    vt_free(t);
}

/* The ROM wraps at once, so a line that fills its last column owns the
 * empty row the cursor went to: widened it is one row with the cursor
 * after it, narrowed back the empty row returns. */
static void reflow_line_ending_at_the_margin(void)
{
    vt_term *t = h_new(10, 4, VT_AMIGA);
    int x, y;
    vt_set_reflow(t, 1);
    h_put(t, "0123456789");
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 1);
    CHECK_INT(x, 0);
    vt_resize(t, 20, 4);
    CHECK_STR(h_screen(t), "0123456789");
    CHECK(!vt_row_wrapped(t, 0));
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 0);
    CHECK_INT(x, 10);
    vt_resize(t, 10, 4);
    CHECK(vt_row_wrapped(t, 0));
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 1);
    CHECK_INT(x, 0);
    h_put(t, "a");
    CHECK_STR(h_screen(t), "0123456789|a");
    vt_free(t);
}

static void reflow_never_joins_a_hard_newline(void)
{
    vt_term *t = h_new(10, 4, VT_AMIGA);
    vt_set_reflow(t, 1);
    h_put(t, "abc\ndef\n012345678901");
    CHECK_STR(h_screen(t), "abc|def|0123456789|01");
    vt_resize(t, 20, 4);
    CHECK_STR(h_screen(t), "abc|def|012345678901");
    vt_resize(t, 5, 8);
    CHECK_STR(h_screen(t), "abc|def|01234|56789|01");
    vt_free(t);
}

static void reflow_keeps_the_cursor_on_its_character(void)
{
    vt_term *t = h_new(45, 10, VT_AMIGA);
    int x, y;
    vt_set_reflow(t, 1);
    h_put(t, hundred);
    h_put(t, "\x9b" "2;5H");           /* on the 50th character */
    vt_resize(t, 90, 10);
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 0);
    CHECK_INT(x, 49);
    vt_resize(t, 20, 10);
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 2);
    CHECK_INT(x, 9);
    h_put(t, "X");
    CHECK_INT(h_cell(t, 9, 2)->ch, 'X');
    vt_free(t);
}

/* Off (the default, XCON:): rows are cut or padded as before. */
static void reflow_off_keeps_the_rows(void)
{
    vt_term *t = h_new(45, 10, VT_AMIGA);
    int x, y;
    h_put(t, hundred);
    vt_resize(t, 90, 10);
    CHECK_INT(strlen(h_row(t, 0)), 45);
    CHECK_INT(strlen(h_row(t, 1)), 45);
    CHECK_STR(h_row(t, 2), "0123456789");
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 2);
    CHECK_INT(x, 10);
    vt_free(t);
}

/* The ROM console, measured (DP4 cdprobe NODRAW part, KS 40.63): three
 * 40-character lines in a 47 x 10 charmap unit; the window shrunk to
 * 22 x 4 pushes rows off the top (cursor 4;19); grown back to 47 x 10 every
 * line is there again, cursor 3;41. Rows a resize pushes out come back. */
static void reflow_shrink_and_grow_brings_the_rows_back(void)
{
    static const char x40[] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    vt_term *t = h_new(47, 10, VT_AMIGA);
    int x, y;
    vt_set_reflow(t, 1);
    h_put(t, x40);
    h_put(t, "\n");
    h_put(t, x40);
    h_put(t, "\n");
    h_put(t, x40);
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 2);
    CHECK_INT(x, 40);
    vt_resize(t, 22, 4);
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 3);                   /* 4;19 in the console's terms */
    CHECK_INT(x, 18);
    vt_resize(t, 47, 10);
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 2);                   /* 3;41 */
    CHECK_INT(x, 40);
    CHECK_STR(h_row(t, 0), x40);
    CHECK_STR(h_row(t, 1), x40);
    CHECK_STR(h_row(t, 2), x40);
    vt_free(t);
}

/* Output that scrolls ends it: the rows above are history then, not part
 * of the screen a later resize lays out again. */
static void reflow_output_scroll_forgets_the_pushed_rows(void)
{
    vt_term *t = h_new(10, 3, VT_AMIGA);
    int x, y;
    vt_set_reflow(t, 1);
    h_put(t, "aaaaaaaaaa\nbbb\nccc");
    vt_resize(t, 10, 2);               /* a pushed out */
    h_put(t, "\nddd");                 /* scrolls: b pushed out by output */
    vt_resize(t, 10, 5);
    CHECK_STR(h_row(t, 0), "ccc");
    CHECK_STR(h_row(t, 1), "ddd");
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 1);
    vt_free(t);
}

void suite_amiga(void)
{
    reflow_widening_rewraps_as_the_rom_does();
    reflow_shrink_and_grow_brings_the_rows_back();
    reflow_output_scroll_forgets_the_pushed_rows();
    reflow_there_and_back_restores_the_layout();
    reflow_line_ending_at_the_margin();
    reflow_never_joins_a_hard_newline();
    reflow_keeps_the_cursor_on_its_character();
    reflow_off_keeps_the_rows();
    rom_measured_cursor_motion();
    shift_out_sets_the_high_bit();
    del_is_a_glyph();
    vertical_tab_moves_up();
    a_tab_at_the_last_column_wraps_only_after_a_tab();
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
    amiga_ignores_the_ecma_extras();
}
