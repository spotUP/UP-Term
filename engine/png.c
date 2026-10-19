/*
 * png.c - PNG pictures (f=100) unpacked by the engine itself, so a system
 * with no picture.datatype still shows them. Non-interlaced, colour types
 * 0 2 3 4 6, bit depths 1 2 4 8 (16 keeps the high byte), PLTE, tRNS, all
 * five filters, several IDAT chunks joined, built on vt_zinflate. The CRCs
 * are not checked: the stream came over a reliable pipe, and a flipped bit
 * shows as a wrong pixel, not a crash.
 *
 * Copyright (c) 2026 the vtcon authors, MIT licence.
 *
 * Included by kitty.c (one translation unit with the engine), after
 * inflate.c; KIT_RAW_MAX is defined there.
 */

#define PNG_MAX_PIXELS (KIT_RAW_MAX / 4)

static vt_u32 png_be32(const vt_u8 *p)
{
    return (vt_u32)p[0] << 24 | (vt_u32)p[1] << 16 | (vt_u32)p[2] << 8 | p[3];
}

/* The next chunk at *pos (length, 4 type bytes, data, CRC) of d[0..n):
 * 1 and its data pointer and length, 0 when none is left or it runs past
 * the end. */
static int png_chunk(const vt_u8 *d, long n, long *pos, const vt_u8 **type, long *len)
{
    vt_u32 l;
    if (n - *pos < 12)
        return 0;
    l = png_be32(d + *pos);
    if (l > (vt_u32)(n - *pos - 12))
        return 0;
    *type = d + *pos + 4;
    *len = (long)l;
    *pos += 12 + (long)l;
    return 1;
}

static int png_is(const vt_u8 *type, const char *s)
{
    return type[0] == (vt_u8)s[0] && type[1] == (vt_u8)s[1] && type[2] == (vt_u8)s[2] &&
           type[3] == (vt_u8)s[3];
}

static int png_paeth(int a, int b, int c)
{
    int p = a + b - c, pa = p > a ? p - a : a - p, pb = p > b ? p - b : b - p,
        pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc)
        return a;
    return pb <= pc ? b : c;
}

/* Sample i of a row at depth bits: the raw value (16 bits whole). */
static int png_sample(const vt_u8 *row, long i, int depth)
{
    if (depth == 16)
        return row[2 * i] << 8 | row[2 * i + 1];
    if (depth == 8)
        return row[i];
    return (row[i * depth / 8] >> (8 - depth - (int)(i * depth % 8))) & ((1 << depth) - 1);
}

/* Undo the filters of h rows of stride bytes (a filter byte, then bpp-spaced
 * data) in place; 0 for a filter byte that is none of the five. */
static int png_unfilter(vt_u8 *raw, long h, long stride, int bpp)
{
    long y, i;
    for (y = 0; y < h; y++) {
        vt_u8 *row = raw + y * stride + 1;
        const vt_u8 *up = y ? row - stride : 0;
        int f = row[-1];
        for (i = 0; i < stride - 1; i++) {
            int a = i >= bpp ? row[i - bpp] : 0, b = up ? up[i] : 0,
                c = (up && i >= bpp) ? up[i - bpp] : 0, x = row[i];
            switch (f) {
            case 0: break;
            case 1: x += a; break;
            case 2: x += b; break;
            case 3: x += (a + b) / 2; break;
            case 4: x += png_paeth(a, b, c); break;
            default: return 0;
            }
            row[i] = (vt_u8)x;
        }
    }
    return 1;
}

/* d[0..n) a PNG file into *argb (VT_MALLOC'd 0xAARRGGBB words, *w x *h).
 * 1 decoded; 0 broken (a PNG that cannot be a picture); -1 not for this
 * decoder: no PNG signature (another format, the datatypes may know it) or
 * interlaced; -2 a picture over the size limit, refused by every decoder. */
static int png_decode(const vt_u8 *d, long n, vt_u32 **argb, int *w, int *h)
{
    static const vt_u8 sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    const vt_u8 *type, *c, *plte = 0, *trns = 0;
    long pos = 8, len, nplte = 0, ntrns = 0, zlen = 0, rawlen, stride, x, y, zn;
    int depth, ctype, chan, bits, bpp, i, ok = 0, seen_idat = 0;
    vt_u32 pw, ph, *out = 0;
    vt_u8 *z = 0, *raw = 0;
    if (n < 8)
        return -1;
    for (i = 0; i < 8; i++)
        if (d[i] != sig[i])
            return -1;
    if (!png_chunk(d, n, &pos, &type, &len) || !png_is(type, "IHDR") || len != 13)
        return 0;
    c = type + 4;
    pw = png_be32(c);
    ph = png_be32(c + 4);
    depth = c[8];
    ctype = c[9];
    if (c[10] || c[11] || c[12] > 1)
        return 0;
    if (c[12] == 1)
        return -1;
    chan = ctype == 2 ? 3 : ctype == 4 ? 2 : ctype == 6 ? 4 : 1;
    if (ctype != 0 && ctype != 2 && ctype != 3 && ctype != 4 && ctype != 6)
        return 0;
    if ((depth != 1 && depth != 2 && depth != 4 && depth != 8 && depth != 16) ||
        (ctype == 3 && depth == 16) || (ctype != 0 && ctype != 3 && depth < 8))
        return 0;
    if (!pw || !ph)
        return 0;
    if (pw > (vt_u32)PNG_MAX_PIXELS || ph > (vt_u32)PNG_MAX_PIXELS ||
        (long)pw * (long)ph > PNG_MAX_PIXELS)
        return -2;                      /* too big for anyone: never handed to the datatypes */
    bits = chan * depth;
    bpp = bits >= 8 ? bits / 8 : 1;
    stride = ((long)pw * bits + 7) / 8 + 1;
    rawlen = stride * (long)ph;
    /* the chunks: the sizes of the IDATs first, then a pass to join them */
    for (i = 0; i < 2; i++) {
        long p = 33, at = 0;
        while (png_chunk(d, n, &p, &type, &len)) {
            c = type + 4;
            if (png_is(type, "IDAT")) {
                if (i == 0)
                    zlen += len;
                else {
                    long k;
                    for (k = 0; k < len; k++)
                        z[at + k] = c[k];
                    at += len;
                }
                seen_idat = 1;
            } else if (i == 0 && png_is(type, "PLTE") && len % 3 == 0 && len <= 768) {
                plte = c;
                nplte = len / 3;
            } else if (i == 0 && png_is(type, "tRNS")) {
                trns = c;
                ntrns = len;
            } else if (png_is(type, "IEND"))
                break;
        }
        if (i == 0) {
            if (!seen_idat || zlen < 2 || (ctype == 3 && !nplte))
                return 0;
            z = (vt_u8 *)VT_MALLOC(zlen);
            if (!z)
                return 0;
        }
    }
    zn = zlen;
    raw = (vt_u8 *)VT_MALLOC(rawlen);
    if (!raw)
        goto done;
    if (vt_zinflate(z, zn, raw, rawlen) != rawlen || !png_unfilter(raw, (long)ph, stride, bpp))
        goto done;
    out = (vt_u32 *)VT_MALLOC((long)pw * (long)ph * (long)sizeof(vt_u32));
    if (!out)
        goto done;
    for (y = 0; y < (long)ph; y++) {
        const vt_u8 *row = raw + y * stride + 1;
        for (x = 0; x < (long)pw; x++) {
            int r, g, b, a = 255, v, mx = (1 << (depth < 16 ? depth : 8)) - 1;
            if (ctype == 3) {
                v = png_sample(row, x, depth);
                if (v < nplte) {
                    r = plte[3 * v];
                    g = plte[3 * v + 1];
                    b = plte[3 * v + 2];
                } else
                    r = g = b = 0;
                if (v < ntrns)
                    a = trns[v];
            } else if (ctype == 0 || ctype == 4) {
                int raw16 = png_sample(row, x * chan, depth);
                v = depth == 16 ? raw16 >> 8 : raw16 * 255 / mx;
                r = g = b = v;
                if (ctype == 4)
                    a = png_sample(row, x * 2 + 1, depth) >> (depth == 16 ? 8 : 0);
                else if (ntrns >= 2 && raw16 == (trns[0] << 8 | trns[1]))
                    a = 0;
            } else {
                int rr = png_sample(row, x * chan, depth), gg = png_sample(row, x * chan + 1, depth),
                    bb = png_sample(row, x * chan + 2, depth), sh = depth == 16 ? 8 : 0;
                r = rr >> sh;
                g = gg >> sh;
                b = bb >> sh;
                if (ctype == 6)
                    a = png_sample(row, x * 4 + 3, depth) >> sh;
                else if (ntrns >= 6 && rr == (trns[0] << 8 | trns[1]) &&
                         gg == (trns[2] << 8 | trns[3]) && bb == (trns[4] << 8 | trns[5]))
                    a = 0;
            }
            out[y * (long)pw + x] = (vt_u32)a << 24 | (vt_u32)r << 16 | (vt_u32)g << 8 | (vt_u32)b;
        }
    }
    *argb = out;
    *w = (int)pw;
    *h = (int)ph;
    out = 0;
    ok = 1;
done:
    if (out)
        VT_FREE(out);
    if (raw)
        VT_FREE(raw);
    VT_FREE(z);
    return ok;
}
