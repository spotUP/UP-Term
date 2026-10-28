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

/* The programs a tab can run. TAB_SHELL is a shell, not a program. */
enum { TAB_SHELL = 0, TAB_DEMO, TAB_CLAUDE, TAB_TUTORIAL, TAB_PI };

/* The AI agents: the one table from the ai-agent preference's word (Amarchy's settings file) to the tab that
 * runs it. MENU_AGENT (super+l) opens the one the preference names; MENU_CLAUDE and MENU_PI open theirs. */
struct tabprog_agent {
    const char *name;
    int tab;
};
static const struct tabprog_agent tabprog_agents[] = { { "claude", TAB_CLAUDE }, { "pi", TAB_PI } };
#define TABPROG_NAGENTS (sizeof(tabprog_agents) / sizeof(tabprog_agents[0]))

/* The tab of the agent `name` (any case); an empty or unknown word is the default, the first row (Claude). */
static int tabprog_agent_tab(const char *name)
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

/* Help > Shell tutorial and /tutorial: a tab running the tutorial script
 * under vsh (the kit puts both in UP-Term:bin, as upkg-ask). The file is
 * what the handler looks for first; the command is the script's one line.
 * 80x24: mdv sets the lessons for the window's width. */
#define TABPROG_TUTORIAL_FILE "UP-Term:bin/tutorial"
#define TABPROG_TUTORIAL_CMD "UP-Term:bin/vsh UP-Term:bin/tutorial next"
#define TABPROG_TUTORIAL_COLS 80
#define TABPROG_TUTORIAL_ROWS 24

/* The script: no FailAt stop, pre ("" none), the command line; when it fails
 * (rc 5 or more: WARN) its message stays up until Return; then EndCLI
 * closes the tab. 1 when it fits in out[size], 0 when not. */
static int tabprog_script(char *out, unsigned long size, const char *pre, const char *cmd)
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
static int tabprog_grow(int cols, int rows, int need_cols, int need_rows, int bar_rows, int *want_cols,
                        int *want_rows)
{
    *want_cols = cols < need_cols ? need_cols : cols;
    *want_rows = rows - bar_rows < need_rows ? need_rows + bar_rows : rows;
    return *want_cols != cols || *want_rows != rows;
}

#endif
