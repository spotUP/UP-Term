/* amconf_dos: Amarchy's settings file on AmigaDOS (see amconf_core.h for the chain and the rules). */
#ifndef AMCONF_DOS_H
#define AMCONF_DOS_H
#include "amconf_core.h"

int amconf_load(am_prefs *out);                     /* the AMCONF_* source taken */
int amconf_save(const am_prefs *p, int keep, int *failed);
int amconf_migrate(void);                           /* AMCONF_MIG_* */

#endif
