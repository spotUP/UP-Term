/* The OS side of watching amarchy-index: see ixwatch.h. */
#include <exec/execbase.h>
#include <proto/exec.h>
#include <string.h>
#include "ixwatch.h"

ixshared *ixw_find(void)
{
    ixshared *u;
    Forbid();
    u = (ixshared *)FindSemaphore((STRPTR)IX_PROG_NAME);
    Permit();
    return u;
}

/* Is t still a task? Walks the run, ready and wait lists under Forbid. A stale address equal to the
 * caller's own task counts as gone. */
static int task_exists(struct Task *t, const char *tname)
{
    struct Node *n;
    int ok = 0;
    if (!t || t == FindTask(0))
        return 0;
    Forbid();
    if ((struct Task *)SysBase->ThisTask == t)
        ok = 1;
    for (n = SysBase->TaskReady.lh_Head; !ok && n->ln_Succ; n = n->ln_Succ)
        ok = ((struct Task *)n == t);
    for (n = SysBase->TaskWait.lh_Head; !ok && n->ln_Succ; n = n->ln_Succ)
        ok = ((struct Task *)n == t);
    if (ok && t->tc_Node.ln_Name && tname[0] && strncmp((const char *)t->tc_Node.ln_Name, tname, 31))
        ok = 0;                 /* the address was reused by another task */
    Permit();
    return ok;
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
    *alive = task_exists(t, tname);
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
