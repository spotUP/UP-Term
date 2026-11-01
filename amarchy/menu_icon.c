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
            return ce_page_glyph_px(mi_page_data, (MI_CP0 + i) & 255, MI_PIXELS);
    return 0;
}

void mi_item_id(const char *node, const mn_item *it, char *id, int size)
{
    if (MN_IS_INDEX(it)) {
        strncpy(id, node, size - 1);
        id[size - 1] = 0;
        if (!id[0] || !mi_find(id))
            strcpy(id, "apps");
        return;
    }
    mn_child_node(node, it, id, size);
    if (MN_IS_GROUP(it) && !mi_find(id))
        strcpy(id, "drawer");
}

int mi_size(const mg_metrics *m)
{
    int h = m->unit * 8 / 15;
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

int mi_source(const vt_u8 *glyph, vt_u8 *idx)
{
    if (!glyph)
        return 0;
    ce_unpack(glyph + 2 + 4 * (glyph[1] + 1), glyph[0], MI_SRC, MI_SRC, MI_SRC, MI_SRC, idx);
    return 1;
}

int mi_render(const vt_u8 *glyph, int w, int h, vt_u8 *out)
{
    static vt_u8 idx[MI_SRC * MI_SRC];
    int x, y, sx, sy;
    if (w < 1 || w > MI_MAX_W || h < 1 || h > MI_MAX || !mi_source(glyph, idx))
        return 0;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            /* output pixel x covers [x * MI_SRC, x * MI_SRC + MI_SRC) of the source's MI_SRC * w units
             * across, and likewise down: the weights of one output pixel sum to MI_SRC * MI_SRC */
            long sum = 0;
            for (sy = y * MI_SRC / h; sy < MI_SRC && sy * h < (y + 1) * MI_SRC; sy++) {
                int wy = overlap(sy * (long)h, (sy + 1) * (long)h, y * (long)MI_SRC, (y + 1) * (long)MI_SRC);
                for (sx = x * MI_SRC / w; sx < MI_SRC && sx * w < (x + 1) * MI_SRC; sx++) {
                    int wx = overlap(sx * (long)w, (sx + 1) * (long)w, x * (long)MI_SRC, (x + 1) * (long)MI_SRC);
                    vt_u8 i = idx[sy * MI_SRC + sx];
                    if (i != CE_BG)
                        sum += (long)wx * wy * glyph[2 + 4 * i + 3];
                }
            }
            out[y * w + x] = (vt_u8)((sum + MI_SRC * MI_SRC / 2) / (MI_SRC * MI_SRC));
        }
    return 1;
}
