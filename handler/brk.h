/* Who a console's break signals go to (Ctrl-C/D/E/F), shared by XCON:
 * and PTY:. The target is the last ACTION_CHANGE_SIGNAL port while its
 * process lives, else the process that opened the console: a program the
 * Shell runs may set itself and end without handing it back, and
 * signalling a task that is gone crashed the rig (#80000008). */
#ifndef BRK_H
#define BRK_H

#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/ports.h>

/* Is t a task that still exists (running, ready or waiting)? Call under
 * Forbid to use the answer. */
int task_alive(struct Task *t);

typedef struct brk {
    struct MsgPort *port;   /* ACTION_CHANGE_SIGNAL's */
    struct Task *owner;     /* port's task, read while it was alive */
    struct MsgPort *home;   /* the opener's: the target again when port's process ends */
} brk;

/* The opener: the target until a CHANGE_SIGNAL. */
void brk_open(brk *b, struct MsgPort *opener);

/* ACTION_CHANGE_SIGNAL (dp_Arg2; 0 leaves it). */
void brk_change(brk *b, struct MsgPort *port);

/* The target, or 0. Call under Forbid to use the answer. */
struct Task *brk_task(brk *b);

/* Signal the target with sig (SIGBREAKF_*); returns it, or 0. */
struct Task *brk_send(brk *b, ULONG sig);

#endif
