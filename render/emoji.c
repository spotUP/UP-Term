/* emoji: see emoji.h. */
#include "emoji.h"

static unsigned long be32(const vt_u8 *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

static int present(const vt_u8 *p, int lo)
{
    return (p[12 + (lo >> 3)] & (0x80 >> (lo & 7))) != 0;
}

/* The bytes of the glyph record at g (gen_emoji.py's layout). */
static long glyph_size(const vt_u8 *g)
{
    long n = (long)g[1] + 1;
    return 2 + 4 * n + (long)(CE_W * 16 + CE_W * 8) * g[0] / 8;
}

long ce_page_length(const vt_u8 *p, long n)
{
    unsigned long len;
    if (!p || n < CE_HEADER || p[0] != 'U' || p[1] != 'C' || p[2] != 'E' || p[3] != '1')
        return -1;
    len = be32(p + 8);
    if (len < CE_HEADER || len > (unsigned long)CE_PAGE_MAX)
        return -1;
    return (long)len;
}

int ce_page_check(const vt_u8 *p, long len, int page)
{
    long off = CE_HEADER;
    int lo, count = 0;
    if (ce_page_length(p, len) != len || (((unsigned)p[4] << 8) | p[5]) != (unsigned)page)
        return 0;
    for (lo = 0; lo < 256; lo++) {
        if (!(lo & 31) && be32(p + 44 + 4 * (lo >> 5)) != (unsigned long)off)
            return 0;
        if (!present(p, lo))
            continue;
        if (off + 2 > len || (p[off] != 4 && p[off] != 8) || (p[off] == 4 && p[off + 1] > 15) ||
            p[off + 1] == 255)
            return 0; /* 4-bit indices into at most 16 entries; index 255 is the background's */
        off += glyph_size(p + off);
        count++;
    }
    return off == len && (unsigned)count == (((unsigned)p[6] << 8) | p[7]);
}

const vt_u8 *ce_page_glyph(const vt_u8 *p, int lo)
{
    unsigned long off;
    int k;
    if (lo < 0 || lo > 255 || !present(p, lo))
        return 0;
    off = be32(p + 44 + 4 * (lo >> 5));
    for (k = lo & ~31; k < lo; k++)
        if (present(p, k))
            off += (unsigned long)glyph_size(p + off);
    return p + off;
}

/* x / 255 rounded to nearest, for x up to 255 * 255 */
static vt_u32 div255(vt_u32 x)
{
    return (x + 127) / 255;
}

void ce_blend(const vt_u8 *rgba, int n, vt_u32 bg, vt_u32 *ctab)
{
    vt_u32 br = (bg >> 16) & 0xFF, bgr = (bg >> 8) & 0xFF, bb = bg & 0xFF;
    int i;
    for (i = 0; i < n && i < 256; i++, rgba += 4) {
        vt_u32 a = rgba[3], na = 255 - a;
        ctab[i] = (div255(rgba[0] * a + br * na) << 16) | (div255(rgba[1] * a + bgr * na) << 8) |
                  div255(rgba[2] * a + bb * na);
    }
    for (; i < 256; i++)
        ctab[i] = bg & 0xFFFFFFUL;
}

int ce_image(const vt_u8 *g, int bw, int bh, vt_u8 *idx)
{
    int bits = g[0], n = g[1] + 1, h, x0, y0, x, y, i;
    const vt_u8 *src = g + 2 + 4 * n;
    if (bw < CE_W || bh < 8 || (long)bw * bh > CE_BOX_MAX)
        return 0;
    h = bh >= 16 ? 16 : 8;
    if (h == 8)
        src += CE_W * 16 * bits / 8; /* past the 16x16 image */
    x0 = (bw - CE_W) / 2;
    y0 = (bh - h) / 2;
    for (i = 0; i < bw * bh; i++)
        idx[i] = CE_BG;
    for (y = 0; y < h; y++) {
        vt_u8 *d = idx + (y0 + y) * bw + x0;
        if (bits == 8) {
            for (x = 0; x < CE_W; x++)
                d[x] = src[y * CE_W + x];
        } else {
            for (x = 0; x < CE_W; x += 2) {
                vt_u8 b = src[(y * CE_W + x) >> 1];
                d[x] = (vt_u8)(b >> 4);
                d[x + 1] = (vt_u8)(b & 15);
            }
        }
    }
    return 1;
}

int ce_target_ok(const ce_target *t)
{
    return t && t->rtg && t->depth >= 15 && t->write_lut;
}

void ce_init(ce_painter *p)
{
    p->tg.rtg = 0;
    p->tg.depth = 0;
    p->tg.write_lut = 0;
    p->tg.user = 0;
    p->store = 0;
    p->cw = p->ch = 0;
    p->cur = 0;
    p->drawn = 0;
}

void ce_set_cell(ce_painter *p, int cw, int ch)
{
    p->cw = cw;
    p->ch = ch;
}

int ce_has(void *painter, vt_u32 cp, int cells)
{
    ce_painter *p = (ce_painter *)painter;
    const vt_u8 *pg;
    long len;
    p->cur = 0;
    if (cells != 2 || !p->store || !ce_target_ok(&p->tg) || cp >= (vt_u32)UF_PAGES << 8)
        return 0;
    if (2 * p->cw < CE_W || p->ch < 8 || 2L * p->cw * p->ch > CE_BOX_MAX)
        return 0; /* a box the images do not fit: no page read for it */
    if (!(pg = uf_page(&p->store->pages, (int)(cp >> 8), &len)))
        return 0;
    p->cur = ce_page_glyph(pg, (int)(cp & 0xFF));
    return p->cur != 0;
}

int ce_paint(ce_painter *p, int px, int py, vt_u32 bg)
{
    int bw = 2 * p->cw;
    ce_store *s = p->store;
    if (!p->cur || !s || !ce_target_ok(&p->tg) || !ce_image(p->cur, bw, p->ch, s->idx))
        return 0;
    ce_blend(p->cur + 2, p->cur[1] + 1, bg, s->ctab);
    p->tg.write_lut(p->tg.user, s->idx, bw, p->ch, s->ctab, px, py);
    p->drawn++;
    return 1;
}
