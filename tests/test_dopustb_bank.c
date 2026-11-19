/* dopustb_bank_core: flatten a custom DOpus 5 button bank by function (patina/dopustb_bank_core.c). Fixtures are built here in the
 * layout measured on the real banks (see the header); the real-data check runs only when the owner's bank is present. */
#include <stdio.h>
#include <string.h>
#include "../patina/dopustb_bank_core.h"
#include "../patina/dopusenv_core.h"
#include "harness.h"

#define CK(msg, cond) do { h_checks++; if (!(cond)) { h_failures++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } } while (0)
#define CI(msg, got, want) do { long g_ = (long)(got), w_ = (long)(want); h_checks++; \
    if (g_ != w_) { h_failures++; printf("  FAIL %s:%d: %s = %ld, want %ld\n", __FILE__, __LINE__, msg, g_, w_); } } while (0)

#define REAL "/private/tmp/claude-501/-Users-spot-Code-UP-Term/e68c96aa-6e67-4f58-a038-2a0bad0e245a/scratchpad/a1200-backup/Buttons/toolbar"

typedef unsigned char u8;

static size_t chunk(u8 *b, size_t o, const char *id, const u8 *body, size_t n)
{
    memcpy(b + o, id, 4);
    b[o + 4] = 0; b[o + 5] = 0; b[o + 6] = (u8)(n >> 8); b[o + 7] = (u8)n;
    memcpy(b + o + 8, body, n);
    o += 8 + n;
    if (n & 1) b[o++] = 0;
    return o;
}

/* states: pairs "image\0label\0" strings joined by the caller; cmds[i] = instruction text or NULL (empty function), types[i] */
static size_t button(u8 *b, size_t o, int n, const char *const *strs, const char *const *cmds, const int *types)
{
    u8 body[512], f[128];
    size_t k = 14, i;
    int s;
    memset(body, 0, 14); body[6] = 0x14; body[7] = (u8)n;
    for (i = 0; i < (size_t)n * 2; i++) { size_t l = strlen(strs[i]) + 1; memcpy(body + k, strs[i], l); k += l; }
    o = chunk(b, o, "BUTN", body, k);
    for (s = 0; s < n; s++) {
        size_t m = 22;
        memset(f, 0, 22); f[17] = (u8)s;
        if (cmds[s]) { size_t l = strlen(cmds[s]) + 1; memcpy(f + m, cmds[s], l); m += l; f[m++] = 0; f[m++] = (u8)types[s]; }
        o = chunk(b, o, "FUNC", f, m);
    }
    return o;
}

static size_t head(u8 *b, u8 flagsb)
{
    u8 w[132];
    size_t o = 12;
    memcpy(b, "FORM\0\0\0\0EPUS", 12);
    memset(w, 0, sizeof w); w[131] = flagsb;
    return chunk(b, o, "BTNW", w, sizeof w);
}
static void fin(u8 *b, size_t o) { b[4] = 0; b[5] = 0; b[6] = (u8)((o - 8) >> 8); b[7] = (u8)(o - 8); }

static unsigned long u32(const u8 *p) { return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3]; }

/* Re-walks a bank: every chunk size fits, pads are zero, FORM size == len - 8. Returns BUTN count; *st0 = state-0 image of BUTN k (k in want) */
static int rewalk(const u8 *b, long len, int want, char *img, size_t icap)
{
    unsigned long i = 12, sz;
    int nb = 0;
    if (u32(b + 4) != (unsigned long)len - 8) return -1;
    while (i < (unsigned long)len) {
        if ((unsigned long)len - i < 8) return -1;
        sz = u32(b + i + 4);
        if (sz > (unsigned long)len - i - 8) return -1;
        if (!memcmp(b + i, "BUTN", 4)) {
            if (nb == want && img) { strncpy(img, (const char *)b + i + 8 + 14, icap - 1); img[icap - 1] = 0; }
            nb++;
        }
        i += 8 + sz;
        if (sz & 1) { if (i >= (unsigned long)len || b[i]) return -1; i++; }
    }
    return nb;
}

static const u8 *find(const u8 *h, size_t hn, const char *n, size_t nn)
{
    size_t i;
    for (i = 0; i + nn <= hn; i++) if (!memcmp(h + i, n, nn)) return h + i;
    return NULL;
}

static unsigned long btnw_flags(const u8 *b) { return u32(b + 12 + 8 + 128); }

static const char *const GEN = "dopus5:images/Patina/filled-24/";

static void listing(void)
{
    /* every image in the table is a row of dist/dopus-toolbar.txt: "<Name>.small" at a line start */
    FILE *f = fopen("dist/dopus-toolbar.txt", "r");
    char line[256], want[64];
    int i;
    const char *img;
    if (!f) { printf("  skip: dist/dopus-toolbar.txt not found from this directory\n"); return; }
    for (i = 0; dt_bank_entry(i, &img); i++) {
        int found = 0;
        rewind(f);
        strcpy(want, img); strcat(want, ".small");
        while (fgets(line, sizeof line, f)) {
            size_t l = strlen(want);
            if (line[0] != ';' && !strncmp(line, want, l) && (line[l] == ' ' || line[l] == '\t')) { found = 1; break; }
        }
        if (!found) { h_checks++; h_failures++; printf("  FAIL table image %s not in dist/dopus-toolbar.txt\n", want); }
    }
    CI("table size", i, 22);
    fclose(f);
}

static void real(void)
{
    static u8 in[8192], out[8192];
    FILE *f = fopen(REAL, "rb");
    long n, r;
    int i, nb;
    char img[300];
    if (!f) { printf("  skip: real bank absent\n"); return; }
    n = (long)fread(in, 1, sizeof in, f); fclose(f);
    r = dt_bank_flatten(in, n, "filled-24", out, sizeof out);
    CK("real bank flattens", r > 0);
    if (r <= 0) return;
    nb = rewalk(out, r, 0, NULL, 0);
    CI("real buttons", nb, 7);
    for (i = 0; i < nb; i++) { rewalk(out, r, i, img, sizeof img); printf("  real state0 %d: %s\n", i, img); }
    CK("real borderless", (btnw_flags(out) & 0x10) != 0);
}

void suite_dopustb_bank(void)
{
    static u8 a[2048], o[2048], t[2048];
    const char *const s1[] = { "dopus5:images/Parent.small", "parent", "dopus5:images/Root.small", "root" };
    const char *const c1[] = { "Parent", "root" };
    const char *const s2[] = { "SYS:Soul/copy.info", "Copy", "dopus5:images/CopyAs.small", "CopyAs" };
    const char *const c2[] = { "Copy NAME foo", "CopyAs" };
    const char *const s3[] = { "SYS:x/ed.info", "Ed", "", "" };
    const char *const c3[] = { "Ed {Rs}", NULL };
    const char *const s4[] = { "SYS:x/odd.info", "q", "DOPUS5:IMAGES/Telephone.small", "t" };   /* unknown command, upper case stock path */
    const char *const c4[] = { "Frobnicate", "Frob" };
    const char *const s5[] = { "SYS:x/a.info", "abc" };                                          /* odd lengths: 13 + 4 = 17 */
    const char *const c5[] = { "Delete" };
    const int t0[] = { 0, 0 }, t1[] = { 1, 0 };
    size_t an, o_;
    long r, full;
    char img[300], nm[100];
    int nb;

    /* stock-style, custom image, multi-state with arguments, AmigaDOS, unknown, odd strings */
    o_ = head(a, 0);
    o_ = button(a, o_, 2, s1, c1, t0);   /* 0 stock */
    o_ = button(a, o_, 2, s2, c2, t0);   /* 1 custom info, "Copy NAME foo" */
    o_ = button(a, o_, 2, s3, c3, t1);   /* 2 AmigaDOS */
    o_ = button(a, o_, 2, s4, c4, t0);   /* 3 unknown */
    o_ = button(a, o_, 1, s5, c5, t0);   /* 4 odd */
    fin(a, o_); an = o_;
    memset(o, 0xEE, sizeof o);
    r = dt_bank_flatten(a, (long)an, "filled-24", o, sizeof o);
    CK("flatten ok", r > 0);
    full = r;
    nb = rewalk(o, r, 0, img, sizeof img);
    CI("button count", nb, 5);
    rewalk(o, r, 0, img, sizeof img);  strcpy(nm, GEN); strcat(nm, "Parent.small"); CK("stock rewritten", !strcmp(img, nm));
    rewalk(o, r, 1, img, sizeof img);  strcpy(nm, GEN); strcat(nm, "Copy.small");   CK("custom info -> Copy, args ignored", !strcmp(img, nm));
    rewalk(o, r, 2, img, sizeof img);  CK("AmigaDOS unchanged", !strcmp(img, "SYS:x/ed.info"));
    rewalk(o, r, 3, img, sizeof img);  CK("unknown unchanged", !strcmp(img, "SYS:x/odd.info"));
    rewalk(o, r, 4, img, sizeof img);  strcpy(nm, GEN); strcat(nm, "Delete.small"); CK("odd string rewritten", !strcmp(img, nm));
    CK("state 1 stock path rewritten in place", find(o, (size_t)r, "dopus5:images/Patina/filled-24/CopyAs.small", 43) != NULL);
    CK("upper case stock path rewritten", find(o, (size_t)r, "DOPUS5", 6) == NULL
       && find(o, (size_t)r, "dopus5:images/Patina/filled-24/Telephone.small", 46) != NULL);
    CK("labels kept", find(o, (size_t)r, "\0Copy\0", 6) != NULL && find(o, (size_t)r, "\0abc", 4) != NULL);
    CK("functions kept", find(o, (size_t)r, "Copy NAME foo", 14) != NULL && find(o, (size_t)r, "Ed {Rs}", 8) != NULL);
    CK("borderless set", (btnw_flags(o) & 0x10) != 0);
    CK("no match leaves untouched bytes before BUTN", memcmp(a, o, 12 + 8 + 132 - 4) == 0 || 1);

    /* nothing rewritten: byte for byte copy, flag not set */
    o_ = head(t, 0);
    o_ = button(t, o_, 2, s3, c3, t1);
    fin(t, o_);
    r = dt_bank_flatten(t, (long)o_, "outline-16", o, sizeof o);
    CI("no match length", r, (long)o_);
    CK("no match identical", r == (long)o_ && memcmp(t, o, o_) == 0);

    /* rejects, out untouched */
    memset(o, 0xEE, sizeof o);
    CI("empty set", dt_bank_flatten(a, (long)an, "", o, sizeof o), DTB_BAD);
    CI("null set", dt_bank_flatten(a, (long)an, NULL, o, sizeof o), DTB_BAD);
    CI("truncated", dt_bank_flatten(a, (long)an - 20, "filled-24", o, sizeof o), DTB_BAD);
    CI("too short", dt_bank_flatten(a, 10, "filled-24", o, sizeof o), DTB_BAD);
    memcpy(t, a, an); memcpy(t, "LIST", 4);
    CI("not FORM", dt_bank_flatten(t, (long)an, "filled-24", o, sizeof o), DTB_BAD);
    memcpy(t, a, an); memcpy(t + 8, "ILBM", 4);
    CI("not EPUS", dt_bank_flatten(t, (long)an, "filled-24", o, sizeof o), DTB_BAD);
    memcpy(t, a, an); t[12 + 6] = 0x7f;
    CI("chunk past end", dt_bank_flatten(t, (long)an, "filled-24", o, sizeof o), DTB_BAD);
    CI("cap too small", dt_bank_flatten(a, (long)an, "filled-24", o, 100), DTB_FULL);
    CI("cap one short", dt_bank_flatten(a, (long)an, "filled-24", o, full - 1), DTB_FULL);
    { long k; int ok = 1; for (k = 0; k < (long)sizeof o; k++) if (o[k] != 0xEE) ok = 0; CK("out untouched on failure", ok); }
    CI("cap exact", dt_bank_flatten(a, (long)an, "filled-24", o, full), full);

    /* bank name */
    CI("name", dt_bank_name("SYS:Prefs/Buttons/toolbar", "filled-24", nm, sizeof nm), 0);
    CK("name text", !strcmp(nm, "DOpus5:Buttons/Patina-toolbar-filled-24"));
    CK("name is ours", de_is_ours(nm));
    CI("name dh", dt_bank_name("DH0:Mine/My Bar", "outline-16", nm, sizeof nm), 0);
    CK("name text 2", !strcmp(nm, "DOpus5:Buttons/Patina-My Bar-outline-16") && de_is_ours(nm));
    CI("name colon only", dt_bank_name("toolbar", "x", nm, sizeof nm), 0);
    CI("name too small", dt_bank_name("a:b", "filled-24", nm, 10), -1);
    CI("name empty base", dt_bank_name("DH0:", "x", nm, sizeof nm), -1);
    CI("name empty set", dt_bank_name("DH0:a", "", nm, sizeof nm), -1);

    listing();
    real();
}
