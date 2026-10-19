/* splits_state: the split panes of a tab as the text of the file
 * ENV:up-term/splits, and back -- so a window that closes and opens again
 * can put its panes where they were. Pure C89 with no OS calls and no
 * allocation, so it is host-tested (tests/test_splits_state.c) and the
 * XCON: handler only has to read and write the file.
 *
 * The file (upconf's key = value style, one section):
 *
 *   [splits]
 *   layout = r32768(p,d40000(p,p))
 *   pane0-cwd = file://host/Work/src
 *   pane0-cmd = vim foo.c
 *   pane1-cwd = file://host/Work
 *
 * layout is lay_save's text (render/layout.h). paneK is the K-th pane in
 * the order lay_save wrote them (left to right, top to bottom), not a pane
 * id: the same order lay_steps hands back. cwd is the OSC 7 URL the pane
 * last reported and cmd the command it was running; either may be absent,
 * which means the pane opens in the host's directory, or with no command.
 *
 * Reading is forgiving: keys are case-insensitive, whitespace around key
 * and value is dropped, CR LF ends a line like LF, blank lines and lines
 * starting with ; or # are skipped, unknown keys and paneK keys with K at
 * or past the pane count are ignored. A value that does not fit its cap is
 * kept empty, never cut short: a truncated path or command would run
 * somewhere else, an empty one only runs nowhere special. A command with a
 * control character is kept empty too, on write and on read, so a file
 * can never smuggle a keystroke into a pane. (A cwd is held to the same
 * rule on write: a newline in it would split the line.)
 */
#ifndef SPLITS_STATE_H
#define SPLITS_STATE_H

#include "../render/layout.h"   /* LAY_MAX_PANES, lay_steps */

#define SPLITS_LAYOUT_MAX 64
#define SPLITS_CWD_MAX 256   /* an OSC 7 URL; a longer one is kept empty (pane opens in the host's dir) */
#define SPLITS_CMD_MAX 200   /* a command; a longer one, or one with a control char, is kept empty */

typedef struct splits_state {
    char layout[SPLITS_LAYOUT_MAX];          /* lay_save's text */
    int n;                                   /* panes */
    char cwd[LAY_MAX_PANES][SPLITS_CWD_MAX]; /* by written place (lay_save order) */
    char cmd[LAY_MAX_PANES][SPLITS_CMD_MAX];
} splits_state;

/* The file's text into buf. Returns its length, or -1 when it does not fit
 * size bytes with the terminating NUL (nothing is cut), or when s holds no
 * layout lay_steps accepts for s->n panes (2..LAY_MAX_PANES). Empty cwd and
 * cmd are left out. */
int splits_state_text(const splits_state *s, char *buf, int size);

/* Parse text of len bytes (it need not end in NUL). Returns the pane count
 * (2..LAY_MAX_PANES) and fills s, or 0 with s cleared when there is no
 * usable layout: none, one lay_steps refuses, or fewer than two panes. */
int splits_state_parse(splits_state *s, const char *buf, long len);

#endif
