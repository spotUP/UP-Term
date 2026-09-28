#include "glyphmap.h"

#include "glyph_tables.inc"

/* Latin-1 stand-ins for what Unix text commonly carries outside Latin-1:
 * typographic punctuation, arrows, and the DEC graphics symbols that have
 * no line/block form. Sorted by code point for the binary search. */
static const struct { vt_u16 cp; vt_u8 ch; } approx[] = {
    { 0x0152, 'O' }, { 0x0153, 'o' }, { 0x0160, 'S' }, { 0x0161, 's' }, { 0x0178, 'Y' },
    { 0x017D, 'Z' }, { 0x017E, 'z' }, { 0x0192, 'f' }, { 0x02C6, '^' }, { 0x02DC, '~' },
    { 0x03C0, 'p' }, { 0x2010, '-' }, { 0x2011, '-' }, { 0x2012, '-' }, { 0x2013, '-' },
    { 0x2014, '-' }, { 0x2015, '-' }, { 0x2018, '\'' }, { 0x2019, '\'' }, { 0x201A, ',' },
    { 0x201C, '"' }, { 0x201D, '"' }, { 0x201E, '"' }, { 0x2020, '+' }, { 0x2022, 0xB7 },
    { 0x2026, '.' }, { 0x2030, '%' }, { 0x2039, '<' }, { 0x203A, '>' }, { 0x20AC, 'E' },
    { 0x2122, 'T' }, { 0x2190, '<' }, { 0x2191, '^' }, { 0x2192, '>' }, { 0x2193, 'v' },
    { 0x2212, '-' }, { 0x2219, 0xB7 }, { 0x221A, 'v' }, { 0x221E, '8' }, { 0x2248, '~' },
    { 0x2260, '#' }, { 0x2261, '=' }, { 0x2264, '<' }, { 0x2265, '>' }, { 0x2302, '^' },
    { 0x2409, 'T' }, { 0x240A, 'L' }, { 0x240B, 'V' }, { 0x240C, 'F' }, { 0x240D, 'C' },
    { 0x2424, 'N' }, { 0x25B2, '^' }, { 0x25B6, '>' }, { 0x25BA, '>' }, { 0x25BC, 'v' },
    { 0x25C0, '<' }, { 0x25C4, '<' }, { 0x25CB, 'o' }, { 0x25CF, 'o' }, { 0x2713, 'v' },
    { 0x2714, 'v' }, { 0x2717, 'x' }, { 0x2718, 'x' }, { 0xFFFD, '?' }
};

static int approx_find(vt_u32 cp)
{
    int lo = 0, hi = (int)(sizeof(approx) / sizeof(approx[0])) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (approx[mid].cp == cp)
            return approx[mid].ch;
        if (approx[mid].cp < cp)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

static vt_glyph mk(int kind, int code)
{
    vt_glyph g;
    g.kind = (vt_u8)kind;
    g.code = (vt_u8)code;
    return g;
}

vt_glyph vt_map_glyph(vt_u32 cp, enum vt_font_enc enc)
{
    int a;
    if (cp < 0x80)
        return mk(VT_GLYPH_FONT, (int)cp);
    if (enc == VT_ENC_CP437) {
        /* The IBM font has the line and block glyphs itself: they join
         * as the art was drawn for. */
        const vt_u16 *t = vt_cp437_table();
        int i;
        for (i = 0; i < 128; i++)
            if (t[i] == cp)
                return mk(VT_GLYPH_FONT, 0x80 + i);
        if (cp == 0x2302)
            return mk(VT_GLYPH_FONT, 0x7F);
    } else if (cp >= 0xA0 && cp <= 0xFF) {
        return mk(VT_GLYPH_FONT, (int)cp);
    }
    if (cp >= 0x2500 && cp < 0x2580) {
        if (cp == 0x2571)
            return mk(VT_GLYPH_DIAGONAL, 1);
        if (cp == 0x2572)
            return mk(VT_GLYPH_DIAGONAL, 2);
        if (cp == 0x2573)
            return mk(VT_GLYPH_DIAGONAL, 3);
        return mk(VT_GLYPH_BOX, box_arms[cp - 0x2500]);
    }
    if (cp >= 0x2580 && cp < 0x25A0)
        return mk(VT_GLYPH_BLOCK, block_shape[cp - 0x2580]);
    /* DEC graphics scan lines 1, 3, 7, 9 (5 is U+2500): thin lines at the
     * top, a quarter, three quarters and the bottom of the cell. */
    if (cp >= 0x23BA && cp <= 0x23BD) {
        static const vt_u8 y8[4] = { 0, 2, 6, 7 };
        return mk(VT_GLYPH_HLINE, y8[cp - 0x23BA]);
    }
    if (cp == 0x25C6 || cp == 0x2666)
        return mk(VT_GLYPH_DIAMOND, 0);
    if (cp == 0x25A0 || cp == 0x25AE)
        return mk(VT_GLYPH_BLOCK, 0x40 | 0x0F);
    a = approx_find(cp);
    if (a >= 0) {
        if (enc == VT_ENC_CP437 && a >= 0x80)
            return vt_map_glyph((vt_u32)a, enc); /* e.g. the middle dot */
        return mk(VT_GLYPH_FONT, a);
    }
    return mk(VT_GLYPH_FONT, '?');
}
