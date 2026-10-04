/* ui -- the Claude client's screen: the console it runs in (cl_io), the
 * renderer the answer's text goes through (cl_render), a status line with
 * a spinner while Claude is silent, tool calls and the permission
 * question. Claude is an ordinary console program: input is the window's
 * own line editing and history (cooked mode), output is text with SGR.
 *
 * The renderer is a hook. ui_plain() is the plain-text one: the text as it
 * arrives, control characters other than newline and tab dropped (the
 * model must not be able to send escape sequences to the terminal). The
 * Markdown renderer of feature/highlight-markdown (view/md.h, md_render,
 * which takes a whole document) plugs in as another cl_render when it
 * lands: it will collect the text and render each finished block (up to a
 * blank line outside a code fence) as it completes.
 * Portable C89, host-tested through the REPL suite. */
#ifndef CL_UI_H
#define CL_UI_H

typedef struct cl_io {
    void *u;
    /* one line of input without its newline: its length, -1 at the end */
    long (*read_line)(void *u, char *buf, long cap);
    void (*write)(void *u, const char *s, long n);
    /* was Ctrl+C pressed since the last call? (clears it) */
    int (*brk)(void *u);
    /* waits; 1 when Ctrl+C cut it short */
    int (*sleep)(void *u, long ms);
    /* a clock in milliseconds */
    unsigned long (*ms)(void *u);
    /* the debug log (0: none). Never given the API key. */
    void (*log)(void *u, const char *s, long n);
    /* The screen of its own (ledger A3), 0 for the line mode above:
     * bytes typed in raw mode -- their count, 0 when none came within
     * timeout_ms (0: do not wait), -1 at the end of input; */
    long (*read)(void *u, char *buf, long cap, long timeout_ms);
    /* the window's size: 0 with *cols, *rows set, -1 unknown; */
    int (*size)(void *u, int *cols, int *rows);
    /* raw mode on (1) or back as it was (0): 0, -1 refused */
    int (*raw)(void *u, int on);
} cl_io;

typedef struct cl_render {
    void *u;
    void (*text)(void *u, const char *s, long n);
    /* the answer (or this part of it) is complete */
    void (*end)(void *u);
} cl_render;

struct cl_tui;
struct cl_show;

typedef struct cl_ui {
    cl_io *io;
    int col0;                   /* the cursor is at the start of a line */
    int status;                 /* a status line is showing */
    int frame;
    /* the screen of its own (ledger A3): when set, everything below goes
     * there instead of the line mode's text */
    struct cl_tui *tui;
    struct cl_show *show;
} cl_ui;

void ui_init(cl_ui *u, cl_io *io);
/* text of the program's own, as it is */
void ui_puts(cl_ui *u, const char *s);
/* a message on a line of its own */
void ui_line(cl_ui *u, const char *s);
/* "what" with a turning spinner on the current line (call again to turn) */
void ui_status(cl_ui *u, const char *what);
void ui_status_clear(cl_ui *u);
/* the plain-text renderer */
void ui_plain(cl_ui *u, cl_render *r);
/* a tool call, shown before it runs (tool: T_*, in: its input JSON) */
void ui_tool(cl_ui *u, int tool, const char *name, const char *what, const char *in, long inn);
/* the permission question: ASK_NO / ASK_ONCE / ASK_SESSION / ASK_STOP (tools.h) */
int ui_ask(cl_ui *u, int tool, const char *name, const char *what, int outside);
/* a write's or an edit's change before it is asked for, and a result */
void ui_preview(cl_ui *u, int tool, const char *path, const char *before, long bn, const char *after, long an);
void ui_result(cl_ui *u, int tool, const char *in, long inn, int is_error, const char *text, long n);
/* a turn runs (the spinner) / is over */
void ui_busy(cl_ui *u, int on);
/* tokens of the answer so far (the spinner shows them) */
void ui_tokens(cl_ui *u, long n);
/* during a turn: 1 when the user asked to stop (Ctrl+C; in the screen
 * also Esc); the screen also reads type-ahead and turns the spinner */
int ui_poll(cl_ui *u);
/* the user's line, echoed into the transcript (the screen only) */
void ui_user(cl_ui *u, const char *line);
/* a choice from a list (/model, /effort): its index, -1 none or no screen */
int ui_pick(cl_ui *u, const char *title, const char *const *opt, int n, int sel);

#endif
