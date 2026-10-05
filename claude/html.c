/* html -- see html.h. */
#include <string.h>
#include "html.h"
#include "util.h"

typedef struct hs {
    jw *o;
    long start, max;
    int pre;                    /* inside <pre> */
    int nl;                     /* newlines wanted before the next text */
    int space;                  /* a space wanted before the next text */
    int bol;                    /* at the start of a line */
    int cell;                   /* table cells in this row so far */
    int ol[8], depth;           /* list nesting: 0 bullets, else the next number */
    char href[400];             /* the open link's target, "" none */
    int in_a;
    int title;                  /* inside <title> */
} hs;

static void raw(hs *h, const char *s, long n)
{
    jw_raw(h->o, s, n);
    if (n)
        h->bol = s[n - 1] == '\n';
}

/* what text needs before it: the pending newlines, or a space */
static void lead(hs *h)
{
    if (h->nl && h->o->n > h->start) {
        /* nl 1: a new line, 2: a blank line; the newlines already there count */
        int have = 0;
        while (have < 2 && h->o->n - have > h->start && h->o->p[h->o->n - 1 - have] == '\n')
            have++;
        while (have++ < h->nl)
            raw(h, "\n", 1);
    } else if (h->space && !h->bol && h->o->n > h->start)
        raw(h, " ", 1);
    h->nl = 0;
    h->space = 0;
}

static void text(hs *h, const char *s, long n)
{
    if (!n)
        return;
    lead(h);
    raw(h, s, n);
}

static void block(hs *h, int k)
{
    if (h->nl < k)
        h->nl = k;
}

static void put_cp(hs *h, unsigned long cp)
{
    char u[4];
    int l;
    if (cp < 0x80) {
        u[0] = (char)cp;
        l = 1;
    } else if (cp < 0x800) {
        u[0] = (char)(0xc0 | (cp >> 6));
        u[1] = (char)(0x80 | (cp & 0x3f));
        l = 2;
    } else if (cp < 0x10000) {
        u[0] = (char)(0xe0 | (cp >> 12));
        u[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
        u[2] = (char)(0x80 | (cp & 0x3f));
        l = 3;
    } else {
        u[0] = (char)(0xf0 | (cp >> 18));
        u[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
        u[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
        u[3] = (char)(0x80 | (cp & 0x3f));
        l = 4;
    }
    text(h, u, l);
}

static const struct {
    const char *name;
    unsigned long cp;
} ents[] = {
    { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' }, { "apos", '\'' }, { "nbsp", ' ' },
    { "mdash", 0x2014 }, { "ndash", 0x2013 }, { "hellip", 0x2026 }, { "copy", 0xa9 }, { "reg", 0xae },
    { "trade", 0x2122 }, { "laquo", 0xab }, { "raquo", 0xbb }, { "lsquo", 0x2018 }, { "rsquo", 0x2019 },
    { "ldquo", 0x201c }, { "rdquo", 0x201d }, { "bull", 0x2022 }, { "middot", 0xb7 }, { "deg", 0xb0 },
    { "auml", 0xe4 }, { "ouml", 0xf6 }, { "uuml", 0xfc }, { "Auml", 0xc4 }, { "Ouml", 0xd6 },
    { "Uuml", 0xdc }, { "szlig", 0xdf }, { "eacute", 0xe9 }, { "egrave", 0xe8 }, { "aacute", 0xe1 },
    { "times", 0xd7 }, { "euro", 0x20ac }, { "pound", 0xa3 }, { "sect", 0xa7 }, { 0, 0 }
};

/* an entity at s (after '&'): its length up to and with ';', 0 none */
static long entity(hs *h, const char *s, long n)
{
    long k = 0;
    unsigned long cp = 0;
    int i;
    while (k < n && k < 12 && s[k] != ';' && s[k] != '&' && s[k] != ' ' && s[k] != '<')
        k++;
    if (k >= n || s[k] != ';' || !k)
        return 0;
    if (s[0] == '#') {
        long i2 = 1;
        int hex = k > 1 && (s[1] == 'x' || s[1] == 'X');
        if (hex)
            i2 = 2;
        for (; i2 < k; i2++) {
            int c = (unsigned char)s[i2], v;
            if (c >= '0' && c <= '9')
                v = c - '0';
            else if (hex && c >= 'a' && c <= 'f')
                v = c - 'a' + 10;
            else if (hex && c >= 'A' && c <= 'F')
                v = c - 'A' + 10;
            else
                return 0;
            cp = cp * (hex ? 16 : 10) + (unsigned long)v;
            if (cp > 0x10ffff)
                return 0;
        }
        if (!cp)
            return 0;
        if (cp == 0xa0) {
            h->space = 1;           /* a no-break space is white space here */
            return k + 1;
        }
        put_cp(h, cp);
        return k + 1;
    }
    for (i = 0; ents[i].name; i++)
        if ((long)strlen(ents[i].name) == k && !memcmp(ents[i].name, s, (size_t)k)) {
            if (!strcmp(ents[i].name, "nbsp"))
                h->space = 1;
            else
                put_cp(h, ents[i].cp);
            return k + 1;
        }
    return 0;
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static int name_is(const char *nm, const char *z)
{
    return !strcmp(nm, z);
}

/* an attribute's value from a tag's text, decoded only for &amp; */
static void attr(const char *t, long n, const char *name, char *out, long cap)
{
    long i, nl = (long)strlen(name), o = 0;
    out[0] = 0;
    for (i = 0; i + nl < n; i++) {
        long k;
        char q;
        if (i && t[i - 1] != ' ' && t[i - 1] != '\t' && t[i - 1] != '\n' && t[i - 1] != '\r')
            continue;
        for (k = 0; k < nl && lower((unsigned char)t[i + k]) == name[k]; k++)
            ;
        if (k < nl)
            continue;
        k = i + nl;
        while (k < n && t[k] == ' ')
            k++;
        if (k >= n || t[k] != '=')
            continue;
        k++;
        while (k < n && t[k] == ' ')
            k++;
        q = k < n && (t[k] == '"' || t[k] == '\'') ? t[k++] : 0;
        while (k < n && (q ? t[k] != q : (t[k] != ' ' && t[k] != '>')) && o < cap - 1) {
            if (t[k] == '&' && k + 4 < n && !memcmp(t + k, "&amp;", 5)) {
                out[o++] = '&';
                k += 5;
            } else
                out[o++] = t[k++];
        }
        out[o] = 0;
        return;
    }
}

static const char *const skipped[] = { "script", "style", "noscript", "svg", "template", "iframe", "select", 0 };

void html_to_md(const char *s, long n, jw *out, long max)
{
    hs h;
    long i = 0;
    memset(&h, 0, sizeof(h));
    h.o = out;
    h.start = out->n;
    h.max = max;
    h.bol = 1;
    while (i < n && (!max || out->n - h.start < max)) {
        char c = s[i];
        if (c == '<') {
            char nm[16];
            long j = i + 1, k = 0, te;
            int close = 0, q = 0;
            if (i + 3 < n && !memcmp(s + i, "<!--", 4)) {
                const char *e = 0;
                long m;
                for (m = i + 4; m + 2 < n; m++)
                    if (s[m] == '-' && s[m + 1] == '-' && s[m + 2] == '>') {
                        e = s + m;
                        break;
                    }
                i = e ? (long)(e - s) + 3 : n;
                continue;
            }
            if (j < n && s[j] == '/') {
                close = 1;
                j++;
            }
            while (j < n && k < (long)sizeof(nm) - 1 &&
                   ((s[j] >= 'a' && s[j] <= 'z') || (s[j] >= 'A' && s[j] <= 'Z') || (k && s[j] >= '0' && s[j] <= '9')))
                nm[k++] = (char)lower((unsigned char)s[j++]);
            nm[k] = 0;
            if (!k && !(j < n && (s[j] == '!' || s[j] == '?'))) {
                text(&h, "<", 1);   /* a lone '<' in the text */
                i++;
                continue;
            }
            /* the tag's end, quotes respected */
            for (te = j; te < n; te++) {
                if (q) {
                    if (s[te] == q)
                        q = 0;
                } else if (s[te] == '"' || s[te] == '\'')
                    q = s[te];
                else if (s[te] == '>')
                    break;
            }
            if (!close) {
                int sk;
                for (sk = 0; skipped[sk]; sk++)
                    if (name_is(nm, skipped[sk]))
                        break;
                if (skipped[sk]) {
                    /* everything up to its closing tag */
                    long m, l = (long)strlen(nm);
                    for (m = te; m + l + 2 < n; m++)
                        if (s[m] == '<' && s[m + 1] == '/') {
                            long z;
                            for (z = 0; z < l && lower((unsigned char)s[m + 2 + z]) == nm[z]; z++)
                                ;
                            if (z == l)
                                break;
                        }
                    while (m < n && s[m] != '>')
                        m++;
                    i = m + 1;
                    continue;
                }
            }
            if (nm[0] == 'h' && nm[1] >= '1' && nm[1] <= '6' && !nm[2]) {
                block(&h, 2);
                if (!close) {
                    int lv = nm[1] - '0';
                    lead(&h);
                    while (lv--)
                        raw(&h, "#", 1);
                    raw(&h, " ", 1);
                }
            } else if (name_is(nm, "title")) {
                h.title = !close;
                block(&h, 2);
                if (!close) {
                    lead(&h);
                    raw(&h, "# ", 2);
                }
            } else if (name_is(nm, "p") || name_is(nm, "blockquote") || name_is(nm, "table") ||
                       name_is(nm, "ul") || name_is(nm, "ol") || name_is(nm, "dl")) {
                block(&h, 2);
                if ((name_is(nm, "ul") || name_is(nm, "ol"))) {
                    if (!close && h.depth < 8)
                        h.ol[h.depth++] = name_is(nm, "ol") ? 1 : 0;
                    else if (close && h.depth)
                        h.depth--;
                }
            } else if (name_is(nm, "div") || name_is(nm, "section") || name_is(nm, "article") ||
                       name_is(nm, "header") || name_is(nm, "footer") || name_is(nm, "main") ||
                       name_is(nm, "nav") || name_is(nm, "aside") || name_is(nm, "form") ||
                       name_is(nm, "dt") || name_is(nm, "dd") || name_is(nm, "figure") || name_is(nm, "center")) {
                block(&h, 1);
            } else if (name_is(nm, "br")) {
                lead(&h);
                raw(&h, "\n", 1);
            } else if (name_is(nm, "hr")) {
                block(&h, 2);
                lead(&h);
                raw(&h, "---", 3);
                block(&h, 2);
            } else if (name_is(nm, "li")) {
                block(&h, 1);
                if (!close) {
                    int d;
                    lead(&h);
                    for (d = 1; d < h.depth; d++)
                        raw(&h, "  ", 2);
                    if (h.depth && h.ol[h.depth - 1]) {
                        char num[16];
                        cl_ltoa(h.ol[h.depth - 1]++, num);
                        raw(&h, num, (long)strlen(num));
                        raw(&h, ". ", 2);
                    } else
                        raw(&h, "- ", 2);
                }
            } else if (name_is(nm, "tr")) {
                block(&h, 1);
                h.cell = 0;
            } else if (name_is(nm, "td") || name_is(nm, "th")) {
                if (!close && h.cell++) {
                    lead(&h);
                    raw(&h, " | ", 3);
                }
            } else if (name_is(nm, "pre")) {
                if (!close) {
                    block(&h, 2);
                    lead(&h);
                    raw(&h, "```\n", 4);
                    h.pre = 1;
                } else if (h.pre) {
                    if (!h.bol)
                        raw(&h, "\n", 1);
                    raw(&h, "```", 3);
                    h.pre = 0;
                    block(&h, 2);
                }
            } else if (name_is(nm, "code") && !h.pre) {
                text(&h, "`", 1);
            } else if (name_is(nm, "strong") || name_is(nm, "b")) {
                text(&h, "**", 2);
            } else if (name_is(nm, "em") || name_is(nm, "i")) {
                text(&h, "*", 1);
            } else if (name_is(nm, "a")) {
                if (!close) {
                    attr(s + j, te - j, "href", h.href, sizeof(h.href));
                    if (h.href[0] && h.href[0] != '#' && strncmp(h.href, "javascript:", 11)) {
                        text(&h, "[", 1);
                        h.in_a = 1;
                    } else
                        h.in_a = 0;
                } else if (h.in_a) {
                    text(&h, "](", 2);
                    raw(&h, h.href, (long)strlen(h.href));
                    raw(&h, ")", 1);
                    h.in_a = 0;
                }
            } else if (name_is(nm, "img") && !close) {
                char alt[200];
                attr(s + j, te - j, "alt", alt, sizeof(alt));
                if (alt[0]) {
                    h.space = 1;
                    text(&h, alt, (long)strlen(alt));
                    h.space = 1;
                }
            }
            i = te + 1;
            continue;
        }
        if (c == '&') {
            long l = entity(&h, s + i + 1, n - i - 1);
            if (l) {
                i += l + 1;
                continue;
            }
        }
        if (!h.pre && (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f')) {
            h.space = 1;
            i++;
            continue;
        }
        {
            /* a run of plain text */
            long e = i;
            while (e < n && s[e] != '<' && s[e] != '&' &&
                   (h.pre || (s[e] != ' ' && s[e] != '\t' && s[e] != '\n' && s[e] != '\r' && s[e] != '\f')))
                e++;
            if (e == i)
                e++;                /* a '&' that starts no entity is text */
            if (h.pre) {
                lead(&h);
                raw(&h, s + i, e - i);
            } else
                text(&h, s + i, e - i);
            i = e;
        }
    }
    if (max && out->n - h.start > max)
        out->n = h.start + max;
    if (out->p)
        out->p[out->n] = 0;
}
