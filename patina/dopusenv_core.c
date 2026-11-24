/* dopusenv_core -- see dopusenv_core.h. */
#include <string.h>
#include "dopusenv_core.h"

static uint32_t rd32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

/* Walks every chunk (so a truncated one anywhere is DE_BAD). *off = offset of the first chunk id's header, *size = its body size. */
static int find_chunk(const uint8_t *buf, size_t len, const char *id, size_t *off, uint32_t *size)
{
    size_t i = 12, end;
    int hit = 0;
    if (!buf || len < 12 || memcmp(buf, "FORM", 4) != 0 || memcmp(buf + 8, "EPUS", 4) != 0) return DE_BAD;
    end = (size_t)rd32(buf + 4) + 8;
    if (end > len || end < 12) return DE_BAD;
    while (i < end) {
        uint32_t sz;
        if (end - i < 8) return DE_BAD;
        sz = rd32(buf + i + 4);
        if (sz > end - i - 8) return DE_BAD;
        if (!hit && memcmp(buf + i, id, 4) == 0) { *off = i; *size = sz; hit = 1; }
        i += 8 + (size_t)sz;
        if (sz & 1) { if (i < end) i++; else if (i > end) return DE_BAD; }  /* the pad may be missing on the last chunk only */
    }
    return hit ? DE_OK : DE_NOTBAR;
}

static int find_tbar(const uint8_t *buf, size_t len, size_t *off, uint32_t *size)
{
    return find_chunk(buf, len, "TBAR", off, size);
}

int de_userpens_zero(uint8_t *buf, size_t len)
{
    size_t off;
    uint32_t sz;
    uint8_t *p;
    int r = find_chunk(buf, len, "ENVR", &off, &sz);
    if (r != DE_OK) return r == DE_NOTBAR ? DE_NOENVR : r;
    if (sz < DE_ENVR_USERPENS + 2) return DE_BAD;
    p = buf + off + 8 + DE_ENVR_USERPENS;
    if (p[0] == 0 && p[1] == 0) return 0;
    p[0] = 0;
    p[1] = 0;
    return 1;
}

int de_tbar_get(const uint8_t *buf, size_t len, char *out, size_t cap)
{
    size_t off;
    uint32_t sz;
    int r = find_tbar(buf, len, &off, &sz);
    if (r != DE_OK) return r;
    if (sz == 0 || buf[off + 8 + sz - 1] != 0 || sz > cap || memchr(buf + off + 8, 0, sz) != buf + off + 8 + sz - 1) return DE_FULL;
    memcpy(out, buf + off + 8, sz);
    return DE_OK;
}

long de_tbar_set(const uint8_t *buf, size_t len, const char *path, uint8_t *out, size_t outcap)
{
    size_t off, n, tail, newlen;
    uint32_t sz, nsz;
    int r = find_tbar(buf, len, &off, &sz);
    if (r != DE_OK) return r;
    n = strlen(path);
    if (n == 0 || n >= DE_PATHMAX) return DE_FULL;
    nsz = (uint32_t)n + 1;
    tail = off + 8 + (size_t)sz + (sz & 1);
    if (tail > len) tail = len;                       /* a missing last pad */
    newlen = off + 8 + nsz + (nsz & 1) + (len - tail);
    if (newlen > outcap) return DE_FULL;
    memcpy(out, buf, off + 4);                        /* head and the TBAR id */
    wr32(out + off + 4, nsz);
    memcpy(out + off + 8, path, nsz);
    if (nsz & 1) out[off + 8 + nsz] = 0;
    memcpy(out + off + 8 + nsz + (nsz & 1), buf + tail, len - tail);
    wr32(out + 4, rd32(buf + 4) + (uint32_t)newlen - (uint32_t)len);  /* delta; bytes after the FORM stay as they were */
    return (long)newlen;
}

static int ieq_prefix(const char *s, const char *pre)
{
    for (; *pre; s++, pre++) {
        char a = *s, b = *pre;
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return 0;
    }
    return 1;
}

int de_is_ours(const char *cur) { return cur && (ieq_prefix(cur, DE_BANK_A) || ieq_prefix(cur, DE_BANK_B) || ieq_prefix(cur, DE_BANK_C)); }
int de_may_replace(const char *cur) { return cur && (de_is_ours(cur) || (ieq_prefix(cur, DE_STOCK) && cur[sizeof(DE_STOCK) - 1] == 0)); }
int de_may_restore(const char *cur) { return de_is_ours(cur); }

/* FLATTEN SOURCE: the user's own bank to flatten from, or NULL when the stock flow applies. A custom path (not stock, not ours) is its own
 * source. Our flattened copy (DE_BANK_C) goes back to the saved original, never to the copy; a saved path that is ours is never trusted. */
const char *de_orig_pick(const char *cur, const char *saved)
{
    if (!cur || !cur[0]) return NULL;
    if (ieq_prefix(cur, DE_BANK_A) || ieq_prefix(cur, DE_BANK_B)) return NULL;   /* the stock flat banks (DE_BANK_C no longer overlaps them) */
    if (ieq_prefix(cur, DE_BANK_C)) return (saved && saved[0] && !de_is_ours(saved)) ? saved : NULL;
    if (de_is_ours(cur) || (ieq_prefix(cur, DE_STOCK) && cur[sizeof(DE_STOCK) - 1] == 0)) return NULL;
    return cur;
}
