/* vtinput: the window's input policy (see vtinput.h). */
#include "vtinput.h"

#define MOUSE_MODES (VT_MODE_MOUSE_X10 | VT_MODE_MOUSE_NORMAL | VT_MODE_MOUSE_BUTTON | VT_MODE_MOUSE_ANY)

int vti_mods(unsigned qual, int meta_alt)
{
    int mods = 0;
    if (qual & VTI_QUAL_SHIFT)
        mods |= VT_MOD_SHIFT;
    if (qual & VTI_QUAL_CONTROL)
        mods |= VT_MOD_CTRL;
    if (qual & (meta_alt ? VTI_QUAL_ALT : VTI_QUAL_LCOMMAND))
        mods |= VT_MOD_ALT;
    return mods;
}

int vti_button_code(unsigned code, int *btn, int *down)
{
    switch (code & ~(unsigned)VTI_CODE_UP) {
    case VTI_CODE_LBUTTON: *btn = 0; break;
    case VTI_CODE_MBUTTON: *btn = 1; break;
    case VTI_CODE_RBUTTON: *btn = 2; break;
    default: return 0;
    }
    *down = !(code & VTI_CODE_UP);
    return 1;
}

int vti_cell_at(const vti_geom *g, int px, int py, int *x, int *y)
{
    int cx, cy;
    if (px < g->ox || py < g->oy || g->cw <= 0 || g->ch <= 0)
        return 0;
    cx = (px - g->ox) / g->cw;
    cy = (py - g->oy) / g->ch;
    if (cx >= g->cols || cy >= g->rows)
        return 0;
    *x = cx;
    *y = cy;
    return 1;
}

/* The cell under the pointer, or the nearest one when it is over the border. */
static void cell_clamped(const vti_geom *g, int px, int py, int *x, int *y)
{
    int cx = g->cw > 0 && px > g->ox ? (px - g->ox) / g->cw : 0;
    int cy = g->ch > 0 && py > g->oy ? (py - g->oy) / g->ch : 0;
    *x = cx < g->cols ? cx : g->cols - 1;
    *y = cy < g->rows ? cy : g->rows - 1;
    if (*x < 0)
        *x = 0;
    if (*y < 0)
        *y = 0;
}

void vti_mouse_reset(vti_mouse *m)
{
    m->held = 0;
    m->last_x = m->last_y = -1;
    m->clicks = 0;
    m->click_x = m->click_y = -1;
}

int vti_button(vti_mouse *m, const vt_term *t, int btn, int down, int x, int y, int in,
               int mods, int selecting, int dclick, vt_u8 *out, int *n)
{
    int bit = 1 << btn;
    *n = 0;
    if (!down && (m->held & bit)) {
        /* the program had the press: the release is its too, wherever the
         * pointer went (off the grid: the cell it was last told about) */
        m->held &= ~bit;
        if (!in) {
            x = m->last_x;
            y = m->last_y;
        }
        *n = vt_encode_mouse(t, btn, 1, x, y, mods, out);
        return *n ? VTI_REPORT : VTI_NONE; /* X10 has no releases */
    }
    if (!(mods & VT_MOD_SHIFT) && in && !selecting && (vt_modes(t) & MOUSE_MODES)) {
        *n = vt_encode_mouse(t, btn, down ? 0 : 1, x, y, mods, out);
        if (*n) {
            if (down)
                m->held |= bit;
            m->last_x = x;
            m->last_y = y;
            return VTI_REPORT;
        }
    }
    if (btn == 0 && down && in) {
        /* a click soon after the last on the same cell: word, then line,
         * then a character again */
        if (dclick && m->clicks && x == m->click_x && y == m->click_y)
            m->clicks = m->clicks % 3 + 1;
        else
            m->clicks = 1;
        m->click_x = x;
        m->click_y = y;
        return VTI_SELECT;
    }
    if (btn == 0 && !down && selecting)
        return VTI_SELECT_END;
    return VTI_NONE;
}

int vti_wants_motion(const vti_mouse *m, const vt_term *t)
{
    vt_u32 md = vt_modes(t);
    return (md & VT_MODE_MOUSE_ANY) || ((md & VT_MODE_MOUSE_BUTTON) && m->held);
}

int vti_motion(vti_mouse *m, const vt_term *t, int x, int y, int mods, vt_u8 *out)
{
    int b, n;
    if (!vti_wants_motion(m, t) || (x == m->last_x && y == m->last_y))
        return 0;
    /* the lowest button held (xterm reports one), 3 for none */
    b = (m->held & 1) ? 0 : (m->held & 2) ? 1 : (m->held & 4) ? 2 : 3;
    n = vt_encode_mouse(t, b, 2, x, y, mods, out);
    if (n) {
        m->last_x = x;
        m->last_y = y;
    }
    return n;
}

int vti_wheel(const vti_geom *g, const vt_term *t, int view, int up, int mods,
              int px, int py, vt_u8 *out, int *lines)
{
    int n = 0, x, y;
    *lines = up ? VTI_WHEEL_LINES : -VTI_WHEEL_LINES;
    if (view)
        return 0; /* reading the scrollback: the wheel goes on doing that */
    if (vt_modes(t) & MOUSE_MODES) {
        cell_clamped(g, px, py, &x, &y);
        n = vt_encode_mouse(t, up ? 64 : 65, 0, x, y, mods, out);
    } else if ((vt_modes(t) & (VT_MODE_ALT_SCREEN | VT_MODE_ALT_SCROLL)) ==
               (VT_MODE_ALT_SCREEN | VT_MODE_ALT_SCROLL)) {
        /* ?1007 (xterm's alternateScroll): a pager on the alternate screen
         * scrolls as the cursor keys move it; there is no scrollback here */
        int i;
        for (i = 0; i < VTI_WHEEL_LINES; i++)
            n += vt_encode_key(t, up ? VT_KEY_UP : VT_KEY_DOWN, 0, out + n);
    }
    return n;
}
