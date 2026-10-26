/* Running a leaf of the [menu] tree, shared by the text menu (amarchy-menu) and the drawn one
 * (amarchy-gui). Neither program's UI is known here: asking for a value and showing a refusal are
 * callbacks. lf_plan (leaf_core.c) is the host-testable decision; lf_run (leaf.c) is the Amiga part. */
#ifndef AMARCHY_LEAF_H
#define AMARCHY_LEAF_H

#include "../config/keymap.h"
#include "index_core.h"

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

/* The command line a rexx: row or bind runs (the commodity's act_rexx and the menus' LF_REXX share it). A
 * script, where the first word is a path (holds ':' or '/') or ends in .rexx or .rx: `RX <arg>`, the rest of
 * arg its arguments. Anything else is an ARexx string: `RX "<arg>"` with each '"' turned into '\''. RX runs a
 * quoted first argument as program text, so a quoted script path was never opened: Super+Shift+D and every
 * rexx:UP-Term:Rexx/dopus-*.rexx row did nothing on the A1200 (2026-10-26). 0 (out "") when arg is empty,
 * holds a control character, or the line does not fit in cap. */
int lf_rexx_command(char *out, int cap, const char *arg);

typedef enum {
    LF_WINDOW,   /* run: a program, in a new UP-Term window of its own (never the menu's console) */
    LF_REXX,     /* rexx: RX script or RX "string" (lf_rexx_command) */
    LF_OPEN,     /* open:uterm: a new UP-Term window */
    LF_DETACH,   /* detach: a program in the background, input and output NIL: (no window) */
    LF_SCAN,     /* scan: like detach:, and lf_run returns LF_WATCH so the menu watches the scan */
    LF_VIEWER,   /* viewer: the key viewer, the program super+k opens, in its own window */
    LF_WIN,      /* win: a window verb, sent to the commodity (K8 wires the port) */
    LF_GTHEME,   /* gtheme: the drawn menu's theme is saved (gthemes_dos.h gt_dos_select) */
    LF_REQUEST   /* tab:, menuid:, slash: ... a terminal window's request port takes the action */
} lf_how;

lf_how lf_plan(int kind);

/* The notice for a finished ARexx leaf: the dopus-*.rexx scripts end with RC 5 when DOPUS.1 is absent (plan
 * 2026-10-25-amarchy-dopus-integration.md, D5), which says "Directory Opus is not running". 0 for anything else. */
const char *lf_rexx_notice(const char *script, long rc);

/* The RX line a hotkey runs for a dopus-*.rexx script, asynchronously: it calls the script by its path and, only
 * when the script returned 5, opens the XCON window spec and writes the lf_rexx_notice message there (no newline: the window is one row high) for three
 * seconds. 1 with out set; 0 (out "") when script has no notice, or script or spec holds a quote or control
 * character, or the line does not fit in cap. */
int lf_rexx_notice_line(char *out, int cap, const char *script, const char *spec);

/* The viewer's command line: the quoted full path of amarchy-gui in dir (the directory the running menu was
 * loaded from, as the commodity's cx_helper_cmd does), then "viewer". A bare name goes through vsh's PATH,
 * which is stale or missing where UP-Term: is not assigned (the rig): the program is beside us, not on a path.
 * 0 when dir is empty, holds a quote or the line does not fit. */
int lf_viewer_command(char *out, int cap, const char *dir);
/* The ARexx lines that bring one Directory Opus lister forward: step 0 `dopus front`, 1 `lister set <h> visible off`,
 * 2 `... visible on`. 1 when a line was written, 0 past the last step or when cap is too small. */
int lf_lister_front_command(char *out, int cap, int step, unsigned long handle);

/* The clipboard history row a path names (index_core.h IX_KIND_CLIP): "clip:<n>" gives n >= 0, the empty
 * ring's row IX_CLIP_NONE gives -1, any other path (a tool, a game) gives -2. */
int lf_clip_entry(const char *path);

/* Fill the lister rows (index_core.h IX_LISTERS_NODE) from Directory Opus: `lister query all`, then `lister query
 * <handle> path` per lister, over ARexx port DOPUS.1. No port, no rows, no wait. Call after ix_load_file. */
void lf_load_listers(ix_index *x);

typedef struct lf_ui {
    /* a value for an action that needs one; 1 with out set, 0 on Escape or an empty line */
    int (*ask_value)(void *ctx, const char *label, const char *usage, char *out, int n);
    /* a refused action says why (answer may be empty) */
    void (*refused)(void *ctx, const char *label, long rc, const char *answer);
    void *ctx;
    /* 1 when the menu is itself an UP-Term window (the text menu), 0 when it is a plain Intuition
     * window (the graphical menu): which MRU entry is the terminal (upmru_target) */
    int self_in_mru;
} lf_ui;

/* 1 when the menu opened over an UP-Term window: upactive_terminal_present (handler/upactive.h) with
 * the menu's self_in_mru. Both menus and the commodity ask it once at open and pass the answer to
 * ix_level_t (TERM-ROWS-ONLY-OVER-TERMINAL). */

/* lf_run returns this after a scan: leaf started: the scanner runs and the menu should watch it. A positive
 * code that is no UPREQ_* answer (those are 5..35). */
#define LF_WATCH 1

/* returns the action's code (0 = done, LF_WATCH = a scan was started) */
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
