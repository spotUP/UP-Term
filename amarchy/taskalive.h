/* Is a Task pointer still a live task? Shared by ixwatch.c (the scanner of amarchy-index), the menu
 * server's liveness check and mo_close_open (menu_open.c): a public port or record found by name whose
 * owner task is gone must not be trusted. */
#ifndef TASKALIVE_H
#define TASKALIVE_H

#include <exec/tasks.h>
#include <dos/dos.h>

/* 1 when t is on the run, ready or wait list and (when tname is not empty) still carries that name (the node name, or for a System()-started process the name of the program it runs), which
 * catches an address reused by another task. A stale address equal to the caller's own task counts as gone.
 * A suspended task still counts as alive. */
int task_alive(struct Task *t, const char *tname);

/* The process whose standard output (pr_COS, or its CLI's cli_StandardOutput) is the file handle out, 0 when none is
 * on the run, ready or wait list. A System()-started process carries the name "Background CLI", so the output handle
 * the caller gave it is the one identity that is unique to that job. */
struct Task *task_find_by_output(BPTR out);

/* A file's DateStamp as one number (ticks); 0 when the file is not there. The change test of "is the file on
 * disk the one I loaded": the upkg status file (pkgs_dos.c) and the warm menu server's own program. */
long ta_file_stamp(const char *path);

#endif
