/* A new UP-Term window of its own, as Patina opens it (open:uterm, and a
 * menuid: row for a program with no terminal window open). The window runs
 * what a handler tab of the same kind runs (handler/tabprog.h, the one
 * table): vsh for a shell, the program's FROM script for a program.
 *
 * s77, 2026-11-13: open:uterm ran `NewShell "XCON:..."` with no FROM, so
 * the window was the AmigaDOS Shell, not vsh (no banner, no logo); and the
 * Claude row with no terminal open first opened that shell, then asked it
 * for a Claude tab, so two tabs opened. */
#ifndef UTERM_DOS_H
#define UTERM_DOS_H

/* The NewShell line of a window on spec running prog (TAB_SHELL or a
 * TAB_* program) with the agent's first message task (0 none). The FROM
 * script is written here. 1 ready, 0 failed, -1 the program is not
 * installed (uterm_missing says so). */
int uterm_line(char *line, int cap, const char *spec, int prog, const char *task);

/* The requester text for a program that is not installed. */
const char *uterm_missing(int prog);

#endif
