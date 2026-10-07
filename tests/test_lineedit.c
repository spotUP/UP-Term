/* handler/lineedit: the cooked line editor, typed at and read back through
 * the engine's screen. */
#include "harness.h"
#include "../handler/lineedit.h"

static long colour_sgrs; /* the command word's green / red written (W44) */

static void to_term(void *u, const unsigned char *b, long n)
{
    if (n == 5 && (!memcmp(b, "\033[31m", 5) || !memcmp(b, "\033[32m", 5)))
        colour_sgrs++;
    vt_write((vt_term *)u, b, n);
}

static le_line le;

static vt_term *start(int cols, int rows, const char *prompt)
{
    vt_term *t = h_new(cols, rows, VT_XTERM);
    vt_set_onlcr(t, 1);
    h_put(t, prompt);
    le_free(&le); /* the last test's history and undo blocks */
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

/* A prompt with wide characters and emoji (a zsh/starship prompt, an
 * icon from a Nerd Font) comes back as it was: no blank for the right
 * half of a wide character, the emoji whole. */
static void ctrl_l_keeps_a_prompt_beyond_the_bmp(void)
{
    vt_term *t = start(40, 5, "junk\r\n\xe4\xb8\xad\xf0\x9f\x98\x80\xf3\xb0\x80\x81> ");
    type("ls");
    ctrl(0x0C);
    CHECK_STR(h_screen(t), "\xe4\xb8\xad\xf0\x9f\x98\x80\xf3\xb0\x80\x81> ls");
    vt_free(t);
}

static void command_word_gets_colour_until_it_changes(void)
{
    vt_term *t = start(40, 4, "> ");
    type("list ram:");
    le_set_command(&le, (const unsigned char *)"list", 1);
    CHECK_INT(h_cell(t, 2, 0)->fg, 2);  /* green */
    CHECK_INT(h_cell(t, 7, 0)->fg, VT_COLOR_DEFAULT); /* arguments stay plain */
    le_set_command(&le, (const unsigned char *)"lis", 0); /* a stale answer */
    CHECK_INT(h_cell(t, 2, 0)->fg, 2);
    key(VT_KEY_LEFT, VT_MOD_SHIFT);
    type("x");                           /* the word changed: colour drops */
    CHECK_STR(h_row(t, 0), "> xlist ram:");
    CHECK_INT(h_cell(t, 3, 0)->fg, VT_COLOR_DEFAULT);
    le_set_command(&le, (const unsigned char *)"xlist", 0);
    CHECK_INT(h_cell(t, 3, 0)->fg, VT_COLOR_DEFAULT); /* typed: kept until the keys rest */
    CHECK_INT(le_command_rested(&le, LE_CMD_REST_US), 1);
    CHECK_INT(h_cell(t, 3, 0)->fg, 1);  /* red */
    vt_free(t);
}

/* One key of a word typed in a Shell: the handler asks about the first
 * word at every key and the command cache (W22) answers at once -- known
 * for `known`, not for the prefixes before it. */
static void type_asked(const char *s, const char *known, long frame_us)
{
    unsigned char w[64];
    for (; *s; s++) {
        le_key(&le, (unsigned char)*s, 0, (const unsigned char *)s, 1);
        le_first_word(&le, w, sizeof(w));
        if (w[0])
            le_set_command(&le, w, !strcmp((const char *)w, known));
        if (frame_us)
            le_command_rested(&le, frame_us); /* the frame clock ticked before the next key */
    }
}

/* W44 (owner 2026-10-05: "it changes back and forth during typing"): a
 * word typed quickly stays plain; its colour comes once, 300 ms after the
 * last key -- it was red at l, li, lis and green at list. */
static void typing_a_word_quickly_colours_it_once_when_the_keys_rest(void)
{
    vt_term *t = start(40, 4, "> ");
    long rested = 60000L;
    int x, y;
    colour_sgrs = 0;
    type_asked("list", "list", 60000L); /* a key every 60 ms */
    CHECK_STR(h_row(t, 0), "> list");
    CHECK_INT(colour_sgrs, 0);
    CHECK_INT(h_cell(t, 2, 0)->fg, VT_COLOR_DEFAULT);
    while (!le_command_rested(&le, 20000L) && rested < 1000000L) {
        rested += 20000L; /* 20 ms frames: still nothing */
        CHECK_INT(colour_sgrs, 0);
        CHECK_INT(h_cell(t, 5, 0)->fg, VT_COLOR_DEFAULT);
    }
    CHECK_INT(rested + 20000L, LE_CMD_REST_US); /* coloured 300 ms after the last key */
    CHECK_INT(LE_CMD_REST_US, 300000L);
    CHECK_INT(colour_sgrs, 1);                  /* one change: plain to green */
    CHECK_INT(h_cell(t, 2, 0)->fg, 2);
    CHECK_INT(h_cell(t, 5, 0)->fg, 2);
    CHECK_STR(h_row(t, 0), "> list");
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 6);
    CHECK_INT(le_command_rested(&le, 300000L), 0); /* nothing more due */
    CHECK_INT(colour_sgrs, 1);
    vt_free(t);
}

/* The word is finished -- a space, Return -- and its colour shows at once,
 * no rest; an answer that comes after the space too. */
static void a_finished_word_is_coloured_at_once(void)
{
    vt_term *t = start(40, 4, "> ");
    colour_sgrs = 0;
    type_asked("list", "list", 30000L);
    CHECK_INT(colour_sgrs, 0);
    type(" ");                          /* the space: no clock tick */
    CHECK_INT(colour_sgrs, 1);
    CHECK_INT(h_cell(t, 2, 0)->fg, 2);
    CHECK_INT(h_cell(t, 6, 0)->fg, VT_COLOR_DEFAULT);
    CHECK_STR(h_row(t, 0), "> list");
    le_reset(&le);
    h_put(t, "\r\n> ");
    colour_sgrs = 0;
    type("dir ");                       /* the answer is still out */
    le_set_command(&le, (const unsigned char *)"dir", 1);
    CHECK_INT(colour_sgrs, 1);
    CHECK_INT(h_cell(t, 2, 1)->fg, 2);
    le_reset(&le);
    h_put(t, "\r\n> ");
    colour_sgrs = 0;
    type_asked("dirx", "dir", 30000L);
    CHECK(key(VT_KEY_RETURN, 0));       /* Return: red before the line goes */
    CHECK_INT(colour_sgrs, 1);
    CHECK_INT(h_cell(t, 2, 2)->fg, 1);
    CHECK_STR(h_row(t, 2), "> dirx");
    vt_free(t);
}

/* A word already coloured keeps its colour while the rest of the line is
 * typed and edited: no flip, its cells not written again. */
static void a_known_word_does_not_flip_while_typing_goes_on(void)
{
    vt_term *t = start(40, 4, "> ");
    const char *rest = " ram:";
    char one[2];
    colour_sgrs = 0;
    type_asked("list", "list", 60000L);
    CHECK_INT(le_command_rested(&le, LE_CMD_REST_US), 1);
    CHECK_INT(colour_sgrs, 1);
    CHECK_INT(h_cell(t, 2, 0)->fg, 2);
    one[1] = 0;
    for (; *rest; rest++) {
        one[0] = *rest;
        type_asked(one, "list", 60000L);
        CHECK_INT(h_cell(t, 2, 0)->fg, 2);
        CHECK_INT(colour_sgrs, 1);
    }
    key(VT_KEY_LEFT, 0);
    key(VT_KEY_BACKSPACE, 0);           /* an edit in the arguments */
    CHECK_STR(h_row(t, 0), "> list ra:");
    CHECK_INT(le_command_rested(&le, LE_CMD_REST_US), 0);
    CHECK(key(VT_KEY_RETURN, 0));
    CHECK_INT(colour_sgrs, 1);          /* green once, from l to Return */
    CHECK_INT(h_cell(t, 5, 0)->fg, 2);
    CHECK_INT(h_cell(t, 7, 0)->fg, VT_COLOR_DEFAULT);
    vt_free(t);
}

/* W31: a program's cooked read (Ask, C:Claude PLAIN) is not a command
 * line: the word's colour goes and the text stays. */
static void a_programs_line_gets_no_command_colour(void)
{
    vt_term *t = start(40, 4, "? ");
    type("hello there");
    le_set_command(&le, (const unsigned char *)"hello", 0);
    CHECK_INT(h_cell(t, 2, 0)->fg, 1);  /* red, as before the fix */
    le_no_command(&le);
    CHECK_INT(h_cell(t, 2, 0)->fg, VT_COLOR_DEFAULT);
    CHECK_STR(h_row(t, 0), "? hello there");
    vt_free(t);
}

static void menu_lists_names_and_redraws_prompt_and_line(void)
{
    vt_term *t = start(30, 8, "1.SYS:> ");
    type("li");
    le_show_list(&le, "list\0lister\0lister2\0", 20);
    CHECK_STR(h_row(t, 1), "list     lister   lister2");
    CHECK_STR(h_row(t, 2), "1.SYS:> li");
    type("st");
    CHECK_STR(h_row(t, 2), "1.SYS:> list");
    vt_free(t);
}

/* W30: /theme with no name -- the themes under the finished line, one a
 * row, chosen with the arrows and Return, Escape leaving the screen as it
 * was. */
static int menu_key(le_menu *m, long k, int mods, const char *b)
{
    return le_menu_key(m, k, mods, (const unsigned char *)b, b ? (int)strlen(b) : 0);
}

static void theme_menu_chosen_with_arrows_and_return(void)
{
    static const char names[] = "ayu-dark\0dracula\0nord\0solarized\0";
    le_menu m;
    vt_term *t = start(40, 8, "1.SYS:> ");
    type("/theme");
    CHECK(key(VT_KEY_RETURN, 0));
    le_reset(&le);
    memset(&m, 0, sizeof(m));
    le_menu_open(&le, &m, names, 4, 2, 2);  /* the window has nord: on it, marked */
    CHECK_STR(h_row(t, 0), "1.SYS:> /theme");
    CHECK_STR(h_row(t, 1), "  ayu-dark");
    CHECK_STR(h_row(t, 2), "  dracula");
    CHECK_STR(h_row(t, 3), "* nord");
    CHECK_STR(h_row(t, 4), "  solarized");
    CHECK_STR(h_row(t, 5), "3 of 4: Up/Down choose, Enter applies,");
    CHECK(h_cell(t, 2, 3)->attr & VT_ATTR_INVERSE);
    CHECK(h_cell(t, 11, 3)->attr & VT_ATTR_INVERSE);  /* the bar: the widest name and one */
    CHECK(!(h_cell(t, 2, 2)->attr & VT_ATTR_INVERSE));
    /* the keys: state only */
    CHECK_INT(menu_key(&m, VT_KEY_DOWN, 0, 0), LE_MENU_MOVED);
    CHECK_INT(m.sel, 3);
    CHECK_INT(menu_key(&m, VT_KEY_DOWN, 0, 0), LE_MENU_MOVED);
    CHECK_INT(m.sel, 0);                              /* wraps to the top */
    CHECK_INT(menu_key(&m, VT_KEY_UP, 0, 0), LE_MENU_MOVED);
    CHECK_INT(m.sel, 3);                              /* and back to the bottom */
    CHECK_INT(menu_key(&m, VT_KEY_HOME, 0, 0), LE_MENU_MOVED);
    CHECK_INT(menu_key(&m, VT_KEY_HOME, 0, 0), LE_MENU_NONE);
    CHECK_INT(menu_key(&m, 0, 0, "D"), LE_MENU_MOVED);
    CHECK_INT(m.sel, 1);                              /* a letter: the next starting so */
    CHECK_INT(menu_key(&m, VT_KEY_DOWN, VT_MOD_SHIFT, 0), LE_MENU_MOVED);
    CHECK_INT(m.sel, 3);                              /* a page, stopping at the end */
    CHECK_INT(menu_key(&m, VT_KEY_LEFT, 0, 0), LE_MENU_NONE);
    le_menu_draw(&le, &m);
    CHECK(h_cell(t, 2, 4)->attr & VT_ATTR_INVERSE);
    CHECK(!(h_cell(t, 2, 3)->attr & VT_ATTR_INVERSE));
    CHECK_STR(h_row(t, 3), "* nord");
    CHECK_INT(menu_key(&m, VT_KEY_RETURN, 0, "\r"), LE_MENU_TAKE);
    CHECK_INT(menu_key(&m, 0, 0, "\r"), LE_MENU_TAKE);
    CHECK_INT(menu_key(&m, VT_KEY_ESCAPE, 0, "\033"), LE_MENU_CANCEL);
    CHECK_INT(menu_key(&m, 0, 0, "\033"), LE_MENU_CANCEL);
    CHECK_STR(le_menu_name(&m, m.sel), "solarized");
    /* closed: the rows gone, the cursor where the reader's prompt goes */
    le_menu_close(&le, &m);
    CHECK(!m.open);
    CHECK_STR(h_screen(t), "1.SYS:> /theme");
    {
        int x, y;
        vt_cursor(t, &x, &y);
        CHECK_INT(x, 0);
        CHECK_INT(y, 1);
    }
    vt_free(t);
}

static void theme_menu_scrolls_a_long_list_at_the_bottom(void)
{
    char names[40 * 4];
    int i, n = 0, x, y;
    le_menu m;
    vt_term *t = start(30, 6, "> ");
    for (i = 0; i < 40; i++) {                        /* t00 .. t39 */
        names[n++] = 't';
        names[n++] = (char)('0' + i / 10);
        names[n++] = (char)('0' + i % 10);
        names[n++] = 0;
    }
    h_put(t, "\r\n\r\n\r\n\r\n> ");                    /* the prompt on the bottom row */
    type("/theme");
    key(VT_KEY_RETURN, 0);
    le_reset(&le);
    memset(&m, 0, sizeof(m));
    le_menu_open(&le, &m, names, 40, 0, -1);
    CHECK_INT(m.rows, 4);                             /* 6 rows: the line and the help */
    CHECK_STR(h_row(t, 0), "> /theme");               /* the screen moved up for it */
    CHECK_STR(h_row(t, 1), "  t00");
    CHECK_STR(h_row(t, 4), "  t03");
    CHECK_STR(h_row(t, 5), "1 of 40: Up/Down choose, Ente");
    CHECK_INT(menu_key(&m, VT_KEY_END, 0, 0), LE_MENU_MOVED);
    le_menu_draw(&le, &m);
    CHECK_STR(h_row(t, 1), "  t36");
    CHECK_STR(h_row(t, 4), "  t39");
    CHECK(h_cell(t, 2, 4)->attr & VT_ATTR_INVERSE);
    vt_cursor(t, &x, &y);
    CHECK_INT(y, 4);                                  /* on the chosen row */
    le_menu_close(&le, &m);
    CHECK_STR(h_screen(t), "> /theme");
    vt_free(t);
}

/* Owner 2026-10-05: "the whole screen redraws when i navigate the theme
 * list". Each key redrew the whole list, and /theme put each theme passed
 * on the window -- a full repaint a key. Now a move draws the row the bar
 * left, the row it is on and the "k of n" row (a scroll moves the rows and
 * draws the one coming in), and the preview waits for the bar to rest. */
static long menu_rows_drawn;
static void count_rows(void *u, const unsigned char *b, long n)
{
    long i;
    for (i = 0; i + 3 < n; i++)
        if (b[i] == 0x1B && b[i + 1] == '[' && b[i + 2] == '2' && b[i + 3] == 'K')
            menu_rows_drawn++; /* each row of the list starts with EL 2 */
    vt_write((vt_term *)u, b, n);
}

static vt_term *menu_start(int cols, int rows, enum vt_personality p, const char *prompt)
{
    vt_term *t = h_new(cols, rows, p);
    vt_set_onlcr(t, 1);
    h_put(t, prompt);
    le_free(&le);
    le_init(&le, t, count_rows, t);
    type("/theme");
    key(VT_KEY_RETURN, 0);
    le_reset(&le);
    menu_rows_drawn = 0;
    return t;
}

static void menu_names(char *names, int n) /* t00 .. */
{
    int i, k = 0;
    for (i = 0; i < n; i++) {
        names[k++] = 't';
        names[k++] = (char)('0' + i / 10);
        names[k++] = (char)('0' + i % 10);
        names[k++] = 0;
    }
}

/* every cell's character and whether it is reversed */
static void menu_snap(vt_term *t, unsigned long *s)
{
    int x, y, k = 0;
    for (y = 0; y < vt_rows(t); y++)
        for (x = 0; x < vt_cols(t); x++) {
            const vt_cell *c = h_cell(t, x, y);
            s[k++] = vt_cell_char(t, c) * 2UL + ((c->attr & VT_ATTR_INVERSE) != 0);
        }
}

/* The screen as the rows left it is the screen a whole draw makes. */
static int menu_as_drawn_whole(vt_term *t, le_menu *m)
{
    static unsigned long a[40 * 24], b[40 * 24];
    long drawn = menu_rows_drawn;
    int x0, y0, x1, y1;
    menu_snap(t, a);
    vt_cursor(t, &x0, &y0);
    m->drawn_top = -1; /* the next draw is whole */
    le_menu_draw(&le, m);
    menu_snap(t, b);
    vt_cursor(t, &x1, &y1);
    menu_rows_drawn = drawn;
    return !memcmp(a, b, sizeof(unsigned long) * vt_cols(t) * vt_rows(t)) && x0 == x1 && y0 == y1;
}

static void holding_down_in_the_theme_list_draws_rows_and_previews_once(void)
{
    char names[40 * 4];
    le_menu m;
    int i, previews = 0, keys = 30;
    vt_term *t;
    menu_names(names, 40);
    t = menu_start(30, 16, VT_XTERM, "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n> ");
    memset(&m, 0, sizeof(m));
    le_menu_open(&le, &m, names, 40, 0, 0);
    CHECK_INT(m.rows, 12);
    CHECK_STR(h_row(t, 15), "1 of 40: Up/Down choose, Ente"); /* at the bottom */
    CHECK_INT(menu_rows_drawn, 13);                               /* opened: drawn whole */
    menu_rows_drawn = 0;
    h_scroll_calls = 0;
    /* Down held: a key every 30 ms on the caller's clock */
    for (i = 0; i < keys; i++) {
        CHECK_INT(menu_key(&m, VT_KEY_DOWN, 0, 0), LE_MENU_MOVED);
        le_menu_draw(&le, &m);
        previews += le_menu_rested(&m, 30000L);
    }
    CHECK_INT(previews, 0);                    /* none while the keys come */
    CHECK_INT(menu_rows_drawn, 3 * keys);      /* the left row, the bar's row, "k of n": was 13 a key */
    CHECK_INT(h_scroll_calls > 0, 1);          /* past the 12th the names moved, not drawn again */
    for (i = 0; i < 20; i++)
        previews += le_menu_rested(&m, 20000L); /* the keys stopped */
    CHECK_INT(previews, 1);                    /* one preview, for where the bar rests: was one a key */
    CHECK_INT(m.sel, 30);
    CHECK_STR(h_row(t, 3), "  t19");
    CHECK_STR(h_row(t, 14), "  t30");
    CHECK(h_cell(t, 2, 14)->attr & VT_ATTR_INVERSE);
    CHECK(!(h_cell(t, 2, 13)->attr & VT_ATTR_INVERSE));
    CHECK_STR(h_row(t, 15), "31 of 40: Up/Down choose, Ent");
    CHECK(menu_as_drawn_whole(t, &m));
    /* slow keys: each rest previews */
    previews = 0;
    for (i = 0; i < 2; i++) {
        menu_key(&m, VT_KEY_UP, 0, 0);
        le_menu_draw(&le, &m);
        previews += le_menu_rested(&m, 300000L);
    }
    CHECK_INT(previews, 2);
    /* Escape or Enter with a preview still waiting: the close cancels it */
    menu_key(&m, VT_KEY_DOWN, 0, 0);
    le_menu_draw(&le, &m);
    CHECK_INT(le_menu_rested(&m, 30000L), 0);
    le_menu_close(&le, &m);
    CHECK_INT(le_menu_rested(&m, 300000L), 0);
    vt_free(t);
}

/* Drawn by rows -- the bar moving, the names scrolling either way by DL
 * and IL, a wrap, a page, a letter -- the list is what a whole draw makes,
 * in each personality; with text under the list it is drawn whole and the
 * text stays. */
static void the_list_drawn_by_rows_is_the_list_drawn_whole(void)
{
    static const long seq[] = { VT_KEY_DOWN, VT_KEY_DOWN, VT_KEY_DOWN, VT_KEY_DOWN, VT_KEY_DOWN, VT_KEY_DOWN,
                                VT_KEY_DOWN, VT_KEY_DOWN, VT_KEY_DOWN, VT_KEY_DOWN, VT_KEY_DOWN, VT_KEY_DOWN,
                                VT_KEY_DOWN, VT_KEY_DOWN, VT_KEY_DOWN, VT_KEY_UP, VT_KEY_UP, VT_KEY_UP, VT_KEY_UP,
                                VT_KEY_UP, VT_KEY_UP, VT_KEY_UP, VT_KEY_UP, VT_KEY_UP, VT_KEY_UP, VT_KEY_UP,
                                VT_KEY_UP, VT_KEY_UP, VT_KEY_UP, VT_KEY_UP, VT_KEY_UP, VT_KEY_PAGE_DOWN,
                                VT_KEY_PAGE_DOWN, VT_KEY_PAGE_UP, VT_KEY_END, VT_KEY_HOME, 't' };
    static const enum vt_personality pers[] = { VT_XTERM, VT_AMIGA, VT_PCANSI };
    char names[40 * 4];
    le_menu m;
    int p, i, ok, below;
    menu_names(names, 40);
    for (p = 0; p < 3; p++)
        for (below = 0; below < 2; below++) {
            vt_term *t = menu_start(30, 20, pers[p], "> ");
            memset(&m, 0, sizeof(m));
            le_menu_open(&le, &m, names, 40, 0, -1); /* rows 1-12, "k of n" on 13 */
            if (below)
                h_put(t, "\0337\033[19;3Hstays\0338");
            ok = 1;
            h_scroll_calls = 0;
            for (i = 0; i < (int)(sizeof(seq) / sizeof(seq[0])); i++) {
                int r = seq[i] == 't' ? menu_key(&m, 0, 0, "t") : menu_key(&m, seq[i], 0, 0);
                if (r != LE_MENU_MOVED)
                    continue;
                le_menu_draw(&le, &m);
                if (!below && (strcmp(h_row(t, 14), "") || strcmp(h_row(t, 19), ""))) {
                    printf("  personality %d: key %d left [%s] under the list\n", p, i, h_row(t, 14));
                    ok = 0;
                    break;
                }
                if (!menu_as_drawn_whole(t, &m)) {
                    printf("  personality %d below %d: key %d (sel %d top %d) drawn by rows differs\n", p,
                           below, i, m.sel, m.top);
                    ok = 0;
                    break;
                }
            }
            CHECK(ok);
            CHECK_INT(h_scroll_calls > 0, !below); /* blank under the list: the names moved */
            CHECK_STR(h_row(t, 18), below ? "  stays" : "");
            le_menu_close(&le, &m);
            CHECK_STR(h_row(t, 0), "> /theme");
            CHECK_STR(h_row(t, 1), "");
            vt_free(t);
        }
}

/* H8.1: KingCON prints its list (FNCMODE L, Ctrl+D) in 19-character
 * columns, (width + 1) / 19 a row, and cuts a name over 18 (its suffix
 * counted) to 15 + "..." -- not sized to the widest name. */
static void kingcon_list_has_19_char_columns_and_cuts_long_names(void)
{
    static const char names[] = "a \0Startup-Sequence \0AVeryLongFileName1 \0Prefs/\0";
    vt_term *t = start(40, 8, "1.SYS:> ");
    type("dir S:");
    le_kc_show_list(&le, names, (int)sizeof(names) - 1);
    CHECK_STR(h_row(t, 1), "a                  Startup-Sequence");
    CHECK_STR(h_row(t, 2), "AVeryLongFileNa... Prefs/");
    CHECK_STR(h_row(t, 3), "1.SYS:> dir S:");
    vt_free(t);
    t = start(18, 8, "> ");              /* (17 + 1) / 19 = 0: one column */
    type("x");
    le_kc_show_list(&le, names, (int)sizeof(names) - 1);
    CHECK_STR(h_row(t, 1), "a");
    CHECK_STR(h_row(t, 2), "Startup-Sequence");
    vt_free(t);
}

static void replace_word_for_menu_cycling(void)
{
    vt_term *t = start(40, 3, "> ");
    type("cd Wor");
    le_replace_word(&le, 3, (const unsigned char *)"Work:", 5);
    CHECK_STR(line(), "cd Work:");
    le_replace_word(&le, 3, (const unsigned char *)"WorkBench/", 10);
    CHECK_STR(h_row(t, 0), "> cd WorkBench/");
    vt_free(t);
}

/* KingCON's completion word and quoting (research/2026-10-02_kingcon-completion.md) */
static const char *kc(const char *typed, const char *entry)
{
    vt_term *t = start(80, 3, "> ");
    int q, a;
    type(typed);
    a = le_kc_word(&le, &q);
    le_kc_insert(&le, a, q, (const unsigned char *)entry, (int)strlen(entry));
    vt_free(t);
    return line();
}

static void kingcon_word_and_quoting(void)
{
    vt_term *t = start(80, 3, "> ");
    int q;
    type("echo x >RAM:t");
    CHECK_INT(le_kc_word(&le, &q), 8);             /* > ends a word, = | ; do not */
    CHECK_INT(q, -1);
    le_reset(&le);
    type("type \"My Fi");
    CHECK_INT(le_kc_word(&le, &q), 6);             /* odd quotes: after the last one */
    CHECK_INT(q, 5);
    vt_free(t);
    CHECK_STR(kc("dir S:Sh", "Shell-Startup "), "dir S:Shell-Startup ");
    CHECK_STR(kc("dir SYS:Pre", "Prefs/"), "dir SYS:Prefs/");
    CHECK_STR(kc("cd Sy", "SYS:"), "cd SYS:");
    CHECK_STR(kc("type \"My Fi", "My File "), "type \"My File\" ");
    CHECK_STR(kc("type RAM:My", "My File "), "type \"RAM:My File\" ");
    CHECK_STR(kc("cd \"Work Dir/Su", "Sub/"), "cd \"Work Dir/Sub/");
    CHECK_STR(kc("copy a=b", "a=bc "), "copy a=bc ");  /* = is not a delimiter */
}

static void kingcon_cycle_and_fncmode(void)
{
    vt_term *t = start(80, 3, "> ");
    unsigned char snap[LE_MAX];
    int q, a, sp;
    type("type RAM:My");
    type("X");
    key(VT_KEY_LEFT, 0);           /* text after the cursor stays through the cycle */
    a = le_kc_word(&le, &q);
    sp = le.pos;
    memcpy(snap, le.buf, le.len);
    le_kc_redo(&le, snap, sp, a, q, (const unsigned char *)"My File ", 8);
    CHECK_STR(line(), "type \"RAM:My File\" X");
    le_kc_redo(&le, snap, sp, a, q, (const unsigned char *)"Myfile ", 7);
    CHECK_STR(line(), "type RAM:Myfile X");  /* the quote the last one needed is gone */
    le_kc_redo(&le, snap, sp, a, q, (const unsigned char *)"Mydir/", 6);
    CHECK_STR(line(), "type RAM:Mydir/X");
    vt_free(t);
    CHECK_INT(le_kc_fncmode(""), LE_KC_WINDOW);
    CHECK_INT(le_kc_fncmode("w"), LE_KC_WINDOW);
    CHECK_INT(le_kc_fncmode("WLB"), LE_KC_WINDOW);     /* W clears L and B */
    CHECK_INT(le_kc_fncmode("bl"), LE_KC_CYCLE | LE_KC_LIST);
    CHECK_INT(le_kc_fncmode("CB S"), LE_KC_COMMON | LE_KC_CYCLE | LE_KC_SILENT);
    CHECK_INT(le_kc_fncmode("S"), LE_KC_WINDOW | LE_KC_SILENT);
}

/* The V47 Shell forces a line into an empty prompt (ACTION_FORCE after it
 * listed completions): the line is drawn after the prompt, not nowhere
 * (rig 3.2, 2026-10-03: the restored "dir RAM:" did not show) */
static void a_replaced_line_on_a_fresh_prompt_draws_after_it(void)
{
    vt_term *t = start(40, 4, "1.SYS:> ");
    le_replace_word(&le, 0, (const unsigned char *)"dir RAM:", 8);
    CHECK_STR(line(), "dir RAM:");
    CHECK_STR(h_row(t, 0), "1.SYS:> dir RAM:");
    vt_free(t);
}

/* A reflow moves the line being edited (gaps #11): the editor finds its
 * start again from the cursor, so editing goes on in the right place. */
static void reflow_moves_the_line_and_editing_follows(void)
{
    vt_term *t = h_new(20, 8, VT_XTERM);
    vt_set_onlcr(t, 1);
    vt_set_reflow(t, 1);
    h_put(t, "0123456789012345678901234567890\n$ "); /* two rows at 20, four at 10 */
    le_free(&le);
    le_init(&le, t, to_term, t);
    type("abcdefghijklmnopqrstuvwxyz");
    CHECK_STR(h_row(t, 2), "$ abcdefghijklmnopqr");
    key(VT_KEY_LEFT, 0);
    key(VT_KEY_LEFT, 0);
    key(VT_KEY_LEFT, 0);
    vt_resize(t, 10, 8);
    le_resized(&le);
    type("_");
    CHECK_STR(h_screen(t), "0123456789|0123456789|0123456789|0|$ abcdefgh|ijklmnopqr|stuvw_xyz");
    key(VT_KEY_LEFT, VT_MOD_SHIFT); /* to the start */
    type("^");
    CHECK_STR(h_row(t, 4), "$ ^abcdefg");
    CHECK(key(VT_KEY_RETURN, 0));
    CHECK_STR(line(), "^abcdefghijklmnopqrstuvw_xyz\n");
    vt_free(t);
}

/* History and undo are packed and grow as lines come
 * (research/2026-10-04_window-memory.md): nothing is allocated when the
 * line editor starts, a full history still keeps 100 lines of 255 bytes
 * and drops the oldest, undo still goes 8 steps back on a long line, and
 * le_free gives every byte back. */
/* V88: the shell's `history` builtin reads and edits the window's list through these (and the packet) */
static void history_list_is_readable_and_editable(void)
{
    unsigned char b[16];
    vt_term *t;
    le_free(&le);
    t = h_new(80, 24, VT_XTERM);
    le_init(&le, t, to_term, t);
    CHECK_INT(le_hist_count(&le), 0);
    CHECK_INT(le_hist_get(&le, 0, b, sizeof(b)), -1);
    le_hist_add(&le, (const unsigned char *)"one\n", 4);
    le_hist_add(&le, (const unsigned char *)"two words", 9);
    le_hist_add(&le, (const unsigned char *)"three", 5);
    CHECK_INT(le_hist_count(&le), 3);
    CHECK_INT(le_hist_get(&le, 1, b, sizeof(b)), 9);
    CHECK(!strcmp((const char *)b, "two words"));
    CHECK_INT(le_hist_get(&le, 1, b, 4), 3); /* cut to the buffer */
    CHECK(!strcmp((const char *)b, "two"));
    CHECK_INT(le_hist_get(&le, 3, b, sizeof(b)), -1);
    CHECK_INT(le_hist_get(&le, -1, b, sizeof(b)), -1);
    CHECK_INT(le_hist_del(&le, 1), 0);
    CHECK_INT(le_hist_count(&le), 2);
    CHECK(le_hist_get(&le, 1, b, sizeof(b)) == 5 && !strcmp((const char *)b, "three"));
    CHECK_INT(le_hist_del(&le, 2), -1);
    CHECK_INT(le_hist_del(&le, 0), 0);
    CHECK(le_hist_get(&le, 0, b, sizeof(b)) == 5 && !strcmp((const char *)b, "three"));
    /* the arrow keys see the edited list */
    key(VT_KEY_UP, 0);
    CHECK(!strcmp(line(), "three"));
    le_hist_clear(&le);
    CHECK_INT(le_hist_count(&le), 0);
    le_hist_add(&le, (const unsigned char *)"after", 5); /* usable after a clear */
    CHECK_INT(le_hist_count(&le), 1);
    le_free(&le);
}

static void history_and_undo_grow_and_free(void)
{
    vt_term *t;
    long before;
    unsigned char ln[400];
    int i, k;
    le_free(&le);
    t = h_new(80, 24, VT_XTERM);
    before = vt_count_live;
    le_init(&le, t, to_term, t);
    CHECK_INT(vt_count_live, before); /* the window opens with none */
    CHECK(le.hist == 0 && le.undo_buf == 0);
    CHECK(sizeof(le_line) <= 3200);   /* the sentinel: was 36 KB with the slots */

    /* a short history costs about its bytes */
    le_hist_add(&le, (const unsigned char *)"dir\n", 4);
    le_hist_add(&le, (const unsigned char *)"dir\n", 4); /* the same line again: one entry */
    CHECK_INT(le.hist_n, 1);
    CHECK_INT(le.hist_used, 4);
    CHECK_INT(vt_count_live - before, 256);

    /* 101 different lines of 300 bytes: each kept at 255, the first gone */
    for (k = 0; k < LE_HIST + 1; k++) {
        memset(ln, 'a' + k % 26, sizeof(ln));
        ln[0] = (unsigned char)('0' + k / 100);
        ln[1] = (unsigned char)('0' + k / 10 % 10);
        ln[2] = (unsigned char)('0' + k % 10);
        le_hist_add(&le, ln, 300);
    }
    CHECK_INT(le.hist_n, LE_HIST);
    CHECK_INT(le.hist_used, (long)LE_HIST * LE_HIST_LEN);
    CHECK(le.hist_cap <= LE_HIST_BYTES);
    for (i = 0; i < LE_HIST; i++)
        if (strlen((const char *)le.hist + le.hist_at[i]) != LE_HIST_LEN - 1)
            break;
    CHECK_INT(i, LE_HIST);
    key(VT_KEY_UP, 0); /* the newest: line 100 */
    CHECK(!strncmp(line(), "100", 3));
    CHECK_INT(le.len, LE_HIST_LEN - 1);
    CHECK_INT(line()[3], 'a' + 100 % 26);
    for (k = 1; k < LE_HIST; k++)
        key(VT_KEY_UP, 0);
    CHECK(!strncmp(line(), "001", 3)); /* the oldest kept: line 1 (line 0 dropped) */
    CHECK_INT(line()[3], 'a' + 1);
    key(VT_KEY_UP, 0); /* no older one */
    CHECK(!strncmp(line(), "001", 3));
    le_reset(&le);

    /* undo: ten edits on a 900-byte line, eight of them taken back */
    memset(ln, 0, sizeof(ln));
    for (i = 0; i < 900; i++) {
        unsigned char ch = (unsigned char)(i % 10 == 9 ? ' ' : 'a' + i % 26);
        le_key(&le, ch, 0, &ch, 1);
    }
    CHECK_INT(le.len, 900);
    for (k = 0; k < 10; k++)
        key(VT_KEY_BACKSPACE, VT_MOD_ALT); /* a word back each: one undo step each */
    CHECK_INT(le.len, 900 - 10 * 10);
    for (k = 0; k < 8; k++)
        ctrl(0x1F);
    CHECK_INT(le.len, 900 - 2 * 10);
    for (i = 0; i < le.len; i++)
        if (le.buf[i] != (unsigned char)(i % 10 == 9 ? ' ' : 'a' + i % 26))
            break;
    CHECK_INT(i, 900 - 2 * 10); /* the bytes too, not just the length */
    CHECK_INT(le.undo_n, 0);
    ctrl(0x1F); /* nothing older: the line stays */
    CHECK_INT(le.len, 900 - 2 * 10);
    CHECK(le.undo_cap <= LE_UNDO_BYTES);

    le_free(&le);
    CHECK_INT(vt_count_live, before);
    CHECK(le.hist == 0 && le.undo_buf == 0 && le.hist_n == 0);
    le_hist_add(&le, (const unsigned char *)"again", 5); /* usable after le_free */
    CHECK_INT(le.hist_n, 1);
    le_free(&le);
    vt_free(t);
}

static char medium_buf[LE_MEDIUM_MAX];

/* V47 medium mode (SetMode 2): TAB, Shift+TAB, Up and Down are reported at
 * once as CSI code;length;cursor+1 U (codes 12, 13, 2, 3; measured on the
 * 3.2.3 ROM: "abcd" with the cursor two left gives 12;4;3U), every other key
 * edits the line. */
static const char *medium(long k, int mods)
{
    static char b[LE_MEDIUM_MAX + 1];
    int n = le_medium_report(&le, k, mods, (unsigned char *)b);
    b[n] = 0;
    return b;
}

static void medium_mode_reports_tab_shift_tab_up_and_down_at_once(void)
{
    vt_term *t = start(40, 3, "> ");
    type("abcd");
    key(VT_KEY_LEFT, 0);
    key(VT_KEY_LEFT, 0);
    CHECK_STR(medium(VT_KEY_TAB, 0), "\x9b" "12;4;3U");
    CHECK_STR(medium(VT_KEY_TAB, VT_MOD_SHIFT), "\x9b" "13;4;3U");
    CHECK_STR(medium(VT_KEY_UP, 0), "\x9b" "2;4;3U");
    CHECK_STR(medium(VT_KEY_DOWN, 0), "\x9b" "3;4;3U");
    CHECK_STR(line(), "abcd"); /* reporting does not touch the line */
    vt_free(t);
}

static void medium_mode_leaves_the_editing_keys_to_the_editor(void)
{
    vt_term *t = start(40, 3, "> ");
    type("abcd");
    CHECK_INT(le_medium_report(&le, VT_KEY_LEFT, 0, (unsigned char *)medium_buf), 0);
    CHECK_INT(le_medium_report(&le, VT_KEY_BACKSPACE, 0, (unsigned char *)medium_buf), 0);
    CHECK_INT(le_medium_report(&le, 'x', 0, (unsigned char *)medium_buf), 0);
    CHECK_INT(le_medium_report(&le, VT_KEY_UP, VT_MOD_SHIFT, (unsigned char *)medium_buf), 0);
    key(VT_KEY_BACKSPACE, 0);
    CHECK_STR(line(), "abc");
    type("e");
    CHECK_STR(line(), "abce");
    vt_free(t);
}

void suite_lineedit(void)
{
    medium_mode_reports_tab_shift_tab_up_and_down_at_once();
    medium_mode_leaves_the_editing_keys_to_the_editor();
    reflow_moves_the_line_and_editing_follows();
    a_replaced_line_on_a_fresh_prompt_draws_after_it();
    kingcon_word_and_quoting();
    kingcon_cycle_and_fncmode();
    command_word_gets_colour_until_it_changes();
    typing_a_word_quickly_colours_it_once_when_the_keys_rest();
    a_finished_word_is_coloured_at_once();
    a_known_word_does_not_flip_while_typing_goes_on();
    a_programs_line_gets_no_command_colour();
    menu_lists_names_and_redraws_prompt_and_line();
    kingcon_list_has_19_char_columns_and_cuts_long_names();
    replace_word_for_menu_cycling();
    suggestion_shows_grey_and_right_takes_it();
    return_does_not_run_the_suggestion();
    ctrl_r_searches_history();
    ctrl_r_cancel_restores();
    word_motions_and_undo();
    ctrl_l_clears_and_keeps_prompt_and_line();
    ctrl_l_keeps_a_prompt_beyond_the_bmp();
    editing_inside_the_line();
    kill_keys();
    a_long_line_wraps_and_edits_across_rows();
    a_line_that_fills_the_bottom_row_exactly();
    history_and_prefix_search();
    utf8_characters_move_as_one();
    history_list_is_readable_and_editable();
    history_and_undo_grow_and_free();
    theme_menu_chosen_with_arrows_and_return();
    theme_menu_scrolls_a_long_list_at_the_bottom();
    holding_down_in_the_theme_list_draws_rows_and_previews_once();
    the_list_drawn_by_rows_is_the_list_drawn_whole();
}
