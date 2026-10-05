/* edit -- C:Claude's input line (ledger A3): a multi-line UTF-8 buffer
 * with Emacs keys as Claude Code has them (Ctrl+A/E/K/U/W/Y, Alt+B/F/D,
 * Alt+Backspace, Ctrl+_ undo, Alt+Y's ring of the last kills after
 * Ctrl+Y), the history on Up/Down when the cursor is on the first or last
 * line, a paste inserted whole, and the layout of the text in a box of a
 * given width (wrapped by character, as Claude Code's box does). Vim mode
 * (A4 1.8, /vim): INSERT, NORMAL and VISUAL, '.', vim.c.
 * Portable C89, host-tested (tests/test_claude_tui.c). */
#ifndef CL_EDIT_H
#define CL_EDIT_H

#include "keys.h"

#define ED_HIST 100
#define ED_UNDO 24

/* vim mode: off, or the mode the editor is in (VISUAL: character-wise,
 * VLINE: line-wise visual selection, from vanchor to cur) */
enum { VIM_OFF, VIM_INSERT, VIM_NORMAL, VIM_VISUAL, VIM_VLINE };

#define ED_RING 8               /* Alt+Y: the kills kept */

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
    char *ring[ED_RING];/* Alt+Y: the kills, oldest first */
    int nring;
    long ya, yz;        /* the text the last Ctrl+Y / Alt+Y put in */
    int yidx;           /* which kill it was (0 the newest), -1 the register */
    int yanked;         /* the last key was Ctrl+Y or Alt+Y */
    ed_snapshot undo[ED_UNDO];
    int nundo;
    int lastk;          /* the last key's kind: typed characters undo as one */
    /* vim (vim.c) */
    int vim;            /* VIM_* */
    int vcount, vopcount;   /* the count typed so far, the operator's */
    int vop;            /* a pending operator: 'd' 'c' 'y' '>' '<', 0 none */
    int vpend;          /* a pending key: 'f' 'F' 't' 'T' 'r' 'g' 'i' 'a', 0 none */
    int vlastf, vlastc; /* the last f/F/t/T and its character (; and ,) */
    long vanchor;       /* visual mode: the selection's other end */
    long nsnap;         /* undo snapshots taken (a change happened) */
    /* '.': the keys of the change being typed, and of the last one */
    char *vrec, *vdot;
    long nvrec, cvrec, nvdot, vreclast;
    int vrecon;         /* recording a change */
    int vreplay;        /* '.' is replaying one */
    long vsnap0;        /* nsnap when the recording started */
    long vcmdcount;     /* the count the last command had, 0 none */
    long vdotcount;     /* ... the last change's */
    /* vimInsertModeRemaps: two-key INSERT sequences to Esc ("jj"), the
     * first key typed (vrp) and when; now_ms the key's time (the screen
     * sets it before each key) */
    char vremap[17];
    int vrp;
    unsigned long vrp_ms, now_ms;
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
/* INSERT: a typed character that ends a remapped sequence ("jj" within a
 * second): the first one taken out, NORMAL mode, 1; else 0 (typed) */
int vim_remap(cl_edit *e, const cl_key *k);
/* vim mode on (INSERT, as Claude Code starts) or off */
void ed_set_vim(cl_edit *e, int on);
/* visual mode: the selection's keys (VISUAL, VLINE); the rest common keys */
int vim_visual(cl_edit *e, const cl_key *k);
/* '.': each key the editor gets is offered to the recorder first, and
 * what a key did is looked at after it */
void vim_record(cl_edit *e, const cl_key *k);
void vim_after(cl_edit *e);
/* the selection as bytes [*a, *z): 1 in visual mode, else 0 */
int vim_selection(const cl_edit *e, long *a, long *z);
/* the mode's word for the status line ("-- INSERT --"), 0 none */
const char *vim_label(const cl_edit *e);
/* Alt+Y's ring: a kill kept (ed_cut with keep does it) */
void ed_ring_add(cl_edit *e, const char *s, long n);

int ed_layout(const cl_edit *e, int width, long *a, long *z, int max, int *crow, int *ccol);

#endif
