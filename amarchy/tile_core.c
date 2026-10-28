/* The pure part of AmTile: classify, order, master-stack boxes (tile_core.h). */
#include <string.h>
#include "tile_core.h"

#define TILE_FRESH_MAX 64

tile_kind tile_classify(const tile_win *w)
{
    if (!(w->flags & TW_SIZEGADGET) || (w->flags & (TW_BACKDROP | TW_BORDERLESS)))
        return TILE_FLOAT;
    if (w->title && strcmp(w->title, "Amarchy") == 0)
        return TILE_FLOAT;
    if ((w->box.maxw > 0 && w->box.maxw <= w->box.minw) || (w->box.maxh > 0 && w->box.maxh <= w->box.minh))
        return TILE_FLOAT;                                 /* fixed size: nothing to resize */
    return TILE_TILE;
}

static int has(const long *a, int n, long id)
{
    int i;
    for (i = 0; i < n; i++)
        if (a[i] == id)
            return 1;
    return 0;
}

int tile_order_update(long *order, int n, const long *set, int m)
{
    long fresh[TILE_FRESH_MAX];
    int i, k = 0, f = 0;
    for (i = 0; i < n; i++)
        if (has(set, m, order[i]))
            order[k++] = order[i];
    for (i = 0; i < m && f < TILE_FRESH_MAX; i++)
        if (!has(order, k, set[i]))
            fresh[f++] = set[i];
    if (f) {
        memmove(order + f, order, (size_t)k * sizeof order[0]);
        memcpy(order, fresh, (size_t)f * sizeof fresh[0]);
        k += f;
    }
    return k;
}

/* One cell: the window's own limits and the room the screen leaves from the cell's corner decide the size. */
static void cell(win_box *o, const tile_win *w, const win_screen *s, int x, int y, int cw, int ch)
{
    *o = w->box;
    o->left = x;
    o->top = y;
    o->width = win_fit(cw, w->box.minw, w->box.maxw, s->width - x);
    o->height = win_fit(ch, w->box.minh, w->box.maxh, s->height - y);
}

int tile_screen_switch(long *cur, long next, int n)
{
    if (*cur == next)
        return n;
    *cur = next;
    return 0;
}

void tile_master_stack(const win_screen *s, int gap, int ratio_pct, int n, const tile_win *in, win_box *out)
{
    int x0 = gap, y0 = s->title + gap;
    int w = s->width - 2 * gap, h = s->height - s->title - 2 * gap;
    int mw, sx, sw, cells, ch, i, y;
    if (n < 1)
        return;
    if (n == 1) {
        cell(&out[0], &in[0], s, x0, y0, w, h);
        return;
    }
    mw = (w - gap) * ratio_pct / 100;
    sx = x0 + mw + gap;
    sw = w - gap - mw;
    cell(&out[0], &in[0], s, x0, y0, mw, h);
    cells = n - 1;
    ch = (h - (cells - 1) * gap) / cells;
    for (i = 1, y = y0; i < n; i++, y += ch + gap)
        cell(&out[i], &in[i], s, sx, y, sw, i == n - 1 ? y0 + h - y : ch);
}

int tile_state_on(const char *buf, long len)
{
    return len >= 2 && (buf[0] == 'o' || buf[0] == 'O') && (buf[1] == 'n' || buf[1] == 'N') &&
           (len == 2 || buf[2] == '\n' || buf[2] == '\r' || buf[2] == ' ');
}
