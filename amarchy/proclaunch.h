/* proclaunch: start a program detached (input and output NIL:, no window) on the stack its own "$STACK: n" cookie
 * asks for (RULES.md rule 6), with a copy of this process's command path. The one code path for the warm menu
 * servers (act.c srv_start_proc) and the Packages rows (leaf.c, plan 2026-10-27-amarchy-packages-submenu.md).
 * DOS calls only: linked by the commodity (libnix) and by amarchy-menu and amarchy-gui (ixemul). */
#ifndef PROCLAUNCH_H
#define PROCLAUNCH_H

#include <dos/dos.h>

/* Clone the path list of this process's CLI for a child, which owns and frees it. 0 when there is no CLI. */
BPTR pl_dup_path(void);
/* Free a path list pl_dup_path made that no child took. */
void pl_free_path(BPTR p);
/* Run line asynchronously. The stack is the cookie in prog's file when that is larger than floor, else floor;
 * prog 0 or a file that does not load gives floor. name (may be 0) is the process name. 1 started, 0 not. */
int pl_start(const char *line, const char *prog, ULONG floor, const char *name);
/* The stack pl_start and pl_run give prog: its cookie when larger than floor, else floor. */
ULONG pl_stack(const char *prog, ULONG floor);
/* Run line and wait for it, input NIL:, output to out (the caller's, still open after). The return code, -1 when it
 * did not start. */
LONG pl_run(const char *line, const char *prog, ULONG floor, BPTR out);

#endif
