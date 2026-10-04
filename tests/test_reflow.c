/* Reflow on resize with a scrollback (ledger W1, gaps #11): the scrollback
 * and the screen re-wrap as one text, the way iTerm2, kitty and
 * Terminal.app do; the window's default since 2026-10-04. The ROM-console
 * reflow without a scrollback is suite_amiga's. */
#include "harness.h"

/* Every row from the oldest scrollback line to the screen's last, '|'
 * between rows, trailing blank rows cut. */
static const char *history(vt_term *t)
{
    static char buf[16384];
    int y, len = 0, keep = 0;
    for (y = -vt_scrollback_lines(t); y < vt_rows(t); y++) {
        const char *r = h_row(t, y);
        int n = (int)strlen(r);
        if (y > -vt_scrollback_lines(t))
            buf[len++] = '|';
        memcpy(buf + len, r, n);
        len += n;
        if (n)
            keep = len;
    }
    buf[keep] = 0;
    return buf;
}

static vt_term *reflowing(int cols, int rows)
{
    vt_term *t = h_new(cols, rows, VT_XTERM);
    vt_set_reflow(t, 1);
    return t;
}

static void scrollback_lines_rewrap_with_the_screen(void)
{
    vt_term *t = reflowing(10, 3);
    int x, y;
    h_put(t, "0123456789abcde\r\nx\r\ny\r\nz");
    CHECK_STR(history(t), "0123456789|abcde|x|y|z");
    CHECK_INT(vt_scrollback_lines(t), 2);
    vt_resize(t, 20, 3);
    CHECK_INT(vt_scrollback_lines(t), 1);
    CHECK_STR(h_row(t, -1), "0123456789abcde");
    CHECK(!vt_row_wrapped(t, -1));
    CHECK_STR(h_screen(t), "x|y|z");
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 1);
    CHECK_INT(y, 2);
    vt_resize(t, 5, 3);
    CHECK_STR(history(t), "01234|56789|abcde|x|y|z");
    CHECK(vt_row_wrapped(t, -3));
    CHECK(vt_row_wrapped(t, -2));
    CHECK(!vt_row_wrapped(t, -1));
    vt_resize(t, 10, 3);
    CHECK_STR(history(t), "0123456789|abcde|x|y|z");
    vt_free(t);
}

/* A line whose start scrolled into the scrollback joins its rest. */
static void line_split_between_scrollback_and_screen_joins(void)
{
    vt_term *t = reflowing(10, 3);
    int x, y;
    h_put(t, "AAAAAAAAAABBBBBBBBBBCCCCC\r\nz");
    CHECK_STR(h_row(t, -1), "AAAAAAAAAA");
    CHECK_STR(h_screen(t), "BBBBBBBBBB|CCCCC|z");
    vt_resize(t, 30, 3);
    CHECK_INT(vt_scrollback_lines(t), 0);
    CHECK_STR(h_screen(t), "AAAAAAAAAABBBBBBBBBBCCCCC|z");
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 1);
    CHECK_INT(y, 1);
    vt_free(t);
}

/* A taller window brings rows down from the scrollback; grid row +
 * vt_lines_scrolled still names the same line. */
static void taller_window_brings_rows_back(void)
{
    vt_term *t = reflowing(10, 3);
    long base;
    int x, y;
    h_put(t, "1\r\n2\r\n3\r\n4\r\n5");
    CHECK_STR(h_screen(t), "3|4|5");
    base = vt_lines_scrolled(t);
    vt_resize(t, 10, 5);
    CHECK_STR(h_screen(t), "1|2|3|4|5");
    CHECK_INT(vt_scrollback_lines(t), 0);
    CHECK_INT(vt_lines_scrolled(t), base - 2);
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 4);
    vt_resize(t, 10, 3);
    CHECK_STR(h_screen(t), "3|4|5");
    CHECK_INT(vt_scrollback_lines(t), 2);
    CHECK_INT(vt_lines_scrolled(t), base);
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 2);
    CHECK_INT(x, 1);
    vt_free(t);
}

static void wide_glyphs_rewrap_without_their_padding(void)
{
    vt_term *t = reflowing(10, 4);
    /* 9 cells, then a wide glyph that does not fit: a blank pads column 9 */
    h_put(t, "123456789\xe6\x97\xa5\xe6\x9c\xac");
    CHECK_STR(h_screen(t), "123456789|\xe6\x97\xa5\xe6\x9c\xac");
    vt_resize(t, 20, 4);
    CHECK_STR(h_screen(t), "123456789\xe6\x97\xa5\xe6\x9c\xac");
    CHECK_INT(h_cell(t, 9, 0)->width, 2);
    CHECK_INT(h_cell(t, 10, 0)->width, 0);
    CHECK_INT(h_cell(t, 11, 0)->width, 2);
    vt_resize(t, 5, 4);
    CHECK_STR(h_screen(t), "12345|6789|\xe6\x97\xa5\xe6\x9c\xac");
    vt_resize(t, 10, 4);
    CHECK_STR(h_screen(t), "123456789|\xe6\x97\xa5\xe6\x9c\xac");
    CHECK(vt_row_wrapped(t, 0));
    vt_free(t);
}

/* A line that filled its last column and then ended (CR LF) is one line:
 * it never joins the next, at any width. */
static void line_ending_at_the_last_column(void)
{
    vt_term *t = reflowing(10, 4);
    h_put(t, "0123456789\r\nnext");
    CHECK(!vt_row_wrapped(t, 0));
    vt_resize(t, 5, 4);
    CHECK_STR(h_screen(t), "01234|56789|next");
    CHECK(vt_row_wrapped(t, 0));
    CHECK(!vt_row_wrapped(t, 1));
    vt_resize(t, 20, 4);
    CHECK_STR(h_screen(t), "0123456789|next");
    CHECK(!vt_row_wrapped(t, 0));
    vt_free(t);
}

static void cursor_on_a_wrapped_line_stays_on_its_character(void)
{
    vt_term *t = reflowing(10, 4);
    int x, y;
    h_put(t, "abcdefghijklmnopqrst\033[2;3H"); /* on the 'm' */
    vt_resize(t, 7, 4);
    CHECK_STR(h_screen(t), "abcdefg|hijklmn|opqrst");
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 5);
    CHECK_INT(y, 1);
    h_put(t, "X");
    vt_resize(t, 20, 4);
    CHECK_STR(h_screen(t), "abcdefghijklXnopqrst");
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 13);
    CHECK_INT(y, 0);
    vt_free(t);
}

/* Narrower lines need more rows than the scrollback holds: the newest
 * stay, the oldest go, nothing breaks. */
static void full_scrollback_keeps_the_newest_rows(void)
{
    vt_term *t = reflowing(10, 3);
    int i;
    char line[16];
    CHECK(vt_set_scrollback(t, 4));
    for (i = 1; i <= 8; i++) {
        strcpy(line, "\r\nL0abcdefgh");
        line[3] = (char)('0' + i);
        h_put(t, i > 1 ? line : line + 2);
    }
    CHECK_INT(vt_scrollback_lines(t), 4);
    vt_resize(t, 5, 3);
    CHECK_INT(vt_scrollback_lines(t), 4);
    CHECK_STR(history(t), "defgh|L6abc|defgh|L7abc|defgh|L8abc|defgh");
    vt_resize(t, 10, 3);
    CHECK_STR(history(t), "defgh|L6abcdefgh|L7abcdefgh|L8abcdefgh");
    /* the re-wrapped rows are reused as blank ones once the full ring hands
     * them back: their written cells must be known (vt_line.used), or the
     * clear leaves text behind (VT_CHECK_USED aborts on it) */
    h_put(t, "\r\n1\r\n2\r\n3\r\n4\r\n5\r\n6\r\n7\r\n8");
    CHECK_STR(h_screen(t), "6|7|8");
    CHECK_STR(history(t), "2|3|4|5|6|7|8");
    vt_free(t);
}

/* Shrink, then grow back: every row, colour and the cursor as before. */
static void shrink_then_grow_is_a_round_trip(void)
{
    vt_term *t = reflowing(30, 6);
    char before[4096];
    int x0, y0, x, y;
    h_put(t, "\033[31mred\033[m plain text that runs on past the right edge of it\r\n"
             "short\r\n"
             "\xe6\x97\xa5\xe6\x9c\xac wide wide wide wide wide wide wide wide\r\n"
             "123456789012345678901234567890\r\n"
             "$ ");
    strcpy(before, history(t));
    vt_cursor(t, &x0, &y0);
    vt_resize(t, 7, 6);
    vt_resize(t, 13, 4);
    vt_resize(t, 80, 10);
    vt_resize(t, 30, 6);
    CHECK_STR(history(t), before);
    vt_cursor(t, &x, &y);
    CHECK_INT(x, x0);
    CHECK_INT(y, y0);
    {
        int n;
        const vt_cell *c = vt_row(t, -vt_scrollback_lines(t), &n);
        CHECK(c != 0);
        if (c) {
            CHECK_INT(c[0].fg, 1); /* the colour travelled with its cells */
            CHECK_INT(c[3].fg, VT_COLOR_DEFAULT);
        }
    }
    h_put(t, "ls\r\nout");
    CHECK_STR(h_row(t, vt_rows(t) - 1), "out");
    vt_free(t);
}

/* The alternate screen is cut or padded; behind it the primary screen and
 * the scrollback reflow, and come back re-wrapped. */
static void alternate_screen_waits_while_history_reflows(void)
{
    vt_term *t = reflowing(10, 2);
    h_put(t, "0123456789abc\r\nend\033[?1049h\033[HALT");
    vt_resize(t, 20, 2);
    CHECK_STR(h_screen(t), "ALT");
    h_put(t, "\033[?1049l");
    CHECK_STR(h_screen(t), "0123456789abc|end");
    CHECK_INT(vt_scrollback_lines(t), 0);
    vt_free(t);
}

void suite_reflow(void)
{
    scrollback_lines_rewrap_with_the_screen();
    line_split_between_scrollback_and_screen_joins();
    taller_window_brings_rows_back();
    wide_glyphs_rewrap_without_their_padding();
    line_ending_at_the_last_column();
    cursor_on_a_wrapped_line_stays_on_its_character();
    full_scrollback_keeps_the_newest_rows();
    shrink_then_grow_is_a_round_trip();
    alternate_screen_waits_while_history_reflows();
}
