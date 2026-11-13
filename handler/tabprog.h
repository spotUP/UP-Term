/* A tab that runs one program instead of a shell (Help > Demo tour runs
 * C:UPDemo, UP-Term > Claude runs C:Claude): the FROM script its NewShell
 * runs, and how big the window must be first. Pure, no OS calls, so the
 * host suite pins both (tests/test_tabprog.c).
 *
 * The A1200, 2026-10-18: Demo tour in a window under UPDemo's 76x20 only
 * redrew the window. UPDemo said "the window is WxH, it needs 76x20" and
 * ended, and the script's EndCLI closed the tab before anyone could read
 * it. Now the window grows to the program's size first (as far as the
 * screen allows), and a program that fails anyway keeps its tab open
 * until Return. */
#ifndef TABPROG_H
#define TABPROG_H

#include <string.h>
#include "../shell/vsh_quote.h"
#include "../demo/updemo.h"
#include "menu_ids.h"

/* Header-only and read by the handler, Patina's menus and the host suite, each using a few of these: unused is
 * not an error (the config/upassign.h pattern). */
#if defined(__GNUC__) || defined(__clang__)
#define TABPROG_FN static __attribute__((unused))
#else
#define TABPROG_FN static
#endif

/* The programs a tab can run. TAB_SHELL is a shell, not a program. */
enum { TAB_SHELL = 0, TAB_DEMO, TAB_CLAUDE, TAB_TUTORIAL, TAB_PI };

/* The AI agents: the one table from the ai-agent preference's word (Patina's settings file) to the tab that
 * runs it. MENU_AGENT (super+l) opens the one the preference names; MENU_CLAUDE and MENU_PI open theirs. */
struct tabprog_agent {
    const char *name;
    int tab;
};
static const struct tabprog_agent tabprog_agents[] = { { "claude", TAB_CLAUDE }, { "pi", TAB_PI } };
#define TABPROG_NAGENTS (sizeof(tabprog_agents) / sizeof(tabprog_agents[0]))

/* The tab of the agent `name` (any case); an empty or unknown word is the default, the first row (Claude). */
TABPROG_FN int tabprog_agent_tab(const char *name)
{
    unsigned long i;
    const char *a, *b;
    for (i = 0; name && i < TABPROG_NAGENTS; i++) {
        for (a = name, b = tabprog_agents[i].name; *a && *b; a++, b++)
            if ((*a | 0x20) != *b)
                break;
        if (!*a && !*b)
            return tabprog_agents[i].tab;
    }
    return tabprog_agents[0].tab;
}

/* The agent's command line with a task: `cmd` and the task as one quoted item ("C:Claude "fix the *"bug*""),
 * the first message of an interactive session (C:Claude: claude/main_amiga.c, its PROMPT; amiga-pi: args.c,
 * its [prompt ...]). A null or empty task is `cmd` alone. 1 when it fits in out[size], 0 when not. */
TABPROG_FN int tabprog_task_cmd(char *out, unsigned long size, const char *cmd, const char *task)
{
    unsigned long n = strlen(cmd);
    if (n + 1 > size)
        return 0;
    strcpy(out, cmd);
    if (!task || !task[0])
        return 1;
    if (n + 1 + 2 * strlen(task) + 3 > size) /* the worst case: every character escaped, two quotes, the NUL */
        return 0;
    out[n++] = ' ';
    n = (unsigned long)(vsh_put_word(out + n, task, 1) - out);
    out[n] = 0;
    return 1;
}

/* Help > Shell tutorial and /tutorial: a tab running the tutorial script
 * under vsh (the kit puts both in UP-Term:bin, as upkg-ask). The file is
 * what the handler looks for first; the command is the script's one line.
 * 80x24: mdv sets the lessons for the window's width. */
#define TABPROG_TUTORIAL_FILE "UP-Term:bin/tutorial"
#define TABPROG_TUTORIAL_CMD "UP-Term:bin/vsh UP-Term:bin/tutorial next"
#define TABPROG_TUTORIAL_COLS 80
#define TABPROG_TUTORIAL_ROWS 24

/* A tab that runs one program instead of a shell: Help > Demo tour (UPDemo)
 * and UP-Term > Claude (C:Claude). The tab closes when the program ends.
 * `file` is looked up first: a program that is not there is said in a
 * requester, as Preferences... says it of the editor. The one table: the
 * handler's tabs and Patina's new windows (patina/uterm_dos.c) both read
 * it, so a Claude window from the menu starts as a Claude tab does. */
struct tab_prog {
    const char *file;    /* C:... the program */
    const char *cmd;     /* the script's command line (the program, or vsh and its script) */
    const char *script;  /* T:... the FROM script that runs it */
    const char *pre;     /* a script line before it ("" none): Claude refuses a stack under 16000 */
    const char *missing; /* the requester's text when file is not there */
    int cols, rows;      /* the grid it needs (0 0: any); the window grows to it first */
};
static const struct tab_prog tab_progs[] = {
    { 0, 0, 0, "", 0, 0, 0 },
    { "C:UPDemo", "C:UPDemo", "T:UP-Term-demo", "",
      "The demo is not installed:\nC:UPDemo was not found.\n\nInstall UP-Term from its archive to get it.",
      UPDEMO_MIN_COLS, UPDEMO_MIN_ROWS },
    { "C:Claude", "C:Claude", "T:UP-Term-claude", "Stack 32768\n",
      "Claude is not installed:\nC:Claude was not found.\n\nInstall UP-Term from its archive to get it.", 0, 0 },
    { TABPROG_TUTORIAL_FILE, TABPROG_TUTORIAL_CMD, "T:UP-Term-tutorial", "",
      "The tutorial is not installed:\nUP-Term:bin/tutorial was not found.\n\nInstall UP-Term from its archive to get it.",
      TABPROG_TUTORIAL_COLS, TABPROG_TUTORIAL_ROWS },
    /* amiga-pi (the up-pi package) takes the same stack cookie as Claude */
    { "C:amiga-pi", "C:amiga-pi", "T:UP-Term-pi", "Stack 32768\n",
      "pi is not installed:\nC:amiga-pi was not found.\n\nInstall the up-pi package to get it.", 0, 0 }
};

/* The program a menu item runs in a tab of its own (the handler's menu, and
 * a Patina menuid: row): TAB_SHELL for every item that is not one. agent:
 * the ai-agent preference's word, for MENU_AGENT. */
TABPROG_FN int tabprog_menu_tab(long id, const char *agent)
{
    switch (id) {
    case MENU_DEMO:
        return TAB_DEMO;
    case MENU_CLAUDE:
        return TAB_CLAUDE;
    case MENU_PI:
        return TAB_PI;
    case MENU_TUTORIAL:
        return TAB_TUTORIAL;
    case MENU_AGENT:
        return tabprog_agent_tab(agent);
    default:
        return TAB_SHELL;
    }
}

/* A shell tab's FROM script: no FailAt stop, the CD into dir (0 none; a
 * quote cannot be in it), then vsh (0: none, the AmigaDOS Shell stays) and
 * EndCLI, so the window closes with vsh -- whatever vsh returns: its last
 * command's status (127 for a name not found) failed the script at FailAt
 * 10, and the tab stayed open on the Shell's prompt. 1 when it fits in
 * out[size], 0 when not. */
TABPROG_FN int tabprog_shell_script(char *out, unsigned long size, const char *dir, const char *vsh)
{
    static const char head[] = "FailAt 2147483647\n";
    static const char tail[] = "\nEndCLI >NIL:\n";
    unsigned long n = sizeof(head) + (dir ? strlen(dir) + 6 : 0) + (vsh ? strlen(vsh) + sizeof(tail) : 0);
    if (n > size)
        return 0;
    strcpy(out, head);
    if (dir) {
        strcat(out, "CD \"");
        strcat(out, dir);
        strcat(out, "\"\n");
    }
    if (vsh) {
        strcat(out, vsh);
        strcat(out, tail);
    }
    return 1;
}

/* The NewShell line of a new window of its own: spec, the CELLS option when
 * cols is not 0 (the program's grid), and FROM script (0: none). 1 when it
 * fits in out[size], 0 when not. */
TABPROG_FN int tabprog_window_line(char *out, unsigned long size, const char *spec, int cols, int rows,
                               const char *script)
{
    char cells[32];
    unsigned long n;
    int i = 0, k, v;
    cells[0] = 0;
    if (cols > 0 && rows > 0) {
        strcpy(cells, "/CELLS ");
        i = 7;
        for (k = 0; k < 2; k++) {
            char d[8];
            int j = 0;
            v = k ? rows : cols;
            do {
                d[j++] = (char)('0' + v % 10);
                v /= 10;
            } while (v && j < 7);
            while (j)
                cells[i++] = d[--j];
            if (!k)
                cells[i++] = 'x';
        }
        cells[i] = 0;
    }
    n = 11 + strlen(spec) + strlen(cells) + (script ? 6 + strlen(script) : 0);
    if (n > size)
        return 0;
    strcpy(out, "NewShell \"");
    strcat(out, spec);
    strcat(out, cells);
    strcat(out, "\"");
    if (script) {
        strcat(out, " FROM ");
        strcat(out, script);
    }
    return 1;
}

/* The script: no FailAt stop, pre ("" none), the command line; when it fails
 * (rc 5 or more: WARN) its message stays up until Return; then EndCLI
 * closes the tab. 1 when it fits in out[size], 0 when not. */
TABPROG_FN int tabprog_script(char *out, unsigned long size, const char *pre, const char *cmd)
{
    static const char head[] = "FailAt 2147483647\n";
    static const char tail[] = "\nIf WARN\nAsk \"Press Return to close this tab.\"\nEndIf\nEndCLI >NIL:\n";
    unsigned long n = sizeof(head) - 1 + strlen(pre) + strlen(cmd) + sizeof(tail);
    if (n > size)
        return 0;
    strcpy(out, head);
    strcat(out, pre);
    strcat(out, cmd);
    strcat(out, tail);
    return 1;
}

/* The grid the window must have before the tab opens, so that the tab's
 * grid is at least need_cols x need_rows: cols x rows the grid now,
 * bar_rows the rows the tab bar takes when this tab brings it up (0 when
 * it is up already). 1 with *want_cols x *want_rows when the window must
 * grow; 0 when it is big enough (it never shrinks). */
TABPROG_FN int tabprog_grow(int cols, int rows, int need_cols, int need_rows, int bar_rows, int *want_cols,
                        int *want_rows)
{
    *want_cols = cols < need_cols ? need_cols : cols;
    *want_rows = rows - bar_rows < need_rows ? need_rows + bar_rows : rows;
    return *want_cols != cols || *want_rows != rows;
}

#endif
