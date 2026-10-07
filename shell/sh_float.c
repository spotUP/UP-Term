/* printf's floating conversions (see sh_float.h). */
#include <string.h>
#include "sh_float.h"

#define NL 100          /* 16-bit limbs: 1600 bits, 10^330 times a 40-digit number fits */

typedef struct fbig {
    unsigned long d[NL];    /* each below 65536 (the limb arithmetic fits 32 bits) */
    int n;
} fbig;

static void bnorm(fbig *b)
{
    while (b->n && !b->d[b->n - 1])
        b->n--;
}

/* b = b * m + a, m and a below 65536 */
static void bmul(fbig *b, unsigned long m, unsigned long a)
{
    int i;
    for (i = 0; i < b->n; i++) {
        unsigned long t = b->d[i] * m + a;
        b->d[i] = t & 0xffff;
        a = t >> 16;
    }
    if (a && b->n < NL)
        b->d[b->n++] = a;
}

/* b = b / dv (below 65536), returns the remainder */
static unsigned long bdivs(fbig *b, unsigned long dv)
{
    unsigned long rem = 0;
    int i;
    for (i = b->n - 1; i >= 0; i--) {
        unsigned long t = (rem << 16) | b->d[i];
        b->d[i] = t / dv;
        rem = t % dv;
    }
    bnorm(b);
    return rem;
}

static int bbits(const fbig *b)
{
    int k = 0;
    unsigned long t;
    if (!b->n)
        return 0;
    for (t = b->d[b->n - 1]; t; t >>= 1)
        k++;
    return 16 * (b->n - 1) + k;
}

static int bbit(const fbig *b, int i)
{
    return i / 16 < b->n ? (int)((b->d[i / 16] >> (i % 16)) & 1) : 0;
}

static void bshl(fbig *b, int k)
{
    int limbs = k / 16, bits = k % 16, i, on = b->n + limbs + 1;
    if (!b->n)
        return;
    if (on > NL)
        on = NL;
    for (i = on - 1; i >= 0; i--) {
        int lo = i - limbs;
        unsigned long v = 0;
        if (lo >= 0 && lo < b->n)
            v = b->d[lo] << bits;
        if (lo >= 1 && lo - 1 < b->n)
            v |= b->d[lo - 1] >> (16 - bits);
        b->d[i] = v & 0xffff;
    }
    b->n = on;
    bnorm(b);
}

static void bshr(fbig *b, int k)
{
    int limbs = k / 16, bits = k % 16, i;
    if (limbs >= b->n) {
        b->n = 0;
        return;
    }
    for (i = 0; i < b->n - limbs; i++) {
        unsigned long v = b->d[i + limbs] >> bits;
        if (i + limbs + 1 < b->n)
            v |= b->d[i + limbs + 1] << (16 - bits);
        b->d[i] = v & 0xffff;
    }
    b->n -= limbs;
    bnorm(b);
}

static int bcmpn(const fbig *a, const fbig *b)
{
    int i;
    if (a->n != b->n)
        return a->n < b->n ? -1 : 1;
    for (i = a->n - 1; i >= 0; i--)
        if (a->d[i] != b->d[i])
            return a->d[i] < b->d[i] ? -1 : 1;
    return 0;
}

static void bsub(fbig *a, const fbig *b)    /* a >= b */
{
    long borrow = 0;
    int i;
    for (i = 0; i < a->n; i++) {
        long t = (long)a->d[i] - (i < b->n ? (long)b->d[i] : 0) - borrow;
        borrow = t < 0;
        a->d[i] = (unsigned long)(t + (borrow ? 65536 : 0));
    }
    bnorm(a);
}

static void bone(fbig *b)
{
    b->d[0] = 1;
    b->n = 1;
}

static void bpow10(fbig *b, int n)
{
    bone(b);
    for (; n >= 4; n -= 4)
        bmul(b, 10000, 0);
    for (; n > 0; n--)
        bmul(b, 10, 0);
}

/* v (any size) to 53 bits, half to even; sticky: bits below v are not zero.
 * *e2 grows by the bits dropped. */
static void round53(fbig *v, int sticky, int *e2)
{
    int l = bbits(v), s, half, i;
    s = l - 53;
    if (*e2 + s < -1074)    /* a subnormal: fewer bits */
        s = -1074 - *e2;
    if (s <= 0)
        return;
    half = bbit(v, s - 1);
    for (i = 0; i < s - 1 && !sticky; i++)
        sticky = bbit(v, i);
    bshr(v, s);
    *e2 += s;
    if (half && (sticky || (v->d[0] & 1))) {
        for (i = 0; i < v->n && ++v->d[i] == 65536; i++)
            v->d[i] = 0;
        if (i == v->n)
            v->d[v->n++] = 1;
        if (bbits(v) > 53) {
            bshr(v, 1);
            (*e2)++;
        }
        if (!v->n)
            *e2 = 0;
    }
}

typedef struct sh_dbl {
    int neg, kind;      /* kind: 0 finite, 1 inf, 2 nan */
    int range;          /* the value overflowed, underflowed or is subnormal */
    fbig m;             /* value = m * 2^e2 */
    int e2;
} sh_dbl;

static int lowc(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static int hexval(int c)
{
    c = lowc(c);
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

static int has_word(const char *s, const char *w)    /* prefix, any case */
{
    for (; *w; s++, w++)
        if (lowc(*s) != *w)
            return 0;
    return 1;
}

/* the number at s; returns 1 when anything follows it or none is there */
static int dbl_parse(const char *s, sh_dbl *f)
{
    fbig *m = &f->m, d, p;
    int digs = 0, e10 = 0, seen = 0, sticky = 0, dot = 0, ex = 0, es = 1, q;
    memset(f, 0, sizeof *f);
    while (*s == ' ' || *s == '\t' || *s == '\n')
        s++;
    if (*s == '-' || *s == '+')
        f->neg = *s++ == '-';
    if (has_word(s, "inf")) {
        f->kind = 1;
        return !(has_word(s, "infinity") ? !s[8] : !s[3]);
    }
    if (has_word(s, "nan")) {
        f->kind = 2;
        return s[3] != 0;
    }
    d.n = 0;
    if (s[0] == '0' && lowc(s[1]) == 'x' && hexval(s[2]) >= 0) {
        for (s += 2; hexval(*s) >= 0; s++) {
            int v = hexval(*s);
            if (d.n < NL - 3)
                bmul(&d, 16, (unsigned long)v);
            else
                e10 = 400;   /* too large: infinity below */
        }
    } else {
        for (; (*s >= '0' && *s <= '9') || (*s == '.' && !dot); s++) {
            if (*s == '.') {
                dot = 1;
                continue;
            }
            seen = 1;
            if (!d.n && *s == '0') {
                e10 -= dot;      /* a leading zero counts only after the point */
                continue;
            }
            if (digs < 40) {
                bmul(&d, 10, (unsigned long)(*s - '0'));
                digs++;
                e10 -= dot;
            } else {
                sticky |= *s != '0';
                e10 += !dot;
            }
        }
        if (!seen)
            return 1;
        if (lowc(*s) == 'e' && ((s[1] >= '0' && s[1] <= '9') ||
                                ((s[1] == '-' || s[1] == '+') && s[2] >= '0' && s[2] <= '9'))) {
            s++;
            if (*s == '-' || *s == '+')
                es = *s++ == '-' ? -1 : 1;
            for (; *s >= '0' && *s <= '9'; s++)
                if (ex < 9999)
                    ex = ex * 10 + (*s - '0');
            e10 += es * ex;
        }
        if (sticky) {       /* digits dropped: a lone 1 stands for the nonzero tail */
            bmul(&d, 10, 1);
            e10--;
        }
    }
    q = *s != 0;
    if (!d.n)
        return q;           /* zero */
    if (e10 >= 0) {
        int k;
        if (e10 > 330) {
            f->kind = f->range = 1;
            return q;
        }
        for (k = e10; k >= 4; k -= 4)
            bmul(&d, 10000, 0);
        for (; k > 0; k--)
            bmul(&d, 10, 0);
        *m = d;
        round53(m, 0, &f->e2);
    } else {
        int n = -e10, sh, k;
        if (n > 410) {
            f->range = 1;
            return q;       /* below the smallest double: zero */
        }
        bpow10(&p, n);
        sh = bbits(&p) + 57 - bbits(&d);
        if (sh < 0)
            sh = 0;
        bshl(&d, sh);       /* D * 2^sh / 10^n has at least 55 bits */
        k = bbits(&d) - bbits(&p);
        bshl(&p, k);
        if (bcmpn(&d, &p) < 0) {
            k--;
            bshr(&p, 1);
        }
        for (; k >= 0; k--) {
            int bit = bcmpn(&d, &p) >= 0;
            if (bit)
                bsub(&d, &p);
            bmul(m, 2, (unsigned long)bit);
            bshr(&p, 1);
        }
        f->e2 = -sh;
        round53(m, d.n != 0, &f->e2);
    }
    if (f->e2 + bbits(m) > 1024)
        f->kind = f->range = 1;
    else if (!m->n || f->e2 + bbits(m) <= -1022)
        f->range = 1;
    return q;
}

/* the next digit after the point of fr / 2^fk, which it removes */
static int fracdigit(fbig *fr, int fk)
{
    int dg = 0, idx = fk / 16, off = fk % 16;
    bmul(fr, 10, 0);
    if (idx < fr->n) {
        unsigned long w = fr->d[idx] | (idx + 1 < fr->n ? fr->d[idx + 1] << 16 : 0);
        dg = (int)(w >> off);
        fr->d[idx] &= (1UL << off) - 1;
        fr->n = idx + 1;
        bnorm(fr);
    }
    return dg;
}

/* The decimal digits of f rounded: mode 'f': count digits after the point;
 * mode 'e': count significant digits. out holds the digits (no point), *dp is
 * the number of them before the point (value = 0.DIGITS * 10^dp). Returns
 * how many digits there are. */
static int dbl_digits(const sh_dbl *f, int mode, int count, char *out, int *dp)
{
    fbig t, fr;
    int fk = 0, len = 0, k = 0, nd, i, up, sticky;
    char tmp[420];
    t = f->m;
    fr.n = 0;
    if (!t.n) {
        memset(out, '0', (size_t)count);
        *dp = mode == 'f' ? 0 : 1;
        return count;
    }
    if (f->e2 >= 0)
        bshl(&t, f->e2);
    else {
        fk = -f->e2;
        fr = t;
        bshr(&t, fk);       /* the integer part */
        {
            fbig u = t;
            bshl(&u, fk);
            bsub(&fr, &u);  /* the fraction: fr / 2^fk */
        }
    }
    while (t.n) {
        unsigned long r = bdivs(&t, 10000);
        for (i = 0; i < 4; i++) {
            tmp[k++] = (char)('0' + r % 10);
            r /= 10;
        }
    }
    while (k && tmp[k - 1] == '0')
        k--;
    while (len < k) {
        out[len] = tmp[k - 1 - len];
        len++;
    }
    *dp = len;
    if (mode == 'e' && !len) {      /* skip the zeros after the point */
        int dg;
        while (!(dg = fracdigit(&fr, fk)))
            (*dp)--;
        out[len++] = (char)('0' + dg);
    }
    nd = mode == 'f' ? *dp + count : count;
    while (len < nd + 1)
        out[len++] = (char)('0' + fracdigit(&fr, fk));
    sticky = fr.n != 0;
    for (i = nd + 1; i < len && !sticky; i++)
        sticky = out[i] != '0';
    up = out[nd] > '5' || (out[nd] == '5' && (sticky || (nd > 0 && ((out[nd - 1] - '0') & 1))));
    if (up) {
        for (i = nd - 1; i >= 0 && out[i] == '9'; i--)
            out[i] = '0';
        if (i >= 0)
            out[i]++;
        else {
            memmove(out + 1, out, (size_t)nd);
            out[0] = '1';
            (*dp)++;
            if (mode == 'f')
                nd++;
        }
    }
    return nd;
}

static char *put_exp(char *o, int x, int upper)
{
    char d[8];
    int n = 0;
    *o++ = upper ? 'E' : 'e';
    *o++ = x < 0 ? '-' : '+';
    if (x < 0)
        x = -x;
    do
        d[n++] = (char)('0' + x % 10);
    while ((x /= 10) > 0);
    if (n < 2)
        d[n++] = '0';
    while (n)
        *o++ = d[--n];
    return o;
}

int sh_float_format(const char *arg, int spec, int plus, int space, int alt, long prec,
                    char *body, int *pl, int *zero)
{
    sh_dbl f;
    char dig[SH_FLOAT_MAXPREC + 340];
    char *o = body;
    int bad = 0, upper = spec < 'a', lc = lowc(spec), dp = 0, nd, i;
    if (prec > SH_FLOAT_MAXPREC)
        prec = SH_FLOAT_MAXPREC;
    if (!arg)
        memset(&f, 0, sizeof f);
    else if (*arg == '\'' || *arg == '"') {
        unsigned long c = (unsigned char)arg[1];
        memset(&f, 0, sizeof f);
        if (c) {
            f.m.d[0] = c;
            f.m.n = 1;
        }
    } else {
        bad = dbl_parse(arg, &f);
        if (!bad && f.range)
            bad = 2;
    }
    *zero = f.kind == 0;
    if (f.kind == 2)
        ;           /* nan carries no sign */
    else if (f.neg)
        *o++ = '-';
    else if (plus)
        *o++ = '+';
    else if (space)
        *o++ = ' ';
    *pl = (int)(o - body);
    if (f.kind) {
        const char *w = f.kind == 1 ? (upper ? "INF" : "inf") : (upper ? "NAN" : "nan");
        strcpy(o, w);
        return bad;
    }
    if (prec < 0)
        prec = 6;
    if (lc == 'f') {
        nd = dbl_digits(&f, 'f', (int)prec, dig, &dp);
        if (dp <= 0)
            *o++ = '0';
        for (i = 0; i < dp; i++)
            *o++ = dig[i];
        if (prec > 0 || alt)
            *o++ = '.';
        for (; i < nd; i++)
            *o++ = dig[i];
    } else if (lc == 'e') {
        nd = dbl_digits(&f, 'e', (int)prec + 1, dig, &dp);
        *o++ = dig[0];
        if (prec > 0 || alt)
            *o++ = '.';
        for (i = 1; i < nd; i++)
            *o++ = dig[i];
        o = put_exp(o, dp - 1, upper);
    } else {            /* g */
        int p = prec == 0 ? 1 : (int)prec, x, fl;
        nd = dbl_digits(&f, 'e', p, dig, &dp);
        x = dp - 1;
        if (!alt)
            while (nd > 1 && dig[nd - 1] == '0')
                nd--;
        if (x < -4 || x >= p) {
            *o++ = dig[0];
            if (nd > 1 || alt)
                *o++ = '.';
            for (i = 1; i < nd; i++)
                *o++ = dig[i];
            o = put_exp(o, x, upper);
        } else {
            if (dp <= 0)
                *o++ = '0';
            for (i = 0; i < dp; i++)
                *o++ = i < nd ? dig[i] : '0';
            fl = (dp < 0 ? -dp : 0) + (nd > dp && dp > 0 ? nd - dp : dp <= 0 ? nd : 0);
            if (fl > 0 || alt)
                *o++ = '.';
            for (i = dp; i < 0; i++)
                *o++ = '0';
            for (i = dp > 0 ? dp : 0; i < nd; i++)
                *o++ = dig[i];
        }
    }
    *o = 0;
    return bad;
}
