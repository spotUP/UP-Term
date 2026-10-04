#include "glyphmap.h"

#include "glyph_tables.inc"

/* Latin-1 stand-ins for what Unix text commonly carries outside Latin-1:
 * typographic punctuation, arrows, and the DEC graphics symbols that have
 * no line/block form; and the symbols Claude Code draws its screen with
 * (ledger A1.3, tests/streams/claude-session.80x24.bin): the spinner's
 * stars, its prompt and bullet, the mode marker, the warning sign.
 * Sorted by code point for the binary search. */
static const struct { vt_u16 cp; vt_u8 ch; } approx[] = {
    { 0x0152, 'O' }, { 0x0153, 'o' }, { 0x0160, 'S' }, { 0x0161, 's' }, { 0x0178, 'Y' },
    { 0x017D, 'Z' }, { 0x017E, 'z' }, { 0x0192, 'f' }, { 0x02C6, '^' }, { 0x02DC, '~' },
    { 0x03C0, 'p' }, { 0x2010, '-' }, { 0x2011, '-' }, { 0x2012, '-' }, { 0x2013, '-' },
    { 0x2014, '-' }, { 0x2015, '-' }, { 0x2018, '\'' }, { 0x2019, '\'' }, { 0x201A, ',' },
    { 0x201C, '"' }, { 0x201D, '"' }, { 0x201E, '"' }, { 0x2020, '+' }, { 0x2022, 0xB7 },
    { 0x2026, '.' }, { 0x2030, '%' }, { 0x2039, '<' }, { 0x203A, '>' }, { 0x203B, '*' },
    { 0x20AC, 'E' }, { 0x2122, 'T' }, { 0x2190, '<' }, { 0x2191, '^' }, { 0x2192, '>' },
    { 0x2193, 'v' }, { 0x21B5, '<' }, { 0x2212, '-' }, { 0x2219, 0xB7 }, { 0x221A, 'v' },
    { 0x221E, '8' }, { 0x2248, '~' }, { 0x2260, '#' }, { 0x2261, '=' }, { 0x2264, '<' },
    { 0x2265, '>' }, { 0x2302, '^' }, { 0x23F5, '>' }, { 0x23FA, 'o' }, { 0x2409, 'T' },
    { 0x240A, 'L' }, { 0x240B, 'V' }, { 0x240C, 'F' }, { 0x240D, 'C' }, { 0x2424, 'N' },
    { 0x25B2, '^' }, { 0x25B6, '>' }, { 0x25B8, '>' }, { 0x25BA, '>' }, { 0x25BC, 'v' },
    { 0x25C0, '<' }, { 0x25C4, '<' }, { 0x25CB, 'o' }, { 0x25CF, 'o' }, { 0x25D0, 'o' },
    { 0x25D1, 'o' }, { 0x25D2, 'o' }, { 0x25D3, 'o' }, { 0x26A0, '!' }, { 0x2713, 'v' },
    { 0x2714, 'v' }, { 0x2715, 'x' }, { 0x2716, 'x' }, { 0x2717, 'x' }, { 0x2718, 'x' },
    { 0x2722, '*' }, { 0x2726, '*' }, { 0x2733, '*' }, { 0x2736, '*' }, { 0x273B, '*' },
    { 0x273D, '*' }, { 0x276F, '>' }, { 0xFFFD, '?' }
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

/* *native: 1 when the glyph is cp itself (the font's own, or drawn), 0 for
 * a stand-in or the replacement */
static vt_glyph map(vt_u32 cp, enum vt_font_enc enc, int *native)
{
    int a;
    *native = 1;
    if (cp < 0x80)
        return mk(VT_GLYPH_FONT, (int)cp);
    if (enc == VT_ENC_CP437) {
        /* The IBM font has the line and block glyphs itself: they join
         * as the art was drawn for. */
        int lo = 0, hi = 127;
        while (lo <= hi) { /* 7 steps: BBS art is mostly these characters */
            int mid = (lo + hi) >> 1;
            if (cp437_rev_cp[mid] == cp)
                return mk(VT_GLYPH_FONT, cp437_rev_byte[mid]);
            if (cp437_rev_cp[mid] < cp)
                lo = mid + 1;
            else
                hi = mid - 1;
        }
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
    if (cp == 0x23BF) { /* Claude Code's tool-result hook: drawn as the light corner */
        *native = 0;
        return mk(VT_GLYPH_BOX, box_arms[0x2514 - 0x2500]);
    }
    if (cp >= 0x23BA && cp <= 0x23BD) {
        static const vt_u8 y8[4] = { 0, 2, 6, 7 };
        return mk(VT_GLYPH_HLINE, y8[cp - 0x23BA]);
    }
    if (cp == 0x25C6 || cp == 0x2666)
        return mk(VT_GLYPH_DIAMOND, 0);
    if (cp == 0x25A0 || cp == 0x25AE)
        return mk(VT_GLYPH_BLOCK, 0x40 | 0x0F);
    *native = 0;
    if (cp > 0xFFFF)
        return mk(VT_GLYPH_MISSING, 1); /* an emoji, an icon: no bitmap font has it */
    a = approx_find(cp);
    if (a >= 0) {
        if (enc == VT_ENC_CP437 && a >= 0x80) {
            int n;
            return map((vt_u32)a, enc, &n); /* e.g. the middle dot */
        }
        return mk(VT_GLYPH_FONT, a);
    }
    return mk(VT_GLYPH_FONT, '?');
}

vt_glyph vt_map_glyph(vt_u32 cp, enum vt_font_enc enc)
{
    int native;
    return map(cp, enc, &native);
}

/* Shown as nothing: variation selectors, joiners, the other format
 * characters that only steer how their neighbours look. */
static int invisible(vt_u32 c)
{
    return (c >= 0x200B && c <= 0x200F) || (c >= 0x2060 && c <= 0x206F) ||
           (c >= 0xFE00 && c <= 0xFE0F) || (c >= 0x180B && c <= 0x180F) || c == 0x034F ||
           (c >= 0xE0000UL && c <= 0xE0FFFUL);
}

static int compose_pair(vt_u32 base, vt_u32 mark)
{
    vt_u32 key;
    int lo = 0, hi = COMPOSE_N - 1;
    if (base > 0xFFFF || mark > 0xFFFF)
        return -1;
    key = (base << 16) | mark;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        if (compose_key[mid] == key)
            return compose_to[mid];
        if (compose_key[mid] < key)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

int vt_compose_cell(vt_u32 *cp, int n)
{
    int i, k = 1, c;
    for (i = 1; i < n; i++) {
        if (invisible(cp[i]))
            continue;
        if (k == 1 && (c = compose_pair(cp[0], cp[i])) >= 0)
            cp[0] = (vt_u32)c; /* a mark after one that did not compose stays a mark */
        else
            cp[k++] = cp[i];
    }
    return k;
}

int vt_glyph_native(vt_u32 cp, enum vt_font_enc enc)
{
    int native;
    if (cp < 0x80)
        return 1;
    map(cp, enc, &native);
    return native;
}

int vt_latin1_text(const char *utf8, char *out, int max)
{
    const unsigned char *s = (const unsigned char *)utf8;
    int n = 0;
    if (max <= 0)
        return 0;
    while (*s && n < max - 1) {
        vt_u32 cp;
        int more, ok = 1;
        if (*s < 0x80) {
            out[n++] = (char)*s++;
            continue;
        }
        if ((*s & 0xE0) == 0xC0) {
            cp = *s & 0x1F;
            more = 1;
        } else if ((*s & 0xF0) == 0xE0) {
            cp = *s & 0x0F;
            more = 2;
        } else if ((*s & 0xF8) == 0xF0) {
            cp = *s & 0x07;
            more = 3;
        } else {
            cp = 0xFFFD; /* a stray continuation or invalid lead byte */
            more = 0;
        }
        s++;
        while (more-- > 0) {
            if ((*s & 0xC0) != 0x80) {
                ok = 0; /* cut short: the next byte starts a character of its own */
                break;
            }
            cp = (cp << 6) | (*s++ & 0x3F);
        }
        if (!ok)
            cp = 0xFFFD;
        {
            vt_glyph g = vt_map_glyph(cp, VT_ENC_LATIN1);
            out[n++] = (char)(g.kind == VT_GLYPH_FONT ? g.code : '?');
        }
    }
    out[n] = 0;
    return n;
}
