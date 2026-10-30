/* gthemes_dos: the DOS side of the Omarchy GUI themes (gthemes_core.h): the drawer scan, the file read, the
 * saved choice (AM_GTHEME in Amarchy's settings file), and the Style > Theme rows. Linked by the commodity
 * (act_gtheme), amarchy-gui, amarchy-menu and Prefs. */
#ifndef GTHEMES_DOS_H
#define GTHEMES_DOS_H

#include "gthemes_core.h"
#include "gthemes_scan_dos.h"
#include "index_core.h"

/* the current theme's name: AM_GTHEME of the settings file, GT_DEFAULT when unset */
void gt_dos_current(char *out, int cap);
/* load the theme the settings name (user drawer, shipped drawer, built-in: gt_load) */
void gt_dos_load_current(gtheme *out);
/* load a theme by name */
void gt_dos_load(const char *name, gtheme *out);
/* make name the drawn menu's theme: AM_GTHEME is saved to ENVARC: and ENV:, then Directory Opus (if running) gets its colours (menu_theme.h). 1 saved, 0 refused or failed */
/* DESK-PREVIEW-PALETTE: gt_dos_term_send plus the Workbench colours of the theme on the Workbench screen (the saved theme's name
 * puts the colours back). The pickers' preview hook. gt_dos_desk_restore puts back what the preview overwrote (no-op when none);
 * counters: previews written / restores done (reachability sentinels). */
void gt_dos_preview_send(const char *name);
void gt_dos_desk_restore(void);
extern unsigned long gt_dos_desk_previews, gt_dos_desk_restores;
void gt_dos_term_send(const char *name); /* the console tabs' colours: GT_TERM_FILE (gthemes_core.h) */
extern unsigned long gt_dos_term_writes;
/* Amarchy startup: write GT_TERM_FILE from the configured theme (GT_DEFAULT when none), so consoles opened before any switch start in it */
void gt_dos_term_boot(void);

int gt_dos_select(const char *name, char *why, int cap);
/* how many times gt_dos_select handed the new theme to Directory Opus (dopus_theme_send): the reachability sentinel */
extern unsigned long gt_dos_dopus_sends;
/* how many times gt_dos_select set Workbench pens 0-3 to the theme (wb_pens_apply), and how many times it refused for weak
 * pen 0 / pen 1 contrast (menu_theme.h wb_pens_plan): the reachability sentinels */
extern unsigned long gt_dos_wb_pens, gt_dos_wb_refused;
/* fill the Style > Theme rows (ix_add_gthemes) with the current one marked. Call after the other loaders. */
void gt_dos_load_rows(ix_index *x);

#endif
