/* chips.c -- see chips.h. Portable C89: the host tests run it. */
#include "chips.h"

void vc_scroll_plan(int top, int bottom, int n, vc_scroll *s)
{
    int h = bottom - top, a = n < 0 ? -n : n;
    s->copy = s->src = s->dst = 0;
    s->vac0 = s->vac1 = s->busy0 = s->busy1 = top;
    if (h <= 0 || !a)
        return;
    if (a >= h) {
        s->vac1 = bottom; /* everything moves out: nothing to copy */
        return;
    }
    s->copy = h - a;
    s->busy1 = bottom; /* the copy reads and writes the whole region */
    if (n > 0) {
        s->src = top + a;
        s->dst = top;
        s->vac0 = bottom - a;
        s->vac1 = bottom;
    } else {
        s->src = top;
        s->dst = top + a;
        s->vac1 = top + a;
    }
}

int vc_cpu_may_write(int state, long busy_y0, long busy_y1, long y0, long y1)
{
    if (state == VC_BLIT_IDLE)
        return 1;
    if (state == VC_BLIT_SCROLL)
        return y1 <= busy_y0 || y0 >= busy_y1;
    return 0;
}

int vc_scroll_by_blit(int in_pass, int planes_ok, int rgb_bg)
{
    return in_pass && planes_ok && !rgb_bg;
}

void vc_cursor_rect(int style, int cw, int ch, int *dx, int *dy, int *w, int *h)
{
    *dx = 0;
    *dy = 0;
    *w = cw;
    *h = ch;
    if (style == 3 || style == 4) {
        *h = ch < 2 ? ch : 2;
        *dy = ch - *h;
    } else if (style == 5 || style == 6) {
        *w = cw < 2 ? cw : 2;
    }
}

static int ns_ok(int ns)
{
    return ns == 35 || ns == 70 || ns == 140;
}

int vc_sprite_geom_of(int screen_ns, int sprite_ns, int lace, int cw, int ch, vc_sprite_geom *g)
{
    long span = (long)cw * screen_ns;
    g->screen_ns = screen_ns;
    g->sprite_ns = sprite_ns;
    g->lace = lace != 0;
    g->sw = g->sh = 0;
    if (!ns_ok(screen_ns) || !ns_ok(sprite_ns) || cw < 1 || ch < 1 || span % sprite_ns)
        return 0;
    if (lace && (ch & 1))
        return 0;
    g->sw = (int)(span / sprite_ns);
    g->sh = lace ? ch / 2 : ch;
    return g->sw <= VC_SPRITE_W && g->sh <= VC_SPRITE_H;
}

int vc_sprite_pos(const vc_sprite_geom *g, long x, long y, long *sx, long *sy)
{
    if (x < 0 || y < 0 || (x * g->screen_ns) % VC_POS_NS || (g->lace && (y & 1)))
        return 0;
    *sx = x; /* V39+ MoveSprite takes the ViewPort's own pixels and lines (rig 2026-10-04: */
    *sy = y; /* lores units put the cursor at half its x on a hires screen) */
    return 1;
}

/* glyph pixel (x, row) set? 8 pixels a row, bit 7 the left */
static int glyph_bit(const unsigned char *glyph, long x, long row)
{
    return x >= 0 && x < 8 && ((glyph[row] >> (7 - x)) & 1);
}

int vc_sprite_image(const vc_sprite_geom *g, int rx, int ry, int rw, int rh, const unsigned char *glyph,
                    unsigned short *a, unsigned short *b)
{
    int i, j;
    int rows = g->lace ? 2 : 1;                    /* screen rows a sprite line covers */
    int exact = !g->lace && g->sprite_ns <= g->screen_ns; /* a sprite pixel inside one screen pixel */
    long rx0 = (long)rx * g->screen_ns, rx1 = (long)(rx + rw) * g->screen_ns;
    for (i = 0; i < g->sh; i++) {
        unsigned short pa = 0, pb = 0;
        long r0 = (long)i * rows, r1 = r0 + rows;  /* the screen rows of this line */
        if (r0 < ry)
            r0 = ry;
        if (r1 > ry + rh)
            r1 = ry + rh;
        for (j = 0; j < g->sw && r0 < r1; j++) {
            long x0 = (long)j * g->sprite_ns, x1 = x0 + g->sprite_ns; /* this pixel, in ns */
            unsigned short bit = (unsigned short)(0x8000U >> j);
            int on = 0;
            if (x1 <= rx0 || x0 >= rx1)
                continue; /* outside the rectangle: transparent */
            if (glyph) {
                if (exact) {
                    on = glyph_bit(glyph, x0 / g->screen_ns, r0);
                } else {
                    /* a coarser sprite pixel: shown only where no glyph pixel is under it */
                    long k, row;
                    for (row = r0; row < r1; row++)
                        for (k = x0 / g->screen_ns; k * g->screen_ns < x1; k++)
                            if (k >= rx && k < rx + rw && glyph_bit(glyph, k, row))
                                return 0;
                }
            }
            if (on)
                pb |= bit; /* colour 2: the glyph in the background colour */
            else
                pa |= bit; /* colour 1: the cursor colour */
        }
        a[i] = pa;
        b[i] = pb;
    }
    return 1;
}

int vc_sprite_colour_reg(int num, int even_base, int odd_base, int depth, int count)
{
    int reg;
    if (num < 0 || num > 7 || depth < 0 || depth > 8)
        return -1;
    reg = (num & 1 ? odd_base : even_base) + 4 * (num >> 1) + 1;
    if (reg < (1 << depth) || reg + 1 >= count)
        return -1;
    return reg;
}

int vc_cursor_choice(const vc_cursor_env *e)
{
    if (!e->native)
        return VC_CUR_RTG;
    if (!e->visible)
        return VC_CUR_HIDDEN;
    if (!e->geom)
        return VC_CUR_GEOM;
    if (!e->pos)
        return VC_CUR_POS;
    if (!e->plain_cell)
        return VC_CUR_CELL;
    if (!e->image)
        return VC_CUR_IMAGE;
    if (!e->colours)
        return VC_CUR_COLOURS;
    if (!e->have_sprite)
        return VC_CUR_NONE;
    return VC_CUR_SPRITE;
}
