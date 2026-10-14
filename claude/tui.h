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
 * the bell / OSC 9 / window title notifications. And (A4 input rest):
 * the @ list as the path is typed (directories cached per prompt; in bash
 * mode for a token with '/', Tab on a command from earlier ! commands),
 * Ctrl+S's stash, Ctrl+Enter / Ctrl+X Ctrl+S (send the queue now),
 * Ctrl+B (a foreground command to the background: tui_wait), ?'s
 * shortcuts panel, Alt+P (the model picker), vim's visual selection.
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

#define TUI_DIRS 4              /* directory listings kept for the @ list */
#define TUI_DIR_NAMES 400       /* names kept from one directory at most */

/* the look: Claude Code's (the default) or pi's (amiga-pi plan N0): no
 * frame, the editor between two rules with its cursor drawn as a reverse
 * cell, the working row in the top rule, the working directory and the
 * usage line under it, a list for the slash commands and for questions */
enum { TUI_LOOK_CLAUDE, TUI_LOOK_PI };

/* where the screen is drawn (amiga-pi TM1; pi's tuiMode): INLINE is C:Claude's (the footer pinned to the bottom of
 * the window, the transcript starting at the cursor's row), REGULAR is pi's "regular" (the main screen, not
 * cleared; the footer follows the transcript until the window is full), FULL is pi's "fullscreen" (the alternate
 * screen, cleared; the footer pinned; the transcript scrolls in a viewport by PageUp / PageDown) */
enum { TUI_SCREEN_INLINE, TUI_SCREEN_REGULAR, TUI_SCREEN_FULL };

/* what the box holds: a prompt, a ! command, a # memory */
enum { BOX_PROMPT, BOX_BASH, BOX_MEMORY };

/* one directory's names for the @ list: "name\0" each ("name/\0" a
 * directory), as listed */
typedef struct tui_dir {
    char path[256];             /* "" a free slot */
    jw names;
    int n;
    unsigned long epoch;        /* the prompt it was read for */
} tui_dir;

typedef struct cl_cmd {
    const char *name;           /* "/help" */
    const char *help;
} cl_cmd;

/* permission modes, cycled with Shift+Tab (tools.h PERM_*) */
extern const char *const tui_mode_names[4];

/* TUI_LOOK_PI: a selector over the program's own rows (the session tree): a rule, the title, the help, a search
 * line, a rule, the rows around the selection with "(sel/total)" and the program's status words under them, a
 * rule, the footer. The program's key function sees every key first (act: the keymap's KC_TREE action or
 * KA_NONE) and returns 1 when it took it; else up/down (wrapping), page keys, accept and cancel are the
 * picker's. row writes row i (SGR kept, the cursor mark included) into out. */
typedef struct tui_pick {
    const char *title;
    const char *help;           /* dim, wrapped, two spaces in */
    char query[96];             /* the search line's text */
    int qcur1;                  /* the search line's cursor: its byte offset + 1, 0 at the end of the text (the default). pi's Input.setValue
                                 * keeps the cursor where it was, at the start of a new one (input.ts:66), so a program that sets query sets 1 */
    int n, sel;
    int maxrows;                /* rows shown at most */
    char status[64];            /* after "(sel/total)" */
    /* TUI_PICK_SESSION: pi's SessionSelectorComponent instead of the tree's frame (session-selector.ts:741-755, 313-342):
     * a blank row, a rule, a blank row, title (the header row, composed by the program, SGR kept), help (the hint
     * rows, '\n' between, SGR kept), a blank row, "> " and the search text with a cursor cell, a blank row, the rows
     * (empty: the message when n is 0), "(sel/total)" only where the list scrolls, a blank row, a rule. The frame is
     * left on the screen when the picker ends: pi's -r keeps its selector in the scrollback. */
    void (*row)(void *u, int i, int selected, char *out, long cap);
    int (*key)(void *u, const cl_key *k, int act);
    void *u;
    /* TUI_PICK_AUTH: pi's OAuthSelectorComponent (the /login provider list): a rule, the title in bold and the accent colour,
     * the search input ("> " and the query, a cursor), the rows (one column in), "(sel/total)" only while the list scrolls,
     * a rule. status and help are not shown; `empty` is the row for no match (0: "No matching providers") */
    int layout;
    const char *empty;
    /* TUI_PICK_FRAME: the frame of pi's model, thinking and message selectors (model-selector.ts, thinking-selector.ts,
     * user-message-selector.ts): a rule, the rows of `top` (the program's text, SGR kept, '\n' between, an empty row is a
     * blank one), when `search` the search input ("> " and the query, a cursor) and a blank row, the list (row(); at most
     * maxrows around the selection, no scroll indicator: the program writes it into `tail`), the rows `tail` composes
     * (SGR kept, '\n' between; an empty text is one blank row; 0: none), a rule. A row of `top` that is \001 is a rule; a `top` that
     * begins with \002 has no opening rule (user-message-selector.ts has its own after the header). A list row may hold '\n' (a
     * message and its position line). The program's key function sees every key first. */
    const char *top;
    int search;
    void (*tail)(void *u, char *out, long cap);
    int done;                   /* the program's key function sets it: the picker ends with the selection as it is */
} tui_pick;
enum { TUI_PICK_TREE, TUI_PICK_AUTH, TUI_PICK_SESSION, TUI_PICK_FRAME };
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
    struct cl_perm *perm;       /* the permission mode (tools.h PERM_*), cycled here */
    int *think_off;             /* Alt+T: extended thinking off for the session (the REPL's), 0 none */
    /* slash commands */
    const cl_cmd *cmds;
    int ncmds;
    /* the menu's filter: the indexes into cmds (room for ncmds in idx) of the commands to list for the box (b, len bytes,
     * the cursor at byte cur), in the order the menu shows them; the count. *from is the offset in b where the text begins
     * that a chosen command replaces (up to the cursor). 0 (C:Claude): the commands whose name starts with the box's text
     * (it starts with '/', one line, no space), at most eight (pi's look: all of them). amiga-pi sets pi-tui's filter here,
     * which decides by the text before the cursor itself */
    int (*slash_filter)(void *u, const cl_cmd *cmds, int n, const char *b, long len, long cur, long *from, int *idx);
    void *slash_u;
    int *smatch;                /* the matches (tui.c), ncmds of room */
    int smatch_cap;
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
    /* A4 gaps 3: the window's focus as the terminal reports it (?1004):
     * 1 in, 0 out, -1 never reported; when the last key came */
    int focus;
    unsigned long key_ms;
    /* the open menu goes on without the user after m_afk_ms idle (-1
     * never; AskUserQuestion's askUserQuestionTimeout), the countdown shown
     * for its last m_afk_count_ms; tui_menu resets it */
    long m_afk_ms, m_afk_count_ms;
    int m_left;                 /* the countdown's seconds shown, -1 none */
    /* an extension's dialog in pi's look (tui_ext_menu, tui_ext_input; pi's ExtensionSelectorComponent and
     * ExtensionInputComponent): m_ext marks the menu (accent title and choice, j and k move, no wrap, digits and Tab
     * inert), pi_in_ext the line input; m_timeout_ms (-1 none) closes either by itself after that long, and the title
     * counts the seconds down in m_left (pi's CountdownTimer). The menu and the input reset all three. */
    int m_ext, pi_in_ext;
    long m_timeout_ms;
    int m_ctx;                  /* the menu's binding context (keys.h KC_CONFIRM, KC_MSGSEL, KC_THEME;
                                 * KC_SELECT the others); tui_menu resets it */
    /* A4 gaps 3: Esc or Ctrl+C on the idle, empty box: 1 when it was taken
     * (a pending /loop wakeup cancelled); iu is its argument */
    int (*esc_idle)(void *u);
    int quit_armed;             /* 'c' Ctrl+C, 'd' Ctrl+D pressed once */
    unsigned long quit_ms;
    int quit_after;             /* TUI_LOOK_PI: Ctrl+D stopped the run; the next tui_read ends the program */
    const cl_theme *th;         /* the colours (/theme) */
    const char *bar;            /* /color: the prompt bar's colour for the session (SGR), 0 the theme's */
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
    /* the @-completion list: open while the token at the cursor is an
     * "@path" (in bash mode: a token holding '/'), narrowed per key */
    char comp[TUI_COMP][128];
    int ncomp, csel, copen;
    long ctok;                  /* the token's start in the text */
    int cskip;                  /* 1: an '@' before the path (bash mode: 0) */
    int cclosed;                /* Esc closed the list until the text changes */
    int comp_dirs;              /* the token is /add-dir's or /cd's argument: directories only */
    /* A4 gaps 3: a prompt suggestion, greyed in the empty box; Tab or Right
     * puts it in, typing drops it ("" none) */
    char suggest[200];
    /* the directories the list read (input.c fills them): one disk read
     * each per prompt typed; a submitted prompt (epoch) makes them old */
    tui_dir dirs[TUI_DIRS];
    int dir_next;
    unsigned long epoch;
    long n_lists;               /* directory reads (tests: the cache) */
    /* Ctrl+S: the prompt put aside */
    char *stash;
    long stash_cur;
    int stash_box;
    /* Ctrl+B / Ctrl+Enter: a command runs in the foreground that may move
     * to the background (bgable), and the user asked for it (bg_req) */
    int bgable, bg_req;
    /* ?: the shortcuts panel under the box */
    int show_help;
    /* the line tui_read returned came from a key (Alt+P), not typed */
    int keycmd;
    /* a menu's option Shift+Tab chooses (-1 none); tui_menu resets it */
    int m_btab;
    /* a permission question: Tab on its first (Yes) or last (No) option
     * opens a comment field (m_noting: that option, -1 closed); the
     * comment chosen with it stays in m_note after tui_menu ("" none).
     * m_comment: the caller allows it for the next menu (reset after). */
    int m_comment, m_noting;
    char m_note[200];
    /* type-ahead */
    char *queue[TUI_QUEUE];
    int nq;
    /* Esc Esc */
    unsigned long esc_ms;
    int esc_armed;
    /* A4 gaps 3: the key bindings (keys.h): Claude Code's defaults and
     * keybindings.json's over them; the chords' state */
    cl_keymap km;
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
    char title[200];
    unsigned long busy_t0;
    int notify_after_s;         /* a turn this long ends with the bell */
    /* what was sent (tests: the redraw economy) */
    long n_frames, n_rows, n_lines, n_writes, n_full;
    long n_views, n_redraws, n_notify;
    /* TUI_LOOK_PI: its colours, and the footer's facts as the program formats them (pi's footer.js): the
     * directory, the usage before the context figure, the figure (ctx_role: 0, or a PR_* role it is
     * drawn in), the model on the right; the working row's label ("" idle); the editor rules' role (the
     * thinking level's); the messages waiting (steering, then follow-up) shown above the editor */
    int look;
    const cl_pitheme *pith;
    /* TUI_LOOK_PI -- settings.json's editorPaddingX (0..3: the editor's and the slash menu's side padding), autocompleteMaxVisible (3..20, 0: 5: the
     * slash menu's rows), doubleEscapeAction (0 tree, 1 fork, 2 none: Esc twice on an empty editor submits /tree or /fork) and showHardwareCursor
     * (the window's cursor rests on the editor's cursor cell) */
    int pi_edpad, pi_menu_max, pi_dbl_esc, pi_hwcursor;
    char pi_title[200];         /* the window title in pi's look: the program's (pi's updateTerminalTitle), never C:Claude's */
    char pi_cwd[300], pi_left[160], pi_ctx[48], pi_right[120], pi_work[96];
    int pi_ctx_role, pi_rule;
    /* TUI_LOOK_PI -- what extensions put on the screen (ctx.ui.setWidget, setStatus): widget lines above the editor and below it
     * (text lines separated by line feeds, 0 none; the *_cut flag adds pi's muted "... (widget truncated)"), and the footer's extra
     * status row (extension statuses, sorted and joined by the program) */
    char *pi_wabove, *pi_wbelow;
    int pi_wabove_cut, pi_wbelow_cut;
    char pi_extstat[240];
    char *pi_steer[TUI_QUEUE], *pi_follow[TUI_QUEUE];
    int n_steer, n_follow;
    /* TUI_LOOK_PI -- what the program does for the keys of pi's queue and cycling (pi_ui in amiga-pi):
     * on_queue: a message typed while the agent works (follow 0: steering, Enter; 1: follow-up, Alt+Enter):
     * 1 when the program took it, 0 and the type-ahead queue keeps it; on_dequeue: Alt+Up, and Esc while
     * working (quiet 1: no word about it): the queued messages out of the program's queues, malloc'ed text
     * joined by blank lines (0 none; *count how many); on_cycle: Ctrl+P / Shift+Ctrl+P (what 0, dir 1 / -1) and Shift+Tab (what 1) */
    int (*on_queue)(void *u, const char *text, int follow);
    char *(*on_dequeue)(void *u, int *count, int quiet);
    void (*on_cycle)(void *u, int what, int dir);
    /* app.tools.expand (Ctrl+O): the program toggles pi's tool output expansion (qu is its argument); C:Claude's Ctrl+O is the transcript */
    void (*on_expand)(void *u);
    void *qu;
    const tui_pick *pick;       /* the picker that is open (modal 2), the line input (modal 3: pi_in) */
    struct tui_dialog *dlg;     /* the sign-in dialog that is open (tui_dialog_open) */
    char pi_in[1100];           /* a pasted redirect URL of /login is the longest line */
    const char *pi_in_title;
    const char *pi_in_hint;     /* the first hint word of the line input ("enter save"), 0: "enter save" */
    int pi_nofoot;              /* a question before the screen is up: no footer under it */
    /* TUI_SCREEN_*: set by the program before tui_start. fb: the row above the footer's first (B when pinned);
     * view: rows the viewport is scrolled back from the transcript's end (FULL); view_dirty: the viewport is
     * drawn again at the next frame; scrollbar: pi's fullscreenScrollbar "always" (else hidden: "auto" shows it
     * on a pointer's hover only); exit_hint_only: pi's fullscreenExitOutput "resume-hint" (else "transcript");
     * exit_hint: the line the program prints on leaving ("" none) */
    int screen, fb, view_dirty, scrollbar, exit_hint_only;
    long view;
    char exit_hint[300];
} cl_tui;

/* the row chosen (accept), -1 cancelled or the end of input */
int tui_pick_run(cl_tui *t, tui_pick *p);
/* TUI_LOOK_PI: a one-line input under a title (Enter: 1 with the text in out, Esc: 0, the end of input: -1); pasted
 * text is taken (its line breaks left out) and a line longer than the screen shows its end */
int tui_line_input(cl_tui *t, const char *title, char *out, long cap);
/* the same with the hint's first word given ("enter submit"; 0: "enter save") */
int tui_line_input_hint(cl_tui *t, const char *title, const char *hint, char *out, long cap);
/* TUI_LOOK_PI: an extension's selector (ctx.ui.select, confirm) -- title may hold several lines, as pi's confirm puts the
 * message under the title: the option chosen (0..n-1), -1 for Esc, Ctrl+C and the end of input, TUI_AWAY when timeout_ms
 * (0 none) ran out; the keys of pi's ExtensionSelectorComponent (up, down, j, k, Enter) */
int tui_ext_menu(cl_tui *t, const char *title, const char *const *opt, int n, long timeout_ms);
/* the same with option sel highlighted at the start (the launch question: the last choice) */
int tui_ext_menu_sel(cl_tui *t, const char *title, const char *const *opt, int n, int sel, long timeout_ms);
/* TUI_LOOK_PI: an extension's text input (ctx.ui.input): 1 with the text in out, 0 for Esc, Ctrl+C and timeout_ms (0 none)
 * running out, -1 the end of input */
int tui_ext_input(cl_tui *t, const char *title, char *out, long cap, long timeout_ms);
/* TUI_LOOK_PI: pi's LoginDialogComponent (components/login-dialog.ts), which takes the editor's place while a sign-in runs: a
 * rule, the title in bold and the accent colour, the content, a rule, the footer. The content is rows the program adds in
 * order and shows as it goes: blank rows, text (wrapped one column in, in a theme role), one row of text that carries its
 * own SGR, and the input line ("> " and the typed text; once read it stays as the text typed). */
enum { TUI_DLG_BLANK, TUI_DLG_TEXT, TUI_DLG_ANSI, TUI_DLG_INPUT };
void tui_dialog_open(cl_tui *t, const char *title);
void tui_dialog_clear(cl_tui *t);
/* role: a PR_* of the theme (TUI_DLG_TEXT) */
void tui_dialog_add(cl_tui *t, int kind, int role, const char *text);
/* the last input line typed into: 1 with the text in out, 0 for Esc and Ctrl+C, -1 the end of input */
int tui_dialog_read(cl_tui *t, char *out, long cap);
/* keys typed meanwhile (the program is waiting for the provider): 1 when Esc or Ctrl+C was among them */
int tui_dialog_cancelled(cl_tui *t);
void tui_dialog_close(cl_tui *t);

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
/* A4 gaps 3: has anything been typed? (read now, kept for tui_read; a
 * focus report does not count) */
int tui_pending(cl_tui *t);
void tui_busy(cl_tui *t, int on);
/* A framed menu (the permission question, a picker): the option chosen
 * (0..n-1; Esc and Ctrl+C choose esc), -1 at the end of input, TUI_AWAY
 * when m_afk_ms (set before the call) ran out with no key. */
#define TUI_AWAY (-2)
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
/* A4 gaps 3: the transcript lines drawn since n_lines was mark taken back
 * (the Ctrl+L copy; the screen drawn again without them): 0, -1 when
 * they are no longer all kept */
int tui_takeback(cl_tui *t, long mark);
/* the transcript lines written since mark (tui_lines counted them: n_lines at that moment) replaced by s: pi's
 * tool box changes its background when the tool ends. Drawn in place when those lines are the end of what is on the
 * screen, else the screen is drawn again from the copy. 0, -1 when mark is no longer in the copy */
int tui_replace_tail(cl_tui *t, long mark, const char *s, long n);
/* FULL: scroll the viewport (KA_VIEW_*) */
void tui_scroll(cl_tui *t, int act);
/* the window's title (OSC 2), sent only when it changes */
void tui_title(cl_tui *t, const char *s);
/* bytes for the terminal itself, written now and not part of the screen (amiga-pi's OSC 7501 program status); nothing
 * before tui_start or after tui_stop */
void tui_raw(cl_tui *t, const char *s, long n);
/* the bell and a desktop notification (OSC 9) */
void tui_notify(cl_tui *t, const char *msg);
/* the next typed-ahead prompt (plain ones in a row joined by newlines; a
 * command or a ! line alone), 0 when none (malloc'ed: the caller frees) */
char *tui_dequeue(cl_tui *t, int plain_only);
/* the box's text replaced (the rewind menu puts a prompt back) */
void tui_set_text(cl_tui *t, const char *s);
/* the todo list for Ctrl+T, from a todo_write input */
void tui_set_todos(cl_tui *t, const char *json, long n);
/* While a command runs in the foreground (Bash, a ! line): keys read for
 * up to ms (type-ahead, Esc, Ctrl+B, Ctrl+Enter), the spinner turned.
 * TW_GO, TW_STOP (Esc, Ctrl+C, the end of input) or TW_BACKGROUND
 * (Ctrl+B, or Ctrl+Enter with messages to send): tools.h. */
int tui_wait(cl_tui *t, long ms);
/* the window's size read again: 1 when it changed (the screen will be
 * drawn whole) */
int tui_resized(cl_tui *t);
/* the @ list's cached listing of a directory (full: its path as read):
 * the slot, or 0 when it must be read (tui_dir_put then) */
tui_dir *tui_dir_get(cl_tui *t, const char *full);
tui_dir *tui_dir_put(cl_tui *t, const char *full);

/* the widgets above and below the editor (copied; 0 clears), and the footer's status row ('' none) */
void tui_pi_widgets(cl_tui *t, const char *above, int above_cut, const char *below, int below_cut);
void tui_pi_ext_status(cl_tui *t, const char *text);

#endif
