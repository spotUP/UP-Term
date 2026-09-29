/* sizewatch TITLE SECONDS -- count the IECLASS_SIZEWINDOW input events for
 * the window titled TITLE, seen from an input.device handler at priority
 * 10, where ixemul's SIGWINCH handler sits (ix_sigwinch.c): does a resize
 * of that window reach the place SIGWINCH is raised from? */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/interrupts.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>

struct IntuitionBase *IntuitionBase;
static struct Window *watched;
static volatile LONG seen, any, total;

static struct InputEvent *watch(__reg("a0") struct InputEvent *ev, __reg("a1") APTR data)
{
    struct InputEvent *e;
    for (e = ev; e; e = e->ie_NextEvent) {
        total++;
        if (e->ie_Class == IECLASS_SIZEWINDOW) {
            any++;
            if (e->ie_EventAddress == (APTR)watched)
                seen++;
        }
    }
    return ev;
}

int main(int argc, char **argv)
{
    struct MsgPort *port;
    struct IOStdReq *io;
    struct Interrupt irq;
    struct Screen *s;
    ULONG lock;
    if (argc < 3)
        return 20;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    if (!IntuitionBase)
        return 20;
    lock = LockIBase(0);
    for (s = IntuitionBase->FirstScreen; s && !watched; s = s->NextScreen)
        for (watched = s->FirstWindow; watched; watched = watched->NextWindow)
            if (watched->Title && !strcmp((char *)watched->Title, argv[1]))
                break;
    UnlockIBase(lock);
    port = CreateMsgPort();
    io = (struct IOStdReq *)CreateIORequest(port, sizeof(*io));
    if (!watched || !io || OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)io, 0)) {
        printf("no window or input.device\n");
        return 10;
    }
    memset(&irq, 0, sizeof(irq));
    irq.is_Code = (void (*)())watch;
    irq.is_Node.ln_Pri = 10;
    irq.is_Node.ln_Name = "sizewatch";
    io->io_Data = &irq;
    io->io_Command = IND_ADDHANDLER;
    DoIO((struct IORequest *)io);
    {
        /* input.device's handler list, from our own node's links */
        struct Node *n = &irq.is_Node;
        Forbid();
        while (n->ln_Pred && n->ln_Pred->ln_Pred)
            n = n->ln_Pred;
        for (; n->ln_Succ; n = n->ln_Succ)
            printf("handler pri %4d %s\n", n->ln_Pri, n->ln_Name ? n->ln_Name : "(no name)");
        Permit();
    }
    Delay(atol(argv[2]) * 50);
    io->io_Command = IND_REMHANDLER;
    DoIO((struct IORequest *)io);
    printf("SIZEWINDOW events: %ld for the window, %ld in all; %ld events seen\n", (long)seen, (long)any, (long)total);
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
