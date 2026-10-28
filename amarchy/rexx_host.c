/* rexx_host: see rexx_host.h. */
#include <string.h>
#include <exec/types.h>
#include <exec/ports.h>
#include <rexx/storage.h>
#include <rexx/rxslib.h>
#include <proto/exec.h>
#include <proto/rexxsyslib.h>
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
    if (h->lib) {
        CloseLibrary(h->lib);
        h->lib = 0;
    }
}

long rexx_send(const char *portname, const char *line, char *result, int cap)
{
    struct MsgPort *reply, *port;
    struct RexxMsg *rm;
    struct Library *rxs;
    long rc = -1;
    if (result && cap > 0)
        result[0] = 0;
    rxs = OpenLibrary((STRPTR)"rexxsyslib.library", 0);
    reply = rxs ? CreateMsgPort() : 0;
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
                if (port) {
                    WaitPort(reply);
                    GetMsg(reply);
                    rc = rm->rm_Result1;
                    if (rc == 0 && result && rm->rm_Result2) {
                        strncpy(result, (const char *)rm->rm_Result2, cap - 1);
                        result[cap - 1] = 0;
                        DeleteArgstring((STRPTR)rm->rm_Result2);
                    }
                }
                DeleteArgstring((STRPTR)rm->rm_Args[0]);
            }
            DeleteRexxMsg(rm);
        }
        DeleteMsgPort(reply);
    }
    if (rxs)
        CloseLibrary(rxs);
    return rc;
}
