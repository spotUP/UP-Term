#include "winspec.h"

int winspec_size(int want, int origin, int extent)
{
    int room;
    if (want != 0)
        return want;
    room = extent - origin;
    return room > 0 ? room : extent;
}
