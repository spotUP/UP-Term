/* painter.c -- see painter.h. */
#include "painter.h"

/* One plane, one scanline: the n bytes `v` (already the plane's pattern)
 * at bit phase s into dst, keeping the bits around them. */
static void put_bits(vp_u8 *dst, int s, const vp_u8 *v, int n)
{
    unsigned int acc = 0;
    int i;
    if (!s) {
        for (i = 0; i < n; i++)
            dst[i] = v[i];
        return;
    }
    /* the first byte keeps its top s bits, the one after the run its low 8-s */
    dst[0] = (vp_u8)((dst[0] & (0xFF00 >> s)) | (v[0] >> s));
    acc = v[0];
    for (i = 1; i < n; i++) {
        acc = ((acc << 8) | v[i]) & 0xFFFF;
        dst[i] = (vp_u8)(acc >> s);
    }
    dst[n] = (vp_u8)((dst[n] & (0xFF >> s)) | ((v[n - 1] << (8 - s)) & 0xFF));
}

void vp_span(vp_u8 **planes, int depth, long bpr, long x, long y, const vp_u8 *glyphs, int h,
             const vp_u8 *chars, int n, int fg, int bg, int mask)
{
    vp_u8 v[256];
    int p, r, i, s = (int)(x & 7);
    if (n <= 0 || n > 256)
        return;
    for (p = 0; p < depth; p++) {
        int fb = (fg >> p) & 1, bb = (bg >> p) & 1;
        vp_u8 *row;
        if (!((mask >> p) & 1))
            continue;
        row = planes[p] + y * bpr + (x >> 3);
        for (r = 0; r < h; r++, row += bpr) {
            for (i = 0; i < n; i++) {
                vp_u8 g = glyphs[chars[i] * h + r];
                v[i] = (vp_u8)(fb == bb ? (fb ? 0xFF : 0) : fb ? g : (vp_u8)~g);
            }
            put_bits(row, s, v, n);
        }
    }
}

void vp_fill(vp_u8 **planes, int depth, long bpr, long x, long y, int h, int n, int pen, int mask)
{
    vp_u8 v[256];
    int p, r, i, k;
    for (; n > 0; n -= k, x += 8L * k) {
        k = n < 256 ? n : 256;
        for (p = 0; p < depth; p++) {
            vp_u8 *row;
            if (!((mask >> p) & 1))
                continue;
            for (i = 0; i < k; i++)
                v[i] = (vp_u8)((pen >> p) & 1 ? 0xFF : 0);
            row = planes[p] + y * bpr + (x >> 3);
            for (r = 0; r < h; r++, row += bpr)
                put_bits(row, (int)(x & 7), v, k);
        }
    }
}

#ifdef VP_ASM
void vp_asm_plane(vp_u8 *dst, long bpr, long s, const vp_u8 **gp, long n, long h, long pattern, long constant);

void vp_span_fast(vp_u8 **planes, int depth, long bpr, long x, long y, const vp_u8 *glyphs, int h,
                  const vp_u8 *chars, int n, int fg, int bg, int mask)
{
    const vp_u8 *gp[256];
    long off = y * bpr + (x >> 3);
    int p, i;
    if (n <= 0 || n > 256)
        return;
    for (i = 0; i < n; i++)
        gp[i] = glyphs + chars[i] * h;
    for (p = 0; p < depth; p++) {
        int fb = (fg >> p) & 1, bb = (bg >> p) & 1;
        if (!((mask >> p) & 1))
            continue;
        if (fb == bb)
            vp_asm_plane(planes[p] + off, bpr, x & 7, gp, n, h, fb ? -1L : 0L, 1);
        else
            vp_asm_plane(planes[p] + off, bpr, x & 7, gp, n, h, fb ? 0L : -1L, 0);
    }
}

/* the plane loop's constant path: it reads no glyphs */
void vp_fill_fast(vp_u8 **planes, int depth, long bpr, long x, long y, int h, int n, int pen, int mask)
{
    long off = y * bpr + (x >> 3);
    int p;
    if (n <= 0 || h <= 0)
        return;
    for (p = 0; p < depth; p++)
        if ((mask >> p) & 1)
            vp_asm_plane(planes[p] + off, bpr, x & 7, (const vp_u8 **)0, n, h, (pen >> p) & 1 ? -1L : 0L, 1);
}
#else
void vp_span_fast(vp_u8 **planes, int depth, long bpr, long x, long y, const vp_u8 *glyphs, int h,
                  const vp_u8 *chars, int n, int fg, int bg, int mask)
{
    vp_span(planes, depth, bpr, x, y, glyphs, h, chars, n, fg, bg, mask);
}

void vp_fill_fast(vp_u8 **planes, int depth, long bpr, long x, long y, int h, int n, int pen, int mask)
{
    vp_fill(planes, depth, bpr, x, y, h, n, pen, mask);
}
#endif
