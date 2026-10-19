/* layout: see layout.h. Portable C89, no allocation. */
#include "layout.h"

#define RATIO_ONE 65536L

static int dv_px(const lay *l, int horizontal)
{
    if (l->div_mode == LAY_DIV_PIXEL)
        return 1;
    return horizontal ? l->cw : l->ch;
}

/* Smallest extent a subtree can have along an axis. */
static int min_ext(const lay *l, int i, int horizontal)
{
    const lay_node *n = &l->n[i];
    int ea, eb;
    if (n->pane >= 0)
        return horizontal ? l->min_cols * l->cw : l->min_rows * l->ch;
    ea = min_ext(l, n->a, horizontal);
    eb = min_ext(l, n->b, horizontal);
    if ((n->dir == LAY_RIGHT) == (horizontal != 0))
        return ea + dv_px(l, horizontal) + eb;
    return ea > eb ? ea : eb;
}

/* The first child's extent for a split of room avail: ratio, snapped to
 * whole cells, held between the two children's minimums. */
static int first_ext(const lay *l, const lay_node *n, int avail, int cell)
{
    int horizontal = n->dir == LAY_RIGHT;
    long want = (long)avail * n->ratio / RATIO_ONE;
    int a = (int)((want + cell / 2) / cell) * cell;
    int lo = min_ext(l, n->a, horizontal);
    int hi = avail - min_ext(l, n->b, horizontal);
    if (hi < lo)
        hi = lo;
    if (a < lo)
        a = lo;
    if (a > hi)
        a = hi;
    if (a > avail)
        a = avail;
    if (a < 0)
        a = 0;
    return a;
}

static void solve_node(lay *l, int i, int x, int y, int w, int h)
{
    lay_node *n = &l->n[i];
    int horizontal, dv, avail, a;
    n->r.x = x; n->r.y = y; n->r.w = w; n->r.h = h;
    if (n->pane >= 0)
        return;
    horizontal = n->dir == LAY_RIGHT;
    dv = dv_px(l, horizontal);
    avail = (horizontal ? w : h) - dv;
    if (avail < 0)
        avail = 0;
    a = first_ext(l, n, avail, horizontal ? l->cw : l->ch);
    if (horizontal) {
        n->div.x = x + a; n->div.y = y; n->div.w = dv; n->div.h = h;
        solve_node(l, n->a, x, y, a, h);
        solve_node(l, n->b, x + a + dv, y, avail - a, h);
    } else {
        n->div.x = x; n->div.y = y + a; n->div.w = w; n->div.h = dv;
        solve_node(l, n->a, x, y, w, a);
        solve_node(l, n->b, x, y + a + dv, w, avail - a);
    }
}

static void resolve(lay *l)
{
    solve_node(l, l->root, l->area.x, l->area.y, l->area.w, l->area.h);
}

void lay_solve(lay *l, int x, int y, int w, int h)
{
    l->area.x = x; l->area.y = y; l->area.w = w; l->area.h = h;
    resolve(l);
}

void lay_init(lay *l, int cell_w, int cell_h, int div_mode,
              int min_cols, int min_rows, int x, int y, int w, int h)
{
    int i;
    for (i = 0; i < LAY_MAX_NODES; i++) {
        l->n[i].used = 0;
        l->n[i].pane = -1;
    }
    l->cw = cell_w > 0 ? cell_w : 1;
    l->ch = cell_h > 0 ? cell_h : 1;
    l->div_mode = div_mode;
    l->min_cols = min_cols;
    l->min_rows = min_rows;
    l->root = 0;
    l->n[0].used = 1;
    l->n[0].pane = 0;
    l->npanes = 1;
    lay_solve(l, x, y, w, h);
}

static int leaf_of(const lay *l, int pane)
{
    int i;
    for (i = 0; i < LAY_MAX_NODES; i++)
        if (l->n[i].used && l->n[i].pane == pane)
            return i;
    return -1;
}

static int free_node(const lay *l)
{
    int i;
    for (i = 0; i < LAY_MAX_NODES; i++)
        if (!l->n[i].used)
            return i;
    return -1;
}

static int free_pane(const lay *l)
{
    int p;
    for (p = 0; p < LAY_MAX_PANES; p++)
        if (leaf_of(l, p) < 0)
            return p;
    return -1;
}

static int first_leaf(const lay *l, int i)
{
    while (l->n[i].pane < 0)
        i = l->n[i].a;
    return l->n[i].pane;
}

int lay_pane_count(const lay *l)
{
    return l->npanes;
}

int lay_pane_rect(const lay *l, int pane, lay_rect *r)
{
    int i = leaf_of(l, pane);
    if (i < 0)
        return 0;
    *r = l->n[i].r;
    return 1;
}

int lay_divider_rect(const lay *l, int divider, lay_rect *r)
{
    if (divider < 0 || divider >= LAY_MAX_NODES || !l->n[divider].used
        || l->n[divider].pane >= 0)
        return 0;
    *r = l->n[divider].div;
    return 1;
}

int lay_divider_at(const lay *l, int nth)
{
    int i;
    for (i = 0; i < LAY_MAX_NODES; i++)
        if (l->n[i].used && l->n[i].pane < 0 && nth-- == 0)
            return i;
    return -1;
}

int lay_split(lay *l, int pane, int dir)
{
    int i, ia, ib, np;
    lay_node *n;
    if ((dir != LAY_RIGHT && dir != LAY_DOWN) || l->npanes >= LAY_MAX_PANES)
        return -1;
    i = leaf_of(l, pane);
    np = free_pane(l);
    if (i < 0 || np < 0)
        return -1;
    n = &l->n[i];
    n->used = 1;
    l->n[ia = free_node(l)].used = 1;
    l->n[ib = free_node(l)].used = 1;
    l->n[ia].pane = (signed char)pane;
    l->n[ib].pane = (signed char)np;
    n->pane = -1;
    n->dir = (signed char)dir;
    n->a = (signed char)ia;
    n->b = (signed char)ib;
    n->ratio = RATIO_ONE / 2;
    if (min_ext(l, l->root, 1) > l->area.w || min_ext(l, l->root, 0) > l->area.h) {
        n->pane = (signed char)pane;
        l->n[ia].used = 0;
        l->n[ib].used = 0;
        l->n[ia].pane = -1;
        l->n[ib].pane = -1;
        return -1;
    }
    l->npanes++;
    resolve(l);
    return np;
}

int lay_close(lay *l, int pane)
{
    int i = leaf_of(l, pane), p, s;
    if (i < 0 || l->npanes <= 1)
        return -1;
    for (p = 0; p < LAY_MAX_NODES; p++)
        if (l->n[p].used && l->n[p].pane < 0 && (l->n[p].a == i || l->n[p].b == i))
            break;
    s = l->n[p].a == i ? l->n[p].b : l->n[p].a;
    l->n[p] = l->n[s];      /* the sibling's subtree takes the parent's place */
    l->n[i].used = 0;
    l->n[i].pane = -1;
    if (s != p) {
        l->n[s].used = 0;
        l->n[s].pane = -1;
    }
    l->npanes--;
    resolve(l);
    return first_leaf(l, p);
}

static int overlap(int a0, int a1, int b0, int b1)
{
    int lo = a0 > b0 ? a0 : b0;
    int hi = a1 < b1 ? a1 : b1;
    return hi - lo;
}

int lay_neighbour(const lay *l, int pane, int dir)
{
    lay_rect me, o;
    int p, best = -1, bgap = 0, bdist = 0, gap, dist, ov;
    if (!lay_pane_rect(l, pane, &me))
        return -1;
    for (p = 0; p < LAY_MAX_PANES; p++) {
        if (p == pane || !lay_pane_rect(l, p, &o))
            continue;
        if (dir == LAY_RIGHT) {
            gap = o.x - (me.x + me.w);
            ov = overlap(me.y, me.y + me.h, o.y, o.y + o.h);
            dist = (o.y + o.h / 2) - (me.y + me.h / 2);
        } else if (dir == LAY_LEFT) {
            gap = me.x - (o.x + o.w);
            ov = overlap(me.y, me.y + me.h, o.y, o.y + o.h);
            dist = (o.y + o.h / 2) - (me.y + me.h / 2);
        } else if (dir == LAY_DOWN) {
            gap = o.y - (me.y + me.h);
            ov = overlap(me.x, me.x + me.w, o.x, o.x + o.w);
            dist = (o.x + o.w / 2) - (me.x + me.w / 2);
        } else {
            gap = me.y - (o.y + o.h);
            ov = overlap(me.x, me.x + me.w, o.x, o.x + o.w);
            dist = (o.x + o.w / 2) - (me.x + me.w / 2);
        }
        if (gap < 0 || ov <= 0)
            continue;
        if (dist < 0)
            dist = -dist;
        if (best < 0 || gap < bgap || (gap == bgap && dist < bdist)) {
            best = p; bgap = gap; bdist = dist;
        }
    }
    return best;
}

int lay_hit_divider(const lay *l, int x, int y, int slop)
{
    int i;
    for (i = 0; i < LAY_MAX_NODES; i++) {
        const lay_node *n = &l->n[i];
        int x0, y0, x1, y1;
        if (!n->used || n->pane >= 0)
            continue;
        x0 = n->div.x; y0 = n->div.y;
        x1 = x0 + n->div.w; y1 = y0 + n->div.h;
        if (n->dir == LAY_RIGHT) { x0 -= slop; x1 += slop; }
        else { y0 -= slop; y1 += slop; }
        if (x >= x0 && x < x1 && y >= y0 && y < y1)
            return i;
    }
    return -1;
}

int lay_drag(lay *l, int divider, int px)
{
    lay_node *n;
    int horizontal, dv, avail, cell, start, a, old, lo, hi;
    if (divider < 0 || divider >= LAY_MAX_NODES || !l->n[divider].used
        || l->n[divider].pane >= 0)
        return 0;
    n = &l->n[divider];
    horizontal = n->dir == LAY_RIGHT;
    dv = dv_px(l, horizontal);
    cell = horizontal ? l->cw : l->ch;
    start = horizontal ? n->r.x : n->r.y;
    avail = (horizontal ? n->r.w : n->r.h) - dv;
    if (avail <= 0)
        return 0;
    old = horizontal ? n->div.x - n->r.x : n->div.y - n->r.y;
    a = px - start;
    if (a < 0)
        a = 0;
    a = ((a + cell / 2) / cell) * cell;
    lo = min_ext(l, n->a, horizontal);
    hi = avail - min_ext(l, n->b, horizontal);
    if (hi < lo)
        return 0;
    if (a < lo)
        a = lo;
    if (a > hi)
        a = hi;
    n->ratio = ((long)a * RATIO_ONE + avail / 2) / avail;
    resolve(l);
    return a != old;
}
