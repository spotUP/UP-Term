/* barconf_dos: reads Patina's bar.conf (barconf_core.h): ENV:Patina/bar.conf, else ENVARC:Patina/bar.conf. */
#ifndef PATINA_BARCONF_DOS_H
#define PATINA_BARCONF_DOS_H
#include "barconf_core.h"

#define BARCONF_ENV "ENV:Patina/bar.conf"
#define BARCONF_ENVARC "ENVARC:Patina/bar.conf"

/* 1 when a file was read, 0 when neither exists (c holds the defaults) */
int barconf_load(barconf *c);

#endif
