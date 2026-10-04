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
} cl_io;

typedef struct cl_render {
    void *u;
    void (*text)(void *u, const char *s, long n);
    /* the answer (or this part of it) is complete */
    void (*end)(void *u);
} cl_render;

typedef struct cl_ui {
    cl_io *io;
    int col0;                   /* the cursor is at the start of a line */
    int status;                 /* a status line is showing */
    int frame;
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
/* a tool call, shown before it runs */
void ui_tool(cl_ui *u, const char *tool, const char *what);
/* the permission question: ASK_NO / ASK_ONCE / ASK_SESSION (tools.h) */
int ui_ask(cl_ui *u, const char *tool, const char *what, int outside);

#endif
