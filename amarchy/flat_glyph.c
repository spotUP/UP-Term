/* amarchy-flat glyph geometry: see flat_glyph.h. */
#include <string.h>
#include "flat_glyph.h"

typedef struct plan {
    flat_prim *p;               /* store the rectangles here (flat_glyph_plan), or NULL to stream them to fill */
    flat_fill_fn fill;          /* flat_glyph_draw: each rectangle goes straight out, no array in the caller's frame */
    void *ctx;
    const flat_state *st;
    /* S62-FLAT-RF-TX-BUDGET: shorts, not ints. flat_glyph_draw keeps a plan in its frame on the drawing task's stack; every
     * value here is a gadget coordinate, a pen (-1 .. 255), a role or a count up to FLAT_GLYPH_MAX_PRIMS. */
    short ox, oy;
    short n, cap;
    short w, h, fg, bg;
    short frame_pen;            /* pen of FLAT_ROLE_FRAME, -1: none known (pen_back) */
    short knob_pen;             /* pen of FLAT_ROLE_KNOB, -1: none known (pen_line) */
    short ix0, iy0, ix1, iy1;   /* the inner box the glyph lives in */
    short side;                 /* flat_side_px of the screen: columns per row of visual height (1 square, 2 hires, 4 superhires) */
} plan;

static void put(plan *g, int x0, int y0, int x1, int y1, int role)
{
    if (g->n >= g->cap || x1 < x0 || y1 < y0)
        return;
    if (!g->p) {                /* streamed in draw order, so the painter's order of the stored plan is kept */
        g->fill(g->ctx, g->ox + x0, g->oy + y0, g->ox + x1, g->oy + y1, flat_clone_pen(g->st, role, g->frame_pen, g->knob_pen));
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

/* The scroller arrows: a solid triangle in the inner box, base on the side the arrow comes from. OS 3.2's sysiclass puts nothing
 * into the clone's offscreen rastport for LEFTIMAGE..DOWNIMAGE, and the stock draw under flat pens (shine == shadow == background)
 * leaves glyph and box in one colour, so the arrows get their own block glyph, in the same roles as every window gadget.
 * S31-ARROW-ASPECT: all four arrows have the same shape on screen, sides at 45 degrees (base twice the length), measured in visual
 * units where a column is 1 and a row is g->side (flat_side_px: 2 on hires, whose pixels are twice as tall as wide). Left/right
 * grow g->side columns per row, up/down 2 * g->side columns per row; the triangle is centred in the inner box. Before this, hires
 * showed left/right half as wide and up/down twice as tall as on a square-pixel screen. */
static void g_arrow(plan *g, unsigned long which)
{
    int iw = g->ix1 - g->ix0 + 1, ih = g->iy1 - g->iy0 + 1, k;
    if (which == FLAT_W_LEFT || which == FLAT_W_RIGHT) {
        int lmax = imin(iw, g->side * ((ih + 1) / 2)), xs = g->ix0 + (iw - lmax) / 2;
        for (k = 0; k < ih; k++) {
            int len = imin(lmax, g->side * (imin(k, ih - 1 - k) + 1));
            if (which == FLAT_W_RIGHT)
                put(g, xs, g->iy0 + k, xs + len - 1, g->iy0 + k, g->fg);
            else
                put(g, xs + lmax - len, g->iy0 + k, xs + lmax - 1, g->iy0 + k, g->fg);
        }
    } else {
        /* Up sits on the bottom of its box, down on the top: the up gadget is above the down gadget, so on hires (fewer
         * rows) the pair keeps the gap a full-height glyph pair has instead of growing apart (S31-ARROW-GAP). */
        int n = imin(ih, (iw - 1) / (2 * g->side) + 1), ys = which == FLAT_W_UP ? g->iy1 - n + 1 : g->iy0;
        for (k = 0; k < n; k++) {
            int row = which == FLAT_W_UP ? k : n - 1 - k;
            int len = imin(iw, 2 * g->side * row + 1), x0 = g->ix0 + (iw - len) / 2;
            put(g, x0, ys + k, x0 + len - 1, ys + k, g->fg);
        }
    }
}

int flat_glyph_known(unsigned long which)
{
    switch (which) {
    case FLAT_W_LEFT: case FLAT_W_UP: case FLAT_W_RIGHT: case FLAT_W_DOWN:
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
    if (flat_glyph_knob_gadget(which)) {            /* S32-ARROW-KNOB-COLOUR: arrows and size: knob pen, never the text pen */
        fg = state == FLAT_G_SELECTED ? FLAT_ROLE_BACK : FLAT_ROLE_KNOB;
        bg = state == FLAT_G_SELECTED ? FLAT_ROLE_KNOB : FLAT_ROLE_FRAME;
    }
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

int flat_clone_slot(const flat_clone_key *keys, int n, int cap, unsigned long which, int w, int h)
{
    int i;
    for (i = 0; i < n; i++)
        if (keys[i].which == which && keys[i].w == w && keys[i].h == h)
            return i;
    return n < cap ? n : -1;
}

int flat_clone_route(const flat_state *s, unsigned long which, unsigned long method)
{
    return s && s->enabled && !s->held && method == FLAT_IM_DRAW && flat_glyph_window_gadget(which);
}

/* FLAT-NO-DIVIDERS: is line k (a column when col, else a row) a stock divider / border line? It is when every pixel of it between
 * the two outer rings is drawn and none is the gadget's ground: a glyph never spans the whole box, a bevel edge and the 2 px
 * title bar separator (OS 3.2 close: shadow + shine columns 18-19 of 20, in whatever pens the system used) always do. */
static int edge_line(const uint8_t *grid, int w, int h, int bgm, int k, int col)
{
    int i, n = col ? h : w;
    for (i = 1; i < n - 1; i++) {
        int m = col ? grid[i * w + k] : grid[k * w + i];
        if (m == 0 || m == bgm)
            return 0;
    }
    return 1;
}

/* ---- FLAT-GLYPH-OS-SHAPES: the clone. The system's own image is drawn offscreen with marker pens; this is the pure part. */
/* The pen that fills most of the inside is the gadget's own ground. Counted one marker at a time, not through a
 * per-marker count array: this runs on the drawing task's stack (FLAT-C3 budget, test_flat_budget.py HELPER_MAX). */
static int clone_ground(const uint8_t *grid, int w, int h)
{
    int x, y, i, c, best = 0, bgm = FLAT_CLONE_BACK;
    for (i = 0; i <= FLAT_CLONE_MARKS; i++) {
        c = 0;
        for (y = 1; y < h - 1; y++)
            for (x = 1; x < w - 1; x++)
                c += grid[y * w + x] == i;
        if (c > best) {
            best = c;
            bgm = i;
        }
    }
    return bgm;
}

int flat_clone_map(const uint8_t *grid, int w, int h, uint8_t *out)
{
    int x, y, n = 0;
    int bgm, l, r, t, b;
    if (!grid || !out || w < 3 || h < 3)
        return -1;
    bgm = clone_ground(grid, w, h);
    /* FLAT-NO-DIVIDERS: peel the full-span edge lines from each side (at most a third of the box): only the glyph shows */
    for (l = 0; l < w / 3 && edge_line(grid, w, h, bgm, l, 1); l++)
        ;
    for (r = w - 1; r > w - 1 - w / 3 && edge_line(grid, w, h, bgm, r, 1); r--)
        ;
    for (t = 0; t < h / 3 && edge_line(grid, w, h, bgm, t, 0); t++)
        ;
    for (b = h - 1; b > h - 1 - h / 3 && edge_line(grid, w, h, bgm, b, 0); b--)
        ;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int m = grid[y * w + x], ring = x == 0 || y == 0 || x == w - 1 || y == h - 1;
            int glyph;
            if (m == 0 || m == bgm || m == FLAT_CLONE_BACK)
                glyph = 0;                       /* untouched, ground, or a BACKGROUND hole (S32-CLONE-CLIP: the depth
                                                  * back window's inside is marker 8 with the marker pens, 0 with real pens) */
            else if (x < l || x > r || y < t || y > b)
                glyph = 0;                       /* a divider / border line */
            else if (ring && (m == FLAT_CLONE_SHINE || m == FLAT_CLONE_SHADOW))
                glyph = 0;                       /* the bevel ring */
            else
                glyph = 1;
            out[y * w + x] = (uint8_t)glyph;
            n += glyph;
        }
    return n;
}

int flat_clone_offset(int edge)
{
    return -edge;
}

int flat_clone_pen(const flat_state *s, int role, int frame_pen, int knob_pen)
{
    if (role == FLAT_ROLE_KNOB)
        return knob_pen >= 0 ? knob_pen : s->pen_line;
    return role == FLAT_ROLE_FRAME && frame_pen >= 0 ? frame_pen : flat_glyph_pen(s, role);
}

int flat_glyph_box_role(const flat_state *s, int grole, int brole, int frame_pen, int knob_pen)
{
    if (grole == FLAT_ROLE_KNOB && s && flat_clone_pen(s, brole, frame_pen, knob_pen) == flat_clone_pen(s, grole, frame_pen, knob_pen))
        return FLAT_ROLE_BACK;
    return brole;
}

int flat_glyph_knob_gadget(unsigned long which)
{
    return which == FLAT_W_LEFT || which == FLAT_W_UP || which == FLAT_W_RIGHT || which == FLAT_W_DOWN || which == FLAT_W_SIZE;
}

int flat_clone_band_rows(int box_top, int h, int border_top)
{
    int n = border_top - 1 - box_top;
    return n < 0 ? 0 : n > h ? h : n;
}

int flat_clone_draw(const flat_state *s, unsigned long which, unsigned long method, unsigned long ids, int ox, int oy,
                    int w, int h, int frame_pen, int knob_pen, int text_pen, int band_rows, int edge_pen, flat_render_fn render, void *rctx, flat_fill_fn fill, void *fctx, uint8_t *scratch)
{
    flat_clone_in in;
    in.which = which; in.method = method; in.ids = ids; in.ox = ox; in.oy = oy; in.w = w; in.h = h;
    in.frame_pen = frame_pen; in.knob_pen = knob_pen; in.text_pen = text_pen; in.band_rows = band_rows; in.edge_pen = edge_pen;
    return flat_clone_draw_in(s, &in, render, rctx, fill, fctx, scratch);
}

/* The glyph pen and the box pen of one clone draw. Its own function so the role ints are not part of flat_clone_draw_in's
 * frame (flat budget: the clone draw runs on the caller's small stack). */
static void clone_pens(const flat_state *s, const flat_clone_in *in, int state, int *gpen, int *bpen)
{
    int grole, brole;
    flat_glyph_roles(in->which, state, &grole, &brole);
    *gpen = (grole == FLAT_ROLE_TEXT || grole == FLAT_ROLE_LINE) && in->text_pen >= 0 ? in->text_pen : flat_clone_pen(s, grole, in->frame_pen, in->knob_pen);
    *bpen = flat_clone_pen(s, flat_glyph_box_role(s, grole, brole, in->frame_pen, in->knob_pen), in->frame_pen, in->knob_pen);
}

/* The box fill (title band rows, then the rest) and the glyph runs of the map. Also its own function (flat budget). */
static void clone_paint(const flat_clone_in *in, const uint8_t *map, int gpen, int bpen, flat_fill_fn fill, void *fctx)
{
    int x, y;
    {
        int br = in->band_rows < 0 ? 0 : in->band_rows > in->h ? in->h : in->band_rows;
        if (br > 0)
            fill(fctx, in->ox, in->oy, in->ox + in->w - 1, in->oy + br - 1, bpen);
        if (br < in->h)
            fill(fctx, in->ox, in->oy + br, in->ox + in->w - 1, in->oy + in->h - 1, in->edge_pen >= 0 ? in->edge_pen : bpen);
    }
    for (y = 0; y < in->h; y++)
        for (x = 0; x < in->w; x++)
            if (map[y * in->w + x]) {
                int x1 = x;
                while (x1 + 1 < in->w && map[y * in->w + x1 + 1])
                    x1++;
                fill(fctx, in->ox + x, in->oy + y, in->ox + x1, in->oy + y, gpen);
                x = x1;
            }
}

int flat_clone_draw_in(const flat_state *s, const flat_clone_in *in, flat_render_fn render, void *rctx, flat_fill_fn fill, void *fctx, uint8_t *scratch)
{
    uint8_t *grid = scratch, *map = scratch + FLAT_CLONE_MAX * FLAT_CLONE_MAX;
    int state, gpen, bpen;
    if (!fill || !render || !scratch || !flat_clone_route(s, in->which, in->method) ||
        in->w < FLAT_GLYPH_MIN_SIZE || in->h < FLAT_GLYPH_MIN_SIZE || in->w > FLAT_CLONE_MAX || in->h > FLAT_CLONE_MAX)
        return FLAT_CLONE_NO;
    state = flat_glyph_state(in->ids);
    memset(grid, 0, (size_t)(in->w * in->h));
    /* No glyph pixel is a failed clone, not a plain box: OS 3.2's sysiclass returns 1 from IM_DRAW for the window gadgets
     * yet puts nothing in our offscreen rastport, and drawing that empty map painted the active close/depth/zoom boxes solid. */
    if (!render(rctx, in->ids, grid, in->w, in->h) || flat_clone_map(grid, in->w, in->h, map) <= 0)
        return FLAT_CLONE_FAIL;
    clone_pens(s, in, state, &gpen, &bpen);
    clone_paint(in, map, gpen, bpen, fill, fctx);
    return FLAT_CLONE_OK;
}

/* Both entry points run the same generator; g carries either the array or the fill callback. */
static int plan_run(plan *g, unsigned long which, int w, int h, int state, int side)
{
    int i;
    g->n = 0; g->w = w; g->h = h; g->side = side < 1 ? 1 : side;
    {
        int fg, bg;
        flat_glyph_roles(which, state, &fg, &bg);
        g->fg = (short)fg;
        g->bg = (short)flat_glyph_box_role(g->st, fg, bg, g->frame_pen, g->knob_pen);
    }
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
    case FLAT_W_LEFT: case FLAT_W_UP: case FLAT_W_RIGHT: case FLAT_W_DOWN: g_arrow(g, which); break;
    }
    return g->n;
}

int flat_glyph_plan(unsigned long which, int w, int h, int state, flat_prim *out, int cap)
{
    plan g;
    if (!out || cap < FLAT_GLYPH_MAX_PRIMS || !flat_glyph_known(which) || w < FLAT_GLYPH_MIN_SIZE || h < FLAT_GLYPH_MIN_SIZE)
        return 0;
    g.p = out; g.cap = cap; g.fill = 0; g.ctx = 0; g.st = 0; g.frame_pen = -1; g.knob_pen = -1; g.ox = g.oy = 0;
    return plan_run(&g, which, w, h, state, 1);
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
                    int w, int h, int frame_pen, int knob_pen, int side, flat_fill_fn fill, void *ctx)
{
    plan g;
    if (!fill || !flat_glyph_route(s, which, method) || w < FLAT_GLYPH_MIN_SIZE || h < FLAT_GLYPH_MIN_SIZE)
        return 0;
    g.p = 0; g.cap = FLAT_GLYPH_MAX_PRIMS; g.fill = fill; g.ctx = ctx; g.st = s; g.frame_pen = frame_pen; g.knob_pen = knob_pen; g.ox = ox; g.oy = oy;
    return plan_run(&g, which, w, h, flat_glyph_state(ids), side) > 0;
}

int flat_frame_draw(const flat_state *s, int ids, int ox, int oy, int w, int h, int fill, int role, flat_fill_fn fn, void *ctx)
{
    int x1, y1, sw;
    if (!s || !fn || w < 3 || h < 3)
        return 0;
    x1 = ox + w - 1;
    y1 = oy + h - 1;
    sw = flat_button_side(s->side, w);
    if (fill) {                                      /* OWNER-2026-11-08 FLAT-BUTTON-NO-RING: every push button is a solid fill, never a ring (the secondary role used to be a text-pen outline; it is now a pen_button fill, so the roles still differ: primary accent, secondary the raised button tone). S56-PRIMARY-ACCENT-FILL: primary fill = the theme accent pen (pen_line, a palette pen at every depth), pressed = the text pen; label in pen_ptext (flat_tx_arm); the scroller knob and cycle gadget keep pen_button; no outline */
        int face = s->pen_button ? s->pen_button : s->pen_back;
        fn(ctx, ox, oy, x1, y1, role == FLAT_ROLE_PLAIN ? face : ids == 1 ? s->pen_text : role == FLAT_ROLE_SECONDARY ? face : s->pen_line);
        return 1;
    }
    fn(ctx, ox, oy, x1, oy, s->pen_line);            /* top */
    fn(ctx, ox, y1, x1, y1, s->pen_line);            /* bottom */
    fn(ctx, ox, oy, ox + sw - 1, y1, s->pen_line);   /* left */
    fn(ctx, x1 - sw + 1, oy, x1, y1, s->pen_line);   /* right */
    return 1;
}

int flat_button_corner(int w, int h, int i, int out[4])
{
    int tall = h >= FLAT_ROUND_TALL, c, k, x, y, ix, iy;
    if (w < 3 || h < 3 || i < 0 || i >= (tall ? 12 : 4))
        return 0;
    c = tall ? i / 3 : i;
    k = tall ? i % 3 : 0;
    x = (c & 1) ? w - 1 : 0;
    y = (c & 2) ? h - 1 : 0;
    ix = (c & 1) ? -1 : 1;                                          /* direction into the box */
    iy = (c & 2) ? -1 : 1;
    if (k == 0) {
        out[0] = x; out[1] = y; out[2] = x - ix; out[3] = y - iy;
    } else if (k == 1) {
        out[0] = x + ix; out[1] = y; out[2] = x + ix; out[3] = y - iy;  /* beside the corner, along the row */
    } else {
        out[0] = x; out[1] = y + iy; out[2] = x - ix; out[3] = y + iy;  /* beside the corner, down the column */
    }
    return 1;
}

int flat_button_corners(int w, int h, int out[][4])
{
    int n = 0;
    while (flat_button_corner(w, h, n, out[n]))
        n++;
    return n;
}

int flat_scroller_cuts(int cx, int cy, int cw, int ch, int horiz, int vert, unsigned hpot, unsigned vpot, unsigned hbody, unsigned vbody,
                       int x0, int y0, int x1, int y1, int out[][2])
{
    int kx, ky, kw, kh;
    if (!flat_scroller_knob(cx, cy, cw, ch, horiz, vert, hpot, vpot, hbody, vbody, &kx, &ky, &kw, &kh))
        return 0;
    return flat_scroller_cuts_box(kx, ky, kw, kh, x0, y0, x1, y1, out);
}

int flat_scroller_cut_at(int kx, int ky, int kw, int kh, int x0, int y0, int x1, int y1, int i, int out[2])
{
    int cut[4], px, py;
    if (!flat_button_corner(kw, kh, i, cut))
        return 0;
    px = kx + cut[0];                               /* every corner pixel inside THIS fill: 3.2 paints the knob as several fills */
    py = ky + cut[1];
    if (px < x0 || px > x1 || py < y0 || py > y1)
        return 0;
    out[0] = px;
    out[1] = py;
    return 1;
}

int flat_scroller_cuts_box(int kx, int ky, int kw, int kh, int x0, int y0, int x1, int y1, int out[][2])
{
    int i, m = 0;
    for (i = 0; i < FLAT_ROUND_MAX; i++)
        if (flat_scroller_cut_at(kx, ky, kw, kh, x0, y0, x1, y1, i, out[m]))
            m++;
    return m;
}

int flat_frame_draw_round(const flat_state *s, int ids, int ox, int oy, int w, int h, int fill, int role, flat_fill_fn fn,
                          flat_read_fn rd, void *ctx)
{
    unsigned char pen[FLAT_ROUND_MAX];               /* the low byte of a read pen >= 0, valid where its bit in got is set; S62-FLAT-RF-TX-BUDGET: bytes */
    unsigned got = 0;
    int cut[4], n = 0, i, p;
    if (!s || !fn || w < 3 || h < 3)
        return 0;
    if (rd && fill)
        for (; flat_button_corner(w, h, n, cut); n++) {
            p = rd(ctx, ox + cut[2], oy + cut[3]);                  /* before the box is painted */
            if (p >= 0) {                /* as before the byte array: every pen >= 0 is put back. A pen over 255 goes back as its low
                                            * byte, which is the pen c2_fill's SetAPen keeps (RastPort FgPen is a byte) */
                pen[n] = (unsigned char)p;
                got |= 1u << n;
            }
        }
    if (!flat_frame_draw(s, ids, ox, oy, w, h, fill, role, fn, ctx))
        return 0;
    for (i = 0; i < n; i++)
        if ((got & (1u << i)) && flat_button_corner(w, h, i, cut))
            fn(ctx, ox + cut[0], oy + cut[1], ox + cut[0], oy + cut[1], pen[i]);
    return 1;
}

int flat_knob_frame_draw(const flat_state *s, int depth, int active, int ox, int oy, int w, int h, flat_fill_fn fn, flat_read_fn rd, void *ctx)
{
    flat_state st;
    if (!s)
        return 0;
    st = *s;
    st.pen_button = (uint8_t)flat_knob_state_pen(s, depth, active);
    return flat_frame_draw_round(&st, 0, ox, oy, w, h, 1, FLAT_ROLE_PLAIN, fn, rd, ctx);
}

int flat_smart_knob(int is_prop, unsigned prop_flags, const void *knob_next, const void *image)
{
    return is_prop && image && knob_next == image && (prop_flags & 0x21u) == 0x21u;    /* AUTOKNOB | SMARTKNOBIMAGE */
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

/* The strip i as the RectFill patch paints it: outer_strip widened to px columns on the left and right strips. 0 when absent. */
static int outer_ring_strip(int i, int w, int h, int px, int out[4])
{
    if (!outer_strip(i, w, h, out))
        return 0;
    if (px < 1)
        px = 1;
    if (px > w / 2)
        px = w / 2 < 1 ? 1 : w / 2;
    if (i == 1)
        out[2] = out[0] + px - 1;
    else if (i == 2)
        out[0] = out[2] - px + 1;
    return 1;
}

int flat_outer_edge_fill(int w, int h, int x0, int y0, int x1, int y1, int px, int out[4])
{
    int i, r[4];
    if (w < 2 || h < 2)
        return 0;
    for (i = 0; i < 4; i++)
        if (outer_strip(i, w, h, r) && r[0] == x0 && r[1] == y0 && r[2] == x1 && r[3] == y1)
            return outer_ring_strip(i, w, h, px, out);
    return 0;
}

int flat_outer_ring_overlap(int w, int h, int px, int bx0, int by0, int bx1, int by1, flat_fill_fn fill, void *ctx, int pen)
{
    int i, n = 0, r[4];
    if (w < 2 || h < 2)
        return 0;
    for (i = 0; i < 4; i++) {
        if (!outer_ring_strip(i, w, h, px, r))
            continue;
        if (r[0] < bx0) r[0] = bx0;
        if (r[1] < by0) r[1] = by0;
        if (r[2] > bx1) r[2] = bx1;
        if (r[3] > by1) r[3] = by1;
        if (r[0] > r[2] || r[1] > r[3])
            continue;
        fill(ctx, r[0], r[1], r[2], r[3], pen);
        n++;
    }
    return n;
}
