/* The pure part of AmTile: classify, order, layouts (tile_core.h). */
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

static void master_stack(const win_screen *s, int gap, int ratio_pct, int nmaster, int mirror, int n,
                         const tile_win *in, win_box *out)
{
    int x0 = gap, y0 = s->title + gap;
    int w = s->width - 2 * gap, h = s->height - s->title - 2 * gap;
    int nm = nmaster < 1 ? 1 : nmaster > n ? n : nmaster;
    int mw, sw, mx, sx, ch, i, y, k;
    if (nm >= n) {                                         /* everything in the master column: one full-width stack */
        ch = (h - (n - 1) * gap) / n;
        for (i = 0, y = y0; i < n; i++, y += ch + gap)
            cell(&out[i], &in[i], s, x0, y, w, i == n - 1 ? y0 + h - y : ch);
        return;
    }
    mw = (w - gap) * ratio_pct / 100;
    sw = w - gap - mw;
    mx = mirror ? x0 + sw + gap : x0;
    sx = mirror ? x0 : x0 + mw + gap;
    ch = (h - (nm - 1) * gap) / nm;
    for (i = 0, y = y0; i < nm; i++, y += ch + gap)
        cell(&out[i], &in[i], s, mx, y, mw, i == nm - 1 ? y0 + h - y : ch);
    k = n - nm;
    ch = (h - (k - 1) * gap) / k;
    for (i = nm, y = y0; i < n; i++, y += ch + gap)
        cell(&out[i], &in[i], s, sx, y, sw, i == n - 1 ? y0 + h - y : ch);
}

static void columns(const win_screen *s, int gap, int n, const tile_win *in, win_box *out)
{
    int x0 = gap, y0 = s->title + gap;
    int w = s->width - 2 * gap, h = s->height - s->title - 2 * gap;
    int cw = (w - (n - 1) * gap) / n, i, x;
    for (i = 0, x = x0; i < n; i++, x += cw + gap)
        cell(&out[i], &in[i], s, x, y0, i == n - 1 ? x0 + w - x : cw, h);
}

static void rows(const win_screen *s, int gap, int n, const tile_win *in, win_box *out)
{
    int x0 = gap, y0 = s->title + gap;
    int w = s->width - 2 * gap, h = s->height - s->title - 2 * gap;
    int ch = (h - (n - 1) * gap) / n, i, y;
    for (i = 0, y = y0; i < n; i++, y += ch + gap)
        cell(&out[i], &in[i], s, x0, y, w, i == n - 1 ? y0 + h - y : ch);
}

static void grid(const win_screen *s, int gap, int n, const tile_win *in, win_box *out)
{
    int x0 = gap, y0 = s->title + gap;
    int w = s->width - 2 * gap, h = s->height - s->title - 2 * gap;
    int cols = 1, nrows, rh, r, c, k, cw, x, y, i = 0;
    while (cols * cols < n)
        cols++;
    nrows = (n + cols - 1) / cols;
    rh = (h - (nrows - 1) * gap) / nrows;
    for (r = 0, y = y0; r < nrows; r++, y += rh + gap) {
        k = r == nrows - 1 ? n - (nrows - 1) * cols : cols;
        cw = (w - (k - 1) * gap) / k;
        for (c = 0, x = x0; c < k; c++, x += cw + gap, i++)
            cell(&out[i], &in[i], s, x, y, c == k - 1 ? x0 + w - x : cw, r == nrows - 1 ? y0 + h - y : rh);
    }
}

static void monocle(const win_screen *s, int gap, int n, const tile_win *in, win_box *out)
{
    int i;
    for (i = 0; i < n; i++)
        cell(&out[i], &in[i], s, gap, s->title + gap, s->width - 2 * gap, s->height - s->title - 2 * gap);
}

/* Oldest window first: it takes the left / top share of the longer side, the remainder box shrinks toward in[0]. */
static void dwindle(const win_screen *s, int gap, int ratio_pct, int n, const tile_win *in, win_box *out)
{
    int bx = gap, by = s->title + gap;
    int bw = s->width - 2 * gap, bh = s->height - s->title - 2 * gap;
    int i, f;
    for (i = n - 1; i >= 1; i--) {
        if (bw >= bh) {
            f = (bw - gap) * ratio_pct / 100;
            cell(&out[i], &in[i], s, bx, by, f, bh);
            bx += f + gap;
            bw -= gap + f;
        } else {
            f = (bh - gap) * ratio_pct / 100;
            cell(&out[i], &in[i], s, bx, by, bw, f);
            by += f + gap;
            bh -= gap + f;
        }
    }
    cell(&out[0], &in[0], s, bx, by, bw, bh);
}

void tile_layout(int kind, const win_screen *s, int gap, int ratio_pct, int nmaster, int n, const tile_win *in,
                 win_box *out)
{
    if (n < 1)
        return;
    switch (kind) {
    case TL_MASTER:  master_stack(s, gap, ratio_pct, nmaster, 0, n, in, out); break;
    case TL_MIRROR:  master_stack(s, gap, ratio_pct, nmaster, 1, n, in, out); break;
    case TL_COLUMNS: columns(s, gap, n, in, out); break;
    case TL_ROWS:    rows(s, gap, n, in, out); break;
    case TL_GRID:    grid(s, gap, n, in, out); break;
    case TL_MONOCLE: monocle(s, gap, n, in, out); break;
    default:         dwindle(s, gap, ratio_pct, n, in, out); break;
    }
}

static const char *const layout_names[TL_COUNT] = { "dwindle", "master-stack", "mirrored", "columns", "rows", "grid",
                                                    "monocle" };

const char *tile_layout_name(int kind)
{
    return kind >= 0 && kind < TL_COUNT ? layout_names[kind] : NULL;
}

int tile_layout_parse(const char *word, int *kind)
{
    int k;
    size_t i;
    for (k = 0; k < TL_COUNT; k++) {
        const char *nm = layout_names[k];
        for (i = 0; nm[i] && word[i]; i++)
            if ((word[i] | 0x20) != nm[i])
                break;
        if (!nm[i] && (word[i] == '\0' || word[i] == ' ')) {
            *kind = k;
            return 1;
        }
    }
    return 0;
}

int tile_layout_step(int kind, int dir)
{
    return ((kind + (dir < 0 ? -1 : 1)) % TL_COUNT + TL_COUNT) % TL_COUNT;
}

int tile_promote(long *order, int n, long id)
{
    int i;
    for (i = 0; i < n; i++)
        if (order[i] == id)
            break;
    if (i == n)
        return 0;
    memmove(order + 1, order, (size_t)i * sizeof order[0]);
    order[0] = id;
    return 1;
}

int tile_cycle(const long *order, int n, long id, int dir)
{
    int i;
    if (n < 1)
        return 0;
    for (i = 0; i < n; i++)
        if (order[i] == id)
            return ((i + (dir < 0 ? -1 : 1)) % n + n) % n;
    return 0;
}

int tile_state_on(const char *buf, long len)
{
    return len >= 2 && (buf[0] == 'o' || buf[0] == 'O') && (buf[1] == 'n' || buf[1] == 'N') &&
           (len == 2 || buf[2] == '\n' || buf[2] == '\r' || buf[2] == ' ');
}
