/* barconf_dos: reads Amarchy's bar.conf (barconf_core.h): ENV:Amarchy/bar.conf, else ENVARC:Amarchy/bar.conf. */
#ifndef AMARCHY_BARCONF_DOS_H
#define AMARCHY_BARCONF_DOS_H
#include "barconf_core.h"

#define BARCONF_ENV "ENV:Amarchy/bar.conf"
#define BARCONF_ENVARC "ENVARC:Amarchy/bar.conf"

/* 1 when a file was read, 0 when neither exists (c holds the defaults) */
int barconf_load(barconf *c);

#endif
