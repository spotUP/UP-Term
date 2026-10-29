/* W43 (amarchy/wbmode_core.h): the Amiga half. Amarchy reads ENV:Sys/screenmode.prefs once, then asks every tick
 * whether the Workbench screen already runs that mode. Amiga only. */
#ifndef AMARCHY_WBMODE_H
#define AMARCHY_WBMODE_H

#include "wbmode_core.h"

/* Reads the prefs file (none = nothing to wait for). Call once at start. */
void wbm_begin(void);
/* WBM_WAIT / WBM_READY / WBM_TIMEOUT for elapsed_ticks 1 s ticks after wbm_begin. The Workbench screen is locked
 * only for the length of the call: a held lock would block the very reset being waited for. */
int wbm_check(long elapsed_ticks);

#endif
