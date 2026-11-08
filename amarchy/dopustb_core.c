/* See dopustb_core.h. */
#include <string.h>
#include "dopustb_core.h"
#include "iconpen_core.h"
#include "iconfile_core.h"

void dt_pick_set(unsigned font_h, int rx, int ry, dt_set *out)
{
    static const unsigned size[3] = { 16, 24, 32 };
    unsigned want = 2 * font_h, best = 1, i;
    unsigned d, bd;
    if (!font_h)
        want = 24;
    bd = want > size[1] ? want - size[1] : size[1] - want;
    for (i = 0; i < 3; i += 2) {
        d = want > size[i] ? want - size[i] : size[i] - want;
        if (d < bd || (d == bd && size[i] < size[best])) {
            best = i;
            bd = d;
        }
    }
    out->size = size[best];
    /* a pixel twice as high as wide is the "screen twice as wide as high" case of icons.c, with the roles swapped */
    out->tall = rx > 0 && ry > 0 && li_pick_variant((unsigned)ry, (unsigned)rx);
}

unsigned dt_set_name(const dt_set *s, char *out, unsigned cap)
{
    unsigned n = 0;
    if (cap < 4)
        return 0;
    out[n++] = (char)('0' + s->size / 10);
    out[n++] = (char)('0' + s->size % 10);
    if (s->tall)
        out[n++] = 't';
    out[n] = 0;
    return n;
}

void dt_cmap(const gtheme *t, uint8_t rgb[DT_PENS][3])
{
    static const int slot[DT_PENS] = { IP_SPARE12, IP_FG33, IP_FG66, IP_FG100, IP_ACCENT33, IP_ACCENT66, IP_ACCENT100, IP_SPARE12 };
    uint8_t ip[IP_PENS][3];
    gt_roles r;
    int i;
    gt_roles_of(t, &r);
    ip_colours(t, r.c[GR_SURFACE], ip);
    for (i = 0; i < DT_PENS; i++)
        memcpy(rgb[i], ip[slot[i]], 3);
}

int dt_cmap_patch(uint8_t *buf, size_t len, const uint8_t rgb[DT_PENS][3])
{
    size_t pos = 12, size;
    if (len < 12 || memcmp(buf, "FORM", 4) != 0 || memcmp(buf + 8, "ILBM", 4) != 0)
        return 0;
    while (pos + 8 <= len) {
        size = ((size_t)buf[pos + 4] << 24) | ((size_t)buf[pos + 5] << 16) | ((size_t)buf[pos + 6] << 8) | buf[pos + 7];
        if (memcmp(buf + pos, "CMAP", 4) == 0) {
            if (size != DT_PENS * 3 || pos + 8 + size > len)
                return 0;
            if (memcmp(buf + pos + 8, rgb, size) == 0)
                return 2;
            memcpy(buf + pos + 8, rgb, size);
            return 1;
        }
        pos += 8 + size + (size & 1);
    }
    return 0;
}
