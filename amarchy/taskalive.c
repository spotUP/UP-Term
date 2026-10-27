/* Is a Task pointer still a live task: see taskalive.h. */
#include <exec/execbase.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <string.h>
#include "taskalive.h"

/* A process System() started carries the node name "Background CLI" whatever NP_Name asked for, so the name
 * of the program it runs (the CLI command, the last path part) is its identity: 1 when that is tname. */
static int cli_command_is(struct Task *t, const char *tname)
{
    struct Process *pr = (struct Process *)t;
    struct CommandLineInterface *cli;
    const unsigned char *b;
    int len, n = (int)strlen(tname), i, s;
    if (t->tc_Node.ln_Type != NT_PROCESS || !pr->pr_CLI)
        return 0;
    cli = (struct CommandLineInterface *)((ULONG)pr->pr_CLI << 2);
    if (!cli->cli_CommandName)
        return 0;
    b = (const unsigned char *)((ULONG)cli->cli_CommandName << 2);
    len = b[0];
    for (s = len; s > 0 && b[s] != '/' && b[s] != ':'; s--)
        ;
    if (len - s != n)
        return 0;
    for (i = 0; i < n; i++)
        if (b[s + 1 + i] != (unsigned char)tname[i])
            return 0;
    return 1;
}

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
    if (ok && tname[0] && !(t->tc_Node.ln_Name && !strncmp((const char *)t->tc_Node.ln_Name, tname, 31))
        && !cli_command_is(t, tname))
        ok = 0;                 /* the address was reused by another task */
    Permit();
    return ok;
}
