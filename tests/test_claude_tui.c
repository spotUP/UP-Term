/* C:Claude's screen (ledger A3), piece by piece on the engine: the key
 * decoder, the input editor, the footer (idle prompt, slash menu,
 * permission menu, spinner), the transcript (a streamed Markdown answer,
 * a tool call with a folded result, an edit's diff, the todo list), and
 * what each change costs in redrawn rows. The whole program on these
 * screens is the reachability test in test_claude_repl.c. */
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "harness.h"
#include "claude_screen.h"
#include "../claude/keys.h"
#include "../claude/edit.h"
#include "../claude/tui.h"
#include "../claude/show.h"
#include "../claude/tools.h"
#include "../claude/util.h"
#include "../claude/hist.h"
#include "../claude/theme.h"
#include "../claude/conv.h"
#include "../claude/sys_posix.h"

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
static cl_perm perm;

static void screen(int cols, int rows, const char **script)
{
    cs_open(cols, rows, script);
    cs_io(&io);
    CHECK_INT(tui_init(&tui, &io), 0);
    tui.model = "claude-opus-5-5";
    tui.effort = "medium";
    tui.root = "Work:Project";
    tui.ctx_left = 92;
    memset(&perm, 0, sizeof(perm));
    tui.perm = &perm;
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
    perm.mode = PERM_ACCEPT;
    tui_frame(&tui);
    CHECK(strstr(cs_row(15), "\342\217\265\342\217\265 accept edits on (shift+tab to cycle)") != 0);
    perm.mode = PERM_PLAN;
    tui_frame(&tui);
    CHECK(strstr(cs_row(15), "|| plan mode on") != 0);
    /* gaps 3: bypassPermissions, in the error colour, when the session may use it */
    perm.mode = PERM_BYPASS;
    tui_frame(&tui);
    CHECK(strstr(cs_row(15), "\342\217\265\342\217\265 bypass permissions on (shift+tab to cycle)") != 0);
    CHECK_INT(h_cell(cs.vt, 2, 15)->fg, 1);
    /* Shift+Tab's cycle: bypass only when it may be used */
    perm.mode = PERM_PLAN;
    CHECK_INT(perm_next(&perm), PERM_DEFAULT);
    perm.can_bypass = 1;
    CHECK_INT(perm_next(&perm), PERM_BYPASS);
    perm.mode = PERM_BYPASS;
    CHECK_INT(perm_next(&perm), PERM_DEFAULT);
    perm.mode = PERM_DEFAULT;
    perm.can_bypass = 0;
    unscreen();
    CHECK_INT(cs.raw_on, 0);
    cs_close();
}

/* The statusLine command's output (the REPL puts it in tui.status): its own
 * row between the box and the status line, colours kept, other escapes
 * dropped; "/ for commands" gone, as Claude Code drops its hint; a new
 * text redraws that one row; a second line grows the footer by one. */
static int idles;
static void idle_cb(void *u)
{
    (void)u;
    idles++;
}

static void status_line_row(void)
{
    static const char *script[] = { "x\r", 0 };
    static char st[200];
    char line[16];
    long rows0, full0;
    screen(60, 16, script);
    strcpy(st, "\033[32mmain\033[0m \033[2J\033]0;t\007ok");
    tui.status = st;
    tui.idle = idle_cb;
    idles = 0;
    CHECK_INT(tui_start(&tui), 0);
    CHECK_INT(tui_read(&tui, line, sizeof(line)), 1);
    CHECK(idles >= 1);                      /* the schedule's tick runs while keys are awaited */
    dump("status line");
    CHECK(!strncmp(cs_row(11), "\342\225\255", 3));    /* the box moved up one row for it */
    CHECK_STR(cs_row(14), "  main ok");
    CHECK_INT(h_cell(cs.vt, 2, 14)->fg, 2);             /* green, as the command printed it */
    CHECK_INT(h_cell(cs.vt, 7, 14)->fg, VT_COLOR_DEFAULT);
    CHECK(strstr(cs_row(15), "/ for commands") == 0);
    CHECK(strstr(cs_row(15), "ctx: 92% left") != 0);
    CHECK(strstr(cs.sent.p, "\033[2J") == 0);
    /* a new output: one row sent, no layout */
    rows0 = tui.n_rows;
    full0 = tui.n_full;
    strcpy(st, "\033[33mdev\033[0m ok");
    tui_frame(&tui);
    CHECK_INT(tui.n_rows - rows0, 1);
    CHECK_INT(tui.n_full - full0, 0);
    CHECK_STR(cs_row(14), "  dev ok");
    CHECK_INT(h_cell(cs.vt, 2, 14)->fg, 3);
    /* two lines: two rows; padding moves the text */
    strcpy(st, "one\ntwo");
    tui.status_pad = 2;
    tui_frame(&tui);
    CHECK_STR(cs_row(13), "    one");
    CHECK_STR(cs_row(14), "    two");
    /* none: the row goes and the hint comes back */
    st[0] = 0;
    tui_frame(&tui);
    CHECK(strstr(cs_row(15), "/ for commands") != 0);
    CHECK(!strncmp(cs_row(12), "\342\225\255", 3));
    unscreen();
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
    show_tool(&shw, T_BASH, "{\"command\":\"list\"}", 18, "list");
    show_result(&shw, T_BASH, "{\"command\":\"list\"}", 18, 0,
                "Return code 0.\nl1\nl2\nl3\nl4\nl5\nl6\nl7", 35);
    dump("tool");
    i = cs_find(BULLET " Bash(list)");
    CHECK(i > 0);
    CHECK_STR(cs_row((int)i + 1), "  " CORNER "  l1");
    CHECK_STR(cs_row((int)i + 3), "     l3");
    CHECK_STR(cs_row((int)i + 4), "     ... +4 lines (ctrl+o to expand)");
    /* the bullet of a call that worked is green */
    CHECK_INT(h_cell(cs.vt, 0, (int)i)->fg, 2);
    /* folded on the screen, whole in the transcript viewer's text */
    CHECK(cs_find("     l7") < 0);
    CHECK(strstr(tui.log.p, "     l7") != 0);
    CHECK(strstr(tui.log.p, "ctrl+o to expand") == 0);
    /* a read: one line under the corner */
    show_tool(&shw, T_READ, "{\"file_path\":\"S/Startup-Sequence\"}", 34, "x");
    show_result(&shw, T_READ, "", 0, 0, "     1\ta\n     2\tb\n     3\tc\n", 27);
    CHECK(cs_find(BULLET " Read(S/Startup-Sequence)") > 0);
    CHECK(cs_find("  " CORNER "  Read 3 lines") > 0);
    /* an error in red */
    show_tool(&shw, T_GLOB, "{\"pattern\":\"*\",\"path\":\"Nope\"}", 29, "x");
    show_result(&shw, T_GLOB, "", 0, 1, "no such directory: Nope", 23);
    i = cs_find("Error: no such directory: Nope");
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
    show_tool(&shw, T_EDIT, "{\"file_path\":\"s.txt\"}", 21, "x");
    show_preview(&shw, T_EDIT, "Work:s.txt", before, (long)strlen(before), after, (long)strlen(after));
    show_result(&shw, T_EDIT, "", 0, 0, "Edited", 6);
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

/* The start, as Claude Code's: the mascot in the accent colour, beside it
 * the program, the model and the directory, their text in one column. */
static void welcome_mascot(void)
{
    static const char *script[] = { 0 };
    int top;
    screen(80, 20, script);
    CHECK_INT(tui_start(&tui), 0);
    show_welcome(&shw, "claude-opus-5-5", "RAM:");
    dump("welcome");
    top = cs_find("\342\226\220\342\226\233\342\226\210\342\226\210\342\226\210\342\226\234\342\226\214");
    CHECK(top >= 0);
    if (top >= 0) {
        CHECK_STR(cs_row(top), " \342\226\220\342\226\233\342\226\210\342\226\210\342\226\210\342\226\234"
                               "\342\226\214   C:Claude for the Amiga");
        CHECK_STR(cs_row(top + 1), "\342\226\235\342\226\234\342\226\210\342\226\210\342\226\210\342\226\210"
                                   "\342\226\210\342\226\233\342\226\230  claude-opus-5-5 \302\267 API Usage Billing");
        CHECK_STR(cs_row(top + 2), "  \342\226\230\342\226\230 \342\226\235\342\226\235    RAM:");
        CHECK_INT(h_cell(cs.vt, 1, top)->fg, 3);            /* the accent (dark: yellow) */
        CHECK(h_cell(cs.vt, 11, top)->attr & VT_ATTR_BOLD);  /* the name */
    }
    unscreen();
    cs_close();
    (void)last_col;
}

/* ==== A4 WP1: the input box's and the screen's Claude Code features ==== */

static char tdir[512];
static sys_posix tsp;
static cl_sys tsys;

static void mk_tdir(void)
{
    const char *base = getenv("TMPDIR");
    strcpy(tdir, base && *base ? base : "/tmp");
    if (tdir[strlen(tdir) - 1] == '/')
        tdir[strlen(tdir) - 1] = 0;
    strcat(tdir, "/claude_tui_XXXXXX");
    if (!mkdtemp(tdir))
        tdir[0] = 0;
    sys_posix_init(&tsp, &tsys);
}

static void rm_tdir(void)
{
    char cmd[600];
    if (!tdir[0])
        return;
    strcpy(cmd, "rm -rf ");
    strcat(cmd, tdir);
    if (system(cmd))
        printf("  [ERROR] could not remove %s\n", tdir);
}

static void tpath(char *out, const char *name)
{
    strcpy(out, tdir);
    strcat(out, "/");
    strcat(out, name);
}

static void tfile(const char *name, const char *text)
{
    char p[600];
    tpath(p, name);
    tsys.write(tsys.u, p, text, (long)strlen(text));
}

/* the row's first column (code points from 0) holding text, -1 none */
static int col_of(const char *row, const char *text)
{
    const char *f = strstr(row, text), *p;
    int col = 0;
    if (!f)
        return -1;
    for (p = row; p < f; p++)
        if (((unsigned char)*p & 0xc0) != 0x80)
            col++;
    return col;
}

/* 1.1: prompts kept in a file across sessions; Up recalls them; Ctrl+R */
static void history_and_search(void)
{
    static const char *s1[] = { "first\r", "second prompt\r", "third\r", 0 };
    static const char *s2[] = { "\033[A", 0 };
    static const char *s3[] = { "\022sec", 0 };
    static const char *s4[] = { "draft", "\022thi", "\003", 0 };
    static const char *s5[] = { "\022fir", "\r", 0 };
    static const char *s6[] = { "\022e", "\022", 0 };
    char line[64], path[600], *b = 0;
    long n = 0;
    cl_hist h;
    tpath(path, "history");
    screen(60, 16, s1);
    tui.sys = &tsys;
    tui.histfile = path;
    tui.project = "Work:A";
    tui_start(&tui);
    CHECK_INT(tui_read(&tui, line, sizeof(line)), 5);
    CHECK_INT(tui_read(&tui, line, sizeof(line)), 13);
    CHECK_INT(tui_read(&tui, line, sizeof(line)), 5);
    unscreen();
    cs_close();
    /* the file: one JSON object a line, oldest first */
    CHECK_INT(tsys.read(tsys.u, path, 10000, &b, &n), 0);
    CHECK(b && strstr(b, "{\"display\":\"first\",\"project\":\"Work:A\"}\n") == b);
    CHECK(b && strstr(b, "{\"display\":\"third\",\"project\":\"Work:A\"}\n") != 0);
    free(b);
#define SESSION(script)                                  \
    screen(60, 16, script);                              \
    tui.sys = &tsys;                                     \
    tui.histfile = path;                                 \
    tui.project = "Work:A";                              \
    hist_load(&tui.hist, &tsys, path);                   \
    hist_fill(&tui.hist, &tui.ed, "Work:A");             \
    tui_start(&tui)
    /* a new session: Up brings the last prompt back */
    SESSION(s2);
    CHECK_INT(tui.hist.n, 3);
    tui_read(&tui, line, sizeof(line));
    CHECK_STR(tui.ed.b, "third");
    unscreen();
    cs_close();
    /* Ctrl+R: the newest prompt holding the query, the query in reverse in
     * it, the search in place of the status line */
    SESSION(s3);
    tui_read(&tui, line, sizeof(line));
    dump("ctrl+r");
    CHECK_STR(tui.ed.b, "second prompt");
    CHECK(!strncmp(cs_row(15), "  (reverse-i-search)`sec': ctrl+r older", 39));
    CHECK(h_cell(cs.vt, 4, 13)->attr & VT_ATTR_INVERSE);      /* "sec" */
    CHECK(!(h_cell(cs.vt, 7, 13)->attr & VT_ATTR_INVERSE));   /* "ond" */
    unscreen();
    cs_close();
    /* Ctrl+C gives the typed text back */
    SESSION(s4);
    tui_read(&tui, line, sizeof(line));
    CHECK_STR(tui.ed.b, "draft");
    CHECK_INT(tui.search, 0);
    unscreen();
    cs_close();
    /* Enter sends the match */
    SESSION(s5);
    CHECK_INT(tui_read(&tui, line, sizeof(line)), 5);
    CHECK_STR(line, "first");
    unscreen();
    cs_close();
    /* Ctrl+R again: the next older match */
    SESSION(s6);
    tui_read(&tui, line, sizeof(line));
    CHECK_STR(tui.ed.b, "second prompt");
    unscreen();
    cs_close();
#undef SESSION
    /* a prompt typed twice is found once; another project's too */
    hist_init(&h);
    hist_add(&h, &tsys, 0, "x1", "A");
    hist_add(&h, &tsys, 0, "x2", "B");
    hist_add(&h, &tsys, 0, "x1", "B");
    CHECK_INT(hist_find(&h, "x", h.n), 2);
    CHECK_INT(hist_find(&h, "x", 2), 1);
    CHECK_INT(hist_find(&h, "x", 1), -1);
    hist_free(&h);
}

/* Ctrl+W to white space, Alt+B by letters and digits, Ctrl+_ undo */
static void editor_words_undo(void)
{
    cl_edit e;
    ed_init(&e);
    type(&e, "a src/utils/foo.ts");
    type(&e, "\033b");
    CHECK_INT(e.cur, 16);                   /* "ts" */
    type(&e, "\033b");
    CHECK_INT(e.cur, 12);                   /* "foo" */
    type(&e, "\005\027");                   /* Ctrl+E, Ctrl+W */
    CHECK_STR(e.b, "a ");
    type(&e, "\037");                       /* Ctrl+_ */
    CHECK_STR(e.b, "a src/utils/foo.ts");
    type(&e, "\037");                       /* the typing undoes as one */
    CHECK_STR(e.b, "");
    type(&e, "ab");
    type(&e, "\004");                       /* Ctrl+D at the end: nothing */
    CHECK_STR(e.b, "ab");
    type(&e, "\001\004");                   /* Ctrl+A, Ctrl+D deletes forward */
    CHECK_STR(e.b, "b");
    ed_free(&e);
}

/* 1.8: vim mode */
static void vim_mode(void)
{
    static const char *script[] = { "hi", 0 };
    static const char *script2[] = { "hi", "\033", 0 };
    char line[32];
    cl_edit e;
    ed_init(&e);
    ed_set_vim(&e, 1);
    type(&e, "hello world");
    CHECK_INT(e.vim, VIM_INSERT);
    type(&e, "\033");
    CHECK_INT(e.vim, VIM_NORMAL);
    CHECK_INT(e.cur, 10);
    type(&e, "0w");
    CHECK_INT(e.cur, 6);
    type(&e, "dw");
    CHECK_STR(e.b, "hello ");
    CHECK_INT(e.cur, 5);
    type(&e, "u");
    CHECK_STR(e.b, "hello world");
    type(&e, "x");
    CHECK_STR(e.b, "hello orld");
    type(&e, "u$b");
    CHECK_INT(e.cur, 6);
    type(&e, "cwthere\033");
    CHECK_STR(e.b, "hello there");
    CHECK_INT(e.vim, VIM_NORMAL);
    type(&e, "0fe");
    CHECK_INT(e.cur, 1);
    type(&e, ";");
    CHECK_INT(e.cur, 8);
    type(&e, "osecond\033");
    CHECK_STR(e.b, "hello there\nsecond");
    type(&e, "kdd");
    CHECK_STR(e.b, "second");
    type(&e, "p");
    CHECK_STR(e.b, "second\nhello there");
    type(&e, "ggJ");
    CHECK_STR(e.b, "second hello there");
    type(&e, "0diw");
    CHECK_STR(e.b, " hello there");
    type(&e, "A!\033");
    CHECK_STR(e.b, " hello there!");
    type(&e, "02x");
    CHECK_STR(e.b, "ello there!");
    type(&e, "rE");
    CHECK_STR(e.b, "Ello there!");
    type(&e, "$F D");
    CHECK_STR(e.b, "Ello");
    type(&e, "ccnew\033");
    CHECK_STR(e.b, "new");
    type(&e, "yyP");
    CHECK_STR(e.b, "new\nnew");
    ed_free(&e);
    /* on the screen: "-- INSERT --" while inserting, Esc to NORMAL */
    screen(60, 16, script);
    ed_set_vim(&tui.ed, 1);
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    dump("vim insert");
    CHECK(!strncmp(cs_row(15), "  -- INSERT --", 14));
    unscreen();
    cs_close();
    screen(60, 16, script2);
    ed_set_vim(&tui.ed, 1);
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    CHECK(!strncmp(cs_row(15), "  / for commands", 16));
    CHECK_INT(tui.ed.vim, VIM_NORMAL);
    unscreen();
    cs_close();
}

/* vim in NORMAL mode at the start of text, then the keys; each '|' in
 * keys is a pause (an Esc must not run into the next key as Alt+key).
 * The results are vim 9.1's for the same keys (scratch oracle, -u NONE). */
static void vimrun(cl_edit *e, const char *text, const char *keys)
{
    char part[64];
    const char *p = keys;
    ed_set(e, text);
    e->vim = VIM_NORMAL;
    e->cur = 0;
    while (*p) {
        const char *bar = strchr(p, '|');
        long n = bar ? (long)(bar - p) : (long)strlen(p);
        memcpy(part, p, (size_t)n);
        part[n] = 0;
        type(e, part);
        p += n + (bar ? 1 : 0);
    }
}

static int vimcase(const char *text, const char *keys, const char *want)
{
    cl_edit e;
    int ok;
    ed_init(&e);
    ed_set_vim(&e, 1);
    vimrun(&e, text, keys);
    ok = !strcmp(e.b, want);
    if (!ok)
        printf("  vim \"%s\" %s: got \"%s\", want \"%s\"\n", text, keys, e.b, want);
    ed_free(&e);
    return ok;
}

/* R1-R3: vim's visual mode, '.', the quote and bracket objects, counts */
static void vim_visual_dot(void)
{
    static const char *s1[] = { "hello world", "\033", "0vll", 0 };
    static const char *s2[] = { "one\ntwo", "\033", "V", 0 };
    char line[32];
    cl_edit e;
    long a, z;
    /* text objects */
    CHECK(vimcase("say \"hello there\" (a (b c) d)", "fhdi\"", "say \"\" (a (b c) d)"));
    CHECK(vimcase("say \"hello there\" (a (b c) d)", "fhda\"", "say (a (b c) d)"));
    CHECK(vimcase("say \"hello there\" (a (b c) d)", "fbdi(", "say \"hello there\" (a () d)"));
    CHECK(vimcase("say \"hello there\" (a (b c) d)", "fb2di(", "say \"hello there\" ()"));
    CHECK(vimcase("say \"hello there\" (a (b c) d)", "fbca)x\033", "say \"hello there\" (a x d)"));
    CHECK(vimcase("x = {a, [1, 2]}", "f1di[", "x = {a, []}"));
    CHECK(vimcase("x = {a, [1, 2]}", "f1da{", "x = "));
    CHECK(vimcase("it's 'quoted' here", "fqci'new\033", "it's 'new' here"));
    CHECK(vimcase("one two three", "wd2aw", "one"));
    CHECK(vimcase("one two three four", "d3w", "four"));
    /* visual */
    CHECK(vimcase("one two three", "wvld", "one o three"));
    CHECK(vimcase("one two three", "veU", "ONE two three"));
    CHECK(vimcase("one two three", "vllrx", "xxx two three"));
    CHECK(vimcase("one two three", "wviwd", "one  three"));
    CHECK(vimcase("one\ntwo\nthree", "jVjd", "one"));
    CHECK(vimcase("one\ntwo\nthree", "VjJ", "one two\nthree"));
    CHECK(vimcase("one two three", "vey$vbp", "one two one"));
    CHECK(vimcase("one\ntwo\nthree", "Vj>", "  one\n  two\nthree"));    /* two blanks, Claude Code's indent */
    /* gaps 3: >> and << in NORMAL mode, with a count, and '.' after them */
    CHECK(vimcase("one\ntwo\nthree", ">>", "  one\ntwo\nthree"));
    CHECK(vimcase("one\ntwo\nthree", "2>>", "  one\n  two\nthree"));
    CHECK(vimcase("    one\ntwo", "<<", "  one\ntwo"));
    CHECK(vimcase("one\ntwo", ">>j.", "  one\n  two"));
    CHECK(vimcase("  one\ntwo", "<<<<", "one\ntwo"));       /* nothing left to take: stays */
    /* '.' and its count */
    CHECK(vimcase("abcdef", "x.", "cdef"));
    CHECK(vimcase("abcdef", "x3.", "ef"));
    CHECK(vimcase("abcdef", "2x.", "ef"));
    CHECK(vimcase("abcdef", "2x3.", "f"));
    CHECK(vimcase("one two three four", "cwfoo\033|w.", "foo foo three four"));
    CHECK(vimcase("a\nb\nc\nd", "dd.", "c\nd"));
    CHECK(vimcase("abcdefghij", "vlld.", "ghij"));
    CHECK(vimcase("one\ntwo\nthree\nfour", "Vjd.", ""));
    CHECK(vimcase("a b c d", "vlU.", "A b c d"));
    /* vimInsertModeRemaps "jj": the second j within a second leaves INSERT
     * and takes the first out; later, both stay as typed */
    ed_init(&e);
    ed_set_vim(&e, 1);
    strcpy(e.vremap, "jj");
    e.now_ms = 1000;
    type(&e, "hij");
    e.now_ms = 1400;
    type(&e, "j");
    CHECK_STR(e.b, "hi");
    CHECK_INT(e.vim, VIM_NORMAL);
    type(&e, "A");
    e.now_ms = 5000;
    type(&e, "j");
    e.now_ms = 6200;
    type(&e, "j");
    CHECK_STR(e.b, "hijj");
    CHECK_INT(e.vim, VIM_INSERT);
    ed_free(&e);
    /* o swaps the ends, v / V switch, v again leaves; u undoes a visual change */
    ed_init(&e);
    ed_set_vim(&e, 1);
    vimrun(&e, "one two three", "wvl");
    CHECK(vim_selection(&e, &a, &z) && a == 4 && z == 6);
    type(&e, "o");
    CHECK_INT(e.cur, 4);
    CHECK_INT(e.vanchor, 5);
    type(&e, "V");
    CHECK_INT(e.vim, VIM_VLINE);
    CHECK(vim_selection(&e, &a, &z) && a == 0 && z == 13);
    type(&e, "v");
    CHECK_INT(e.vim, VIM_VISUAL);
    type(&e, "v");
    CHECK_INT(e.vim, VIM_NORMAL);
    CHECK(!vim_selection(&e, &a, &z));
    type(&e, "v");
    type(&e, "\033");
    CHECK_INT(e.vim, VIM_NORMAL);
    type(&e, "veduu");
    CHECK_STR(e.b, "one two three");
    ed_free(&e);
    /* on the screen: the mode's word where -- INSERT -- is, the selection
     * in reverse */
    screen(60, 16, s1);
    ed_set_vim(&tui.ed, 1);
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    dump("vim visual");
    CHECK(!strncmp(cs_row(15), "  -- VISUAL --", 14));
    CHECK(h_cell(cs.vt, 4, 13)->attr & VT_ATTR_INVERSE);      /* "hel" */
    CHECK(h_cell(cs.vt, 6, 13)->attr & VT_ATTR_INVERSE);
    CHECK(!(h_cell(cs.vt, 7, 13)->attr & VT_ATTR_INVERSE));
    unscreen();
    cs_close();
    screen(60, 16, s2);
    ed_set_vim(&tui.ed, 1);
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    CHECK(!strncmp(cs_row(15), "  -- VISUAL LINE --", 19));
    unscreen();
    cs_close();
}

/* 1.3 / 1.4: ! and # turn the box into bash and memory mode */
static void bash_and_memory_box(void)
{
    static const char *s1[] = { "!!", "ls -l", 0 };
    static const char *s2[] = { "!!", "ls\r", 0 };
    static const char *s3[] = { "#", "note\r", 0 };
    static const char *s4[] = { "!!", "\177", 0 };
    static const char *s5[] = { "!!", "\033", 0 };
    char line[32];
    screen(60, 16, s1);
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    dump("bash mode");
    CHECK_INT(tui.box, BOX_BASH);
    CHECK_STR(tui.ed.b, "ls -l");
    CHECK(!strncmp(cs_row(13), V " ! ls -l", 10));
    CHECK_INT(h_cell(cs.vt, 0, 12)->fg, 13);        /* the frame in the bash colour */
    CHECK_INT(h_cell(cs.vt, 2, 13)->fg, 13);
    CHECK(!strncmp(cs_row(15), "  ! for bash mode (esc to leave)", 32));
    unscreen();
    cs_close();
    screen(60, 16, s2);
    tui_start(&tui);
    CHECK_INT(tui_read(&tui, line, sizeof(line)), 3);
    CHECK_STR(line, "!ls");
    CHECK_INT(tui.box, BOX_PROMPT);
    unscreen();
    cs_close();
    screen(60, 16, s3);
    tui_start(&tui);
    CHECK_INT(tui_read(&tui, line, sizeof(line)), 5);
    CHECK_STR(line, "#note");
    unscreen();
    cs_close();
    screen(60, 16, s4);
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    CHECK_INT(tui.box, BOX_PROMPT);
    unscreen();
    cs_close();
    screen(60, 16, s5);
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    CHECK_INT(tui.box, BOX_PROMPT);
    unscreen();
    cs_close();
}

/* 1.2 / R8: the @ list opens and narrows as the path is typed; Tab or
 * Enter takes the row; each directory read once a prompt */
static void at_completion(void)
{
    static const char *s1[] = { "see @al", 0 };
    static const char *s2[] = { "see @al", "\033[B", "\t", 0 };
    static const char *s3[] = { "@be", "\t", 0 };
    static const char *s4[] = { "see @al", "\033", "\t", 0 };
    static const char *s5[] = { "@a", "l", "p", "h", "\177", "\r", "\r", "@a", 0 };
    static const char *s6[] = { "@beta", "\r", 0 };
    static const char *s7[] = { "!!", "ls at/al", 0 };
    static const char *s8[] = { "!!", "ech", "\t", 0 };
    static cl_ui u;
    char line[32], d[600];
    tpath(d, "at");
    mkdir(d, 0700);
    tfile("at/alpha.txt", "a\n");
    tfile("at/beta", "b\n");
    tpath(d, "at/alpine");
    mkdir(d, 0700);
    tpath(d, "at");
    memset(&u, 0, sizeof(u));
    u.sys = &tsys;
    u.root = d;
    screen(60, 16, s1);
    tui.complete = input_complete;
    tui.cu = &u;
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    dump("@ list");
    /* typed, no Tab: the list is there, the text as typed */
    CHECK_STR(tui.ed.b, "see @al");
    CHECK_INT(tui.ncomp, 2);
    CHECK_STR(cs_row(14), "  @alpha.txt");
    CHECK_STR(cs_row(15), "  @alpine/");
    CHECK_INT(h_cell(cs.vt, 2, 14)->fg, 6);         /* the selection */
    unscreen();
    cs_close();
    screen(60, 16, s2);
    tui.complete = input_complete;
    tui.cu = &u;
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    CHECK_STR(tui.ed.b, "see @alpine/");
    unscreen();
    cs_close();
    screen(60, 16, s3);
    tui.complete = input_complete;
    tui.cu = &u;
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    CHECK_STR(tui.ed.b, "@beta ");
    unscreen();
    cs_close();
    /* Esc closes it; Tab then: the common start and the list again */
    screen(60, 16, s4);
    tui.complete = input_complete;
    tui.cu = &u;
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    CHECK_STR(tui.ed.b, "see @alp");
    CHECK_INT(tui.copen, 1);
    unscreen();
    cs_close();
    /* the screen's cache: four keys, one directory read; a sent prompt
     * makes it old (a new file is seen at the next prompt) */
    screen(60, 16, s5);
    u.tui = &tui;
    tui.complete = input_complete;
    tui.cu = &u;
    tui_start(&tui);
    CHECK_INT(tui_read(&tui, line, sizeof(line)), 11);     /* Enter took the row, Enter sent */
    CHECK_STR(line, "@alpha.txt ");
    CHECK_INT(tui.n_lists, 1);
    tfile("at/apple", "c\n");
    tui_read(&tui, line, sizeof(line));
    CHECK_INT(tui.n_lists, 2);
    CHECK_INT(tui.ncomp, 3);
    unscreen();
    cs_close();
    /* Enter on a row that is what is typed sends it */
    screen(60, 16, s6);
    u.tui = &tui;
    tui.complete = input_complete;
    tui.cu = &u;
    tui_start(&tui);
    CHECK_INT(tui_read(&tui, line, sizeof(line)), 5);
    CHECK_STR(line, "@beta");
    unscreen();
    cs_close();
    /* bash mode: a token with a '/' gets the list (no '@') */
    u.root = tdir;
    screen(60, 16, s7);
    u.tui = &tui;
    tui.complete = input_complete;
    tui.cu = &u;
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    dump("bash path list");
    CHECK_INT(tui.ncomp, 2);
    CHECK_STR(cs_row(14), "  at/alpha.txt");
    unscreen();
    cs_close();
    /* bash mode, Tab on a command: the newest earlier ! command so started */
    screen(60, 16, s8);
    tui.project = "P";
    hist_add(&tui.hist, &tsys, 0, "!echo hello", "P");
    hist_add(&tui.hist, &tsys, 0, "!echo other", "Q");
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    CHECK_STR(tui.ed.b, "echo hello");
    CHECK_INT(tui.box, BOX_BASH);
    unscreen();
    cs_close();
    u.tui = 0;
}

/* 1.5: Enter while a turn runs queues the prompt */
static void queue_while_busy(void)
{
    static const char *s1[] = { "!next one\r", 0 };
    static const char *s2[] = { "!/cost\r", "!two\r", 0 };
    static const char *s3[] = { "!a\r", "!b\r", "!\033[A", 0 };
    char *q;
    int spin;
    screen(60, 16, s1);
    tui_start(&tui);
    tui_busy(&tui, 1);
    CHECK_INT(tui_poll(&tui), 0);
    tui_tick(&tui);
    dump("queued");
    CHECK_INT(tui.nq, 1);
    spin = cs_find("esc to interrupt");
    CHECK(spin > 0 && !strcmp(cs_row(spin + 1), "  > next one"));
    CHECK(strstr(cs_row(15), "queued") != 0);
    CHECK(h_cell(cs.vt, 4, spin + 1)->attr & VT_ATTR_FAINT);
    q = tui_dequeue(&tui, 1);
    CHECK_STR(q ? q : "", "next one");
    free(q);
    tui_busy(&tui, 0);
    unscreen();
    cs_close();
    /* a command waits for the turn's end and goes alone */
    screen(60, 16, s2);
    tui_start(&tui);
    tui_busy(&tui, 1);
    tui_poll(&tui);
    tui_poll(&tui);
    CHECK_INT(tui.nq, 2);
    CHECK(tui_dequeue(&tui, 1) == 0);
    q = tui_dequeue(&tui, 0);
    CHECK_STR(q ? q : "", "/cost");
    free(q);
    q = tui_dequeue(&tui, 0);
    CHECK_STR(q ? q : "", "two");
    free(q);
    unscreen();
    cs_close();
    /* Up takes them back into the box */
    screen(60, 16, s3);
    tui_start(&tui);
    tui_busy(&tui, 1);
    tui_poll(&tui);
    tui_poll(&tui);
    tui_poll(&tui);
    CHECK_INT(tui.nq, 0);
    CHECK_STR(tui.ed.b, "a\nb");
    unscreen();
    cs_close();
}

/* the comment field seen while the menu is open (before Enter) */
static int note_row;
static void note_look(void)
{
    if (cs.next == 2 && cs_find("> ok but short") >= 0 && cs_find("enter answers with it") >= 0)
        note_row = 1;
}

/* R4-R7, R10: Ctrl+Enter / Ctrl+X Ctrl+S, Ctrl+S, Alt+Y, Ctrl+B, ?,
 * Alt+P, Shift+Tab and Tab on a permission question */
static void keys_rest(void)
{
    static const char *s_c1[] = { "\t", "ok but short", "\r", 0 };
    static const char *s_c2[] = { "\t", "\t", "nope", "\t", "\r", 0 };
    static const char *s_q1[] = { "!first\r", "!draft", "!\033[13;5u", 0 };
    static const char *s_q2[] = { "!first\r", "!more", "!\030\023", 0 };
    static const char *s_q3[] = { "!first\r", "!\033[13;5u", 0 };
    static const char *s_q4[] = { "!!", "!ls", "!\033[27;5;13~", 0 };
    static const char *s_q5[] = { "!/cost\r", "!!x\r", "!hi\r", "!\033[13;5u", 0 };
    static const char *s_b1[] = { "!\002", 0 };
    static const char *s_b2[] = { "!\033", 0 };
    static const char *s_b3[] = { "!x", 0 };
    static const char *s_b4[] = { "!\002", 0 };
    static const char *s_s1[] = { "draft one", "\023", 0 };
    static const char *s_s2[] = { "draft one", "\033[D", "\023", "\023", 0 };
    static const char *s_s3[] = { "!!", "ls", "\023", "\023", 0 };
    static const char *s_h1[] = { "?", 0 };
    static const char *s_h2[] = { "?", "x", 0 };
    static const char *s_p[] = { "draft", "\033p", 0 };
    static const char *s_m[] = { "\033[Z", 0 };
    static const char *const opt[] = { "Yes", "Yes, and don't ask again this session", "No" };
    char line[64];
    cl_key k;
    cl_edit e;
    /* the keys: Ctrl+Enter (kitty, modifyOtherKeys) is Enter with Ctrl;
     * Shift+Enter stays a newline */
    CHECK(key1("\033[13;5u", &k) && k.k == K_ENTER && k.mods == KM_CTRL);
    CHECK(key1("\033[27;5;13~", &k) && k.k == K_ENTER && k.mods == KM_CTRL);
    CHECK(key1("\033[13;6u", &k) && k.k == K_NEWLINE);
    /* Ctrl+Enter while Claude writes: the draft queued behind, the turn
     * stopped so the queue goes next */
    screen(60, 16, s_q1);
    tui_start(&tui);
    tui_busy(&tui, 1);
    CHECK_INT(tui_poll(&tui), 1);           /* (one poll reads all that was typed) */
    CHECK_INT(tui.nq, 2);
    CHECK_STR(tui.queue[1], "draft");
    CHECK_INT(tui.ed.n, 0);
    unscreen();
    cs_close();
    /* Ctrl+X Ctrl+S, the same in any terminal */
    screen(60, 16, s_q2);
    tui_start(&tui);
    tui_busy(&tui, 1);
    CHECK_INT(tui_poll(&tui), 1);
    CHECK_INT(tui.nq, 2);
    unscreen();
    cs_close();
    /* a command running in the foreground: it moves to the background, the
     * turn goes on (the messages go in after the tool round) */
    screen(60, 16, s_q3);
    tui_start(&tui);
    tui_busy(&tui, 1);
    CHECK_INT(tui_wait(&tui, 10), TW_BACKGROUND);
    CHECK_INT(tui.nq, 1);
    unscreen();
    cs_close();
    /* bash mode: the key only queues */
    screen(60, 16, s_q4);
    tui_start(&tui);
    tui_busy(&tui, 1);
    tui_poll(&tui);
    tui_poll(&tui);
    CHECK_INT(tui_poll(&tui), 0);
    CHECK_INT(tui.nq, 1);
    CHECK_STR(tui.queue[0], "!ls");
    unscreen();
    cs_close();
    /* a ! queued ahead of the messages: the turn is stopped */
    screen(60, 16, s_q5);
    tui_start(&tui);
    tui_busy(&tui, 1);
    CHECK_INT(tui_wait(&tui, 10), TW_STOP);
    CHECK_INT(tui.nq, 3);
    unscreen();
    cs_close();
    /* Ctrl+B while a command runs: background; Esc: stop; nothing: go on */
    screen(60, 16, s_b1);
    tui_start(&tui);
    tui_busy(&tui, 1);
    CHECK_INT(tui_wait(&tui, 10), TW_BACKGROUND);
    unscreen();
    cs_close();
    screen(60, 16, s_b2);
    tui_start(&tui);
    tui_busy(&tui, 1);
    CHECK_INT(tui_wait(&tui, 10), TW_STOP);
    unscreen();
    cs_close();
    screen(60, 16, s_b3);
    tui_start(&tui);
    tui_busy(&tui, 1);
    CHECK_INT(tui_wait(&tui, 10), TW_GO);
    CHECK_STR(tui.ed.b, "x");               /* typed ahead into the box */
    CHECK_INT(tui_wait(&tui, 10), TW_GO);
    unscreen();
    cs_close();
    /* Ctrl+B while Claude only writes: nothing to move, a word why */
    screen(60, 16, s_b4);
    tui_start(&tui);
    tui_busy(&tui, 1);
    CHECK_INT(tui_poll(&tui), 0);
    CHECK(strstr(tui.hint, "Nothing to move") != 0);
    unscreen();
    cs_close();
    /* Ctrl+S: the prompt put aside, the box empty; again: back, with its
     * cursor and its mode */
    screen(60, 16, s_s1);
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    dump("stash");
    CHECK_INT(tui.ed.n, 0);
    CHECK_STR(tui.stash ? tui.stash : "", "draft one");
    CHECK(strstr(cs_row(15), "Prompt stashed") != 0);
    unscreen();
    cs_close();
    screen(60, 16, s_s2);
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    CHECK_STR(tui.ed.b, "draft one");
    CHECK_INT(tui.ed.cur, 8);
    CHECK(tui.stash == 0);
    unscreen();
    cs_close();
    screen(60, 16, s_s3);
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    CHECK_STR(tui.ed.b, "ls");
    CHECK_INT(tui.box, BOX_BASH);
    unscreen();
    cs_close();
    /* Alt+Y after Ctrl+Y: the older kills in turn, round the ring */
    ed_init(&e);
    type(&e, "aaa bbb ccc");
    type(&e, "\027\027");                   /* Ctrl+W twice: "ccc", "bbb " */
    CHECK_STR(e.b, "aaa ");
    type(&e, "\033y");                      /* Alt+Y alone: nothing */
    CHECK_STR(e.b, "aaa ");
    type(&e, "\031");
    CHECK_STR(e.b, "aaa bbb ");
    type(&e, "\033y");
    CHECK_STR(e.b, "aaa ccc");
    type(&e, "\033y");
    CHECK_STR(e.b, "aaa bbb ");
    type(&e, "\037");                       /* undo: the Alt+Y before */
    CHECK_STR(e.b, "aaa ccc");
    type(&e, "x\033y");                     /* a key between: Alt+Y does nothing */
    CHECK_STR(e.b, "aaa cccx");
    ed_free(&e);
    /* ?: the shortcuts under the box; any key closes them */
    screen(80, 16, s_h1);
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    dump("shortcuts");
    CHECK_INT(tui.show_help, 1);
    CHECK(cs_find("! for bash mode") >= 0 && cs_find("ctrl+s to stash the prompt") >= 0);
    CHECK(cs_find("ctx:") < 0);
    CHECK_INT(tui.ed.n, 0);
    unscreen();
    cs_close();
    screen(80, 16, s_h2);
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    CHECK_INT(tui.show_help, 0);
    CHECK_STR(tui.ed.b, "x");
    CHECK(cs_find("ctx:") >= 0);
    unscreen();
    cs_close();
    /* Alt+P: /model comes back as a key's line, the draft stays */
    screen(60, 16, s_p);
    tui_start(&tui);
    CHECK_INT(tui_read(&tui, line, sizeof(line)), 6);
    CHECK_STR(line, "/model");
    CHECK_INT(tui.keycmd, 1);
    CHECK_STR(tui.ed.b, "draft");
    unscreen();
    cs_close();
    /* Shift+Tab on a file's permission question: allowed for the session */
    screen(70, 18, s_m);
    tui_start(&tui);
    tui.m_btab = 1;
    CHECK_INT(tui_menu(&tui, "Edit file", "q", opt, 3, 0, 2), 1);
    CHECK_INT(tui.m_btab, -1);
    unscreen();
    cs_close();
    /* Tab on Yes: a comment field; Enter answers Yes with it */
    screen(70, 18, s_c1);
    tui_start(&tui);
    tui.m_comment = 1;
    cs.before_read = note_look;
    CHECK_INT(tui_menu(&tui, "Edit file", "q", opt, 3, 0, 2), 0);
    cs.before_read = 0;
    CHECK_INT(note_row, 1);
    CHECK_STR(tui.m_note, "ok but short");
    CHECK_INT(tui.m_comment, 0);
    unscreen();
    cs_close();
    /* Tab on the middle option moves on; Tab on No, a comment, Tab closes
     * it (dropped), Enter: No without one */
    screen(70, 18, s_c2);
    tui_start(&tui);
    tui.m_comment = 1;
    CHECK_INT(tui_menu(&tui, "Edit file", "q", opt, 3, 1, 2), 2);
    CHECK_STR(tui.m_note, "");
    unscreen();
    cs_close();
}

/* R9: the viewer follows a resize while it is open */
static int rs_status, rs_seen;
static void rs_look(void)
{
    if (cs.next == 1)
        cs_resize(70, 14);
    if (cs.next == 2) {
        rs_seen = 1;
        rs_status = cs_find(" Transcript ");
    }
}

static void transcript_resize(void)
{
    static const char *script[] = { "\033[A", "x", "q", 0 };
    int k;
    screen(60, 12, script);
    tui_start(&tui);
    for (k = 1; k <= 40; k++) {
        char l[16];
        cl_copy(l, "line ", sizeof(l));
        cl_ltoa(k, l + 5);
        cl_cat(l, "\n", sizeof(l));
        tui_lines(&tui, l, (long)strlen(l));
    }
    cs.before_read = rs_look;
    tui_transcript(&tui);
    cs.before_read = 0;
    CHECK_INT(rs_seen, 1);
    CHECK_INT(rs_status, 13);           /* the status row on the new last row */
    CHECK_INT(tui.rows, 14);
    CHECK(cs_find(PROMPT) >= 0);        /* back: the footer drawn at the new size */
    unscreen();
    cs_close();
}

/* 1.6: Ctrl+O's transcript viewer, looked at while it is open */
static int tv_seen, tv_rev, tv_hidden, tv_status;
static void tv_look(void)
{
    int r;
    if (cs.next != 3)                   /* before "q": the search is done */
        return;
    tv_seen = 1;
    r = cs_find("line 5");
    tv_rev = r >= 0 && cs_find("line 50") < 0 && (h_cell(cs.vt, 0, r)->attr & VT_ATTR_INVERSE);
    tv_hidden = cs_find("     in full 7") >= 0 || cs_find("detail") >= 0;
    tv_status = cs_find(" Transcript ") == cs.rows - 1;
}

static void transcript_view(void)
{
    static const char *script[] = { "\033[A", "/line 5", "\r", "q", 0 };
    int k;
    long t0;
    screen(60, 12, script);
    tui_start(&tui);
    for (k = 1; k <= 40; k++) {
        char l[16];
        cl_copy(l, "line ", sizeof(l));
        cl_ltoa(k, l + 5);
        cl_cat(l, "\n", sizeof(l));
        tui_lines(&tui, l, (long)strlen(l));
    }
    tui_log(&tui, "the hidden detail\n", 18);
    t0 = (long)cs.sent.n;
    cs.before_read = tv_look;
    tui_transcript(&tui);
    cs.before_read = 0;
    CHECK_INT(tui.n_views, 1);
    CHECK_INT(tv_seen, 1);
    CHECK_INT(tv_rev, 1);
    CHECK_INT(tv_status, 1);
    /* the alternate screen in and out; Up scrolled by one SD and one row */
    CHECK(strstr(cs.sent.p + t0, "\033[?1049h") != 0);
    CHECK(strstr(cs.sent.p + t0, "\033[?2026h\033[T") != 0);
    CHECK(strstr(cs.sent.p + t0, "\033[?1049l") != 0);
    /* back: the transcript and the footer as they were */
    CHECK(cs_find("line 40") >= 0);
    CHECK(cs_find(PROMPT) >= 0);
    CHECK(cs_find("Transcript") < 0);
    (void)tv_hidden;
    unscreen();
    cs_close();
}

/* 1.7: Esc Esc: the draft cleared, or (empty box) the rewind menu */
static int rewinds;
static void rw_cb(void *u)
{
    (void)u;
    rewinds++;
}

static int menu_seen;
static void rw_look(void)
{
    if (cs.next == 0 && cs_find("Rewind") >= 0 && cs_find("1. first q") >= 0 && cs_find(PROMPT " 2. second q") >= 0)
        menu_seen = 1;
}

static void esc_esc(void)
{
    static const char *s1[] = { "\033", "\033", 0 };
    static const char *s2[] = { "draft", "\033", "\033", "\033[A", 0 };
    static const char *s3[] = { "\r", "\r", 0 };
    static cl_ui u;
    char line[32];
    cl_conv c;
    screen(60, 16, s1);
    tui.on_rewind = rw_cb;
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    CHECK_INT(rewinds, 1);
    unscreen();
    cs_close();
    screen(60, 16, s2);
    tui.on_rewind = rw_cb;
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    CHECK_INT(rewinds, 1);
    CHECK_STR(tui.ed.b, "draft");           /* cleared, then Up brought it back */
    unscreen();
    cs_close();
    /* the menu over the conversation (the stub until checkpoints) */
    conv_init(&c);
    conv_add_user_text(&c, "first q", 7);
    conv_add(&c, 0, "[{\"type\":\"text\",\"text\":\"a1\"}]", 29);
    conv_add_user_text(&c, "second q", 8);
    conv_add(&c, 0, "[{\"type\":\"text\",\"text\":\"a2\"}]", 29);
    screen(60, 16, s3);
    memset(&u, 0, sizeof(u));
    u.tui = &tui;
    u.show = &shw;
    tui_start(&tui);
    ui_attach(&u, &tsys, tdir, &c);
    cs.before_read = rw_look;
    ui_rewind(&u);
    cs.before_read = 0;
    dump("rewound");
    CHECK_INT(menu_seen, 1);
    CHECK_INT(c.n, 2);                      /* back to before "second q" */
    CHECK_STR(tui.ed.b, "second q");        /* the prompt back in the box */
    CHECK(cs_find("Rewound the conversation.") >= 0);
    conv_free(&c);
    unscreen();
    cs_close();
}

/* 1.9: thinking: one dim line, the text in the transcript viewer */
static void thinking_shown(void)
{
    static const char *script[] = { 0 };
    cl_render r;
    int row;
    screen(60, 20, script);
    tui_start(&tui);
    show_render(&shw, &r);
    show_think(&shw, "Let me think about it.", 22);
    cs.clock += 3000;
    r.text(r.u, "Answer.", 7);
    r.end(r.u);
    dump("thinking");
    row = cs_find("Thought for 3s (ctrl+o to show thinking)");
    CHECK(row >= 0);
    CHECK(row >= 0 && (h_cell(cs.vt, 2, row)->attr & VT_ATTR_FAINT));
    CHECK(cs_find("Let me think") < 0);
    CHECK(strstr(tui.log.p, "Let me think about it.") != 0);
    CHECK(cs_find(BULLET " Answer.") > row);
    CHECK_INT(shw.n_think, 1);
    unscreen();
    cs_close();
}

/* 1.10: themes */
static const char *set_k, *set_v;
static void set_cb(void *u, const char *k, const char *v)
{
    (void)u;
    set_k = k;
    set_v = v;
}

static void themes(void)
{
    static const char *script[] = { 0 };
    static cl_ui u;
    jw x;
    screen(60, 16, script);
    tui.th = theme_get("monochrome");
    tui_start(&tui);
    CHECK_INT(h_cell(cs.vt, 0, 12)->fg, VT_COLOR_DEFAULT);     /* no colour at all */
    show_preview(&shw, T_EDIT, "a", "x\n", 2, "y\n", 2);
    CHECK(h_cell(cs.vt, 8, cs_find("1 - x"))->attr & VT_ATTR_INVERSE);
    CHECK(theme_get("nonsense") == &cl_themes[0]);
    /* /theme NAME: the screen's colours and the setting */
    memset(&u, 0, sizeof(u));
    u.tui = &tui;
    u.show = &shw;
    u.sys = &tsys;
    u.set_setting = set_cb;
    jw_init(&x);
    CHECK_INT(ui_input(&u, "/theme light", &x), IN_DONE);
    CHECK_STR(tui.th->name, "light");
    CHECK_STR(set_k ? set_k : "", "theme");
    CHECK_STR(set_v ? set_v : "", "light");
    CHECK(cs_find("Theme: Light mode") >= 0);
    tui_busy(&tui, 1);
    CHECK_INT(h_cell(cs.vt, 0, cs_find("esc to interrupt"))->fg, 1);   /* the light spinner: red */
    tui_busy(&tui, 0);
    CHECK_INT(ui_input(&u, "/vim", &x), IN_DONE);
    CHECK_INT(tui.ed.vim, VIM_INSERT);
    CHECK_STR(set_v ? set_v : "", "vim");
    jw_free(&x);
    unscreen();
    cs_close();
}

/* 1.11: the bell, OSC 9 and the window title */
static void notifications(void)
{
    static const char *script[] = { 0 };
    int b0;
    screen(60, 16, script);
    tui_start(&tui);
    CHECK_STR(vt_title(cs.vt), "Claude");
    tui_busy(&tui, 1);
    CHECK_STR(vt_title(cs.vt), "Claude - working");
    b0 = h_bells;
    tui_busy(&tui, 0);
    CHECK_INT(h_bells, b0);                 /* a short turn: no bell */
    tui_busy(&tui, 1);
    cs.clock += 12000;
    tui_busy(&tui, 0);
    CHECK_INT(h_bells, b0 + 1);
    CHECK(strstr(cs.sent.p, "\033]9;Claude is waiting for your input\a") != 0);
    CHECK_STR(vt_title(cs.vt), "Claude");
    unscreen();
    /* the title stack: pushed at the start, popped at the end */
    CHECK(strstr(cs.sent.p, "\033[22;0t") != 0);
    CHECK(strstr(cs.sent.p + cs.sent.n - 40, "\033[23;0t") != 0);
    cs_close();
}

/* 1.12: Ctrl+L redraws, Ctrl+G edits in the editor; Ctrl+T the todo list */
static char ed_saw[64];
static int ed_raw;
static int edit_stub(void *u, const char *path)
{
    char *b = 0;
    long n = 0;
    (void)u;
    ed_raw = cs.raw_on;
    if (tsys.read(tsys.u, path, 100, &b, &n) == 0)
        cl_copy(ed_saw, b, sizeof(ed_saw));
    free(b);
    return tsys.write(tsys.u, path, "from the editor\n", 16);
}

static void redraw_editor_todos(void)
{
    static const char *s1[] = { "\014", 0 };
    static const char *s2[] = { "draft", "\007", 0 };
    static const char *s3[] = { "\024", 0 };
    static const char todo[] = "{\"todos\":[{\"content\":\"Read\",\"status\":\"completed\"},"
                               "{\"content\":\"Edit\",\"status\":\"in_progress\"},{\"content\":\"Tell\",\"status\":\"pending\"}]}";
    char line[32], p[600];
    screen(40, 12, s1);
    tui_start(&tui);
    tui_lines(&tui, "one\ntwo\n", 8);
    vt_write(cs.vt, (const vt_u8 *)"\033[r\033[H\033[2JGARBAGE", 18);
    tui_read(&tui, line, sizeof(line));
    dump("ctrl+l");
    CHECK_STR(cs_row(0), "one");
    CHECK_STR(cs_row(1), "two");
    CHECK(cs_find("GARBAGE") < 0);
    CHECK(cs_find(PROMPT) >= 0);
    CHECK_INT(tui.n_redraws, 1);
    unscreen();
    cs_close();
    screen(60, 16, s2);
    io.edit = edit_stub;
    tpath(p, "prompt.txt");
    tui.edit_path = p;
    tui.sys = &tsys;
    tui_start(&tui);
    tui_read(&tui, line, sizeof(line));
    CHECK_STR(ed_saw, "draft");
    CHECK_INT(ed_raw, 0);                   /* raw mode off for the editor */
    CHECK_INT(cs.raw_on, 1);
    CHECK_STR(tui.ed.b, "from the editor");
    io.edit = 0;
    unscreen();
    cs_close();
    screen(60, 16, s3);
    tui_start(&tui);
    tui_set_todos(&tui, todo, (long)strlen(todo));
    tui_read(&tui, line, sizeof(line));
    dump("ctrl+t");
    CHECK_STR(cs_row(12), "  \342\234\224 Read");
    CHECK_STR(cs_row(13), "  \342\226\240 Edit");
    CHECK_STR(cs_row(14), "  \342\227\213 Tell");
    CHECK(strstr(cs_row(15), "ctx:") != 0);
    CHECK(h_cell(cs.vt, 4, 13)->attr & VT_ATTR_BOLD);
    unscreen();
    cs_close();
    (void)col_of;
}

/* ---- A4 gaps 3: the screen's rows (ledger 2026-10-05-a4-gaps3-tui-progress) ---- */

/* G5: Alt+T on a model that always thinks changes nothing and says so; on
 * one that may go without it flips the session's flag */
static void gaps3_think(void)
{
    static const char *s1[] = { "\033t", 0 };
    static const char *s2[] = { "\033t", "\033t", "\033t", 0 };
    char line[32];
    int off = 0;
    screen(60, 16, s1);
    tui.think_off = &off;
    tui_start(&tui);
    CHECK_INT(tui_read(&tui, line, sizeof(line)), -1);
    CHECK(cs_find("Thinking can't be turned off for this model") >= 0);
    CHECK_INT(off, 0);
    unscreen();
    cs_close();
    screen(60, 16, s2);
    tui.model = "claude-sonnet-4-6";
    tui.think_off = &off;
    tui_start(&tui);
    CHECK_INT(tui_read(&tui, line, sizeof(line)), -1);
    CHECK_INT(off, 1);                      /* three presses: off, on, off */
    CHECK(cs_find("Thinking off") >= 0);
    unscreen();
    cs_close();
}

/* G6: /add-dir and /cd: the argument's directory suggestions, as typed
 * and with Tab; files are not offered */
static void dirs_run(const char **script, cl_ui *u, char *line)
{
    screen(60, 16, script);
    tui.complete = input_complete;
    tui.cu = u;
    u->tui = &tui;
    tui_start(&tui);
    tui_read(&tui, line, 32);
}

static void gaps3_dirs(void)
{
    static const char *s1[] = { "/add-dir al", 0 };
    static const char *s2[] = { "/add-dir al", "\t", 0 };
    static const char *s3[] = { "/cd b", "\t", 0 };
    static const char *s4[] = { "/add-dir ", 0 };
    static const char *s5[] = { "/add-dir ", "\t", 0 };
    static const char *s6[] = { "/add-dir al", "\r", "\r", 0 };
    static cl_ui u;
    char line[32], d[600];
    tpath(d, "ad");
    mkdir(d, 0700);
    tfile("ad/alpha.txt", "a\n");
    tpath(d, "ad/alpine");
    mkdir(d, 0700);
    tpath(d, "ad/beta");
    mkdir(d, 0700);
    tpath(d, "ad");
    memset(&u, 0, sizeof(u));
    u.sys = &tsys;
    u.root = d;
    dirs_run(s1, &u, line);
    dump("add-dir list");
    CHECK_INT(tui.copen, 1);
    CHECK_INT(tui.ncomp, 1);                /* alpine/, not alpha.txt */
    CHECK_STR(cs_row(15), "  alpine/");
    unscreen();
    cs_close();
    dirs_run(s2, &u, line);
    CHECK_STR(tui.ed.b, "/add-dir alpine/");
    unscreen();
    cs_close();
    dirs_run(s3, &u, line);
    CHECK_STR(tui.ed.b, "/cd beta/");
    unscreen();
    cs_close();
    dirs_run(s4, &u, line);
    CHECK_INT(tui.copen, 0);                /* nothing begun: no list */
    unscreen();
    cs_close();
    dirs_run(s5, &u, line);
    CHECK_INT(tui.copen, 1);                /* Tab: all of them */
    CHECK_INT(tui.ncomp, 2);
    unscreen();
    cs_close();
    dirs_run(s6, &u, line);
    CHECK_STR(line, "/add-dir alpine/");    /* Enter takes the row first, then sends */
    unscreen();
    cs_close();
}

void suite_claude_tui(void)
{
    keys();
    editor();
    idle_prompt();
    status_line_row();
    typing_and_slash_menu();
    permission_menu();
    spinner();
    answer_and_tools();
    edit_diff_and_todos();
    grow_over_transcript();
    welcome_mascot();
    mk_tdir();
    history_and_search();
    editor_words_undo();
    vim_mode();
    vim_visual_dot();
    bash_and_memory_box();
    at_completion();
    queue_while_busy();
    keys_rest();
    transcript_view();
    transcript_resize();
    esc_esc();
    thinking_shown();
    themes();
    notifications();
    redraw_editor_todos();
    gaps3_think();
    gaps3_dirs();
    rm_tdir();
}
