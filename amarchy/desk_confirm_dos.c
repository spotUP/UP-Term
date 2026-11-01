#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include "desk_confirm_dos.h"
#include "desk_core.h"
#include "desk_dos.h"
#include "sentinel_dos.h"
#include "tick.h"

unsigned long dc_shown, dc_keeps, dc_reverts_user, dc_reverts_timeout;

static desk_confirm st;
static struct Window *req;

static void publish(void)
{
    am_sentinel("AMARCHY_CONFIRM_SHOWN", (long)dc_shown);
    am_sentinel("AMARCHY_CONFIRM_KEEPS", (long)dc_keeps);
    am_sentinel("AMARCHY_DESK_PERSISTS", (long)desk_dos_persists);
    am_sentinel("AMARCHY_CONFIRM_REVERTS_USER", (long)dc_reverts_user);
    am_sentinel("AMARCHY_CONFIRM_REVERTS_TIMEOUT", (long)dc_reverts_timeout);
}

static void req_open(void)
{
    static struct EasyStruct es = {
        sizeof(struct EasyStruct), 0, (UBYTE *)"Amarchy desktop theme",
        (UBYTE *)"The desktop theme was applied.\n"
                  "Keep it, or it goes back to the previous look\n"
                  "after 15 seconds.\n"
                  "Some parts (colours 4 to 7) apply only after a\n"
                  "Workbench reset or a reboot.",
        (UBYTE *)"Keep|Revert" };
    if (req)
        return;
    if (IntuitionBase)        /* the commodity's own base (amarchy.c), open for its whole life */
        req = BuildEasyRequestArgs(0, &es, 0, 0);
}

static void req_close(void)
{
    if (req)
        FreeSysRequest(req);
    req = 0;
}

static void act(int verdict)
{
    if (verdict == DC_KEEP) {
        dc_keeps++;
        desk_dos_confirmed();
    } else if (verdict == DC_REVERT_USER || verdict == DC_REVERT_TIMEOUT) {
        if (verdict == DC_REVERT_USER)
            dc_reverts_user++;
        else
            dc_reverts_timeout++;
        req_close();           /* before the revert: IPrefs defers a palette reset while a window of ours is on the screen */
        desk_dos_revert();
    }
    if (verdict != DC_WAIT) {
        req_close();
        publish();
    }
}

void dc_begin(unsigned mask)
{
    desk_confirm_begin(&st, tick_now(), mask);
    if (!req) {
        dc_shown++;
        req_open();
        publish();
    }
    if (!req)                 /* no requester can be shown, so there is no way to ask: the apply stays */
        act(desk_confirm_answer(&st, 1));
}

static void dc_tick(long now)
{
    if (st.pending)
        act(desk_confirm_tick(&st, now));
}

unsigned long dc_sigmask(void)
{
    return req ? 1UL << req->UserPort->mp_SigBit : 0;
}

void dc_serve(void)
{
    LONG r;
    ULONG idcmp = 0;
    if (!req)
        return;
    r = SysReqHandler(req, &idcmp, FALSE);
    if (r == -2)               /* no answer yet */
        return;
    act(desk_confirm_answer(&st, r == 1));
}

void dc_init(void)
{
    desk_confirm_init(&st);
    desk_dos_confirm_begin = dc_begin;
    tick_hook(dc_tick);
}
