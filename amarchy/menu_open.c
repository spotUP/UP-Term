/* MENU-TOGGLE registry, see menu_open.h. Same Forbid/FindPort/AddPort pattern as Amarchy's own ports. */
#include <exec/types.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include "taskalive.h"
#include "atiming.h"
#include <stdlib.h>
#include "menu_open.h"

static struct MsgPort *mo_port;
static int mo_atexit;

void mo_unregister(void)
{
    struct MsgPort *p = mo_port;
    if (!p)
        return;
    Forbid();
    RemPort(p);
    mo_port = 0;
    Permit();
    DeleteMsgPort(p);
}

void mo_register(void)
{
    struct MsgPort *p;
    SetSignal(0, SIGBREAKF_CTRL_C);   /* a break left over from a shell must not close this menu at once */
    p = CreateMsgPort();
    if (!p)
        return;
    p->mp_Node.ln_Name = (char *)MO_PORT;
    Forbid();
    if (FindPort((STRPTR)MO_PORT)) {
        Permit();
        DeleteMsgPort(p);
        return;
    }
    AddPort(p);
    mo_port = p;
    Permit();
    if (!mo_atexit) {                 /* the warm server registers once per open: one hook only */
        mo_atexit = 1;
        atexit(mo_unregister);
    }
}

int mo_close_open(void)
{
    struct MsgPort *p;
    int found = 0, alive = 0, v;
    Forbid();
    p = FindPort((STRPTR)MO_PORT);
    if (p) {
        found = 1;
        /* no name check: the gui and the text menu started by the shell carry no fixed task name */
        alive = p->mp_SigTask && task_alive((struct Task *)p->mp_SigTask, "");
    }
    v = mo_verdict(found, alive);
    if (v == MO_SIGNAL)
        Signal((struct Task *)p->mp_SigTask, SIGBREAKF_CTRL_C);
    else if (v == MO_TAKE_OVER)
        RemPort(p);                 /* the owner is gone: its port must not swallow every later hotkey */
    Permit();
    return v == MO_SIGNAL;
}

static void srv_drain(mo_srv *s)
{
    struct Message *m;
    while (s->reply && (m = GetMsg(s->reply)) != 0)
        if (m == &s->msg.m)
            s->out = 0;
}

static void srv_find(void *ctx, int *found, int *alive, int *outstanding)
{
    mo_srv *s = (mo_srv *)ctx;
    struct MsgPort *p;
    srv_drain(s);
    Forbid();
    p = FindPort((STRPTR)s->port);
    *found = p != 0;
    at_note("AMARCHY_PORT_OWNER", p ? (unsigned long)p->mp_SigTask : 0);
    *alive = p && p->mp_SigTask && task_alive((struct Task *)p->mp_SigTask, s->task);
    Permit();
    *outstanding = s->out;
}

static void srv_take_over(void *ctx)
{
    mo_srv *s = (mo_srv *)ctx;
    struct MsgPort *p;
    Forbid();
    p = FindPort((STRPTR)s->port);
    if (p && !(p->mp_SigTask && task_alive((struct Task *)p->mp_SigTask, s->task)))
        RemPort(p);
    Permit();
}

static void srv_send(void *ctx, const ms_req *r)
{
    mo_srv *s = (mo_srv *)ctx;
    struct MsgPort *p;
    int sent = 0;
    memset(&s->msg, 0, sizeof s->msg);
    s->msg.m.mn_Node.ln_Type = NT_MESSAGE;
    s->msg.m.mn_Length = sizeof s->msg;
    s->msg.m.mn_ReplyPort = s->reply;
    s->msg.kind = r->kind;
    s->msg.anchored = r->anchored;
    s->msg.ax = r->ax;
    s->msg.ay = r->ay;
    strncpy(s->msg.spec, s->spec, MS_SPEC - 1);
    strncpy(s->msg.node, r->node ? r->node : "", MS_NODE - 1);
    Forbid();
    p = FindPort((STRPTR)s->port);
    sent = p && p->mp_SigTask && task_alive((struct Task *)p->mp_SigTask, s->task);
    Permit();
    if (sent)
        at_mark("send");            /* opt-in: AMARCHY_TIMING; only the warm path marks it (the fallback does not).
                                     * Written before the PutMsg: the server marks "start" the moment it has the
                                     * message and two Opens of the one file at once lose a mark. */
    sent = 0;
    Forbid();
    p = FindPort((STRPTR)s->port);
    if (p && p->mp_SigTask && task_alive((struct Task *)p->mp_SigTask, s->task)) {
        PutMsg(p, &s->msg.m);
        s->out = 1;
        sent = 1;
    }
    Permit();
    if (!sent)
        s->spawn(s->user, r);       /* the port went between find and send */
}

static void srv_spawn(void *ctx, const ms_req *r)
{
    mo_srv *s = (mo_srv *)ctx;
    s->spawn(s->user, r);
}

static void srv_start(void *ctx)
{
    mo_srv *s = (mo_srv *)ctx;
    s->start(s->user);
}

static long srv_now(void *ctx)
{
    struct DateStamp ds;
    (void)ctx;
    DateStamp(&ds);
    return ds.ds_Days * 86400L + ds.ds_Minute * 60L + ds.ds_Tick / 50;
}

static long srv_stamp(void *ctx)
{
    mo_srv *s = (mo_srv *)ctx;
    return s->stamp ? s->stamp(s->user) : 0;
}

static int srv_quit(void *ctx)
{
    return mo_srv_quit((mo_srv *)ctx);
}

void mo_srv_ops(ms_ops *ops, mo_srv *s)
{
    ops->ctx = s;
    ops->find = srv_find;
    ops->take_over = srv_take_over;
    ops->send = srv_send;
    ops->spawn = srv_spawn;
    ops->start_server = srv_start;
    ops->now = srv_now;
    ops->stamp = srv_stamp;
    ops->quit = srv_quit;
    ops->st = &s->st;
}

int mo_srv_quit(mo_srv *s)
{
    struct MsgPort *p;
    int sent = 0, i;
    srv_drain(s);
    if (s->out)
        return 0;                   /* wedged: nothing to wait for */
    memset(&s->msg, 0, sizeof s->msg);
    s->msg.m.mn_Node.ln_Type = NT_MESSAGE;
    s->msg.m.mn_Length = sizeof s->msg;
    s->msg.m.mn_ReplyPort = s->reply;
    s->msg.kind = MS_QUIT;
    Forbid();
    p = FindPort((STRPTR)s->port);
    if (p && p->mp_SigTask && task_alive((struct Task *)p->mp_SigTask, s->task)) {
        PutMsg(p, &s->msg.m);
        s->out = 1;
        sent = 1;
    }
    Permit();
    for (i = 0; sent && i < 20; i++) {
        srv_drain(s);
        if (!s->out)
            return 1;
        Delay(5);
    }
    /* Timed out: the server may still hold s->msg (it replies at once, but a server busy in an open has not
     * reached its GetMsg yet). s->msg lives in the static mo_srv and is never freed, and the caller must
     * leave s->reply allocated too (s->out stays 1): freeing either now would let the server reply into
     * freed memory. A leaked port at process exit is harmless; a use-after-free is not. */
    return 0;
}
