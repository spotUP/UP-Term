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
    m->unit = row;
    /* the icon (8/15 of Omarchy's row, mi_size) and a thin band above and below it: Omarchy's full row
     * left a wide empty band around every label on the Amiga's small fonts */
    m->row_h = imax(f->ysize + 2, row * 8 / 15 + 2 * vr(row, 5));
    m->row_gap = imax(1, vr(row, 3));
    m->pitch = m->row_h + m->row_gap;
    m->row_pad = imax(hr(s, row, 12), half_w);
    m->icon_gap = imax(hr(s, row, 10), half_w);
    m->chev_gap = imax(hr(s, row, 12), half_w);
    if (s->minimal)                 /* text only: no border art, nothing between label and marker */
        m->bx = m->by = m->icon_gap = m->chev_gap = 0;
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

void (*mg_relabel)(struct mn_item *it, int n);
void (*mg_rechip)(const struct mn_item *it, int n, long *out);

int mg_chip_w(const mg_metrics *m)
{
    return (m->row_h - 4) + 4 + m->row_pad;
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
    /* The item buffer is walker scratch, rewritten on every callback, so the painter's own
     * "Label: value" text goes in place: the window is sized from what the row will show. */
    if (mg_relabel)
        mg_relabel((mn_item *)it, n);
    for (i = 0; i < n; i++) {
        int w = a->text(it[i].label, a->ctx);
        if (a->icon_w > 0)
            w += a->icon_w + a->m->icon_gap;
        if (MN_IS_SETTING(&it[i]) && mg_rechip) {
            long rgb = -1;
            mg_rechip(&it[i], 1, &rgb);
            if (rgb >= 0)
                w += mg_chip_w(a->m);
        }
        if (MN_IS_GROUP(&it[i]))
            w += a->m->chev_gap + a->chev_w;
        if (w > a->maxw)
            a->maxw = w;
    }
}

int mg_width_t(const keymap *k, const char *node, const mg_font *f, int icon_w, int chev_w,
               mg_text_fn text, void *ctx, const mg_screen *s, int cond)
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
    mn_levels_t(k, node, width_level, &a, cond);
    w = 2 * m.bx + 2 * m.hpad + 2 * m.row_pad + a.maxw;
    w = imax(w, m.min_w);
    return imin(w, s->w - 2 * m.margin_x);
}

int mg_width(const keymap *k, const char *node, const mg_font *f, int icon_w, int chev_w,
             mg_text_fn text, void *ctx, const mg_screen *s)
{
    return mg_width_t(k, node, f, icon_w, chev_w, text, ctx, s, 1);
}

/* One layout of n scrolling entries. When tail >= 0 the entries are text lines (the font plus a small leading) and
 * `tail` rows of the menu's row height are pinned in a strip below them (the welcome window's check box and buttons:
 * they stay in view, only the text scrolls). The strip's height is taken off the room the text gets. */
static void layout(mg_geom *g, const mg_font *f, int w, int n, int tail, const mg_screen *s)
{
    mg_metrics *m = &g->m;
    int chrome, room, fit, strip = 0, ntext;
    mg_metrics_for(m, f, s);
    g->tail = 0;
    g->tail_h = m->row_h;
    g->tail_gap = m->row_gap;
    g->max_row_h = m->row_h;
    if (tail >= 0) {                        /* text lines: the font plus a small leading */
        int leading = m->row_gap;
        g->tail = tail;
        m->row_h = f->ysize + leading;
        m->row_gap = 0;
        m->pitch = m->row_h;
        if (tail > 0)
            strip = m->gap + tail * (g->tail_h + g->tail_gap) - g->tail_gap;
    }
    ntext = n;
    chrome = 2 * m->by + 2 * m->vpad + m->head_h + m->gap;
    room = imin(s->h - 2 * m->margin_y - chrome - strip, s->h * 7 / 10 - strip);
    fit = imin(imax(1, (room + m->row_gap) / m->pitch), MG_MAX_ROWS);
    g->n = n;
    if (n > fit) {
        g->peek = m->row_h * 55 / 100;
        g->visible = imin(imax(1, (room - g->peek) / m->pitch), MG_MAX_ROWS);
        g->list_h = g->visible * m->pitch + g->peek;
    } else {
        g->peek = 0;
        g->visible = n;
        g->list_h = n > 0 ? ntext * m->pitch - m->row_gap : 0;
    }
    if (g->tail && g->tail_h > g->max_row_h)
        g->max_row_h = g->tail_h;
    g->w = w;
    g->h = chrome + g->list_h + strip;
    if (s->anchored) {
        g->x = imax(0, imin(s->ax - g->w, s->w - g->w));
        g->y = imax(0, imin(s->ay - g->h, s->h - g->h));
    } else {
        g->x = (s->w - g->w) / 2;
        g->y = imax(0, (s->h - g->h) / 2);
    }
    g->head_x = g->list_x = m->bx + m->hpad;
    g->head_w = g->list_w = w - 2 * (m->bx + m->hpad);
    g->head_y = m->by + m->vpad;
    g->list_y = g->head_y + m->head_h + m->gap;
    g->strip_y = g->list_y + g->list_h + m->gap;
}

void mg_layout(mg_geom *g, const mg_font *f, int w, int n, const mg_screen *s)
{
    layout(g, f, w, n, -1, s);
}

void mg_layout_text(mg_geom *g, const mg_font *f, int w, int n, int tail, const mg_screen *s)
{
    layout(g, f, w, n, tail < 0 ? 0 : tail, s);
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

int mg_strip_y(const mg_geom *g, int i)
{
    return g->strip_y + i * (g->tail_h + g->tail_gap);
}

int mg_strip_hit(const mg_geom *g, int mx, int my)
{
    int i;
    if (mx < g->list_x || mx >= g->list_x + g->list_w)
        return -1;
    for (i = 0; i < g->tail; i++)
        if (my >= mg_strip_y(g, i) && my < mg_strip_y(g, i) + g->tail_h)
            return i;
    return -1;
}

int mg_row_h_of(const mg_geom *g, int e)
{
    (void)e;
    return g->m.row_h;
}

int mg_row_y_at(const mg_geom *g, int top, int i)
{
    (void)top;
    return g->list_y + i * g->m.pitch;
}

int mg_row_y(const mg_geom *g, int i)
{
    return mg_row_y_at(g, 0, i);
}

int mg_hit(const mg_geom *g, int top, int mx, int my)
{
    int i, rows = g->visible + (mg_more(g, top) ? 1 : 0);
    if (mx < g->list_x || mx >= g->list_x + g->list_w || my < g->list_y
        || my >= g->list_y + g->list_h)
        return -1;
    for (i = 0; i < rows && top + i < g->n; i++) {
        int y = mg_row_y_at(g, top, i);
        if (my < y)
            return -1;                 /* in the gap below the row before */
        if (my < y + mg_row_h_of(g, top + i))
            return top + i;
    }
    return -1;                         /* past the rows */
}
int mg_blit_safe(int beam, int top, int h, int frame, int blit)
{
    int d;
    if (frame <= 0)
        return 1;
    if (blit <= 0)
        blit = frame / 2;     /* not measured: plan for the slowest copy seen, a row on the A1200 */
    if (blit > frame - h)
        return 1;             /* no start keeps the beam off the box while it is copied */
    d = ((top - beam - 1) % frame + frame) % frame + 1;   /* lines until the beam is back on top, 1..frame */
    return d >= blit && d <= frame - h;   /* the beam is off the box now and stays off until the copy is done */
}

long mg_blit_rate(long est, long took)
{
    if (took >= est)
        return took;
    return est - (est - took) / 16;
}

int mg_blit_strip(int y0, int y1, int row_h, int i, int *s0, int *s1)
{
    if (row_h < 1)
        row_h = 1;
    *s0 = y0 + i * row_h;
    if (*s0 > y1)
        return 0;
    *s1 = *s0 + row_h - 1;
    if (*s1 > y1)
        *s1 = y1;
    return 1;
}
