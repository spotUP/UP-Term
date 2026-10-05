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
    le_set_command(&le, (const unsigned char *)"xlist", 0);
    CHECK_INT(h_cell(t, 3, 0)->fg, 1);  /* red */
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

void suite_lineedit(void)
{
    reflow_moves_the_line_and_editing_follows();
    a_replaced_line_on_a_fresh_prompt_draws_after_it();
    kingcon_word_and_quoting();
    kingcon_cycle_and_fncmode();
    command_word_gets_colour_until_it_changes();
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
    history_and_undo_grow_and_free();
}
