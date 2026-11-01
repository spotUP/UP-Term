/* The shared poll timer (poll_timer.h); the code is tile.c's former timer_* functions. */
#include <proto/exec.h>
#include "poll_timer.h"

int pt_open(poll_timer *t)
{
    if (!(t->port = CreateMsgPort()))
        return 0;
    if (!(t->req = (struct timerequest *)CreateIORequest(t->port, sizeof(struct timerequest))))
        return 0;
    if (OpenDevice((STRPTR) "timer.device", UNIT_VBLANK, (struct IORequest *)t->req, 0))
        return 0;
    t->open = 1;
    return 1;
}

void pt_stop(poll_timer *t)
{
    if (t->busy) {
        AbortIO((struct IORequest *)t->req);
        WaitIO((struct IORequest *)t->req);
        t->busy = 0;
    }
}

void pt_start(poll_timer *t, unsigned long micros)
{
    pt_stop(t);
    if (!t->open)
        return;
    t->req->tr_node.io_Command = TR_ADDREQUEST;
    t->req->tr_time.tv_secs = micros / 1000000;
    t->req->tr_time.tv_micro = micros % 1000000;
    SendIO((struct IORequest *)t->req);
    t->busy = 1;
}

void pt_close(poll_timer *t)
{
    pt_stop(t);
    if (t->open)
        CloseDevice((struct IORequest *)t->req);
    t->open = 0;
    if (t->req)
        DeleteIORequest((struct IORequest *)t->req);
    t->req = 0;
    if (t->port)
        DeleteMsgPort(t->port);
    t->port = 0;
}

unsigned long pt_sigmask(const poll_timer *t)
{
    return t->port ? 1UL << t->port->mp_SigBit : 0;
}

int pt_fired(poll_timer *t, unsigned long sigs)
{
    if (t->busy && (sigs & pt_sigmask(t)) && CheckIO((struct IORequest *)t->req)) {
        WaitIO((struct IORequest *)t->req);
        t->busy = 0;
        return 1;
    }
    return 0;
}
