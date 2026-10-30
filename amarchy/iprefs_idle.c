#include "desk_core.h"

/* IPrefs is idle: pure, shared by Amarchy's prefs apply and the boot tool IPrefsWait (no other desk_core.c dependency) */
int desk_iprefs_idle(int have, int waiting, unsigned long sigwait, unsigned long recvd, unsigned long idle_mask)
{
    if (!have)
        return 1;
    return waiting && idle_mask != 0 && sigwait == idle_mask && (recvd & idle_mask) == 0;
}
