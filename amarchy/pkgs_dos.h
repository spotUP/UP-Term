/* pkgs_dos: the DOS side of the Packages menu (pkgs_core.h, plan 2026-10-27-amarchy-packages-submenu.md). It runs
 * `upkg list-installed --tsv` through the UP-Term shell (upkg is on vsh's PATH in UP-Term:bin, not on the AmigaDOS
 * Path) into a T: file of this task, reads and deletes that file, and stores the rows. upkg takes about 100 ms with
 * no package installed (rig 7, 060), a third of a warm open, so the rows load only when the menu enters
 * IX_PKGS_NODE (ix_pkgs_due). Linked by amarchy-menu and amarchy-gui. */
#ifndef PKGS_DOS_H
#define PKGS_DOS_H

#include "index_core.h"

/* Fill the Install > Packages rows (ix_add_pkgs); upkg missing or failing gives the one inert row. Call after the
 * other loaders (it is the last adder). 1 stored, 0 when memory is short. */
int pk_dos_load_rows(ix_index *x);

#endif
