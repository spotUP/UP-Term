/* desk_dos: the DOS side of the Amarchy native desktop theme (desk_core.h, plan 2026-10-29-amarchy-p1a-native-theme.md,
 * R1.7): writes the Workbench Palette, WBPattern, Pointer and Font preference files for a theme and applies them live
 * through the stock editors (Palette|WBPattern|Pointer|Font FROM file USE). Linked wherever gthemes_dos.c is. */
#ifndef DESK_DOS_H
#define DESK_DOS_H

#include "gthemes_core.h"

/* Apply th to the desktop for every part the amconf switches enable (amconf_desk_mask; 0 = master off, nothing is read
 * or written). The old files are copied to T:amarchy-revert/ first. Returns the mask applied, 0 when off, -1 when the
 * theme is refused (desk_check; why gets the text) or a write failed. ENVARC: is never written (persist is R1.10). */
int desk_dos_apply(const gtheme *th, char *why, int cap);
/* how many times desk_dos_apply ran its writes (the reachability sentinel) and the mask of the last run */
extern unsigned long desk_dos_applies, desk_dos_last_mask;

#endif
