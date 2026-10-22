/* keymap: see keymap.h for the file format and the contract. */
#include <string.h>
#include "upconf.h"
#include "keymap.h"

typedef struct kname {
    const char *name;
    unsigned char code;
} kname;

/* Amiga raw key codes. Letters and digits are board positions. */
static const kname knames[] = {
    { "1", 0x01 }, { "2", 0x02 }, { "3", 0x03 }, { "4", 0x04 }, { "5", 0x05 },
    { "6", 0x06 }, { "7", 0x07 }, { "8", 0x08 }, { "9", 0x09 }, { "0", 0x0A },
    { "minus", 0x0B }, { "plus", 0x0C },
    { "q", 0x10 }, { "w", 0x11 }, { "e", 0x12 }, { "r", 0x13 }, { "t", 0x14 },
    { "y", 0x15 }, { "u", 0x16 }, { "i", 0x17 }, { "o", 0x18 }, { "p", 0x19 },
    { "a", 0x20 }, { "s", 0x21 }, { "d", 0x22 }, { "f", 0x23 }, { "g", 0x24 },
    { "h", 0x25 }, { "j", 0x26 }, { "k", 0x27 }, { "l", 0x28 },
    { "z", 0x31 }, { "x", 0x32 }, { "c", 0x33 }, { "v", 0x34 }, { "b", 0x35 },
    { "n", 0x36 }, { "m", 0x37 }, { "comma", 0x38 }, { "period", 0x39 },
    { "space", 0x40 }, { "backspace", 0x41 }, { "tab", 0x42 }, { "return", 0x44 },
    { "esc", 0x45 }, { "del", 0x46 },
    { "up", 0x4C }, { "down", 0x4D }, { "right", 0x4E }, { "left", 0x4F },
    { "f1", 0x50 }, { "f2", 0x51 }, { "f3", 0x52 }, { "f4", 0x53 }, { "f5", 0x54 },
    { "f6", 0x55 }, { "f7", 0x56 }, { "f8", 0x57 }, { "f9", 0x58 }, { "f10", 0x59 },
    { "help", 0x5F }
};
#define NKNAMES ((int)(sizeof(knames) / sizeof(knames[0])))

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
    for (i = 0; i < NKNAMES; i++)
        if (km_ieq(name, knames[i].name))
            return knames[i].code;
    return -1;
}

const char *keymap_key_name(unsigned code)
{
    int i;
    for (i = 0; i < NKNAMES; i++)
        if (knames[i].code == code)
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

/* Copy and cut to n-1 (a display label may be cut; an action may not). */
static void km_copy_cut(char *dst, int n, const char *src)
{
    int len = (int)strlen(src);
    if (len >= n)
        len = n - 1;
    memcpy(dst, src, (size_t)len);
    dst[len] = 0;
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

/* 1 stored (or removed); 0 table full. */
static int bind_apply(keymap *k, unsigned qual, unsigned code, const action *a,
                      const char *label, const char *cat)
{
    int i;
    km_bind *b = 0;
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
    if (!b) {
        if (k->nbind >= KM_MAX_BIND)
            return 0;
        b = &k->bind[k->nbind++];
    }
    b->qual = (unsigned short)qual;
    b->code = (unsigned char)code;
    b->kind = (unsigned char)a->kind;
    km_copy(b->arg, KM_ARG, a->arg);
    km_copy_cut(b->label, KM_LABEL, label);
    km_copy_cut(b->cat, KM_CAT, cat);
    return 1;
}

static int menu_apply(keymap *k, const char *id, const action *a, const char *label)
{
    int i;
    km_menu *m = 0;
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
    if (!m) {
        if (k->nmenu >= KM_MAX_MENU)
            return 0;
        m = &k->menu[k->nmenu++];
    }
    m->kind = (unsigned char)a->kind;
    km_copy(m->id, KM_ID, id);
    km_copy_cut(m->label, KM_LABEL, label);
    km_copy(m->arg, KM_ARG, a->arg);
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
    char text[KM_LINE];
    char act[KM_LINE], lab[KM_LINE];
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
    upconf_rows(buf, len, "keymap", row_keymap, &c, 0);
    return c.version;
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
    if (v)
        k->version = v;
    upconf_rows(buf, len, "keymap", row_keymap, &c, 0);
    upconf_rows(buf, len, "bind", row_bind, &c, &dropped);
    upconf_rows(buf, len, "menu", row_menu, &c, 0);
    if (dropped) {
        k->skipped += dropped;
        k->overflow = 1;
    }
    return 1;
}

int keymap_parse(keymap *k, const char *buf, long len)
{
    int v;
    memset(k, 0, sizeof(*k));
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
