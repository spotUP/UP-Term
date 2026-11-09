/* iTerm2 inline images: OSC 1337 ; File = [k=v;k=v...] : <base64>.
 * This half is the pure part: the argument list and the unit arithmetic, no
 * terminal in it (tests/test_iterm2.c includes this file alone). The glue that
 * decodes and places the picture is in kitty.c, on the kitty path. */

#define ITERM_AUTO 0 /* width / height units */
#define ITERM_CELLS 1
#define ITERM_PIXELS 2
#define ITERM_PERCENT 3

typedef struct iterm_args {
    int inl;             /* inline=1: show it; anything else is a download, which is ignored */
    int keep;            /* preserveAspectRatio, default 1 */
    int nomove;          /* doNotMoveCursor=1 */
    long size;           /* size=, bytes (only a hint), -1 when absent */
    int wu, hu;          /* ITERM_* of width= and height= */
    long wv, hv;         /* their numbers */
} iterm_args;

/* One size value: "auto", "N" (cells), "Npx", "N%". */
static void iterm_dim(const char *s, int n, int *unit, long *val)
{
    long v = 0;
    int i = 0;
    *unit = ITERM_AUTO;
    *val = 0;
    while (i < n && s[i] >= '0' && s[i] <= '9') {
        if (v < 100000000L)
            v = v * 10 + (s[i] - '0');
        i++;
    }
    if (!i)
        return; /* auto, or not a number: auto */
    *val = v;
    if (i == n)
        *unit = v ? ITERM_CELLS : ITERM_AUTO;
    else if (n - i == 2 && s[i] == 'p' && s[i + 1] == 'x')
        *unit = v ? ITERM_PIXELS : ITERM_AUTO;
    else if (n - i == 1 && s[i] == '%')
        *unit = v ? ITERM_PERCENT : ITERM_AUTO;
}

static int iterm_is(const char *k, int kn, const char *name)
{
    int i = 0;
    while (i < kn && name[i] && k[i] == name[i])
        i++;
    return i == kn && !name[i];
}

/* The text between "File=" and ':' . Unknown keys (name=, ...) are skipped. */
static void iterm_parse(const char *s, int n, iterm_args *a)
{
    const char *e = s + n;
    a->inl = 0;
    a->keep = 1;
    a->nomove = 0;
    a->size = -1;
    a->wu = a->hu = ITERM_AUTO;
    a->wv = a->hv = 0;
    while (s < e) {
        const char *k = s, *v;
        int kn, vn;
        while (s < e && *s != '=' && *s != ';')
            s++;
        kn = (int)(s - k);
        if (s < e && *s == '=')
            s++;
        v = s;
        while (s < e && *s != ';')
            s++;
        vn = (int)(s - v);
        if (s < e)
            s++;
        if (iterm_is(k, kn, "inline"))
            a->inl = vn == 1 && v[0] == '1';
        else if (iterm_is(k, kn, "preserveAspectRatio"))
            a->keep = !(vn == 1 && v[0] == '0');
        else if (iterm_is(k, kn, "doNotMoveCursor"))
            a->nomove = vn == 1 && v[0] == '1';
        else if (iterm_is(k, kn, "width"))
            iterm_dim(v, vn, &a->wu, &a->wv);
        else if (iterm_is(k, kn, "height"))
            iterm_dim(v, vn, &a->hu, &a->hv);
        else if (iterm_is(k, kn, "size")) {
            long z = 0;
            int i;
            for (i = 0; i < vn && v[i] >= '0' && v[i] <= '9'; i++)
                if (z < 100000000L)
                    z = z * 10 + (v[i] - '0');
            a->size = z;
        }
    }
}

/* A width or height in cells: 0 for auto. `cellpx` is the cell's pixels on
 * that axis, `total` the screen's cells on it (the base of a percent). */
static int iterm_cells(int unit, long val, int cellpx, int total)
{
    long c = 0;
    if (unit == ITERM_CELLS)
        c = val;
    else if (unit == ITERM_PIXELS)
        c = cellpx > 0 ? (val + cellpx - 1) / cellpx : val;
    else if (unit == ITERM_PERCENT)
        c = (long)total * val / 100;
    if (unit != ITERM_AUTO && c < 1)
        c = 1;
    if (c > total)
        c = total;
    return (int)c;
}

/* ---- inline=0 downloads: the file name and the path it is saved at -------- */

#include "dlname.h"

/* The name= value (base64) as a safe file name: base name only (after the last
 * '/', '\\' or ':'), no control characters, never "." / ".." / empty ("download"). */
static void iterm_name(const char *b64, int n, char *out, int max)
{
    static const char tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char raw[80];
    int rn = 0, i;
    unsigned long acc = 0;
    int bits = 0;
    for (i = 0; i < n && rn < (int)sizeof(raw) - 1; i++) {
        const char *p = b64[i] == '=' ? 0 : strchr(tab, b64[i]);
        if (!p || !b64[i])
            continue;
        acc = (acc << 6) | (unsigned long)(p - tab);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            raw[rn++] = (char)((acc >> bits) & 0xFF);
        }
    }
    dl_clean_name(raw, rn, out, max, "download");
}

/* <host>/<name>, relative: where it lands is the host's business (D22). */
static void iterm_path(char *out, int max, const char *host, const char *name)
{
    const char *part[2];
    int p, o = 0;
    part[0] = host && *host ? host : "local";
    part[1] = name;
    for (p = 0; p < 2; p++) {
        const char *c = part[p];
        if (p && o < max - 1)
            out[o++] = '/';
        while (*c && o < max - 1)
            out[o++] = *c++;
    }
    out[o] = 0;
}
