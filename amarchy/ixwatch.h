/* The OS side of watching amarchy-index (RESCAN-FEEDBACK): finds the public progress record the scanner
 * keeps (ix_prog, amarchy/index_core.h) and copies it out. amarchy-index (--where), amarchy-menu and
 * amarchy-gui share it; what a copy means (the row text, the state) is ix_watch_row in index_core.c. */
#ifndef IXWATCH_H
#define IXWATCH_H

#include <exec/semaphores.h>
#include "index_core.h"

/* The public record. ss is first so AddSemaphore's node is the allocation's start; the scanner never
 * takes the semaphore (a task that died holding it would block a reader), it only names the record for
 * FindSemaphore. */
typedef struct ixshared {
    struct SignalSemaphore ss;
    char name[16];
    struct Task *task;          /* the scanner of the last run; only compared, never dereferenced alone */
    char tname[32];
    ix_prog p;
} ixshared;

/* The record of this layout, or 0 when no scan has run since the machine started. */
ixshared *ixw_find(void);
/* A copy of the record and whether its scanner task still exists; 0 (out untouched) when no record. */
int ixw_snapshot(ix_prog *out, int *alive);
/* 1 when only a record of an older layout (ix_prog_old_names) is in memory. */
int ixw_other_layout(void);

#endif
