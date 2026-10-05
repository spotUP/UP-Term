/* json -- see json.h. */
#include <stdlib.h>
#include <string.h>
#include "json.h"
#include "util.h"

static long ws(const char *p, long n, long i)
{
    while (i < n && (p[i] == ' ' || p[i] == '\t' || p[i] == '\n' || p[i] == '\r'))
        i++;
    return i;
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* the length of the string at p[0] == '"', or -1 */
static long str_len(const char *p, long n)
{
    long i = 1;
    while (i < n) {
        unsigned char c = (unsigned char)p[i];
        if (c == '"')
            return i + 1;
        if (c < 0x20)
            return -1;
        if (c == '\\') {
            if (i + 1 >= n)
                return -1;
            c = (unsigned char)p[i + 1];
            if (c == 'u') {
                int k;
                if (i + 5 >= n)
                    return -1;
                for (k = 2; k < 6; k++)
                    if (hexval((unsigned char)p[i + k]) < 0)
                        return -1;
                i += 6;
                continue;
            }
            if (!strchr("\"\\/bfnrt", c) || !c)
                return -1;
            i += 2;
            continue;
        }
        i++;
    }
    return -1;
}

static int digit(int c)
{
    return c >= '0' && c <= '9';
}

static long num_len(const char *p, long n)
{
    long i = 0;
    if (i < n && p[i] == '-')
        i++;
    if (i >= n)
        return -1;
    if (p[i] == '0')
        i++;
    else if (digit((unsigned char)p[i])) {
        while (i < n && digit((unsigned char)p[i]))
            i++;
    } else
        return -1;
    if (i < n && p[i] == '.') {
        i++;
        if (i >= n || !digit((unsigned char)p[i]))
            return -1;
        while (i < n && digit((unsigned char)p[i]))
            i++;
    }
    if (i < n && (p[i] == 'e' || p[i] == 'E')) {
        i++;
        if (i < n && (p[i] == '+' || p[i] == '-'))
            i++;
        if (i >= n || !digit((unsigned char)p[i]))
            return -1;
        while (i < n && digit((unsigned char)p[i]))
            i++;
    }
    return i;
}

static long lit_len(const char *p, long n, const char *w)
{
    long l = (long)strlen(w);
    return n >= l && !memcmp(p, w, (size_t)l) ? l : -1;
}

long json_value(const char *p, long n)
{
    char st[JSON_DEPTH];
    int d = 0;
    long i = 0, l;
    for (;;) {
        /* a value at i */
        i = ws(p, n, i);
        if (i >= n)
            return -1;
        switch (p[i]) {
        case '{':
        case '[':
            if (d >= JSON_DEPTH)
                return -1;
            st[d++] = p[i++];
            i = ws(p, n, i);
            if (i >= n)
                return -1;
            if (p[i] == (st[d - 1] == '{' ? '}' : ']')) {
                d--;
                i++;
                goto after;
            }
            if (st[d - 1] == '{')
                goto key;
            continue;
        case '"':
            l = str_len(p + i, n - i);
            break;
        case 't':
            l = lit_len(p + i, n - i, "true");
            break;
        case 'f':
            l = lit_len(p + i, n - i, "false");
            break;
        case 'n':
            l = lit_len(p + i, n - i, "null");
            break;
        default:
            l = num_len(p + i, n - i);
            break;
        }
        if (l < 0)
            return -1;
        i += l;
    after:
        if (!d)
            return i;
        i = ws(p, n, i);
        if (i >= n)
            return -1;
        if (p[i] == ',') {
            i++;
            if (st[d - 1] == '{')
                goto key;
            continue;
        }
        if (p[i] == (st[d - 1] == '{' ? '}' : ']')) {
            d--;
            i++;
            goto after;
        }
        return -1;
    key:
        i = ws(p, n, i);
        if (i >= n || p[i] != '"')
            return -1;
        l = str_len(p + i, n - i);
        if (l < 0)
            return -1;
        i = ws(p, n, i + l);
        if (i >= n || p[i] != ':')
            return -1;
        i++;
    }
}

int json_parse(const char *p, long n, jv *v)
{
    long a = ws(p, n, 0), l;
    if (a >= n)
        return -1;
    l = json_value(p + a, n - a);
    if (l < 0 || ws(p, n, a + l) != n)
        return -1;
    v->p = p + a;
    v->n = l;
    return 0;
}

int json_type(jv v)
{
    if (v.n <= 0)
        return J_BAD;
    switch (v.p[0]) {
    case '{':
        return J_OBJ;
    case '[':
        return J_ARR;
    case '"':
        return J_STR;
    case 't':
        return J_TRUE;
    case 'f':
        return J_FALSE;
    case 'n':
        return J_NULL;
    }
    return J_NUM;
}

void json_iter(jv v, jit *it)
{
    it->p = v.p + 1;
    it->e = v.p + v.n - 1;
    if (v.n < 2 || (v.p[0] != '{' && v.p[0] != '['))
        it->p = it->e;
}

int json_next(jit *it, jv *key, jv *val)
{
    long i = ws(it->p, it->e - it->p, 0), l;
    const char *p = it->p;
    long n = it->e - it->p;
    if (i < n && p[i] == ',')
        i = ws(p, n, i + 1);
    if (i >= n)
        return 0;
    if (key) {
        l = str_len(p + i, n - i);
        if (l < 0)
            return 0;
        key->p = p + i;
        key->n = l;
        i = ws(p, n, i + l);
        if (i >= n || p[i] != ':')
            return 0;
        i = ws(p, n, i + 1);
    }
    l = json_value(p + i, n - i);
    if (l < 0)
        return 0;
    val->p = p + i;
    val->n = l;
    it->p = p + i + l;
    return 1;
}

long json_count(jv v)
{
    jit it;
    jv k, x;
    long c = 0;
    json_iter(v, &it);
    while (json_next(&it, json_type(v) == J_OBJ ? &k : 0, &x))
        c++;
    return c;
}

int json_get(jv obj, const char *key, jv *out)
{
    jit it;
    jv k, v;
    if (json_type(obj) != J_OBJ)
        return 0;
    json_iter(obj, &it);
    while (json_next(&it, &k, &v))
        if (json_streq(k, key)) {
            *out = v;
            return 1;
        }
    return 0;
}

static int put_utf8(char *o, unsigned long cp)
{
    if (cp < 0x80) {
        o[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        o[0] = (char)(0xc0 | (cp >> 6));
        o[1] = (char)(0x80 | (cp & 0x3f));
        return 2;
    }
    if (cp < 0x10000) {
        o[0] = (char)(0xe0 | (cp >> 12));
        o[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
        o[2] = (char)(0x80 | (cp & 0x3f));
        return 3;
    }
    o[0] = (char)(0xf0 | (cp >> 18));
    o[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
    o[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
    o[3] = (char)(0x80 | (cp & 0x3f));
    return 4;
}

static unsigned long hex4(const char *p)
{
    return ((unsigned long)hexval((unsigned char)p[0]) << 12) |
           ((unsigned long)hexval((unsigned char)p[1]) << 8) |
           ((unsigned long)hexval((unsigned char)p[2]) << 4) | (unsigned long)hexval((unsigned char)p[3]);
}

/* decodes into out when out is not 0; the decoded length */
static long decode(jv v, char *out, long cap)
{
    long i = 1, o = 0, end = v.n - 1;
    char u[4];
    while (i < end) {
        const char *src = v.p + i;
        int k = 1;
        if (*src != '\\') {
            u[0] = *src;
            i++;
        } else {
            char c = src[1];
            i += 2;
            switch (c) {
            case 'b': u[0] = '\b'; break;
            case 'f': u[0] = '\f'; break;
            case 'n': u[0] = '\n'; break;
            case 'r': u[0] = '\r'; break;
            case 't': u[0] = '\t'; break;
            case 'u': {
                unsigned long cp = hex4(src + 2);
                i += 4;
                if (cp >= 0xd800 && cp < 0xdc00) {
                    if (i + 6 <= end && v.p[i] == '\\' && v.p[i + 1] == 'u') {
                        unsigned long lo = hex4(v.p + i + 2);
                        if (lo >= 0xdc00 && lo < 0xe000) {
                            cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
                            i += 6;
                        } else
                            cp = 0xfffd;
                    } else
                        cp = 0xfffd;
                } else if (cp >= 0xdc00 && cp < 0xe000)
                    cp = 0xfffd;
                k = put_utf8(u, cp);
                break;
            }
            default: u[0] = c; break;
            }
        }
        if (out) {
            int j;
            for (j = 0; j < k; j++)
                if (o + j < cap - 1)
                    out[o + j] = u[j];
        }
        o += k;
    }
    if (out && cap > 0)
        out[o < cap - 1 ? o : cap - 1] = 0;
    return o;
}

long json_str(jv v, char *out, long cap)
{
    if (json_type(v) != J_STR)
        return -1;
    return decode(v, out, cap);
}

char *json_strdup(jv v, long *len)
{
    long l;
    char *s;
    if (json_type(v) != J_STR)
        return 0;
    l = decode(v, 0, 0);
    s = (char *)malloc((size_t)l + 1);
    if (!s)
        return 0;
    decode(v, s, l + 1);
    if (len)
        *len = l;
    return s;
}

int json_streq(jv v, const char *z)
{
    long zl = (long)strlen(z), l, i;
    /* fast path: no escapes */
    if (json_type(v) != J_STR)
        return 0;
    for (i = 1; i < v.n - 1 && v.p[i] != '\\'; i++)
        ;
    if (i == v.n - 1)
        return v.n - 2 == zl && !memcmp(v.p + 1, z, (size_t)zl);
    {
        char buf[256];
        if (zl >= (long)sizeof(buf))
            return 0;
        l = decode(v, buf, sizeof(buf));
        return l == zl && !memcmp(buf, z, (size_t)zl);
    }
}

long json_long(jv v, long def)
{
    long r = 0, i = 0;
    int neg = 0;
    if (json_type(v) != J_NUM)
        return def;
    if (v.p[0] == '-') {
        neg = 1;
        i++;
    }
    for (; i < v.n && digit((unsigned char)v.p[i]); i++)
        r = r * 10 + (v.p[i] - '0');
    return neg ? -r : r;
}

int json_utf8(const char *s, long n, unsigned long *cp)
{
    const unsigned char *u = (const unsigned char *)s;
    int k, i;
    unsigned long c;
    if (u[0] < 0x80) {
        *cp = u[0];
        return 1;
    }
    if (u[0] >= 0xc2 && u[0] <= 0xdf) {
        k = 2;
        c = u[0] & 0x1f;
    } else if (u[0] >= 0xe0 && u[0] <= 0xef) {
        k = 3;
        c = u[0] & 0x0f;
    } else if (u[0] >= 0xf0 && u[0] <= 0xf4) {
        k = 4;
        c = u[0] & 0x07;
    } else
        return 0;
    if (n < k)
        return 0;
    for (i = 1; i < k; i++) {
        if ((u[i] & 0xc0) != 0x80)
            return 0;
        c = (c << 6) | (u[i] & 0x3f);
    }
    if ((k == 3 && (c < 0x800 || (c >= 0xd800 && c < 0xe000))) || (k == 4 && (c < 0x10000 || c > 0x10ffff)))
        return 0;
    *cp = c;
    return k;
}

/* ---- the writer ---- */

void jw_init(jw *w)
{
    w->p = 0;
    w->n = w->cap = 0;
    w->oom = 0;
}

void jw_free(jw *w)
{
    free(w->p);
    jw_init(w);
}

void jw_reset(jw *w)
{
    w->n = 0;
    if (w->p)
        w->p[0] = 0;
}

static int room(jw *w, long more)
{
    long c;
    char *q;
    if (w->oom)
        return 0;
    if (w->n + more + 1 <= w->cap)
        return 1;
    c = w->cap ? w->cap : 256;
    while (c < w->n + more + 1)
        c *= 2;
    q = (char *)realloc(w->p, (size_t)c);
    if (!q) {
        w->oom = 1;
        return 0;
    }
    w->p = q;
    w->cap = c;
    return 1;
}

void jw_raw(jw *w, const char *s, long n)
{
    if (!room(w, n))
        return;
    memcpy(w->p + w->n, s, (size_t)n);
    w->n += n;
    w->p[w->n] = 0;
}

void jw_rawz(jw *w, const char *s)
{
    jw_raw(w, s, (long)strlen(s));
}

void jw_str(jw *w, const char *s, long n)
{
    static const char hex[] = "0123456789abcdef";
    long i = 0, run = 0;
    if (!room(w, n + 2))
        return;
    jw_raw(w, "\"", 1);
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        unsigned long cp;
        int k;
        char e[8];
        if (c >= 0x20 && c != '"' && c != '\\' && c < 0x80) {
            run++;
            i++;
            continue;
        }
        if (run) {
            jw_raw(w, s + i - run, run);
            run = 0;
        }
        if (c >= 0x80) {
            k = json_utf8(s + i, n - i, &cp);
            if (k) {
                jw_raw(w, s + i, k);
                i += k;
            } else {
                k = put_utf8(e, c);      /* a Latin-1 byte */
                jw_raw(w, e, k);
                i++;
            }
            continue;
        }
        e[0] = '\\';
        switch (c) {
        case '"': e[1] = '"'; k = 2; break;
        case '\\': e[1] = '\\'; k = 2; break;
        case '\n': e[1] = 'n'; k = 2; break;
        case '\r': e[1] = 'r'; k = 2; break;
        case '\t': e[1] = 't'; k = 2; break;
        case '\b': e[1] = 'b'; k = 2; break;
        case '\f': e[1] = 'f'; k = 2; break;
        default:
            e[1] = 'u';
            e[2] = '0';
            e[3] = '0';
            e[4] = hex[c >> 4];
            e[5] = hex[c & 15];
            k = 6;
            break;
        }
        jw_raw(w, e, k);
        i++;
    }
    if (run)
        jw_raw(w, s + i - run, run);
    jw_raw(w, "\"", 1);
}

void jw_strz(jw *w, const char *s)
{
    jw_str(w, s, (long)strlen(s));
}

void jw_long(jw *w, long v)
{
    char b[16];
    jw_raw(w, b, cl_ltoa(v, b));
}
