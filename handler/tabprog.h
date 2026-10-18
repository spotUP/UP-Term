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

/* The script: no FailAt stop, pre ("" none), the program; when it fails
 * (rc 5 or more: WARN) its message stays up until Return; then EndCLI
 * closes the tab. 1 when it fits in out[size], 0 when not. */
static int tabprog_script(char *out, unsigned long size, const char *pre, const char *file)
{
    static const char head[] = "FailAt 2147483647\n";
    static const char tail[] = "\nIf WARN\nAsk \"Press Return to close this tab.\"\nEndIf\nEndCLI >NIL:\n";
    unsigned long n = sizeof(head) - 1 + strlen(pre) + strlen(file) + sizeof(tail);
    if (n > size)
        return 0;
    strcpy(out, head);
    strcat(out, pre);
    strcat(out, file);
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
