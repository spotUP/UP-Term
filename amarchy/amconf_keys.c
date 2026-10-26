/* amconf_keys: the key table and the legacy strip, apart from amconf_core so that every program linking
 * prefs_core (whose stage drops the keys) has them without the parse state of amconf_core. */
#include "amconf_core.h"
#include "../prefs/settings_def.h"

const am_key *amconf_keys(void)
{
    static am_key keys[AM_NKEYS];
    static int filled;
    int k;
    if (!filled) {
        for (k = 0; k < AM_NKEYS; k++) {
            const sd_setting *s = sd_amarchy_key(k);
            keys[k].name = s ? s->key : "";
            keys[k].def = s ? s->def : "";
        }
        filled = 1;
    }
    return keys;
}

int amconf_strip_legacy(upconf *c)
{
    int k, n = 0;
    for (k = 0; k < AM_NKEYS; k++)
        if (upconf_str(c, "default", am_keys[k].name, 0) && upconf_del(c, "default", am_keys[k].name))
            n++;
    return n;
}
