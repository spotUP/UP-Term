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
#include "wbreset_core.h"

unsigned long dc_shown, dc_keeps, dc_reverts_user, dc_reverts_timeout;

static desk_confirm st;
static struct Window *req;
static struct Window *rreq;   /* TE-7: the "Reset Workbench Now / Later" requester (no timer, no revert) */
unsigned long dc_reset_shown, dc_reset_nows;

static void publish(void)
{
    am_sentinel("AMARCHY_CONFIRM_SHOWN", (long)dc_shown);
    am_sentinel("AMARCHY_CONFIRM_KEEPS", (long)dc_keeps);
    am_sentinel("AMARCHY_DESK_PERSISTS", (long)desk_dos_persists);
    am_sentinel("AMARCHY_DESK_ORIG_DROPPED", (long)desk_dos_originals_dropped);
    am_sentinel("AMARCHY_DESK_ENV_FIX_RETRIES", (long)desk_dos_env_fix_retries);
    am_sentinel("AMARCHY_DESK_ENV_FIX_FAILS", (long)desk_dos_env_fix_fails);
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

/* TE-7.1 (D1): Save changed a reset-bound field; the files are written, Workbench only needs the reset. Later keeps the files. */
static void rreq_open(void)
{
    static struct EasyStruct es = {
        sizeof(struct EasyStruct), 0, (UBYTE *)"Amarchy theme",
        (UBYTE *)"The theme was saved.\n"
                  "Fonts, title bar height and gadget size\n"
                  "apply after a Workbench reset.\n"
                  "Reset Workbench now? It closes open windows.",
        (UBYTE *)"Reset Workbench Now|Later" };
    if (!rreq && IntuitionBase)
        rreq = BuildEasyRequestArgs(0, &es, 0, 0);
    if (rreq) {
        dc_reset_shown++;
        am_sentinel("AMARCHY_CONFIRM_RESET_SHOWN", (long)dc_reset_shown);
    }
}

void dc_begin(unsigned mask)
{
    if (mask & WBR_ASK_MASK) {
        rreq_open();
        return;
    }
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
    return (req ? 1UL << req->UserPort->mp_SigBit : 0) | (rreq ? 1UL << rreq->UserPort->mp_SigBit : 0);
}

void dc_serve(void)
{
    LONG r;
    ULONG idcmp = 0;
    if (rreq) {
        r = SysReqHandler(rreq, &idcmp, FALSE);
        if (r != -2) {
            FreeSysRequest(rreq);
            rreq = 0;
            if (r == 1) {          /* Reset Workbench Now: the patched CloseWorkBench hides the clients first (wbreset.c) */
                dc_reset_nows++;
                am_sentinel("AMARCHY_CONFIRM_RESET_NOWS", (long)dc_reset_nows);
                if (CloseWorkBench())
                    OpenWorkBench();
            }
        }
    }
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
