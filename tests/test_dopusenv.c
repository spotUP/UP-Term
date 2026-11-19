/* dopusenv_core: the TBAR chunk of a DOpus 5.104 FORM EPUS environment file (patina/dopusenv_core.c). The fixture is built here in
 * the stock layout (ENVR, TBAR "PROGDIR:Buttons/toolbar" + NUL = 24 bytes, LMEN, odd-sized BANK + pad), measured on the real files. */
#include <stdio.h>
#include <string.h>
#include "../patina/dopusenv_core.h"
#include "harness.h"

#define CK(msg, cond) do { h_checks++; if (!(cond)) { h_failures++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } } while (0)
#define CI(msg, got, want) do { long g_ = (long)(got), w_ = (long)(want); h_checks++; \
    if (g_ != w_) { h_failures++; printf("  FAIL %s:%d: %s = %ld, want %ld\n", __FILE__, __LINE__, msg, g_, w_); } } while (0)

static size_t put_chunk(uint8_t *b, size_t o, const char *id, const char *body, size_t n)
{
    memcpy(b + o, id, 4);
    b[o + 4] = 0; b[o + 5] = 0; b[o + 6] = (uint8_t)(n >> 8); b[o + 7] = (uint8_t)n;
    memcpy(b + o + 8, body, n);
    o += 8 + n;
    if (n & 1) b[o++] = 0;
    return o;
}

static size_t build(uint8_t *b, const char *tbar)
{
    char env[40];
    size_t o = 12;
    memset(env, 7, sizeof env);
    memcpy(b, "FORM\0\0\0\0EPUS", 12);
    o = put_chunk(b, o, "ENVR", env, 40);
    o = put_chunk(b, o, "TBAR", tbar, strlen(tbar) + 1);
    o = put_chunk(b, o, "LMEN", env, 28);
    o = put_chunk(b, o, "BANK", env, 13);      /* odd: pad byte */
    b[4] = 0; b[5] = 0; b[6] = (uint8_t)((o - 8) >> 8); b[7] = (uint8_t)(o - 8);
    return o;
}

/* the FORM size field equals len - 8, and every chunk before and after TBAR is unchanged (tail compared from the end) */
static void check_set(const uint8_t *a, size_t an, const char *path)
{
    uint8_t o[1024];
    char got[DE_PATHMAX];
    size_t tail = 8 + 28 + 8 + 14;              /* LMEN + BANK with pad */
    long n = de_tbar_set(a, an, path, o, sizeof o);
    CK("set ok", n > 0);
    CI("FORM size", ((size_t)o[4] << 24 | (size_t)o[5] << 16 | (size_t)o[6] << 8 | o[7]), n - 8);
    CI("even", n & 1, 0);
    CK("head kept", memcmp(a + 8, o + 8, 4) == 0 && memcmp(a + 12, o + 12, 8 + 40) == 0);
    CK("tail kept", memcmp(a + an - tail, o + n - tail, tail) == 0);
    CI("get", de_tbar_get(o, (size_t)n, got, sizeof got), DE_OK);
    CK("get returns the path", strcmp(got, path) == 0);
}

void suite_dopusenv(void)
{
    uint8_t a[1024], o[1024], t[1024];
    char got[DE_PATHMAX], small[8];
    size_t an = build(a, DE_STOCK);
    long n;

    CI("stock get", de_tbar_get(a, an, got, sizeof got), DE_OK);
    CK("stock value", strcmp(got, DE_STOCK) == 0);
    CI("get into a small buffer", de_tbar_get(a, an, small, sizeof small), DE_FULL);
    check_set(a, an, "DOpus5:Buttons/Patina-toolbar-filled-32t");        /* longer, 41 bytes with NUL: odd */
    check_set(a, an, "DOpus5:Buttons/Patina-toolbar-outline-32");        /* 40 + NUL: odd size, other parity */
    check_set(a, an, "RAM:t");                                           /* shorter, even total */
    check_set(a, an, "RAM:tb");                                          /* shorter, odd total */
    n = de_tbar_set(a, an, DE_STOCK, o, sizeof o);
    CK("original value gives identical bytes", n == (long)an && memcmp(a, o, an) == 0);
    n = de_tbar_set(a, an, "DOpus5:Buttons/Patina-toolbar-filled-24", o, sizeof o);
    CI("then back to stock", de_tbar_set(o, (size_t)n, DE_STOCK, t, sizeof t), (long)an);
    CK("round trip identical", memcmp(a, t, an) == 0);

    /* failures write nothing */
    memset(o, 0xAA, sizeof o);
    CI("output too small", de_tbar_set(a, an, "DOpus5:Buttons/Patina-toolbar-filled-32t", o, an), DE_FULL);
    CK("output untouched", o[0] == 0xAA && o[an - 1] == 0xAA);
    CI("empty path", de_tbar_set(a, an, "", o, sizeof o), DE_FULL);
    memcpy(t, a, an); memcpy(t + 8, "ILBM", 4);
    CI("not EPUS", de_tbar_set(t, an, "x:y", o, sizeof o), DE_BAD);
    memcpy(t, a, an); memcpy(t, "LIST", 4);
    CI("not FORM", de_tbar_get(t, an, got, sizeof got), DE_BAD);
    CI("too short", de_tbar_get(a, 10, got, sizeof got), DE_BAD);
    CI("truncated file", de_tbar_get(a, an - 20, got, sizeof got), DE_BAD);
    memcpy(t, a, an); t[12 + 6] = 0x7f;                                   /* ENVR size far past the end */
    CI("chunk size past end", de_tbar_set(t, an, "x:y", o, sizeof o), DE_BAD);
    memcpy(t, a, an); memcpy(t + 12 + 8 + 40, "TBA2", 4);
    CI("no TBAR", de_tbar_set(t, an, "x:y", o, sizeof o), DE_NOTBAR);
    CI("no TBAR get", de_tbar_get(t, an, got, sizeof got), DE_NOTBAR);
    memcpy(t, a, an); t[12 + 8 + 40 + 8 + 23] = 'x';                      /* TBAR not NUL ended */
    CI("unterminated TBAR", de_tbar_get(t, an, got, sizeof got), DE_FULL);

    /* policy */
    CK("stock replaceable", de_may_replace("PROGDIR:Buttons/toolbar"));
    CK("stock any case", de_may_replace("progdir:buttons/TOOLBAR"));
    CK("our bank A replaceable", de_may_replace("DOpus5:Buttons/Patina-toolbar-filled-32"));
    CK("our bank B replaceable", de_may_replace("PROGDIR:Buttons/Patina-toolbar-outline-16t"));
    CK("flattened custom bank is ours", de_may_replace("DOpus5:Buttons/Patina-bank-mybar-filled-24") && de_may_restore("dopus5:buttons/patina-bank-mybar-outline-16"));
    CK("user path left alone", !de_may_replace("DH1:Mine/toolbar"));
    CK("stock lookalike left alone", !de_may_replace("PROGDIR:Buttons/toolbar2"));
    CK("user bank left alone", !de_may_replace("DOpus5:Buttons/Other"));
    CK("empty and NULL left alone", !de_may_replace("") && !de_may_replace(NULL));
    CK("restore from our bank", de_may_restore("dopus5:buttons/patina-toolbar-filled-24"));
    CK("no restore of stock", !de_may_restore(DE_STOCK));
    CK("no restore of a user path", !de_may_restore("DH1:Mine/toolbar"));
    CK("pick: custom is its own source", strcmp(de_orig_pick("DH1:Mine/toolbar", NULL), "DH1:Mine/toolbar") == 0);
    CK("pick: custom wins over a stale record", strcmp(de_orig_pick("DH1:New", "DH1:Old"), "DH1:New") == 0);
    CK("pick: stock and stock flat banks use the stock flow", !de_orig_pick(DE_STOCK, "DH1:Old") && !de_orig_pick("DOpus5:Buttons/Patina-toolbar-filled-24", "DH1:Old"));
    CK("pick: copy goes back to the saved original", strcmp(de_orig_pick("DOpus5:Buttons/Patina-bank-mybar-filled-24", "DH1:Mine/mybar"), "DH1:Mine/mybar") == 0);
    CK("pick: copy without a usable record is stock flow", !de_orig_pick("DOpus5:Buttons/Patina-bank-mybar-filled-24", NULL) && !de_orig_pick("DOpus5:Buttons/Patina-bank-mybar-filled-24", "") && !de_orig_pick("DOpus5:Buttons/Patina-bank-mybar-filled-24", "DOpus5:Buttons/Patina-bank-mybar-outline-16"));
    CK("pick: empty and NULL", !de_orig_pick("", "x") && !de_orig_pick(NULL, "x"));
}
