/* tasks_amiga -- exec's tasks into proc_core's table (ps, top). */
#ifndef TASKS_AMIGA_H
#define TASKS_AMIGA_H
#include "proc_core.h"
/* every task and process (the running one, the ready and the waiting lists) into t, at most max: their number */
int pa_snapshot(pc_task *t, int max);
#endif
