/* MENU-TOGGLE registry, see menu_open.h. Same Forbid/FindPort/AddPort pattern as Amarchy's own ports. */
#include <exec/types.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <stdlib.h>
#include "menu_open.h"

static struct MsgPort *mo_port;

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
    atexit(mo_unregister);
}

int mo_close_open(void)
{
    struct MsgPort *p;
    int found = 0;
    Forbid();
    p = FindPort((STRPTR)MO_PORT);
    if (p && p->mp_SigTask) {
        Signal((struct Task *)p->mp_SigTask, SIGBREAKF_CTRL_C);
        found = 1;
    }
    Permit();
    return found;
}
