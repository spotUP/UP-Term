/* unifont: see unifont.h. */
#include "unifont.h"

#define UF_ASCENT_ROW 13 /* Unifont's last row above the baseline (FONT_ASCENT 14) */

static unsigned be16(const vt_u8 *p)
{
    return ((unsigned)p[0] << 8) | p[1];
}

static int bit(const vt_u8 *set, int i)
{
    return (set[i >> 3] & (0x80 >> (i & 7))) != 0;
}

void uf_page_name(int page, char *out)
{
    static const char hex[] = "0123456789ABCDEF";
    out[0] = hex[(page >> 4) & 15];
    out[1] = hex[page & 15];
    out[2] = 0;
}

int uf_page_check(const vt_u8 *p, long len, int page)
{
    long off = UF_HEADER;
    int lo, n = 0;
    if (!p || len < UF_HEADER || len > UF_PAGE_MAX)
        return 0;
    if (p[0] != 'U' || p[1] != 'F' || p[2] != 'P' || p[3] != '1' || p[4] != page || p[5] != 1)
        return 0;
    for (lo = 0; lo < 256; lo++) {
        if (!(lo & 31) && be16(p + 72 + 2 * (lo >> 5)) != (unsigned)off)
            return 0;
        if (bit(p + 40, lo) && !bit(p + 8, lo))
            return 0; /* wide but not there */
        if (bit(p + 8, lo)) {
            off += bit(p + 40, lo) ? 32 : 16;
            n++;
        }
    }
    return off == len && (unsigned)n == be16(p + 6);
}

const vt_u8 *uf_page_glyph(const vt_u8 *p, int lo, int *wide)
{
    unsigned off;
    int k;
    *wide = 0;
    if (lo < 0 || lo > 255 || !bit(p + 8, lo))
        return 0;
    off = be16(p + 72 + 2 * (lo >> 5));
    for (k = lo & ~31; k < lo; k++)
        if (bit(p + 8, k))
            off += bit(p + 40, k) ? 32 : 16;
    *wide = bit(p + 40, lo);
    return p + off;
}

/* Row r (0-15) of a glyph as 16 bits, its leftmost pixel the top bit. */
static unsigned glyph_row(const vt_u8 *g, int wide, int r)
{
    return wide ? ((unsigned)g[2 * r] << 8) | g[2 * r + 1] : (unsigned)g[r] << 8;
}

int uf_make_mask(const vt_u8 *g, int wide, int cw, int ch, int base, int cells, vt_u8 *mask, int bpr,
                 int size)
{
    int w = (cells == 2 ? 2 : 1) * cw, gw = wide ? 16 : 8, hs, ew, x0, shift, y, i;
    if ((ch != 16 && ch != 8) || cw < 1 || bpr * 8 < w || (long)bpr * ch > size)
        return 0;
    for (i = 0; i < bpr * ch; i++)
        mask[i] = 0;
    /* across: 1:1, or pixel pairs OR'd when the glyph is twice as wide as
     * the cells (a 16x16 glyph in one 8-pixel cell); centred, a wider one
     * cut on both sides */
    hs = gw >= 2 * w ? 2 : 1;
    ew = gw / hs;
    x0 = (w - ew) / 2;
    /* down: 1:1 at 16, pairs of rows OR'd at 8; Unifont's last ascent row
     * on the font's baseline row */
    shift = base - (ch == 16 ? UF_ASCENT_ROW : UF_ASCENT_ROW / 2);
    for (y = 0; y < ch; y++) {
        int sy = y - shift, dx;
        unsigned row;
        if (sy < 0 || sy >= ch)
            continue;
        row = ch == 16 ? glyph_row(g, wide, sy) : glyph_row(g, wide, 2 * sy) | glyph_row(g, wide, 2 * sy + 1);
        if (!row)
            continue;
        for (dx = 0; dx < w; dx++) {
            int sx = dx - x0;
            unsigned m;
            if (sx < 0 || sx >= ew)
                continue;
            m = (0x8000u >> (sx * hs)) | (hs == 2 ? 0x8000u >> (sx * hs + 1) : 0);
            if (row & m)
                mask[y * bpr + (dx >> 3)] |= (vt_u8)(0x80 >> (dx & 7));
        }
    }
    return 1;
}

void uf_init(uf_cache *c, uf_load_fn load, uf_release_fn release, void *user, vt_u8 *mask, int mask_size)
{
    int i;
    char *p = (char *)c;
    for (i = 0; i < (int)sizeof(*c); i++)
        p[i] = 0;
    c->load = load;
    c->release = release;
    c->user = user;
    c->mask = mask;
    c->mask_size = mask_size;
}

void uf_flush(uf_cache *c)
{
    int i;
    for (i = 0; i < UF_SLOTS; i++) {
        if (c->slot[i].buf && c->release)
            c->release(c->user, c->slot[i].buf);
        c->slot[i].buf = 0;
        c->slot[i].len = 0;
    }
    for (i = 0; i < 32; i++)
        c->absent[i] = 0; /* the pages may be installed by now */
}

void uf_set_cell(uf_cache *c, int cw, int ch, int base)
{
    c->cw = cw;
    c->ch = ch;
    c->base = base;
}

/* page's slot, its file read now when it is not in one (the least recently
 * used slot given up for it); 0 when there is no good file */
static uf_slot *page_slot(uf_cache *c, int page)
{
    uf_slot *s = 0;
    vt_u8 *buf = 0;
    long n;
    int i;
    for (i = 0; i < UF_SLOTS; i++)
        if (c->slot[i].buf && c->slot[i].page == page)
            return &c->slot[i];
    if (c->absent[page >> 3] & (0x80 >> (page & 7)) || !c->load)
        return 0;
    n = c->load(c->user, page, &buf);
    c->loads++;
    if (n == -2)
        return 0; /* not now: asked again next time */
    if (n < 0 || !uf_page_check(buf, n, page)) {
        if (n >= 0 && buf && c->release)
            c->release(c->user, buf);
        c->absent[page >> 3] |= (vt_u8)(0x80 >> (page & 7));
        return 0;
    }
    s = &c->slot[0];
    for (i = 0; i < UF_SLOTS; i++) {
        if (!c->slot[i].buf) {
            s = &c->slot[i]; /* an empty one */
            break;
        }
        if (c->slot[i].used < s->used)
            s = &c->slot[i];
    }
    if (s->buf && c->release)
        c->release(c->user, s->buf);
    s->buf = buf;
    s->len = n;
    s->page = page;
    return s;
}

const vt_u8 *uf_glyph(void *cache, vt_u32 cp, int cells, int *bpr)
{
    uf_cache *c = (uf_cache *)cache;
    uf_slot *s;
    const vt_u8 *g;
    int wide, b;
    if (!c || !c->mask || cp > 0xFFFF || (c->ch != 16 && c->ch != 8) || c->cw < 1)
        return 0; /* a cell Unifont cannot be drawn at: no file read for it */
    if (!(s = page_slot(c, (int)(cp >> 8))))
        return 0;
    s->used = ++c->clock;
    if (!(g = uf_page_glyph(s->buf, (int)(cp & 0xFF), &wide)))
        return 0;
    cells = cells == 2 ? 2 : 1;
    b = ((cells * c->cw + 15) >> 4) << 1;
    if (!uf_make_mask(g, wide, c->cw, c->ch, c->base, cells, c->mask, b, c->mask_size))
        return 0;
    *bpr = b;
    return c->mask;
}
