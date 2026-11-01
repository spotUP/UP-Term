/* A repeating poll on timer.device UNIT_VBLANK, shared by AmTile (tile.c) and the Amarchy bar (bar.c): one request,
 * re-armed by the owner after each tick, its signal joins the owner's Wait. Amiga only. */
#ifndef AMARCHY_POLL_TIMER_H
#define AMARCHY_POLL_TIMER_H

#include <exec/types.h>
#include <exec/ports.h>
#include <devices/timer.h>

typedef struct poll_timer {
    struct MsgPort *port;
    struct timerequest *req;
    int open, busy;
} poll_timer;

/* 1 when the device is open; on 0 call pt_close. */
int pt_open(poll_timer *t);
void pt_close(poll_timer *t);
/* Arms one request (an armed one is aborted first). */
void pt_start(poll_timer *t, unsigned long micros);
void pt_stop(poll_timer *t);
unsigned long pt_sigmask(const poll_timer *t);
/* 1 when `sigs` carries the timer's bit and the request finished (the bit can be left over from an AbortIO: only a
 * finished request is a tick); the request is then idle and the owner re-arms it. */
int pt_fired(poll_timer *t, unsigned long sigs);

#endif
