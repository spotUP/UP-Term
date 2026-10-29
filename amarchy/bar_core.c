/* The Amarchy bar, the portable core (bar_core.h). */
#include <string.h>
#include "bar_core.h"

void bar_init(bar *b, int width, int pad, int gap)
{
    memset(b, 0, sizeof *b);
    b->width = width;
    b->pad = pad;
    b->gap = gap;
}

static void put_text(bar_seg *s, const char *text, int tw)
{
    strncpy(s->text, text ? text : "", BAR_TEXT - 1);
    s->text[BAR_TEXT - 1] = 0;
    s->tw = tw;
}

int bar_add(bar *b, int id, int side, const char *text, int tw)
{
    bar_seg *s;
    if (b->n >= BAR_MAX_SEGS)
        return -1;
    s = &b->seg[b->n];
    memset(s, 0, sizeof *s);
    s->id = id;
    s->side = side;
    put_text(s, text, tw);
    s->dirty = 1;
    return b->n++;
}

int bar_set_text(bar *b, int id, const char *text, int tw)
{
    int i;
    for (i = 0; i < b->n; i++) {
        bar_seg *s = &b->seg[i];
        if (s->id != id)
            continue;
        if (s->tw == tw && !strncmp(s->text, text ? text : "", BAR_TEXT - 1))
            return 0;
        put_text(s, text, tw);
        s->dirty = 1;
        return 1;
    }
    return 0;
}

static void place(bar_seg *s, int x, int w)
{
    if (s->x != x || s->w != w)
        s->dirty = 1;
    s->x = x;
    s->w = w;
}

void bar_layout(bar *b, int width)
{
    int i, x, r;
    b->width = width;
    x = 0;
    for (i = 0; i < b->n; i++) {
        bar_seg *s = &b->seg[i];
        if (s->side != BAR_LEFT)
            continue;
        place(s, x, s->tw + 2 * b->pad);
        x += s->w + b->gap;
    }
    r = width;
    for (i = b->n - 1; i >= 0; i--) {
        bar_seg *s = &b->seg[i];
        if (s->side != BAR_RIGHT)
            continue;
        place(s, r - (s->tw + 2 * b->pad), s->tw + 2 * b->pad);
        r = s->x - b->gap;
    }
}

int bar_hit(const bar *b, int x)
{
    int i;
    for (i = 0; i < b->n; i++)
        if (x >= b->seg[i].x && x < b->seg[i].x + b->seg[i].w)
            return i;
    return -1;
}

int bar_click(const bar *b, int x, int button, int *id)
{
    int i = bar_hit(b, x);
    if (id)
        *id = 0;
    if (i < 0)
        return BAR_ACT_NONE;
    if (id)
        *id = b->seg[i].id;
    switch (b->seg[i].id) {
    case BAR_SEG_AGENTS:
        if (button == BAR_BTN_LEFT)
            return BAR_ACT_PANEL;
        if (button == BAR_BTN_RIGHT)
            return BAR_ACT_LAUNCH_AGENT;
        break;
    }
    return BAR_ACT_NONE;
}

int bar_press_act(const bar *b, int px, int py, int top, int h, int button)
{
    if (py < top || py >= top + h || px < 0 || px >= b->width)
        return BAR_ACT_NONE;
    return bar_click(b, px, button, 0);
}

int bar_take_dirty(bar *b, int from)
{
    int i;
    for (i = from < 0 ? 0 : from; i < b->n; i++)
        if (b->seg[i].dirty) {
            b->seg[i].dirty = 0;
            return i;
        }
    return -1;
}

void bar_panel_anchor(int screen_w, int bar_top, int *ax, int *ay)
{
    *ax = screen_w;
    *ay = bar_top;
}

int bar_covered(int l, int t, int r, int b, const bar_lyr *front, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        const bar_lyr *f = &front[i];
        if (f->ours)
            continue;
        if (f->l <= r && f->r >= l && f->t <= b && f->b >= t)
            return 1;
    }
    return 0;
}
