/* Is a Task pointer still a live task? Shared by ixwatch.c (the scanner of amarchy-index), the menu
 * server's liveness check and mo_close_open (menu_open.c): a public port or record found by name whose
 * owner task is gone must not be trusted. */
#ifndef TASKALIVE_H
#define TASKALIVE_H

#include <exec/tasks.h>

/* 1 when t is on the run, ready or wait list and (when tname is not empty) still carries that name (the node name, or for a System()-started process the name of the program it runs), which
 * catches an address reused by another task. A stale address equal to the caller's own task counts as gone.
 * A suspended task still counts as alive. */
int task_alive(struct Task *t, const char *tname);

#endif
