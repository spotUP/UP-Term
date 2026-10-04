/* vw_text -- the writer and text measurement (vw_text.h). */
#include <string.h>
#include "../engine/vtengine.h"
#include "../engine/vtwidth.h"
#include "vw_text.h"

void vo_init(vw_out *o, vw_sink sink, void *u, int cs, int depth, const hl_theme *theme)
{
    o->sink = sink;
    o->u = u;
    o->cs = cs;
    o->depth = depth;
    o->theme = theme;
    o->cur.fg = o->cur.bg = HL_DEFAULT;
    o->cur.attr = 0;
    o->styled = 0;
    o->osc8 = 0;
    o->link[0] = 0;
    for (o->n = 0; o->n < HL_NCLASS; o->n++)
        o->sgrlen[o->n] = -1;
    o->n = 0;
}

void vo_flush(vw_out *o)
{
    if (o->n)
        o->sink(o->u, o->buf, o->n);
    o->n = 0;
}

void vo_raw(vw_out *o, const char *s, long n)
{
    if (n <= (long)sizeof(o->buf) - o->n) {
        /* a byte loop: an SGR or a gutter is a few bytes, and vbcc's
         * memcpy is a call that moves bytes one by one anyway */
        char *d = o->buf + o->n;
        o->n += (int)n;
        while (n-- > 0)
            *d++ = *s++;
        if (o->n == (int)sizeof(o->buf))
            vo_flush(o);
        return;
    }
    while (n > 0) {
        long k = (long)sizeof(o->buf) - o->n;
        if (k > n)
            k = n;
        memcpy(o->buf + o->n, s, k);
        o->n += (int)k;
        s += k;
        n -= k;
        if (o->n == (int)sizeof(o->buf))
            vo_flush(o);
    }
}

void vo_rawz(vw_out *o, const char *s)
{
    vo_raw(o, s, (long)strlen(s));
}

static void putb(vw_out *o, int c)
{
    if (o->n == (int)sizeof(o->buf))
        vo_flush(o);
    o->buf[o->n++] = (char)c;
}

void vo_spaces(vw_out *o, int n)
{
    while (n-- > 0)
        putb(o, ' ');
}

int vw_char(const char *s, long n, unsigned long *cp)
{
    const unsigned char *u = (const unsigned char *)s;
    unsigned long c = u[0];
    int len, i;
    if (c < 0x80) {
        *cp = c;
        return 1;
    }
    if (c >= 0xC2 && c <= 0xDF) {
        len = 2;
        c &= 0x1F;
    } else if (c >= 0xE0 && c <= 0xEF) {
        len = 3;
        c &= 0x0F;
    } else if (c >= 0xF0 && c <= 0xF4) {
        len = 4;
        c &= 0x07;
    } else {
        *cp = u[0];
        return 1;
    }
    if (n < len) {
        *cp = u[0];
        return 1;
    }
    for (i = 1; i < len; i++) {
        if ((u[i] & 0xC0) != 0x80) {
            *cp = u[0];
            return 1;
        }
        c = (c << 6) | (u[i] & 0x3F);
    }
    if ((len == 3 && (c < 0x800 || (c >= 0xD800 && c <= 0xDFFF))) ||
        (len == 4 && (c < 0x10000 || c > 0x10FFFF))) {
        *cp = u[0];
        return 1;
    }
    *cp = c;
    return len;
}

int vw_put_utf8(char *out, unsigned long cp)
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

int vw_cp_width(unsigned long cp)
{
    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0))
        return 0;
    return vt_char_width((vt_u32)cp);
}

int vw_width(const char *s, long n)
{
    int w = 0;
    while (n > 0) {
        unsigned long cp;
        int k;
        if ((unsigned char)*s < 0x80) {
            w += (unsigned char)*s >= 0x20 && *s != 0x7F;
            s++;
            n--;
            continue;
        }
        k = vw_char(s, n, &cp);
        w += vw_cp_width(cp);
        s += k;
        n -= k;
    }
    return w;
}

long vw_fit(const char *s, long n, int cols)
{
    long i = 0;
    int w = 0;
    while (i < n) {
        unsigned long cp;
        int k = vw_char(s + i, n - i, &cp);
        int cw = vw_cp_width(cp);
        if (w + cw > cols)
            break;
        w += cw;
        i += k;
    }
    return i;
}

void vo_text(vw_out *o, const char *s, long n)
{
    long i = 0, run = 0;
    /* ASCII straight into the buffer: one pass, no call per token */
    if (n <= (long)sizeof(o->buf) - o->n) {
        char *d = o->buf + o->n;
        while (i < n && !((unsigned char)s[i] & 0x80))
            *d++ = s[i++];
        o->n += (int)i;
        if (i == n)
            return;
        run = i;
    }
    while (i < n) {
        unsigned long cp;
        int k;
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) {
            i++;
            continue;
        }
        k = vw_char(s + i, n - i, &cp);
        if (o->cs == VW_UTF8) {
            if (k > 1) {
                i += k;
                continue;
            }
            /* a Latin-1 byte: its UTF-8 */
            vo_raw(o, s + run, i - run);
            {
                char u[4];
                vo_raw(o, u, vw_put_utf8(u, cp));
            }
        } else {
            if (k == 1) {
                i++;
                continue;
            }
            vo_raw(o, s + run, i - run);
            putb(o, cp < 0x100 ? (int)cp : '?');
        }
        i += k;
        run = i;
    }
    vo_raw(o, s + run, n - run);
}

void vo_textz(vw_out *o, const char *s)
{
    vo_text(o, s, (long)strlen(s));
}

static int same(const hl_sty *a, const hl_sty *b)
{
    return a->fg == b->fg && a->bg == b->bg && a->attr == b->attr;
}

void vo_style(vw_out *o, const hl_sty *s)
{
    char sgr[64];
    if (o->depth <= 0 || same(&o->cur, s))
        return;
    o->cur = *s;
    o->styled = s->fg != HL_DEFAULT || s->bg != HL_DEFAULT || s->attr;
    vo_raw(o, sgr, hl_sgr(sgr, s, o->depth));
}

/* a class's style; its SGR is made once (hl sends one per token) */
void vo_class(vw_out *o, int cls)
{
    const hl_sty *s = &o->theme->s[cls];
    if (o->depth <= 0 || same(&o->cur, s))
        return;
    if (o->sgrlen[cls] < 0)
        o->sgrlen[cls] = (short)hl_sgr(o->sgr[cls], s, o->depth);
    o->cur = *s;
    o->styled = s->fg != HL_DEFAULT || s->bg != HL_DEFAULT || s->attr;
    vo_raw(o, o->sgr[cls], o->sgrlen[cls]);
}

void vo_reset(vw_out *o)
{
    if (o->depth > 0 && o->styled)
        vo_raw(o, "\033[0m", 4);
    o->cur.fg = o->cur.bg = HL_DEFAULT;
    o->cur.attr = 0;
    o->styled = 0;
}

void vo_link(vw_out *o, const char *url)
{
    if (!o->osc8)
        return;
    if (!url)
        url = "";
    if (!strcmp(o->link, url))
        return;
    if (strlen(url) >= sizeof(o->link))
        url = "";
    vo_raw(o, "\033]8;;", 5);
    vo_rawz(o, url);
    vo_raw(o, "\033\\", 2);
    strcpy(o->link, url);
}
