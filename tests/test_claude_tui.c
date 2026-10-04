/* C:Claude's screen (ledger A3), piece by piece on the engine: the key
 * decoder, the input editor, the footer (idle prompt, slash menu,
 * permission menu, spinner), the transcript (a streamed Markdown answer,
 * a tool call with a folded result, an edit's diff, the todo list), and
 * what each change costs in redrawn rows. The whole program on these
 * screens is the reachability test in test_claude_repl.c. */
#include <stdlib.h>
#include "harness.h"
#include "claude_screen.h"
#include "../claude/keys.h"
#include "../claude/edit.h"
#include "../claude/tui.h"
#include "../claude/show.h"
#include "../claude/tools.h"
#include "../claude/util.h"

#define BULLET "\342\217\272"
#define CORNER "\342\216\277"
#define PROMPT "\342\235\257"
#define V "\342\224\202"
#define H "\342\224\200"

static void dump(const char *what)
{
    int r;
    if (!getenv("CL_DUMP"))
        return;
    printf("---- %s\n", what);
    for (r = 0; r < cs.rows; r++)
        printf("%2d|%s\n", r, cs_row(r));
}

/* ---- keys ---- */

static int key1(const char *bytes, cl_key *k)
{
    static cl_keys ks;
    int got;
    keys_init(&ks);
    keys_feed(&ks, bytes, (long)strlen(bytes));
    got = keys_next(&ks, k, 1);
    if (got && k->k == K_PASTE) {
        static char keep[256];
        long n = k->n < 255 ? k->n : 255;
        memcpy(keep, k->text, (size_t)n);
        keep[n] = 0;
        k->text = keep;
    }
    keys_free(&ks);
    return got;
}

static void keys(void)
{
    cl_key k;
    cl_keys ks;
    CHECK(key1("a", &k) && k.k == K_CHAR && k.ch == 'a');
    CHECK(key1("\303\274", &k) && k.k == K_CHAR && k.ch == 0xfc);     /* UTF-8 */
    CHECK(key1("\374", &k) && k.k == K_CHAR && k.ch == 0xfc);         /* Latin-1 */
    CHECK(key1("\r", &k) && k.k == K_ENTER);
    CHECK(key1("\n", &k) && k.k == K_NEWLINE);                        /* Ctrl+J */
    CHECK(key1("\033[13;2u", &k) && k.k == K_NEWLINE);                /* kitty Shift+Enter */
    CHECK(key1("\033[27;2;13~", &k) && k.k == K_NEWLINE);             /* modifyOtherKeys */
    CHECK(key1("\033\r", &k) && k.k == K_NEWLINE);                    /* Meta+Enter */
    CHECK(key1("\033[27u", &k) && k.k == K_ESC);                      /* kitty Esc */
    CHECK(key1("\033", &k) && k.k == K_ESC);                          /* a lone ESC, idle */
    CHECK(key1("\033[Z", &k) && k.k == K_BTAB);
    CHECK(key1("\233Z", &k) && k.k == K_BTAB);                        /* 8-bit CSI (Amiga) */
    CHECK(key1("\033[A", &k) && k.k == K_UP);
    CHECK(key1("\033OB", &k) && k.k == K_DOWN);
    CHECK(key1("\033[1;5C", &k) && k.k == K_RIGHT && (k.mods & KM_CTRL));
    CHECK(key1("\033[3~", &k) && k.k == K_DEL);
    CHECK(key1("\033[97;5u", &k) && k.k == K_CTRL && k.ch == 'a');    /* kitty Ctrl+A */
    CHECK(key1("\001", &k) && k.k == K_CTRL && k.ch == 'a');
    CHECK(key1("\003", &k) && k.k == K_CTRL && k.ch == 'c');
    CHECK(key1("\177", &k) && k.k == K_BS);
    CHECK(key1("\033\177", &k) && k.k == K_ALT && k.ch == 0x7f);
    CHECK(key1("\033[12;40R", &k) && k.k == K_CPR && k.row == 12 && k.col == 40);
    CHECK(key1("\033[200~one\r\ntwo\033[201~", &k) && k.k == K_PASTE && !strcmp(k.text, "one\r\ntwo"));
    /* a sequence split over two reads waits for its rest */
    keys_init(&ks);
    keys_feed(&ks, "\033[1;", 4);
    CHECK(!keys_next(&ks, &k, 0));
    keys_feed(&ks, "2A", 2);
    CHECK(keys_next(&ks, &k, 0) && k.k == K_UP && k.mods == KM_SHIFT);
    /* a paste in pieces is one key; an ESC inside it is text */
    keys_feed(&ks, "\033[200~a\033b", 9);
    CHECK(!keys_next(&ks, &k, 0));
    keys_feed(&ks, "c\033[201~x", 9);
    CHECK(keys_next(&ks, &k, 0) && k.k == K_PASTE && k.n == 4 && !memcmp(k.text, "a\033bc", 4));
    CHECK(keys_next(&ks, &k, 0) && k.k == K_CHAR && k.ch == 'x');
    /* half a UTF-8 character waits too */
    keys_feed(&ks, "\303", 1);
    CHECK(!keys_next(&ks, &k, 0));
    keys_feed(&ks, "\251", 1);
    CHECK(keys_next(&ks, &k, 0) && k.k == K_CHAR && k.ch == 0xe9);
    keys_free(&ks);
}

/* ---- the editor ---- */

static void type(cl_edit *e, const char *bytes)
{
    cl_keys ks;
    cl_key k;
    keys_init(&ks);
    keys_feed(&ks, bytes, (long)strlen(bytes));
    while (keys_next(&ks, &k, 1))
        ed_key(e, &k);
    keys_free(&ks);
}

static void editor(void)
{
    cl_edit e;
    long a[8], z[8];
    int cr, cc, n;
    ed_init(&e);
    type(&e, "hello world");
    CHECK_STR(e.b, "hello world");
    type(&e, "\027");                       /* Ctrl+W */
    CHECK_STR(e.b, "hello ");
    type(&e, "\001X\005Y");                 /* Ctrl+A, Ctrl+E */
    CHECK_STR(e.b, "Xhello Y");
    type(&e, "\033[D\033[D\013");           /* Left Left Ctrl+K */
    CHECK_STR(e.b, "Xhello");
    type(&e, "\031");                       /* Ctrl+Y puts it back */
    CHECK_STR(e.b, "Xhello Y");
    type(&e, "\025");                       /* Ctrl+U */
    CHECK_STR(e.b, "");
    type(&e, "one\033[13;2utwo");           /* Shift+Enter */
    CHECK_STR(e.b, "one\ntwo");
    type(&e, "\033[A");                     /* Up: the line above, same column */
    CHECK_INT(e.cur, 3);
    type(&e, "\033[200~p\r\nq\033[201~");   /* a paste, CR LF as one newline */
    CHECK_STR(e.b, "onep\nq\ntwo");
    /* history: Up on the first line, Down back to the draft */
    ed_remember(&e, "earlier");
    ed_set(&e, "draft");
    e.cur = 0;
    type(&e, "\033[A");
    CHECK_STR(e.b, "earlier");
    type(&e, "\033[B");
    CHECK_STR(e.b, "draft");
    /* the layout: wrapped at the width, the cursor's row and column */
    ed_set(&e, "abcdefgh\nxy");
    n = ed_layout(&e, 5, a, z, 8, &cr, &cc);
    CHECK_INT(n, 3);
    CHECK_INT(a[1], 5);
    CHECK_INT(z[1], 8);
    CHECK_INT(cr, 2);
    CHECK_INT(cc, 2);
    ed_free(&e);
}

/* ---- the footer ---- */

static cl_io io;
static cl_tui tui;
static cl_show shw;
static int mode;

static void screen(int cols, int rows, const char **script)
{
    cs_open(cols, rows, script);
    cs_io(&io);
    CHECK_INT(tui_init(&tui, &io), 0);
    tui.model = "claude-opus-5-5";
    tui.effort = "medium";
    tui.root = "Work:Project";
    tui.ctx_left = 92;
    mode = PERM_DEFAULT;
    tui.mode = &mode;
    show_init(&shw, &tui);
}

static void unscreen(void)
{
    tui_stop(&tui);
    show_free(&shw);
    tui_free(&tui);
}

static const cl_cmd cmds[] = {
    { "/clear", "Start a new conversation" }, { "/compact", "Summarise" }, { "/context", "The context" },
    { "/cost", "The cost" }, { "/exit", "Leave" }, { "/help", "Help" }
};

static void idle_prompt(void)
{
    static const char *script[] = { "hel", "lo\r", 0 };
    char line[64];
    screen(60, 16, script);
    /* the shell's last lines stay above, the cursor was on row 3 */
    vt_write(cs.vt, (const vt_u8 *)"1> Claude\r\n", 11);
    CHECK_INT(tui_start(&tui), 0);
    CHECK_INT(cs.raw_on, 1);
    /* the start's modes and DSR went out whole, no stray NUL after them */
    CHECK(strstr(cs.sent.p, "\033[?2004h\033[>1u\033[>4;1m\033[6n") == cs.sent.p);
    CHECK(memchr(cs.sent.p, 0, (size_t)cs.sent.n) == 0);
    CHECK_INT(tui.tr, 2);
    CHECK_INT(tui.B, 12);
    CHECK_INT(tui_read(&tui, line, sizeof(line)), 5);
    CHECK_STR(line, "hello");
    dump("idle");
    /* the golden idle screen: box, prompt, status line with the facts */
    CHECK_STR(cs_row(0), "1> Claude");
    CHECK(!strncmp(cs_row(12), "\342\225\255" H H H, 12));
    CHECK(strstr(cs_row(12), "\342\225\256") != 0);
    {
        /* the frame's sides at columns 1 and 60, the prompt at 3 */
        char want[200];
        int k;
        strcpy(want, V " " PROMPT);
        for (k = 3; k < 59; k++)
            strcat(want, " ");
        strcat(want, V);
        CHECK_STR(cs_row(13), want);
    }
    CHECK(!strncmp(cs_row(14), "\342\225\260" H, 6));
    /* 60 columns: the start directory is the first fact left out */
    CHECK_STR(cs_row(15), "  / for commands   claude-opus-5-5 \302\267 medium \302\267 ctx: 92% left");
    /* the cursor in the box, after the prompt */
    {
        int x, y;
        vt_cursor(cs.vt, &x, &y);
        CHECK_INT(y, 13);
        CHECK_INT(x, 4);
    }
    /* the frame is a rounded box in grey */
    CHECK_INT(h_cell(cs.vt, 0, 12)->fg, 8);
    /* Shift+Tab: accept edits, then plan, then back */
    mode = PERM_ACCEPT;
    tui_frame(&tui);
    CHECK(strstr(cs_row(15), "\342\217\265\342\217\265 accept edits on (shift+tab to cycle)") != 0);
    mode = PERM_PLAN;
    tui_frame(&tui);
    CHECK(strstr(cs_row(15), "|| plan mode on") != 0);
    unscreen();
    CHECK_INT(cs.raw_on, 0);
    cs_close();
}

static void typing_and_slash_menu(void)
{
    static const char *script[] = { "/c", 0 };
    static const char *script2[] = { "/co", "\033[B", "\t", 0 };
    char line[64];
    long rows0;
    screen(60, 16, script);
    tui.cmds = cmds;
    tui.ncmds = 6;
    CHECK_INT(tui_start(&tui), 0);
    rows0 = tui.n_rows;
    CHECK_INT(tui_read(&tui, line, sizeof(line)), -1);     /* the script ran out */
    dump("slash menu");
    /* the menu under the box: the four commands that start with /c */
    CHECK_INT(tui.B, 9);
    CHECK(strstr(cs_row(12), "/clear") && strstr(cs_row(12), "Start a new conversation"));
    CHECK(strstr(cs_row(13), "/compact") != 0);
    CHECK(strstr(cs_row(15), "/cost") != 0);
    CHECK(strstr(cs_row(10), V " " PROMPT " /c") != 0);
    (void)rows0;
    unscreen();
    cs_close();
    /* Down moves the selection, Tab completes it */
    screen(60, 16, script2);
    tui.cmds = cmds;
    tui.ncmds = 6;
    tui_start(&tui);
    CHECK_INT(tui_read(&tui, line, sizeof(line)), -1);
    CHECK_STR(tui.ed.b, "/context ");
    unscreen();
    cs_close();
    /* one typed key redraws one row of the footer, nothing else */
    {
        static const char *one[] = { "x", 0 };
        long before;
        screen(60, 16, one);
        tui_start(&tui);
        before = tui.n_rows;
        tui_read(&tui, line, sizeof(line));
        CHECK_INT(tui.n_rows - before, 1);
        unscreen();
        cs_close();
    }
}

static void permission_menu(void)
{
    static const char *script[] = { "\033[B", 0 };
    static const char *script2[] = { "\033[B", "\r", 0 };
    static const char *script3[] = { "\033", 0 };
    static const char *const opt[] = { "Yes", "Yes, and don't ask again this session",
                                       "No, and tell Claude what to do differently (esc)" };
    screen(70, 18, script);
    tui_start(&tui);
    CHECK_INT(tui_menu(&tui, "Edit file", "Do you want to make this edit to s.c?", opt, 3, 0, 2), -1);
    tui.modal = 1;                  /* as it was while asking */
    tui_frame(&tui);
    dump("permission");
    CHECK(strstr(cs_row(11), "\342\225\255") != 0);
    CHECK(!strncmp(cs_row(12), V " Edit file   ", 15) && vw_width(cs_row(12), (long)strlen(cs_row(12))) == 70);
    CHECK(strstr(cs_row(13), "Do you want to make this edit to s.c?") != 0);
    CHECK(strstr(cs_row(14), "   1. Yes") != 0);
    CHECK(strstr(cs_row(15), " " PROMPT " 2. Yes, and don't ask again this session") != 0);
    CHECK(strstr(cs_row(16), "3. No, and tell Claude what to do differently (esc)") != 0);
    CHECK(strstr(cs_row(17), "\342\225\260") != 0);
    tui.modal = 0;
    unscreen();
    cs_close();
    screen(70, 18, script2);
    tui_start(&tui);
    CHECK_INT(tui_menu(&tui, "Edit file", "q", opt, 3, 0, 2), 1);
    /* the box is back after the answer */
    CHECK(cs_find(PROMPT " ") >= 0);
    CHECK(cs_find("Edit file") < 0);
    unscreen();
    cs_close();
    screen(70, 18, script3);
    tui_start(&tui);
    CHECK_INT(tui_menu(&tui, "Run command", "q", opt, 3, 0, 2), 2);    /* Esc is "No, and tell" */
    unscreen();
    cs_close();
}

static void spinner(void)
{
    static const char *script[] = { "!\033", 0 };
    long rows0;
    screen(60, 16, script);
    tui_start(&tui);
    tui_busy(&tui, 1);
    tui.tokens = 1234;
    cs.clock += 12000;
    tui_tick(&tui);
    dump("spinner");
    CHECK(cs_find("(12s \302\267 1.2k tokens \302\267 esc to interrupt)") == 11);
    CHECK(strstr(cs_row(11), "...") != 0);
    CHECK(tui.B == 11);
    /* the next tick redraws the spinner row only */
    rows0 = tui.n_rows;
    cs.clock += 300;
    tui_tick(&tui);
    CHECK_INT(tui.n_rows - rows0, 1);
    /* nothing changed: nothing is sent at all */
    rows0 = cs.writes;
    tui_frame(&tui);
    CHECK_INT(cs.writes, rows0);
    /* Esc typed while Claude works stops it */
    CHECK_INT(tui_poll(&tui), 1);
    tui_busy(&tui, 0);
    CHECK(cs_find("esc to interrupt") < 0);
    unscreen();
    cs_close();
}

/* ---- the transcript ---- */

static void answer_and_tools(void)
{
    static const char *script[] = { 0 };
    static const char md[] = "Here is **the plan**:\n\n- one\n- two\n\n```c\nint x = 1;\n```\nDone.";
    cl_render r;
    long i, rows0;
    char out[40];
    screen(60, 30, script);
    tui_start(&tui);
    show_render(&shw, &r);
    rows0 = tui.n_rows;
    for (i = 0; i < (long)sizeof(md) - 1; i += 3)
        r.text(r.u, md + i, (long)sizeof(md) - 1 - i < 3 ? (long)sizeof(md) - 1 - i : 3);
    r.end(r.u);
    /* the transcript went past the footer without one footer row redrawn */
    CHECK_INT(tui.n_rows - rows0, 0);
    dump("answer");
    CHECK_STR(cs_row(1), BULLET " Here is the plan:");
    CHECK_STR(cs_row(3), "  \342\200\242 one");
    CHECK_STR(cs_row(6), "    int x = 1;");
    CHECK_STR(cs_row(8), "  Done.");
    /* bold in the answer, the code coloured */
    CHECK(h_cell(cs.vt, 10, 1)->attr & VT_ATTR_BOLD);
    /* a command with long output: folded */
    show_tool(&shw, T_RUN_COMMAND, "{\"command\":\"list\"}", 18, "list");
    show_result(&shw, T_RUN_COMMAND, "{\"command\":\"list\"}", 18, 0,
                "Return code 0.\nl1\nl2\nl3\nl4\nl5\nl6\nl7", 35);
    dump("tool");
    i = cs_find(BULLET " Run(list)");
    CHECK(i > 0);
    CHECK_STR(cs_row((int)i + 1), "  " CORNER "  l1");
    CHECK_STR(cs_row((int)i + 3), "     l3");
    CHECK_STR(cs_row((int)i + 4), "     ... +4 lines (ctrl+o to expand)");
    /* the bullet of a call that worked is green */
    CHECK_INT(h_cell(cs.vt, 0, (int)i)->fg, 2);
    /* Ctrl+O: the folded result in full */
    tui.expand = 1;
    show_expand(&shw);
    CHECK(cs_find("     l7") > (int)i);
    tui.expand = 0;
    /* a read: one line under the corner */
    show_tool(&shw, T_READ_FILE, "{\"path\":\"S/Startup-Sequence\"}", 29, "x");
    show_result(&shw, T_READ_FILE, "", 0, 0, "a\nb\nc\n", 6);
    CHECK(cs_find(BULLET " Read(S/Startup-Sequence)") > 0);
    CHECK(cs_find("  " CORNER "  Read 3 lines") > 0);
    /* an error in red */
    show_tool(&shw, T_LIST_DIR, "{\"path\":\"Nope\"}", 15, "x");
    show_result(&shw, T_LIST_DIR, "", 0, 1, "not a directory: Nope", 21);
    i = cs_find("Error: not a directory: Nope");
    CHECK(i > 0);
    CHECK_INT(h_cell(cs.vt, 0, (int)i - 1)->fg, 1);
    cl_copy(out, "", sizeof(out));
    unscreen();
    cs_close();
}

static void edit_diff_and_todos(void)
{
    static const char *script[] = { 0 };
    static const char before[] = "one\ntwo\nthree\nfour\nfive\nsix\n";
    static const char after[] = "one\ntwo\nthree\nFOUR\n4b\nfive\nsix\n";
    static const char todo[] = "{\"todos\":[{\"content\":\"Read\",\"status\":\"completed\"},"
                               "{\"content\":\"Edit\",\"status\":\"in_progress\"},{\"content\":\"Tell\",\"status\":\"pending\"}]}";
    int i;
    screen(60, 30, script);
    tui_start(&tui);
    show_tool(&shw, T_EDIT_FILE, "{\"path\":\"s.txt\"}", 16, "x");
    show_preview(&shw, T_EDIT_FILE, "Work:s.txt", before, (long)strlen(before), after, (long)strlen(after));
    show_result(&shw, T_EDIT_FILE, "", 0, 0, "Edited", 6);
    dump("diff");
    i = cs_find(BULLET " Update(s.txt)");
    CHECK(i >= 0);
    CHECK_STR(cs_row(i + 1), "     1   one");
    CHECK_STR(cs_row(i + 3), "     3   three");
    CHECK(!strncmp(cs_row(i + 4), "     4 - four", 13));
    CHECK(!strncmp(cs_row(i + 5), "     4 + FOUR", 13));
    CHECK(!strncmp(cs_row(i + 6), "     5 + 4b", 11));
    CHECK_STR(cs_row(i + 7), "     6   five");
    CHECK_STR(cs_row(i + 9), "  " CORNER "  Updated s.txt with 2 additions and 1 removal");
    /* removed on red, added on green, to the row's end */
    CHECK_INT(h_cell(cs.vt, 10, i + 4)->bg, 1);
    CHECK_INT(h_cell(cs.vt, 50, i + 5)->bg, 2);
    CHECK_INT(h_cell(cs.vt, 10, i + 7)->bg, VT_COLOR_DEFAULT);
    /* the todo list as a checklist */
    show_result(&shw, T_TODO_WRITE, todo, (long)strlen(todo), 0, "ok", 2);
    dump("todos");
    i = cs_find(BULLET " Update Todos");
    CHECK(i > 0);
    CHECK_STR(cs_row(i + 1), "  " CORNER "  \342\234\224 Read");
    CHECK_STR(cs_row(i + 2), "     \342\226\240 Edit");
    CHECK_STR(cs_row(i + 3), "     \342\227\213 Tell");
    CHECK(h_cell(cs.vt, 7, i + 2)->attr & VT_ATTR_BOLD);
    unscreen();
    cs_close();
}

/* the footer grows over a full transcript: the transcript scrolls up into
 * the scrollback, nothing is overwritten */
static void grow_over_transcript(void)
{
    static const char *script[] = { 0 };
    int k;
    screen(40, 12, script);
    tui_start(&tui);
    for (k = 0; k < 10; k++) {
        char l[16];
        cl_copy(l, "line ", sizeof(l));
        l[5] = (char)('0' + k);
        l[6] = '\n';
        l[7] = 0;
        tui_lines(&tui, l, 7);
    }
    CHECK_STR(cs_row(7), "line 9");
    tui_busy(&tui, 1);              /* one row more: the spinner */
    dump("grown");
    CHECK_STR(cs_row(6), "line 9");
    CHECK_STR(cs_row(0), "line 3");
    CHECK(vt_scrollback_lines(cs.vt) >= 2);
    tui_busy(&tui, 0);
    tui_lines(&tui, "after\n", 6);
    CHECK_STR(cs_row(7), "after");
    /* a resize: the footer redrawn at the new bottom */
    cs_resize(50, 14);
    tui_tick(&tui);
    CHECK(strstr(cs_row(13), "ctx:") != 0);
    CHECK(strstr(cs_row(10), "\342\225\255") != 0);
    unscreen();
    cs_close();
}

/* the column (code points from 0) of the last occurrence of the UTF-8
 * sequence g in a screen row, -1 when absent */
static int last_col(const char *row, const char *g)
{
    int col = 0, found = -1;
    size_t n = strlen(g);
    while (*row) {
        if (!strncmp(row, g, n))
            found = col;
        row++;
        while ((*row & 0xC0) == 0x80)
            row++;
        col++;
    }
    return found;
}

/* The welcome box: its right side in one column on every row (the first
 * row's width was counted 3 short: its border stood 3 columns out). */
static void welcome_box_sides_line_up(void)
{
    static const char *script[] = { 0 };
    int r, top = -1, right;
    screen(80, 20, script);
    CHECK_INT(tui_start(&tui), 0);
    show_welcome(&shw, "claude-opus-5-5", "RAM:");
    for (r = 0; r < cs.rows && top < 0; r++)
        if (strstr(cs_row(r), "\342\225\255"))
            top = r;
    CHECK(top >= 0);
    right = last_col(cs_row(top), "\342\225\256");
    CHECK(right > 0);
    for (r = top + 1; r <= top + 5; r++)
        CHECK_INT(last_col(cs_row(r), V), right);
    CHECK_INT(last_col(cs_row(top + 6), "\342\225\257"), right);
    unscreen();
    cs_close();
}

void suite_claude_tui(void)
{
    keys();
    editor();
    idle_prompt();
    typing_and_slash_menu();
    permission_menu();
    spinner();
    answer_and_tools();
    edit_diff_and_todos();
    grow_over_transcript();
    welcome_box_sides_line_up();
}
