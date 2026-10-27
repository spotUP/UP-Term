/* Is a Task pointer still a live task: see taskalive.h. */
#include <exec/execbase.h>
#include <proto/exec.h>
#include <string.h>
#include "taskalive.h"

/* Walks the run, ready and wait lists under Forbid. */
int task_alive(struct Task *t, const char *tname)
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
