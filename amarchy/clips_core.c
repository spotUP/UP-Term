/* amarchy/clips_core: the clipboard history ring, see clips_core.h. Portable C89. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "clips_core.h"

void clips_init(clip_ring *r)
{
    memset(r, 0, sizeof *r);
}

void clips_free(clip_ring *r)
{
    int i;
    for (i = 0; i < r->n; i++)
        free(r->text[i]);
    clips_init(r);
}

/* The length of a UTF-8 sequence from its lead byte; 1 for a stray or continuation byte, so a bad
 * byte is kept as one unit and never swallows its neighbours. */
static int seq_len(unsigned char c)
{
    if (c >= 0xF0 && c < 0xF8)
        return 4;
    if (c >= 0xE0)
        return c < 0xF0 ? 3 : 1;
    if (c >= 0xC2 && c < 0xE0)
        return 2;
    return 1;
}

/* Does a whole sequence of `len` bytes start at s with `avail` bytes left? A lead byte promising more
 * than is there (or followed by a non-continuation byte) is not a sequence: callers treat the lead as
 * a single byte. */
static int seq_ok(const unsigned char *s, int len, long avail)
{
    int i;
    if (len > avail)
        return 0;
    for (i = 1; i < len; i++)
        if ((s[i] & 0xC0) != 0x80)
            return 0;
    return 1;
}

int clips_prepend(clip_ring *r, const char *text, long len)
{
    char *copy;
    int i, at;
    long n = 0;
    while (n < len && text[n])          /* an entry ends at its first NUL */
        n++;
    if (n > CLIPS_TEXT) {
        n = CLIPS_TEXT;
        while (n > 0 && ((unsigned char)text[n] & 0xC0) == 0x80)   /* do not cut inside a sequence */
            n--;
    }
    if (n <= 0)
        return 0;
    for (at = 0; at < r->n; at++)
        if (r->len[at] == n && !memcmp(r->text[at], text, (size_t)n))
            break;
    if (at == 0 && r->n > 0)
        return 0;                       /* already the newest */
    if (at < r->n) {                    /* a repeat: the held copy moves to the front */
        copy = r->text[at];
    } else {
        copy = (char *)malloc((size_t)n + 1);
        if (!copy)
            return 0;
        memcpy(copy, text, (size_t)n);
        copy[n] = 0;
        if (r->n < CLIPS_MAX) {
            r->n++;
        } else {                        /* a full ring drops the oldest */
            free(r->text[CLIPS_MAX - 1]);
        }
        at = r->n - 1;
    }
    for (i = at; i > 0; i--) {
        r->text[i] = r->text[i - 1];
        r->len[i] = r->len[i - 1];
    }
    r->text[0] = copy;
    r->len[0] = n;
    return 1;
}

void clips_label(const char *text, long len, char *out, int n)
{
    const unsigned char *s = (const unsigned char *)text;
    long at = 0;
    int o = 0, chars = 0;
    if (n <= 0)
        return;
    while (at < len && s[at] && chars < CLIPS_LABEL) {
        int l = seq_len(s[at]);
        if (l > 1 && !seq_ok(s + at, l, len - at))
            l = 1;
        if (o + l >= n)
            break;
        if (l == 1) {
            out[o++] = (char)((s[at] < 0x20 || s[at] == 0x7F) ? ' ' : s[at]);
        } else {
            memcpy(out + o, s + at, (size_t)l);
            o += l;
        }
        at += l;
        chars++;
    }
    out[o] = 0;
}

char *clips_serialize(const clip_ring *r, long *len)
{
    long total = 0, o = 0;
    char *buf;
    int i;
    for (i = 0; i < r->n; i++)
        total += 16 + r->len[i] + 1;    /* digits and LF, bytes, LF */
    buf = (char *)malloc((size_t)total + 1);
    if (!buf)
        return 0;
    for (i = 0; i < r->n; i++) {
        o += snprintf(buf + o, 16, "%ld\n", r->len[i]);
        memcpy(buf + o, r->text[i], (size_t)r->len[i]);
        o += r->len[i];
        buf[o++] = '\n';
    }
    *len = o;
    return buf;
}

int clips_parse(clip_ring *r, const char *text, long len)
{
    long at = 0;
    clips_free(r);
    while (r->n < CLIPS_MAX && at < len) {
        long l = 0;
        int digits = 0;
        char *copy;
        while (at < len && text[at] >= '0' && text[at] <= '9' && digits < 6) {
            l = l * 10 + (text[at++] - '0');
            digits++;
        }
        if (!digits || at >= len || text[at] != '\n' || l < 1 || l > CLIPS_TEXT || at + 1 + l + 1 > len ||
            text[at + 1 + l] != '\n')
            break;
        copy = (char *)malloc((size_t)l + 1);
        if (!copy)
            break;
        memcpy(copy, text + at + 1, (size_t)l);
        copy[l] = 0;
        r->text[r->n] = copy;
        r->len[r->n] = l;
        r->n++;
        at += 1 + l + 1;
    }
    return r->n;
}

int clips_load_file(clip_ring *r, const char *path)
{
    FILE *f = fopen(path, "rb");
    char *buf = 0;
    long size = 0, got = 0;
    clips_free(r);
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size > 0 && size < 1000000L)
        buf = (char *)malloc((size_t)size);
    if (buf)
        got = (long)fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (got > 0)
        clips_parse(r, buf, got);
    free(buf);
    return r->n;
}

int clips_save_file(const clip_ring *r, const char *path)
{
    long len = 0;
    char *buf = clips_serialize(r, &len);
    FILE *f;
    int ok;
    if (!buf)
        return 0;
    f = fopen(path, "wb");
    if (!f) {
        free(buf);
        return 0;
    }
    ok = fwrite(buf, 1, (size_t)len, f) == (size_t)len;
    ok = fclose(f) == 0 && ok;
    free(buf);
    return ok;
}
