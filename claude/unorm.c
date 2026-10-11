/* see unorm.h */
#include <stdlib.h>
#include <string.h>
#include "unorm.h"
#include "json.h"               /* json_utf8: one UTF-8 character */
#include "unorm_tab.h"

#define SBASE 0xAC00UL
#define LBASE 0x1100UL
#define VBASE 0x1161UL
#define TBASE 0x11A7UL
#define LCOUNT 19
#define VCOUNT 21
#define TCOUNT 28
#define NCOUNT (VCOUNT * TCOUNT)
#define SCOUNT (LCOUNT * NCOUNT)

int un_ccc(unsigned long c)
{
    long lo = 0, hi = UN_CCC - 1;
    if (c < 0x300)
        return 0;
    while (lo <= hi) {
        long mid = (lo + hi) / 2;
        if (c < un_ccc_cp[mid])
            hi = mid - 1;
        else if (c >= un_ccc_cp[mid] + un_ccc_n[mid])
            lo = mid + 1;
        else
            return un_ccc_v[mid];
    }
    return 0;
}

/* the decomposition of c (UTF-8 in un_data): its start, *n its length; -1 none */
static long decomp(unsigned long c, long *n)
{
    long lo = 0, hi;
    if (c < 0xA0)
        return -1;
    if (c < 0x10000) {
        hi = UN_DBMP - 1;
        while (lo <= hi) {
            long mid = (lo + hi) / 2;
            if (c < un_dbmp[mid])
                hi = mid - 1;
            else if (c > un_dbmp[mid])
                lo = mid + 1;
            else {
                *n = (long)un_dbmp_at[mid + 1] - un_dbmp_at[mid];
                return un_dbmp_at[mid];
            }
        }
        return -1;
    }
    hi = UN_DAST - 1;
    while (lo <= hi) {
        long mid = (lo + hi) / 2;
        if (c < un_dast[mid])
            hi = mid - 1;
        else if (c > un_dast[mid])
            lo = mid + 1;
        else {
            *n = (long)un_dast_at[mid + 1] - un_dast_at[mid];
            return un_dast_at[mid];
        }
    }
    return -1;
}

/* the primary composite of a and b, 0 none */
static unsigned long compose(unsigned long a, unsigned long b)
{
    long lo = 0, hi;
    if (a >= LBASE && a < LBASE + LCOUNT && b >= VBASE && b < VBASE + VCOUNT)
        return SBASE + ((a - LBASE) * VCOUNT + (b - VBASE)) * TCOUNT;
    if (a >= SBASE && a < SBASE + SCOUNT && (a - SBASE) % TCOUNT == 0 && b > TBASE && b < TBASE + TCOUNT)
        return a + (b - TBASE);
    if (a < 0x10000 && b < 0x10000) {
        hi = UN_CBMP - 1;
        while (lo <= hi) {
            long mid = (lo + hi) / 2;
            unsigned long ka = un_cbmp[3 * mid], kb = un_cbmp[3 * mid + 1];
            if (a < ka || (a == ka && b < kb))
                hi = mid - 1;
            else if (a > ka || b > kb)
                lo = mid + 1;
            else
                return un_cbmp[3 * mid + 2];
        }
    }
    lo = 0;
    hi = UN_CAST - 1;
    while (lo <= hi) {
        long mid = (lo + hi) / 2;
        unsigned long ka = un_cast[3 * mid], kb = un_cast[3 * mid + 1];
        if (a < ka || (a == ka && b < kb))
            hi = mid - 1;
        else if (a > ka || b > kb)
            lo = mid + 1;
        else
            return un_cast[3 * mid + 2];
    }
    return 0;
}

typedef struct cps {
    unsigned long *p;
    long n, cap;
    int oom;
} cps;

static void put(cps *b, unsigned long c)
{
    if (b->n == b->cap) {
        long nc = b->cap ? b->cap * 2 : 64;
        unsigned long *np = (unsigned long *)realloc(b->p, (size_t)nc * sizeof(unsigned long));
        if (!np) {
            b->oom = 1;
            return;
        }
        b->p = np;
        b->cap = nc;
    }
    b->p[b->n++] = c;
}

/* the next character of s[i..n): its code point, *len its bytes (an invalid byte: U+FFFD, 1) */
static unsigned long next(const char *s, long n, long i, int *len)
{
    unsigned long c;
    if ((unsigned char)s[i] < 0x80) {
        *len = 1;
        return (unsigned char)s[i];
    }
    *len = json_utf8(s + i, n - i, &c);
    if (*len <= 0) {
        *len = 1;
        return 0xFFFD;
    }
    return c;
}

static void put_utf8(char *o, long *k, unsigned long c)
{
    if (c < 0x80)
        o[(*k)++] = (char)c;
    else if (c < 0x800) {
        o[(*k)++] = (char)(0xC0 | (c >> 6));
        o[(*k)++] = (char)(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        o[(*k)++] = (char)(0xE0 | (c >> 12));
        o[(*k)++] = (char)(0x80 | ((c >> 6) & 0x3F));
        o[(*k)++] = (char)(0x80 | (c & 0x3F));
    } else {
        o[(*k)++] = (char)(0xF0 | (c >> 18));
        o[(*k)++] = (char)(0x80 | ((c >> 12) & 0x3F));
        o[(*k)++] = (char)(0x80 | ((c >> 6) & 0x3F));
        o[(*k)++] = (char)(0x80 | (c & 0x3F));
    }
}

int un_nfkc(const char *s, long n, char **out, long *on)
{
    cps b;
    long i, w, starter, k;
    int last, len;
    char *o;
    memset(&b, 0, sizeof(b));
    /* full compatibility decomposition */
    for (i = 0; i < n; i += len) {
        unsigned long c = next(s, n, i, &len);
        long at, dn = 0;
        if (c >= SBASE && c < SBASE + SCOUNT) {
            unsigned long x = c - SBASE;
            put(&b, LBASE + x / NCOUNT);
            put(&b, VBASE + (x % NCOUNT) / TCOUNT);
            if (x % TCOUNT)
                put(&b, TBASE + x % TCOUNT);
        } else if ((at = decomp(c, &dn)) >= 0) {
            long j;
            int l2;
            for (j = 0; j < dn; j += l2)
                put(&b, next((const char *)un_data + at, at + dn, j + at, &l2));
        } else
            put(&b, c);
    }
    if (b.oom) {
        free(b.p);
        return -1;
    }
    /* canonical ordering: each run of non-starters sorted by class, stable */
    for (i = 1; i < b.n; i++) {
        int c = un_ccc(b.p[i]);
        long j = i;
        if (!c)
            continue;
        while (j > 0) {
            int pc = un_ccc(b.p[j - 1]);
            unsigned long t;
            if (!pc || pc <= c)
                break;
            t = b.p[j];
            b.p[j] = b.p[j - 1];
            b.p[j - 1] = t;
            j--;
        }
    }
    /* canonical composition */
    w = b.n ? 1 : 0;
    starter = 0;
    last = b.n ? un_ccc(b.p[0]) : 0;
    if (last)
        last = 256;             /* not a starter: nothing composes with it */
    for (i = 1; i < b.n; i++) {
        unsigned long c = b.p[i], comp;
        int cc = un_ccc(c);
        comp = starter >= 0 && (last < cc || last == 0) ? compose(b.p[starter], c) : 0;
        if (comp && starter >= 0) {
            b.p[starter] = comp;
            continue;
        }
        if (!cc)
            starter = w;
        last = cc;
        b.p[w++] = c;
    }
    o = (char *)malloc((size_t)w * 4 + 1);
    if (!o) {
        free(b.p);
        return -1;
    }
    for (i = 0, k = 0; i < w; i++)
        put_utf8(o, &k, b.p[i]);
    o[k] = 0;
    free(b.p);
    *out = o;
    *on = k;
    return 0;
}
