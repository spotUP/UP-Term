#include "winspec.h"

int winspec_size(int want, int origin, int extent)
{
    int room;
    if (want != 0)
        return want;
    room = extent - origin;
    return room > 0 ? room : extent;
}

/* The UP-Term ground (light grey on black) unless the spec chose colours; the
 * personality (CON: is the amiga dialect) is escape-sequence protocol and does
 * not decide the look. Returns 1 when it set *fg / *bg. */
int winspec_ground(int colours, unsigned long *fg, unsigned long *bg)
{
    if (colours)
        return 0;
    *fg = 0xC0C0C0UL;
    *bg = 0x000000UL;
    return 1;
}
