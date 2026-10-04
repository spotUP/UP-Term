/* allocwatch SECONDS [MIN] -- which task allocates how much: patches exec
 * AllocMem for SECONDS (default 20), records every allocation of MIN bytes
 * or more (default 4096) with the allocating task's name, then prints them
 * and the total per task. Found the per-window chip cost of an XCON window
 * on a chip-only A1200 (2026-10-04). Leaves the patch in place if someone
 * patched AllocMem after it (prints a warning and keeps running). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <proto/exec.h>
#include <proto/dos.h>

#define N 512
struct rec { ULONG size, flags; struct Task *task; char name[24]; };
static struct rec recs[N];
static volatile int nrec;
static ULONG minsize = 4096;
static APTR (*old)(__reg("d0") ULONG, __reg("d1") ULONG, __reg("a6") struct ExecBase *);

static APTR patched(__reg("d0") ULONG size, __reg("d1") ULONG flags, __reg("a6") struct ExecBase *sb)
{
    if (size >= minsize) {
        int i = nrec;
        if (i < N) {
            struct Task *t = sb->ThisTask;
            const char *s = t && t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "?";
            int k;
            nrec = i + 1;
            recs[i].size = size;
            recs[i].flags = flags;
            recs[i].task = t;
            for (k = 0; k < 23 && s[k]; k++)
                recs[i].name[k] = s[k];
            recs[i].name[k] = 0;
        }
    }
    return old(size, flags, sb);
}

int main(int argc, char **argv)
{
    struct ExecBase *sb = *(struct ExecBase **)4;
    long secs = argc > 1 ? atol(argv[1]) : 20;
    APTR now;
    int i, j;
    if (argc > 2)
        minsize = (ULONG)atol(argv[2]);
    Forbid();
    old = (APTR)SetFunction((struct Library *)sb, -198, (APTR)patched);
    Permit();
    printf("watching %ld s\n", secs);
    fflush(stdout);
    Delay(secs * 50);
    Forbid();
    now = SetFunction((struct Library *)sb, -198, (APTR)old);
    if (now != (APTR)patched) {
        SetFunction((struct Library *)sb, -198, now);
        Permit();
        printf("AllocMem was patched after us; staying resident\n");
        Wait(0);
    }
    Permit();
    for (i = 0; i < nrec; i++)
        printf("%7lu %s %s\n", recs[i].size, (recs[i].flags & MEMF_CHIP) ? "chip" : "any ", recs[i].name);
    for (i = 0; i < nrec; i++) {
        ULONG tot = 0;
        for (j = 0; j < i; j++)
            if (recs[j].task == recs[i].task && !strcmp(recs[j].name, recs[i].name))
                break;
        if (j < i)
            continue;
        for (j = i; j < nrec; j++)
            if (recs[j].task == recs[i].task && !strcmp(recs[j].name, recs[i].name))
                tot += recs[j].size;
        printf("TOTAL %8lu %s\n", tot, recs[i].name);
    }
    return 0;
}
