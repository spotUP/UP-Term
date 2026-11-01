/* ipatch.h - one intuition.library jump-table patch (Amiga build only): the gate stub (wbr_gate_build) whose JMP
 * operand swap is the flat_swap model (restore only if ours, leak the gate when a foreign patcher chained on it).
 * wbreset_patch.c (CloseWorkBench, OpenWorkBench) and strut_patch.c (OpenWindowTagList, OpenWindow) use it; the
 * install / remove order of each caller is its own. */
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
/* After no caller can be inside: free the gate unless rc (ipatch_unarm) says it is still chained. */
void ipatch_free(ipatch *p, int rc);

#endif
