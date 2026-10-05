/* edit -- C:Claude's input line (ledger A3): a multi-line UTF-8 buffer
 * with Emacs keys as Claude Code has them (Ctrl+A/E/K/U/W/Y, Alt+B/F/D,
 * Alt+Backspace, Ctrl+_ undo), the history on Up/Down when the cursor is
 * on the first or last line, a paste inserted whole, and the layout of the
 * text in a box of a given width (wrapped by character, as Claude Code's
 * box does). Vim mode (A4 1.8, /vim): INSERT and NORMAL, vim.c.
 * Portable C89, host-tested (tests/test_claude_tui.c). */
#ifndef CL_EDIT_H
#define CL_EDIT_H

#include "keys.h"

#define ED_HIST 100
#define ED_UNDO 24

/* vim mode: off, or the mode the editor is in */
enum { VIM_OFF, VIM_INSERT, VIM_NORMAL };

typedef struct ed_snapshot {
    char *b;
    long cur;
} ed_snapshot;

typedef struct cl_edit {
    char *b;            /* the text, terminated; '\n' between its lines */
    long n, cap;
    long cur;           /* the cursor, a byte offset at a character's start */
    char *hist[ED_HIST];
    int nh;
    int hpos;           /* nh: the line being typed (kept in draft) */
    char *draft;
    char *kill;         /* what Ctrl+K/U/W took, for Ctrl+Y (vim: the register) */
    int kill_lines;     /* the register holds whole lines (vim dd, yy) */
    ed_snapshot undo[ED_UNDO];
    int nundo;
    int lastk;          /* the last key's kind: typed characters undo as one */
    /* vim (vim.c) */
    int vim;            /* VIM_* */
    int vcount, vopcount;   /* the count typed so far, the operator's */
    int vop;            /* a pending operator: 'd' 'c' 'y' '>' '<', 0 none */
    int vpend;          /* a pending key: 'f' 'F' 't' 'T' 'r' 'g' 'i' 'a', 0 none */
    int vlastf, vlastc; /* the last f/F/t/T and its character (; and ,) */
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
/* Ctrl+_ / vim u: the text and cursor before the last change */
void ed_snap(cl_edit *e);
int ed_undo(cl_edit *e);
/* the pieces vim.c builds on */
void ed_cut(cl_edit *e, long a, long z, int keep);
long ed_prev(const cl_edit *e, long i);
long ed_next(const cl_edit *e, long i);
long ed_lstart(const cl_edit *e, long i);
long ed_lend(const cl_edit *e, long i);
void ed_hist_go(cl_edit *e, int to);
/* vim.c: one key in NORMAL mode: 1 handled, 0 not the editor's (Enter,
 * a key the screen owns), -1 left to the common keys (arrows, Home...) */
int vim_normal(cl_edit *e, const cl_key *k);
/* nothing half-typed (a count, an operator, f's character) */
int vim_idle(const cl_edit *e);
/* INSERT -> NORMAL (Esc): the cursor steps back onto the last character */
void vim_escape(cl_edit *e);
/* vim mode on (INSERT, as Claude Code starts) or off */
void ed_set_vim(cl_edit *e, int on);

int ed_layout(const cl_edit *e, int width, long *a, long *z, int max, int *crow, int *ccol);

#endif
