/* WBRESET-EXT: the Amiga side of wbreset_core (design: thoughts/shared/research/2026-10-31_wbreset-external-use.md).
 * Clients (the bar in Amarchy, amarchy-gui, amarchy-menu) report whether they hold a window on the Workbench
 * screen; the Amarchy commodity owns the record and the CloseWorkBench / OpenWorkBench patch (wbreset_patch.c). */
#ifndef AMARCHY_WBRESET_H
#define AMARCHY_WBRESET_H

#include <exec/semaphores.h>
#include "wbreset_core.h"

typedef struct wbr_pub {
    struct SignalSemaphore ss;      /* found by name (WBR_SEM_NAME); only Forbid guards the record */
    char name[20];
    wbr_rec rec;
} wbr_pub;

/* Register or update this task: sig is what the patch sends, mode WBR_HIDE / WBR_DISMISS, has_win 1 while a window
 * of this task is on the Workbench screen. No Amarchy running: nothing happens. */
void wbr_report(ULONG sig, int mode, int has_win);
/* The client's signal arrived: WBR_DO_HIDE / WBR_DO_SHOW / WBR_DO_NONE (wbr_client_step). */
int wbr_client_wake(void);
void wbr_client_drop(void);

/* Amarchy only. 0 = installed; the patch stays off (and Amarchy runs as before) on any error. */
int wbr_patch_install(void);
void wbr_patch_remove(void);

#endif
