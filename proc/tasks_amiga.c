/* tasks_amiga -- see tasks_amiga.h. Under Forbid the lists hold still; names are copied, nothing is kept. */
#include <string.h>
#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include "tasks_amiga.h"

extern struct ExecBase *SysBase;

static void copy_name(char *out, const char *s)
{
    int n = 0;
    if (s)
        while (s[n] && n < PC_NAME - 1) {
            out[n] = s[n];
            n++;
        }
    out[n] = 0;
}

static void one(pc_task *t, struct Task *k)
{
    memset(t, 0, sizeof(*t));
    t->id = (unsigned long)k;
    copy_name(t->name, k->tc_Node.ln_Name);
    t->pri = k->tc_Node.ln_Pri;
    t->state = k->tc_State == TS_RUN ? PC_RUN : k->tc_State == TS_READY ? PC_READY
               : k->tc_State == TS_WAIT ? PC_WAIT : PC_OTHER;
    if (k->tc_Node.ln_Type == NT_PROCESS) {
        struct Process *p = (struct Process *)k;
        struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(p->pr_CLI);
        t->process = 1;
        t->cli = p->pr_CLI ? (int)p->pr_TaskNum : 0;
        if (cli && cli->cli_Module && cli->cli_CommandName) {
            /* the command a CLI runs: a BCPL string */
            const UBYTE *b = (const UBYTE *)BADDR(cli->cli_CommandName);
            int n = b[0] < PC_NAME - 1 ? b[0] : PC_NAME - 1;
            memcpy(t->cmd, b + 1, (size_t)n);
            t->cmd[n] = 0;
        }
    }
}

int pa_snapshot(pc_task *t, int max)
{
    struct Node *n;
    int i = 0;
    Forbid();
    if (i < max)
        one(&t[i++], SysBase->ThisTask);
    for (n = SysBase->TaskReady.lh_Head; n->ln_Succ && i < max; n = n->ln_Succ)
        one(&t[i++], (struct Task *)n);
    for (n = SysBase->TaskWait.lh_Head; n->ln_Succ && i < max; n = n->ln_Succ)
        one(&t[i++], (struct Task *)n);
    Permit();
    return i;
}
