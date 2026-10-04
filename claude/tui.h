/* tui -- C:Claude's screen as Claude Code draws it (ledger A3).
 *
 * The window is split by a scroll region (DECSTBM 1..B): the transcript
 * above, written only as whole lines at its bottom so the terminal itself
 * scrolls it (and keeps what leaves row 1 in its scrollback), and a footer
 * fixed below it -- the spinner line while a turn runs, the input box
 * (rounded frame, "> " prompt, the editor's text), and the status line or
 * the slash-command menu; or, while a question is open, a framed menu.
 *
 * The footer is built as one string per row each frame and only the rows
 * that differ from what is on the screen are sent: a spinner tick is one
 * row, a typed key one row, a transcript line no footer row at all. Every
 * update goes out in one write, wrapped in synchronized output (?2026).
 * The transcript's next row is tracked here (every line written is
 * measured), so the footer can grow and shrink without overwriting it.
 *
 * Input is the window in raw mode: keys.h decodes, edit.h edits.
 * Portable C89 over cl_io; host-tested against the engine
 * (tests/test_claude_tui.c). */
#ifndef CL_TUI_H
#define CL_TUI_H

#include "ui.h"
#include "keys.h"
#include "edit.h"
#include "json.h"

#define TUI_FOOT 24             /* footer rows at most */
#define TUI_BOX_ROWS 8          /* the input's rows shown at most */

typedef struct cl_cmd {
    const char *name;           /* "/help" */
    const char *help;
} cl_cmd;

/* permission modes, cycled with Shift+Tab (tools.h PERM_*) */
extern const char *const tui_mode_names[3];

typedef struct cl_tui {
    cl_io *io;
    int cols, rows;
    int B;                      /* the transcript: rows 1..B */
    int tr;                     /* the row its next line goes to, 1..B+1 */
    int started;
    /* the footer: as built, and as on the screen */
    char *want[TUI_FOOT];
    int wantw[TUI_FOOT], nwant;
    char *drawn[TUI_FOOT];
    int ndrawn;
    int full;                   /* everything must be drawn again */
    int crow, ccol;             /* the cursor after a frame; crow 0: hidden */
    int lrow, lcol;             /* where the last frame left it */
    jw o;                       /* one update's bytes */
    cl_keys keys;
    cl_edit ed;
    int scroll;                 /* the box's first shown row */
    /* the spinner */
    int busy;
    unsigned long t0, tick;
    int frame;
    long tokens;
    /* the status line's facts (the REPL's, read each frame) */
    const char *model, *effort, *root;
    int ctx_left;               /* percent, -1 unknown */
    int *mode;                  /* PERM_*, cycled here */
    /* slash commands */
    const cl_cmd *cmds;
    int ncmds;
    int msel;                   /* the slash menu's selection */
    int mclosed;                /* Esc closed it for this text */
    /* a modal menu */
    int modal;
    const char *m_title, *m_q;
    const char *const *m_opt;
    int m_n, m_sel;
    char hint[96];              /* replaces the status line's left side once */
    int quit_armed;
    int expand;                 /* Ctrl+O: results in full */
    void (*on_expand)(void *u); /* Ctrl+O pressed */
    void *eu;
    /* what was sent (tests: the redraw economy) */
    long n_frames, n_rows, n_lines, n_writes, n_full;
} cl_tui;

/* 0, or -1 when out of memory. cols x rows from io->size (80x24 when it
 * says nothing). */
int tui_init(cl_tui *t, cl_io *io);
void tui_free(cl_tui *t);
/* Raw mode on, the modes the client needs (bracketed paste, kitty's
 * disambiguation, modifyOtherKeys), the start row from a DSR answer, the
 * footer drawn: 0, or -1 when the console refused raw mode. */
int tui_start(cl_tui *t);
/* the footer cleared, the scroll region and the modes reset, raw off */
void tui_stop(cl_tui *t);

/* Transcript lines: s holds whole lines, each ending in "\n" (SGR inside
 * is fine; each line should end with its attributes reset). */
void tui_lines(cl_tui *t, const char *s, long n);
/* the transcript and the screen cleared (/clear) */
void tui_clear(cl_tui *t);
/* the footer brought up to date (only the rows that changed) */
void tui_frame(cl_tui *t);

/* A line typed and submitted: its length (in buf, terminated), -1 when
 * the user quit (Ctrl+C twice, Ctrl+D on an empty line, end of input). */
long tui_read(cl_tui *t, char *buf, long cap);
/* While a turn runs: keys read without waiting (type-ahead goes into the
 * box), the spinner turned. 1 when Esc or Ctrl+C asked to stop. */
int tui_poll(cl_tui *t);
/* the spinner turned and the window's size checked, no keys read */
void tui_tick(cl_tui *t);
void tui_busy(cl_tui *t, int on);
/* A framed menu (the permission question, a picker): the option chosen
 * (0..n-1; Esc and Ctrl+C choose esc), -1 at the end of input. */
int tui_menu(cl_tui *t, const char *title, const char *question, const char *const *opt, int n, int sel,
             int esc);

/* the display width of s[0..n), escape sequences not counted */
int tui_width(const char *s, long n);

#endif
