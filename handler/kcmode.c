/* KingCON's FNCMODE letters (see kcmode.h). */
#include "kcmode.h"

int le_kc_fncmode(const char *letters)
{
    int m = 0;
    for (; letters && *letters; letters++)
        switch (*letters | 0x20) {
        case 'w': m |= LE_KC_WINDOW; break;
        case 'l': m |= LE_KC_LIST; break;
        case 'b': m |= LE_KC_CYCLE; break;
        case 'c': m |= LE_KC_COMMON; break;
        case 's': m |= LE_KC_SILENT; break;
        }
    if (!(m & (LE_KC_WINDOW | LE_KC_LIST | LE_KC_CYCLE | LE_KC_COMMON)))
        m |= LE_KC_WINDOW;
    if (m & LE_KC_WINDOW)
        m &= ~(LE_KC_LIST | LE_KC_CYCLE);
    return m;
}
