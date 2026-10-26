/* Stand-in for CygnusEd's ced: a cback.o-style detacher. The parent takes the seglist out of
 * cli_Module (so the shell does not unload it), starts a child that runs code in that seglist,
 * and returns at once. The child, 3 s later, asks whether its own code memory is still allocated
 * (AllocAbs succeeds only on free memory), then writes a line and exits. */
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

long __stack = 16384;
const char stackcookie[] = "$STACK:16384\n";

BPTR out_h;
static char line[160];

static void child(void)
{
    APTR p;
    BPTR o;
    Delay(150);
    o = Open("RAM:child.txt", MODE_NEWFILE);
    if (!o) return;
    p = AllocAbs(64, (APTR)(((ULONG)child + 7) & ~7UL));
    if (p) {
        FreeMem(p, 64);
        Write(o, "detach child: my code memory is FREE: the shell unloaded my seglist under me\n", strlen("detach child: my code memory is FREE: the shell unloaded my seglist under me\n"));
    } else {
        Write(o, "detach child: my code memory is still allocated: seglist kept\n", strlen("detach child: my code memory is still allocated: seglist kept\n"));
    }
    Close(o);
}

int main(int argc, char **argv)
{
    struct Process *me = (struct Process *)FindTask(0);
    struct CommandLineInterface *cli = Cli();
    BPTR mod = cli ? cli->cli_Module : 0, s;
    int mine = 0;
    if (argc > 1) {
        sprintf(line, "args: argc=%d argv1=[%s] argstr=[%s] stack=%lu\n", argc, argv[1], (char *)GetArgStr(), (unsigned long)((char *)me->pr_Task.tc_SPUpper - (char *)me->pr_Task.tc_SPLower));
        Write(Output(), line, strlen(line));
        return 0;
    }
    for (s = mod; s; s = *(BPTR *)BADDR(s)) {
        ULONG *h = (ULONG *)BADDR(s);
        if ((ULONG)child >= (ULONG)h && (ULONG)child < (ULONG)h - 4 + h[-1])
            mine = 1;
    }
    sprintf(line, "detach parent: cli_Module=%08lx holds my code=%d pr_CIS=%08lx pr_COS=%08lx\n",
            (unsigned long)mod, mine, (unsigned long)me->pr_CIS, (unsigned long)me->pr_COS);
    Write(Output(), line, strlen(line));
    out_h = Output();
    if (cli && mine)
        cli->cli_Module = 0; /* the seglist is the child's now (cback.o) */
    if (!CreateNewProcTags(NP_Entry, (ULONG)child, NP_Name, (ULONG)"detach child",
                           NP_StackSize, 8000, NP_Output, (ULONG)out_h, NP_CloseOutput, FALSE,
                           NP_Input, (ULONG)me->pr_CIS, NP_CloseInput, FALSE, TAG_END))
        return 20;
    return 0;
}
