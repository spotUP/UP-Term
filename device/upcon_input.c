/* upcon_input.c -- the device's input handler (plan
 * 2026-09-30-console-device.md, DD6, DD7, D1.4): priority 5, above the ROM
 * console's (0, which passes nothing on: DP1) and below Intuition (50) and
 * ixemul (10). For each event meant for one of our units (upc_route) it
 * copies the event into that unit's ring and signals the unit; the chain
 * goes on unchanged. It runs in input.device's task: it never waits and
 * never allocates. Added with the first unit, removed with the last. */
#include <exec/memory.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include "upcon.h"

extern struct IntuitionBase *IntuitionBase;

static struct InputEvent *upc_ih(__reg("a0") struct InputEvent *chain, __reg("a1") APTR data)
{
    struct upc_base *b = (struct upc_base *)data;
    const void *wins[UPC_MAXUNITS];
    struct InputEvent *ev;
    const void *active = IntuitionBase->ActiveWindow;
    int i;
    for (i = 0; i < UPC_MAXUNITS; i++)
        wins[i] = b->units[i] ? b->units[i]->win : 0;
    for (ev = chain; ev; ev = ev->ie_NextEvent) {
        struct upc_unit *u;
        upc_event e;
        i = upc_route(ev->ie_Class, ev->ie_EventAddress, active, wins, UPC_MAXUNITS);
        if (i < 0 || !(u = b->units[i]))
            continue;
        e.cls = ev->ie_Class;
        e.subclass = ev->ie_SubClass;
        e.code = ev->ie_Code;
        e.qual = ev->ie_Qualifier;
        e.x = ev->ie_X;
        e.y = ev->ie_Y;
        e.addr = ev->ie_EventAddress;
        e.secs = ev->ie_TimeStamp.tv_secs;
        e.micros = ev->ie_TimeStamp.tv_micro;
        if (upc_ev_push(&u->ring, &e))
            Signal(u->task, u->ev_sig);
        else
            b->dropped++;
    }
    return chain;
}

static int handler_io(struct upc_base *b, UWORD cmd)
{
    struct MsgPort *p = CreateMsgPort();
    struct IOStdReq *io;
    int ok = 0;
    if (!p)
        return 0;
    io = (struct IOStdReq *)CreateIORequest(p, sizeof(struct IOStdReq));
    if (io && !OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)io, 0)) {
        io->io_Command = cmd;
        io->io_Data = &b->ih;
        ok = DoIO((struct IORequest *)io) == 0;
        CloseDevice((struct IORequest *)io);
    }
    if (io)
        DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(p);
    return ok;
}

int upc_input_add(struct upc_base *b)
{
    if (b->ih_added)
        return 1;
    b->ih.is_Code = (void (*)())upc_ih;
    b->ih.is_Data = b;
    b->ih.is_Node.ln_Pri = 5;
    b->ih.is_Node.ln_Name = (char *)"UP-Term console";
    b->ih.is_Node.ln_Type = NT_INTERRUPT;
    b->ih_added = handler_io(b, IND_ADDHANDLER);
    return b->ih_added;
}

void upc_input_rem(struct upc_base *b)
{
    if (!b->ih_added)
        return;
    handler_io(b, IND_REMHANDLER);
    b->ih_added = 0;
}
