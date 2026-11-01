/* amarchy-flat glyph geometry: see flat_glyph.h. */
#include <string.h>
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


/* Both entry points and the clone path take their roles from here: one definition of "glyph pen" and "box pen". */
void flat_glyph_roles(unsigned long which, int state, int *glyph_role, int *box_role)
{
    int fg = state == FLAT_G_SELECTED ? FLAT_ROLE_BACK : state == FLAT_G_INACTIVE ? FLAT_ROLE_LINE : FLAT_ROLE_TEXT;
    int bg = state == FLAT_G_SELECTED ? FLAT_ROLE_TEXT : FLAT_ROLE_BACK;
    if (state != FLAT_G_SELECTED && flat_glyph_window_gadget(which))
        bg = FLAT_ROLE_FRAME;           /* the gadget box takes the frame's colour: only the glyph lines show */
    *glyph_role = fg;
    *box_role = bg;
}

int flat_glyph_window_gadget(unsigned long which)
{
    return which == FLAT_W_CLOSE || which == FLAT_W_DEPTH || which == FLAT_W_ZOOM || which == FLAT_W_SIZE || which == FLAT_W_ICONIFY ||
           which == FLAT_W_LEFT || which == FLAT_W_UP || which == FLAT_W_RIGHT || which == FLAT_W_DOWN;
}

int flat_glyph_on_band(unsigned long which)
{
    return which == FLAT_W_CLOSE || which == FLAT_W_DEPTH || which == FLAT_W_ZOOM || which == FLAT_W_ICONIFY;
}

int flat_clone_route(const flat_state *s, unsigned long which, unsigned long method)
{
    return s && s->enabled && !s->held && method == FLAT_IM_DRAW && flat_glyph_window_gadget(which);
}

/* ---- FLAT-GLYPH-OS-SHAPES: the clone. The system's own image is drawn offscreen with marker pens; this is the pure part. */
int flat_clone_map(const uint8_t *grid, int w, int h, uint8_t *out)
{
    int x, y, n = 0, i, best = 0, cnt[FLAT_CLONE_MARKS + 1];
    int bgm;
    if (!grid || !out || w < 3 || h < 3)
        return -1;
    for (i = 0; i <= FLAT_CLONE_MARKS; i++)
        cnt[i] = 0;
    for (y = 1; y < h - 1; y++)
        for (x = 1; x < w - 1; x++)
            if (grid[y * w + x] <= FLAT_CLONE_MARKS)
                cnt[grid[y * w + x]]++;
    bgm = FLAT_CLONE_BACK;
    for (i = 0; i <= FLAT_CLONE_MARKS; i++)     /* the pen that fills most of the inside is the gadget's own ground */
        if (cnt[i] > best) {
            best = cnt[i];
            bgm = i;
        }
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int m = grid[y * w + x], ring = x == 0 || y == 0 || x == w - 1 || y == h - 1;
            int glyph;
            if (m == 0 || m == bgm)
                glyph = 0;                       /* untouched or ground */
            else if (ring && (m == FLAT_CLONE_SHINE || m == FLAT_CLONE_SHADOW || m == FLAT_CLONE_BACK))
                glyph = 0;                       /* the bevel ring */
            else
                glyph = 1;
            out[y * w + x] = (uint8_t)glyph;
            n += glyph;
        }
    return n;
}

int flat_clone_pen(const flat_state *s, int role, int frame_pen)
{
    return role == FLAT_ROLE_FRAME && frame_pen >= 0 ? frame_pen : flat_glyph_pen(s, role);
}

int flat_clone_draw(const flat_state *s, unsigned long which, unsigned long method, unsigned long ids, int ox, int oy,
                    int w, int h, int frame_pen, int text_pen, flat_render_fn render, void *rctx, flat_fill_fn fill, void *fctx, uint8_t *scratch)
{
    uint8_t *grid = scratch, *map = scratch + FLAT_CLONE_MAX * FLAT_CLONE_MAX;
    int state, grole, brole, gpen, bpen, x, y;
    if (!fill || !render || !scratch || !flat_clone_route(s, which, method) ||
        w < FLAT_GLYPH_MIN_SIZE || h < FLAT_GLYPH_MIN_SIZE || w > FLAT_CLONE_MAX || h > FLAT_CLONE_MAX)
        return FLAT_CLONE_NO;
    state = flat_glyph_state(ids);
    memset(grid, 0, (size_t)(w * h));
    if (!render(rctx, ids, grid, w, h) || flat_clone_map(grid, w, h, map) < 0)
        return FLAT_CLONE_FAIL;
    flat_glyph_roles(which, state, &grole, &brole);
    gpen = grole == FLAT_ROLE_TEXT && text_pen >= 0 ? text_pen : flat_clone_pen(s, grole, frame_pen);
    bpen = flat_clone_pen(s, brole, frame_pen);
    fill(fctx, ox, oy, ox + w - 1, oy + h - 1, bpen);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            if (map[y * w + x]) {
                int x1 = x;
                while (x1 + 1 < w && map[y * w + x1 + 1])
                    x1++;
                fill(fctx, ox + x, oy + y, ox + x1, oy + y, gpen);
                x = x1;
            }
    return FLAT_CLONE_OK;
}

/* Both entry points run the same generator; g carries either the array or the fill callback. */
static int plan_run(plan *g, unsigned long which, int w, int h, int state)
{
    int i;
    g->n = 0; g->w = w; g->h = h;
    flat_glyph_roles(which, state, &g->fg, &g->bg);
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

int flat_frame_draw(const flat_state *s, int ids, int ox, int oy, int w, int h, int fill, int role, flat_fill_fn fn, void *ctx)
{
    int x1, y1;
    if (!s || !fn || w < 3 || h < 3)
        return 0;
    x1 = ox + w - 1;
    y1 = oy + h - 1;
    if (fill && ids != 1 && role == FLAT_ROLE_SECONDARY) {   /* BUTTON-ROLES: the secondary button is a 1 px text-pen outline, no fill */
        fn(ctx, ox, oy, x1, y1, s->pen_back);        /* clears a previous pressed fill */
        fn(ctx, ox, oy, x1, oy, s->pen_text);
        fn(ctx, ox, y1, x1, y1, s->pen_text);
        fn(ctx, ox, oy, ox, y1, s->pen_text);
        fn(ctx, x1, oy, x1, y1, s->pen_text);
        return 1;
    }
    if (fill) {                                      /* FLAT-BUTTON-FILL: primary fill (flat_button_pen), accent when pressed, no outline */
        fn(ctx, ox, oy, x1, y1, ids == 1 ? s->pen_line : (s->pen_button ? s->pen_button : s->pen_back));
        return 1;
    }
    fn(ctx, ox, oy, x1, oy, s->pen_line);            /* top */
    fn(ctx, ox, y1, x1, y1, s->pen_line);            /* bottom */
    fn(ctx, ox, oy, ox, y1, s->pen_line);            /* left */
    fn(ctx, x1, oy, x1, y1, s->pen_line);            /* right */
    return 1;
}

int flat_button_corners(int w, int h, int out[][4])
{
    int c, n = 0, tall = h >= FLAT_ROUND_TALL;
    if (w < 3 || h < 3)
        return 0;
    for (c = 0; c < 4; c++) {
        int x = (c & 1) ? w - 1 : 0, y = (c & 2) ? h - 1 : 0;
        int ix = (c & 1) ? -1 : 1, iy = (c & 2) ? -1 : 1;           /* direction into the box */
        out[n][0] = x; out[n][1] = y; out[n][2] = x - ix; out[n][3] = y - iy; n++;
        if (tall) {
            out[n][0] = x + ix; out[n][1] = y; out[n][2] = x + ix; out[n][3] = y - iy; n++;     /* beside the corner, along the row */
            out[n][0] = x; out[n][1] = y + iy; out[n][2] = x - ix; out[n][3] = y + iy; n++;     /* beside the corner, down the column */
        }
    }
    return n;
}

int flat_frame_draw_round(const flat_state *s, int ids, int ox, int oy, int w, int h, int fill, int role, flat_fill_fn fn,
                          flat_read_fn rd, void *ctx)
{
    int cut[FLAT_ROUND_MAX][4], pen[FLAT_ROUND_MAX], n = 0, i;
    if (!s || !fn || w < 3 || h < 3)
        return 0;
    if (rd && fill) {
        n = flat_button_corners(w, h, cut);
        for (i = 0; i < n; i++)
            pen[i] = rd(ctx, ox + cut[i][2], oy + cut[i][3]);        /* before the box is painted */
    }
    if (!flat_frame_draw(s, ids, ox, oy, w, h, fill, role, fn, ctx))
        return 0;
    for (i = 0; i < n; i++)
        if (pen[i] >= 0)
            fn(ctx, ox + cut[i][0], oy + cut[i][1], ox + cut[i][0], oy + cut[i][1], pen[i]);
    return 1;
}

/* The one definition of the four outer-edge strips (window-relative), in the shapes Intuition's own border redraw uses
 * (measured on 3.1 and 3.2): top (0,0)-(w-1,0), left (0,1)-(0,h-1), right (w-1,1)-(w-1,h-2), bottom (1,h-1)-(w-1,h-1).
 * Together they cover the ring once per pixel. Returns 0 when strip i does not exist (right strip of a 2 row window). */
static int outer_strip(int i, int w, int h, int r[4])
{
    switch (i) {
    case 0: r[0] = 0;     r[1] = 0;     r[2] = w - 1; r[3] = 0;     return 1;
    case 1: r[0] = 0;     r[1] = 1;     r[2] = 0;     r[3] = h - 1; return 1;
    case 2: r[0] = w - 1; r[1] = 1;     r[2] = w - 1; r[3] = h - 2; return h > 2;
    default: r[0] = 1;    r[1] = h - 1; r[2] = w - 1; r[3] = h - 1; return 1;
    }
}

int flat_outline_rects(const flat_state *s, int w, int h, flat_fill_fn fn, void *ctx)
{
    int i, r[4];
    if (!s || !fn || !s->pen_outline || w < 2 || h < 2)
        return 0;
    for (i = 0; i < 4; i++)
        if (outer_strip(i, w, h, r))
            fn(ctx, r[0], r[1], r[2], r[3], s->pen_outline);
    return 4;
}

int flat_outer_edge_match(int w, int h, int x0, int y0, int x1, int y1)
{
    int i, r[4];
    if (w < 2 || h < 2)
        return 0;
    for (i = 0; i < 4; i++)
        if (outer_strip(i, w, h, r) && r[0] == x0 && r[1] == y0 && r[2] == x1 && r[3] == y1)
            return 1;
    return 0;
}
