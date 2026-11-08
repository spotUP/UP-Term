/* roundaa_core -- see roundaa_core.h */
#include "roundaa_core.h"
#include "../render/emoji.h"
#include "../render/pen_match.h"

int raa_radius(int h)
{
    return h >= FLAT_ROUND_TALL ? 4 : 2;
}

static int isqrt(int v)
{
    int r = 0;
    while ((r + 1) * (r + 1) <= v)
        r++;
    return r;
}

int raa_round_inset(int w, int h, int r, int y)
{
    int dy2, half;
    if (w <= 0 || h <= 0 || y < 0 || y >= h)
        return 0;
    if (r > h / 2)
        r = h / 2;
    if (r > w / 2)
        r = w / 2;
    if (r <= 0)
        return 0;
    if (y >= h - r)
        y = h - 1 - y;
    if (y >= r)
        return 0;
    dy2 = 2 * r - 2 * y - 1;
    half = isqrt(4 * r * r - dy2 * dy2);
    return r - (half + 1) / 2;
}

/* Samples at 1/8 pixel units: sample (i, j) of pixel (x, y) sits at (8x + 2i + 1, 8y + 2j + 1) / 8, inside the corner circle when
 * its squared distance to the centre (r, r) (in 1/8) is at most (8r)^2 */
int raa_cover(int w, int h, int r, int x, int y)
{
    int i, j, n = 0, cx, cy;
    if (w <= 0 || h <= 0 || x < 0 || y < 0 || x >= w || y >= h)
        return 0;
    if (r > h / 2)
        r = h / 2;
    if (r > w / 2)
        r = w / 2;
    if (x >= w - r)
        x = w - 1 - x;                          /* mirror onto the top left corner */
    if (y >= h - r)
        y = h - 1 - y;
    if (r <= 0 || x >= r || y >= r)
        return 255;
    for (j = 0; j < 4; j++)
        for (i = 0; i < 4; i++) {
            cx = 8 * r - (8 * x + 2 * i + 1);
            cy = 8 * r - (8 * y + 2 * j + 1);
            if (cx * cx + cy * cy <= 64 * r * r)
                n++;
        }
    return n * 255 / 16;
}

int raa_nearest(raa_rgb_fn rgb, void *ctx, int npens, unsigned long want)
{
    pm_target t;
    long bd = -1, d;
    int i, best = 0;
    pm_aim(want, &t);
    for (i = 0; i < npens; i++) {
        d = pm_dist(&t, rgb(ctx, i));
        if (bd < 0 || d < bd) {
            bd = d;
            best = i;
        }
    }
    return best;
}

int raa_frame_draw(const flat_state *s, int ids, int ox, int oy, int w, int h, int fill, int role, flat_fill_fn fn, flat_read_fn rd,
                   raa_rgb_fn rgb, int npens, void *ctx, int *blended)
{
    unsigned char bgpen[4];  /* a read pen, checked 0 .. npens-1 (npens <= RAA_MAX_PENS 256); S62-FLAT-RF-TX-BUDGET: bytes, not ints */
    int c, r, dx, dy, inner, ok, b;
    unsigned long fg;
    if (blended)
        *blended = 0;
    ok = s && fn && rd && rgb && fill && npens > 0 && npens <= RAA_MAX_PENS && w >= 3 && h >= 3;
    for (c = 0; ok && c < 4; c++) {
        b = rd(ctx, (c & 1) ? ox + w : ox - 1, (c & 2) ? oy + h : oy - 1);     /* beside the corner, before the box is painted */
        if (b < 0 || b >= npens)
            ok = 0;
        else
            bgpen[c] = (unsigned char)b;
    }
    if (!flat_frame_draw_round(s, ids, ox, oy, w, h, fill, role, fn, rd, ctx))
        return 0;
    if (!ok)
        return 1;
    r = raa_radius(h);
    if (r > h / 2)
        r = h / 2;
    if (r > w / 2)
        r = w / 2;
    inner = rd(ctx, ox + w / 2, oy + h / 2);
    if (inner < 0 || inner >= npens)
        return 1;
    fg = rgb(ctx, inner);
    for (c = 0; c < 4; c++)
        for (dy = 0; dy < r; dy++)
            for (dx = 0; dx < r; dx++) {
                int px = (c & 1) ? w - 1 - dx : dx, py = (c & 2) ? h - 1 - dy : dy;
                int cov = raa_cover(w, h, r, px, py), pen;
                if (cov >= 255)
                    continue;
                pen = cov == 0 ? bgpen[c] : raa_nearest(rgb, ctx, npens, ce_mix(fg, rgb(ctx, bgpen[c]), cov));
                fn(ctx, ox + px, oy + py, ox + px, oy + py, pen);
                if (blended && cov > 0)
                    (*blended)++;
            }
    return 1;
}

int raa_knob_frame_draw(const flat_state *s, int depth, int active, int ox, int oy, int w, int h, flat_fill_fn fn, flat_read_fn rd,
                        raa_rgb_fn rgb, int npens, void *ctx, int *blended)
{
    flat_state st;
    if (blended)
        *blended = 0;
    if (!s)
        return 0;
    st = *s;
    st.pen_button = (uint8_t)flat_knob_state_pen(s, depth, active);
    return raa_frame_draw(&st, 0, ox, oy, w, h, 1, FLAT_ROLE_PLAIN, fn, rd, rgb, npens, ctx, blended);
}
