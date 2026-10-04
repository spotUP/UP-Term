/* upconf: see upconf.h for the file format and the contract. */
#include <string.h>
#include "upconf.h"

/* 1 when s starts with "profile", case-insensitively. */
static int uc_prefix_profile(const char *s)
{
    static const char want[] = "profile";
    int i;
    for (i = 0; want[i]; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        if (c != want[i])
            return 0;
    }
    return 1;
}

/* Case-insensitive compare of two NUL-terminated strings. */
static int uc_ieq(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb - 'A' + 'a');
        if (ca != cb)
            return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

/* Copy at most n-1 bytes, always NUL-terminating. 1 when src was longer than
 * n-1 (truncated), 0 otherwise. */
static int uc_copy(char *dst, const char *src, int n)
{
    int i;
    for (i = 0; i < n - 1 && src[i]; i++)
        dst[i] = src[i];
    dst[i] = 0;
    return src[i] ? 1 : 0;
}

static int prof_find(const upconf *c, const char *name)
{
    int i;
    for (i = 0; i < c->nprof; i++)
        if (uc_ieq(c->prof[i], name))
            return i;
    return -1;
}

static int key_find(const upconf *c, int p, const char *name)
{
    int i;
    for (i = 0; i < c->n[p]; i++)
        if (uc_ieq(c->key[p][i], name))
            return i;
    return -1;
}

/* A new profile at the end of the table: its index, or -1 when the table
 * is full (overflow marked). */
static int prof_add(upconf *c, const char *name)
{
    int p;
    if (c->nprof >= UC_MAX_PROFILES) {
        c->overflow = 1;
        return -1;
    }
    p = c->nprof++;
    if (uc_copy(c->prof[p], name, UC_NAME))
        c->overflow = 1;
    c->n[p] = 0;
    return p;
}

/* Keep a comment line, len bytes of s as typed (a trailing CR dropped),
 * before key k of profile p (-1: before every section). 0 when there is no
 * room for it (overflow marked: a save would lose it). */
static int note_add(upconf *c, int p, int k, const char *s, long len)
{
    while (len > 0 && s[len - 1] == '\r')
        len--;
    if (c->nnote >= UC_MAX_NOTES || c->notelen + len + 1 > UC_NOTE_BYTES) {
        c->overflow = 1;
        return 0;
    }
    c->note_at[c->nnote] = (short)c->notelen;
    c->note_prof[c->nnote] = (signed char)p;
    c->note_key[c->nnote] = (unsigned char)k;
    memcpy(c->note + c->notelen, s, (size_t)len);
    c->note[c->notelen + len] = 0;
    c->notelen += (int)len + 1;
    c->nnote++;
    return 1;
}

/* Forget comment line i; the later ones keep their order. */
static void note_drop(upconf *c, int i)
{
    int at = c->note_at[i], n = (int)strlen(c->note + at) + 1, j;
    memmove(c->note + at, c->note + at + n, (size_t)(c->notelen - at - n));
    c->notelen -= n;
    for (j = i; j < c->nnote - 1; j++) {
        c->note_at[j] = (short)(c->note_at[j + 1] - n);
        c->note_prof[j] = c->note_prof[j + 1];
        c->note_key[j] = c->note_key[j + 1];
    }
    c->nnote--;
}

/* Copy s through *o while it fits in [o, end); *o advances past it. 0 when
 * it does not fit. */
static int uc_copyto(char **o, char *end, const char *s)
{
    while (*s) {
        if (*o >= end)
            return 0;
        *(*o)++ = *s++;
    }
    return 1;
}

int upconf_set(upconf *c, const char *profile, const char *key, const char *value)
{
    int p, k;
    if (!profile || !*profile || !key || !*key)
        return 0;
    p = prof_find(c, profile);
    if (p < 0 && (p = prof_add(c, profile)) < 0)
        return 0;
    k = key_find(c, p, key);
    if (k < 0) {
        if (c->n[p] >= UC_MAX_KEYS) {
            c->overflow = 1;
            return 0;
        }
        k = c->n[p]++;
        if (uc_copy(c->key[p][k], key, UC_NAME))
            c->overflow = 1;
    }
    if (uc_copy(c->val[p][k], value ? value : "", UC_MAX_VALUE))
        c->overflow = 1;
    return 1;
}

int upconf_del(upconf *c, const char *profile, const char *key)
{
    int p, k, i;
    if (!key || !*key)
        return 0;
    p = prof_find(c, profile);
    if (p < 0)
        return 0;
    k = key_find(c, p, key);
    if (k < 0)
        return 0;
    /* The later keys slide down over it: a save writes them where they were,
     * and the comments between them stay put (the last key once moved into
     * the gap, away from the comment above it). */
    for (i = k; i < c->n[p] - 1; i++) {
        memcpy(c->key[p][i], c->key[p][i + 1], sizeof(c->key[0][0]));
        memcpy(c->val[p][i], c->val[p][i + 1], sizeof(c->val[0][0]));
    }
    c->n[p]--;
    for (i = 0; i < c->nnote; i++)
        if (c->note_prof[i] == p && c->note_key[i] > k)
            c->note_key[i]--;
    return 1;
}

int upconf_rmprof(upconf *c, const char *profile)
{
    int p, i;
    p = prof_find(c, profile);
    if (p < 0)
        return 0;
    /* Slide the later profiles down over it; their order is kept. */
    for (i = p; i < c->nprof - 1; i++) {
        int j = i + 1;
        memcpy(c->prof[i], c->prof[j], sizeof(c->prof[0]));
        memcpy(c->key[i], c->key[j], sizeof(c->key[0]));
        memcpy(c->val[i], c->val[j], sizeof(c->val[0]));
        c->n[i] = c->n[j];
    }
    c->nprof--;
    /* its comments go with it; the later profiles' follow them down */
    for (i = c->nnote - 1; i >= 0; i--)
        if (c->note_prof[i] == p)
            note_drop(c, i);
    for (i = 0; i < c->nnote; i++)
        if (c->note_prof[i] > p)
            c->note_prof[i]--;
    return 1;
}

void upconf_clear(upconf *c)
{
    memset(c, 0, sizeof(*c));
}

int upconf_parse(upconf *c, const char *buf, long len)
{
    char line[256];
    char curprof[UC_NAME];
    long i = 0;
    int insec = 0; /* a [profile] line was read */
    upconf_clear(c);
    if (!buf || len <= 0)
        return 0;
    uc_copy(curprof, "default", UC_NAME);
    while (i < len) {
        long n = 0, e, f;
        /* a comment is kept as typed, however long, from the buffer itself */
        for (e = i; e < len && buf[e] != '\n'; e++)
            ;
        for (f = i; f < e && (buf[f] == ' ' || buf[f] == '\t'); f++)
            ;
        if (f < e && (buf[f] == ';' || buf[f] == '#')) {
            int p = prof_find(c, curprof);
            if (p < 0 && insec)
                p = prof_add(c, curprof); /* a section of comments only is kept */
            if (p >= 0 || !insec)
                note_add(c, p, p < 0 ? 0 : c->n[p], buf + i, e - i);
            i = e < len ? e + 1 : e;
            continue;
        }
        while (i < len && buf[i] != '\n' && n < (long)sizeof(line) - 1)
            line[n++] = buf[i++];
        line[n] = 0;
        if (i < len && buf[i] != '\n') {
            /* longer than a line can be: the whole line is dropped -- its
             * tail parsed as a line of its own let a long value smuggle in
             * a key (2026-10-02 review). A comment never gets here (kept
             * above, whole); a blank one loses nothing. */
            const char *b = line;
            while (i < len && buf[i] != '\n')
                i++;
            while (*b == ' ' || *b == '\t')
                b++;
            if (*b)
                c->overflow = 1; /* a setting was lost */
            if (i < len)
                i++;
            continue;
        }
        if (i < len)
            i++; /* the newline */
        {
            char *s = line;
            char *eq;
            while (*s == ' ' || *s == '\t')
                s++;
            if (!*s || *s == ';' || *s == '#')
                continue;
            while (n > 0 && (line[n - 1] == ' ' || line[n - 1] == '\t' || line[n - 1] == '\r'))
                line[--n] = 0;
            if (*s == '[') {
                char *close = strchr(s, ']');
                if (!close)
                    continue;
                *close = 0;
                s++;
                /* "profile name" or "name" */
                if (uc_prefix_profile(s))
                    s += 8;
                while (*s == ' ')
                    s++;
                uc_copy(curprof, *s ? s : "default", UC_NAME);
                insec = 1;
            } else {
                char *value;
                eq = strchr(s, '=');
                if (!eq)
                    continue;
                value = eq + 1;
                *eq = 0;
                while (eq > s && (eq[-1] == ' ' || eq[-1] == '\t'))
                    *--eq = 0;
                while (*value == ' ' || *value == '\t')
                    value++;
                upconf_set(c, curprof, s, value);
            }
        }
    }
    return 1;
}

const char *upconf_get(const upconf *c, const char *profile, const char *key)
{
    int p, k;
    if (!profile || !*profile || !key || !*key)
        return 0;
    p = prof_find(c, profile);
    if (p < 0)
        return 0;
    k = key_find(c, p, key);
    if (k < 0)
        return 0;
    return c->val[p][k];
}

const char *upconf_str(const upconf *c, const char *profile, const char *key, const char *def)
{
    const char *v = upconf_get(c, profile, key);
    if (!v || !*v)
        return def;
    return v;
}

long upconf_int(const upconf *c, const char *profile, const char *key, long def)
{
    const char *v = upconf_get(c, profile, key);
    long r = 0;
    int neg = 0, any = 0;
    if (!v)
        return def;
    while (*v == ' ')
        v++;
    if (*v == '-') {
        neg = 1;
        v++;
    }
    while (*v >= '0' && *v <= '9') {
        r = r * 10 + (*v - '0');
        any = 1;
        v++;
    }
    if (!any)
        return def;
    return neg ? -r : r;
}

uc_u32 upconf_hex(const char *v, uc_u32 def)
{
    uc_u32 r = 0;
    int any = 0;
    if (!v)
        return def;
    while (*v == ' ')
        v++;
    if (*v == '#')
        v++;
    else if (*v == '0' && (v[1] == 'x' || v[1] == 'X'))
        v += 2;
    while (*v) {
        char h = *v;
        if (h >= '0' && h <= '9')
            r = (r << 4) | (uc_u32)(h - '0');
        else if (h >= 'a' && h <= 'f')
            r = (r << 4) | (uc_u32)(h - 'a' + 10);
        else if (h >= 'A' && h <= 'F')
            r = (r << 4) | (uc_u32)(h - 'A' + 10);
        else
            break;
        any = 1;
        v++;
    }
    if (!any)
        return def;
    return r & 0xFFFFFFUL;
}

/* Strictly six hex digits, nothing else: for a field the user typed, where a
 * seventh digit or trailing junk must be an error rather than silently cut.
 * upconf_hex is the lenient one for values already in a file. */
int upconf_hex6(const char *s, uc_u32 *rgb)
{
    uc_u32 r = 0;
    int i;
    if (!s || !rgb)
        return 0;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '#')
        s++;
    else if (*s == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;
    for (i = 0; i < 6; i++) {
        char h = s[i];
        if (h >= '0' && h <= '9')
            r = (r << 4) | (uc_u32)(h - '0');
        else if (h >= 'a' && h <= 'f')
            r = (r << 4) | (uc_u32)(h - 'a' + 10);
        else if (h >= 'A' && h <= 'F')
            r = (r << 4) | (uc_u32)(h - 'A' + 10);
        else
            return 0;
    }
    s += 6;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s)
        return 0;
    *rgb = r;
    return 1;
}

uc_u32 upconf_rgb(const upconf *c, const char *profile, const char *key, uc_u32 def)
{
    return upconf_hex(upconf_get(c, profile, key), def);
}

int upconf_palette_parse(const char *s, uc_u32 *out16)
{
    const char *p;
    int n = 0;
    if (!out16)
        return 0;
    memset(out16, 0, 16 * sizeof(uc_u32));
    if (!s)
        return 0;
    p = s;
    while (*p) {
        long idx = 0, rgb = 0;
        int any = 0;
        /* the pair "index,RRGGBB": a malformed one stops the list */
        while (*p >= '0' && *p <= '9') {
            if (idx < 16) /* capped while it is read: no digit count wraps it */
                idx = idx * 10 + (*p - '0');
            p++;
            any = 1;
        }
        if (!any || *p != ',')
            break;
        p++;
        if (*p == '0' && (p[1] == 'x' || p[1] == 'X'))
            p += 2;
        else if (*p == '#')
            p++;
        any = 0;
        while (*p && *p != ',') {
            char h = *p;
            if (h >= '0' && h <= '9')
                rgb = rgb * 16 + (h - '0');
            else if (h >= 'a' && h <= 'f')
                rgb = rgb * 16 + (h - 'a' + 10);
            else if (h >= 'A' && h <= 'F')
                rgb = rgb * 16 + (h - 'A' + 10);
            else
                break;
            if (rgb > 0xFFFFFF) /* too many digits: stop before it wraps */
                break;
            p++;
            any = 1;
        }
        if (!any || idx < 0 || idx >= 16 || rgb > 0xFFFFFF)
            break;
        out16[(int)idx] = 0x01000000UL | (uc_u32)rgb;
        n++;
        if (*p == ',')
            p++;
    }
    return n;
}

long upconf_palette_str(const uc_u32 *in16, char *out, long cap)
{
    char *o = out;
    char *end = out + cap;
    int i, first = 1;
    if (!in16 || !out || cap <= 0)
        return -1;
    *o = 0;
    for (i = 0; i < 16; i++) {
        char num[3];
        char hex[8];
        char *h;
        uc_u32 rgb;
        if (!(in16[i] & 0x01000000UL))
            continue;
        rgb = in16[i] & 0xFFFFFFUL;
        num[0] = (char)('0' + i / 10);
        num[1] = (char)('0' + i % 10);
        num[2] = 0;
        hex[6] = 0; /* six digits, then the end: the terminator was at [5],
                     * where the first digit went, and the entry ran on into
                     * whatever followed on the stack (2026-10-02) */
        h = hex + 5;
        while (h >= hex) {
            *h-- = (char)"0123456789ABCDEF"[rgb & 0xF];
            rgb >>= 4;
        }
        if (!first && !uc_copyto(&o, end, ","))
            return -1;
        if (!uc_copyto(&o, end, num) || !uc_copyto(&o, end, ",") ||
            !uc_copyto(&o, end, hex))
            return -1;
        first = 0;
    }
    return (long)(o - out);
}

int upconf_has(const upconf *c, const char *profile, const char *key)
{
    return upconf_get(c, profile, key) ? 1 : 0;
}

static int prof_index(const upconf *c, const char *profile)
{
    const char *names[UC_MAX_PROFILES + 1];
    int n = upconf_profiles(c, names), i;
    for (i = 0; i < n; i++)
        if (uc_ieq(names[i], profile))
            return i;
    return -1;
}

/* every key of profile in a has that value in b (b's last, as lookups see it) */
static int keys_in(const upconf *a, const upconf *b, const char *profile)
{
    int i = prof_index(a, profile), k;
    if (i < 0)
        return 1;
    for (k = 0; k < a->n[i]; k++) {
        const char *va = upconf_get(a, profile, a->key[i][k]);
        const char *vb = upconf_get(b, profile, a->key[i][k]);
        if (!vb || strcmp(va, vb))
            return 0;
    }
    return 1;
}

int upconf_profile_equal(const upconf *a, const upconf *b, const char *profile)
{
    return keys_in(a, b, profile) && keys_in(b, a, profile);
}

int upconf_profiles(const upconf *c, const char **names)
{
    int i;
    for (i = 0; i < c->nprof; i++)
        names[i] = c->prof[i];
    names[c->nprof] = 0;
    return c->nprof;
}

/* The comment lines anchored before key k of profile p, each on its line. */
static int notes_to(const upconf *c, int p, int k, char **o, char *end)
{
    int i;
    for (i = 0; i < c->nnote; i++)
        if (c->note_prof[i] == p && (p < 0 || c->note_key[i] == k) &&
            (!uc_copyto(o, end, c->note + c->note_at[i]) || !uc_copyto(o, end, "\n")))
            return 0;
    return 1;
}

long upconf_save(const upconf *c, char *buf, long cap)
{
    char *o = buf;
    char *end = buf + cap;
    int p, k;
    if (!notes_to(c, -1, 0, &o, end))
        return -1;
    for (p = 0; p < c->nprof; p++) {
        if (!uc_copyto(&o, end, p ? "\n\n[profile " : o > buf ? "\n[profile " : "[profile ") ||
            !uc_copyto(&o, end, c->prof[p]) || !uc_copyto(&o, end, "]\n"))
            return -1;
        for (k = 0; k <= c->n[p]; k++) {
            if (!notes_to(c, p, k, &o, end))
                return -1;
            if (k == c->n[p])
                break;
            if (!uc_copyto(&o, end, c->key[p][k]) ||
                !uc_copyto(&o, end, " = ") ||
                !uc_copyto(&o, end, c->val[p][k]) ||
                !uc_copyto(&o, end, "\n"))
                return -1;
        }
    }
    return (long)(o - buf);
}
