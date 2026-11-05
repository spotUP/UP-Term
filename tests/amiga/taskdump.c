/* taskdump [NAME...] -- every task and process exec knows of (ready, waiting, this one): name,
 * state, the signals it waits for, has and owns, its stack pointer and the first longwords of its saved
 * stack, and for a process its CLI command name, the messages on its port and the hunks of the command
 * (cli_Module). With names: only the tasks whose name starts with one of them. Each saved-stack longword
 * that falls in a library's code is resolved to "lib LVO+off" (the nearest jump-table target below it)
 * or "hunk N+off" of the command, and the held public semaphores (owner, waiters) and the public ports
 * (signal task, bit) follow, so a hang names the call and the object it waits on. The rig reads where a
 * hung pipeline's processes wait (tools/rig/repeat_rig.py runs it when a pass hangs). */
#include <stdio.h>
#include <string.h>
#include <exec/execbase.h>
#include <exec/semaphores.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>

long __stack = 32768;
const char __stackcookie[] = "$STACK:32768";

#define MAXT 64
#define NSTK 64
#define NHUNK 8

struct snap {
    char name[40];
    char cmd[40];
    UBYTE state, type;
    BYTE pri;
    ULONG wait, recvd, alloc, sp, lower, upper, self, seglist, msgs, port;
    ULONG stk[NSTK];
    ULONG hlo[NHUNK], hhi[NHUNK];
    int nh;
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
    p->alloc = t->tc_SigAlloc;
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
        p->port = (ULONG)&pr->pr_MsgPort;
        for (m = pr->pr_MsgPort.mp_MsgList.lh_Head; m->ln_Succ; m = m->ln_Succ)
            p->msgs++;
        if (cli && cli->cli_CommandName) {
            UBYTE *b = (UBYTE *)BADDR(cli->cli_CommandName);
            int l = b[0] < sizeof(p->cmd) - 1 ? b[0] : sizeof(p->cmd) - 1;
            memcpy(p->cmd, b + 1, l);
        }
        if (cli && cli->cli_Module) {
            ULONG *h = (ULONG *)BADDR(cli->cli_Module);
            while (h && p->nh < NHUNK) {
                p->hlo[p->nh] = (ULONG)h;
                p->hhi[p->nh] = (ULONG)h + h[-1];   /* the hunk's allocation size sits before it */
                p->nh++;
                h = (ULONG *)BADDR(h[0]);
            }
        }
    }
}

/* the library function whose code starts nearest below addr (jump-table targets of every library and device) */
static int resolve(ULONG addr, char *out)
{
    struct List *lists[2];
    struct Node *nd;
    ULONG best = 0;
    const char *bname = 0;
    int i, blvo = 0;
    if (addr < 0x400 || (addr & 1))
        return 0;
    lists[0] = &SysBase->LibList;
    lists[1] = &SysBase->DeviceList;
    for (i = 0; i < 2; i++)
        for (nd = lists[i]->lh_Head; nd->ln_Succ; nd = nd->ln_Succ) {
            struct Library *lb = (struct Library *)nd;
            int k, nv = lb->lib_NegSize / 6;
            for (k = 1; k <= nv && k < 400; k++) {
                UBYTE *e = (UBYTE *)lb - 6 * k;
                ULONG tgt;
                if (e[0] != 0x4e || e[1] != 0xf9)
                    continue;
                tgt = *(ULONG *)(e + 2);
                if (tgt <= addr && addr - tgt < 0x2000 && tgt > best) {
                    best = tgt;
                    bname = nd->ln_Name;
                    blvo = -6 * k;
                }
            }
        }
    if (!bname)
        return 0;
    sprintf(out, "%.30s %d+%lx", bname, blvo, addr - best);
    return 1;
}

static void objects(void)
{
    struct Node *nd;
    Forbid();
    for (nd = SysBase->SemaphoreList.lh_Head; nd->ln_Succ; nd = nd->ln_Succ) {
        struct SignalSemaphore *ss = (struct SignalSemaphore *)nd;
        struct MinNode *w;
        if (ss->ss_QueueCount < 0 && !ss->ss_Owner)
            continue;
        printf("sem %08lx \"%s\" owner %08lx \"%s\" nest %d queue %d waiters", (ULONG)ss, nd->ln_Name ? nd->ln_Name : "?",
               (ULONG)ss->ss_Owner, ss->ss_Owner && ss->ss_Owner->tc_Node.ln_Name ? ss->ss_Owner->tc_Node.ln_Name : "-",
               (int)ss->ss_NestCount, (int)ss->ss_QueueCount);
        for (w = ss->ss_WaitQueue.mlh_Head; w->mln_Succ; w = w->mln_Succ)
            printf(" %08lx", (ULONG)((struct SemaphoreRequest *)w)->sr_Waiter);
        printf("\n");
    }
    for (nd = SysBase->PortList.lh_Head; nd->ln_Succ; nd = nd->ln_Succ) {
        struct MsgPort *mp = (struct MsgPort *)nd;
        printf("port %08lx \"%s\" sigtask %08lx bit %d\n", (ULONG)mp, nd->ln_Name ? nd->ln_Name : "?", (ULONG)mp->mp_SigTask,
               (int)mp->mp_SigBit);
    }
    Permit();
}

int main(int argc, char **argv)
{
    struct ExecBase *eb = SysBase;
    struct Node *t;
    int i, k, h;
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
        printf("task %08lx \"%s\" cmd \"%s\" state %d pri %d wait %08lx recvd %08lx alloc %08lx msgs %lu port %08lx sp %08lx stack %08lx-%08lx seglist %08lx\n",
               p->self, p->name, p->cmd, (int)p->state, (int)p->pri, p->wait, p->recvd, p->alloc, p->msgs, p->port, p->sp,
               p->lower, p->upper, p->seglist);
        for (h = 0; h < p->nh; h++)
            printf("  hunk %d %08lx-%08lx\n", h, p->hlo[h], p->hhi[h]);
        for (k = 0; k < NSTK; k++)
            printf("%08lx%s", p->stk[k], (k % 8) == 7 ? "\n" : " ");
        for (k = 0; k < NSTK; k++) {
            char sym[64];
            int done = 0;
            for (h = 0; h < p->nh && !done; h++)
                if (p->stk[k] >= p->hlo[h] + 4 && p->stk[k] < p->hhi[h]) {
                    printf("  sp+%d %08lx hunk %d+%lx\n", 4 * k, p->stk[k], h, p->stk[k] - p->hlo[h] - 4);
                    done = 1;
                }
            if (!done && resolve(p->stk[k], sym))
                printf("  sp+%d %08lx %s\n", 4 * k, p->stk[k], sym);
        }
    }
    objects();
    return 0;
}
