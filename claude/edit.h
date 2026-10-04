/* edit -- C:Claude's input line (ledger A3): a multi-line UTF-8 buffer
 * with Emacs keys as Claude Code has them (Ctrl+A/E/K/U/W/Y, Alt+B/F,
 * Alt+Backspace), the history on Up/Down when the cursor is on the first
 * or last line, a paste inserted whole, and the layout of the text in a
 * box of a given width (wrapped by character, as Claude Code's box does).
 * Portable C89, host-tested (tests/test_claude_tui.c). */
#ifndef CL_EDIT_H
#define CL_EDIT_H

#include "keys.h"

#define ED_HIST 100

typedef struct cl_edit {
    char *b;            /* the text, terminated; '\n' between its lines */
    long n, cap;
    long cur;           /* the cursor, a byte offset at a character's start */
    char *hist[ED_HIST];
    int nh;
    int hpos;           /* nh: the line being typed (kept in draft) */
    char *draft;
    char *kill;         /* what Ctrl+K/U/W took, for Ctrl+Y */
    int oom;
} cl_edit;

void ed_init(cl_edit *e);
void ed_free(cl_edit *e);
void ed_clear(cl_edit *e);
void ed_set(cl_edit *e, const char *s);
void ed_insert(cl_edit *e, const char *s, long n);
/* One key: 1 when the text or the cursor changed, 0 when the key is not
 * the editor's (Enter, Esc, Tab, Ctrl+C...). */
int ed_key(cl_edit *e, const cl_key *k);
/* the text becomes a history entry (an empty one or a repeat is not) */
void ed_remember(cl_edit *e, const char *s);

/* The text in rows of at most width columns: row r is b[a[r]..z[r]),
 * its lines ending at '\n' or wrapped. Fills up to max rows; returns the
 * row count; *crow, *ccol the cursor's row and column. */
int ed_layout(const cl_edit *e, int width, long *a, long *z, int max, int *crow, int *ccol);

#endif
