/* amarchy-flat glyph geometry: see flat_glyph.h. */
#include "flat_glyph.h"

typedef struct plan {
    flat_prim *p;               /* store the rectangles here (flat_glyph_plan), or NULL to stream them to fill */
    flat_fill_fn fill;          /* flat_glyph_draw: each rectangle goes straight out, no array in the caller's frame */
    void *ctx;
    const flat_state *st;
    int ox, oy;
    int n, cap;
    int w, h, fg, bg;
    int frame_pen;              /* pen of FLAT_ROLE_FRAME, -1: none known (pen_back) */
    int ix0, iy0, ix1, iy1;     /* the inner box the glyph lives in */
} plan;

static void put(plan *g, int x0, int y0, int x1, int y1, int role)
{
    if (g->n >= g->cap || x1 < x0 || y1 < y0)
        return;
    if (!g->p) {                /* streamed in draw order, so the painter's order of the stored plan is kept */
        g->fill(g->ctx, g->ox + x0, g->oy + y0, g->ox + x1, g->oy + y1, role == FLAT_ROLE_FRAME && g->frame_pen >= 0 ? g->frame_pen : flat_glyph_pen(g->st, role));
        g->n++;
        return;
    }
    g->p[g->n].x0 = (int16_t)x0;
    g->p[g->n].y0 = (int16_t)y0;
    g->p[g->n].x1 = (int16_t)x1;
    g->p[g->n].y1 = (int16_t)y1;
    g->p[g->n].role = (uint8_t)role;
    g->n++;
}

/* a square or rectangle: one solid block (the only style: FLAT-SOLID-FILL) */
static void box(plan *g, int x0, int y0, int x1, int y1, int role)
{
    put(g, x0, y0, x1, y1, role);
}

static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

static void g_close(plan *g)
{
    int s = imax(3, imin(g->ix1 - g->ix0 + 1, g->iy1 - g->iy0 + 1) / 2 + 1);
    int cx = g->w / 2, cy = g->h / 2;
    int x0 = cx - s / 2, y0 = cy - s / 2;
    box(g, x0, y0, x0 + s - 1, y0 + s - 1, g->fg);
}

static void g_depth(plan *g)
{
    int iw = g->ix1 - g->ix0 + 1, ih = g->iy1 - g->iy0 + 1;
    int q = imax(3, imin(iw, ih) * 2 / 3);
    box(g, g->ix0, g->iy0, g->ix0 + q - 1, g->iy0 + q - 1, g->fg);
    put(g, g->ix1 - q + 1, g->iy1 - q + 1, g->ix1, g->iy1, g->bg);      /* the front square hides the back one */
    box(g, g->ix1 - q + 1, g->iy1 - q + 1, g->ix1, g->iy1, g->fg);
}

static void g_zoom(plan *g)
{
    int iw = g->ix1 - g->ix0 + 1, ih = g->iy1 - g->iy0 + 1;
    int q = imax(3, imin(iw, ih) / 2);
    box(g, g->ix0, g->iy0, g->ix1, g->iy1, g->fg);
    put(g, g->ix0 + 1, g->iy0 + 1, g->ix0 + q - 1, g->iy0 + q - 1, g->bg);
}

static void g_size(plan *g)
{
    int iw = g->ix1 - g->ix0 + 1, ih = g->iy1 - g->iy0 + 1;
    put(g, g->ix0 + iw / 2, g->iy0 + ih / 2, g->ix1, g->iy1, g->fg);
}

static void g_iconify(plan *g)
{
    put(g, g->ix0, g->iy1 - 1, g->ix1, g->iy1, g->fg);
}

static void g_check(plan *g)
{
    int ih = g->iy1 - g->iy0 + 1;
    int a = imax(2, ih / 3);
    int x = g->ix0, y = g->iy0 + ih / 2, k;
    for (k = 0; k < a; k++) {                           /* the short leg, down to the right */
        put(g, x + k, y + k, x + k, y + k + 1, g->fg);
    }
    for (k = 1; y + a - 1 - k >= g->iy0 && x + a - 1 + k <= g->ix1; k++) {     /* the long leg, up to the right */
        put(g, x + a - 1 + k, y + a - 1 - k, x + a - 1 + k, y + a - 1 - k + 1, g->fg);
    }
}

static void g_amigakey(plan *g)
{
    int cy = (g->iy0 + g->iy1) / 2;
    box(g, g->ix0, g->iy0, g->ix1, g->iy1, g->fg);
    put(g, g->ix0 + 1, cy, g->ix1 - 1, cy, g->bg);
}

static void g_menusub(plan *g)
{
    int iw = g->ix1 - g->ix0 + 1, ih = g->iy1 - g->iy0 + 1, k;
    for (k = 0; k < ih; k++) {                          /* a right-pointing triangle, widest on the middle rows */
        int len = imin(imin(k, ih - 1 - k) + 1, iw);
        put(g, g->ix0, g->iy0 + k, g->ix0 + len - 1, g->iy0 + k, g->fg);
    }
}

int flat_glyph_known(unsigned long which)
{
    switch (which) {
    case FLAT_W_DEPTH: case FLAT_W_ZOOM: case FLAT_W_SIZE: case FLAT_W_CLOSE: case FLAT_W_MENUCHECK:
    case FLAT_W_AMIGAKEY: case FLAT_W_ICONIFY: case FLAT_W_MENUSUB:
        return 1;
    default:
        return 0;
    }
}

int flat_glyph_state(unsigned long ids)
{
    switch (ids) {
    case 1: return FLAT_G_SELECTED;                     /* IDS_SELECTED */
    case 2: case 5: case 6: case 7: return FLAT_G_INACTIVE;     /* IDS_DISABLED, IDS_INACTIVE* */
    default: return FLAT_G_NORMAL;
    }
}

/* Both entry points run the same generator; g carries either the array or the fill callback. */
static int plan_run(plan *g, unsigned long which, int w, int h, int state)
{
    int i;
    g->n = 0; g->w = w; g->h = h;
    g->fg = state == FLAT_G_SELECTED ? FLAT_ROLE_BACK : state == FLAT_G_INACTIVE ? FLAT_ROLE_LINE : FLAT_ROLE_TEXT;
    g->bg = state == FLAT_G_SELECTED ? FLAT_ROLE_TEXT : FLAT_ROLE_BACK;
    if (state != FLAT_G_SELECTED && (which == FLAT_W_CLOSE || which == FLAT_W_DEPTH || which == FLAT_W_ZOOM ||
                                     which == FLAT_W_SIZE || which == FLAT_W_ICONIFY))
        g->bg = FLAT_ROLE_FRAME;        /* the gadget box takes the frame's colour: only the glyph lines show */
    i = imax(1, imin(w, h) / 4);
    g->ix0 = i; g->iy0 = i; g->ix1 = w - 1 - i; g->iy1 = h - 1 - i;
    put(g, 0, 0, w - 1, h - 1, g->bg);
    switch (which) {
    case FLAT_W_CLOSE: g_close(g); break;
    case FLAT_W_DEPTH: g_depth(g); break;
    case FLAT_W_ZOOM: g_zoom(g); break;
    case FLAT_W_SIZE: g_size(g); break;
    case FLAT_W_ICONIFY: g_iconify(g); break;
    case FLAT_W_MENUCHECK: g_check(g); break;
    case FLAT_W_AMIGAKEY: g_amigakey(g); break;
    case FLAT_W_MENUSUB: g_menusub(g); break;
    }
    return g->n;
}

int flat_glyph_plan(unsigned long which, int w, int h, int state, flat_prim *out, int cap)
{
    plan g;
    if (!out || cap < FLAT_GLYPH_MAX_PRIMS || !flat_glyph_known(which) || w < FLAT_GLYPH_MIN_SIZE || h < FLAT_GLYPH_MIN_SIZE)
        return 0;
    g.p = out; g.cap = cap; g.fill = 0; g.ctx = 0; g.st = 0; g.frame_pen = -1; g.ox = g.oy = 0;
    return plan_run(&g, which, w, h, state);
}

int flat_glyph_pen(const flat_state *s, int role)
{
    return role == FLAT_ROLE_TEXT ? s->pen_text : role == FLAT_ROLE_LINE ? s->pen_line : s->pen_back;
}

int flat_glyph_route(const flat_state *s, unsigned long which, unsigned long method)
{
    return s && s->enabled && !s->held && method == FLAT_IM_DRAW && flat_glyph_known(which);
}

int flat_glyph_draw(const flat_state *s, unsigned long which, unsigned long method, unsigned long ids, int ox, int oy,
                    int w, int h, int frame_pen, flat_fill_fn fill, void *ctx)
{
    plan g;
    if (!fill || !flat_glyph_route(s, which, method) || w < FLAT_GLYPH_MIN_SIZE || h < FLAT_GLYPH_MIN_SIZE)
        return 0;
    g.p = 0; g.cap = FLAT_GLYPH_MAX_PRIMS; g.fill = fill; g.ctx = ctx; g.st = s; g.frame_pen = frame_pen; g.ox = ox; g.oy = oy;
    return plan_run(&g, which, w, h, flat_glyph_state(ids)) > 0;
}

int flat_frame_draw(const flat_state *s, int ids, int ox, int oy, int w, int h, int fill, flat_fill_fn fn, void *ctx)
{
    int x1, y1;
    if (!s || !fn || w < 3 || h < 3)
        return 0;
    x1 = ox + w - 1;
    y1 = oy + h - 1;
    if (fill)
        fn(ctx, ox, oy, x1, y1, ids == 1 ? s->pen_line : s->pen_back);
    fn(ctx, ox, oy, x1, oy, s->pen_line);            /* top */
    fn(ctx, ox, y1, x1, y1, s->pen_line);            /* bottom */
    fn(ctx, ox, oy, ox, y1, s->pen_line);            /* left */
    fn(ctx, x1, oy, x1, y1, s->pen_line);            /* right */
    return 1;
}
