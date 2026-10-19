/* painter.c -- see painter.h.
 *
 * vp_span_long is a port of the design of CTerm's planar painter
 * (src/ppaint.s, draw.c pp_row / pp_cells; creep-cterm,
 * MIT licence, Copyright (c) 2026 Tobias Karlsson; the licence text is in
 * the third-party notice, dist/LICENSES.txt): the cells' glyph bits are
 * set up once a scanline, already shifted to the span's bit phase, in whole
 * longs; every plane then gets those longs written once, (glyph AND A) XOR X,
 * with no read-modify-write but at the two edge longs. */
#include "painter.h"

unsigned long vp_long_spans; /* sentinel: spans vp_span_long drew */

#if defined(__VBCC__) || defined(__m68k__)
#define VP_GET32(p) (*(const vp_u32 *)(p))
#define VP_PUT32(p, v) (*(vp_u32 *)(p) = (v))
#else
static vp_u32 vp_get32(const vp_u8 *p)
{
    return ((vp_u32)p[0] << 24) | ((vp_u32)p[1] << 16) | ((vp_u32)p[2] << 8) | p[3];
}
static void vp_put32(vp_u8 *p, vp_u32 v)
{
    p[0] = (vp_u8)(v >> 24);
    p[1] = (vp_u8)(v >> 16);
    p[2] = (vp_u8)(v >> 8);
    p[3] = (vp_u8)v;
}
#define VP_GET32(p) vp_get32(p)
#define VP_PUT32(p, v) vp_put32((p), (v))
#endif

#define VP_MAXL 67

int vp_span_long(vp_u8 **planes, int depth, long bpr, long x, long y, const vp_u8 *glyphs, int h,
                 const vp_u8 *chars, int n, int fg, int bg, int mask)
{
    vp_u32 g[VP_MAXL], pa[8], px[8], e0, e1;
    int p, r, i, ph, nl, last;
    long l0, off;
    if (n <= 0 || n > 256 || (bpr & 3) || depth > 8)
        return 0;
    for (p = 0; p < depth; p++)
        if (((mask >> p) & 1) && ((unsigned long)planes[p] & 3))
            return 0;
    ph = (int)(x & 31);
    l0 = x >> 5;
    nl = (int)(((x + 8L * n - 1) >> 5) - l0 + 1);
    last = (int)((x + 8L * n - 1) & 31);
    e0 = 0xFFFFFFFFUL >> ph;
    e1 = 0xFFFFFFFFUL << (31 - last);
    if (nl == 1)
        e0 &= e1;
    for (p = 0; p < depth; p++) {
        int fb = (fg >> p) & 1, bb = (bg >> p) & 1;
        pa[p] = fb == bb ? 0UL : 0xFFFFFFFFUL;
        px[p] = fb == bb ? (fb ? 0xFFFFFFFFUL : 0UL) : (fb ? 0UL : 0xFFFFFFFFUL);
    }
    off = y * bpr + l0 * 4;
    for (r = 0; r < h; r++, off += bpr) {
        for (i = 0; i < nl; i++)
            g[i] = 0;
        for (i = 0; i < n; i++) {
            int pos = ph + 8 * i, sh = pos & 31;
            vp_u32 v = (vp_u32)glyphs[chars[i] * h + r] << 24;
            g[pos >> 5] |= v >> sh;
            if (sh > 24)
                g[(pos >> 5) + 1] |= v << (32 - sh);
        }
        for (p = 0; p < depth; p++) {
            vp_u8 *d;
            int j;
            if (!((mask >> p) & 1))
                continue;
            d = planes[p] + off;
            if (nl == 1) {
                vp_u32 o = VP_GET32(d);
                VP_PUT32(d, o ^ ((((g[0] & pa[p]) ^ px[p]) ^ o) & e0));
                continue;
            }
            {
                vp_u32 o = VP_GET32(d);
                VP_PUT32(d, o ^ ((((g[0] & pa[p]) ^ px[p]) ^ o) & e0));
            }
            for (j = 1; j < nl - 1; j++)
                VP_PUT32(d + 4 * j, (g[j] & pa[p]) ^ px[p]);
            {
                vp_u32 o = VP_GET32(d + 4 * (nl - 1));
                VP_PUT32(d + 4 * (nl - 1), o ^ ((((g[nl - 1] & pa[p]) ^ px[p]) ^ o) & e1));
            }
        }
    }
    vp_long_spans++;
    return 1;
}

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
    if (vp_span_long(planes, depth, bpr, x, y, glyphs, h, chars, n, fg, bg, mask))
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
#else
void vp_span_fast(vp_u8 **planes, int depth, long bpr, long x, long y, const vp_u8 *glyphs, int h,
                  const vp_u8 *chars, int n, int fg, int bg, int mask)
{
    if (!vp_span_long(planes, depth, bpr, x, y, glyphs, h, chars, n, fg, bg, mask))
        vp_span(planes, depth, bpr, x, y, glyphs, h, chars, n, fg, bg, mask);
}
#endif
