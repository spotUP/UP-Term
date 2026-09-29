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

static void ctrl(unsigned char c)
{
    le_key(&le, c, 0, &c, 1);
}

static void run(const char *s)
{
    type(s);
    key(VT_KEY_RETURN, 0);
    le_reset(&le);
    h_put(le.t, "> ");
}

static void suggestion_shows_grey_and_right_takes_it(void)
{
    vt_term *t = start(40, 6, "> ");
    run("list SYS:Prefs");
    type("li");
    CHECK_STR(h_row(t, 1), "> list SYS:Prefs");   /* the grey tail is on screen */
    CHECK(h_cell(t, 4, 1)->attr & VT_ATTR_FAINT); /* ...and it is faint */
    CHECK(!(h_cell(t, 3, 1)->attr & VT_ATTR_FAINT));
    CHECK_STR(line(), "li");                       /* but not in the line */
    key(VT_KEY_RIGHT, 0);                          /* Right at the end takes it */
    CHECK_STR(line(), "list SYS:Prefs");
    CHECK(!(h_cell(t, 4, 1)->attr & VT_ATTR_FAINT));
    vt_free(t);
}

static void return_does_not_run_the_suggestion(void)
{
    vt_term *t = start(40, 6, "> ");
    run("echo long command");
    type("ec");
    CHECK(key(VT_KEY_RETURN, 0));
    CHECK_STR(line(), "ec\n");
    CHECK_STR(h_row(t, 1), "> ec"); /* the grey tail was wiped */
    vt_free(t);
}

static void ctrl_r_searches_history(void)
{
    vt_term *t = start(50, 6, "> ");
    run("copy a b");
    run("list SYS:");
    run("copy c d");
    ctrl(0x12);
    type("co");
    CHECK_STR(line(), "copy c d");
    ctrl(0x12); /* again: older */
    CHECK_STR(line(), "copy a b");
    CHECK(strstr(h_row(t, 3), "(search: co)") != 0);
    key(VT_KEY_RIGHT, 0); /* any movement keeps the found line */
    CHECK_STR(line(), "copy a b");
    CHECK(strstr(h_row(t, 3), "search") == 0);
    vt_free(t);
}

static void ctrl_r_cancel_restores(void)
{
    vt_term *t = start(50, 6, "> ");
    run("dir ram:");
    type("typed");
    ctrl(0x12);
    type("dir");
    CHECK_STR(line(), "dir ram:");
    ctrl(0x07); /* Ctrl-G */
    CHECK_STR(line(), "typed");
    vt_free(t);
}

static void word_motions_and_undo(void)
{
    vt_term *t = start(50, 4, "> ");
    type("copy from to");
    key(VT_KEY_LEFT, VT_MOD_CTRL);
    CHECK_INT(le.pos, 10);
    key(VT_KEY_LEFT, VT_MOD_CTRL);
    CHECK_INT(le.pos, 5);
    le_key(&le, 'd', VT_MOD_ALT, (const unsigned char *)"\033d", 2); /* Meta-D */
    CHECK_STR(line(), "copy  to");
    ctrl(0x1F); /* undo */
    CHECK_STR(line(), "copy from to");
    CHECK_STR(h_row(t, 0), "> copy from to");
    vt_free(t);
}

static void ctrl_l_clears_and_keeps_prompt_and_line(void)
{
    vt_term *t = start(40, 5, "junk\r\nmore junk\r\n1.SYS:> ");
    type("echo hi");
    ctrl(0x0C);
    CHECK_STR(h_screen(t), "1.SYS:> echo hi");
    type("!");
    CHECK_STR(line(), "echo hi!");
    CHECK_STR(h_row(t, 0), "1.SYS:> echo hi!");
    vt_free(t);
}

void suite_lineedit(void)
{
    suggestion_shows_grey_and_right_takes_it();
    return_does_not_run_the_suggestion();
    ctrl_r_searches_history();
    ctrl_r_cancel_restores();
    word_motions_and_undo();
    ctrl_l_clears_and_keeps_prompt_and_line();
    editing_inside_the_line();
    kill_keys();
    a_long_line_wraps_and_edits_across_rows();
    a_line_that_fills_the_bottom_row_exactly();
    history_and_prefix_search();
    utf8_characters_move_as_one();
}
