/* WBRESET-EXT client side, see wbreset.h. Every access finds the record again under Forbid, so a client never keeps
 * a pointer into a record that Amarchy freed, and a restarted Amarchy is found on the next update. */
#include <exec/types.h>
#include <proto/exec.h>
#include "wbreset.h"

static wbr_rec *rec_find(void)
{
    struct SignalSemaphore *ss = FindSemaphore((STRPTR)WBR_SEM_NAME);
    return ss ? &((wbr_pub *)ss)->rec : 0;
}

void wbr_report(ULONG sig, int mode, int has_win)
{
    wbr_rec *r;
    Forbid();
    if ((r = rec_find()) != 0)
        wbr_set(r, FindTask(0), sig, mode, has_win);
    Permit();
}

int wbr_client_wake(void)
{
    wbr_rec *r;
    int d = WBR_DO_NONE;
    Forbid();
    if ((r = rec_find()) != 0)
        d = wbr_client_step(r, FindTask(0));
    Permit();
    return d;
}

void wbr_client_drop(void)
{
    wbr_rec *r;
    Forbid();
    if ((r = rec_find()) != 0)
        wbr_drop(r, FindTask(0));
    Permit();
}
