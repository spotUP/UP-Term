/* clipfmt -- see clipfmt.h. Portable C89, no OS calls. */
#include "clipfmt.h"

unsigned long cf_next(const char *s, long n, long *i)
{
    const unsigned char *u = (const unsigned char *)s;
    unsigned long c = u[*i];
    int need, k;
    if (c < 0x80) {
        (*i)++;
        return c;
    }
    if (c >= 0xC2 && c <= 0xDF)
        need = 1;
    else if (c >= 0xE0 && c <= 0xEF)
        need = 2;
    else if (c >= 0xF0 && c <= 0xF4)
        need = 3;
    else
        need = 0;
    if (need && *i + need < n) {
        unsigned long v = c & (0x3F >> need);
        for (k = 1; k <= need; k++) {
            if ((u[*i + k] & 0xC0) != 0x80)
                break;
            v = (v << 6) | (u[*i + k] & 0x3F);
        }
        if (k > need && !(need == 2 && v < 0x800) && !(need == 3 && (v < 0x10000 || v > 0x10FFFF)) &&
            !(v >= 0xD800 && v <= 0xDFFF)) {
            *i += need + 1;
            return v;
        }
    }
    (*i)++;
    return c; /* not UTF-8: the Latin-1 byte */
}

long cf_to_latin1(const char *in, long n, char *out)
{
    long i = 0, k = 0;
    while (i < n) {
        unsigned long c = cf_next(in, n, &i);
        out[k++] = (char)(c < 0x100 ? c : '?');
    }
    return k;
}

long cf_from_latin1(const char *in, long n, char *out)
{
    long i, k = 0;
    for (i = 0; i < n; i++) {
        unsigned char b = (unsigned char)in[i];
        if (b < 0x80) {
            out[k++] = (char)b;
        } else {
            out[k++] = (char)(0xC0 | (b >> 6));
            out[k++] = (char)(0x80 | (b & 0x3F));
        }
    }
    return k;
}

int cf_paste_keeps(unsigned long cp)
{
    if (cp == '\t' || cp == '\n' || cp == '\r')
        return 1;
    return !(cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F));
}

static void put32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static unsigned long get32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

void cf_ftxt_head(unsigned char *head, long chrs, long utf8)
{
    unsigned long form = 4 + 8 + (unsigned long)chrs + (chrs & 1) + 8 + (unsigned long)utf8 + (utf8 & 1);
    head[0] = 'F'; head[1] = 'O'; head[2] = 'R'; head[3] = 'M';
    put32(head + 4, form);
    head[8] = 'F'; head[9] = 'T'; head[10] = 'X'; head[11] = 'T';
    head[12] = 'C'; head[13] = 'H'; head[14] = 'R'; head[15] = 'S';
    put32(head + 16, (unsigned long)chrs);
}

int cf_ftxt_mid(unsigned char *mid, long chrs, long utf8)
{
    int k = 0;
    if (chrs & 1)
        mid[k++] = 0;
    mid[k++] = 'U'; mid[k++] = 'T'; mid[k++] = 'F'; mid[k++] = '8';
    put32(mid + k, (unsigned long)utf8);
    return k + 4;
}

/* n bytes read and dropped; 0 when the clip ended first */
static int skip(cf_read_fn rd, void *u, unsigned long n)
{
    char sink[64];
    while (n) {
        long k = (long)(n < sizeof(sink) ? n : sizeof(sink));
        if (rd(u, sink, k) != k)
            return 0;
        n -= (unsigned long)k;
    }
    return 1;
}

char *cf_read_ftxt(cf_read_fn rd, void *u, void *(*alloc)(unsigned long), void (*release)(void *),
                   long *len)
{
    unsigned char h[12];
    unsigned long left;
    char *text = 0;
    *len = 0;
    if (rd(u, h, 12) != 12 || get32(h) != 0x464F524DUL /* FORM */ || get32(h + 8) != 0x46545854UL /* FTXT */)
        return 0;
    left = get32(h + 4) - 4;
    while (left >= 8) {
        unsigned long id, size, pad;
        int utf8, chrs;
        char *t, *raw;
        if (rd(u, h, 8) != 8)
            break;
        left -= 8;
        id = get32(h);
        size = get32(h + 4);
        pad = size & 1;
        if (size + pad > left) {
            size = left; /* a FORM that claims less than its chunk: as far as it goes */
            pad = 0;
        }
        utf8 = id == 0x55544638UL;         /* UTF8 */
        chrs = id == 0x43485253UL && !text; /* the first CHRS */
        if (!utf8 && !chrs) {
            if (!skip(rd, u, size + pad))
                break;
            left -= size + pad;
            continue;
        }
        /* CHRS is Latin-1, up to twice its size as UTF-8: it lands in the
         * upper half and is widened to the front (a byte written never
         * passes one not yet read) */
        t = (char *)alloc((chrs ? 2 * size : size) + 1);
        if (!t)
            break;
        raw = chrs ? t + size : t;
        if ((unsigned long)rd(u, raw, (long)size) != size) {
            release(t);
            break;
        }
        if (text)
            release(text);
        text = t;
        *len = chrs ? cf_from_latin1(raw, (long)size, t) : (long)size;
        t[*len] = 0;
        if (utf8)
            break; /* the text: the rest of the FORM is not needed */
        left -= size;
        if (pad) {
            if (!skip(rd, u, pad))
                break;
            left -= pad;
        }
    }
    return text;
}
