/* icon_core: see icon_core.h. */
#include <stdlib.h>
#include <string.h>

#include "icon_core.h"

int ic_init(ic_cache *c, int slots)
{
    if (slots < IC_SLOTS)
        slots = IC_SLOTS;
    memset(c, 0, sizeof *c);
    c->path = calloc((size_t)slots, sizeof *c->path);
    c->state = calloc((size_t)slots, sizeof *c->state);
    c->used = calloc((size_t)slots, sizeof *c->used);
    if (!c->path || !c->state || !c->used) {
        ic_free(c);
        return 0;
    }
    c->slots = slots;
    return 1;
}

void ic_free(ic_cache *c)
{
    free(c->path);
    free(c->state);
    free(c->used);
    memset(c, 0, sizeof *c);
}

int ic_find(ic_cache *c, const char *path)
{
    int i;
    for (i = 0; i < c->slots; i++)
        if (c->state[i] != IC_EMPTY && !strcmp(c->path[i], path)) {
            c->used[i] = ++c->tick;
            return i;
        }
    return -1;
}

int ic_claim(ic_cache *c, const char *path)
{
    int i, s = 0;
    if (strlen(path) >= IX_PATH)
        return -1;
    for (i = 0; i < c->slots; i++) {
        if (c->state[i] == IC_EMPTY) {
            s = i;
            break;
        }
        if (c->used[i] < c->used[s])
            s = i;
    }
    strcpy(c->path[s], path);
    c->state[s] = IC_EMPTY;
    c->used[s] = ++c->tick;
    return s;
}

void ic_set(ic_cache *c, int slot, int state)
{
    if (slot >= 0 && slot < c->slots)
        c->state[slot] = state;
}

int ic_fit(int sw, int sh, int bw, int bh, int *dw, int *dh, int *dx, int *dy)
{
    if (sw < 1 || sh < 1 || bw < 1 || bh < 1)
        return 0;
    /* the larger scale down wins: dw / sw and dh / sh are both at most bw / sw and bh / sh */
    if ((long)bw * sh <= (long)bh * sw) {
        *dw = bw;
        *dh = (int)((long)sh * bw / sw);
    } else {
        *dh = bh;
        *dw = (int)((long)sw * bh / sh);
    }
    if (*dw < 1)
        *dw = 1;
    if (*dh < 1)
        *dh = 1;
    *dx = (bw - *dw) / 2;
    *dy = (bh - *dh) / 2;
    return 1;
}

static void put16(unsigned char *p, int v)
{
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

static int get16(const unsigned char *p)
{
    return (p[0] << 8) | p[1];
}

long ic_file_head(unsigned char *buf, long cap, int bw, int bh, int depth)
{
    if (cap < IC_FILE_HEAD || bw < 1 || bw > 0xFFFF || bh < 1 || bh > 0xFFFF || depth < 1 || depth > 8)
        return -1;
    memcpy(buf, "AMIC", 4);
    buf[4] = IC_FILE_VERSION;
    put16(buf + 5, bw);
    put16(buf + 7, bh);
    buf[9] = (unsigned char)depth;
    return IC_FILE_HEAD;
}

long ic_file_entry(unsigned char *buf, long cap, const char *path, int dw, int dh, int dx, int dy,
                   const unsigned char *pens, const unsigned char *mask)
{
    long pl = (long)strlen(path), px, need;
    if (pl < 1 || pl >= IX_PATH || dw < 1 || dh < 1 || dw > IC_FILE_MAXDIM || dh > IC_FILE_MAXDIM ||
        dx < 0 || dy < 0 || dx > 0xFFFF || dy > 0xFFFF)
        return -1;
    px = (long)dw * dh;
    need = pl + 1 + 8 + 2 * px;
    if (px > IC_FILE_MAXPIX || need > cap)
        return -1;
    memcpy(buf, path, (size_t)pl + 1);
    buf += pl + 1;
    put16(buf, dw);
    put16(buf + 2, dh);
    put16(buf + 4, dx);
    put16(buf + 6, dy);
    memcpy(buf + 8, pens, (size_t)px);
    memcpy(buf + 8 + px, mask, (size_t)px);
    return need;
}

int ic_file_decode(const unsigned char *buf, long len, int bw, int bh, int depth, ic_dec *out, int max)
{
    long off = IC_FILE_HEAD, pl, px;
    int n = 0, dw, dh;
    const unsigned char *e;
    if (len < IC_FILE_HEAD || memcmp(buf, "AMIC", 4) || buf[4] != IC_FILE_VERSION || get16(buf + 5) != bw ||
        get16(buf + 7) != bh || buf[9] != depth)
        return 0;
    while (off < len && n < max) {
        for (pl = 0; off + pl < len && buf[off + pl]; pl++)
            if (pl >= IX_PATH)
                return 0;
        if (off + pl >= len || pl < 1)
            return 0;
        e = buf + off + pl + 1;
        if (len - (off + pl + 1) < 8)
            return 0;
        dw = get16(e);
        dh = get16(e + 2);
        if (dw < 1 || dh < 1 || dw > IC_FILE_MAXDIM || dh > IC_FILE_MAXDIM)
            return 0;
        px = (long)dw * dh;
        if (px > IC_FILE_MAXPIX || len - (off + pl + 1) < 8 + 2 * px)
            return 0;
        out[n].path = (const char *)buf + off;
        out[n].dw = dw;
        out[n].dh = dh;
        out[n].dx = get16(e + 4);
        out[n].dy = get16(e + 6);
        out[n].pens = e + 8;
        out[n].mask = e + 8 + px;
        n++;
        off += pl + 1 + 8 + 2 * px;
    }
    return n;
}
