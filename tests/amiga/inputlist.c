/* inputlist: input.device's handler list, one line per handler (name, priority, is_Code, is_Data), then
 * `handlers N intuition=<0|1> console=<0|1>`. Adds a probe handler at the lowest priority and walks the list
 * from its node under Forbid (the way the SIGWINCH handler leak was found, thoughts/spot/crash/accum2.md).
 * splits_rig.py runs it after host_exit_cycles and pane_close: a freed window's handler left on the list
 * shows as a surviving "vtcon SIGWINCH" and a count above the one at case start. */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <proto/exec.h>

extern struct ExecBase *SysBase;

static struct InputEvent *probe_code(__reg("a0") struct InputEvent *chain, __reg("a1") APTR data)
{
    return chain;
}

int main(void)
{
    static char name[48][40];
    static LONG pri[48], code[48], dat[48];
    static struct Interrupt probe;
    struct MsgPort *port = CreateMsgPort();
    struct IOStdReq *io = port ? (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq)) : 0;
    struct Node *n;
    int k = 0, i, intuition = 0, console = 0;

    if (!io || OpenDevice("input.device", 0, (struct IORequest *)io, 0)) {
        printf("handlers -1 intuition=0 console=0\n");
        return 20;
    }
    probe.is_Code = (void (*)())probe_code;
    probe.is_Data = 0;
    probe.is_Node.ln_Pri = -128;
    probe.is_Node.ln_Name = (char *)"inputlist probe";
    io->io_Command = IND_ADDHANDLER;
    io->io_Data = &probe;
    DoIO((struct IORequest *)io);

    Forbid();
    n = &probe.is_Node;
    while (n->ln_Pred && n->ln_Pred->ln_Pred)
        n = n->ln_Pred;
    for (; n->ln_Succ && k < 48; n = n->ln_Succ, k++) {
        const char *s = n->ln_Name ? n->ln_Name : "(no name)";
        for (i = 0; i < 39 && s[i]; i++)
            name[k][i] = s[i];
        name[k][i] = 0;
        pri[k] = n->ln_Pri;
        code[k] = (LONG)((struct Interrupt *)n)->is_Code;
        dat[k] = (LONG)((struct Interrupt *)n)->is_Data;
    }
    Permit();

    io->io_Command = IND_REMHANDLER;
    io->io_Data = &probe;
    DoIO((struct IORequest *)io);
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);

    for (i = 0; i < k; i++) {
        printf("%s %ld %08lx %08lx\n", name[i], (long)pri[i], (unsigned long)code[i], (unsigned long)dat[i]);
        if (strstr(name[i], "intuition"))
            intuition = 1;
        if (strstr(name[i], "console"))
            console = 1;
    }
    printf("handlers %d intuition=%d console=%d\n", k, intuition, console);
    return 0;
}
