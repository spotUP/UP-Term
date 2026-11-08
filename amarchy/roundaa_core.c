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

unsigned raa_partial_mask(int w, int h, int r)
{
    unsigned m = 0;
    int x, y;
    for (y = 0; y < r && y < 4; y++)
        for (x = 0; x < r && x < 4; x++)
            if (raa_cover(w, h, r, x, y) < 255)
                m |= 1u << (y * 4 + x);
    return m;
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

/* The corner blend of raa_frame_draw, after the box is painted: each corner pixel the rounded box does not fully cover takes the
 * mix of the box's inner pen and the pen read beside that corner (bg: corner c's pen in bits 8c .. 8c+7, as raa_frame_box packs it).
 * S62-FLAT-RF-TX-BUDGET: the box half and this half run one after the other, not nested, so a caller on a foreign task's stack
 * (flat.c bb_draw) holds no raa_frame_draw frame under the nearest-pen search. */
/* Pixel k (row k / r, column k % r, from the corner) of corner c of the job's box, corner radius r, inner colour fg: a pixel the
 * rounded box does not fully cover takes the mix of fg and the pen beside the corner. Returns 1 when it blended (coverage 1..254).
 * A helper of its own so the corner loop keeps few values live (S62-FLAT-RF-TX-BUDGET). */
static int raa_corner_px(const raa_job *j, unsigned long fg, int r, int c, int k)
{
    int w = j->w, h = j->h, dx = k % r, dy = k / r;
    int px = (c & 1) ? w - 1 - dx : dx, py = (c & 2) ? h - 1 - dy : dy;
    int cov = raa_cover(w, h, r, px, py), bp, pen;
    if (cov >= 255)
        return 0;
    bp = (int)((j->bg >> (8 * c)) & 255);
    pen = cov == 0 ? bp : raa_nearest(j->rgb, j->ctx, j->npens, ce_mix(fg, j->rgb(j->ctx, bp), cov));
    j->fn(j->ctx, j->ox + px, j->oy + py, j->ox + px, j->oy + py, pen);
    return cov > 0;
}

void raa_frame_corners(const raa_job *j, int *blended)
{
    int c, k, r, inner, n = 0;
    unsigned long fg;
    r = raa_radius(j->h);
    if (r > j->h / 2)
        r = j->h / 2;
    if (r > j->w / 2)
        r = j->w / 2;
    inner = j->rd(j->ctx, j->ox + j->w / 2, j->oy + j->h / 2);
    if (inner < 0 || inner >= j->npens)
        return;
    fg = j->rgb(j->ctx, inner);
    for (c = 0; c < 4; c++)
        for (k = 0; k < r * r; k++)        /* row by row, as the box is painted */
            n += raa_corner_px(j, fg, r, c, k);
    if (blended)
        *blended += n;
}

int raa_frame_box(raa_job *j)
{
    int c, ok, b;
    ok = j->s && j->fn && j->rd && j->rgb && j->fill && j->npens > 0 && j->npens <= RAA_MAX_PENS && j->w >= 3 && j->h >= 3;
    j->bg = 0;                                 /* a read pen, checked 0 .. npens-1 (npens <= RAA_MAX_PENS 256): one byte per corner */
    for (c = 0; ok && c < 4; c++) {
        b = j->rd(j->ctx, (c & 1) ? j->ox + j->w : j->ox - 1, (c & 2) ? j->oy + j->h : j->oy - 1);   /* beside the corner, before the box is painted */
        if (b < 0 || b >= j->npens)
            ok = 0;
        else
            j->bg |= (unsigned long)b << (8 * c);
    }
    if (!flat_frame_draw_round(j->s, j->ids, j->ox, j->oy, j->w, j->h, j->fill, j->role, j->fn, j->rd, j->ctx))
        return RAA_BOX_NONE;
    return ok ? RAA_BOX_BLEND : RAA_BOX_PLAIN;
}

int raa_frame_draw(const flat_state *s, int ids, int ox, int oy, int w, int h, int fill, int role, flat_fill_fn fn, flat_read_fn rd,
                   raa_rgb_fn rgb, int npens, void *ctx, int *blended)
{
    raa_job j;
    int k;
    if (blended)
        *blended = 0;
    j.s = s; j.fn = fn; j.rd = rd; j.rgb = rgb; j.ctx = ctx;
    j.ids = ids; j.ox = ox; j.oy = oy; j.w = w; j.h = h; j.fill = fill; j.role = role; j.npens = npens;
    k = raa_frame_box(&j);
    if (k == RAA_BOX_BLEND)
        raa_frame_corners(&j, blended);
    return k != RAA_BOX_NONE;
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
