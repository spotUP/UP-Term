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
/* make name the drawn menu's theme: AM_GTHEME is saved to ENVARC: and ENV:. 1 saved, 0 refused or failed */
int gt_dos_select(const char *name);
/* fill the Style > Theme rows (ix_add_gthemes) with the current one marked. Call after the other loaders. */
void gt_dos_load_rows(ix_index *x);

#endif
