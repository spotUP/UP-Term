/* Amarchy drawn menu, the geometry: see menu_geom.h. */
#include <string.h>

#include "menu_geom.h"

/* The columns that look as long as `lines` lines on this screen's pixels (rx, ry: DisplayInfo
 * Resolution ticks of one pixel, 0 when unknown = square). */
int mg_cols(const mg_screen *s, int lines)
{
    if (s->rx <= 0 || s->ry <= 0)
        return lines;
    return (lines * s->ry + s->rx / 2) / s->rx;
}

static int imax(int a, int b) { return a > b ? a : b; }
static int imin(int a, int b) { return a < b ? a : b; }

/* Omarchy's px (its rows are 50 px) in this font's rows: down, and across (same look on tall pixels) */
static int vr(int row, int px)
{
    return (px * row + 25) / 50;
}

static int hr(const mg_screen *s, int row, int px)
{
    return mg_cols(s, vr(row, px));
}

void mg_metrics_for(mg_metrics *m, const mg_font *f, const mg_screen *s)
{
    int half_w = imax(1, f->cw / 2);
    int row = imax(f->ysize * 15 / 4, f->ysize + 2);
    m->by = imax(2, vr(row, 2));
    m->bx = mg_cols(s, m->by);
    m->hpad = imax(hr(s, row, 18), f->cw);
    m->vpad = imax(vr(row, 18), f->ysize / 2);
    m->head_h = imax(vr(row, 34), f->ysize + 2 * imax(1, vr(row, 6)));
    m->gap = imax(vr(row, 6), imax(1, f->ysize / 4));
    m->row_h = row;
    m->row_gap = imax(1, vr(row, 3));
    m->pitch = m->row_h + m->row_gap;
    m->row_pad = imax(hr(s, row, 12), half_w);
    m->icon_gap = imax(hr(s, row, 10), half_w);
    m->chev_gap = imax(hr(s, row, 12), half_w);
    m->min_w = imax(hr(s, row, 300), 16 * f->cw);
    m->margin_y = imax(1, vr(row, 10));
    m->margin_x = imax(1, mg_cols(s, m->margin_y));
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
        if (MN_IS_GROUP(&it[i]))
            w += a->m->chev_gap + a->chev_w;
        if (w > a->maxw)
            a->maxw = w;
    }
}

int mg_width_t(const keymap *k, const char *node, const mg_font *f, int icon_w, int chev_w,
               mg_text_fn text, void *ctx, const mg_screen *s, int term)
{
    mg_metrics m;
    width_acc a;
    char head[KM_ID];
    int w;
    mg_metrics_for(&m, f, s);
    a.m = &m;
    a.icon_w = icon_w;
    a.chev_w = chev_w;
    a.text = text;
    a.ctx = ctx;
    /* the levels below show their group's label as header, which their parent's row already
     * measured; only the opening level's own header is new */
    mg_header(node, head, sizeof head);
    a.maxw = text(head, ctx);
    mn_levels_t(k, node, width_level, &a, term);
    w = 2 * m.bx + 2 * m.hpad + 2 * m.row_pad + a.maxw;
    w = imax(w, m.min_w);
    return imin(w, s->w - 2 * m.margin_x);
}

int mg_width(const keymap *k, const char *node, const mg_font *f, int icon_w, int chev_w,
             mg_text_fn text, void *ctx, const mg_screen *s)
{
    return mg_width_t(k, node, f, icon_w, chev_w, text, ctx, s, 1);
}

void mg_layout(mg_geom *g, const mg_font *f, int w, int n, const mg_screen *s)
{
    const mg_metrics *m = &g->m;
    int chrome, room, fit;
    mg_metrics_for(&g->m, f, s);
    chrome = 2 * m->by + 2 * m->vpad + m->head_h + m->gap;
    room = imin(s->h - 2 * m->margin_y - chrome, s->h * 7 / 10);
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
    g->x = (s->w - g->w) / 2;
    g->y = imax(0, (s->h - g->h) / 2);
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
