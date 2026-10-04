/* vtinput: the window's input policy (see vtinput.h). */
#include "vtinput.h"

#define MOUSE_MODES (VT_MODE_MOUSE_X10 | VT_MODE_MOUSE_NORMAL | VT_MODE_MOUSE_BUTTON | VT_MODE_MOUSE_ANY)

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
    }
    return n;
}
