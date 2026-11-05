/* iclabel_core -- see iclabel_core.h. C89, no OS calls. */
#include <string.h>
#include "iclabel_core.h"
#include "iconpen_core.h"
#include "../render/aa_ink_rule.h"

int icl_accept(const char *task, int mode, int apen, int bpen, int x, int y, int w, int h, int il, int it, int ir, int ib)
{
    if (!task || (strcmp(task, "Workbench") != 0 && strcmp(task, "dopus") != 0))
        return 0;
    if (mode & (ICL_COMPLEMENT | ICL_INVERSVID))
        return 0;
    if (apen == bpen)
        return 0;
    if (w <= 0 || h <= 0 || w > ICL_MAXW || h > ICL_MAXH)
        return 0;
    if (x < il || y < it || x + w - 1 > ir || y + h - 1 > ib)
        return 0;
    return 1;
}

static long icl_rgbdist(uint32_t a, uint32_t b)
{
    long dr = (long)((a >> 16) & 0xFF) - (long)((b >> 16) & 0xFF);
    long dg = (long)((a >> 8) & 0xFF) - (long)((b >> 8) & 0xFF);
    long db = (long)(a & 0xFF) - (long)(b & 0xFF);
    return dr * dr + dg * dg + db * db;
}

int icl_mask(int jam2, int exact, const uint32_t *before, const uint32_t *after, uint32_t apen, uint32_t bpen, int n,
             uint8_t *mask)
{
    int i, glyph = 0;
    for (i = 0; i < n; i++) {
        int g;
        if (jam2) {
            if (exact)
                g = after[i] == apen;
            else
                g = icl_rgbdist(after[i], apen) < icl_rgbdist(after[i], bpen);
        } else {
            g = after[i] != before[i];
        }
        mask[i] = (uint8_t)(g ? 1 : 0);
        glyph += g;
    }
    return glyph;
}

int icl_coverage(const uint8_t *mask, int w, int h, uint8_t *cov)
{
    int x, y, partial = 0;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int i = y * w + x, nn, ss, ee, ww, corners;
            if (mask[i]) {
                cov[i] = 255;
                continue;
            }
            nn = y > 0 && mask[i - w];
            ss = y < h - 1 && mask[i + w];
            ww = x > 0 && mask[i - 1];
            ee = x < w - 1 && mask[i + 1];
            corners = (nn && ee) + (nn && ww) + (ss && ee) + (ss && ww);
            cov[i] = (uint8_t)(corners == 0 ? 0 : corners == 1 ? ICL_COV_ONE : ICL_COV_TWO);
            if (corners)
                partial++;
        }
    }
    return partial;
}

int icl_fg(const uint8_t *mask, const uint32_t *after, int n, uint32_t *fg)
{
    int i;
    for (i = 0; i < n; i++)
        if (mask[i]) {
            *fg = after[i];
            return 1;
        }
    return 0;
}

int icl_blend_rgb(const uint8_t *cov, const uint32_t *after, uint32_t fg, int n, uint32_t *out)
{
    int i, blended = 0;
    for (i = 0; i < n; i++) {
        uint32_t a = after[i];
        if (cov[i] > 0 && cov[i] < 255) {
            out[i] = (uint32_t)ce_mix((vt_u32)(fg & 0xFFFFFFu), (vt_u32)(a & 0xFFFFFFu), cov[i]);
            blended++;
        } else {
            out[i] = a;
        }
    }
    return blended;
}

uint32_t icl_pen_target(uint32_t fg_rgb, uint32_t bg_rgb, int k, int n)
{
    return (uint32_t)ce_mix((vt_u32)fg_rgb, (vt_u32)bg_rgb, aa_ink_level(k, n));
}

int icl_pen_choice(int k, int n, int pen, long d_pen, long d_fg, long d_bg, int fgpen, int bgpen)
{
    if (pen >= 0 && aa_ink_shade_accept(d_pen, d_fg, d_bg))
        return pen;
    return 2 * k >= n + 1 ? fgpen : bgpen;
}

int icl_blend_pens(const uint8_t *cov, const uint32_t *after, int fgpen, int n, icl_pen_fn pen_for, void *u, int count,
                   uint32_t *out)
{
    int i, changed = 0;
    for (i = 0; i < count; i++) {
        uint32_t a = after[i];
        int c = cov[i], k, p;
        out[i] = a;
        if (c == 0 || c == 255)
            continue;
        k = aa_ink_index(c, n);
        if (k <= 0)
            continue;
        p = k >= n + 1 ? fgpen : pen_for(u, fgpen, (int)a, k);
        if (p >= 0 && (uint32_t)p != a) {
            out[i] = (uint32_t)p;
            changed++;
        }
    }
    return changed;
}

void icl_cache_clear(icl_pencache *c)
{
    memset(c, 0, sizeof(*c));
}

int icl_cache_find(const icl_pencache *c, int fgpen, int bgpen, int k, uint32_t fg_rgb, uint32_t bg_rgb)
{
    int i;
    for (i = 0; i < ICL_CACHE; i++) {
        const icl_pen_entry *e = &c->e[i];
        if (e->used && e->fgpen == fgpen && e->bgpen == bgpen && e->k == k)
            return e->fg_rgb == fg_rgb && e->bg_rgb == bg_rgb ? e->pen : -1;
    }
    return -1;
}

int icl_cache_has_room(const icl_pencache *c, int fgpen, int bgpen, int k)
{
    int i;
    for (i = 0; i < ICL_CACHE; i++)
        if (!c->e[i].used || (c->e[i].fgpen == fgpen && c->e[i].bgpen == bgpen && c->e[i].k == k))
            return 1;
    return 0;
}

int icl_cache_put(icl_pencache *c, int fgpen, int bgpen, int k, uint32_t fg_rgb, uint32_t bg_rgb, int pen, int obtained,
                  int *old_pen)
{
    int i, slot = -1;
    *old_pen = -1;
    for (i = 0; i < ICL_CACHE; i++) {
        icl_pen_entry *e = &c->e[i];
        if (e->used && e->fgpen == fgpen && e->bgpen == bgpen && e->k == k) {
            if (e->obtained)
                *old_pen = e->pen;
            slot = i;
            break;
        }
        if (!e->used && slot < 0)
            slot = i;
    }
    if (slot < 0)
        return 0;
    c->e[slot].fgpen = (int16_t)fgpen;
    c->e[slot].bgpen = (int16_t)bgpen;
    c->e[slot].k = (int16_t)k;
    c->e[slot].pen = (int16_t)pen;
    c->e[slot].fg_rgb = fg_rgb;
    c->e[slot].bg_rgb = bg_rgb;
    c->e[slot].obtained = (uint8_t)(obtained ? 1 : 0);
    c->e[slot].used = 1;
    return 1;
}

int icl_cache_drain(icl_pencache *c, int *out)
{
    int i, n = 0;
    for (i = 0; i < ICL_CACHE; i++)
        if (c->e[i].used && c->e[i].obtained)
            out[n++] = c->e[i].pen;
    icl_cache_clear(c);
    return n;
}

int icl_cache_bind(icl_pencache *c, const void *screen, unsigned epoch)
{
    if (screen && c->screen == screen && c->epoch == epoch)
        return 0;
    icl_cache_clear(c);                 /* another identity: forget, never release */
    c->screen = screen;
    c->epoch = epoch;
    return 1;
}

int icl_cache_verify(icl_pencache *c, int (*is_free)(void *ctx, int pen), void *ctx)
{
    int pens[ICL_CACHE], i, n = 0;
    const void *screen = c->screen;
    unsigned epoch = c->epoch;
    for (i = 0; i < ICL_CACHE; i++)
        if (c->e[i].used && c->e[i].obtained)       /* a fg/bg pen the entry merely names is not ours to probe */
            pens[n++] = c->e[i].pen;
    if (!ip_any_free(is_free, ctx, pens, n))
        return 0;
    icl_cache_clear(c);                 /* the pens went with the old ColorMap: forget, never release */
    c->screen = screen;
    c->epoch = epoch;
    return 1;
}

int icl_cache_release(icl_pencache *c, const void *screen, unsigned epoch, int *out)
{
    int n = 0;
    if (screen && c->screen == screen && c->epoch == epoch)
        n = icl_cache_drain(c, out);
    icl_cache_clear(c);
    return n;
}
