/* gthemes_scan_dos: the DOS drawer scan of the Omarchy GUI themes (gthemes_core.h gt_scan). Split from
 * gthemes_dos.h so Prefs, which only lists the themes, links no menu index. */
#ifndef GTHEMES_SCAN_DOS_H
#define GTHEMES_SCAN_DOS_H

#include "gthemes_core.h"

/* the theme list: every file of the shipped and the user drawer */
void gt_dos_scan(gt_list *l);

#endif
