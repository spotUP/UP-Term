/* hl_lex -- the lexer engine (hl_lex.h). The language tables are in
 * hl_langs.c. */
#include <stdlib.h>
#include <string.h>
#include "hl_lex.h"

/* what a line can start inside */
enum {
    CTX_NONE, CTX_BLOCK, CTX_STR, CTX_LONGSTR, CTX_LONGCMT, CTX_HEREDOC,
    CTX_TAG, CTX_TAGSTR, CTX_MCOMMENT, CTX_CDATA, CTX_DECL, CTX_FENCE, CTX_HUNK
};

/* ---- keyword tables: the blank-separated lists, hashed once -------------- */

typedef struct kwent {
    const char *p;
    unsigned char len, cls;
} kwent;

typedef struct kwtab {
    unsigned long mask;
    int maxlen;                 /* the longest keyword */
    kwent *e;
    unsigned char ct[256];      /* what each byte can be (CT_*) */
} kwtab;

/* A byte's roles in a language. CT_HOT: some rule of the language looks
 * at it; a run of bytes without it is plain text, taken in one step (the
 * lexer's time on a 68020 is in this loop: 100 KB of C must take well
 * under a few seconds). */
#define CT_ID1 1                /* inside a name */
#define CT_ID0 2                /* starts a name */
#define CT_IDX 4                /* an identx character: a name if a letter follows */
#define CT_SPACE 8
#define CT_HOT 16
#define CT_CMT 32               /* starts a comment opener */
#define CT_KW 64                /* starts a keyword */

#define KW_CACHE 64
static kwtab *kwcache[KW_CACHE];

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static void hot(unsigned char *ct, const char *set)
{
    while (set && *set)
        ct[(unsigned char)*set++] |= CT_HOT;
}

static void ct_fill(const hl_lang *L, unsigned char *ct)
{
    int c, k;
    unsigned long f = L->flags;
    for (c = 0; c < 256; c++) {
        int a = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80;
        int d = c >= '0' && c <= '9';
        ct[c] = (unsigned char)((a ? CT_ID0 | CT_ID1 | CT_HOT : 0) | (d ? CT_ID1 | CT_HOT : 0));
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v')
            ct[c] |= CT_SPACE | CT_HOT;
    }
    for (k = 0; L->identx && L->identx[k]; k++)
        ct[(unsigned char)L->identx[k]] |= CT_ID1 | CT_IDX | CT_HOT;
    for (k = 0; k < 2; k++) {
        if (L->lcomment[k])
            ct[(unsigned char)L->lcomment[k][0]] |= CT_CMT | CT_HOT;
        if (L->bopen[k])
            ct[(unsigned char)L->bopen[k][0]] |= CT_CMT | CT_HOT;
    }
    hot(ct, L->quotes);
    hot(ct, ".");
    if (f & HLF_LUALONG)
        hot(ct, "-[");
    if (f & HLF_HEXDOLLAR)
        hot(ct, "$%@");
    if (f & (HLF_DOLLAR | HLF_AMIGADOS))
        hot(ct, "$");
    if (f & HLF_AMIGADOS)
        hot(ct, "<{");
    if (f & HLF_HEREDOC)
        hot(ct, "<");
    if (f & HLF_REGEX)
        hot(ct, "/");
    if (f & HLF_ATSIGN)
        hot(ct, "@");
    if (f & HLF_YAML)
        hot(ct, "&*!");
    if (f & HLF_CSS)
        hot(ct, "{}#.:!");
}

static unsigned long hash(const char *s, int n, int fold)
{
    unsigned long h = 5381;
    int i;
    if (fold)
        for (i = 0; i < n; i++)
            h = ((h << 5) + h) ^ (unsigned long)lower((unsigned char)s[i]);
    else
        for (i = 0; i < n; i++)
            h = ((h << 5) + h) ^ (unsigned long)(unsigned char)s[i];
    return h;
}

static int same(const char *a, const char *b, int n, int fold)
{
    int i;
    if (!fold)
        return !memcmp(a, b, n);
    for (i = 0; i < n; i++)
        if (lower((unsigned char)a[i]) != lower((unsigned char)b[i]))
            return 0;
    return 1;
}

static kwent *kw_slot(kwtab *t, const char *s, int n, int fold)
{
    unsigned long h = hash(s, n, fold) & t->mask;
    for (;;) {
        kwent *e = &t->e[h];
        if (!e->p || (e->len == n && same(e->p, s, n, fold)))
            return e;
        h = (h + 1) & t->mask;
    }
}

static kwtab *kw_build(const hl_lang *L)
{
    kwtab *t;
    int k, count = 0;
    unsigned long size = 16;
    int fold = (L->flags & HLF_NOCASE) != 0;
    for (k = 0; k < 6; k++) {
        const char *p = L->kw[k];
        while (p && *p) {
            while (*p == ' ')
                p++;
            if (*p)
                count++;
            while (*p && *p != ' ')
                p++;
        }
    }
    while (size < (unsigned long)count * 2 + 2)
        size <<= 1;
    t = (kwtab *)malloc(sizeof(kwtab));
    if (!t)
        return 0;
    t->e = (kwent *)calloc(size, sizeof(kwent));
    if (!t->e) {
        free(t);
        return 0;
    }
    t->mask = size - 1;
    t->maxlen = 0;
    ct_fill(L, t->ct);
    for (k = 0; k < 6; k++) {
        const char *p = L->kw[k];
        while (p && *p) {
            const char *w;
            while (*p == ' ')
                p++;
            w = p;
            while (*p && *p != ' ')
                p++;
            if (p > w && p - w < 256) {
                kwent *e = kw_slot(t, w, (int)(p - w), fold);
                if (!e->p) {
                    e->p = w;
                    e->len = (unsigned char)(p - w);
                    e->cls = (unsigned char)(k < 2 ? HL_KEYWORD : k < 4 ? HL_TYPE : HL_BUILTIN);
                    if (p - w > t->maxlen)
                        t->maxlen = (int)(p - w);
                    t->ct[(unsigned char)w[0]] |= CT_KW;
                    if (fold)
                        t->ct[(unsigned char)(w[0] >= 'a' && w[0] <= 'z' ? w[0] - 32 : lower((unsigned char)w[0]))] |= CT_KW;
                }
            }
        }
    }
    return t;
}

void hl_cleanup(void)
{
    int i;
    for (i = 0; i < KW_CACHE; i++)
        if (kwcache[i]) {
            free(kwcache[i]->e);
            free(kwcache[i]);
            kwcache[i] = 0;
        }
}

static kwtab *kw_for(const hl_lang *L)
{
    long i = L - hl_langs;
    if (i < 0 || i >= hl_nlangs || i >= KW_CACHE)
        return 0;
    if (!kwcache[i])
        kwcache[i] = kw_build(L);
    return kwcache[i];
}

/* ---- the line being lexed ------------------------------------------------ */

typedef struct lx {
    hl_state *st;
    const hl_lang *L;
    kwtab *kw;
    const char *s;
    long n, i;
    hl_emit emit;
    void *u;
    int pcls;
    long pst, plen;
    int operand;            /* the last token was a value (no regex can follow) */
    const unsigned char *ct;
} lx;

static void flush(lx *x)
{
    if (x->plen)
        x->emit(x->u, x->pcls, x->s + x->pst, x->plen);
    x->plen = 0;
}

/* s[a..b) is of class cls */
static void put_fn(lx *x, int cls, long a, long b)
{
    if (b <= a)
        return;
    if (x->plen && x->pcls == cls && x->pst + x->plen == a) {
        x->plen += b - a;
        return;
    }
    flush(x);
    x->pcls = cls;
    x->pst = a;
    x->plen = b - a;
}

/* the common case, more text of the class before it, without a call */
#define put(x, cls, a, b)                                                                  \
    do {                                                                                   \
        long a_ = (a), b_ = (b);                                                           \
        if ((x)->plen && (x)->pcls == (cls) && (x)->pst + (x)->plen == a_ && b_ > a_)      \
            (x)->plen += b_ - a_;                                                          \
        else                                                                               \
            put_fn((x), (cls), a_, b_);                                                    \
    } while (0)

/* the rest of the line from x->i is cls */
static void rest(lx *x, int cls)
{
    put(x, cls, x->i, x->n);
    x->i = x->n;
}

static int kw_class(lx *x, long a, long b)
{
    kwent *e;
    if (!x->kw || b - a > x->kw->maxlen || !(x->ct[(unsigned char)x->s[a]] & CT_KW))
        return 0;
    e = kw_slot(x->kw, x->s + a, (int)(b - a), (x->L->flags & HLF_NOCASE) != 0);
    return e->p ? e->cls : 0;
}

static int in(const char *set, int c)
{
    return set && c && strchr(set, c) != 0;
}

/* Macros, not functions: they run for every byte, and a call costs a
 * 68020 more than the test (vbcc does not inline them). Arguments are
 * evaluated more than once: never pass one with a side effect. */
#define is_alpha(c) ((unsigned)(((c) | 32) - 'a') < 26u)
#define is_digit(c) ((unsigned)((c) - '0') < 10u)
#define is_hex(c) (is_digit(c) || (unsigned)(((c) | 32) - 'a') < 6u)
#define is_space(c) ((c) == ' ' || (unsigned)((c) - 9) < 5u)
static int at(lx *x, long i)
{
    return i < x->n && i >= 0 ? (unsigned char)x->s[i] : 0;
}

static int id1(lx *x, int c)
{
    return (x->ct[c & 255] & CT_ID1) != 0;
}

/* can a name start at i */
static int id0(lx *x, long i)
{
    int f;
    if (i >= x->n)
        return 0;
    f = x->ct[(unsigned char)x->s[i]];
    if (f & CT_ID0)
        return 1;
    if (f & CT_IDX) {
        int d = at(x, i + 1);
        return is_alpha(d) || d == '_';
    }
    return 0;
}

static long id_end(lx *x, long i)
{
    const unsigned char *s = (const unsigned char *)x->s;
    i++;
    while (i < x->n && (x->ct[s[i]] & CT_ID1))
        i++;
    return i;
}

static int match(lx *x, long i, const char *w)
{
    long k = (long)strlen(w);
    return k > 0 && i + k <= x->n && !memcmp(x->s + i, w, k);
}

static long skip_space(lx *x, long i)
{
    while (i < x->n && is_space((unsigned char)x->s[i]))
        i++;
    return i;
}

/* ---- the pieces every mode uses ------------------------------------------ */

/* A block comment of pair p from x->i to its end or the line's: from is
 * where the comment token starts. 1 when it closed. */
static int scan_block(lx *x, int p, long from)
{
    const char *op = x->L->bopen[p], *cl = x->L->bclose[p];
    int nest = (x->L->flags & (p ? HLF_NEST2 : HLF_NEST1)) != 0;
    long ol = (long)strlen(op), cll = (long)strlen(cl);
    while (x->i < x->n) {
        char ch = x->s[x->i];
        if (ch != op[0] && ch != cl[0]) {
            x->i++;
        } else if (nest && match(x, x->i, op)) {
            x->st->depth++;
            x->i += ol;
        } else if (match(x, x->i, cl)) {
            x->i += cll;
            if (--x->st->depth <= 0) {
                put(x, HL_COMMENT, from, x->i);
                x->st->ctx = CTX_NONE;
                x->st->depth = 0;
                return 1;
            }
        } else {
            x->i++;
        }
    }
    put(x, HL_COMMENT, from, x->n);
    x->st->ctx = CTX_BLOCK;
    x->st->aux = p;
    return 0;
}

/* a $variable at x->i (shell, make): 1 when there was one */
static int scan_dollar(lx *x)
{
    long i = x->i, j = i + 1;
    int c = at(x, j);
    if (c == '{' || c == '(') {
        int close = c == '{' ? '}' : ')', depth = 0;
        while (j < x->n) {
            int d = (unsigned char)x->s[j];
            if (d == c)
                depth++;
            else if (d == close && --depth == 0) {
                j++;
                break;
            }
            j++;
        }
    } else if (is_alpha(c) || c == '_') {
        while (j < x->n && (is_alpha(at(x, j)) || is_digit(at(x, j)) || at(x, j) == '_'))
            j++;
    } else if (is_digit(c) || in("@*#?$!-<^%+", c)) {
        j++;
    } else {
        return 0;
    }
    put(x, HL_VARIABLE, i, j);
    x->i = j;
    x->operand = 1;
    return 1;
}

/* A string from x->i (after the opening quote) to its close or the line's
 * end; from is where its token starts (the quote, a prefix). q the quote,
 * triple for """, raw without escapes. 1 when it closed. */
static int scan_str(lx *x, int q, int triple, int raw, long from)
{
    const hl_lang *L = x->L;
    int esc = raw ? 0 : (unsigned char)L->esc;
    long st = from;
    x->operand = 1;
    while (x->i < x->n) {
        int c = (unsigned char)x->s[x->i];
        if (esc && c == esc) {
            long e = x->i + 1;
            put(x, HL_STRING, st, x->i);
            if (e >= x->n) {
                /* an escaped newline: the string goes on */
                put(x, HL_ESCAPE, x->i, e);
                x->i = e;
                x->st->ctx = CTX_STR;
                x->st->aux = q;
                x->st->aux2 = (triple ? 1 : 0) | (raw ? 2 : 0);
                return 0;
            }
            e++;
            while (e < x->n && ((unsigned char)x->s[e] & 0xC0) == 0x80)
                e++;
            put(x, HL_ESCAPE, x->i, e);
            x->i = e;
            st = e;
            continue;
        }
        if ((L->flags & HLF_DOLLAR) && c == '$' && q == '"') {
            put(x, HL_STRING, st, x->i);
            if (scan_dollar(x)) {
                st = x->i;
                continue;
            }
            x->i++;
            continue;
        }
        if (c == q) {
            if (triple) {
                if (at(x, x->i + 1) == q && at(x, x->i + 2) == q) {
                    x->i += 3;
                    put(x, HL_STRING, st, x->i);
                    x->st->ctx = CTX_NONE;
                    return 1;
                }
                x->i++;
                continue;
            }
            if ((L->flags & HLF_QQ) && at(x, x->i + 1) == q) {
                put(x, HL_STRING, st, x->i);
                put(x, HL_ESCAPE, x->i, x->i + 2);
                x->i += 2;
                st = x->i;
                continue;
            }
            x->i++;
            put(x, HL_STRING, st, x->i);
            x->st->ctx = CTX_NONE;
            return 1;
        }
        x->i++;
    }
    put(x, HL_STRING, st, x->n);
    if (triple || in(L->mlquotes, q)) {
        x->st->ctx = CTX_STR;
        x->st->aux = q;
        x->st->aux2 = (triple ? 1 : 0) | (raw ? 2 : 0);
    } else {
        x->st->ctx = CTX_NONE;
    }
    return 0;
}

/* where a string opened at i (the quote) closes, honouring escapes; -1
 * when not on this line */
static long str_close(lx *x, long i)
{
    int q = at(x, i);
    long j = i + 1;
    while (j < x->n) {
        int c = (unsigned char)x->s[j];
        if (x->L->esc && c == (unsigned char)x->L->esc) {
            j += 2;
            continue;
        }
        if (c == q)
            return j;
        j++;
    }
    return -1;
}

/* a number at x->i: 1 when there was one */
static int scan_number(lx *x)
{
    long i = x->i, j = i;
    int c = at(x, i);
    if ((x->L->flags & HLF_HEXDOLLAR) && (c == '$' || c == '%' || c == '@')) {
        int ok = c == '$' ? is_hex(at(x, i + 1)) : c == '%' ? (at(x, i + 1) == '0' || at(x, i + 1) == '1')
                                                          : (at(x, i + 1) >= '0' && at(x, i + 1) <= '7');
        if (c == '%' && ok) {
            /* x % 2 is a remainder: a value before it */
            long k = i - 1;
            while (k >= 0 && is_space(at(x, k)))
                k--;
            if (k >= 0 && (id1(x, at(x, k)) || at(x, k) == ')' || at(x, k) == ']'))
                ok = 0;
        }
        if (!ok)
            return 0;
        j = i + 1;
        while (j < x->n && (is_hex(at(x, j)) || at(x, j) == '_'))
            j++;
        put(x, HL_NUMBER, i, j);
        x->i = j;
        x->operand = 1;
        return 1;
    }
    if (!is_digit(c) && !(c == '.' && is_digit(at(x, i + 1))))
        return 0;
    if (c == '0' && (at(x, i + 1) == 'x' || at(x, i + 1) == 'X')) {
        j = i + 2;
        while (j < x->n && (is_hex(at(x, j)) || at(x, j) == '_'))
            j++;
    } else {
        while (j < x->n && (is_digit(at(x, j)) || at(x, j) == '_'))
            j++;
        if (at(x, j) == '.' && is_digit(at(x, j + 1))) {
            j++;
            while (j < x->n && (is_digit(at(x, j)) || at(x, j) == '_'))
                j++;
        } else if (at(x, j) == '.' && !is_alpha(at(x, j + 1)) && at(x, j + 1) != '.' &&
                   x->L->mode == HLM_CODE && !(x->L->flags & (HLF_CSS | HLF_HEXDOLLAR))) {
            j++; /* 1. */
        }
        if ((at(x, j) == 'e' || at(x, j) == 'E') &&
            (is_digit(at(x, j + 1)) || ((at(x, j + 1) == '+' || at(x, j + 1) == '-') && is_digit(at(x, j + 2))))) {
            j += 2;
            while (j < x->n && is_digit(at(x, j)))
                j++;
        }
    }
    /* suffixes and units: 10u 1.5f 3i32 12px 50% */
    while (j < x->n && (is_alpha(at(x, j)) || is_digit(at(x, j)) || at(x, j) == '_'))
        j++;
    if ((x->L->flags & HLF_CSS) && at(x, j) == '%')
        j++;
    if (at(x, j) == '.' && is_alpha(at(x, j + 1))) {
        /* 8.8.font, 1.2.3-beta: a name with digits, not a number */
        while (j < x->n && (id1(x, at(x, j)) || at(x, j) == '.'))
            j++;
        put(x, HL_PLAIN, i, j);
        x->i = j;
        x->operand = 1;
        return 1;
    }
    put(x, HL_NUMBER, i, j);
    x->i = j;
    x->operand = 1;
    return 1;
}

/* Lua's [[ / [==[ at i: its level, -1 if none */
static int long_open(lx *x, long i)
{
    long j = i + 1;
    if (at(x, i) != '[')
        return -1;
    while (at(x, j) == '=')
        j++;
    return at(x, j) == '[' ? (int)(j - i - 1) : -1;
}

static int scan_long(lx *x, int level, int cls, long from)
{
    while (x->i < x->n) {
        if (x->s[x->i] == ']') {
            long j = x->i + 1;
            int k = 0;
            while (at(x, j) == '=') {
                j++;
                k++;
            }
            if (k == level && at(x, j) == ']') {
                x->i = j + 1;
                put(x, cls, from, x->i);
                x->st->ctx = CTX_NONE;
                return 1;
            }
        }
        x->i++;
    }
    put(x, cls, from, x->n);
    x->st->ctx = cls == HL_COMMENT ? CTX_LONGCMT : CTX_LONGSTR;
    x->st->aux = level;
    return 0;
}

/* ---- code ---------------------------------------------------------------- */

/* Picks up what the previous line left open. 0 when the line ended
 * inside it again. */
static int resume(lx *x)
{
    hl_state *st = x->st;
    switch (st->ctx) {
    case CTX_BLOCK:
        return scan_block(x, st->aux, 0);
    case CTX_STR:
        return scan_str(x, st->aux, st->aux2 & 1, (st->aux2 & 2) != 0, 0);
    case CTX_LONGSTR:
        return scan_long(x, st->aux, HL_STRING, 0);
    case CTX_LONGCMT:
        return scan_long(x, st->aux, HL_COMMENT, 0);
    default:
        st->ctx = CTX_NONE;
        return 1;
    }
}

/* A heredoc's line: its text, or the end word. */
static void heredoc_line(lx *x)
{
    hl_state *st = x->st;
    long i = 0, k = (long)strlen(st->hd);
    if (st->hdstrip)
        while (i < x->n && x->s[i] == '\t')
            i++;
    if (x->n - i == k && !memcmp(x->s + i, st->hd, k)) {
        put(x, HL_PLAIN, 0, i);
        put(x, HL_LABEL, i, x->n);
        st->ctx = CTX_NONE;
    } else {
        put(x, HL_STRING, 0, x->n);
    }
    x->i = x->n;
}

/* <<WORD, <<-WORD, <<'WORD', <<"WORD" at x->i */
static int scan_heredoc(lx *x)
{
    long i = x->i, j = i + 2, w;
    int q = 0, strip = 0;
    if (at(x, j) == '<')
        return 0; /* <<< here-string */
    if (at(x, j) == '-') {
        strip = 1;
        j++;
    }
    j = skip_space(x, j);
    if (at(x, j) == '\'' || at(x, j) == '"') {
        q = at(x, j);
        j++;
    }
    w = j;
    while (j < x->n && (is_alpha(at(x, j)) || is_digit(at(x, j)) || at(x, j) == '_'))
        j++;
    if (j == w || j - w >= (long)sizeof(x->st->hd))
        return 0;
    memcpy(x->st->hd, x->s + w, j - w);
    x->st->hd[j - w] = 0;
    if (q && at(x, j) == q)
        j++;
    x->st->hdpend = 1;
    x->st->hdstrip = strip;
    put(x, HL_PLAIN, i, w - (q ? 1 : 0));
    put(x, HL_LABEL, w - (q ? 1 : 0), j);
    x->i = j;
    return 1;
}

/* '#' first on a line in C: the directive, an include's <file> */
static void directive(lx *x, long hash)
{
    long j = hash + 1, w;
    if (at(x, j) == '[' || (at(x, j) == '!' && at(x, j + 1) == '[')) {
        /* Rust's #[attr] and #![attr] */
        int depth = 0;
        while (j < x->n) {
            if (x->s[j] == '[')
                depth++;
            else if (x->s[j] == ']' && --depth == 0) {
                j++;
                break;
            }
            j++;
        }
        put(x, HL_PREPROC, hash, j);
        x->i = j;
        return;
    }
    j = skip_space(x, j);
    w = j;
    while (j < x->n && is_alpha(at(x, j)))
        j++;
    put(x, HL_PREPROC, hash, j);
    x->i = j;
    if ((j - w == 7 && !memcmp(x->s + w, "include", 7)) || (j - w == 6 && !memcmp(x->s + w, "import", 6))) {
        long k = skip_space(x, j);
        if (at(x, k) == '<') {
            long e = k;
            while (e < x->n && x->s[e] != '>')
                e++;
            if (e < x->n) {
                put(x, HL_PLAIN, j, k);
                put(x, HL_STRING, k, e + 1);
                x->i = e + 1;
            }
        }
    }
}

/* ini/toml/yaml/conf: the key of a key = value line */
static void key_line(lx *x, long first)
{
    const hl_lang *L = x->L;
    long i = first, j, e;
    int yaml = (L->flags & HLF_YAML) != 0;
    if (yaml) {
        while (at(x, i) == '-' && (i + 1 >= x->n || is_space(at(x, i + 1)))) {
            put(x, HL_PLAIN, x->i, i);
            put(x, HL_KEYWORD, i, i + 1);
            x->i = i + 1;
            i = skip_space(x, i + 1);
        }
    }
    if (at(x, i) == '"' || at(x, i) == '\'') {
        long c = str_close(x, i);
        if (c > 0) {
            long k = skip_space(x, c + 1);
            if (in(L->keysep, at(x, k)) && (!yaml || k + 1 >= x->n || is_space(at(x, k + 1)))) {
                put(x, HL_PLAIN, x->i, i);
                put(x, HL_KEY, i, c + 1);
                x->i = c + 1;
            }
        }
        return;
    }
    for (j = i; j < x->n; j++) {
        int c = (unsigned char)x->s[j];
        if (in(L->keysep, c)) {
            if (yaml && !(j + 1 >= x->n || is_space(at(x, j + 1))))
                continue;
            break;
        }
        if (c == '"' || c == '\'' || c == '[' || c == '{' || (c == '#' && (j == i || is_space(at(x, j - 1)))) ||
            (c == ';' && !yaml))
            return;
    }
    if (j >= x->n)
        return;
    e = j;
    while (e > i && is_space(at(x, e - 1)))
        e--;
    if (e == i)
        return;
    put(x, HL_PLAIN, x->i, i);
    put(x, HL_KEY, i, e);
    x->i = e;
}

/* Makefile: NAME = value, NAME := value, target: prerequisites */
static void make_line(lx *x)
{
    long j = 0, e;
    int depth = 0;
    if (!x->n || is_space(at(x, 0)) || at(x, 0) == '#')
        return;
    if (id0(x, 0) || at(x, 0) == '-') {
        long w = id_end(x, 0);
        if (kw_class(x, 0, w) == HL_KEYWORD)
            return;
    }
    for (j = 0; j < x->n; j++) {
        int c = (unsigned char)x->s[j];
        if (c == '$' && (at(x, j + 1) == '(' || at(x, j + 1) == '{'))
            depth++, j++;
        else if ((c == ')' || c == '}') && depth)
            depth--;
        else if (depth)
            continue;
        else if (c == '#')
            return;
        else if (c == '=' || ((c == '?' || c == '+' || c == '!') && at(x, j + 1) == '=') ||
                 (c == ':' && (at(x, j + 1) == '=' || (at(x, j + 1) == ':' && at(x, j + 2) == '=')))) {
            e = j;
            while (e > 0 && is_space(at(x, e - 1)))
                e--;
            put(x, HL_KEY, 0, e);
            x->i = e;
            return;
        } else if (c == ':') {
            put(x, HL_LABEL, 0, j + 1);
            x->i = j + 1;
            return;
        }
    }
}

/* CSS: is position i in a selector (outside a rule, or on a line that
 * opens one: "a:hover {", the selectors inside @media) */
static int css_selector(lx *x, long i)
{
    if (x->st->braces == 0)
        return 1;
    for (; i < x->n; i++) {
        if (x->s[i] == '{')
            return 1;
        if (x->s[i] == ';' || x->s[i] == '}')
            return 0;
    }
    return 0;
}

/* an identifier from x->i */
static void ident(lx *x)
{
    const hl_lang *L = x->L;
    long i = x->i, j = id_end(x, i);
    int cls = kw_class(x, i, j);
    if (!cls && x->L->mode == HLM_ASM) {
        long d = j;
        while (d > i + 1 && x->s[d - 1] != '.')
            d--;
        if (d > i + 1)
            cls = kw_class(x, i, d - 1);
    }
    if (cls) {
        put(x, cls, i, j);
        x->i = j;
        x->operand = cls == HL_BUILTIN;
        return;
    }
    if ((L->flags & HLF_STRPREFIX) && j - i <= 2 && in(L->quotes, at(x, j)) && at(x, j) != '`') {
        long k;
        int raw = 0;
        for (k = i; k < j; k++) {
            if (!in("rRbBfFuU", at(x, k)))
                break;
            if (at(x, k) == 'r' || at(x, k) == 'R')
                raw = 1;
        }
        if (k == j) {
            int q = at(x, j), triple = (L->flags & HLF_TRIPLE) && at(x, j + 1) == q && at(x, j + 2) == q;
            x->i = j + (triple ? 3 : 1);
            scan_str(x, q, triple, raw || in(L->rawquotes, q), i);
            return;
        }
    }
    x->i = j;
    x->operand = 1;
    if ((L->flags & HLF_MACROBANG) && at(x, j) == '!' && at(x, j + 1) != '=') {
        put(x, HL_FUNCTION, i, j + 1);
        x->i = j + 1;
        return;
    }
    if (L->flags & HLF_CSS) {
        long k = skip_space(x, j);
        int sel = css_selector(x, i);
        if (!sel && at(x, k) == ':' && at(x, k + 1) != ':')
            put(x, HL_KEY, i, j);
        else if (at(x, k) == '(')
            put(x, HL_FUNCTION, i, j);
        else if (sel)
            put(x, HL_TAG, i, j);
        else
            put(x, HL_PLAIN, i, j);
        return;
    }
    if (L->flags & HLF_FUNC) {
        long k = skip_space(x, j);
        if (at(x, k) == '(') {
            put(x, HL_FUNCTION, i, j);
            return;
        }
    }
    put(x, HL_PLAIN, i, j);
}

/* /regex/flags at x->i: 1 when there was one */
static int scan_regex(lx *x)
{
    long j = x->i + 1;
    int cls = 0;
    if (at(x, j) == '/' || at(x, j) == '*' || at(x, j) == ' ')
        return 0;
    while (j < x->n) {
        int c = (unsigned char)x->s[j];
        if (c == '\\')
            j++;
        else if (c == '[')
            cls = 1;
        else if (c == ']')
            cls = 0;
        else if (c == '/' && !cls)
            break;
        j++;
    }
    if (j >= x->n)
        return 0;
    j++;
    while (j < x->n && is_alpha(at(x, j)))
        j++;
    put(x, HL_STRING, x->i, j);
    x->i = j;
    x->operand = 1;
    return 1;
}

static void lex_code(lx *x)
{
    const hl_lang *L = x->L;
    hl_state *st = x->st;
    long first;
    if (st->ctx == CTX_HEREDOC) {
        heredoc_line(x);
        return;
    }
    if (st->ctx != CTX_NONE) {
        if (!resume(x))
            return;
    } else {
        first = skip_space(x, 0);
        put(x, HL_PLAIN, 0, first);
        x->i = first;
        if (first < x->n) {
            int c = (unsigned char)x->s[first];
            if ((L->flags & HLF_STARCMT) && c == '*') {
                rest(x, HL_COMMENT);
                return;
            }
            if ((L->flags & HLF_CPP) && c == '#')
                directive(x, first);
            else if ((L->flags & HLF_AMIGADOS) && c == '.' && is_alpha(at(x, first + 1))) {
                rest(x, HL_PREPROC);
                return;
            } else if ((L->flags & HLF_SECTION) && c == '[') {
                long e = x->n;
                while (e > first && x->s[e - 1] != ']')
                    e--;
                if (e > first) {
                    put(x, HL_SECTION, first, e);
                    x->i = e;
                }
            } else if (L->flags & HLF_MAKE) {
                make_line(x);
            } else if (L->flags & HLF_KEYLINE) {
                if ((L->flags & HLF_YAML) && (match(x, first, "---") || match(x, first, "...")) &&
                    skip_space(x, first + 3) == x->n) {
                    rest(x, HL_META);
                    return;
                }
                key_line(x, first);
            }
        }
    }
    while (x->i < x->n) {
        long i = x->i;
        int c = (unsigned char)x->s[i];
        int k, lev, f = x->ct[c];
        if (!(f & CT_HOT)) {
            /* operators and punctuation no rule looks at: one plain run */
            long e = i + 1;
            while (e < x->n && !(x->ct[(unsigned char)x->s[e]] & CT_HOT))
                e++;
            put(x, HL_PLAIN, i, e);
            x->i = e;
            c = (unsigned char)x->s[e - 1];
            x->operand = c == ')' || c == ']' || c == '}';
            continue;
        }
        if (f & CT_ID0) {
            ident(x);
            continue;
        }
        if (f & CT_SPACE) {
            long e = skip_space(x, i);
            put(x, HL_PLAIN, i, e);
            x->i = e;
            continue;
        }
        if (L->flags & HLF_LUALONG) {
            if (c == '-' && at(x, i + 1) == '-' && (lev = long_open(x, i + 2)) >= 0) {
                x->i = i + 4 + lev;
                if (!scan_long(x, lev, HL_COMMENT, i))
                    return;
                continue;
            }
            if ((lev = long_open(x, i)) >= 0) {
                x->i = i + 2 + lev;
                if (!scan_long(x, lev, HL_STRING, i))
                    return;
                x->operand = 1;
                continue;
            }
        }
        if (f & CT_CMT) {
            for (k = 0; k < 2; k++)
                if (L->bopen[k] && match(x, i, L->bopen[k]))
                    break;
            if (k < 2) {
                st->depth = 1;
                x->i = i + (long)strlen(L->bopen[k]);
                if (!scan_block(x, k, i))
                    return;
                continue;
            }
            for (k = 0; k < 2; k++)
                if (L->lcomment[k] && match(x, i, L->lcomment[k]) &&
                    !(L->lcomment[k][0] == '#' && (L->flags & HLF_HASHWORD) && i > 0 && !is_space(at(x, i - 1))))
                    break;
            if (k < 2) {
                rest(x, HL_COMMENT);
                return;
            }
        }
        if ((L->flags & HLF_HEXDOLLAR) && (c == '$' || c == '%' || c == '@') && scan_number(x))
            continue;
        if ((L->flags & (HLF_DOLLAR | HLF_AMIGADOS)) && c == '$' && scan_dollar(x))
            continue;
        if ((L->flags & HLF_AMIGADOS) && (c == '<' || c == '{')) {
            long j = i + 1;
            int close = c == '<' ? '>' : '}';
            while (j < x->n && (id1(x, at(x, j)) || at(x, j) == '$' || at(x, j) == '='))
                j++;
            if (j > i + 1 && at(x, j) == close && is_alpha(at(x, i + 1))) {
                put(x, HL_VARIABLE, i, j + 1);
                x->i = j + 1;
                continue;
            }
        }
        if ((L->flags & HLF_HEREDOC) && c == '<' && at(x, i + 1) == '<' && scan_heredoc(x))
            continue;
        if (in(L->quotes, c)) {
            int triple = (L->flags & HLF_TRIPLE) && at(x, i + 1) == c && at(x, i + 2) == c;
            if ((L->flags & HLF_CHARLIT) && c == '\'') {
                unsigned long d = (unsigned char)at(x, i + 1);
                long e = i + 2;
                if (d == '\\') {
                    long cl = str_close(x, i);
                    if (cl > 0 && cl - i <= 12) {
                        x->i = i + 1;
                        scan_str(x, c, 0, 0, i);
                        continue;
                    }
                } else if (d) {
                    while (e < x->n && ((unsigned char)x->s[e] & 0xC0) == 0x80)
                        e++;
                    if (at(x, e) == '\'') {
                        put(x, HL_STRING, i, e + 1);
                        x->i = e + 1;
                        x->operand = 1;
                        continue;
                    }
                }
                if (id0(x, i + 1)) {
                    long j = id_end(x, i + 1);
                    put(x, HL_LABEL, i, j);
                    x->i = j;
                    continue;
                }
                put(x, HL_PLAIN, i, i + 1);
                x->i = i + 1;
                continue;
            }
            if ((L->flags & HLF_KEYSTR) && !triple) {
                long cl = str_close(x, i);
                if (cl > 0 && at(x, skip_space(x, cl + 1)) == ':') {
                    put(x, HL_KEY, i, cl + 1);
                    x->i = cl + 1;
                    continue;
                }
            }
            x->i = i + (triple ? 3 : 1);
            if (!scan_str(x, c, triple, in(L->rawquotes, c), i))
                return;
            continue;
        }
        if ((L->flags & HLF_REGEX) && c == '/' && !x->operand && scan_regex(x))
            continue;
        if (!(L->flags & HLF_CSS) || c != '#') {
            if (!(i > 0 && id1(x, at(x, i - 1)) && c != '.') && scan_number(x))
                continue;
        }
        if ((L->flags & HLF_ATSIGN) && c == '@' && id0(x, i + 1)) {
            long j = id_end(x, i + 1);
            while (at(x, j) == '.' && id0(x, j + 1))
                j = id_end(x, j + 1);
            put(x, HL_PREPROC, i, j);
            x->i = j;
            continue;
        }
        if (L->flags & HLF_YAML) {
            if ((c == '&' || c == '*') && id0(x, i + 1)) {
                long j = id_end(x, i + 1);
                put(x, HL_VARIABLE, i, j);
                x->i = j;
                continue;
            }
            if (c == '!' && (at(x, i + 1) == '!' || id0(x, i + 1))) {
                long j = id_end(x, i + 1);
                put(x, HL_TYPE, i, j);
                x->i = j;
                continue;
            }
        }
        if (L->flags & HLF_CSS) {
            if (c == '{' || c == '}') {
                st->braces += c == '{' ? 1 : (st->braces > 0 ? -1 : 0);
                put(x, HL_PLAIN, i, i + 1);
                x->i = i + 1;
                continue;
            }
            if (c == '#' && !css_selector(x, i) && is_hex(at(x, i + 1))) {
                long j = i + 1;
                while (j < x->n && id1(x, at(x, j)))
                    j++;
                put(x, HL_NUMBER, i, j);
                x->i = j;
                continue;
            }
            if ((c == '#' || c == '.') && css_selector(x, i) && id0(x, i + 1)) {
                long j = id_end(x, i + 1);
                put(x, HL_TYPE, i, j);
                x->i = j;
                continue;
            }
            if (c == ':' && css_selector(x, i) && (id0(x, i + 1) || (at(x, i + 1) == ':' && id0(x, i + 2)))) {
                long j = id_end(x, i + (at(x, i + 1) == ':' ? 2 : 1));
                put(x, HL_BUILTIN, i, j);
                x->i = j;
                continue;
            }
            if (c == '!' && is_alpha(at(x, skip_space(x, i + 1)))) {
                long j = id_end(x, skip_space(x, i + 1));
                put(x, HL_KEYWORD, i, j);
                x->i = j;
                continue;
            }
        }
        if (id0(x, i)) {
            ident(x);
            continue;
        }
        put(x, HL_PLAIN, i, i + 1);
        x->i = i + 1;
        x->operand = c == ')' || c == ']' || c == '}';
    }
}

/* ---- 68k assembler (vasm mot / Devpac) ----------------------------------- */

static void lex_asm(lx *x)
{
    long first = skip_space(x, 0), i, prev = -1;
    int c;
    if (first < x->n && (x->s[first] == ';' || x->s[first] == '*')) {
        rest(x, HL_COMMENT);
        return;
    }
    if (x->n && id0(x, 0)) {
        long j = id_end(x, 0);
        while (at(x, j) == ':')
            j++;
        put(x, HL_LABEL, 0, j);
        x->i = j;
    } else {
        put(x, HL_PLAIN, 0, first);
        x->i = first;
        if (id0(x, first)) {
            long j = id_end(x, first);
            if (at(x, j) == ':') {
                put(x, HL_LABEL, first, j + 1);
                x->i = j + 1;
            }
        }
    }
    i = skip_space(x, x->i);
    put(x, HL_PLAIN, x->i, i);
    x->i = i;
    if (at(x, i) == ';') {
        rest(x, HL_COMMENT);
        return;
    }
    /* the mnemonic or directive; an unknown name is a macro */
    if (id0(x, i)) {
        long j = id_end(x, i), d = j;
        int cls = kw_class(x, i, j);
        while (!cls && d > i + 1 && x->s[d - 1] != '.')
            d--;
        if (!cls && d > i + 1)
            cls = kw_class(x, i, d - 1);
        put(x, cls ? cls : HL_FUNCTION, i, j);
        x->i = j;
    }
    i = skip_space(x, x->i);
    put(x, HL_PLAIN, x->i, i);
    x->i = i;
    /* the operands: up to a blank that no comma is next to; after it, a
     * comment (Devpac and vasm take the rest of the line as one) */
    while (x->i < x->n) {
        i = x->i;
        c = (unsigned char)x->s[i];
        if (c == ';') {
            rest(x, HL_COMMENT);
            return;
        }
        if (is_space(c)) {
            long k = skip_space(x, i);
            put(x, HL_PLAIN, i, k);
            x->i = k;
            if (k < x->n && prev != ',' && at(x, k) != ',' && at(x, k) != ';') {
                rest(x, HL_COMMENT);
                return;
            }
            continue;
        }
        prev = c;
        if (c == '\'' || c == '"') {
            x->i = i + 1;
            scan_str(x, c, 0, 1, i);
            x->st->ctx = CTX_NONE;
            prev = 'x';
            continue;
        }
        if (scan_number(x)) {
            prev = '0';
            continue;
        }
        if (c == '\\' && (is_digit(at(x, i + 1)) || at(x, i + 1) == '@' || is_alpha(at(x, i + 1)))) {
            put(x, HL_VARIABLE, i, i + 2);
            x->i = i + 2;
            continue;
        }
        if (id0(x, i)) {
            long j = id_end(x, i), d = j;
            int cls = kw_class(x, i, j);
            while (!cls && d > i + 1 && x->s[d - 1] != '.')
                d--;
            if (!cls && d > i + 1)
                cls = kw_class(x, i, d - 1);
            put(x, cls == HL_BUILTIN ? HL_BUILTIN : HL_PLAIN, i, j);
            x->i = j;
            prev = 'x';
            continue;
        }
        put(x, HL_PLAIN, i, i + 1);
        x->i = i + 1;
    }
}

/* ---- diff ---------------------------------------------------------------- */

static long count_after(lx *x, long i, long *val)
{
    long v = 0;
    while (i < x->n && is_digit(at(x, i)))
        v = v * 10 + (x->s[i++] - '0');
    *val = v;
    return i;
}

static void lex_diff(lx *x)
{
    hl_state *st = x->st;
    int c = x->n ? (unsigned char)x->s[0] : ' ';
    static const char *const meta[] = { "diff ", "index ", "--- ", "+++ ", "new file", "deleted file",
                                        "old mode", "new mode", "similarity", "rename ", "Binary files",
                                        "Only in ", "commit ", "From ", 0 };
    int k;
    if (st->ctx == CTX_HUNK) {
        if (c == ' ' || c == '+' || c == '-' || c == '\\') {
            if (c == '\\') {
                rest(x, HL_COMMENT);
                return;
            }
            if (c != '+')
                st->aux--;
            if (c != '-')
                st->aux2--;
            if (st->aux <= 0 && st->aux2 <= 0)
                st->ctx = CTX_NONE;
            rest(x, c == '+' ? HL_ADDED : c == '-' ? HL_REMOVED : HL_PLAIN);
            return;
        }
        st->ctx = CTX_NONE;
    }
    if (match(x, 0, "@@")) {
        long i = 2, e, old = 1, nw = 1, v;
        e = 2;
        while (e < x->n && !(x->s[e] == '@' && at(x, e + 1) == '@'))
            e++;
        e = e < x->n ? e + 2 : x->n;
        while (i < e && x->s[i] != '-')
            i++;
        i = count_after(x, i + 1, &v);
        if (at(x, i) == ',')
            count_after(x, i + 1, &old);
        while (i < e && x->s[i] != '+')
            i++;
        i = count_after(x, i + 1, &v);
        if (at(x, i) == ',')
            count_after(x, i + 1, &nw);
        put(x, HL_SECTION, 0, e);
        x->i = e;
        rest(x, HL_PLAIN);
        st->aux = (int)old;
        st->aux2 = (int)nw;
        st->ctx = old > 0 || nw > 0 ? CTX_HUNK : CTX_NONE;
        return;
    }
    for (k = 0; meta[k]; k++)
        if (match(x, 0, meta[k])) {
            rest(x, HL_META);
            return;
        }
    rest(x, c == '+' || c == '>' ? HL_ADDED : c == '-' || c == '<' ? HL_REMOVED : c == '\\' ? HL_COMMENT : HL_PLAIN);
}

/* ---- HTML / XML ---------------------------------------------------------- */

/* the token from `from` runs to `end` (searched from x->i) or the line's
 * end, where ctx_if_open carries it on */
static int find_to(lx *x, long from, const char *end, int cls, int ctx_if_open)
{
    long st = from, k = (long)strlen(end);
    while (x->i < x->n) {
        if (match(x, x->i, end)) {
            x->i += k;
            put(x, cls, st, x->i);
            x->st->ctx = CTX_NONE;
            return 1;
        }
        x->i++;
    }
    put(x, cls, st, x->n);
    x->st->ctx = ctx_if_open;
    return 0;
}

static void lex_markup(lx *x)
{
    hl_state *st = x->st;
    while (x->i < x->n) {
        long i = x->i;
        int c = (unsigned char)x->s[i];
        switch (st->ctx) {
        case CTX_MCOMMENT:
            if (!find_to(x, i, "-->", HL_COMMENT, CTX_MCOMMENT))
                return;
            continue;
        case CTX_CDATA:
            if (!find_to(x, i, "]]>", HL_STRING, CTX_CDATA))
                return;
            continue;
        case CTX_DECL:
            if (!find_to(x, i, ">", HL_PREPROC, CTX_DECL))
                return;
            continue;
        case CTX_TAGSTR:
            while (x->i < x->n && x->s[x->i] != (char)st->aux)
                x->i++;
            if (x->i < x->n) {
                x->i++;
                st->ctx = CTX_TAG;
            }
            put(x, HL_STRING, i, x->i);
            continue;
        case CTX_TAG:
            if (c == '>' || (c == '/' && at(x, i + 1) == '>') || (c == '?' && at(x, i + 1) == '>')) {
                long e = i + (c == '>' ? 1 : 2);
                put(x, HL_TAG, i, e);
                x->i = e;
                st->ctx = CTX_NONE;
            } else if (c == '"' || c == '\'') {
                st->aux = c;
                st->ctx = CTX_TAGSTR;
                x->i = i + 1;
                while (x->i < x->n && x->s[x->i] != (char)c)
                    x->i++;
                if (x->i < x->n) {
                    x->i++;
                    st->ctx = CTX_TAG;
                }
                put(x, HL_STRING, i, x->i);
            } else if (is_alpha(c) || c == '_' || c == ':' || c == '@' || c == '-') {
                long j = i + 1;
                while (j < x->n && (id1(x, at(x, j)) || at(x, j) == '-' || at(x, j) == ':' || at(x, j) == '.'))
                    j++;
                put(x, HL_ATTR, i, j);
                x->i = j;
            } else {
                put(x, HL_PLAIN, i, i + 1);
                x->i = i + 1;
            }
            continue;
        default:
            break;
        }
        if (c == '<') {
            if (match(x, i, "<!--")) {
                x->i = i + 4;
                if (!find_to(x, i, "-->", HL_COMMENT, CTX_MCOMMENT))
                    return;
                continue;
            }
            if (match(x, i, "<![CDATA[")) {
                x->i = i + 9;
                if (!find_to(x, i, "]]>", HL_STRING, CTX_CDATA))
                    return;
                continue;
            }
            if (at(x, i + 1) == '!' || at(x, i + 1) == '?') {
                x->i = i + 2;
                if (!find_to(x, i, ">", HL_PREPROC, CTX_DECL))
                    return;
                continue;
            }
            if (is_alpha(at(x, i + 1)) || (at(x, i + 1) == '/' && is_alpha(at(x, i + 2)))) {
                long j = i + (at(x, i + 1) == '/' ? 2 : 1);
                while (j < x->n && (id1(x, at(x, j)) || at(x, j) == '-' || at(x, j) == ':' || at(x, j) == '.'))
                    j++;
                put(x, HL_TAG, i, j);
                x->i = j;
                st->ctx = CTX_TAG;
                continue;
            }
        }
        if (c == '&') {
            long j = i + 1;
            while (j < x->n && j - i < 12 && (id1(x, at(x, j)) || at(x, j) == '#'))
                j++;
            if (j > i + 1 && at(x, j) == ';') {
                put(x, HL_ESCAPE, i, j + 1);
                x->i = j + 1;
                continue;
            }
        }
        put(x, HL_PLAIN, i, i + 1);
        x->i = i + 1;
    }
}

/* ---- Markdown source ----------------------------------------------------- */

static long run_of(lx *x, long i, int c)
{
    long j = i;
    while (j < x->n && x->s[j] == (char)c)
        j++;
    return j - i;
}

/* the inline part of a Markdown line from x->i */
static void md_inline(lx *x)
{
    while (x->i < x->n) {
        long i = x->i, j;
        int c = (unsigned char)x->s[i];
        if (c == '\\' && i + 1 < x->n && !is_alpha(at(x, i + 1)) && !is_digit(at(x, i + 1)) && at(x, i + 1) != ' ') {
            put(x, HL_ESCAPE, i, i + 2);
            x->i = i + 2;
            continue;
        }
        if (c == '`') {
            long r = run_of(x, i, '`');
            for (j = i + r; j < x->n; j++)
                if (x->s[j] == '`') {
                    long r2 = run_of(x, j, '`');
                    if (r2 == r)
                        break;
                    j += r2 - 1;
                }
            if (j < x->n) {
                put(x, HL_CODE, i, j + r);
                x->i = j + r;
            } else {
                put(x, HL_PLAIN, i, i + r);
                x->i = i + r;
            }
            continue;
        }
        if ((c == '*' || c == '_') && !(c == '_' && i > 0 && id1(x, at(x, i - 1)))) {
            long r = run_of(x, i, c);
            if (r <= 3 && i + r < x->n && !is_space(at(x, i + r))) {
                for (j = i + r; j + r <= x->n; j++)
                    if (x->s[j] == (char)c && run_of(x, j, c) >= r && !is_space(at(x, j - 1)))
                        break;
                if (j + r <= x->n) {
                    put(x, HL_EMPH, i, j + r);
                    x->i = j + r;
                    continue;
                }
            }
            put(x, HL_PLAIN, i, i + r);
            x->i = i + r;
            continue;
        }
        if (c == '[' || (c == '!' && at(x, i + 1) == '[')) {
            long s0 = c == '!' ? i + 1 : i, e = s0 + 1;
            int depth = 1;
            while (e < x->n && depth) {
                if (x->s[e] == '[')
                    depth++;
                else if (x->s[e] == ']')
                    depth--;
                e++;
            }
            if (!depth) {
                put(x, HL_LINK, i, e);
                x->i = e;
                if (at(x, e) == '(' || at(x, e) == '[') {
                    int close = at(x, e) == '(' ? ')' : ']';
                    long k = e + 1;
                    while (k < x->n && x->s[k] != (char)close)
                        k++;
                    if (k < x->n) {
                        put(x, HL_STRING, e, k + 1);
                        x->i = k + 1;
                    }
                } else if (at(x, e) == ':' && i == skip_space(x, 0)) {
                    put(x, HL_PLAIN, e, e + 1);
                    x->i = e + 1;
                    x->i = skip_space(x, x->i);
                    put(x, HL_PLAIN, e + 1, x->i);
                    rest(x, HL_STRING);
                }
                continue;
            }
        }
        if (c == '<') {
            j = i + 1;
            while (j < x->n && x->s[j] != '>' && x->s[j] != '<' && x->s[j] != ' ')
                j++;
            if (at(x, j) == '>' && j > i + 1 && (match(x, i + 1, "http") || strchr("/", at(x, i + 1)) ||
                                                 is_alpha(at(x, i + 1)))) {
                put(x, match(x, i + 1, "http") || match(x, i + 1, "mailto") ? HL_LINK : HL_TAG, i, j + 1);
                x->i = j + 1;
                continue;
            }
            if (is_alpha(at(x, i + 1)) || at(x, i + 1) == '/') {
                j = i + 1;
                while (j < x->n && x->s[j] != '>')
                    j++;
                if (j < x->n) {
                    put(x, HL_TAG, i, j + 1);
                    x->i = j + 1;
                    continue;
                }
            }
        }
        if (c == '|') {
            put(x, HL_META, i, i + 1);
            x->i = i + 1;
            continue;
        }
        put(x, HL_PLAIN, i, i + 1);
        x->i = i + 1;
    }
}

static void lex_markdown(lx *x)
{
    hl_state *st = x->st;
    long first = skip_space(x, 0), r, j;
    int c = at(x, first);
    if (st->ctx == CTX_FENCE) {
        if (first - 0 < 4 && c == st->aux && (r = run_of(x, first, c)) >= st->aux2 &&
            skip_space(x, first + r) == x->n) {
            rest(x, HL_META);
            st->ctx = CTX_NONE;
        } else {
            rest(x, HL_CODE);
        }
        return;
    }
    if (first >= 4) {
        md_inline(x);
        return;
    }
    if ((c == '`' || c == '~') && (r = run_of(x, first, c)) >= 3) {
        rest(x, HL_META);
        st->ctx = CTX_FENCE;
        st->aux = c;
        st->aux2 = (int)r;
        return;
    }
    if (c == '#' && (r = run_of(x, first, '#')) <= 6 && (first + r == x->n || is_space(at(x, first + r)))) {
        rest(x, HL_HEADING);
        return;
    }
    if (c == '-' || c == '*' || c == '_' || c == '=') {
        long k, marks = 0;
        for (k = first; k < x->n; k++) {
            if (x->s[k] == (char)c)
                marks++;
            else if (!is_space(at(x, k)))
                break;
        }
        if (k == x->n && marks >= (c == '=' ? 1 : 3)) {
            rest(x, c == '=' ? HL_HEADING : HL_META);
            return;
        }
    }
    put(x, HL_PLAIN, 0, first);
    x->i = first;
    while (at(x, x->i) == '>') {
        j = x->i + 1;
        if (at(x, j) == ' ')
            j++;
        put(x, HL_COMMENT, x->i, j);
        x->i = j;
    }
    j = skip_space(x, x->i);
    put(x, HL_PLAIN, x->i, j);
    x->i = j;
    c = at(x, j);
    if ((c == '-' || c == '*' || c == '+') && (j + 1 == x->n || at(x, j + 1) == ' ')) {
        put(x, HL_KEYWORD, j, j + 1);
        x->i = j + 1;
    } else if (is_digit(c)) {
        long k = j;
        while (is_digit(at(x, k)))
            k++;
        if ((at(x, k) == '.' || at(x, k) == ')') && (k + 1 == x->n || at(x, k + 1) == ' ')) {
            put(x, HL_KEYWORD, j, k + 1);
            x->i = k + 1;
        }
    }
    j = skip_space(x, x->i);
    if (j > x->i && at(x, j) == '[' && (at(x, j + 1) == ' ' || at(x, j + 1) == 'x' || at(x, j + 1) == 'X') &&
        at(x, j + 2) == ']') {
        put(x, HL_PLAIN, x->i, j);
        put(x, HL_BUILTIN, j, j + 3);
        x->i = j + 3;
    }
    md_inline(x);
}

/* ---- entry points -------------------------------------------------------- */

void hl_begin(hl_state *st, const hl_lang *lang)
{
    memset(st, 0, sizeof(*st));
    st->lang = lang;
}

void hl_line(hl_state *st, const char *s, long n, hl_emit emit, void *u)
{
    lx x;
    x.st = st;
    x.L = st->lang;
    x.s = s;
    x.n = n;
    x.i = 0;
    x.emit = emit;
    x.u = u;
    x.plen = 0;
    x.pcls = HL_PLAIN;
    x.pst = 0;
    x.operand = 0;
    if (!x.L) {
        put(&x, HL_PLAIN, 0, n);
        flush(&x);
        return;
    }
    x.kw = kw_for(x.L);
    if (x.kw) {
        x.ct = x.kw->ct;
    } else {
        /* no memory for the tables: the rules still hold, only slower */
        static unsigned char ct[256];
        ct_fill(x.L, ct);
        x.ct = ct;
    }
    if (st->hdpend && st->ctx == CTX_NONE) {
        st->ctx = CTX_HEREDOC;
        st->hdpend = 0;
    }
    switch (x.L->mode) {
    case HLM_ASM:
        lex_asm(&x);
        break;
    case HLM_DIFF:
        lex_diff(&x);
        break;
    case HLM_MARKUP:
        lex_markup(&x);
        break;
    case HLM_MARKDOWN:
        lex_markdown(&x);
        break;
    default:
        lex_code(&x);
        break;
    }
    if (x.i < n)
        put(&x, HL_PLAIN, x.i, n);
    flush(&x);
}
