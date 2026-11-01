/* kittyout.c -- see kittyout.h. Portable C89. */
#include <string.h>
#include "kittyout.h"

#define RAW_CHUNK 3072L /* 3072 bytes are 4096 base64 characters */

static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void out_num(char *b, int *l, long v)
{
    char t[12];
    int n = 0;
    if (v <= 0) {
        b[(*l)++] = '0';
        return;
    }
    while (v > 0) {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    }
    while (n > 0)
        b[(*l)++] = t[--n];
}

static void key(char *b, int *l, char k, long v)
{
    b[(*l)++] = ',';
    b[(*l)++] = k;
    b[(*l)++] = '=';
    out_num(b, l, v);
}

/* the data in chunks; keys is the first chunk's key list ("a=T,f=24,..."), without a trailing m= */
static void send_chunks(kout_put put, void *u, const char *keys, const unsigned char *d, long n)
{
    char head[96], enc[4096 + 4];
    long off = 0;
    int first = 1;
    if (n == 0)
        n = 0;
    do {
        long take = n - off > RAW_CHUNK ? RAW_CHUNK : n - off;
        int more = off + take < n, hl = 0, el = 0;
        long i;
        head[hl++] = '\033';
        head[hl++] = '_';
        head[hl++] = 'G';
        if (first) {
            long kl = (long)strlen(keys);
            memcpy(head + hl, keys, (size_t)kl);
            hl += (int)kl;
            head[hl++] = ',';
        }
        head[hl++] = 'm';
        head[hl++] = '=';
        head[hl++] = more ? '1' : '0';
        head[hl++] = ';';
        put(u, head, hl);
        for (i = 0; i < take; i += 3) {
            unsigned long v = (unsigned long)d[off + i] << 16;
            int have = take - i >= 3 ? 3 : (int)(take - i);
            if (have > 1)
                v |= (unsigned long)d[off + i + 1] << 8;
            if (have > 2)
                v |= d[off + i + 2];
            enc[el++] = b64[(v >> 18) & 63];
            enc[el++] = b64[(v >> 12) & 63];
            enc[el++] = have > 1 ? b64[(v >> 6) & 63] : '=';
            enc[el++] = have > 2 ? b64[v & 63] : '=';
        }
        put(u, enc, el);
        put(u, "\033\\", 2);
        off += take;
        first = 0;
    } while (off < n);
}

static void common(char *k, int *l, const kout_opts *o)
{
    if (o && o->cols > 0)
        key(k, l, 'c', o->cols);
    if (o && o->rows > 0)
        key(k, l, 'r', o->rows);
    if (o && o->quiet > 0)
        key(k, l, 'q', o->quiet);
}

int kout_pixels(kout_put put, void *u, const unsigned char *px, int w, int h, int bpp, const kout_opts *o)
{
    char k[96];
    int l = 0;
    if (w <= 0 || h <= 0 || (bpp != 3 && bpp != 4))
        return 0;
    memcpy(k, "a=T,f=", 6);
    l = 6;
    out_num(k, &l, bpp == 3 ? 24 : 32);
    key(k, &l, 's', w);
    key(k, &l, 'v', h);
    common(k, &l, o);
    k[l] = 0;
    send_chunks(put, u, k, px, (long)w * h * bpp);
    return 1;
}

int kout_png(kout_put put, void *u, const unsigned char *png, long n, const kout_opts *o)
{
    char k[96];
    int l = 0;
    if (n <= 0)
        return 0;
    memcpy(k, "a=T,f=100", 9);
    l = 9;
    common(k, &l, o);
    k[l] = 0;
    send_chunks(put, u, k, png, n);
    return 1;
}

void kout_query(kout_put put, void *u)
{
    static const char q[] = "\033_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\033\\";
    put(u, q, (long)sizeof(q) - 1);
}

int kout_query_ok(const char *buf, long n)
{
    static const char ok[] = "\033_Gi=31;OK";
    long i, m = (long)sizeof(ok) - 1;
    for (i = 0; i + m <= n; i++)
        if (!memcmp(buf + i, ok, (size_t)m))
            return 1;
    return 0;
}

void kout_scale(const unsigned char *src, int w, int h, int bpp, unsigned char *dst, int dw, int dh)
{
    int x, y, b;
    for (y = 0; y < dh; y++) {
        long sy = (long)y * h / dh;
        for (x = 0; x < dw; x++) {
            long sx = (long)x * w / dw;
            for (b = 0; b < bpp; b++)
                dst[((long)y * dw + x) * bpp + b] = src[(sy * w + sx) * bpp + b];
        }
    }
}

void kout_fit(int w, int h, long max_px, int max_w, int *dw, int *dh)
{
    long tw = w, th = h;
    if (max_w > 0 && tw > max_w) {
        th = th * max_w / tw;
        tw = max_w;
    }
    while (tw * th > max_px && tw > 1 && th > 1) {
        tw = tw * 9 / 10;
        th = th * 9 / 10;
    }
    *dw = (int)(tw < 1 ? 1 : tw);
    *dh = (int)(th < 1 ? 1 : th);
}
