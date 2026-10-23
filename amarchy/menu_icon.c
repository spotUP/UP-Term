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
    int h = m->row_h * 8 / 15;
    return h < 8 ? 8 : (h > MI_MAX ? MI_MAX : h);
}

int mi_width(const mg_metrics *m, const mg_screen *s)
{
    int w = mg_cols(s, mi_size(m));
    return w > MI_MAX_W ? MI_MAX_W : w;
}

static int overlap(long a0, long a1, long b0, long b1)
{
    long lo = a0 > b0 ? a0 : b0, hi = a1 < b1 ? a1 : b1;
    return hi > lo ? (int)(hi - lo) : 0;
}

int mi_render(const vt_u8 *glyph, int w, int h, vt_u8 *out)
{
    static vt_u8 idx[16 * 16];
    int x, y, sx, sy;
    if (!glyph || w < 1 || w > MI_MAX_W || h < 1 || h > MI_MAX || !ce_image(glyph, CE_W, 16, idx))
        return 0;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            /* output pixel x covers [x * 16, x * 16 + 16) of the source's 16 * w units across, and
             * likewise [y * 16, y * 16 + 16) of 16 * h down: the weights of one output pixel sum to 256 */
            long sum = 0;
            for (sy = y * 16 / h; sy < 16 && sy * h < (y + 1) * 16; sy++) {
                int wy = overlap(sy * (long)h, (sy + 1) * (long)h, y * 16L, (y + 1) * 16L);
                for (sx = x * 16 / w; sx < 16 && sx * w < (x + 1) * 16; sx++) {
                    int wx = overlap(sx * (long)w, (sx + 1) * (long)w, x * 16L, (x + 1) * 16L);
                    vt_u8 i = idx[sy * 16 + sx];
                    if (i != CE_BG)
                        sum += (long)wx * wy * glyph[2 + 4 * i + 3];
                }
            }
            out[y * w + x] = (vt_u8)((sum + 128) / 256);
        }
    return 1;
}
