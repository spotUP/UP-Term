/* The pure part of the win: kind (win_core.h). */
#include <string.h>
#include "win_core.h"

static int word(const char *w, int n, const char *const *names, const win_arg *vals, win_arg *out)
{
    int i;
    for (i = 0; names[i]; i++)
        if ((int)strlen(names[i]) == n && !strncmp(w, names[i], (size_t)n)) {
            *out = vals[i];
            return 1;
        }
    return 0;
}

static int digits(const char *p, int *out)
{
    int n, v = 0;
    for (n = 0; p[n]; n++) {
        if (p[n] < '0' || p[n] > '9')
            return 0;
        v = v * 10 + (p[n] - '0');
    }
    if (n < 1 || n > 4 || v < 1)
        return 0;
    *out = v;
    return 1;
}

int win_parse(const char *text, win_cmd *c)
{
    static const struct { const char *name; win_verb v; } bare[] = {
        { "front", WV_FRONT }, { "back", WV_BACK }, { "zip", WV_ZIP }, { "fullwidth", WV_FULLWIDTH },
        { "fullscreen", WV_FULLSCREEN }, { "save", WV_SAVE }, { "restore", WV_RESTORE },
        { "closedrawers", WV_CLOSEDRAWERS }
    };
    static const struct { const char *name; win_verb v; } withArg[] = {
        { "cycle", WV_CYCLE }, { "move", WV_MOVE }, { "size", WV_SIZE }, { "focus", WV_FOCUS },
        { "screen", WV_SCREEN }, { "title", WV_TITLE }, { "snap", WV_SNAP }
    };
    static const char *const d4[] = { "left", "right", "up", "down", 0 };
    static const win_arg v4[] = { WA_LEFT, WA_RIGHT, WA_UP, WA_DOWN };
    static const char *const d6[] = { "left", "right", "up", "down", "next", "prev", 0 };
    static const win_arg v6[] = { WA_LEFT, WA_RIGHT, WA_UP, WA_DOWN, WA_NEXT, WA_PREV };
    static const char *const cy[] = { "next", "prev", 0 };
    static const win_arg vcy[] = { WA_NEXT, WA_PREV };
    static const char *const sc[] = { "next", "prev", "workbench", "back", 0 };
    static const win_arg vsc[] = { WA_NEXT, WA_PREV, WA_WORKBENCH, WA_BACK };
    static const char *const ti[] = { "toggle", "show", "hide", 0 };
    static const win_arg vti[] = { WA_TOGGLE, WA_SHOW, WA_HIDE };
    static const char *const sn[] = { "left", "right", "up", "down", "tl", "tr", "bl", "br", "center", 0 };
    static const win_arg vsn[] = { WA_LEFT, WA_RIGHT, WA_UP, WA_DOWN, WA_TL, WA_TR, WA_BL, WA_BR, WA_CENTER };
    const char *sp, *arg, *end;
    int i, vn;
    memset(c, 0, sizeof *c);
    if (!text)
        return 0;
    sp = strchr(text, ' ');
    if (!sp) {
        for (i = 0; i < (int)(sizeof bare / sizeof bare[0]); i++)
            if (!strcmp(text, bare[i].name)) {
                c->verb = bare[i].v;
                return 1;
            }
        return 0;
    }
    vn = (int)(sp - text);
    arg = sp + 1;
    end = strchr(arg, ' ');
    for (i = 0; i < (int)(sizeof withArg / sizeof withArg[0]); i++)
        if ((int)strlen(withArg[i].name) == vn && !strncmp(text, withArg[i].name, (size_t)vn))
            break;
    if (i == (int)(sizeof withArg / sizeof withArg[0]))
        return 0;
    c->verb = withArg[i].v;
    if (c->verb == WV_MOVE || c->verb == WV_SIZE) {
        c->pixels = WIN_STEP;
        if (end) {
            if (!word(arg, (int)(end - arg), d4, v4, &c->arg) || !digits(end + 1, &c->pixels))
                goto bad;
            return 1;
        }
        if (!word(arg, (int)strlen(arg), d4, v4, &c->arg))
            goto bad;
        return 1;
    }
    if (end)
        goto bad;
    if ((c->verb == WV_CYCLE && word(arg, (int)strlen(arg), cy, vcy, &c->arg)) ||
        (c->verb == WV_FOCUS && word(arg, (int)strlen(arg), d6, v6, &c->arg)) ||
        (c->verb == WV_SCREEN && word(arg, (int)strlen(arg), sc, vsc, &c->arg)) ||
        (c->verb == WV_TITLE && word(arg, (int)strlen(arg), ti, vti, &c->arg)) ||
        (c->verb == WV_SNAP && word(arg, (int)strlen(arg), sn, vsn, &c->arg)))
        return 1;
bad:
    memset(c, 0, sizeof *c);
    return 0;
}

static int clamp(int v, int lo, int hi)
{
    if (v > hi)
        v = hi;
    if (v < lo)
        v = lo;
    return v;
}

int win_move(win_box *b, win_arg dir, int pixels, const win_screen *s)
{
    int l = b->left, t = b->top;
    switch (dir) {
    case WA_LEFT:  l -= pixels; break;
    case WA_RIGHT: l += pixels; break;
    case WA_UP:    t -= pixels; break;
    case WA_DOWN:  t += pixels; break;
    default: return 0;
    }
    l = clamp(l, 0, s->width - b->width);
    t = clamp(t, 0, s->height - s->bottom - b->height);   /* stops at the bar */
    if (l == b->left && t == b->top)
        return 0;
    b->left = l;
    b->top = t;
    return 1;
}

/* a limit of one dimension: the window's own Min / Max, and the room the screen leaves from the window's edge */
int win_fit(int v, int min, int max, int room)
{
    if (max > 0 && v > max)
        v = max;
    if (v > room)
        v = room;
    if (v < min)
        v = min;
    return v < 1 ? 1 : v;
}

int win_size(win_box *b, win_arg dir, int pixels, const win_screen *s)
{
    int w = b->width, h = b->height;
    switch (dir) {
    case WA_LEFT:  w -= pixels; break;
    case WA_RIGHT: w += pixels; break;
    case WA_UP:    h -= pixels; break;
    case WA_DOWN:  h += pixels; break;
    default: return 0;
    }
    w = win_fit(w, b->minw, b->maxw, s->width - b->left);
    h = win_fit(h, b->minh, b->maxh, s->height - b->top);
    if (w == b->width && h == b->height)
        return 0;
    b->width = w;
    b->height = h;
    return 1;
}

int win_fullwidth(win_box *b, const win_screen *s)
{
    int w = win_fit(s->width, b->minw, b->maxw, s->width);
    if (b->left == 0 && b->width == w)
        return 0;
    b->left = 0;
    b->width = w;
    return 1;
}

void win_work_area(const win_screen *s, int gap, int *x0, int *y0, int *w, int *h)
{
    *x0 = gap;
    *y0 = s->title + gap;
    *w = s->width - 2 * gap;
    *h = s->height - s->title - s->bottom - 2 * gap;
}

int win_strut(const win_box *b, const win_screen *s)
{
    if (b->left == 0 && b->width == s->width && b->top > 0 && b->top + b->height == s->height)
        return b->height;
    return 0;
}

int win_is_amarchy(const char *title)
{
    return title && (!strcmp(title, "Amarchy") || !strcmp(title, "Amarchy bar"));
}

int win_snap(win_box *b, win_arg arg, int gap, const win_screen *s)
{
    int x0, y0, w, h, cw, ch, x, y, nw, nh;
    int col = 0, row = 0, ncol = 1, nrow = 1;      /* the zone: first cell, cell counts */
    win_work_area(s, gap, &x0, &y0, &w, &h);
    cw = (w - gap) / 2;
    ch = (h - gap) / 2;
    switch (arg) {
    case WA_LEFT:   ncol = 2; break;
    case WA_RIGHT:  ncol = 2; col = 1; break;
    case WA_UP:     nrow = 2; break;
    case WA_DOWN:   nrow = 2; row = 1; break;
    case WA_TL:     ncol = nrow = 2; break;
    case WA_TR:     ncol = nrow = 2; col = 1; break;
    case WA_BL:     ncol = nrow = 2; row = 1; break;
    case WA_BR:     ncol = nrow = 2; col = row = 1; break;
    case WA_CENTER: break;
    default: return 0;
    }
    if (arg == WA_CENTER) {
        nw = win_fit(b->width, b->minw, b->maxw, w);
        nh = win_fit(b->height, b->minh, b->maxh, h);
        x = x0 + (w - nw) / 2;
        y = y0 + (h - nh) / 2;
    } else {
        x = ncol == 2 && col ? x0 + cw + gap : x0;
        y = nrow == 2 && row ? y0 + ch + gap : y0;
        nw = ncol == 2 ? (col ? w - gap - cw : cw) : w;
        nh = nrow == 2 ? (row ? h - gap - ch : ch) : h;
        nw = win_fit(nw, b->minw, b->maxw, s->width - x);
        nh = win_fit(nh, b->minh, b->maxh, s->height - s->bottom - y);
    }
    if (b->left == x && b->top == y && b->width == nw && b->height == nh)
        return 0;
    b->left = x;
    b->top = y;
    b->width = nw;
    b->height = nh;
    return 1;
}

int win_fullscreen(win_box *b, const win_screen *s)
{
    int w = win_fit(s->width, b->minw, b->maxw, s->width);
    int h = win_fit(s->height - s->title - s->bottom, b->minh, b->maxh, s->height - s->title - s->bottom);
    if (b->left == 0 && b->top == s->title && b->width == w && b->height == h)
        return 0;
    b->left = 0;
    b->top = s->title;
    b->width = w;
    b->height = h;
    return 1;
}

int win_is_fullscreen_terminal(int backdrop_borderless, int left, int top, int width, int height, const win_screen *s)
{
    return backdrop_borderless && left == 0 && top == 0 && width == s->width && height == s->height;
}

void win_ring_clear(win_ring *r)
{
    memset(r, 0, sizeof *r);
}

void win_save(win_ring *r, long id, const win_box *b)
{
    int i;
    for (i = 0; i < r->n; i++)
        if (r->id[i] == id)
            break;
    if (i == r->n && r->n == WIN_RING_MAX)
        i = 0;                                  /* full: the oldest goes */
    else if (i == r->n)
        r->n++;
    for (; i < r->n - 1; i++) {                 /* close the gap: the newest sits last */
        r->id[i] = r->id[i + 1];
        r->box[i] = r->box[i + 1];
    }
    r->id[r->n - 1] = id;
    r->box[r->n - 1] = *b;
}

int win_restore(const win_ring *r, long id, win_box *out)
{
    int i;
    for (i = 0; i < r->n; i++)
        if (r->id[i] == id) {
            *out = r->box[i];
            return 1;
        }
    return 0;
}

int win_pick(const win_box *boxes, int n, int target, win_arg dir)
{
    int i, best = -1, bestd = 0;
    long tx, ty;
    if (target < 0 || target >= n)
        return -1;
    tx = 2L * boxes[target].left + boxes[target].width;      /* twice the centre: stays integer */
    ty = 2L * boxes[target].top + boxes[target].height;
    if (dir == WA_NEXT || dir == WA_PREV) {
        int step = dir == WA_NEXT ? 1 : n - 1;
        for (i = (target + step) % n; i != target; i = (i + step) % n)
            if (boxes[i].screen == boxes[target].screen)
                return i;
        return -1;
    }
    for (i = 0; i < n; i++) {
        long dx, dy, main, side, d;
        if (i == target || boxes[i].screen != boxes[target].screen)
            continue;
        dx = 2L * boxes[i].left + boxes[i].width - tx;
        dy = 2L * boxes[i].top + boxes[i].height - ty;
        switch (dir) {
        case WA_LEFT:  main = -dx; side = dy; break;
        case WA_RIGHT: main = dx;  side = dy; break;
        case WA_UP:    main = -dy; side = dx; break;
        case WA_DOWN:  main = dy;  side = dx; break;
        default: return -1;
        }
        if (main <= 0)
            continue;
        if (side < 0)
            side = -side;
        d = (int)(main + 2 * side);                 /* a window straight ahead beats a nearer one off to the side */
        if (best < 0 || d < bestd) {
            best = i;
            bestd = (int)d;
        }
    }
    return best;
}

int win_ticket_valid(int usable, const win_box *taken, const win_box *now)
{
    return usable && taken->left == now->left && taken->top == now->top && taken->width == now->width &&
           taken->height == now->height;
}
