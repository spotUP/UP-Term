/* themeprev_core -- see themeprev_core.h */
#include <string.h>
#include "themeprev_core.h"

static void set(themeprev *o, int i, int x, int y, int w, int h, int elem, int state, int radius, int text, const char *label)
{
    tpv_item *t = &o->it[i];
    if (w < 0 || h < 0 || x < 0 || y < 0 || x + w > o->w || y + h > o->h)
        w = h = 0;                   /* does not fit: not drawn */
    t->x = x; t->y = y; t->w = w; t->h = h;
    t->elem = (unsigned char)elem; t->state = (unsigned char)state;
    t->radius = (unsigned char)radius; t->text = (unsigned char)text; t->label = label;
}

/* one window: frame, bar, title, body, three gadgets; scroll bar and knob when scroll >= 0 */
static void window(themeprev *o, const gt_look *l, int base, int x, int y, int w, int bar, int bw, int gs, int active, int body_h,
                   int scroll)
{
    int i, gx, gstep = gs + (l->gadget_pad ? l->gadget_pad : 2), r = l->corner_radius;
    int fe = active ? GTE_BORDER : GTE_BORDER_INACTIVE, be = active ? GTE_TITLE_BAR : GTE_TITLE_BAR_INACTIVE;
    int te = active ? GTE_TITLE_TEXT : GTE_TITLE_TEXT_INACTIVE, ge = active ? GTE_GADGET_GLYPH : GTE_GADGET_GLYPH_INACTIVE;
    set(o, base, x, y, w, bar + body_h + bw, fe, GT_ST_NORMAL, r, 0, 0);
    set(o, base + 1, x + bw, y + bw, w - 2 * bw, bar, be, GT_ST_NORMAL, 0, 0, 0);
    gx = l->gadget_order ? x + w - bw - 2 - 3 * gstep + (gstep - gs) : x + bw + 2;
    for (i = 0; i < 3; i++)
        set(o, base + 4 + i, gx + i * gstep, y + bw + (bar - gs) / 2, gs, gs, ge, GT_ST_NORMAL, r > 2 ? 2 : r, 0, 0);
    set(o, base + 2, l->gadget_order ? x + bw + 4 : x + bw + 2 + 3 * gstep, y + bw, w - 2 * bw - 3 * gstep - 6, bar, te, GT_ST_NORMAL,
        0, 1, active ? "Active Window" : "Inactive Window");
    set(o, base + 3, x + bw, y + bw + bar, w - 2 * bw, body_h, GTE_REQUESTER_BACKGROUND, GT_ST_NORMAL, 0, 0, 0);
    if (scroll >= 0 && active) {
        int sx = x + w - bw - scroll, sy = y + bw + bar + 2, sh = body_h - 4;
        set(o, TPV_ACT_SCROLL, sx, sy, scroll, sh, GTE_SCROLL_TRACK, GT_ST_NORMAL, 0, 0, 0);
        set(o, TPV_ACT_KNOB, sx + 1, sy + 1, scroll - 2, sh / 3 > 4 ? sh / 3 : 4, GTE_SCROLL_KNOB, GT_ST_NORMAL,
            l->knob_shape ? l->corner_radius : 0, 0, 0);
    }
}

void themeprev_layout(const gt_look *look, int w, int h, themeprev *o)
{
    int bar = look->bar_height ? look->bar_height : TPV_STOCK_BAR;
    int bw = look->border_width ? look->border_width : TPV_STOCK_BORDER;
    int gs = look->gadget_size ? look->gadget_size : bar - 2;
    int sc = look->scroll_width ? look->scroll_width : TPV_STOCK_SCROLL;
    int r = look->corner_radius, m = TPV_MARGIN, lw, rx, rw, y, body = 30;
    memset(o, 0, sizeof *o);
    o->w = w;
    o->h = h;
    if (gs < 4)
        gs = 4;
    if (gs > bar)
        gs = bar;
    set(o, TPV_DESKTOP, 0, 0, w, h, GTE_DESKTOP_BACKDROP, GT_ST_NORMAL, 0, 0, 0);
    lw = (w - 3 * m) * 3 / 5;
    rx = m + lw + m;
    rw = w - rx - m;
    window(o, look, TPV_ACT_FRAME, m, m, lw, bar, bw, gs, 1, body, sc);
    window(o, look, TPV_INA_FRAME, m, m + bar + body + 2 * bw + m, lw, bar, bw, gs, 0, body, -1);
    /* the menu strip: header, selected item, disabled item */
    y = m;
    set(o, TPV_MENU, rx, y, rw, 3 * 12 + 4, GTE_MENU_BACKGROUND, GT_ST_NORMAL, r, 0, 0);
    set(o, TPV_MENU_HEADER, rx + 4, y + 2, rw - 8, 12, GTE_MENU_HEADER, GT_ST_NORMAL, 0, 1, "Project");
    set(o, TPV_MENU_SEL, rx + 2, y + 14, rw - 4, 12, GTE_MENU_SELECTION, GT_ST_NORMAL, r, 0, 0);
    set(o, TPV_MENU_DISABLED, rx + 4, y + 26, rw - 8, 12, GTE_MENU_TEXT, GT_ST_DISABLED, 0, 1, "Disabled");
    y += 3 * 12 + 4 + m;
    set(o, TPV_BUTTON, rx, y, rw, 14, GTE_BUTTON_FACE, GT_ST_NORMAL, r, 0, 0);
    y += 14 + m;
    set(o, TPV_FIELD, rx, y, rw, 14, GTE_TEXT_FIELD, GT_ST_NORMAL, 0, 0, 0);
    set(o, TPV_CURSOR, rx + 4, y + 2, 6, 10, GTE_TEXT_CURSOR, GT_ST_NORMAL, 0, 0, 0);
    y += 14 + m;
    set(o, TPV_ICON, rx + (rw - 24) / 2, y, 24, 24, GTE_ICON_FOREGROUND, GT_ST_NORMAL, 0, 0, 0);
    set(o, TPV_ICON_LABEL, rx, y + 26, rw, 10, GTE_ICON_LABEL_TEXT, GT_ST_NORMAL, 0, 1, "Workbench");
}

uc_u32 themeprev_rgb(const gt_model *m, const tpv_item *it)
{
    return gt_resolve_rgb(m, it->elem, it->state);
}

int themeprev_placement(int screen_w, int rows_w)
{
    return screen_w >= rows_w + TPV_WIDTH + 4 * TPV_MARGIN ? TPV_BESIDE : TPV_TOGGLE;
}
