/* addrowner ADDR -- which code owns a hex address (a PC sample of a spinning task): the resident module (RomTag
 * to rt_EndSkip), the process seglist segment (pr_SegList and cli_Module of every task), the DOS resident
 * segment, and every library, device or resource vector (JMP abs.l) whose target lies within 8 KB below or
 * 512 bytes above it (a SetFunction patch or the function itself). Ends with 128 bytes of memory around it for
 * a host disassembler. A second hex argument: where that longword sits in each CLI command's hunks, and the library whose
 * base it is. STACK <task address>: the return addresses on that task's stack (stack()). Used by the s28 flat-loop hunt (thoughts/shared/research/s28-evidence/rig10-flat-off-hang.md). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>

static ULONG want, find;

/* FIND: every longword equal to a register value (a library base in a6) in the CLI command hunks: which variable held it */
static void scan(const char *who, BPTR seg)
{
    int i = 0;
    while (seg && i < 64) {
        ULONG *m = (ULONG *)BADDR(seg);
        ULONG k, nl = (m[-1] - 8) / 4;
        for (k = 0; k < nl && k < 0x40000; k++)
            if (m[1 + k] == find)
                printf("FIND %s hunk %d off %08lx\n", who, i, k * 4);
        seg = (BPTR)m[0];
        i++;
    }
}

static void segs(const char *who, BPTR seg)
{
    int i = 0;
    while (seg && i < 64) {
        ULONG *m = (ULONG *)BADDR(seg);
        ULONG size = m[-1];
        ULONG lo = (ULONG)m, hi = (ULONG)m + size;
        if (want >= lo && want < hi)
            printf("SEG %s hunk %d %08lx-%08lx off %08lx (code off %08lx)\n", who, i, lo, hi, want - lo, want - lo - 4);
        seg = (BPTR)m[0];
        i++;
    }
}

static void libs(const char *kind, struct List *l)
{
    struct Node *n;
    for (n = l->lh_Head; n->ln_Succ; n = n->ln_Succ) {
        struct Library *b = (struct Library *)n;
        UWORD k, nv = b->lib_NegSize / 6;
        for (k = 1; k <= nv && k < 400; k++) {
            UWORD *v = (UWORD *)((UBYTE *)b - 6 * k);
            ULONG t;
            if (k == 1 && find == (ULONG)b)
                printf("LIB %s %s is %08lx\n", kind, n->ln_Name ? n->ln_Name : "?", find);
            if (v[0] != 0x4EF9)
                continue;
            t = ((ULONG)v[1] << 16) | v[2];
            if (t <= want + 512 && t + 8192 >= want)
                printf("VEC %s %s -%d target %08lx (addr %+ld)\n", kind, n->ln_Name ? n->ln_Name : "?", 6 * k, t,
                       (long)want - (long)t);
        }
    }
}

static void task_segs(struct Task *t)
{
    struct Process *p = (struct Process *)t;
    char who[60];
    if (t->tc_Node.ln_Type != NT_PROCESS)
        return;
    sprintf(who, "proc \"%.40s\"", t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "?");
    if (p->pr_SegList) {
        BPTR *a = (BPTR *)BADDR(p->pr_SegList);
        int j;
        for (j = 1; j <= 3 && j <= (int)a[0]; j++)
            segs(who, a[j]);
    }
    if (p->pr_CLI) {
        struct CommandLineInterface *c = (struct CommandLineInterface *)BADDR(p->pr_CLI);
        segs(who, c->cli_Module);
        if (find)
            scan(who, c->cli_Module);
    }
}

/* STACK <task>: the task's code hunks, then every longword from its saved SP to tc_SPUpper that points into one of them
 * (return addresses: the call chain into the spin), into ROM, or into low fast RAM below 0x10020000 */
static int stack(ULONG ta)
{
    struct Task *t = (struct Task *)ta;
    struct Process *p = (struct Process *)t;
    ULONG lo[16], hi[16], *sp, *up;
    int nh = 0, j;
    BPTR seg = 0;
    if (p->pr_CLI)
        seg = ((struct CommandLineInterface *)BADDR(p->pr_CLI))->cli_Module;
    while (seg && nh < 16) {
        ULONG *m = (ULONG *)BADDR(seg);
        lo[nh] = (ULONG)(m + 1);
        hi[nh] = (ULONG)m + m[-1];
        printf("HUNK %d %08lx-%08lx\n", nh, lo[nh], hi[nh]);
        nh++;
        seg = (BPTR)m[0];
    }
    sp = (ULONG *)((ULONG)t->tc_SPReg & ~1UL);
    up = (ULONG *)t->tc_SPUpper;
    printf("SP %08lx UPPER %08lx\n", (ULONG)t->tc_SPReg, (ULONG)up);
    for (; sp < up; sp = (ULONG *)((UBYTE *)sp + 2)) {
        ULONG v = *sp;
        for (j = 0; j < nh; j++)
            if (v >= lo[j] && v < hi[j])
                printf("RET sp+%05lx %08lx hunk %d off %08lx\n", (ULONG)sp - (ULONG)t->tc_SPReg, v, j, v - lo[j]);
        if ((v >= 0x00f80000UL && v < 0x01000000UL) || (v >= 0x10000000UL && v < 0x10020000UL))
            printf("LOW sp+%05lx %08lx\n", (ULONG)sp - (ULONG)t->tc_SPReg, v);
    }
    return 0;
}

int main(int argc, char **argv)
{
    struct ExecBase *eb = SysBase;
    ULONG *rm;
    struct Node *t;
    UBYTE *b;
    int i;
    if (argc < 2)
        return 20;
    if (!strcmp(argv[1], "STACK") && argc > 2)
        return stack(strtoul(argv[2], 0, 16));
    want = strtoul(argv[1], 0, 16);
    if (argc > 2)
        find = strtoul(argv[2], 0, 16);
    printf("ADDR %08lx\n", want);
    Forbid();
    for (rm = (ULONG *)eb->ResModules; rm && *rm;) {
        if (*rm & 0x80000000UL) {
            rm = (ULONG *)(*rm & 0x7fffffffUL);
            continue;
        }
        {
            struct Resident *r = (struct Resident *)*rm;
            if (want >= (ULONG)r && want < (ULONG)r->rt_EndSkip)
                printf("RES %s %08lx-%08lx off %08lx\n", r->rt_IdString ? (char *)r->rt_IdString : r->rt_Name,
                       (ULONG)r, (ULONG)r->rt_EndSkip, want - (ULONG)r);
            else if ((ULONG)r >= 0x10000000UL && (ULONG)r < 0x10100000UL)
                printf("res-low %s %08lx-%08lx\n", r->rt_Name, (ULONG)r, (ULONG)r->rt_EndSkip);
        }
        rm++;
    }
    task_segs(eb->ThisTask);
    for (t = eb->TaskReady.lh_Head; t->ln_Succ; t = t->ln_Succ)
        task_segs((struct Task *)t);
    for (t = eb->TaskWait.lh_Head; t->ln_Succ; t = t->ln_Succ)
        task_segs((struct Task *)t);
    {
        struct DosInfo *di = (struct DosInfo *)BADDR(DOSBase->dl_Root->rn_Info);
        struct Segment *s;
        for (s = (struct Segment *)BADDR(di->di_ResList); s; s = (struct Segment *)BADDR(s->seg_Next)) {
            char who[60];
            sprintf(who, "resident \"%.40s\"", (char *)s->seg_Name + 1);
            if ((LONG)s->seg_Seg > 0 && s->seg_UC >= -1)
                segs(who, s->seg_Seg);
        }
    }
    libs("lib", &eb->LibList);
    libs("dev", &eb->DeviceList);
    libs("res", &eb->ResourceList);
    Permit();
    b = (UBYTE *)((want & ~15UL) - 64);
    for (i = 0; i < 160; i++)
        printf("%s%02x", (i % 16) == 0 ? (i ? "\nMEM " : "MEM ") : " ", b[i]);
    printf("\nMEMBASE %08lx\n", (ULONG)b);
    return 0;
}
