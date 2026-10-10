/* inwho -- whose stream is this program's input? (tools/rig/shstdin_rig.py)
 *
 * Prints one line: Input()'s handler (the task behind the handle's port),
 * whether it is interactive, and the handler of pr_ConsoleTask, the port
 * Open("*") goes to. A program vsh starts in the foreground on vsh's own
 * input must get that input, not a second handle on some other console. */
#include <stdio.h>
#include <string.h>
#include <exec/execbase.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>

static const char *port_task(struct MsgPort *p)
{
    struct Task *t;
    if (!p)
        return "none";
    t = (struct Task *)p->mp_SigTask;
    return t && t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "?";
}

int main(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    BPTR in = Input();
    struct FileHandle *fh = in ? (struct FileHandle *)BADDR(in) : 0;
    printf("in=%s interactive=%d console=%s same=%d\n", fh ? port_task(fh->fh_Type) : "0",
           in ? (int)(IsInteractive(in) != 0) : -1, port_task(me->pr_ConsoleTask),
           fh && fh->fh_Type == me->pr_ConsoleTask);
    return 0;
}
