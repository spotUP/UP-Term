/* barconf_dos: see barconf_dos.h. The file read is amconf's (amconf_dos_io), the same callback the prefs chain uses. */
#include "barconf_dos.h"
#include "amconf_dos.h"

#define BARCONF_MAX 2048

int barconf_load(barconf *c)
{
    static char buf[BARCONF_MAX];
    const amconf_io *io = amconf_dos_io();
    long n = io->read(0, BARCONF_ENV, buf, BARCONF_MAX);
    if (n < 0)
        n = io->read(0, BARCONF_ENVARC, buf, BARCONF_MAX);
    if (n < 0) {
        barconf_defaults(c);
        return 0;
    }
    return barconf_parse(buf, n, c);
}
