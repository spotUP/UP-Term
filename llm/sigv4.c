/* see sigv4.h */
#include <stdlib.h>
#include <string.h>
#include "sigv4.h"
#include "sha256.h"
#include "../claude/util.h"

void llm_sigout_init(llm_sigout *o)
{
    memset(o, 0, sizeof(*o));
    jw_init(&o->creq);
    jw_init(&o->sts);
    jw_init(&o->shdr);
    jw_init(&o->auth);
}

void llm_sigout_free(llm_sigout *o)
{
    jw_free(&o->creq);
    jw_free(&o->sts);
    jw_free(&o->shdr);
    jw_free(&o->auth);
}

static void hex(const unsigned char *b, int n, char *out)
{
    static const char d[] = "0123456789abcdef";
    int i;
    for (i = 0; i < n; i++) {
        out[2 * i] = d[b[i] >> 4];
        out[2 * i + 1] = d[b[i] & 15];
    }
    out[2 * n] = 0;
}

void llm_sigv4_date(llm_ts t, char *out)
{
    long days = t.sec / 86400, rem = t.sec % 86400, z, era, doe, yoe, y, doy, mp, d, m;
    if (rem < 0) {
        rem += 86400;
        days--;
    }
    /* Howard Hinnant's civil_from_days */
    z = days + 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2)
        y++;
    out[0] = (char)('0' + y / 1000 % 10);
    out[1] = (char)('0' + y / 100 % 10);
    out[2] = (char)('0' + y / 10 % 10);
    out[3] = (char)('0' + y % 10);
    out[4] = (char)('0' + m / 10);
    out[5] = (char)('0' + m % 10);
    out[6] = (char)('0' + d / 10);
    out[7] = (char)('0' + d % 10);
    out[8] = 'T';
    out[9] = (char)('0' + rem / 36000 % 10);
    out[10] = (char)('0' + rem / 3600 % 10);
    out[11] = (char)('0' + rem % 3600 / 600);
    out[12] = (char)('0' + rem % 600 / 60);
    out[13] = (char)('0' + rem % 60 / 10);
    out[14] = (char)('0' + rem % 10);
    out[15] = 'Z';
    out[16] = 0;
}

/* ---- URI encoding ---- */

static int unreserved(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' ||
           c == '_' || c == '~';
}

/* UriEncode: every byte but the unreserved ones (and "/" when slash) as %XX */
void llm_uri_encode(jw *w, const char *s, long n, int slash)
{
    static const char d[] = "0123456789ABCDEF";
    long i;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (unreserved(c) || (slash && c == '/'))
            jw_raw(w, s + i, 1);
        else {
            char t[3];
            t[0] = '%';
            t[1] = d[c >> 4];
            t[2] = d[c & 15];
            jw_raw(w, t, 3);
        }
    }
}

static int hexv(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* the canonical URI: the path split at "/", "." and ".." and empty segments dealt with when normalize, then encoded */
static void canon_path(jw *w, const char *p, int normalize)
{
    long n = (long)strlen(p);
    if (n == 0) {
        jw_rawz(w, "/");
        return;
    }
    if (!normalize) {
        llm_uri_encode(w, p, n, 1);
        return;
    }
    {
        long i = 0, nseg = 0, cap = 8, k;
        const char **sp = (const char **)malloc((size_t)cap * sizeof(*sp));
        long *sl = (long *)malloc((size_t)cap * sizeof(*sl));
        if (!sp || !sl) {
            free(sp);
            free(sl);
            w->oom = 1;
            return;
        }
        while (i < n) {
            long j = i;
            while (j < n && p[j] != '/')
                j++;
            if (j - i == 0 || (j - i == 1 && p[i] == '.')) {
                /* nothing */
            } else if (j - i == 2 && p[i] == '.' && p[i + 1] == '.') {
                if (nseg > 0)
                    nseg--;
            } else {
                if (nseg == cap) {
                    const char **np;
                    long *nl;
                    cap *= 2;
                    np = (const char **)realloc(sp, (size_t)cap * sizeof(*sp));
                    if (!np) {
                        free(sp);
                        free(sl);
                        w->oom = 1;
                        return;
                    }
                    sp = np;
                    nl = (long *)realloc(sl, (size_t)cap * sizeof(*sl));
                    if (!nl) {
                        free(sp);
                        free(sl);
                        w->oom = 1;
                        return;
                    }
                    sl = nl;
                }
                sp[nseg] = p + i;
                sl[nseg++] = j - i;
            }
            i = j + 1;
        }
        jw_rawz(w, "/");
        for (k = 0; k < nseg; k++) {
            llm_uri_encode(w, sp[k], sl[k], 0);
            if (k + 1 < nseg || (n > 1 && p[n - 1] == '/'))
                jw_rawz(w, "/");
        }
        free(sp);
        free(sl);
    }
}

/* ---- the canonical query ---- */

typedef struct qp {
    char *k, *v;                    /* encoded name and value, malloc'd */
} qp;

static char *enc_dup(const char *s, long n, int decode)
{
    jw t;
    char *d = 0;
    long i;
    jw_init(&t);
    if (decode) {
        char *raw = (char *)malloc((size_t)n + 1);
        long m = 0;
        if (!raw) {
            return 0;
        }
        for (i = 0; i < n; i++) {
            if (s[i] == '%' && i + 2 < n && hexv((unsigned char)s[i + 1]) >= 0 &&
                hexv((unsigned char)s[i + 2]) >= 0) {
                raw[m++] = (char)(hexv((unsigned char)s[i + 1]) * 16 + hexv((unsigned char)s[i + 2]));
                i += 2;
            } else
                raw[m++] = s[i];
        }
        llm_uri_encode(&t, raw, m, 0);
        free(raw);
    } else
        llm_uri_encode(&t, s, n, 0);
    if (!t.oom) {
        d = (char *)malloc((size_t)t.n + 1);
        if (d) {
            memcpy(d, t.p ? t.p : "", (size_t)t.n);
            d[t.n] = 0;
        }
    }
    jw_free(&t);
    return d;
}

static int qp_cmp(const void *a, const void *b)
{
    const qp *x = (const qp *)a, *y = (const qp *)b;
    int c = strcmp(x->k, y->k);
    return c ? c : strcmp(x->v, y->v);
}

/* ---- the canonical headers ---- */

typedef struct hh {
    char *name;                     /* lower case */
    jw val;                         /* the values joined with "," */
} hh;

static int hh_cmp(const void *a, const void *b)
{
    return strcmp(((const hh *)a)->name, ((const hh *)b)->name);
}

/* a header value: the leading and trailing white space gone, runs of white space (a folded line included) one space */
static void put_value(jw *w, const char *v)
{
    long i = 0, n = (long)strlen(v);
    int sp = 0, any = 0;
    while (i < n && (v[i] == ' ' || v[i] == '\t' || v[i] == '\r' || v[i] == '\n'))
        i++;
    for (; i < n; i++) {
        char c = v[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            sp = 1;
        else {
            if (sp && any)
                jw_rawz(w, " ");
            sp = 0;
            any = 1;
            jw_raw(w, &c, 1);
        }
    }
}

static int hh_add(hh **hs, int *n, int *cap, const char *name, const char *value)
{
    int i;
    hh *h = 0;
    char *ln;
    long k, nl = (long)strlen(name);
    ln = (char *)malloc((size_t)nl + 1);
    if (!ln)
        return -1;
    for (k = 0; k < nl; k++)
        ln[k] = name[k] >= 'A' && name[k] <= 'Z' ? (char)(name[k] + 32) : name[k];
    ln[nl] = 0;
    for (i = 0; i < *n; i++)
        if (!strcmp((*hs)[i].name, ln))
            h = &(*hs)[i];
    if (h) {
        free(ln);
        jw_rawz(&h->val, ",");
    } else {
        if (*n == *cap) {
            hh *g;
            *cap = *cap ? *cap * 2 : 8;
            g = (hh *)realloc(*hs, (size_t)*cap * sizeof(**hs));
            if (!g) {
                free(ln);
                return -1;
            }
            *hs = g;
        }
        h = &(*hs)[(*n)++];
        h->name = ln;
        jw_init(&h->val);
    }
    put_value(&h->val, value);
    return h->val.oom ? -1 : 0;
}

int llm_sigv4_sign(const llm_sigv4 *in, llm_sigout *o)
{
    unsigned char dig[32], kd[32], k2[32];
    char scope[96], datestamp[9], sha[65];
    jw q;
    hh *hs = 0;
    int nh = 0, caph = 0, i, rc = -1, nq = 0, capq = 0;
    qp *qs = 0;
    const char *query = in->query ? in->query : "";
    int tok_header = in->token && !in->query_form;
    int tok_query = in->token && in->query_form;
    char exp[16];

    llm_sha256_hex(in->body ? in->body : "", in->bn, o->payload);
    memcpy(datestamp, in->amzdate, 8);
    datestamp[8] = 0;
    cl_copy(scope, datestamp, sizeof(scope));
    cl_cat(scope, "/", sizeof(scope));
    cl_cat(scope, in->region, sizeof(scope));
    cl_cat(scope, "/", sizeof(scope));
    cl_cat(scope, in->service, sizeof(scope));
    cl_cat(scope, "/aws4_request", sizeof(scope));
    jw_init(&q);
    for (i = 0; i < in->nhdr; i++)
        if (hh_add(&hs, &nh, &caph, in->hdr[i].name, in->hdr[i].value))
            goto out;
    if (!in->query_form) {
        if (hh_add(&hs, &nh, &caph, "x-amz-date", in->amzdate))
            goto out;
        if (tok_header && in->sign_token && hh_add(&hs, &nh, &caph, "x-amz-security-token", in->token))
            goto out;
        if (in->sign_body && hh_add(&hs, &nh, &caph, "x-amz-content-sha256", o->payload))
            goto out;
    }
    qsort(hs, (size_t)nh, sizeof(*hs), hh_cmp);
    for (i = 0; i < nh; i++) {
        jw_rawz(&o->shdr, i ? ";" : "");
        jw_rawz(&o->shdr, hs[i].name);
    }
    /* the query: the request's parameters, and in the query form the X-Amz-* ones */
    {
        const char *p = query;
        while (*p) {
            const char *e = strchr(p, '&'), *eq;
            long len = e ? (long)(e - p) : (long)strlen(p);
            if (len > 0) {
                long kl;
                eq = (const char *)memchr(p, '=', (size_t)len);
                kl = eq ? (long)(eq - p) : len;
                if (nq == capq) {
                    qp *g;
                    capq = capq ? capq * 2 : 8;
                    g = (qp *)realloc(qs, (size_t)capq * sizeof(*qs));
                    if (!g)
                        goto out;
                    qs = g;
                }
                qs[nq].k = enc_dup(p, kl, 1);
                qs[nq].v = enc_dup(eq ? eq + 1 : "", eq ? len - kl - 1 : 0, 1);
                if (!qs[nq].k || !qs[nq].v) {
                    free(qs[nq].k);
                    free(qs[nq].v);
                    goto out;
                }
                nq++;
            }
            if (!e)
                break;
            p = e + 1;
        }
    }
    if (in->query_form) {
        jw cred;
        const char *names[6];
        char *vals[6];
        int j, nn = 0;
        jw_init(&cred);
        jw_rawz(&cred, in->access);
        jw_rawz(&cred, "/");
        jw_rawz(&cred, scope);
        cl_ltoa(in->expires, exp);
        names[nn] = "X-Amz-Algorithm";
        vals[nn++] = enc_dup("AWS4-HMAC-SHA256", 16, 0);
        names[nn] = "X-Amz-Credential";
        vals[nn++] = enc_dup(cred.p, cred.n, 0);
        names[nn] = "X-Amz-Date";
        vals[nn++] = enc_dup(in->amzdate, (long)strlen(in->amzdate), 0);
        names[nn] = "X-Amz-Expires";
        vals[nn++] = enc_dup(exp, (long)strlen(exp), 0);
        names[nn] = "X-Amz-SignedHeaders";
        vals[nn++] = enc_dup(o->shdr.p ? o->shdr.p : "", o->shdr.n, 0);
        if (tok_query && in->sign_token) {
            names[nn] = "X-Amz-Security-Token";
            vals[nn++] = enc_dup(in->token, (long)strlen(in->token), 0);
        }
        jw_free(&cred);
        for (j = 0; j < nn; j++) {
            if (nq == capq) {
                qp *g;
                capq = capq ? capq * 2 : 8;
                g = (qp *)realloc(qs, (size_t)capq * sizeof(*qs));
                if (!g)
                    goto out;
                qs = g;
            }
            qs[nq].k = enc_dup(names[j], (long)strlen(names[j]), 0);
            qs[nq].v = vals[j];
            if (!qs[nq].k || !qs[nq].v)
                goto out;
            nq++;
        }
    }
    qsort(qs, (size_t)nq, sizeof(*qs), qp_cmp);
    for (i = 0; i < nq; i++) {
        jw_rawz(&q, i ? "&" : "");
        jw_rawz(&q, qs[i].k);
        jw_rawz(&q, "=");
        jw_rawz(&q, qs[i].v);
    }
    /* the canonical request */
    jw_rawz(&o->creq, in->method);
    jw_rawz(&o->creq, "\n");
    canon_path(&o->creq, in->path ? in->path : "", in->normalize);
    jw_rawz(&o->creq, "\n");
    if (q.n)
        jw_raw(&o->creq, q.p, q.n);
    jw_rawz(&o->creq, "\n");
    for (i = 0; i < nh; i++) {
        jw_rawz(&o->creq, hs[i].name);
        jw_rawz(&o->creq, ":");
        jw_raw(&o->creq, hs[i].val.p ? hs[i].val.p : "", hs[i].val.n);
        jw_rawz(&o->creq, "\n");
    }
    jw_rawz(&o->creq, "\n");
    jw_raw(&o->creq, o->shdr.p ? o->shdr.p : "", o->shdr.n);
    jw_rawz(&o->creq, "\n");
    jw_rawz(&o->creq, o->payload);
    /* the string to sign */
    llm_sha256_hex(o->creq.p ? o->creq.p : "", o->creq.n, sha);
    jw_rawz(&o->sts, "AWS4-HMAC-SHA256\n");
    jw_rawz(&o->sts, in->amzdate);
    jw_rawz(&o->sts, "\n");
    jw_rawz(&o->sts, scope);
    jw_rawz(&o->sts, "\n");
    jw_rawz(&o->sts, sha);
    /* the key: HMAC over the date, region, service and "aws4_request", seeded with "AWS4" + secret */
    {
        jw seed;
        jw_init(&seed);
        jw_rawz(&seed, "AWS4");
        jw_rawz(&seed, in->secret);
        llm_hmac_sha256(seed.p, seed.n, datestamp, 8, dig);
        jw_free(&seed);
    }
    llm_hmac_sha256(dig, 32, in->region, (long)strlen(in->region), kd);
    llm_hmac_sha256(kd, 32, in->service, (long)strlen(in->service), k2);
    llm_hmac_sha256(k2, 32, "aws4_request", 12, kd);
    llm_hmac_sha256(kd, 32, o->sts.p ? o->sts.p : "", o->sts.n, dig);
    hex(dig, 32, o->sig);
    if (in->query_form) {
        /* the query a presigned request carries: X-Amz-Algorithm .. X-Amz-Expires [.. Token], then the signature */
        jw_rawz(&o->auth, "X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=");
        {
            jw cred;
            jw_init(&cred);
            jw_rawz(&cred, in->access);
            jw_rawz(&cred, "/");
            jw_rawz(&cred, scope);
            llm_uri_encode(&o->auth, cred.p, cred.n, 0);
            jw_free(&cred);
        }
        jw_rawz(&o->auth, "&X-Amz-Date=");
        jw_rawz(&o->auth, in->amzdate);
        jw_rawz(&o->auth, "&X-Amz-SignedHeaders=");
        llm_uri_encode(&o->auth, o->shdr.p ? o->shdr.p : "", o->shdr.n, 0);
        jw_rawz(&o->auth, "&X-Amz-Expires=");
        jw_rawz(&o->auth, exp);
        if (tok_query) {
            jw_rawz(&o->auth, "&X-Amz-Security-Token=");
            llm_uri_encode(&o->auth, in->token, (long)strlen(in->token), 0);
        }
        jw_rawz(&o->auth, "&X-Amz-Signature=");
        jw_rawz(&o->auth, o->sig);
    } else {
        jw_rawz(&o->auth, "AWS4-HMAC-SHA256 Credential=");
        jw_rawz(&o->auth, in->access);
        jw_rawz(&o->auth, "/");
        jw_rawz(&o->auth, scope);
        jw_rawz(&o->auth, ", SignedHeaders=");
        jw_raw(&o->auth, o->shdr.p ? o->shdr.p : "", o->shdr.n);
        jw_rawz(&o->auth, ", Signature=");
        jw_rawz(&o->auth, o->sig);
    }
    rc = o->creq.oom || o->sts.oom || o->auth.oom || o->shdr.oom || q.oom ? -1 : 0;
out:
    for (i = 0; i < nh; i++) {
        free(hs[i].name);
        jw_free(&hs[i].val);
    }
    free(hs);
    for (i = 0; i < nq; i++) {
        free(qs[i].k);
        free(qs[i].v);
    }
    free(qs);
    jw_free(&q);
    return rc;
}
