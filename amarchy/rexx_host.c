/* rexx_host: see rexx_host.h. */
#include <string.h>
#include <exec/types.h>
#include <exec/ports.h>
#include <rexx/storage.h>
#include <rexx/rxslib.h>
#include <proto/exec.h>
#include <exec/memory.h>
#include <proto/rexxsyslib.h>
#include "poll_timer.h"
#include "rexx_host.h"

struct RxsLib *RexxSysBase;   /* the proto/rexxsyslib.h stubs read this */

int rexx_port_exists(const char *name)
{
    struct MsgPort *p;
    Forbid();
    p = FindPort((STRPTR)name);
    Permit();
    return p != 0;
}

int rexx_host_open(struct rexx_host *h, const char *name)
{
    h->port = 0;
    h->lib = OpenLibrary((STRPTR)"rexxsyslib.library", 0);
    RexxSysBase = (struct RxsLib *)h->lib;
    Forbid();
    h->port = FindPort((STRPTR)name) ? 0 : CreateMsgPort();
    if (h->port) {
        h->port->mp_Node.ln_Name = (char *)name;
        AddPort(h->port);
    }
    Permit();
    return h->port != 0;
}

unsigned long rexx_host_sigmask(const struct rexx_host *h)
{
    return h->port ? 1UL << h->port->mp_SigBit : 0;
}

void rexx_host_serve(struct rexx_host *h, rexx_host_fn fn, void *ud)
{
    struct RexxMsg *m;
    if (!h->port)
        return;
    while ((m = (struct RexxMsg *)GetMsg(h->port)) != 0) {
        char out[REXX_HOST_OUT];
        long rc;
        out[0] = 0;
        rc = fn(ud, (const char *)m->rm_Args[0], out);
        m->rm_Result1 = rc;
        m->rm_Result2 = 0;
        if (out[0] && (m->rm_Action & RXFF_RESULT) && RexxSysBase)
            m->rm_Result2 = (long)CreateArgstring((STRPTR)out, strlen(out));
        ReplyMsg((struct Message *)m);
    }
}

void rexx_host_close(struct rexx_host *h, rexx_host_fn fn, void *ud)
{
    if (h->port) {
        RemPort(h->port);
        rexx_host_serve(h, fn, ud);
        DeleteMsgPort(h->port);
        h->port = 0;
    }
    rexx_send_reap();
    if (h->lib) {
        CloseLibrary(h->lib);
        h->lib = 0;
    }
}

/* The reply wait is bounded: the main process of Amarchy sends to foreign ports (DOPUS.1 on a theme switch, AMTILE)
 * from the loop that also serves its own port, so a peer that never replies must not freeze it (REXX-DEAD-AFTER-THEME).
 * A sent RexxMsg cannot be recalled, so on a timeout the message and its reply port are parked: the port is made by
 * hand (not CreateMsgPort) so its signal can be freed and the port turned PA_IGNORE, and a late ReplyMsg only queues
 * the message there. rexx_send_reap frees the parked ones that came back; one that never does stays allocated. */
unsigned long rexx_send_timeouts;   /* replies not in within REXX_SEND_SECS: the sentinel of the bound */

#define RX_PARKED 8
static struct rx_parked {
    struct MsgPort *reply;
    struct RexxMsg *rm;
    struct Library *rxs;
} rx_parked[RX_PARKED];

static struct MsgPort *rx_port_new(void)
{
    struct MsgPort *p = (struct MsgPort *)AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC | MEMF_CLEAR);
    BYTE sig;
    if (!p)
        return 0;
    if ((sig = AllocSignal(-1)) == -1) {
        FreeMem(p, sizeof(struct MsgPort));
        return 0;
    }
    p->mp_Node.ln_Type = NT_MSGPORT;
    p->mp_Flags = PA_SIGNAL;
    p->mp_SigBit = (UBYTE)sig;
    p->mp_SigTask = FindTask(0);
    p->mp_MsgList.lh_Head = (struct Node *)&p->mp_MsgList.lh_Tail;
    p->mp_MsgList.lh_Tail = 0;
    p->mp_MsgList.lh_TailPred = (struct Node *)&p->mp_MsgList.lh_Head;
    return p;
}

static void rx_port_free(struct MsgPort *p)
{
    if (p->mp_Flags == PA_SIGNAL)
        FreeSignal(p->mp_SigBit);
    FreeMem(p, sizeof(struct MsgPort));
}

/* The reply is in: free its result string, the command string, the message (through the library it was made
 * with), then the library and the port. */
static void rx_done(struct MsgPort *reply, struct RexxMsg *rm, struct Library *rxs, char *result, int cap)
{
    struct RxsLib *keep = RexxSysBase;
    RexxSysBase = (struct RxsLib *)rxs;
    if (rm->rm_Result1 == 0 && (rm->rm_Action & RXFF_RESULT) && rm->rm_Result2) {
        if (result && cap > 0) {
            strncpy(result, (const char *)rm->rm_Result2, cap - 1);
            result[cap - 1] = 0;
        }
        DeleteArgstring((STRPTR)rm->rm_Result2);
    }
    DeleteArgstring((STRPTR)rm->rm_Args[0]);
    DeleteRexxMsg(rm);
    RexxSysBase = keep;
    CloseLibrary(rxs);
    rx_port_free(reply);
}

void rexx_send_reap(void)
{
    int i;
    for (i = 0; i < RX_PARKED; i++)
        if (rx_parked[i].reply && GetMsg(rx_parked[i].reply)) {
            rx_done(rx_parked[i].reply, rx_parked[i].rm, rx_parked[i].rxs, 0, 0);
            rx_parked[i].reply = 0;
        }
}

/* No reply in time: stop the port signalling, give its bit back, and park it; a full table leaks the slot's memory
 * (still safe: the port stays valid for the late reply). */
static void rx_park(struct MsgPort *reply, struct RexxMsg *rm, struct Library *rxs)
{
    int i;
    Forbid();
    reply->mp_Flags = PA_IGNORE;
    Permit();
    FreeSignal(reply->mp_SigBit);
    rexx_send_timeouts++;
    if (GetMsg(reply)) {   /* it came in between the timeout and PA_IGNORE */
        rx_done(reply, rm, rxs, 0, 0);
        return;
    }
    for (i = 0; i < RX_PARKED; i++)
        if (!rx_parked[i].reply) {
            rx_parked[i].reply = reply;
            rx_parked[i].rm = rm;
            rx_parked[i].rxs = rxs;
            return;
        }
}

long rexx_send(const char *portname, const char *line, char *result, int cap)
{
    struct MsgPort *reply, *port = 0;
    struct RexxMsg *rm = 0;
    struct Library *rxs;
    struct RxsLib *keep = RexxSysBase;   /* a host in this process keeps its own base */
    poll_timer t = { 0 };
    long rc = -1;
    if (result && cap > 0)
        result[0] = 0;
    rexx_send_reap();
    rxs = OpenLibrary((STRPTR)"rexxsyslib.library", 0);
    reply = rxs ? rx_port_new() : 0;
    if (reply) {
        RexxSysBase = (struct RxsLib *)rxs;
        if ((rm = CreateRexxMsg(reply, 0, 0)) != 0) {
            if ((rm->rm_Args[0] = (STRPTR)CreateArgstring((STRPTR)line, strlen(line))) != 0) {
                rm->rm_Action = RXCOMM | (result ? RXFF_RESULT : 0);
                Forbid();
                port = FindPort((STRPTR)portname);
                if (port)
                    PutMsg(port, (struct Message *)rm);
                Permit();
            }
        }
        RexxSysBase = keep;
    }
    if (port) {
        unsigned long rbit = 1UL << reply->mp_SigBit, tmask = 0;
        int in = 0;
        if (pt_open(&t)) {
            pt_start(&t, REXX_SEND_SECS * 1000000UL);
            tmask = pt_sigmask(&t);
        }
        for (;;) {
            unsigned long sigs;
            if (GetMsg(reply)) {
                in = 1;
                break;
            }
            sigs = Wait(rbit | tmask);
            if (!(sigs & rbit) && tmask && pt_fired(&t, sigs))
                break;
        }
        pt_close(&t);
        if (in) {
            rc = rm->rm_Result1;
            rx_done(reply, rm, rxs, result, cap);
        } else
            rx_park(reply, rm, rxs);
        return rc;
    }
    /* not sent: free what was made */
    if (rm) {
        RexxSysBase = (struct RxsLib *)rxs;
        if (rm->rm_Args[0])
            DeleteArgstring((STRPTR)rm->rm_Args[0]);
        DeleteRexxMsg(rm);
        RexxSysBase = keep;
    }
    if (reply)
        rx_port_free(reply);
    if (rxs)
        CloseLibrary(rxs);
    return rc;
}
