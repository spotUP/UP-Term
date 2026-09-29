/* breakport -- a child process makes itself the console's Ctrl-C target
 * (ACTION_CHANGE_SIGNAL, as the ROM Shell does for the commands it runs)
 * and ends without handing it back; then this process waits 5 seconds
 * for Ctrl-C. The console must not keep signalling (or reading) a process
 * that is gone: the break has to come back through the shell, so the
 * answer is "got the break" (rig: tools/rig/sessions/vsh_session.py). */
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>

static struct MsgPort *console;
static LONG console_arg;
static struct Task *parent;
static LONG done_sig;
static LONG changed;

static void child(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    changed = DoPkt(console, ACTION_CHANGE_SIGNAL, console_arg, (LONG)&me->pr_MsgPort, 0, 0, 0);
    Forbid(); /* the signal and our end together */
    Signal(parent, 1UL << done_sig);
}

int main(void)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(Input());
    ULONG got;
    int i;
    if (!fh || !fh->fh_Type)
        return 20;
    console = fh->fh_Type;
    console_arg = fh->fh_Arg1;
    parent = FindTask(0);
    done_sig = AllocSignal(-1);
    if (done_sig < 0)
        return 20;
    SetSignal(0, SIGBREAKF_CTRL_C | (1UL << done_sig));
    if (!CreateNewProcTags(NP_Entry, (ULONG)child, NP_Name, (ULONG)"breakport child",
                           NP_Input, 0, NP_Output, 0, NP_CloseInput, FALSE,
                           NP_CloseOutput, FALSE, TAG_DONE))
        return 20;
    Wait(1UL << done_sig);
    Delay(10); /* the child is gone */
    Printf("breakport: the Ctrl-C target was set to a process that ended (%s); press Ctrl-C\n",
           changed ? "ok" : "refused");
    Flush(Output());
    got = 0;
    for (i = 0; i < 50 && !got; i++) {
        Delay(5);
        got = SetSignal(0, 0) & SIGBREAKF_CTRL_C;
    }
    Printf("breakport: %s\n", got ? "got the break" : "NO BREAK");
    FreeSignal(done_sig);
    return got ? 0 : 10;
}
