/* handler/lineedit: the cooked line editor, typed at and read back through
 * the engine's screen. */
#include "harness.h"
#include "../handler/lineedit.h"

static void to_term(void *u, const unsigned char *b, long n)
{
    vt_write((vt_term *)u, b, n);
}

static le_line le;

static vt_term *start(int cols, int rows, const char *prompt)
{
    vt_term *t = h_new(cols, rows, VT_XTERM);
    vt_set_onlcr(t, 1);
    h_put(t, prompt);
    le_init(&le, t, to_term, t);
    return t;
}

static int type(const char *s)
{
    int done = 0;
    for (; *s; s++)
        done |= le_key(&le, (unsigned char)*s, 0, (const unsigned char *)s, 1);
    return done;
}

static int key(long k, int mods)
{
    unsigned char b[8];
    int n = 0;
    if (k < 0x110000) {
        b[0] = (unsigned char)k;
        n = 1;
    }
    return le_key(&le, k, mods, b, n);
}

static const char *line(void)
{
    static char b[LE_MAX];
    memcpy(b, le.buf, le.len);
    b[le.len] = 0;
    return b;
}

static void editing_inside_the_line(void)
{
    vt_term *t = start(40, 3, "> ");
    int x, y;
    type("hello world");
    key(VT_KEY_LEFT, VT_MOD_SHIFT); /* to the start */
    type("say ");
    CHECK_STR(h_row(t, 0), "> say hello world");
    key(VT_KEY_RIGHT, 0);
    key(VT_KEY_DELETE, 0);
    CHECK_STR(h_row(t, 0), "> say hllo world");
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 7);
    key(VT_KEY_RIGHT, VT_MOD_SHIFT);
    key(VT_KEY_BACKSPACE, 0);
    CHECK_STR(h_row(t, 0), "> say hllo worl");
    CHECK(key(VT_KEY_RETURN, 0));
    CHECK_STR(line(), "say hllo worl\n");
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 1);
    CHECK_INT(x, 0);
    vt_free(t);
}

static void kill_keys(void)
{
    vt_term *t = start(40, 3, "$ ");
    unsigned char c;
    type("one two three");
    c = 0x17; le_key(&le, c, 0, &c, 1); /* Ctrl-W */
    CHECK_STR(h_row(t, 0), "$ one two");
    c = 0x01; le_key(&le, c, 0, &c, 1); /* Ctrl-A */
    key(VT_KEY_RIGHT, 0);
    c = 0x0B; le_key(&le, c, 0, &c, 1); /* Ctrl-K */
    CHECK_STR(h_row(t, 0), "$ o");
    type("ff");
    CHECK_STR(h_row(t, 0), "$ off");
    c = 0x18; le_key(&le, c, 0, &c, 1); /* Ctrl-X */
    CHECK_STR(h_row(t, 0), "$");
    CHECK_INT(le.len, 0);
    vt_free(t);
}

static void a_long_line_wraps_and_edits_across_rows(void)
{
    vt_term *t = start(10, 4, "> ");
    type("abcdefghijklmnop"); /* 2 + 16 = 18 cells: two rows */
    CHECK_STR(h_screen(t), "> abcdefgh|ijklmnop");
    key(VT_KEY_LEFT, VT_MOD_SHIFT);
    type("XY");
    CHECK_STR(h_screen(t), "> XYabcdef|ghijklmnop");
    key(VT_KEY_RIGHT, VT_MOD_SHIFT);
    key(VT_KEY_BACKSPACE, 0);
    key(VT_KEY_BACKSPACE, 0);
    key(VT_KEY_BACKSPACE, 0);
    CHECK_STR(h_screen(t), "> XYabcdef|ghijklm");
    vt_free(t);
}

static void a_line_that_fills_the_bottom_row_exactly(void)
{
    vt_term *t = start(10, 2, "\n> ");
    int x, y;
    type("12345678"); /* exactly to the last column of the bottom row */
    key(VT_KEY_LEFT, 0);
    key(VT_KEY_RIGHT, 0); /* back to after the last cell: the deferred wrap */
    type("9");
    CHECK_STR(h_screen(t), "> 12345678|9");
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 1);
    CHECK_INT(x, 1);
    vt_free(t);
}

static void history_and_prefix_search(void)
{
    vt_term *t = start(40, 5, "> ");
    type("list ram:");
    key(VT_KEY_RETURN, 0);
    le_reset(&le);
    h_put(t, "> ");
    type("echo hi");
    key(VT_KEY_RETURN, 0);
    le_reset(&le);
    h_put(t, "> ");
    key(VT_KEY_UP, 0);
    CHECK_STR(line(), "echo hi");
    key(VT_KEY_UP, 0);
    CHECK_STR(line(), "list ram:");
    CHECK_STR(h_row(t, 2), "> list ram:");
    key(VT_KEY_DOWN, 0);
    key(VT_KEY_DOWN, 0);
    CHECK_STR(line(), "");
    type("li");
    key(VT_KEY_UP, VT_MOD_SHIFT); /* entries starting with "li" */
    CHECK_STR(line(), "list ram:");
    CHECK_INT(le.pos, 2);
    vt_free(t);
}

static void utf8_characters_move_as_one(void)
{
    vt_term *t = start(20, 2, "> ");
    type("a\xc3\xa5z");
    key(VT_KEY_LEFT, 0);
    key(VT_KEY_BACKSPACE, 0);
    CHECK_STR(line(), "az");
    CHECK_STR(h_row(t, 0), "> az");
    vt_free(t);
}

void suite_lineedit(void)
{
    editing_inside_the_line();
    kill_keys();
    a_long_line_wraps_and_edits_across_rows();
    a_line_that_fills_the_bottom_row_exactly();
    history_and_prefix_search();
    utf8_characters_move_as_one();
}
