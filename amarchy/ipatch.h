/* ipatch.h - one intuition.library jump-table patch (Amiga build only): the gate stub (wbr_gate_build) whose JMP
 * operand swap is the flat_swap model (restore only if ours). The gate counts every caller in its own memory before
 * it reads the hook word; the hook leaves with ipatch_leave. A gate is NEVER freed (a task can sit between the
 * library's JMP and the gate's first instruction for any time, unseen by any counter; a gate with hook word 0 passes
 * straight to the original forever). wbreset_patch.c (CloseWorkBench, OpenWorkBench), strut_patch.c (OpenWindowTagList,
 * OpenWindow) and borderblank_patch.c use it; the install / remove order of each caller is its own. */
#ifndef AMARCHY_IPATCH_H
#define AMARCHY_IPATCH_H

#include <stdint.h>
#include "flat_core.h"

typedef struct ipatch {
    unsigned char *gate;    /* the gate stub: hook word, saved target */
    unsigned char *entry;   /* the library's JMP at the LVO */
    uintptr_t *slot;        /* the JMP operand */
    flat_swap sw;
} ipatch;

/* Not under Forbid. 0, -2 (the vector is not a plain JMP) or -4 (no memory). */
int ipatch_alloc(ipatch *p, long lvo);
/* Under Forbid: build the gate towards hook and swap it in. FLAT_OK or a flat_swap_install error. The caller then
 * runs libpatch_done(library). */
int ipatch_arm(ipatch *p, void *hook);
/* Under Forbid: from now on the gate passes straight to the original (also for a gate left chained). */
void ipatch_passthru(ipatch *p);
/* Under Forbid: put the original back if still ours; FLAT_REMOVED, FLAT_STILL_CHAINED or FLAT_NOT_INSTALLED. */
int ipatch_unarm(ipatch *p);
/* The original routine behind the gate. */
uint32_t ipatch_saved(const ipatch *p);
/* Callers currently between the gate's first instruction and ipatch_leave (0 when forgotten). */
long ipatch_busy(const ipatch *p);
/* The last thing a hook does, on the ipatch it was entered through: uncounts the caller the gate counted. */
void ipatch_leave(ipatch *p);
/* After ipatch_busy of every gate is 0: drop the pointers. The gate memory stays allocated on purpose. */
void ipatch_forget(ipatch *p);

#endif
