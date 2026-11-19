/* dopustb_bank_core -- see dopustb_bank_core.h. Two passes over the same walker: dry (validate, count) then write, so out is
 * only touched on success. */
#include <string.h>
#include "dopustb_bank_core.h"

typedef struct { const char *cmd, *img; } dtb_row;
/* Every image is a row of dist/dopus-toolbar.txt (tests/test_dopustb_bank.c checks). Commands are the stock type-0 ones. */
static const dtb_row ROWS[] = {
    { "devicelist", "Root" }, { "root", "Root" }, { "cachelist", "World" }, { "parent", "Parent" }, { "all", "All" },
    { "none", "All_s" }, { "select", "Select" }, { "toggle", "Toggle" }, { "makedir", "Makedir" }, { "assign", "Assign" },
    { "copy", "Copy" }, { "copyas", "CopyAs" }, { "move", "Move" }, { "moveas", "MoveAs" }, { "rename", "Rename" },
    { "delete", "Delete" }, { "show", "Show" }, { "read", "Read" }, { "play", "Play" }, { "protect", "Protect" },
    { "comment", "Comment" }, { "datestamp", "Datestamp" }
};
#define NROWS ((int)(sizeof ROWS / sizeof ROWS[0]))
#define STOCKDIR "dopus5:images/"
#define NAMEMAX 40
#define BTNW_FLAGS 128
#define BORDERLESS 0x10UL

const char *dt_bank_image(const char *cmd)
{
    int i;
    for (i = 0; cmd && i < NROWS; i++) if (strcmp(ROWS[i].cmd, cmd) == 0) return ROWS[i].img;
    return NULL;
}
const char *dt_bank_entry(int i, const char **image)
{
    if (i < 0 || i >= NROWS) return NULL;
    if (image) *image = ROWS[i].img;
    return ROWS[i].cmd;
}

static unsigned long rd32(const unsigned char *p) { return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3]; }
static void wr32(unsigned char *p, unsigned long v) { p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16); p[2] = (unsigned char)(v >> 8); p[3] = (unsigned char)v; }
static char lc(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

static int has_stock(const unsigned char *s, unsigned long n)
{
    unsigned long i, k = sizeof STOCKDIR - 1;
    if (n < k) return 0;
    for (i = 0; i < k; i++) if (lc((char)s[i]) != STOCKDIR[i]) return 0;
    return 1;
}

/* Key of the FUNC chunk body f (n bytes): first word of its first instruction, lowercase, only when that instruction is type 0. */
static const char *func_image(const unsigned char *f, unsigned long n)
{
    char key[16];
    unsigned long i = 22, k = 0;
    if (n < 22) return NULL;
    while (i < n && f[i] && f[i] != ' ' && f[i] != '\t') {
        if (k >= sizeof key - 1) return NULL;
        key[k++] = lc((char)f[i++]);
    }
    key[k] = 0;
    while (i < n && f[i]) i++;                       /* rest of the instruction */
    if (i + 3 > n || f[i + 1] != 0 || f[i + 2] != 0) return NULL;   /* NUL + u16 type 0 */
    return dt_bank_image(key);
}

typedef struct { unsigned char *out; long cap, n; int over; } dtb_w;
static void put(dtb_w *w, const void *p, unsigned long n)
{
    if (w->n + (long)n > w->cap) w->over = 1;
    else if (w->out && n) memcpy(w->out + w->n, p, n);
    w->n += (long)n;
}
static void put_pad(dtb_w *w, unsigned long sz) { if (sz & 1) put(w, "", 1); }

#define PATHMAX 256
/* Replacement for image string s (len bytes, no NUL) of state st: the flat brush when img is set and st is 0, the drawer rewrite
 * for a stock path, else NULL (keep). buf holds the result. */
static const char *new_image(const unsigned char *s, unsigned long len, int st, const char *img, const char *set, char *buf, int *rewrote)
{
    unsigned long k = 0, sl = strlen(set), pre = sizeof STOCKDIR - 1;
    const char *tail;
    unsigned long tl;
    if (st == 0 && img) { tail = img; tl = strlen(img) + 6; }          /* Name.small */
    else if (has_stock(s, len)) { tail = (const char *)s + pre; tl = len - pre; }
    else return NULL;
    if (pre + 7 + sl + 1 + tl + 1 > PATHMAX) return NULL;
    memcpy(buf, STOCKDIR, pre); k = pre;
    memcpy(buf + k, "Patina/", 7); k += 7;
    memcpy(buf + k, set, sl); k += sl;
    buf[k++] = '/';
    memcpy(buf + k, tail, tl - (st == 0 && img ? 6 : 0)); k += tl - (st == 0 && img ? 6 : 0);
    if (st == 0 && img) { memcpy(buf + k, ".small", 6); k += 6; *rewrote = 1; }
    buf[k] = 0;
    return buf;
}

/* Emits one BUTN chunk (header, body, pad) rewritten, or verbatim when the body does not parse. */
static void emit_butn(dtb_w *w, const unsigned char *b, unsigned long n, const char *img, const char *set, int *rewrote)
{
    unsigned long N = n >= 14 ? b[7] : 0, i, st, e, size = 14, ok = n >= 14;
    char buf[PATHMAX];
    int r = 0, pass;
    unsigned char h[8];
    /* parse check: 2N NUL ended strings inside the body */
    for (i = 14, st = 0; ok && st < N * 2; st++) {
        for (e = i; e < n && b[e]; e++) {}
        if (e >= n) ok = 0; else i = e + 1;
    }
    if (!ok) { memcpy(h, "BUTN", 4); wr32(h + 4, n); put(w, h, 8); put(w, b, n); put_pad(w, n); return; }
    for (pass = 0; pass < 2; pass++) {
        const char *rep;
        r = 0;
        for (i = 14, st = 0; st < N * 2; st++) {
            for (e = i; b[e]; e++) {}
            rep = (st & 1) ? NULL : new_image(b + i, e - i, (int)(st >> 1), img, set, buf, &r);
            if (pass == 0) size += (rep ? strlen(rep) : e - i) + 1;
            else if (rep) put(w, rep, strlen(rep) + 1);
            else put(w, b + i, e - i + 1);
            i = e + 1;
        }
        if (pass == 0) {
            size += n - i;                                   /* bytes after the pairs stay */
            memcpy(h, "BUTN", 4); wr32(h + 4, size); put(w, h, 8); put(w, b, 14);
        } else { put(w, b + i, n - i); put_pad(w, size); }
    }
    if (r) *rewrote = 1;
}

/* One pass over the bank into w (w->out NULL = measure only). Returns 0 or DTB_BAD; *rw = buttons rewritten. */
static long walk(const unsigned char *in, long len, const char *set, dtb_w *w, int *rw)
{
    unsigned long end, i = 12, sz, nx;
    long flagpos = -1;
    unsigned char h[12];
    if (len < 12 || memcmp(in, "FORM", 4) || memcmp(in + 8, "EPUS", 4)) return DTB_BAD;
    end = rd32(in + 4) + 8;
    if (end > (unsigned long)len || end < 12) return DTB_BAD;
    memcpy(h, in, 12); put(w, h, 12);
    while (i < end) {
        if (end - i < 8) return DTB_BAD;
        sz = rd32(in + i + 4);
        if (sz > end - i - 8) return DTB_BAD;
        nx = i + 8 + sz + (sz & 1);                           /* a missing last pad is tolerated */
        if (!memcmp(in + i, "BUTN", 4)) {
            const char *img = NULL;
            if (nx + 8 <= end && !memcmp(in + nx, "FUNC", 4) && rd32(in + nx + 4) <= end - nx - 8)
                img = func_image(in + nx + 8, rd32(in + nx + 4));
            emit_butn(w, in + i + 8, sz, img, set, rw);
        } else {
            if (!memcmp(in + i, "BTNW", 4) && sz >= BTNW_FLAGS + 4 && flagpos < 0) flagpos = w->n + 8 + BTNW_FLAGS;
            put(w, in + i, 8 + sz);
            if (sz & 1) put(w, "", 1);
        }
        i = nx;
    }
    if (*rw && flagpos >= 0 && w->out && !w->over)
        wr32(w->out + flagpos, rd32(w->out + flagpos) | BORDERLESS);
    return 0;
}

long dt_bank_flatten(const unsigned char *in, long len, const char *set, unsigned char *out, long cap)
{
    dtb_w w;
    int rw = 0;
    long sl = set ? (long)strlen(set) : 0;
    if (!in || !out || sl == 0 || sl > 32 || len < 12) return DTB_BAD;
    w.out = NULL; w.cap = 0x7fffffffL; w.n = 0; w.over = 0;
    if (walk(in, len, set, &w, &rw) < 0) return DTB_BAD;
    if (w.n > cap) return DTB_FULL;
    w.out = out; w.cap = cap; w.n = 0; rw = 0;
    walk(in, len, set, &w, &rw);
    wr32(out + 4, (unsigned long)w.n - 8);
    return w.n;
}

int dt_bank_name(const char *tbar, const char *set, char *out, int cap)
{
    static const char pre[] = "DOpus5:Buttons/Patina-";
    const char *base = tbar, *p;
    int n, bl, sl;
    if (!tbar || !set || !*tbar || !*set || !out) return -1;
    for (p = tbar; *p; p++) if (*p == ':' || *p == '/') base = p + 1;
    bl = (int)strlen(base); if (bl > NAMEMAX) bl = NAMEMAX;
    sl = (int)strlen(set);
    if (bl == 0) return -1;
    n = (int)sizeof pre - 1;
    if (n + bl + 1 + sl + 1 > cap) return -1;
    memcpy(out, pre, n); memcpy(out + n, base, bl); n += bl; out[n++] = '-'; memcpy(out + n, set, sl); out[n + sl] = 0;
    return 0;
}
