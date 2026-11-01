/* iconpen_dos: the OS half of the shared icon pen holder (plan 2026-11-01-amarchy-filled-icon-set.md F14, H1).
 * One public record ("Amarchy.IconPens" semaphore, ip_shared) lives in memory for as long as an owner wants the 16 icon pens.
 * ObtainPen pens belong to the screen's colour map, not to a task, so the record outlives the amarchy-icons process that
 * created it; amarchy-flat's 1 s tick re-obtains them when the Workbench screen is reopened. The decisions are
 * iconpen_core.c (host-tested). The program defines struct IntuitionBase *IntuitionBase and struct GfxBase *GfxBase and opens
 * both before calling ipd_owner. */
#ifndef ICONPEN_DOS_H
#define ICONPEN_DOS_H

#include <stddef.h>
#include "iconpen_core.h"

#define IPD_SEM_NAME "Amarchy.IconPens"

/* Add (on 1) or drop (on 0) `owner`, then bring the held pens in line with the owners and the Workbench screen as it is now.
 * Returns the reference count after. The record is freed with the last owner. */
int ipd_owner(unsigned owner, int on);
/* Re-sync without changing owners (screen reopened, theme changed). */
void ipd_resync(void);
/* A theme switch: rewrite the held pens 32..47 with SetRGB32 from the new theme (ip_recolour). Does nothing when no record exists. */
void ipd_recolour(void);
/* "pens-held N refs=M" from the record; no record reads "pens-held 0 refs=0". Needs no library. Returns the length. */
int ipd_status_line(char *out, size_t cap);

#endif
