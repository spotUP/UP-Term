/* See menu_text_core.h. */
#include <string.h>
#include "menu_text_core.h"

void mtc_init(mtc *c, const mtc_src *src)
{
    memset(c, 0, sizeof(*c));
    c->src = *src;
}

void mtc_free(mtc *c)
{
    int i;
    for (i = 0; i < MTC_SLOTS; i++)
        if (c->slot[i].cov)
            c->src.release(c->slot[i].cov);
    memset(c->slot, 0, sizeof(c->slot));
}

unsigned long mtc_next(const char *s, int *n)
{
    const unsigned char *p = (const unsigned char *)s;
    unsigned long cp;
    int len, i;
    if (!p[0]) {
        *n = 0;
        return 0;
    }
    if (p[0] < 0x80) {
        *n = 1;
        return p[0];
    }
    if ((p[0] & 0xE0) == 0xC0) {
        len = 2;
        cp = p[0] & 0x1F;
    } else if ((p[0] & 0xF0) == 0xE0) {
        len = 3;
        cp = p[0] & 0x0F;
    } else if ((p[0] & 0xF8) == 0xF0) {
        len = 4;
        cp = p[0] & 0x07;
    } else {
        *n = 1;
        return p[0];
    }
    for (i = 1; i < len; i++) {
        if ((p[i] & 0xC0) != 0x80) {
            *n = 1;
            return p[0];
        }
        cp = (cp << 6) | (p[i] & 0x3F);
    }
    *n = len;
    return cp;
}

static mtc_glyph *slot_of(mtc *c, unsigned long cp)
{
    mtc_glyph *g = &c->slot[cp % MTC_SLOTS];
    if ((g->have_adv || g->have_cov) && g->cp != cp) {
        if (g->cov)
            c->src.release(g->cov);
        memset(g, 0, sizeof(*g));
    }
    g->cp = cp;
    return g;
}

int mtc_advance(mtc *c, unsigned long cp)
{
    mtc_glyph *g = slot_of(c, cp);
    if (g->have_adv) {
        c->hits++;
        return g->adv;
    }
    g->adv = c->src.advance(c->src.u, cp);
    g->have_adv = 1;
    c->advances++;
    return g->adv;
}

const mtc_glyph *mtc_glyph_of(mtc *c, unsigned long cp)
{
    mtc_glyph *g;
    if (!c->src.render)
        return 0;
    mtc_advance(c, cp);
    g = slot_of(c, cp);
    if (!g->have_cov) {
        c->src.render(c->src.u, cp, g);
        g->have_cov = 1;
        c->renders++;
    } else
        c->hits++;
    return g;
}

int mtc_width(mtc *c, const char *s)
{
    int w = 0, n;
    unsigned long cp;
    while ((cp = mtc_next(s, &n)), n > 0) {
        w += mtc_advance(c, cp);
        s += n;
    }
    return w;
}

int mtc_width_n(mtc *c, const char *s, int len)
{
    int w = 0, n;
    unsigned long cp;
    const char *e = s + len;
    while (s < e && ((cp = mtc_next(s, &n)), n > 0) && s + n <= e) {
        w += mtc_advance(c, cp);
        s += n;
    }
    return w;
}

int mtc_prefix(mtc *c, const char *s, int len, int maxw)
{
    int w = 0, used = 0, n;
    unsigned long cp;
    while (used < len && ((cp = mtc_next(s + used, &n)), n > 0)) {
        int a = mtc_advance(c, cp);
        if (used + n > len || w + a > maxw)
            break;
        w += a;
        used += n;
    }
    return used;
}

int mtc_fit(mtc *c, const char *s, int maxw, char *out, int n)
{
    static const char dots[] = "...";
    int total = mtc_width(c, s), dotw = 3 * mtc_advance(c, '.'), used = 0, cut = 0, w = 0, k, len;
    unsigned long cp;
    const char *p = s;
    if (n < 1)
        return 0;
    if (total <= maxw && (int)strlen(s) < n) {
        strcpy(out, s);
        return total;
    }
    if (dotw > maxw) { /* not even the ellipsis: as many dots as fit */
        int d = mtc_advance(c, '.'), cnt = d > 0 ? maxw / d : 0;
        if (cnt > 3)
            cnt = 3;
        if (cnt > n - 1)
            cnt = n - 1;
        for (k = 0; k < cnt; k++)
            out[k] = '.';
        out[cnt] = 0;
        return cnt * d;
    }
    while ((cp = mtc_next(p, &k)), k > 0) {
        int a = mtc_advance(c, cp);
        if (w + a + dotw > maxw || used + k + 3 > n - 1)
            break;
        w += a;
        used += k;
        p += k;
        cut = used;
    }
    len = cut;
    memcpy(out, s, (size_t)len);
    memcpy(out + len, dots, 4);
    return w + dotw;
}

int mtc_fit_tail(mtc *c, const char *s, int keep, int maxw, char *out, int n)
{
    char head[256];
    int tailw, hw, hl;
    if (keep <= 0 || keep >= (int)strlen(s) || keep >= (int)sizeof head)
        return mtc_fit(c, s, maxw, out, n);
    if (mtc_width(c, s) <= maxw && (int)strlen(s) < n) {
        strcpy(out, s);
        return mtc_width(c, s);
    }
    tailw = mtc_width(c, s + keep);
    if (tailw + 3 * mtc_advance(c, '.') > maxw || (int)strlen(s + keep) + 4 > n)
        return mtc_fit(c, s + keep, maxw, out, n);
    memcpy(head, s, (size_t)keep);
    head[keep] = 0;
    hw = mtc_fit(c, head, maxw - tailw, out, n - (int)strlen(s + keep));
    hl = (int)strlen(out);
    strcpy(out + hl, s + keep);
    return hw + tailw;
}

/* ---- TrueType ---- */

int mtc_scale(int units, int upem, int px)
{
    long v = (long)units * px;
    if (upem <= 0)
        return 0;
    return (int)(v >= 0 ? (v + upem / 2) / upem : -((-v + upem / 2) / upem));
}

/* px is the em across, in columns; down it is the same em in rows, which on a tall pixel (rx, ry: the screen's pixel in
 * DisplayInfo ticks, 0 = square) are fewer: the look the terminal's ttf_fit gives a glyph. Sizing both by px drew
 * every glyph twice too tall on a 640x256 screen and doubled the row height. */
int mtc_ttf_init(mtc_ttf *t, ttf_font *f, int px, int rx, int ry)
{
    int py = px;
    int asc = ttf_ascender(f), desc = ttf_descender(f);
    t->f = f;
    t->px = px;
    t->upem = ttf_units_per_em(f);
    if (px < 4 || px > 128 || t->upem <= 0)
        return 0;
    if (rx > 0 && ry > 0)
        py = mtc_scale(px, ry, rx);
    if (py < 1)
        py = 1;
    t->py = py;
    t->scale = ((long)px << 16) / t->upem;
    t->yscale = ((long)py << 16) / t->upem;
    t->asc = asc;
    t->base = mtc_scale(asc, t->upem, py);
    t->line = mtc_scale(asc - desc, t->upem, py);
    if (t->line < py)
        t->line = py;
    if (t->base < 1)
        t->base = py;
    return 1;
}

static int ttf_adv(void *u, unsigned long cp)
{
    mtc_ttf *t = (mtc_ttf *)u;
    unsigned gid = ttf_glyph_index(t->f, cp);
    return mtc_scale(ttf_advance(t->f, gid), t->upem, t->px);
}

/* the box is the advance plus a pad each side (overhang); the coverage is then cut to the columns with ink */
static int ttf_rend(void *u, unsigned long cp, mtc_glyph *g)
{
    mtc_ttf *t = (mtc_ttf *)u;
    void *(*alloc)(unsigned long) = t->alloc;
    void (*rel)(void *) = t->release;
    unsigned gid = ttf_glyph_index(t->f, cp);
    int pad = t->px / 4 + 1, bw = mtc_scale(ttf_advance(t->f, gid), t->upem, t->px) + 2 * pad, bh = t->line;
    int x, y, x0 = bw, x1 = -1;
    unsigned char *tmp;
    g->left = g->w = 0;
    g->h = bh;
    g->cov = 0;
    if (bw > 512 || bh > 512 || !gid || !(tmp = (unsigned char *)alloc((unsigned long)bw * bh)))
        return 0;
    memset(tmp, 0, (size_t)bw * bh);
    if (ttf_render(t->f, gid, t->scale, t->yscale, pad, t->base, tmp, bw, bh) != 1) {
        rel(tmp);
        return 0;
    }
    for (y = 0; y < bh; y++)
        for (x = 0; x < bw; x++)
            if (tmp[y * bw + x]) {
                if (x < x0)
                    x0 = x;
                if (x > x1)
                    x1 = x;
            }
    if (x1 < x0) {
        rel(tmp);
        return 0;
    }
    g->w = x1 - x0 + 1;
    g->left = x0 - pad;
    g->cov = (unsigned char *)alloc((unsigned long)g->w * bh);
    if (g->cov)
        for (y = 0; y < bh; y++)
            memcpy(g->cov + (size_t)y * g->w, tmp + (size_t)y * bw + x0, (size_t)g->w);
    else
        g->w = 0;
    rel(tmp);
    return g->w > 0;
}

void mtc_ttf_source(mtc_src *s, mtc_ttf *t, void *(*alloc)(unsigned long), void (*release)(void *))
{
    t->alloc = alloc;
    t->release = release;
    s->u = t;
    s->advance = ttf_adv;
    s->render = ttf_rend;
    s->alloc = alloc;
    s->release = release;
}
