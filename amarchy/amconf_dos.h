/* amconf_dos: Amarchy's settings file on AmigaDOS (see amconf_core.h for the chain and the rules). */
#ifndef AMCONF_DOS_H
#define AMCONF_DOS_H
#include "amconf_core.h"

int amconf_load(am_prefs *out);                     /* the AMCONF_* source taken */
/* An am_prefs is 4640 bytes: a function that holds one by value keeps a 4.6 KB frame for as long as it runs, and
 * the deepest call chain adds them up (a restart overran a 16 KB stack, rig 177). A caller takes the loaded
 * settings from the heap instead and gives them back on every path out: p = amconf_borrow(); ... amconf_return(p);
 * Never NULL: when no memory is left the defaults come back in a shared static (read it, do not rely on a write
 * surviving; amconf_return knows it). A borrow is private to the caller, so a second task may borrow at once. */
am_prefs *amconf_borrow(void);
void amconf_return(am_prefs *p);
int amconf_save(const am_prefs *p, int keep, int *failed);
int amconf_save_fields(am_prefs *cur, int *src, const prefs_fields *f, int keep, int *failed);
const amconf_io *amconf_dos_io(void);              /* the DOS callbacks, for the *_with functions */
int amconf_migrate(void);                           /* AMCONF_MIG_* */

#endif
