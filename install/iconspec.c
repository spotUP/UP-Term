/* iconspec (see iconspec.h). Portable C89. */
#include <string.h>
#include "iconspec.h"

int iconspec_window_device(const char *value, const char *dev, char *out, int max)
{
    const char *rest;
    int dl = (int)strlen(dev), rl;
    if (!value || !*value)
        value = ICONSPEC_DEFAULT;
    rest = strchr(value, ':');
    rest = rest ? rest : value; /* no device: the whole value is the window spec */
    rl = (int)strlen(rest);
    if (dl + (*rest == ':' ? 0 : 1) + rl + 1 > max)
        return -1;
    memcpy(out, dev, dl);
    if (*rest != ':')
        out[dl++] = ':';
    memcpy(out + dl, rest, rl + 1);
    return strcmp(out, value) != 0;
}
