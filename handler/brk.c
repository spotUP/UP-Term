/* Break targets of a console: see brk.h. */
#include <exec/types.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include "brk.h"

int task_alive(struct Task *t)
{
    struct Node *n;
    int found = 0;
    if (!t)
        return 0;
    Forbid();
    if (t == SysBase->ThisTask)
        found = 1;
    for (n = SysBase->TaskReady.lh_Head; !found && n->ln_Succ; n = n->ln_Succ)
        if (n == &t->tc_Node)
            found = 1;
    for (n = SysBase->TaskWait.lh_Head; !found && n->ln_Succ; n = n->ln_Succ)
        if (n == &t->tc_Node)
            found = 1;
    Permit();
    return found;
}

void brk_open(brk *b, struct MsgPort *opener)
{
    b->port = b->home = opener;
    b->owner = opener ? (struct Task *)opener->mp_SigTask : 0;
}

void brk_change(brk *b, struct MsgPort *port)
{
    if (port) {
        b->port = port;
        b->owner = (struct Task *)port->mp_SigTask;
    }
}

struct Task *brk_task(brk *b)
{
    if (b->port && task_alive(b->owner))
        return b->owner;
    b->port = b->home;
    b->owner = 0;
    if (b->home && task_alive((struct Task *)b->home->mp_SigTask))
        return (struct Task *)b->home->mp_SigTask;
    return 0;
}

struct Task *brk_send(brk *b, ULONG sig)
{
    struct Task *t;
    Forbid();
    if ((t = brk_task(b)) != 0)
        Signal(t, sig);
    Permit();
    return t;
}
