/* The public record of amarchy-flat ("Amarchy.Flat" semaphore), shared by the resident program (flat.c) and
 * whoever reads it (UP-Term Prefs: the status row of the Amarchy page). Only st is a stable, versioned layout
 * (flat_core.h); the rest is the program's own. */
#ifndef AMARCHY_FLAT_SHARED_H
#define AMARCHY_FLAT_SHARED_H

#include <exec/semaphores.h>
#include <exec/tasks.h>
#include <exec/libraries.h>
#include <stdint.h>
#include "flat_core.h"

/* The public record. ss is first so AddSemaphore's node is the allocation's start (as ixshared, ixwatch.h). */
typedef struct flatshared {
    struct SignalSemaphore ss;
    char name[16];
    struct Task *task;          /* the resident program; only compared, never dereferenced alone */
    char tname[32];
    flat_state st;
    flat_swap sw;
    uintptr_t *slot;            /* &sysiclass->cl_Dispatcher.h_SubEntry while installed */
    /* FLAT-C8: the frameiclass hook (second slot) and its sentinels */
    flat_swap fsw;
    uintptr_t *fslot;
    ULONG fcalls, fflat;        /* every frameiclass dispatch / every IM_DRAW(FRAME) that drew flat */
    /* FLAT-D4 */
    struct Library *dlib;       /* dopus5.library, opened while patched so it cannot expunge under the patch */
    flat_swap dsw[FLAT_EL_COUNT];
    uintptr_t *dslot[FLAT_EL_COUNT];
    unsigned dver, drev;
    int dreason, dmask, dfound;
    uint32_t dsum[FLAT_EL_COUNT]; /* flat_sum of each routine at the last scan (printed by STATUS) */
    ULONG dcalls, dflat;        /* every patched call / every call that drew flat (the sentinels) */
    /* FLAT-OUTER-EDGE: the graphics.library RectFill patch (LVO -306) that paints Intuition's outer border edges in pen_outline */
    flat_swap rsw;
    uintptr_t *rslot;
    uintptr_t irom_lo, irom_hi; /* Intuition's ROM code range (FindResident rt_MatchTag .. rt_EndSkip): the caller gate */
    ULONG rcalls, rhits;        /* every patched RectFill call / every outer-edge fill recoloured (the sentinels) */
    /* FLAT-BUTTON-FILL: the graphics.library Text patch (LVO -60) that draws a pressed button's label in pen_ptext */
    flat_swap tsw;
    uintptr_t *tslot;
    flat_tx tx;                 /* the armed pressed frame (flat_core.h) */
    ULONG tcalls, thits;        /* every patched Text call / every pressed label recoloured (the sentinels) */
} flatshared;

#endif
