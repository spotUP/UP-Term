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
/* R1.17: IPrefs resets the Workbench screen after a Palette/WBPattern/Pointer/Font USE and cannot while any window of ours is on it
 * ("Please close the following windows: Amarchy bar"). A Palette USE resets it only when the live DrawInfo pens differ from desk_pens (desk_pens_reset); then the apply closes the windows first and reopens them when the new screen is up.
 * The main process installs direct hooks (it cannot send ARexx to itself); every other process leaves them 0 and the apply sends BAR STATUS, and BAR OFF / BAR ON when the bar was open. */
extern int (*desk_dos_wb_close)(void);   /* returns 1 when the bar was open */
extern void (*desk_dos_wb_open)(int was); /* reopens it when was */
/* how many times the apply closed the windows around a Workbench reset (the reachability sentinel of the fix) */
extern unsigned long desk_dos_wb_closes;
/* the process's OWN windows besides the bar (the drawn menu in amarchy-gui): hidden before the first ENV: write, shown again after IPrefs applied; any window of ours on the Workbench screen makes IPrefs defer the palette */
extern int (*desk_dos_own_close)(void);
extern void (*desk_dos_own_open)(void);
/* how many times desk_dos_apply ran its writes (the reachability sentinel) and the mask of the last run */
extern unsigned long desk_dos_dopus_pens; /* FLAT-D2: pen lines the DOpus port accepted */
extern unsigned long desk_dos_applies, desk_dos_last_mask;

#endif
