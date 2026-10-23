/* Amarchy drawn menu, the geometry: see menu_geom.h. */
#include <string.h>

#include "menu_geom.h"

/* Omarchy's px in this font: vertical over its 12 px line, horizontal over its 7.2 px character */
static int vs(const mg_font *f, int px)
{
    int v = (px * f->ysize + 6) / 12;
    return v < 1 ? 1 : v;
}

static int hs(const mg_font *f, int px)
{
    int v = (px * f->cw * 5 + 18) / 36;
    return v < 1 ? 1 : v;
}

static int imax(int a, int b) { return a > b ? a : b; }
static int imin(int a, int b) { return a < b ? a : b; }

void mg_metrics_for(mg_metrics *m, const mg_font *f)
{
    m->bx = imax(2, hs(f, 2));
    m->by = imax(2, vs(f, 2));
    m->hpad = hs(f, 18);
    m->vpad = vs(f, 18);
    m->head_h = imax(vs(f, 34), f->ysize + 2 * vs(f, 6));
    m->gap = vs(f, 6);
    m->row_h = imax(vs(f, 50), f->ysize + 2 * vs(f, 12));
    m->row_gap = vs(f, 3);
    m->pitch = m->row_h + m->row_gap;
    m->row_pad = hs(f, 12);
    m->icon_gap = hs(f, 10);
    m->chev_gap = hs(f, 12);
    m->min_w = hs(f, 300);
    m->margin_x = hs(f, 10);
    m->margin_y = vs(f, 10);
}

void mg_header(const char *node, char *out, int n)
{
    const char *seg = strrchr(node, '.');
    seg = seg ? seg + 1 : node;
    if (!*seg)
        seg = "Amarchy";
    strncpy(out, seg, n - 1);
    out[n - 1] = 0;
    if (out[0] >= 'a' && out[0] <= 'z')
        out[0] = (char)(out[0] - 'a' + 'A');
}

typedef struct width_acc {
    const mg_metrics *m;
    int icon_w, chev_w, maxw;
    mg_text_fn text;
    void *ctx;
} width_acc;

/* the widest row content of one level */
static void width_level(const mn_item *it, int n, void *ctx)
{
    width_acc *a = (width_acc *)ctx;
    int i;
    for (i = 0; i < n; i++) {
        int w = a->text(it[i].label, a->ctx);
        if (a->icon_w > 0)
            w += a->icon_w + a->m->icon_gap;
        if (it[i].row < 0)
            w += a->m->chev_gap + a->chev_w;
        if (w > a->maxw)
            a->maxw = w;
    }
}

int mg_width(const keymap *k, const char *node, const mg_font *f, int icon_w, int chev_w,
             mg_text_fn text, void *ctx, int scr_w)
{
    mg_metrics m;
    width_acc a;
    char head[KM_ID];
    int w;
    mg_metrics_for(&m, f);
    a.m = &m;
    a.icon_w = icon_w;
    a.chev_w = chev_w;
    a.text = text;
    a.ctx = ctx;
    /* the levels below show their group's label as header, which their parent's row already
     * measured; only the opening level's own header is new */
    mg_header(node, head, sizeof head);
    a.maxw = text(head, ctx);
    mn_levels(k, node, width_level, &a);
    w = 2 * m.bx + 2 * m.hpad + 2 * m.row_pad + a.maxw;
    w = imax(w, m.min_w);
    return imin(w, scr_w - 2 * m.margin_x);
}

void mg_layout(mg_geom *g, const mg_font *f, int w, int n, int scr_w, int scr_h)
{
    const mg_metrics *m = &g->m;
    int chrome, room, fit;
    mg_metrics_for(&g->m, f);
    chrome = 2 * m->by + 2 * m->vpad + m->head_h + m->gap;
    room = imin(scr_h - 2 * m->margin_y - chrome, scr_h * 7 / 10);
    fit = imin(imax(1, (room + m->row_gap) / m->pitch), MG_MAX_ROWS);
    g->n = n;
    if (n > fit) {
        g->peek = m->row_h * 55 / 100;
        g->visible = imin(imax(1, (room - g->peek) / m->pitch), MG_MAX_ROWS);
        g->list_h = g->visible * m->pitch + g->peek;
    } else {
        g->peek = 0;
        g->visible = n;
        g->list_h = n > 0 ? n * m->pitch - m->row_gap : 0;
    }
    g->w = w;
    g->h = chrome + g->list_h;
    g->x = (scr_w - g->w) / 2;
    g->y = imax(0, (scr_h - g->h) / 2);
    g->head_x = g->list_x = m->bx + m->hpad;
    g->head_w = g->list_w = w - 2 * (m->bx + m->hpad);
    g->head_y = m->by + m->vpad;
    g->list_y = g->head_y + m->head_h + m->gap;
}

int mg_scroll(const mg_geom *g, int sel, int top)
{
    if (sel < top)
        top = sel;
    if (sel >= top + g->visible)
        top = sel - g->visible + 1;
    if (top > g->n - g->visible)
        top = g->n - g->visible;
    return top < 0 ? 0 : top;
}

int mg_more(const mg_geom *g, int top)
{
    return g->peek > 0 && top + g->visible < g->n;
}

int mg_row_y(const mg_geom *g, int i)
{
    return g->list_y + i * g->m.pitch;
}

int mg_hit(const mg_geom *g, int top, int mx, int my)
{
    int i, rows = g->visible + (mg_more(g, top) ? 1 : 0);
    if (mx < g->list_x || mx >= g->list_x + g->list_w || my < g->list_y
        || my >= g->list_y + g->list_h)
        return -1;
    i = (my - g->list_y) / g->m.pitch;
    if (i >= rows || my - mg_row_y(g, i) >= g->m.row_h)
        return -1;                     /* past the rows, or in the gap below one */
    return top + i;
}
