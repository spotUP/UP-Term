#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <string.h>
#include "upreq.h"
#include "upactive.h"

struct MsgPort *upreq_open(char name[UPREQ_NAMELEN])
{
    struct MsgPort *mp = CreateMsgPort();
    if (!mp)
        return 0;
    upreq_portname((ULONG)mp, name);
    mp->mp_Node.ln_Name = name;
    mp->mp_Node.ln_Pri = 0;
    AddPort(mp);
    return mp;
}

upreq_msg *upreq_get(struct MsgPort *mp)
{
    return (upreq_msg *)GetMsg(mp);
}

void upreq_reply(upreq_msg *m, long rc)
{
    m->b.rc = rc;
    ReplyMsg(&m->msg);
}

void upreq_close(struct MsgPort *mp)
{
    upreq_msg *m;
    Forbid();
    RemPort(mp);
    Permit();
    while ((m = upreq_get(mp)))
        upreq_reply(m, UPREQ_NOPORT);
    DeleteMsgPort(mp);
}

long upreq_send(const upreq_body *r)
{
    upreq_msg m;
    char name[UPREQ_NAMELEN];
    struct MsgPort *rp, *tp;
    long rc;
    if (!upactive_prev(name))
        return UPREQ_NOPORT;
    rp = CreateMsgPort();
    if (!rp)
        return UPREQ_NOPORT;
    memset(&m, 0, sizeof m);
    m.msg.mn_Node.ln_Type = NT_MESSAGE;
    m.msg.mn_Length = sizeof m;
    m.msg.mn_ReplyPort = rp;
    m.b = *r;
    Forbid();
    tp = FindPort((STRPTR)name);
    if (tp)
        PutMsg(tp, &m.msg);
    Permit();
    if (!tp) {
        DeleteMsgPort(rp);
        return UPREQ_NOPORT;
    }
    WaitPort(rp);
    GetMsg(rp);
    rc = m.b.rc;
    DeleteMsgPort(rp);
    return rc;
}
