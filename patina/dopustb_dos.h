/* dopustb_dos: the Amiga half of the Directory Opus toolbar size and blend (plan 2026-11-08-dopus-toolbar-size-blend.md DT-2..DT-4):
 * the font height and pixel aspect of the DOpus screen, and the CMAP rewrite of the brushes in DOpus5:Images/Patina/<set>/.
 * Hand dopustb_dos_ops to dopus_theme_send (menu_theme.h). */
#ifndef DOPUSTB_DOS_H
#define DOPUSTB_DOS_H
#include "menu_theme.h"

extern const dopus_tb_ops dopustb_dos_ops;

/* The lister toolbar's "<<" arrow is read from DOpus5:Images/ToolbarArrow.image{,_frame}, not from the button bank. Theme apply copies the
 * picked set's flat files over them (the stock ones saved once to DOpus5:Images/Patina/orig/ first, never overwritten); this puts the
 * stock ones back. DOpus reads them at its start: the change shows after the next DOpus start. Returns the number of files written. */
int dopustb_arrow_restore(void);

/* New listers take their toolbar from the TBAR chunk of DOpus5:Environment/default and workbench (stock PROGDIR:Buttons/toolbar). apply writes
 * bank there when the current path is stock or one of our banks; restore writes the stock path back when it is one of ours. A user's own path,
 * a missing volume or file, and any write failure leave the files as they were. Returns the number of files rewritten. */
int dopustb_env_apply(const char *bank, const char *set);
int dopustb_env_restore(void);
#endif
