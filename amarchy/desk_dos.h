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
/* FLAT-PREFS-NOT-NOTIFIED: the prefs file changed: sync the running amarchy-flat (write + redraw only on a change), or start it (BOOT) */
void desk_dos_flat_sync(void);
/* R1.17: IPrefs resets the Workbench screen after a Palette/WBPattern/Pointer/Font USE and cannot while any window of ours is on it
 * ("Please close the following windows: Amarchy bar"). A Palette USE resets it only when the live DrawInfo pens differ from the chosen pen map (desk_pens_for, desk_pens_reset); then the apply closes the windows first and reopens them when the new screen is up.
 * The main process installs direct hooks (it cannot send ARexx to itself); every other process leaves them 0 and the apply sends BAR STATUS, and BAR OFF / BAR ON when the bar was open. */
extern int (*desk_dos_wb_close)(void);   /* returns 1 when the bar was open */
extern void (*desk_dos_wb_open)(int was); /* reopens it when was */
/* how many times the apply closed the windows around a Workbench reset (the reachability sentinel of the fix) */
extern unsigned long desk_dos_wb_closes;
/* the process's OWN windows besides the bar (the drawn menu in amarchy-gui): hidden before the first ENV: write, shown again after IPrefs applied; any window of ours on the Workbench screen makes IPrefs defer the palette */
extern int (*desk_dos_own_close)(void);
extern void (*desk_dos_own_open)(void);
/* how many times desk_dos_apply ran its writes (the reachability sentinel) and the mask of the last run */
extern unsigned long desk_dos_live_pens;       /* FLAT-PEN5-MAGENTA: live sets of the role pens no palette.prefs entry reaches (dk_live_pens) */
extern unsigned long desk_dos_flat_pens_writes; /* FLAT-PENS-LIVE: palette pen writes made by flat-system-look itself */
extern unsigned long desk_dos_wbborder_writes; /* AM-ICON-NOBORDER: Workbench editor writes of the icon border */
extern unsigned long desk_dos_wbgauge_writes;  /* WB-VOLUME-GAUGE-OFF: Workbench editor writes that changed the volume gauge */
extern unsigned long desk_dos_flat_writes; /* FLAT-C9: times desk_dos_apply wrote the flat_state block of a running amarchy-flat */
extern unsigned long desk_dos_dopus_sends; /* FLAT-D2: times desk_dos_apply handed the theme to dopus_theme_send */
extern unsigned long desk_dos_applies, desk_dos_last_mask;

/* FP-1 (R1.8): after an apply that changed something the user has 15 s to Keep it. The commodity installs the begin hook (it owns the
 * requester and the timer); every other process sends the ARexx verb DESK CONFIRM <mask> to it. */
extern void (*desk_dos_confirm_begin)(unsigned mask);
/* Keep: the hook FP-2 fills to persist to ENVARC:; now it clears the pending state */
void desk_dos_confirmed(void);
/* Revert (button or timeout): the snapshot in T:amarchy-revert/ goes back through the stock editors */
void desk_dos_revert(void);
/* FP-3 (R1.9): at Amarchy start, reverts an apply that never reached Keep or Revert (marker in ENVARC:Amarchy). 1 when it did. */
int desk_dos_marker_recover(void);
extern unsigned long desk_dos_marker_reverts;
/* counters (the reachability sentinels): applies that asked, keeps, reverts */
extern unsigned long desk_dos_confirm_asks, desk_dos_keeps, desk_dos_reverts;
/* FP-2: files Keep copied to ENVARC:, originals saved before a first write, originals put back by "off" */
extern unsigned long desk_dos_persists, desk_dos_originals_saved, desk_dos_originals_restored, desk_dos_originals_dropped;
/* DESK-STALE-ORIG: ENV: fix actions repeated after a refused one (IPrefs still reading), fixes that never landed, the last refused open's IoErr */
extern unsigned long desk_dos_env_fix_retries, desk_dos_env_fix_fails, desk_dos_env_fix_err;

#endif
