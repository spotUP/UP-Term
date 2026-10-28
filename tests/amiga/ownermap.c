/* ownermap [ADDR] -- who owns an address, and which commodities brokers have no live task
 * (INSTALLER-END-CRASH). One line per range, snapshot inside Forbid(), printed after Permit()
 * (printing inside Forbid froze the guest in s15):
 *   range START END KIND NAME     KIND: seg (hunk of a process's pr_SegList or cli_Module), lib, dev, res
 *   broker NAME TITLE task=ADDR alive=0|1 port=ADDR
 * With a hex ADDR: "owner ADDR in <range line>" or "owner ADDR in no mapped range, nearest <range line>",
 * plus exec's TypeOfMem. Brokers come from commodities.library's private CopyBrokerList (LVO -186,
 * FreeBrokerList -192), the pair Exchange uses; the BrokerCopy layout is read by offset:
 * name 24 at 14, title 40 at 38, descr 40 at 78, task at 118, port at 122. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>

#define MAXR 700
#define MAXB 32

struct rng {
    ULONG lo, hi;
    char kind[4];
    char name[40];
};
struct brk {
    char name[25], title[41];
    ULONG task, port;
    int alive;
};

static struct rng r[MAXR];
static struct brk b[MAXB];
static int nr, nb;

void CopyBrokerList(__reg("a6") struct Library *, __reg("a0") struct List *) = "\tjsr\t-186(a6)";
void FreeBrokerList(__reg("a6") struct Library *, __reg("a0") struct List *) = "\tjsr\t-192(a6)";

static void add(ULONG lo, ULONG hi, const char *kind, const char *name)
{
    struct rng *p;
    if (nr >= MAXR)
        return;
    p = &r[nr++];
    p->lo = lo;
    p->hi = hi;
    strncpy(p->kind, kind, 3);
    strncpy(p->name, name ? name : "?", sizeof(p->name) - 1);
}

/* every hunk of a seglist (BPTR to the first segment) */
static void segs(BPTR seg, const char *who)
{
    int guard = 0;
    while (seg && guard++ < 200) {
        ULONG *s = (ULONG *)BADDR(seg);
        add((ULONG)s, (ULONG)s - 4 + s[-1], "seg", who);
        seg = (BPTR)s[0];
    }
}

static void take_proc(struct Task *t)
{
    struct Process *pr;
    struct CommandLineInterface *cli;
    const char *nm = t->tc_Node.ln_Name;
    if (t->tc_Node.ln_Type != NT_PROCESS)
        return;
    pr = (struct Process *)t;
    if (pr->pr_SegList) {
        ULONG *a = (ULONG *)BADDR(pr->pr_SegList);
        if (a[0] >= 3)
            segs((BPTR)a[3], nm);
    }
    cli = (struct CommandLineInterface *)BADDR(pr->pr_CLI);
    if (cli && cli->cli_Module)
        segs(cli->cli_Module, nm);
}

static int is_task(ULONG addr, struct ExecBase *eb)
{
    struct Node *t;
    if (!addr)
        return 0;
    if ((ULONG)eb->ThisTask == addr)
        return 1;
    for (t = eb->TaskReady.lh_Head; t->ln_Succ; t = t->ln_Succ)
        if ((ULONG)t == addr)
            return 1;
    for (t = eb->TaskWait.lh_Head; t->ln_Succ; t = t->ln_Succ)
        if ((ULONG)t == addr)
            return 1;
    return 0;
}

static void libs(struct List *l, const char *kind)
{
    struct Node *n;
    for (n = l->lh_Head; n->ln_Succ; n = n->ln_Succ) {
        struct Library *lb = (struct Library *)n;
        add((ULONG)lb - lb->lib_NegSize, (ULONG)lb + lb->lib_PosSize, kind, n->ln_Name);
    }
}

static void show(const struct rng *p)
{
    printf("range %08lx %08lx %s %s\n", p->lo, p->hi, p->kind, p->name);
}

int main(int argc, char **argv)
{
    struct ExecBase *eb = SysBase;
    struct Library *cx = OpenLibrary((STRPTR)"commodities.library", 0);
    struct List bl;
    struct Node *n, *t;
    ULONG want = argc > 1 ? strtoul(argv[1], 0, 16) : 0;
    int i, best = -1;
    ULONG bd = 0xffffffff;

    bl.lh_Head = (struct Node *)&bl.lh_Tail;
    bl.lh_Tail = 0;
    bl.lh_TailPred = (struct Node *)&bl.lh_Head;
    if (cx) {
        CopyBrokerList(cx, &bl);
        for (n = bl.lh_Head; n->ln_Succ && nb < MAXB; n = n->ln_Succ) {
            UBYTE *u = (UBYTE *)n;
            struct brk *p = &b[nb++];
            memcpy(p->name, u + 14, 24);
            memcpy(p->title, u + 38, 40);
            p->task = (ULONG)(((ULONG)u[118] << 24) | (u[119] << 16) | (u[120] << 8) | u[121]);
            p->port = (ULONG)(((ULONG)u[122] << 24) | (u[123] << 16) | (u[124] << 8) | u[125]);
        }
        FreeBrokerList(cx, &bl);
    }
    Forbid();
    take_proc(eb->ThisTask);
    for (t = eb->TaskReady.lh_Head; t->ln_Succ; t = t->ln_Succ)
        take_proc((struct Task *)t);
    for (t = eb->TaskWait.lh_Head; t->ln_Succ; t = t->ln_Succ)
        take_proc((struct Task *)t);
    libs(&eb->LibList, "lib");
    libs(&eb->DeviceList, "dev");
    libs(&eb->ResourceList, "res");
    for (i = 0; i < nb; i++)
        b[i].alive = is_task(b[i].task, eb);
    Permit();
    if (cx)
        CloseLibrary(cx);
    for (i = 0; i < nr; i++) {
        show(&r[i]);
        if (want) {
            ULONG d = want < r[i].lo ? r[i].lo - want : want >= r[i].hi ? want - r[i].hi + 1 : 0;
            if (d < bd) {
                bd = d;
                best = i;
            }
        }
    }
    for (i = 0; i < nb; i++)
        printf("broker \"%s\" \"%s\" task=%08lx alive=%d port=%08lx\n", b[i].name, b[i].title, b[i].task,
               b[i].alive, b[i].port);
    if (want) {
        printf("owner %08lx typeofmem=%lx ", want, (unsigned long)TypeOfMem((APTR)want));
        if (best >= 0 && bd == 0)
            printf("in %s %s %08lx-%08lx\n", r[best].kind, r[best].name, r[best].lo, r[best].hi);
        else if (best >= 0)
            printf("in no mapped range, nearest %s %s %08lx-%08lx (%lu bytes away)\n", r[best].kind, r[best].name,
                   r[best].lo, r[best].hi, bd);
        else
            printf("in no mapped range\n");
    }
    printf("ownermap: %d ranges %d brokers\n", nr, nb);
    return 0;
}
