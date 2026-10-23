/* Running a leaf of the [menu] tree, shared by the text menu (amarchy-menu) and the drawn one
 * (amarchy-gui). Neither program's UI is known here: asking for a value and showing a refusal are
 * callbacks. lf_plan (leaf_core.c) is the host-testable decision; lf_run (leaf.c) is the Amiga part. */
#ifndef AMARCHY_LEAF_H
#define AMARCHY_LEAF_H

#include "../config/keymap.h"

/* the window a run: leaf gets (the README's NewShell line, as open:uterm) */
#define LF_UTERM_SPEC "XCON:0/20/640/300/UP-Term/CLOSE"

typedef enum {
    LF_WINDOW,   /* run: a program, in a new UP-Term window of its own (never the menu's console) */
    LF_REXX,     /* rexx: RX "script" */
    LF_OPEN,     /* open:uterm: a new UP-Term window */
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

/* returns the action's code (0 = done) */
int lf_run(const km_menu *m, const lf_ui *ui);

#endif
