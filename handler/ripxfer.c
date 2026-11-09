/* ripxfer.c -- see ripxfer.h. Reuses zm/zmodem.c for the protocol and engine/dlname.h for
 * the name; the only new code is the in-memory file glue and the 8.3 reduction. */
#include <stdlib.h>
#include <string.h>
#include "ripxfer.h"
#include "../engine/dlname.h"

static int okc(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           (c != 0 && strchr("_-!#$%&'()@^~", c) != 0);
}

int ripxfer_name83(const char *raw, int rn, char *out)
{
    char base[64];
    int dot = -1, i, o = 0, e;
    out[0] = 0;
    if (!dl_clean_name(raw, rn, base, (int)sizeof base, 0))
        return 0;
    for (i = 0; base[i]; i++)
        if (base[i] == '.')
            dot = i;
    for (i = 0; base[i] && (dot < 0 || i < dot) && o < 8; i++)
        out[o++] = okc((unsigned char)base[i]) ? base[i] : '_';
    if (dot >= 0 && base[dot + 1]) {
        out[o++] = '.';
        for (e = 0, i = dot + 1; base[i] && e < 3; i++, e++)
            out[o++] = okc((unsigned char)base[i]) ? base[i] : '_';
    }
    out[o] = 0;
    if (!o || out[0] == '.') { /* ".profile": no stem */
        out[0] = 0;
        return 0;
    }
    return 1;
}

typedef struct rx {
    vt_dl_sink sink;
    const char *host;
    const char *rip_name;
    int *delivered;
    unsigned char *buf;
    long n, cap;
    char name[RIPXFER_NAME];
} rx;

static void *rx_open(void *u, const char *zname, long size, zm_u32 mtime)
{
    rx *r = (rx *)u;
    const char *src = r->rip_name && r->rip_name[0] ? r->rip_name : zname;
    (void)mtime;
    if (size > RIPXFER_MAX || r->buf || !ripxfer_name83(src, (int)strlen(src), r->name))
        return 0; /* skipped: too big, or no name worth saving */
    r->n = 0;
    r->cap = size > 0 ? size : 4096;
    r->buf = (unsigned char *)malloc((size_t)r->cap);
    return r->buf ? (void *)r : 0;
}

static long rx_write(void *u, void *f, const unsigned char *b, long n)
{
    rx *r = (rx *)u;
    (void)f;
    if (r->n + n > RIPXFER_MAX)
        return -1;
    if (r->n + n > r->cap) {
        long nc = r->cap * 2;
        unsigned char *nb;
        if (nc < r->n + n)
            nc = r->n + n;
        nb = (unsigned char *)realloc(r->buf, (size_t)nc);
        if (!nb)
            return -1;
        r->buf = nb;
        r->cap = nc;
    }
    memcpy(r->buf + r->n, b, (size_t)n);
    r->n += n;
    return n;
}

static void rx_close(void *u, void *f, int complete)
{
    rx *r = (rx *)u;
    (void)f;
    if (complete && r->sink && r->sink(r->host, r->name, r->buf, r->n))
        (*r->delivered)++;
    free(r->buf);
    r->buf = 0;
}

int ripxfer_receive(const zm_line *line, vt_dl_sink sink, const char *host, const char *rip_name,
                    int *delivered)
{
    rx r;
    zm_files fl;
    int got = 0, rc;
    char h[64];
    memset(&r, 0, sizeof r);
    if (!dl_clean_name(host ? host : "", host ? (int)strlen(host) : 0, h, (int)sizeof h, "local"))
        strcpy(h, "local");
    r.sink = sink;
    r.host = h;
    r.rip_name = rip_name;
    r.delivered = delivered;
    *delivered = 0;
    memset(&fl, 0, sizeof fl);
    fl.open_write = rx_open;
    fl.write = rx_write;
    fl.close = rx_close;
    fl.user = &r;
    rc = zm_receive(line, &fl, &got);
    free(r.buf);
    return rc;
}
