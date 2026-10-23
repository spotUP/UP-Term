/* menu_icon: see menu_icon.h. */
#include <string.h>

#include "menu_icon.h"
#include "menu_icons.inc"       /* generated: mi_page_data[], mi_ids[] */

#define MI_COUNT ((int)(sizeof mi_ids / sizeof mi_ids[0]))
#define MI_CP0 0xE000           /* the glyph of mi_ids[0]: tools/gen_menu_icons.py BASE */

const vt_u8 *mi_page(long *len)
{
    *len = (long)sizeof mi_page_data;
    return mi_page_data;
}

int mi_count(void)
{
    return MI_COUNT;
}

const char *mi_id(int i)
{
    return i >= 0 && i < MI_COUNT ? mi_ids[i] : 0;
}

const vt_u8 *mi_find(const char *id)
{
    int i;
    for (i = 0; i < MI_COUNT; i++)
        if (!strcmp(mi_ids[i], id))
            return ce_page_glyph(mi_page_data, (MI_CP0 + i) & 255);
    return 0;
}

int mi_size(const mg_metrics *m)
{
    int s = m->row_h / 2;
    return s < 8 ? 8 : (s > MI_MAX ? MI_MAX : s);
}

static int overlap(long a0, long a1, long b0, long b1)
{
    long lo = a0 > b0 ? a0 : b0, hi = a1 < b1 ? a1 : b1;
    return hi > lo ? (int)(hi - lo) : 0;
}

int mi_render(const vt_u8 *glyph, int s, vt_u8 *out)
{
    static vt_u8 idx[16 * 16];
    int x, y, sx, sy;
    if (!glyph || s < 1 || s > MI_MAX || !ce_image(glyph, CE_W, 16, idx))
        return 0;
    for (y = 0; y < s; y++)
        for (x = 0; x < s; x++) {
            /* output pixel x covers [x * 16, x * 16 + 16) of the source's 16 * s units */
            long sum = 0;
            for (sy = y * 16 / s; sy < 16 && sy * s < (y + 1) * 16; sy++) {
                int wy = overlap(sy * (long)s, (sy + 1) * (long)s, y * 16L, (y + 1) * 16L);
                for (sx = x * 16 / s; sx < 16 && sx * s < (x + 1) * 16; sx++) {
                    int wx = overlap(sx * (long)s, (sx + 1) * (long)s, x * 16L, (x + 1) * 16L);
                    vt_u8 i = idx[sy * 16 + sx];
                    if (i != CE_BG)
                        sum += (long)wx * wy * glyph[2 + 4 * i + 3];
                }
            }
            out[y * s + x] = (vt_u8)((sum + 128) / 256);
        }
    return 1;
}
