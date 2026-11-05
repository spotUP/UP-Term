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
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <graphics/layers.h>
#include <graphics/clip.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>

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
    ULONG sem, sowner;          /* SIGF_SINGLE wait: the semaphore whose queue holds this task's request, and its owner */
    WORD snest, squeue;
    char sname[40], soname[40];
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
    /* ObtainSemaphore[Shared] queues a SemaphoreRequest {succ, pred, waiter} on the caller's stack: find the one naming this
     * task, walk pred back to the wait queue's head node (pred 0), and that head sits 16 bytes into the SignalSemaphore */
    if ((t->tc_SigWait & SIGF_SINGLE) && p->sp >= p->lower) {
        ULONG a;
        for (a = p->sp & ~1UL; a + 12 <= p->upper && !p->sem; a += 2) {
            struct MinNode *r = (struct MinNode *)a, *q;
            int hop;
            if ((((ULONG *)a)[2] & ~1UL) != (ULONG)t || !r->mln_Succ || !r->mln_Pred || (((ULONG)r->mln_Pred) & 1))
                continue;
            for (q = r, hop = 0; q->mln_Pred && hop < 64; hop++) {
                if (((ULONG)q->mln_Pred) & 1)
                    break;
                q = q->mln_Pred;
            }
            /* a shared request stores task|1 as its waiter; a free semaphore (queue -1) holds no request, so skip stale frames */
            if (!q->mln_Pred && ((struct SignalSemaphore *)((ULONG)q - 16))->ss_QueueCount >= 0) {
                struct SignalSemaphore *ss = (struct SignalSemaphore *)((ULONG)q - 16);
                struct Task *o = ss->ss_Owner;
                p->sem = (ULONG)ss;
                p->sowner = (ULONG)o;
                p->snest = ss->ss_NestCount;
                p->squeue = ss->ss_QueueCount;
                strncpy(p->sname, ss->ss_Link.ln_Name && !(((ULONG)ss->ss_Link.ln_Name) & 0xff000000UL) ? ss->ss_Link.ln_Name : "-", sizeof(p->sname) - 1);
                strncpy(p->soname, o && o->tc_Node.ln_Name ? o->tc_Node.ln_Name : "-", sizeof(p->soname) - 1);
            }
        }
    }
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

static const char *tname(struct Task *t)
{
    return t && t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "-";
}

/* "taskdump SEM <hex>": name a semaphore found by a semwait line: the dos.library DosInfo locks, a library or device
 * base that holds it, and every task whose stack carries its address (a shared holder leaves no owner field) */
static void semwho(ULONG a)
{
    struct RootNode *rn = (struct RootNode *)DOSBase->dl_Root;
    struct DosInfo *di = (struct DosInfo *)BADDR(rn->rn_Info);
    struct SignalSemaphore *ss = (struct SignalSemaphore *)a;
    struct List *lists[2];
    struct Node *nd;
    int i;
    printf("sem %08lx owner %08lx nest %d queue %d\n", a, (ULONG)ss->ss_Owner, (int)ss->ss_NestCount, (int)ss->ss_QueueCount);
    printf("dosinfo devlock %08lx entrylock %08lx deletelock %08lx\n", (ULONG)&di->di_DevLock, (ULONG)&di->di_EntryLock,
           (ULONG)&di->di_DeleteLock);
    {
        struct GfxBase *gb = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 36);
        if (gb) {
            /* SIGB_BLIT is SIGB_SINGLE: a task in OwnBlitter waits on bit 4 too, queued on BlitWaitQ */
            printf("gfxbase %08lx blitwaitq %08lx blitowner %08lx \"%s\" blitlock %d\n", (ULONG)gb, (ULONG)&gb->BlitWaitQ,
                   (ULONG)gb->BlitOwner, tname(gb->BlitOwner), (int)gb->BlitLock);
            CloseLibrary((struct Library *)gb);
        }
    }
    lists[0] = &SysBase->LibList;
    lists[1] = &SysBase->DeviceList;
    Forbid();
    for (i = 0; i < 2; i++)
        for (nd = lists[i]->lh_Head; nd->ln_Succ; nd = nd->ln_Succ) {
            struct Library *lb = (struct Library *)nd;
            if (a >= (ULONG)lb && a < (ULONG)lb + lb->lib_PosSize)
                printf("in base %s +%lx\n", nd->ln_Name, a - (ULONG)lb);
        }
    for (i = 0; i < 3; i++) {
        struct List *l = i == 0 ? &SysBase->TaskWait : i == 1 ? &SysBase->TaskReady : 0;
        struct Task *t;
        for (nd = l ? l->lh_Head : 0; nd && nd->ln_Succ; nd = nd->ln_Succ) {
            ULONG x;
            t = (struct Task *)nd;
            for (x = ((ULONG)t->tc_SPReg) & ~1UL; x + 4 <= (ULONG)t->tc_SPUpper; x += 2)
                if (*(ULONG *)x == a)
                    printf("on stack %08lx \"%s\" at sp+%lx\n", (ULONG)t, nd->ln_Name ? nd->ln_Name : "?", x - (ULONG)t->tc_SPReg);
        }
    }
    Permit();
}

/* "taskdump LAYERS": every screen's LayerInfo lock and every layer lock with owner, and the window and the task behind
 * each layer (its IDCMP port's task), read under Forbid without taking any of the locks */
static void waiters(const char *what, struct SignalSemaphore *ss)
{
    struct SemaphoreRequest *r;
    printf("    %s %08lx owner %08lx \"%s\" nest %d queue %d waiters:", what, (ULONG)ss, (ULONG)ss->ss_Owner,
           tname(ss->ss_Owner), (int)ss->ss_NestCount, (int)ss->ss_QueueCount);
    for (r = (struct SemaphoreRequest *)ss->ss_WaitQueue.mlh_Head; r->sr_Link.mln_Succ;
         r = (struct SemaphoreRequest *)r->sr_Link.mln_Succ) {
        struct Task *w = (struct Task *)((ULONG)r->sr_Waiter & ~1UL);
        printf(" %s\"%s\"", ((ULONG)r->sr_Waiter & 1UL) ? "shared:" : "", tname(w));
    }
    printf("\n");
}

/* "taskdump DEVS": every DOS list entry (device, volume, assign) with its handler port, the port's task and the
 * number of packets queued on that port, read under Forbid without LockDosList (the list lock may be the one held) */
static void devs(void)
{
    struct DosLibrary *db = (struct DosLibrary *)OpenLibrary((STRPTR)"dos.library", 36);
    struct RootNode *rn;
    struct DosInfo *di;
    struct DosList *dl;
    int n = 0;
    if (!db)
        return;
    rn = (struct RootNode *)db->dl_Root;
    di = (struct DosInfo *)BADDR(rn->rn_Info);
    Forbid();
    for (dl = (struct DosList *)BADDR(di->di_DevInfo); dl && n < 200; dl = (struct DosList *)BADDR(dl->dol_Next), n++) {
        UBYTE *bn = (UBYTE *)BADDR(dl->dol_Name);
        struct MsgPort *mp = dl->dol_Task;
        struct Task *t = mp ? (struct Task *)mp->mp_SigTask : 0;
        int q = 0;
        struct Node *m;
        if (mp)
            for (m = mp->mp_MsgList.lh_Head; m->ln_Succ && q < 999; m = m->ln_Succ)
                q++;
        printf("dol %08lx type %ld \"%.*s\" port %08lx task %08lx \"%s\" state %d wait %08lx msgs %d\n", (ULONG)dl,
               (long)dl->dol_Type, bn ? (int)bn[0] : 0, bn ? (char *)bn + 1 : "", (ULONG)mp, (ULONG)t, tname(t),
               t ? (int)t->tc_State : -1, t ? (ULONG)t->tc_SigWait : 0, q);
    }
    Permit();
    CloseLibrary((struct Library *)db);
}

static void layers(void)
{
    struct IntuitionBase *ib = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    struct Screen *sc;
    if (!ib)
        return;
    Forbid();
    for (sc = ib->FirstScreen; sc; sc = sc->NextScreen) {
        struct SignalSemaphore *li = &sc->LayerInfo.Lock;
        struct Layer *l;
        printf("screen %08lx \"%.30s\" layerinfo %08lx owner %08lx \"%s\" nest %d queue %d\n", (ULONG)sc,
               sc->Title ? (char *)sc->Title : "-", (ULONG)li, (ULONG)li->ss_Owner, tname(li->ss_Owner),
               (int)li->ss_NestCount, (int)li->ss_QueueCount);
        waiters("layerinfo", li);
        if (sc->ViewPort.ColorMap && sc->ViewPort.ColorMap->PalExtra)
            waiters("palextra", &sc->ViewPort.ColorMap->PalExtra->pe_Semaphore);
        for (l = sc->LayerInfo.top_layer; l; l = l->back) {
            struct Window *w = l->Window;
            struct Task *wt = w && w->UserPort ? (struct Task *)w->UserPort->mp_SigTask : 0;
            printf("  layer %08lx lock %08lx owner %08lx \"%s\" nest %d queue %d win %08lx \"%.30s\" task %08lx \"%s\" ptr %08lx\n",
                   (ULONG)l, (ULONG)&l->Lock, (ULONG)l->Lock.ss_Owner, tname(l->Lock.ss_Owner), (int)l->Lock.ss_NestCount,
                   (int)l->Lock.ss_QueueCount, (ULONG)w, w && w->Title ? (char *)w->Title : "-", (ULONG)wt, tname(wt),
                   w ? (ULONG)w->Pointer : 0);
        }
    }
    Permit();
    CloseLibrary((struct Library *)ib);
}

int main(int argc, char **argv)
{
    struct ExecBase *eb = SysBase;
    struct Node *t;
    int i, k, h;
    if (argc == 2 && !strcmp(argv[1], "DEVS")) {
        devs();
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "LAYERS")) {
        layers();
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "SEM")) {
        ULONG a = 0;
        sscanf(argv[2], "%lx", &a);
        if (a && !(a & 1)) {
            semwho(a);
            Forbid();
            if (((struct SignalSemaphore *)a)->ss_QueueCount >= 0)
                waiters("sem", (struct SignalSemaphore *)a);
            Permit();
        }
        return 0;
    }
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
        if (p->sem)
            printf("  semwait %08lx \"%s\" owner %08lx \"%s\" nest %d queue %d\n", p->sem, p->sname, p->sowner, p->soname,
                   (int)p->snest, (int)p->squeue);
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
