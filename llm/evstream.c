/* see evstream.h */
#include <stdlib.h>
#include <string.h>
#include "evstream.h"
#include "../claude/util.h"

#define M32 0xffffffffUL

static unsigned long crc_tab[256];
static int crc_ready;

unsigned long llm_crc32(unsigned long crc, const void *p, long n)
{
    const unsigned char *b = (const unsigned char *)p;
    long i;
    if (!crc_ready) {
        int k, j;
        for (k = 0; k < 256; k++) {
            unsigned long c = (unsigned long)k;
            for (j = 0; j < 8; j++)
                c = (c & 1) ? (0xedb88320UL ^ (c >> 1)) : (c >> 1);
            crc_tab[k] = c;
        }
        crc_ready = 1;
    }
    crc = ~crc & M32;
    for (i = 0; i < n; i++)
        crc = crc_tab[(crc ^ b[i]) & 0xff] ^ (crc >> 8);
    return ~crc & M32;
}

static unsigned long be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

void llm_evs_init(llm_evs *s, void (*fn)(void *u, const llm_evmsg *m), void *u)
{
    memset(s, 0, sizeof(*s));
    s->fn = fn;
    s->u = u;
}

void llm_evs_free(llm_evs *s)
{
    free(s->msg);
    s->msg = 0;
    s->cap = 0;
}

/* the decimal text of an unsigned 32-bit value (cl_ltoa takes a long, which is 32 bits signed on the 68k) */
static void utoa(unsigned long v, char *out)
{
    char t[12];
    int n = 0, i;
    if (!v)
        t[n++] = '0';
    while (v) {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    }
    for (i = 0; i < n; i++)
        out[i] = t[n - 1 - i];
    out[n] = 0;
}

static int fail2(llm_evs *s, const char *a, unsigned long x, const char *b, unsigned long y, const char *c)
{
    char num[16];
    s->state = -1;
    cl_copy(s->err, a, sizeof(s->err));
    utoa(x, num);
    cl_cat(s->err, num, sizeof(s->err));
    cl_cat(s->err, b, sizeof(s->err));
    utoa(y, num);
    cl_cat(s->err, num, sizeof(s->err));
    cl_cat(s->err, c, sizeof(s->err));
    return -1;
}

/* the headers of a checked message: the string ones this layer reads; -1 when they do not parse */
static int headers(const unsigned char *h, long n, char *mt, char *et, char *xt, char *ct, long cap)
{
    long i = 0;
    while (i < n) {
        long nl = h[i++], vl;
        const unsigned char *name = h + i;
        int type;
        if (i + nl + 1 > n)
            return -1;
        i += nl;
        type = h[i++];
        switch (type) {
        case 0:
        case 1:
            vl = 0;
            break;
        case 2:
            vl = 1;
            break;
        case 3:
            vl = 2;
            break;
        case 4:
            vl = 4;
            break;
        case 5:
        case 8:
            vl = 8;
            break;
        case 9:
            vl = 16;
            break;
        case 6:
        case 7:
            if (i + 2 > n)
                return -1;
            vl = ((long)h[i] << 8) | h[i + 1];
            i += 2;
            break;
        default:
            return -1;
        }
        if (i + vl > n)
            return -1;
        if (type == 7) {
            char *dst = 0;
            if (nl == 13 && !memcmp(name, ":message-type", 13))
                dst = mt;
            else if (nl == 11 && !memcmp(name, ":event-type", 11))
                dst = et;
            else if (nl == 15 && !memcmp(name, ":exception-type", 15))
                dst = xt;
            else if (nl == 13 && !memcmp(name, ":content-type", 13))
                dst = ct;
            if (dst) {
                long m = vl < cap - 1 ? vl : cap - 1;
                memcpy(dst, h + i, (size_t)m);
                dst[m] = 0;
            }
        }
        i += vl;
    }
    return 0;
}

/* a whole message in s->msg (total bytes, prelude included): check it, hand it over */
static int message(llm_evs *s)
{
    unsigned long crc = llm_crc32(0, s->msg, s->total - 4);
    unsigned long want = be32(s->msg + s->total - 4);
    char mt[64], et[64], xt[64], ct[64];
    llm_evmsg m;
    if (crc != want) {
        char a[16], b[16];
        utoa(crc, a);
        utoa(want, b);
        s->state = -1;
        cl_copy(s->err, "The message checksum (", sizeof(s->err));
        cl_cat(s->err, a, sizeof(s->err));
        cl_cat(s->err, ") did not match the expected value of ", sizeof(s->err));
        cl_cat(s->err, b, sizeof(s->err));
        return -1;
    }
    mt[0] = et[0] = xt[0] = ct[0] = 0;
    if (headers(s->msg + 12, s->hlen, mt, et, xt, ct, 64) < 0) {
        s->state = -1;
        cl_copy(s->err, "The message's headers are malformed", sizeof(s->err));
        return -1;
    }
    m.message_type = mt;
    m.event_type = et;
    m.exception_type = xt;
    m.content_type = ct;
    m.payload = s->msg + 12 + s->hlen;
    m.n = s->total - 16 - s->hlen;
    s->messages++;
    if (s->fn)
        s->fn(s->u, &m);
    return 0;
}

int llm_evs_feed(llm_evs *s, const void *p, long n)
{
    const unsigned char *b = (const unsigned char *)p;
    while (n > 0) {
        if (s->state < 0)
            return -1;
        if (s->state == 0) {
            long take = 12 - s->have;
            if (take > n)
                take = n;
            memcpy(s->pre + s->have, b, (size_t)take);
            s->have += take;
            b += take;
            n -= take;
            if (s->have < 12)
                break;
            {
                unsigned long want = be32(s->pre + 8), crc = llm_crc32(0, s->pre, 8);
                if (crc != want)
                    return fail2(s, "The prelude checksum specified in the message (", want,
                                 ") does not match the calculated CRC32 checksum (", crc, ")");
            }
            s->total = (long)be32(s->pre);
            s->hlen = (long)be32(s->pre + 4);
            if (be32(s->pre) < 16 || be32(s->pre) > (unsigned long)LLM_EVS_MAX) {
                s->state = -1;
                cl_copy(s->err, be32(s->pre) < 16 ? "Provided message too short to accommodate event stream message overhead"
                                                  : "The message is longer than the limit", sizeof(s->err));
                return -1;
            }
            if (be32(s->pre + 4) > be32(s->pre) - 16) {
                s->state = -1;
                cl_copy(s->err, "The headers are longer than the message", sizeof(s->err));
                return -1;
            }
            if (s->cap < s->total) {
                unsigned char *g = (unsigned char *)realloc(s->msg, (size_t)s->total);
                if (!g) {
                    s->state = -1;
                    cl_copy(s->err, "out of memory", sizeof(s->err));
                    return -1;
                }
                s->msg = g;
                s->cap = s->total;
            }
            memcpy(s->msg, s->pre, 12);
            s->state = 1;
        }
        if (s->state == 1) {
            long take = s->total - s->have;
            if (take > n)
                take = n;
            memcpy(s->msg + s->have, b, (size_t)take);
            s->have += take;
            b += take;
            n -= take;
            if (s->have == s->total) {
                int rc = message(s);
                s->have = 0;
                s->state = rc ? -1 : 0;
                if (rc)
                    return -1;
            }
        }
    }
    return s->state < 0 ? -1 : 0;
}

int llm_evs_end(llm_evs *s)
{
    if (s->state < 0)
        return -1;
    if (s->have > 0) {
        s->state = -1;
        cl_copy(s->err, "Truncated event message received.", sizeof(s->err));   /* the SDK's chunker (getChunkedStream.js) */
        return -1;
    }
    return 0;
}

static void put32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

long llm_evs_encode(const char *const *hdr, int nh, const void *payload, long n, unsigned char *out, long cap)
{
    long hl = 0, total, o = 12;
    int i;
    for (i = 0; i < nh; i++)
        hl += 1 + (long)strlen(hdr[2 * i]) + 1 + 2 + (long)strlen(hdr[2 * i + 1]);
    total = 16 + hl + n;
    if (total > cap || total > LLM_EVS_MAX)
        return -1;
    put32(out, (unsigned long)total);
    put32(out + 4, (unsigned long)hl);
    put32(out + 8, llm_crc32(0, out, 8));
    for (i = 0; i < nh; i++) {
        long nl = (long)strlen(hdr[2 * i]), vl = (long)strlen(hdr[2 * i + 1]);
        out[o++] = (unsigned char)nl;
        memcpy(out + o, hdr[2 * i], (size_t)nl);
        o += nl;
        out[o++] = 7;
        out[o++] = (unsigned char)(vl >> 8);
        out[o++] = (unsigned char)vl;
        memcpy(out + o, hdr[2 * i + 1], (size_t)vl);
        o += vl;
    }
    if (n > 0)
        memcpy(out + o, payload, (size_t)n);
    o += n;
    put32(out + o, llm_crc32(0, out, o));
    return total;
}
