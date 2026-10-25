/* Running a leaf of the [menu] tree, shared by the text menu (amarchy-menu) and the drawn one
 * (amarchy-gui). Neither program's UI is known here: asking for a value and showing a refusal are
 * callbacks. lf_plan (leaf_core.c) is the host-testable decision; lf_run (leaf.c) is the Amiga part. */
#ifndef AMARCHY_LEAF_H
#define AMARCHY_LEAF_H

#include "../config/keymap.h"

/* the window a run: leaf gets (the README's NewShell line, as open:uterm) */
#define LF_UTERM_SPEC "XCON:0/20/640/300/UP-Term/CLOSE"

/* spec with "/SCREEN name" appended so the window opens on that public screen (AMB3: the screen that was
 * in front when the hotkey fired, a DOpus screen for one). screen NULL or "" leaves the spec as it is
 * (the default public screen). 0 when it does not fit in cap, or the name has a '/' (it would end the
 * option); out is then "". */
int lf_screen_spec(char *out, int cap, const char *spec, const char *screen);

/* The command line a run: leaf starts. With a vsh, `"vsh" -c "cmd"`: the command runs the way it would typed
 * in an UP-Term tab, with vshrc's $PATH (UP-Term:nvim/bin and the rest are not on the AmigaDOS path, so
 * SystemTags on "nvim" returned -1: AMB, Apps > Editor did nothing). Without one, cmd as it is. 0 (out "")
 * when it does not fit in cap, or cmd holds a '"' or a control character. */
int lf_shell_command(char *out, int cap, const char *vsh, const char *cmd);

typedef enum {
    LF_WINDOW,   /* run: a program, in a new UP-Term window of its own (never the menu's console) */
    LF_REXX,     /* rexx: RX "script" */
    LF_OPEN,     /* open:uterm: a new UP-Term window */
    LF_DETACH,   /* detach: a program in the background, input and output NIL: (no window) */
    LF_REQUEST   /* tab:, menuid:, slash: ... a terminal window's request port takes the action */
} lf_how;

lf_how lf_plan(int kind);

typedef struct lf_ui {
    /* a value for an action that needs one; 1 with out set, 0 on Escape or an empty line */
    int (*ask_value)(void *ctx, const char *label, const char *usage, char *out, int n);
    /* a refused action says why (answer may be empty) */
    void (*refused)(void *ctx, const char *label, long rc, const char *answer);
    void *ctx;
} lf_ui;

/* 1 when the menu opened over an UP-Term window: the window the request port reaches (the one focused
 * before the menu, handler/upactive.h) is alive. Both menus ask it once at open and pass the answer to
 * ix_level_t (TERM-ROWS-ONLY-OVER-TERMINAL). */
int lf_terminal_present(void);

/* returns the action's code (0 = done) */
int lf_run(const km_menu *m, const lf_ui *ui);

/* The request an index row's launcher sends the running Amarchy (the Amiga side wraps it in an exec
 * Message; wblaunch.c). Amarchy starts the WBStartup launch with its own reply port, so the message
 * outlives the launcher, and answers rc (0 or -1) and err (the reason). */
#define LF_LAUNCH_PORT "AMARCHY.launch"
#define LF_LAUNCH_PATH 256
#define LF_LAUNCH_ERR  96
typedef struct lf_launch {
    long rc;
    char path[LF_LAUNCH_PATH];
    char err[LF_LAUNCH_ERR];
} lf_launch;

/* Fill a request for path (the tool or project, no .info). 1, or 0 when path is empty or too long. */
int lf_launch_fill(lf_launch *b, const char *path);

/* Launch an index row (an app or a game found by amarchy-index) the way a Workbench double-click does
 * (wblaunch.h), never through a shell. Failure goes through ui->refused with the reason as answer, as a
 * run: row's does. 0, or UPREQ_FAILED. */
int lf_open_index(const char *label, const char *path, const lf_ui *ui);

#endif
