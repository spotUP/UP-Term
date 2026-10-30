/* IPrefs' idle state, shared by Amarchy's prefs apply (desk_dos.c, DESK-31) and the boot tool IPrefsWait (RIG-IPREFS-SYSREQ).
 * Split out of desk_dos.c unchanged so both link the one copy. */
#include <string.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "iprefs_wait.h"
#include "desk_core.h"

/* IPrefs' scheduling state (DESK-31): *have 0 when no task named "IPrefs" runs, else *waiting (TS_WAIT), its wait mask and
 * the signals it has received but not yet taken. Read under Disable(): a notification sets tc_SigRecvd from another task. */
static void dk_iprefs(int *have, int *waiting, unsigned long *sigwait, unsigned long *recvd)
{
    extern struct ExecBase *SysBase;
    struct List *lists[2];
    struct Node *n;
    int i;
    *have = *waiting = 0;
    *sigwait = *recvd = 0;
    Disable();
    lists[0] = &SysBase->TaskWait;
    lists[1] = &SysBase->TaskReady;
    for (i = 0; i < 2 && !*have; i++)
        for (n = lists[i]->lh_Head; n->ln_Succ; n = n->ln_Succ)
            if (n->ln_Name && strstr(n->ln_Name, "IPrefs")) {
                struct Task *t = (struct Task *)n;
                *have = 1;
                *waiting = t->tc_State == TS_WAIT;
                *sigwait = t->tc_SigWait;
                *recvd = t->tc_SigRecvd;
                break;
            }
    Enable();
}

/* The wait mask of IPrefs while it waits for a notification, taken before any editor runs (0: no IPrefs or never seen waiting) */
unsigned long desk_iprefs_idle_mask_for(int secs)
{
    int i, have, waiting;
    unsigned long w, r;
    for (i = 0; i < secs * 50 / DESK_IPREFS_POLL_TICKS; i++) {
        dk_iprefs(&have, &waiting, &w, &r);
        if (!have || waiting)
            return have ? w : 0;
        Delay(DESK_IPREFS_POLL_TICKS);
    }
    return 0;
}

/* Windows of ours back only once IPrefs has processed every file the editors wrote: a window opened inside its Workbench reset
 * gets the "close all windows" System Request (DESK-31, rig 7 Run 10: 3.1 Font USE, two resets 0.3-0.8 s after the editor).
 * The positive event is IPrefs back in its idle Wait() with nothing pending; DESK_IPREFS_SECS only bounds a wedged IPrefs. */
void desk_iprefs_wait_for(unsigned long idle_mask, int secs)
{
    int i, have, waiting;
    unsigned long w, r;
    for (i = 0; i < secs * 50 / DESK_IPREFS_POLL_TICKS; i++) {
        dk_iprefs(&have, &waiting, &w, &r);
        if (desk_iprefs_idle(have, waiting, w, r, idle_mask))
            break;
        Delay(DESK_IPREFS_POLL_TICKS);
    }
}

unsigned long desk_iprefs_idle_mask(void)
{
    return desk_iprefs_idle_mask_for(DESK_IPREFS_SECS);
}

void desk_iprefs_wait(unsigned long idle_mask)
{
    desk_iprefs_wait_for(idle_mask, DESK_IPREFS_SECS);
}
