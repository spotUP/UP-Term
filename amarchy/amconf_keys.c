/* amconf_keys: the key table and the legacy strip, apart from amconf_core so that every program linking
 * prefs_core (whose stage drops the keys) has them without the parse state of amconf_core. */
#include "amconf_core.h"

const am_key am_keys[AM_NKEYS] = {
    { "amarchy-music-drawers", "" },
    { "amarchy-demo-drawers", "" },
    { "amarchy-hide-drawers", "" },
    { "amarchy-menu-font", "" },
    { "amarchy-app-icons", "generic" },
    { "menu-style", "graphical" },
    { "amarchy-index-skip", "" },
    { "amarchy-index-volumes", "" },
    { "menu-in-terminal", "text" },
};

int amconf_strip_legacy(upconf *c)
{
    int k, n = 0;
    for (k = 0; k < AM_NKEYS; k++)
        if (upconf_str(c, "default", am_keys[k].name, 0) && upconf_del(c, "default", am_keys[k].name))
            n++;
    return n;
}
