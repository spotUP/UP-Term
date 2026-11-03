/* The host's tabs as slots (plans/2026-10-19-native-splits.md, NS3.1): one
 * slot per entry in the tab bar, each holding the split panes of that tab
 * (render/layout) and which of them has the keys. A pane is a handler
 * process of its own (a con), here only an opaque pointer, so the
 * bookkeeping is pure and the host suite pins it (tests/test_tabslot.c).
 *
 * The bar, the keys, the scroll bar knob and the menu strip all belong to
 * a slot's focused pane (tabslot_focused); a lone pane is the tab as it
 * was before splits. Pane ids are render/layout's: 0..LAY_MAX_PANES-1,
 * they stay with their pane. */
#ifndef TABSLOT_H
#define TABSLOT_H

#include <string.h>
#include "../render/layout.h"

/* the smallest pane a split or a drag leaves (cells) */
#define TABSLOT_MIN_COLS 10
#define TABSLOT_MIN_ROWS 3

typedef struct tab_slot {
    lay l;                       /* the panes' layout; one pane until the first split */
    void *pane[LAY_MAX_PANES];   /* by pane id; 0 = no such pane */
    int focus;                   /* the pane id with the keys */
    int zoom;                    /* 1: the focused pane has the whole slot, the others hidden */
} tab_slot;

static void *tabslot_focused(const tab_slot *t)
{
    return t->pane[t->focus];
}

static int tabslot_panes(const tab_slot *t)
{
    return lay_pane_count(&t->l);
}

/* Every pane of every slot: the window's handler processes */
static int tabslot_all(const tab_slot *t, int n)
{
    int i, all = 0;
    for (i = 0; i < n; i++)
        all += tabslot_panes(&t[i]);
    return all;
}

/* Pane id k is drawn when its slot is shown: it exists, and no other pane is zoomed. */
static int tabslot_shows(const tab_slot *t, int k)
{
    return t->pane[k] && (!t->zoom || k == t->focus);
}

/* The shown pane whose place (layout pixels, the layout solved) holds x y,
 * or -1: one pane, a divider, outside. A zoomed pane is not looked up: it
 * is the only one shown and has the keys anyway. */
static int tabslot_at(const tab_slot *t, int x, int y)
{
    int k;
    lay_rect r;
    if (lay_pane_count(&t->l) < 2 || t->zoom)
        return -1;
    for (k = 0; k < LAY_MAX_PANES; k++)
        if (t->pane[k] && lay_pane_rect(&t->l, k, &r) &&
            x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
            return k;
    return -1;
}

/* A new slot at the end with one pane: its index, or -1 when all max are in use. */
static int tabslot_add(tab_slot *t, int *n, int max, void *pane)
{
    tab_slot *s;
    if (*n >= max || !pane)
        return -1;
    s = &t[*n];
    memset(s, 0, sizeof(*s));
    lay_init(&s->l, 1, 1, LAY_DIV_PIXEL, TABSLOT_MIN_COLS, TABSLOT_MIN_ROWS, 0, 0, 0, 0);
    s->pane[0] = pane;
    s->focus = 0;
    return (*n)++;
}

/* The slot holding pane (and its pane id in *id when id is not 0), or -1. */
static int tabslot_find(const tab_slot *t, int n, const void *pane, int *id)
{
    int i, k;
    if (!pane)
        return -1;
    for (i = 0; i < n; i++)
        for (k = 0; k < LAY_MAX_PANES; k++)
            if (t[i].pane[k] == pane) {
                if (id)
                    *id = k;
                return i;
            }
    return -1;
}

/* Split from's pane in direction dir (LAY_RIGHT / LAY_DOWN) for the new
 * pane, in a slot laid out over the area x y w h (pixels) with a cell of
 * cw x ch. The first split of a slot takes the cell and area; later ones
 * keep the slot's cell and solve the new area. The new pane gets the
 * focus. Returns its pane id, or -1: from is in no slot, the slot has
 * the most panes, or a pane would fall below the minimum. */
static int tabslot_split(tab_slot *t, int n, void *from, void *pane, int dir,
                         int cw, int ch, int x, int y, int w, int h)
{
    int i, k, id;
    if (!pane || (i = tabslot_find(t, n, from, &k)) < 0)
        return -1;
    if (tabslot_panes(&t[i]) == 1)
        lay_init(&t[i].l, cw, ch, LAY_DIV_PIXEL, TABSLOT_MIN_COLS, TABSLOT_MIN_ROWS, x, y, w, h);
    else
        lay_solve(&t[i].l, x, y, w, h);
    if ((id = lay_split(&t[i].l, k, dir)) < 0)
        return -1;
    t[i].pane[id] = pane;
    t[i].focus = id;
    t[i].zoom = 0;
    return id;
}

/* The keys to the previous (step -1) or next (step +1) pane of pane's
 * slot in reading order (left to right, top to bottom: lay_save's order),
 * wrapping round at either end. Returns the slot, or -1: pane is in no
 * slot or its slot has one pane. A cycle ends a zoom. */
static int tabslot_cycle(tab_slot *t, int n, const void *pane, int step)
{
    int i, k, at, cnt, order[LAY_MAX_PANES];
    char buf[96];
    if ((i = tabslot_find(t, n, pane, &k)) < 0)
        return -1;
    if ((cnt = tabslot_panes(&t[i])) < 2 || lay_save(&t[i].l, buf, sizeof(buf), order) < 0)
        return -1;
    for (at = 0; at < cnt && order[at] != k; at++)
        ;
    if (at == cnt)
        return -1;
    t[i].focus = order[(at + (step < 0 ? cnt - 1 : 1)) % cnt];
    t[i].zoom = 0;
    return i;
}

/* Zoom pane's slot to pane (it has the keys), or back. Returns the slot,
 * or -1: pane is in no slot, or its slot has one pane. */
static int tabslot_zoom(tab_slot *t, int n, const void *pane)
{
    int i;
    if ((i = tabslot_find(t, n, pane, 0)) < 0 || tabslot_panes(&t[i]) < 2)
        return -1;
    t[i].zoom = !t[i].zoom;
    return i;
}

/* Take pane out. A slot with other panes keeps its index: the space goes
 * to the pane's sibling, which gets the focus when the pane had it. The
 * last pane of a slot takes the slot with it: the slots after it move
 * down one and *gone is 1. *active (the shown slot) stays on the same
 * slot when one before it went, and on the last when it was the last
 * that went. Returns the slot the pane was in, or -1 when it was in none. */
static int tabslot_remove(tab_slot *t, int *n, int *active, const void *pane, int *gone)
{
    int i, at, k, next;
    *gone = 0;
    if ((i = tabslot_find(t, *n, pane, &k)) < 0)
        return -1;
    if (tabslot_panes(&t[i]) > 1 && (next = lay_close(&t[i].l, k)) >= 0) {
        t[i].pane[k] = 0;
        if (t[i].focus == k)
            t[i].focus = next;
        t[i].zoom = 0;
        return i;
    }
    for (at = i; i < *n - 1; i++)
        t[i] = t[i + 1];
    (*n)--;
    *gone = 1;
    if (at < *active || *active >= *n)
        (*active)--;
    if (*active < 0 && *n)
        *active = 0;
    return at;
}

/* The tab bar's pens (TAB-FILLPEN). The active tab is the inverse of the
 * rest: TEXT pen ground with BACKGROUND pen letters, the others the other
 * way round. The DrawInfo FILLPEN is not used: the Amarchy flat pen map
 * sets FILLPEN = BACKGROUNDPEN, which made the active tab look like the
 * inactive ones. TEXT and BACKGROUND differ under the stock and the flat
 * map. pens is the DrawInfo array; text and background are its indices. */
enum { TABBAR_GROUND = 0, TABBAR_LETTERS = 1 };

static unsigned tabbar_pen(const unsigned short *pens, int text, int background, int active, int part)
{
    int ink = (part == TABBAR_GROUND) == (active != 0);
    return pens[ink ? text : background];
}

#endif
