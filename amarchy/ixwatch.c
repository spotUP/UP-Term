/* The OS side of watching amarchy-index: see ixwatch.h. */
#include <exec/execbase.h>
#include <proto/exec.h>
#include <string.h>
#include "ixwatch.h"
#include "taskalive.h"

ixshared *ixw_find(void)
{
    ixshared *u;
    Forbid();
    u = (ixshared *)FindSemaphore((STRPTR)IX_PROG_NAME);
    Permit();
    return u;
}

int ixw_snapshot(ix_prog *out, int *alive)
{
    ixshared *u = ixw_find();
    char tname[32];
    struct Task *t;
    if (!u)
        return 0;
    Forbid();
    *out = u->p;
    t = u->task;
    strcpy(tname, u->tname);
    Permit();
    *alive = task_alive(t, tname);
    return 1;
}

int ixw_other_layout(void)
{
    int i, found = 0;
    Forbid();
    for (i = 0; !found && ix_prog_old_names[i]; i++)
        found = FindSemaphore((STRPTR)ix_prog_old_names[i]) != 0;
    Permit();
    return found;
}

int ixw_poll(ix_watcher *x)
{
    ix_prog p;
    int alive = 0, have = ixw_snapshot(&p, &alive);
    return ix_watcher_step(x, have, have ? &p : 0, alive);
}
