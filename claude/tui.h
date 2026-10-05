/* tui -- C:Claude's screen as Claude Code draws it (ledger A3).
 *
 * The window is split by a scroll region (DECSTBM 1..B): the transcript
 * above, written only as whole lines at its bottom so the terminal itself
 * scrolls it (and keeps what leaves row 1 in its scrollback), and a footer
 * fixed below it -- the spinner line while a turn runs, the input box
 * (rounded frame, "> " prompt, the editor's text), the statusLine
 * command's own row(s) when there is one, and the status line or the
 * slash-command menu; or, while a question is open, a framed menu.
 *
 * The footer is built as one string per row each frame and only the rows
 * that differ from what is on the screen are sent: a spinner tick is one
 * row, a typed key one row, a transcript line no footer row at all. Every
 * update goes out in one write, wrapped in synchronized output (?2026).
 * The transcript's next row is tracked here (every line written is
 * measured), so the footer can grow and shrink without overwriting it.
 *
 * Input is the window in raw mode: keys.h decodes, edit.h edits.
 *
 * A4 (WP1): the history kept across sessions (hist.h) with Ctrl+R's
 * search, the box's bash (!) and memory (#) modes, Tab completion of
 * @-paths, the type-ahead queue while a turn runs, Esc Esc (clear, or the
 * rewind menu), Ctrl+O's transcript viewer (tview.c), Ctrl+T's todo list,
 * Ctrl+L, Ctrl+G (the external editor), vim mode, themes (theme.h), and
 * the bell / OSC 9 / window title notifications.
 * Portable C89 over cl_io; host-tested against the engine
 * (tests/test_claude_tui.c). */
#ifndef CL_TUI_H
#define CL_TUI_H

#include "ui.h"
#include "keys.h"
#include "edit.h"
#include "json.h"
#include "hist.h"
#include "theme.h"
#include "sys.h"

#define TUI_FOOT 32             /* footer rows at most */
#define TUI_BOX_ROWS 8          /* the input's rows shown at most */
#define TUI_QUEUE 8             /* prompts typed ahead while a turn runs */
#define TUI_COMP 32             /* @-path completions offered at most */
#define TUI_RING 128            /* transcript lines kept for Ctrl+L */
#define TUI_LOG_MAX 196608L     /* the transcript viewer's text at most */
#define TUI_TODOS 5             /* Ctrl+T: the todo list's rows */

/* what the box holds: a prompt, a ! command, a # memory */
enum { BOX_PROMPT, BOX_BASH, BOX_MEMORY };

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
    /* the statusLine command's output (the REPL's; "" or 0 none): each line
     * its own row above the status line, SGR colours kept, as Claude Code
     * draws it; a change redraws only its row */
    const char *status;
    int status_pad;             /* statusLine.padding */
    int hide_vim;               /* statusLine.hideVimModeIndicator: no "-- INSERT --" (A4 gaps) */
    /* called while the screen waits for keys (the status line's schedule) */
    void (*idle)(void *u);
    void *iu;
    /* A4 gaps 2: while the screen waits, a turn to run now (a cron job, a
     * background task's news): malloc'ed, 0 none; iu is its argument */
    char *(*wake)(void *u);
    int quit_armed;             /* 'c' Ctrl+C, 'd' Ctrl+D pressed once */
    unsigned long quit_ms;
    const cl_theme *th;         /* the colours (/theme) */
    /* A4: what the screen needs from the program (ui_attach sets them) */
    cl_sys *sys;
    const char *project;        /* the start directory: the history's project */
    const char *histfile;       /* "" or 0: the history is not kept on disk */
    const char *edit_path;      /* Ctrl+G's file (T:claude-prompt.txt) */
    cl_hist hist;
    /* @-completion: tok the "@path" token's text, out the replacements
     * (with a trailing '/' for a directory): their count */
    int (*complete)(void *u, const char *tok, char out[][128], int max);
    void *cu;
    /* Esc Esc on an empty box: the rewind menu */
    void (*on_rewind)(void *u);
    void *ru;
    int box;                    /* BOX_* */
    /* Ctrl+R */
    int search, sfail;
    char sq[96];
    int sidx;
    char *saved;                /* the text before the search */
    /* the @-completion list */
    char comp[TUI_COMP][128];
    int ncomp, csel, copen;
    long ctok;                  /* the token's start in the text */
    /* type-ahead */
    char *queue[TUI_QUEUE];
    int nq;
    /* Esc Esc, Ctrl+X chords */
    unsigned long esc_ms;
    int esc_armed, ctrlx;
    /* Ctrl+T: the todo list ("<status char><text>\n" per item: c done,
     * p in progress, o pending) */
    jw todos;
    int show_todos;
    /* the transcript: the last lines as drawn (Ctrl+L), all of it in full
     * (Ctrl+O) */
    char *ring[TUI_RING];
    int nring, ringat;
    jw log;
    int nolog;
    /* notifications: the window title as set, when the turn began */
    char title[64];
    unsigned long busy_t0;
    int notify_after_s;         /* a turn this long ends with the bell */
    /* what was sent (tests: the redraw economy) */
    long n_frames, n_rows, n_lines, n_writes, n_full;
    long n_views, n_redraws, n_notify;
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
/* the next key: 1, 0 none within wait ms, -1 the end of input */
int tui_key(cl_tui *t, cl_key *k, long wait);
void tui_busy(cl_tui *t, int on);
/* A framed menu (the permission question, a picker): the option chosen
 * (0..n-1; Esc and Ctrl+C choose esc), -1 at the end of input. */
int tui_menu(cl_tui *t, const char *title, const char *question, const char *const *opt, int n, int sel,
             int esc);

/* the display width of s[0..n), escape sequences not counted */
int tui_width(const char *s, long n);

/* A4 */
/* text for the transcript viewer only (results in full, thinking) */
void tui_log(cl_tui *t, const char *s, long n);
/* Ctrl+O: the transcript viewer (tview.c) until q / Esc / Ctrl+C / Ctrl+O */
void tui_transcript(cl_tui *t);
/* Ctrl+L: the screen cleared and the transcript's last lines drawn again */
void tui_redraw(cl_tui *t);
/* the window's title (OSC 2), sent only when it changes */
void tui_title(cl_tui *t, const char *s);
/* the bell and a desktop notification (OSC 9) */
void tui_notify(cl_tui *t, const char *msg);
/* the next typed-ahead prompt (plain ones in a row joined by newlines; a
 * command or a ! line alone), 0 when none (malloc'ed: the caller frees) */
char *tui_dequeue(cl_tui *t, int plain_only);
/* the box's text replaced (the rewind menu puts a prompt back) */
void tui_set_text(cl_tui *t, const char *s);
/* the todo list for Ctrl+T, from a todo_write input */
void tui_set_todos(cl_tui *t, const char *json, long n);

#endif
