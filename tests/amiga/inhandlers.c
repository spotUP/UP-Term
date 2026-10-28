/* inhandlers: the names of input.device's handlers, one per line, in chain order (winchleak_rig.py:
 * no "vtcon SIGWINCH" node left once every window is gone). Finds the chain by adding a handler of
 * its own and walking back to the head, as chainprobe does; takes it out again before it prints. */
#include <stdio.h>
#include <string.h>
#include <exec/interrupts.h>
#include <exec/nodes.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <proto/exec.h>

static volatile APTR sink;

static struct InputEvent *pass(__reg("a0") struct InputEvent *ev, __reg("a1") APTR data)
{
    sink = data;
    return ev;
}

int main(void)
{
    static char names[40][40];
    static struct Interrupt ours;
    struct MsgPort *port = CreateMsgPort();
    struct IOStdReq *io = port ? (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq)) : 0;
    struct Node *n;
    int k = 0, i;
    if (!io || OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)io, 0))
        return 20;
    ours.is_Code = (void (*)())pass;
    ours.is_Node.ln_Pri = -100;
    ours.is_Node.ln_Name = (char *)"inhandlers";
    ours.is_Node.ln_Type = NT_INTERRUPT;
    io->io_Command = IND_ADDHANDLER;
    io->io_Data = &ours;
    DoIO((struct IORequest *)io);
    Forbid();
    n = &ours.is_Node;
    while (n->ln_Pred && n->ln_Pred->ln_Pred)
        n = n->ln_Pred;
    for (; n->ln_Succ && k < 40; n = n->ln_Succ, k++) {
        const char *s = n->ln_Name ? n->ln_Name : "(no name)";
        for (i = 0; i < 39 && s[i]; i++)
            names[k][i] = s[i];
        names[k][i] = 0;
    }
    Permit();
    io->io_Command = IND_REMHANDLER;
    io->io_Data = &ours;
    DoIO((struct IORequest *)io);
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);
    for (i = 0; i < k; i++)
        printf("%s\n", names[i]);
    return 0;
}
