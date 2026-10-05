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

#include "sys.h"
#include "json.h"

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
    /* A4: a file in the user's editor (Ctrl+G; raw mode is off
     * meanwhile): 0 when the editor ran, -1 not; 0: no editor */
    int (*edit)(void *u, const char *path);
} cl_io;

/* A4 (WP1) -- what the screen needs from the rest of the program. Each
 * has a stub here; WP3 (config.c, memory.c, checkpoint.c) plugs in the
 * real ones by setting the cl_ui fields before repl_screen():
 *
 *   settings   u->setting / u->set_setting / u->su: "theme" (theme.h
 *              names), "editorMode" ("vim" or "normal"). Stub: none read,
 *              /theme and /vim last for the session only.
 *   memory     u->memory_files / u->memory_changed / u->mu: the files a
 *              "# note" may go to, and the news that one changed (reload
 *              the system prompt). Stub: <root>/CLAUDE.md (project) and
 *              ENVARC:Claude/CLAUDE.md (user); no reload.
 *   rewind     u->rw: the points Esc Esc's menu offers. Stub (input.c):
 *              the user prompts of the conversation, conversation-only
 *              restore (conv_rollback); checkpoint.c adds RW_CODE. */
typedef struct cl_memfile {
    char label[64];             /* "Project memory" */
    char path[256];
} cl_memfile;

#define RW_CONV 1               /* the conversation can go back to that point */
#define RW_CODE 2               /* the files can */
#define RW_SUM 4                /* restore's "what": summarise from that point on (A4 gaps) */
#define RW_SUM_UP 8             /* ... or everything before it */

typedef struct cl_rewind {
    void *u;
    /* the points, oldest first (each one is a user prompt) */
    int (*count)(void *u);
    /* point i's prompt text: 0, -1 */
    int (*label)(void *u, int i, char *out, long cap);
    /* RW_* point i can restore */
    int (*can)(void *u, int i);
    /* back to before point i (what: RW_CONV and/or RW_CODE): 0, -1 */
    int (*restore)(void *u, int i, int what);
} cl_rewind;

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
    /* A4 (see above); ui_attach fills what is still unset */
    char histfile[256];         /* the history file, "" none (main sets ENVARC:Claude/history) */
    cl_sys *sys;
    const char *root;
    const char *(*setting)(void *u, const char *key);
    void (*set_setting)(void *u, const char *key, const char *value);
    void *su;
    int (*memory_files)(void *u, cl_memfile *out, int max);
    void (*memory_changed)(void *u, const char *path);
    void *mu;
    cl_rewind rw;
    struct cl_conv *conv;       /* the rewind stub's */
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
/* A tool's question with options (tools.h's choose, CH_* flags): in the
 * screen a framed menu, in the line mode the options numbered and an
 * answer typed. The option, n for an answer of the user's own (in other),
 * -1 declined; CH_MULTI: *picked a bit per option, 0. */
int ui_choose(cl_ui *u, const char *header, const char *question, const char *const *labels,
              const char *const *descs, int n, int flags, unsigned *picked, char *other, long cap);
/* A server tool (web_search) shown: its call (call 1: the block from its
 * start and the input that streamed in) or its result block. */
void ui_server(cl_ui *u, int call, const char *block, long bn, const char *input, long inn);

/* ---- A4 (WP1) ---- */
/* The program's parts the screen's features use: the history loaded into
 * the box, @-completion, the rewind menu, the theme and vim mode from the
 * settings. repl_screen calls it once the screen started. */
void ui_attach(cl_ui *u, cl_sys *sys, const char *root, struct cl_conv *conv);
/* Thinking text as it streams (the transcript viewer shows it) */
void ui_thinking(cl_ui *u, const char *s, long n);
/* After a tool round: the plain prompts typed ahead meanwhile, joined by
 * newlines into out and echoed; 1 when there were any (they go into the
 * same turn, as Claude Code's queue does) */
int ui_take_queued(cl_ui *u, jw *out);
/* What a line typed at the prompt is (input.c): */
enum { IN_PASS, IN_SEND, IN_DONE };
/* IN_PASS -- not one of these, the REPL goes on with it as it is;
 * IN_SEND -- send out as the prompt instead (! ran a command: its output
 *            for Claude; @path mentions: the files' text attached);
 * IN_DONE -- handled here (# memory, /theme, /vim). */
int ui_input(cl_ui *u, const char *line, jw *out);
/* Esc Esc on an empty box: the rewind menu */
void ui_rewind(cl_ui *u);
/* input.c: Tab after @ -- the paths starting with tok, from the start
 * directory (u: the cl_ui); the rewind menu's stub source */
int input_complete(void *u, const char *tok, char out[][128], int max);
void input_rewind_stub(cl_ui *u);

#endif
