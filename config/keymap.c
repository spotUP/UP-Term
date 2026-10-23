/* keymap: see keymap.h for the file format and the contract. */
#include <string.h>
#include "upconf.h"
#include "keymap.h"

#if defined(VT_COUNT_ALLOC)
/* the host tests: every block counted (tests/harness.c), a leak shows as a live byte */
#include <stdlib.h>
#include "../engine/vtengine.h"
#define KM_MALLOC(n) vt_count_malloc((unsigned long)(n))
#define KM_FREE(p) vt_count_free(p)
#elif defined(__VBCC__) || defined(AMIGA) || defined(__amigaos__)
/* vbcc defines neither AMIGA nor __amigaos__: without __VBCC__ here Amarchy and the handler got
 * vc.lib's malloc, whose 16 KB puddles stay until exit (tests/test_keymap_amiga_alloc.py) */
#include <exec/memory.h>
#include <proto/exec.h>
#define KM_MALLOC(n) AllocVec((ULONG)(n), MEMF_ANY)
#define KM_FREE(p) FreeVec(p)
#else
#include <stdlib.h>
#define KM_MALLOC(n) malloc(n)
#define KM_FREE(p) free(p)
#endif

typedef struct kname {
    const char *name;
    unsigned char code;
    char ch;            /* a symbol name carries its character; 0 for the rest */
} kname;

/* Amiga raw key codes. Letters and digits are board positions. */
static const kname knames[] = {
    { "1", 0x01, 0 }, { "2", 0x02, 0 }, { "3", 0x03, 0 }, { "4", 0x04, 0 }, { "5", 0x05, 0 },
    { "6", 0x06, 0 }, { "7", 0x07, 0 }, { "8", 0x08, 0 }, { "9", 0x09, 0 }, { "0", 0x0A, 0 },
    { "minus", 0x0B, '-' }, { "plus", 0x0C, '+' },
    { "q", 0x10, 0 }, { "w", 0x11, 0 }, { "e", 0x12, 0 }, { "r", 0x13, 0 }, { "t", 0x14, 0 },
    { "y", 0x15, 0 }, { "u", 0x16, 0 }, { "i", 0x17, 0 }, { "o", 0x18, 0 }, { "p", 0x19, 0 },
    { "a", 0x20, 0 }, { "s", 0x21, 0 }, { "d", 0x22, 0 }, { "f", 0x23, 0 }, { "g", 0x24, 0 },
    { "h", 0x25, 0 }, { "j", 0x26, 0 }, { "k", 0x27, 0 }, { "l", 0x28, 0 },
    { "z", 0x31, 0 }, { "x", 0x32, 0 }, { "c", 0x33, 0 }, { "v", 0x34, 0 }, { "b", 0x35, 0 },
    { "n", 0x36, 0 }, { "m", 0x37, 0 }, { "comma", 0x38, ',' }, { "period", 0x39, '.' },
    { "space", 0x40, 0 }, { "backspace", 0x41, 0 }, { "tab", 0x42, 0 }, { "return", 0x44, 0 },
    { "esc", 0x45, 0 }, { "del", 0x46, 0 },
    { "up", 0x4C, 0 }, { "down", 0x4D, 0 }, { "right", 0x4E, 0 }, { "left", 0x4F, 0 },
    { "f1", 0x50, 0 }, { "f2", 0x51, 0 }, { "f3", 0x52, 0 }, { "f4", 0x53, 0 }, { "f5", 0x54, 0 },
    { "f6", 0x55, 0 }, { "f7", 0x56, 0 }, { "f8", 0x57, 0 }, { "f9", 0x58, 0 }, { "f10", 0x59, 0 },
    { "help", 0x5F, 0 }
};
#define NKNAMES ((int)(sizeof(knames) / sizeof(knames[0])))

/* The codes the lookups use: knames[] until keymap_set_layout moves the
 * symbol names to the keys that print them. */
static unsigned char km_code[NKNAMES];
static int km_code_ready;

static void km_code_init(void)
{
    int i;
    for (i = 0; i < NKNAMES; i++)
        km_code[i] = knames[i].code;
    km_code_ready = 1;
}

void keymap_set_layout(int (*char_to_raw)(char ch))
{
    int i, r;
    km_code_init();
    if (!char_to_raw)
        return;
    for (i = 0; i < NKNAMES; i++) {
        if (!knames[i].ch)
            continue;
        r = char_to_raw(knames[i].ch);
        if (r >= 0 && r < 0x80)
            km_code[i] = (unsigned char)r;
    }
}

static char km_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static int km_ieq(const char *a, const char *b)
{
    while (*a && *b) {
        if (km_lower(*a) != km_lower(*b))
            return 0;
        a++;
        b++;
    }
    return *a == *b;
}

int keymap_key_code(const char *name)
{
    int i;
    if (!name)
        return -1;
    if (!km_code_ready)
        km_code_init();
    for (i = 0; i < NKNAMES; i++)
        if (km_ieq(name, knames[i].name))
            return km_code[i];
    return -1;
}

const char *keymap_key_name(unsigned code)
{
    int i;
    if (!km_code_ready)
        km_code_init();
    for (i = 0; i < NKNAMES; i++)
        if (km_code[i] == code)
            return knames[i].name;
    return 0;
}

int keymap_action_scope(int kind)
{
    switch (kind) {
    case KA_TAB:
    case KA_MENUID:
    case KA_SLASH:
        return KS_TERMINAL;
    case KA_OPEN:
    case KA_REXX:
        return KS_COMMODITY;
    case KA_MENU:
    case KA_VIEWER:
    case KA_RUN:
        return KS_BOTH;
    }
    return 0;
}

const km_bind *keymap_find(const keymap *k, unsigned qual, unsigned code)
{
    int i;
    for (i = 0; i < k->nbind; i++)
        if (k->bind[i].qual == qual && k->bind[i].code == code)
            return &k->bind[i];
    return 0;
}

/* ---- text helpers ---- */

#define KM_LINE 256

static char *km_trim(char *s)
{
    char *e;
    while (*s == ' ' || *s == '\t')
        s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
        *--e = 0;
    return s;
}

/* Copy src to dst (size n). 0 when it does not fit. */
static int km_copy(char *dst, int n, const char *src)
{
    int len = (int)strlen(src);
    if (len >= n)
        return 0;
    memcpy(dst, src, (size_t)len + 1);
    return 1;
}

/* The length of src cut to n-1 (a display label may be cut; an action may not). */
static int km_cut(const char *src, int n)
{
    int len = (int)strlen(src);
    return len >= n ? n - 1 : len;
}

/* Replace %n with digit d in src into dst (size n). 0 when too long. */
static int km_expand(char *dst, int n, const char *src, char d)
{
    int o = 0;
    while (*src) {
        if (o + 1 >= n)
            return 0;
        if (src[0] == '%' && src[1] == 'n') {
            dst[o++] = d;
            src += 2;
        } else
            dst[o++] = *src++;
    }
    dst[o] = 0;
    return 1;
}

/* ---- actions ---- */

typedef struct action {
    int kind;                   /* KA_*; KA_NONE is the "none" unbind */
    char arg[KM_ARG];
} action;

typedef struct prefix {
    const char *name;
    int kind;
} prefix;

static const prefix prefixes[] = {
    { "menu", KA_MENU }, { "menuid", KA_MENUID }, { "tab", KA_TAB },
    { "slash", KA_SLASH }, { "run", KA_RUN }, { "rexx", KA_REXX }, { "open", KA_OPEN }
};

static int tab_arg_ok(const char *a)
{
    if (!strcmp(a, "new") || !strcmp(a, "close") || !strcmp(a, "next") || !strcmp(a, "prev"))
        return 1;
    if (!strncmp(a, "goto ", 5)) {
        const char *p = a + 5;
        if (!*p)
            return 0;
        for (; *p; p++)
            if (*p < '0' || *p > '9')
                return 0;
        return 1;
    }
    return 0;
}

/* 1 parsed; 0 bad; -1 bad because the argument does not fit. */
static int action_parse(const char *text, action *a)
{
    const char *colon;
    char head[16];
    int i, n;
    a->arg[0] = 0;
    if (!strcmp(text, "none")) {
        a->kind = KA_NONE;
        return 1;
    }
    if (!strcmp(text, "viewer")) {
        a->kind = KA_VIEWER;
        return 1;
    }
    colon = strchr(text, ':');
    if (!colon)
        return 0;
    n = (int)(colon - text);
    if (n <= 0 || n >= (int)sizeof(head))
        return 0;
    memcpy(head, text, (size_t)n);
    head[n] = 0;
    text = colon + 1;
    if (!*text)
        return 0;
    for (i = 0; i < (int)(sizeof(prefixes) / sizeof(prefixes[0])); i++)
        if (!strcmp(head, prefixes[i].name))
            break;
    if (i == (int)(sizeof(prefixes) / sizeof(prefixes[0])))
        return 0;
    a->kind = prefixes[i].kind;
    if (a->kind == KA_TAB && !tab_arg_ok(text))
        return 0;
    if (a->kind == KA_OPEN && strcmp(text, "uterm"))
        return 0;
    if (!km_copy(a->arg, KM_ARG, text))
        return -1;
    return 1;
}

/* ---- memory: the arrays grow to the parsed counts, the strings sit in chunks ---- */

#define KM_CHUNK 1024
#define KM_STEP  16

typedef struct km_chunk {
    struct km_chunk *next;
    int used, cap;
    char data[4];
} km_chunk;

static const char km_empty[] = "";

/* A copy of s (at most len bytes, then NUL) in the pool; 0 when out of memory. */
static const char *pool_str(keymap *k, const char *s, int len)
{
    km_chunk *c = k->pool;
    char *p;
    if (len == 0)
        return km_empty;
    if (!c || c->cap - c->used < len + 1) {
        int cap = len + 1 > KM_CHUNK ? len + 1 : KM_CHUNK;
        c = (km_chunk *)KM_MALLOC(sizeof(km_chunk) + (size_t)cap);
        if (!c)
            return 0;
        c->next = k->pool;
        c->used = 0;
        c->cap = cap;
        k->pool = c;
    }
    p = c->data + c->used;
    memcpy(p, s, (size_t)len);
    p[len] = 0;
    c->used += len + 1;
    return p;
}

/* Room for one more of an array (elem bytes each, at most max): 1 ok, 0 full or no memory. */
static int grow(void **arr, int *cap, int n, int max, size_t elem)
{
    void *na;
    int nc;
    if (n < *cap)
        return 1;
    if (n >= max)
        return 0;
    nc = *cap + KM_STEP;
    if (nc > max)
        nc = max;
    na = KM_MALLOC(elem * (size_t)nc);
    if (!na)
        return 0;
    if (*arr) {
        memcpy(na, *arr, elem * (size_t)n);
        KM_FREE(*arr);
    }
    *arr = na;
    *cap = nc;
    return 1;
}

void keymap_free(keymap *k)
{
    km_chunk *c = k->pool;
    while (c) {
        km_chunk *next = c->next;
        KM_FREE(c);
        c = next;
    }
    if (k->bind)
        KM_FREE(k->bind);
    if (k->menu)
        KM_FREE(k->menu);
    memset(k, 0, sizeof(*k));
}

int keymap_compact(keymap *k, int (*keep)(const km_bind *))
{
    keymap t;
    km_chunk *c;
    km_bind *b;
    size_t chars = 0;
    int i, n = 0;
    for (i = 0; i < k->nbind; i++)
        if (!keep || keep(&k->bind[i])) {
            const km_bind *r = &k->bind[i];
            n++;
            chars += strlen(r->arg) + strlen(r->label) + strlen(r->cat) + 3;
        }
    memset(&t, 0, sizeof t);
    t.version = k->version;
    t.newer = k->newer;
    t.skipped = k->skipped;
    t.refused = k->refused;
    t.overflow = k->overflow;
    t.first_bad = k->first_bad;
    if (n) {
        t.bind = (km_bind *)KM_MALLOC(sizeof(km_bind) * (size_t)n);
        c = (km_chunk *)KM_MALLOC(sizeof(km_chunk) + chars);
        if (!t.bind || !c) {
            if (t.bind)
                KM_FREE(t.bind);
            if (c)
                KM_FREE(c);
            return 0;
        }
        c->next = 0;
        c->used = 0;
        c->cap = (int)chars;
        t.pool = c;
        t.bcap = n;
        b = t.bind;
        for (i = 0; i < k->nbind; i++)
            if (!keep || keep(&k->bind[i])) {
                const km_bind *r = &k->bind[i];
                *b = *r;
                b->arg = pool_str(&t, r->arg, (int)strlen(r->arg));
                b->label = pool_str(&t, r->label, (int)strlen(r->label));
                b->cat = pool_str(&t, r->cat, (int)strlen(r->cat));
                b++;
            }
        t.nbind = n;
    }
    keymap_swap(k, &t);
    return 1;
}

void keymap_swap(keymap *live, keymap *scratch)
{
    keymap_free(live);
    *live = *scratch;
    memset(scratch, 0, sizeof(*scratch));
}

/* ---- tables ---- */

static void bad_row(keymap *k, int lineno, int refused, int overflow)
{
    k->skipped++;
    if (refused)
        k->refused++;
    if (overflow)
        k->overflow = 1;
    if (!k->first_bad)
        k->first_bad = lineno;
}

static void bind_remove(keymap *k, int i)
{
    int j;
    for (j = i; j < k->nbind - 1; j++)
        k->bind[j] = k->bind[j + 1];
    k->nbind--;
}

/* 1 stored (or removed); 0 table full or out of memory. */
static int bind_apply(keymap *k, unsigned qual, unsigned code, const action *a,
                      const char *label, const char *cat)
{
    int i;
    km_bind *b = 0;
    const char *sa, *sl, *sc;
    for (i = 0; i < k->nbind; i++)
        if (k->bind[i].qual == qual && k->bind[i].code == code) {
            if (a->kind == KA_NONE) {
                bind_remove(k, i);
                return 1;
            }
            b = &k->bind[i];
            break;
        }
    if (a->kind == KA_NONE)
        return 1;
    sa = pool_str(k, a->arg, (int)strlen(a->arg));
    sl = pool_str(k, label, km_cut(label, KM_LABEL));
    sc = pool_str(k, cat, km_cut(cat, KM_CAT));
    if (!sa || !sl || !sc)
        return 0;
    if (!b) {
        if (!grow((void **)&k->bind, &k->bcap, k->nbind, KM_MAX_BIND, sizeof(km_bind)))
            return 0;
        b = &k->bind[k->nbind++];
    }
    b->qual = (unsigned short)qual;
    b->code = (unsigned char)code;
    b->kind = (unsigned char)a->kind;
    b->arg = sa;
    b->label = sl;
    b->cat = sc;
    return 1;
}

static int menu_apply(keymap *k, const char *id, const action *a, const char *label)
{
    int i;
    km_menu *m = 0;
    const char *si, *sl, *sa;
    for (i = 0; i < k->nmenu; i++)
        if (!strcmp(k->menu[i].id, id)) {
            if (a->kind == KA_NONE) {
                int j;
                for (j = i; j < k->nmenu - 1; j++)
                    k->menu[j] = k->menu[j + 1];
                k->nmenu--;
                return 1;
            }
            m = &k->menu[i];
            break;
        }
    if (a->kind == KA_NONE)
        return 1;
    si = pool_str(k, id, (int)strlen(id));
    sl = pool_str(k, label, km_cut(label, KM_LABEL));
    sa = pool_str(k, a->arg, (int)strlen(a->arg));
    if (!si || !sl || !sa)
        return 0;
    if (!m) {
        if (!grow((void **)&k->menu, &k->mcap, k->nmenu, KM_MAX_MENU, sizeof(km_menu)))
            return 0;
        m = &k->menu[k->nmenu++];
    }
    m->kind = (unsigned char)a->kind;
    m->id = si;
    m->label = sl;
    m->arg = sa;
    return 1;
}

/* ---- rows ---- */

/* The key spec "super+shift+k" / "super+1..9". 1 ok (qual, lo..hi set),
 * 0 bad, 2 refused. */
static int spec_parse(const char *spec, unsigned *qual, int *lo, int *hi, int *range)
{
    char buf[64];
    char *tok, *next;
    char *key = 0;
    int refused = 0, i, n;
    n = (int)strlen(spec);
    if (n <= 0 || n >= (int)sizeof(buf))
        return 0;
    for (i = 0; i <= n; i++)
        buf[i] = km_lower(spec[i]);
    *qual = 0;
    *range = 0;
    for (tok = buf; tok; tok = next) {
        next = strchr(tok, '+');
        if (next)
            *next++ = 0;
        if (!*tok)
            return 0;
        if (!strcmp(tok, "super"))
            *qual |= KM_Q_SUPER;
        else if (!strcmp(tok, "shift"))
            *qual |= KM_Q_SHIFT;
        else if (!strcmp(tok, "ctrl")) {
            *qual |= KM_Q_CTRL;
            refused = 1;
        } else if (!strcmp(tok, "alt"))
            *qual |= KM_Q_ALT;
        else if (!strcmp(tok, "rshift"))
            *qual |= KM_Q_RSHIFT;
        else if (!strcmp(tok, "mouse") || !strcmp(tok, "lmb") || !strcmp(tok, "rmb") || !strcmp(tok, "mmb")) {
            refused = 1;
            if (key)
                return 0;
            key = tok;
        } else {
            if (key)
                return 0;
            key = tok;
        }
    }
    if (!key)
        return 0;
    if (refused)
        return 2;
    if (!(*qual & KM_Q_SUPER))
        return 0;
    if (strlen(key) == 4 && key[1] == '.' && key[2] == '.' && key[0] >= '0' && key[0] <= '9'
        && key[3] >= '0' && key[3] <= '9' && key[0] <= key[3]) {
        *lo = key[0] - '0';
        *hi = key[3] - '0';
        *range = 1;
        return 1;
    }
    if (!strcmp(key, "m") || !strcmp(key, "n"))
        return 2;
    i = keymap_key_code(key);
    if (i < 0)
        return 0;
    *lo = *hi = i;
    return 1;
}

typedef struct kctx {
    keymap *k;                  /* NULL on the version probe */
    int version;
    char *line;                 /* 3 * KM_LINE of row_bind's scratch: heap, not the (small) stack */
} kctx;

static int row_keymap(void *ud, int lineno, const char *key, const char *value)
{
    kctx *c = (kctx *)ud;
    if (km_ieq(key, "version")) {
        int n = 0;
        while (*value >= '0' && *value <= '9')
            n = n * 10 + (*value++ - '0');
        c->version = n;                 /* the last row wins, like upconf */
    } else if (c->k && km_ieq(key, "super") && !km_ieq(value, "lamiga"))
        bad_row(c->k, lineno, 0, 0);
    return 0;
}

static int row_bind(void *ud, int lineno, const char *key, const char *value)
{
    kctx *c = (kctx *)ud;
    keymap *k = c->k;
    char *text = c->line, *act = c->line + KM_LINE, *lab = c->line + 2 * KM_LINE;
    char *label, *cat, *p;
    unsigned qual;
    int lo, hi, range, r, d;
    action a;

    r = spec_parse(key, &qual, &lo, &hi, &range);
    if (r != 1) {
        bad_row(k, lineno, r == 2, 0);
        return 0;
    }
    if (!km_copy(text, KM_LINE, value)) {
        bad_row(k, lineno, 0, 1);
        return 0;
    }
    label = "";
    cat = "";
    p = strchr(text, ';');
    if (p) {
        *p++ = 0;
        label = p;
        p = strchr(label, ';');          /* a second ; starts a trailing comment */
        if (p)
            *p = 0;
        p = strchr(label, '|');
        if (p) {
            *p++ = 0;
            cat = km_trim(p);
        }
        label = km_trim(label);
    }
    for (d = lo; d <= hi; d++) {
        char dc = (char)('0' + d);
        char *action_text = km_trim(text);
        int pr;
        if (range) {
            if (!km_expand(act, KM_LINE, action_text, dc) || !km_expand(lab, KM_LINE, label, dc)) {
                bad_row(k, lineno, 0, 1);
                continue;
            }
            action_text = act;
        } else {
            if (!km_copy(act, KM_LINE, action_text)) {
                bad_row(k, lineno, 0, 1);
                continue;
            }
            action_text = act;
            km_copy(lab, KM_LINE, label);
        }
        pr = action_parse(action_text, &a);
        if (pr != 1) {
            bad_row(k, lineno, 0, pr < 0);
            continue;
        }
        if (range) {
            /* digit d sits at raw code 1..9, 0 at 10 */
            if (!bind_apply(k, qual, (unsigned)(d == 0 ? 0x0A : d), &a, lab, cat))
                bad_row(k, lineno, 0, 1);
        } else if (!bind_apply(k, qual, (unsigned)lo, &a, lab, cat))
            bad_row(k, lineno, 0, 1);
    }
    return 0;
}

static int row_menu(void *ud, int lineno, const char *key, const char *value)
{
    kctx *c = (kctx *)ud;
    keymap *k = c->k;
    char text[KM_LINE];
    char *bar, *label, *act;
    action a;
    int pr;
    if (!*key || (int)strlen(key) >= KM_ID || !km_copy(text, KM_LINE, value)) {
        bad_row(k, lineno, 0, 1);
        return 0;
    }
    bar = strchr(text, '|');
    if (!bar) {
        bad_row(k, lineno, 0, 0);
        return 0;
    }
    *bar++ = 0;
    label = km_trim(text);
    act = km_trim(bar);
    pr = action_parse(act, &a);
    if (pr != 1) {
        bad_row(k, lineno, 0, pr < 0);
        return 0;
    }
    if (!menu_apply(k, key, &a, label))
        bad_row(k, lineno, 0, 1);
    return 0;
}

/* The [keymap] version, 0 when absent; last row wins, like upconf. */
static int version_of(const char *buf, long len)
{
    kctx c;
    c.k = 0;
    c.version = 0;
    c.line = 0;
    upconf_rows(buf, len, "keymap", row_keymap, &c, 0);
    return c.version;
}

/* 1 when the text has a non-blank, non-comment line that is not a section header,
 * or a [keymap]/[bind]/[menu] header is absent: it carries content we can read.
 * Used to tell a junk file from an intentionally empty keymap. */
static int has_known_section(const char *buf, long len)
{
    long i = 0;
    while (i < len) {
        long e = i;
        const char *p;
        while (e < len && buf[e] != '\n')
            e++;
        p = buf + i;
        while (p < buf + e && (*p == ' ' || *p == '\t' || *p == '\r'))
            p++;
        if (p < buf + e && *p == '[') {
            long n = (buf + e) - p;
            if ((n >= 6 && (p[1] | 32) == 'b' && (p[2] | 32) == 'i' && (p[3] | 32) == 'n'
                 && (p[4] | 32) == 'd' && p[5] == ']') ||
                (n >= 6 && (p[1] | 32) == 'm' && (p[2] | 32) == 'e' && (p[3] | 32) == 'n'
                 && (p[4] | 32) == 'u' && p[5] == ']') ||
                (n >= 8 && (p[1] | 32) == 'k' && (p[2] | 32) == 'e' && (p[3] | 32) == 'y'
                 && (p[4] | 32) == 'm' && (p[5] | 32) == 'a' && (p[6] | 32) == 'p' && p[7] == ']'))
                return 1;
        }
        i = e + 1;
    }
    return 0;
}

/* 1 when the text holds a line that is neither blank nor a comment. */
static int has_content(const char *buf, long len)
{
    long i;
    for (i = 0; i < len; i++) {
        char c = buf[i];
        if (c == '#' || c == ';') {
            while (i < len && buf[i] != '\n')
                i++;
        } else if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != 0) {
            return 1;
        }
    }
    return 0;
}

static int load(keymap *k, const char *buf, long len)
{
    kctx c;
    int v, dropped = 0;
    if (!buf)
        return 0;
    v = version_of(buf, len);
    if (v > KM_VERSION)
        return 0;
    c.k = k;
    c.version = v;
    c.line = (char *)KM_MALLOC(3 * KM_LINE);
    if (!c.line)
        return 0;
    if (v)
        k->version = v;
    upconf_rows(buf, len, "keymap", row_keymap, &c, 0);
    upconf_rows(buf, len, "bind", row_bind, &c, &dropped);
    upconf_rows(buf, len, "menu", row_menu, &c, 0);
    KM_FREE(c.line);
    if (dropped) {
        k->skipped += dropped;
        k->overflow = 1;
    }
    /* A junk file is not a keymap: text with no [keymap]/[bind]/[menu] section, or a
     * file whose every row was skipped, yields nothing usable. Refuse it so the caller
     * keeps the old table. An empty file or an empty [bind] section stays legal (0 rows). */
    if (!k->nbind && !k->nmenu &&
        ((has_content(buf, len) && !has_known_section(buf, len)) || k->skipped))
        return 0;
    return 1;
}

int keymap_parse(keymap *k, const char *buf, long len)
{
    int v;
    keymap_free(k);
    if (!buf)
        return 0;
    v = version_of(buf, len);
    if (v > KM_VERSION) {
        k->newer = 1;
        k->version = v;
        return 0;
    }
    return load(k, buf, len);
}

int keymap_overlay(keymap *k, const char *buf, long len)
{
    return load(k, buf, len);
}
