/* The ACTION_WAIT_CHAR packets one side of a console or pty holds: one
 * per task, each with its own deadline. Portable C89, host-tested.
 *
 * ixemul's select sends a WAIT_CHAR per call. 48.2 kept one select packet
 * per struct file, so a handler held one at a time and a newer one ended
 * the older (it was stale). ixemul 80.x keeps a packet per process: two
 * processes that select one terminal each have one out, and ending the
 * other's made them take turns waking each other. So a newer WAIT_CHAR
 * ends only the older one of the same task (still how ixemul's close of a
 * file whose select is out returns at once), and the rest wait for input
 * or their own timeout. */
#ifndef WAITSET_H
#define WAITSET_H

#define WS_MAX 8

typedef struct ws_time {
    unsigned long s, us;
} ws_time;

typedef struct ws_waiter {
    void *pk;
    void *task;
    ws_time due;
} ws_waiter;

typedef struct waitset {
    ws_waiter w[WS_MAX];
    int n;
} waitset;

/* The waiter of task, taken out: its packet (to answer "no"), or 0. */
void *ws_drop_task(waitset *s, void *task);

/* pk from task waits until micros after now. 0 when it fits; when the set
 * is full, the packet of the oldest waiter, taken out to make room (to
 * answer "no"). Call ws_drop_task for the task first. */
void *ws_add(waitset *s, void *pk, void *task, ws_time now, unsigned long micros);

/* One waiter taken out, the oldest first: its packet, or 0 when none
 * (input arrived: answer each "yes"). */
void *ws_take(waitset *s);

/* One waiter whose time is up at now, taken out: its packet, or 0
 * (answer it "no"). */
void *ws_expired(waitset *s, ws_time now);

/* Microseconds from now to the earliest deadline (0 when it has passed),
 * -1 when no one waits: what the side's timer is set to. */
long ws_next(const waitset *s, ws_time now);

#endif
