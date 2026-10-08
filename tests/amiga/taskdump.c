/* taskdump [NAME...] -- every task and process exec knows of (ready, waiting, this one): name,
 * state, the signals it waits for and has, its stack pointer and the first longwords of its saved
 * stack, and for a process its CLI command name and the messages on its port. With names: only the
 * tasks whose name starts with one of them. The rig reads where a hung pipeline's processes wait
 * (tools/rig/repeat_rig.py runs it when a pass hangs). */
#include <stdio.h>
#include <string.h>
#include <exec/execbase.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>

#define MAXT 64
#define NSTK 40

struct snap {
    char name[40];
    char cmd[40];
    UBYTE state, type;
    BYTE pri;
    ULONG wait, recvd, sp, lower, upper, self, seglist, msgs;
    ULONG stk[NSTK];
};

static struct snap s[MAXT];
static int n;

static int wanted(const char *name, int argc, char **argv)
{
    int i;
    if (argc < 2)
        return 1;
    for (i = 1; i < argc; i++)
        if (!strncmp(name, argv[i], strlen(argv[i])))
            return 1;
    return 0;
}

static void take(struct Task *t)
{
    struct snap *p;
    int i;
    if (n >= MAXT)
        return;
    p = &s[n++];
    strncpy(p->name, t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "?", sizeof(p->name) - 1);
    p->state = t->tc_State;
    p->type = t->tc_Node.ln_Type;
    p->pri = t->tc_Node.ln_Pri;
    p->wait = t->tc_SigWait;
    p->recvd = t->tc_SigRecvd;
    p->sp = (ULONG)t->tc_SPReg;
    p->lower = (ULONG)t->tc_SPLower;
    p->upper = (ULONG)t->tc_SPUpper;
    p->self = (ULONG)t;
    for (i = 0; i < NSTK; i++)
        p->stk[i] = (p->sp >= p->lower && p->sp + 4 * i + 4 <= p->upper) ? ((ULONG *)p->sp)[i] : 0;
    if (t->tc_Node.ln_Type == NT_PROCESS) {
        struct Process *pr = (struct Process *)t;
        struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(pr->pr_CLI);
        struct Node *m;
        p->seglist = (ULONG)pr->pr_SegList;
        for (m = pr->pr_MsgPort.mp_MsgList.lh_Head; m->ln_Succ; m = m->ln_Succ)
            p->msgs++;
        if (cli && cli->cli_CommandName) {
            UBYTE *b = (UBYTE *)BADDR(cli->cli_CommandName);
            int l = b[0] < sizeof(p->cmd) - 1 ? b[0] : sizeof(p->cmd) - 1;
            memcpy(p->cmd, b + 1, l);
        }
    }
}

int main(int argc, char **argv)
{
    struct ExecBase *eb = SysBase;
    struct Node *t;
    int i, k;
    Disable();
    take(eb->ThisTask);
    for (t = eb->TaskReady.lh_Head; t->ln_Succ; t = t->ln_Succ)
        take((struct Task *)t);
    for (t = eb->TaskWait.lh_Head; t->ln_Succ; t = t->ln_Succ)
        take((struct Task *)t);
    Enable();
    for (i = 0; i < n; i++) {
        struct snap *p = &s[i];
        if (!wanted(p->name, argc, argv))
            continue;
        printf("task %08lx \"%s\" cmd \"%s\" state %d pri %d wait %08lx recvd %08lx msgs %lu sp %08lx stack %08lx-%08lx seglist %08lx\n",
               p->self, p->name, p->cmd, (int)p->state, (int)p->pri, p->wait, p->recvd, p->msgs, p->sp, p->lower,
               p->upper, p->seglist);
        for (k = 0; k < NSTK; k++)
            printf("%08lx%s", p->stk[k], (k % 8) == 7 ? "\n" : " ");
    }
    return 0;
}
