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


/* W-T1: a prompt near the top of the screen stays there when the window is
 * resized, as in xterm (resizeGravity north-west) and kitty (screen_resize
 * reflows the screen on its own). The scrollback above a cleared screen is
 * not laid out under the prompt: it used to jump to the bottom. */
static void check_top(vt_term *t, int w, int h, int cy, int sbl)
{
    int x, y;
    vt_resize(t, w, h);
    vt_cursor(t, &x, &y);
    CHECK_STR(h_row(t, 0), "$ ls");
    CHECK_INT(y, cy);
    CHECK_INT(x, 2);
    CHECK_INT(vt_scrollback_lines(t), sbl);
}

static void prompt_near_the_top_stays_there_on_resize(void)
{
    enum vt_personality ps[2];
    int k, i;
    ps[0] = VT_XTERM;
    ps[1] = VT_PCANSI;
    for (k = 0; k < 2; k++) {
        vt_term *t = h_new(40, 10, ps[k]);
        vt_set_reflow(t, 1);
        for (i = 0; i < 30; i++)
            h_put(t, "old line\r\n");
        h_put(t, "\033[H\033[2J$ ls\r\nfile\r\n$ ");
        i = vt_scrollback_lines(t);
        CHECK(i > 0);
        check_top(t, 50, 10, 2, i);
        check_top(t, 50, 14, 2, i);
        check_top(t, 30, 14, 2, i);
        check_top(t, 30, 5, 2, i);
        check_top(t, 20, 12, 2, i);
        vt_free(t);
    }
    /* the Amiga Shell clears with an explicit background pen (ESC[0;31;40m):
     * the blank rows under the prompt are painted, not plain, and still not
     * text */
    {
        vt_term *t = h_new(40, 10, VT_XTERM);
        vt_set_reflow(t, 1);
        for (i = 0; i < 30; i++)
            h_put(t, "old line\r\n");
        h_put(t, "\033[0;31;40m\033[H\033[2J\033[0m$ ls\r\nfile\r\n$ ");
        i = vt_scrollback_lines(t);
        check_top(t, 50, 10, 2, i);
        check_top(t, 50, 14, 2, i);
        check_top(t, 50, 5, 2, i);
        vt_free(t);
    }
    /* the same on a short buffer with an empty scrollback, rows 2 of 24 */
    {
        vt_term *t = reflowing(40, 24);
        h_put(t, "\r\n$ ls\r\nfile\r\n$ ");
        vt_resize(t, 50, 24);
        CHECK_STR(h_row(t, 1), "$ ls");
        vt_resize(t, 50, 30);
        CHECK_STR(h_row(t, 1), "$ ls");
        vt_resize(t, 30, 10);
        CHECK_STR(h_row(t, 1), "$ ls");
        CHECK_INT(vt_scrollback_lines(t), 0);
        vt_free(t);
    }
}

/* ... and a screen that is full keeps its bottom: a taller window brings
 * the scrollback down under it, a shorter one pushes the top off. */
static void full_screen_keeps_its_bottom_on_resize(void)
{
    vt_term *t = reflowing(20, 6);
    int i, x, y, sb;
    for (i = 0; i < 12; i++)
        h_put(t, "line\r\n");
    h_put(t, "$ ");
    sb = vt_scrollback_lines(t);
    CHECK(sb > 3);
    vt_resize(t, 20, 9);
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 8);
    CHECK_INT(vt_scrollback_lines(t), sb - 3);
    CHECK_STR(h_row(t, 8), "$");
    vt_resize(t, 20, 4);
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 3);
    CHECK_INT(vt_scrollback_lines(t), sb + 2);
    vt_free(t);
}

/* W-T5: a line exactly as wide as the screen that went on to the next row, scrolled up so
 * its first row is in the scrollback, then a clear: the scrollback row still says "the text
 * continues on the next line", and the next line is now the cleared screen's first row.
 * A resize joined them ("0123456789new"). The flag belongs to the line below it (kitty keeps
 * it there, as line_attrs.continued of the next line, which erasing the line resets). */
static void a_clear_cuts_the_line_above_the_screen(void)
{
    static const char *const clears[] = {"\033[2J\033[H", "\033[H\033[J", "\033[2J"};
    size_t i;
    vt_term *t;
    for (i = 0; i < sizeof(clears) / sizeof(clears[0]); i++) {
        t = reflowing(10, 3);
        h_put(t, "0123456789abc\r\nx\r\ny");
        CHECK_STR(history(t), "0123456789|abc|x|y");
        CHECK(vt_row_wrapped(t, -1));
        h_put(t, clears[i]);
        h_put(t, "\033[Hnew");
        CHECK(!vt_row_wrapped(t, -1));
        vt_resize(t, 20, 3);
        CHECK_STR(history(t), "0123456789|new");
        vt_free(t);
    }
    /* erasing the first row alone cuts it too (the rows below stay) */
    t = reflowing(10, 3);
    h_put(t, "0123456789abc\r\nx\r\ny\033[H\033[2Knew");
    CHECK(!vt_row_wrapped(t, -1));
    vt_resize(t, 20, 3);
    CHECK(strncmp(history(t), "0123456789|new", 14) == 0);
    vt_free(t);
    /* the join survives a clear that leaves the first row's text: erase only below row 0 */
    t = reflowing(10, 3);
    h_put(t, "0123456789abc\r\nx\r\ny\033[2;1H\033[J");
    CHECK(vt_row_wrapped(t, -1));
    vt_resize(t, 20, 3);
    CHECK_STR(history(t), "0123456789abc");
    vt_free(t);
    /* a full-screen program's clear (alternate screen) leaves the primary screen's join alone */
    t = reflowing(10, 3);
    h_put(t, "0123456789abc\r\nx\r\ny\033[?1049h\033[2J\033[H\033[?1049l");
    CHECK(vt_row_wrapped(t, -1));
    vt_free(t);
}

void suite_reflow(void)
{
    a_clear_cuts_the_line_above_the_screen();
    prompt_near_the_top_stays_there_on_resize();
    full_screen_keeps_its_bottom_on_resize();
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
