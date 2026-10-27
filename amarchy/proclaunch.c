/* proclaunch: see proclaunch.h. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "proclaunch.h"
#define SEGSTACK_DOS
#include "../config/segstack.h"

BPTR pl_dup_path(void)
{
    struct CommandLineInterface *cli = Cli();
    BPTR head = 0, *tail = &head, p;
    if (!cli)
        return 0;
    for (p = cli->cli_CommandDir; p; ) {
        BPTR *src = (BPTR *)BADDR(p);
        BPTR *n = (BPTR *)AllocVec(2 * sizeof(BPTR), MEMF_PUBLIC | MEMF_CLEAR);
        if (!n)
            break;
        n[1] = DupLock(src[1]);
        *tail = MKBADDR(n);
        tail = &n[0];
        p = src[0];
    }
    return head;
}

void pl_free_path(BPTR p)
{
    while (p) {
        BPTR *n = (BPTR *)BADDR(p);
        BPTR next = n[0];
        if (n[1])
            UnLock(n[1]);
        FreeVec(n);
        p = next;
    }
}

ULONG pl_stack(const char *prog, ULONG floor)
{
    ULONG stack = floor;
    BPTR seg = prog ? LoadSeg((STRPTR)prog) : 0;
    if (seg) {
        ULONG v = seg_stack(seg);
        if (v > stack)
            stack = v;
        UnLoadSeg(seg);
    }
    return stack;
}

int pl_start(const char *line, const char *prog, ULONG floor, const char *name)
{
    ULONG stack = pl_stack(prog, floor);
    BPTR in, out, p;
    LONG rc;
    in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    p = pl_dup_path();
    rc = SystemTags((STRPTR)line, SYS_Input, in, SYS_Output, out, SYS_Asynch, TRUE,
                    name ? NP_Name : TAG_IGNORE, (ULONG)name, NP_StackSize, stack,
                    p ? NP_Path : TAG_IGNORE, p, TAG_END);
    if (rc == -1) {                 /* not started: the files and path are still ours */
        if (in)
            Close(in);
        if (out)
            Close(out);
        pl_free_path(p);
        return 0;
    }
    return 1;
}

LONG pl_run(const char *line, const char *prog, ULONG floor, BPTR out)
{
    BPTR in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    LONG rc = SystemTags((STRPTR)line, SYS_Input, in, SYS_Output, out, NP_StackSize, pl_stack(prog, floor),
                         TAG_END);
    if (in)
        Close(in);
    return rc;
}
