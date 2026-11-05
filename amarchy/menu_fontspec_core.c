/* The font spec the Amarchy drawn menu opens (TE-7b.1); see menu_fontspec_core.h. */
#include <stdio.h>
#include <string.h>
#include "menu_fontspec_core.h"

int mt_font_spec(const gtheme *t, const char *am_font, char *out, int outsize)
{
    char name[GT_NAME];
    int size = 0, used = 0;
    if (outsize <= 0)
        return 0;
    out[0] = 0;
    if (t && gt_look_font(t, GT_FONT_MENU, name, (int)sizeof name, &size)) {
        if (size > 0)
            snprintf(out, (size_t)outsize, "%s:%d", name, size);
        else
            snprintf(out, (size_t)outsize, "%s", name);
        used = 1;
    } else if (am_font) {
        strncpy(out, am_font, (size_t)outsize - 1);
        out[outsize - 1] = 0;
    }
    return used;
}
