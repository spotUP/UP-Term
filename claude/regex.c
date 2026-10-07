/* regex -- see regex.h. A parser to a small tree, the tree compiled to a
 * program (Thompson's construction), the program run as a Pike VM. */
#include <stdlib.h>
#include <string.h>
#include "regex.h"

/* self-contained: C:Claude and vsh both link this file and nothing else of claude/ */
static char *cl_copy(char *dst, const char *src, long cap)
{
    long n = (long)strlen(src);
    if (cap <= 0)
        return dst;
    if (n > cap - 1)
        n = cap - 1;
    memcpy(dst, src, (size_t)n);
    dst[n] = 0;
    return dst;
}

/* one UTF-8 character at s (n bytes left): its length, 0 when it is not well formed */
static int json_utf8(const char *s, long n, unsigned long *cp)
{
    const unsigned char *u = (const unsigned char *)s;
    int k, i;
    unsigned long c;
    if (u[0] < 0x80) {
        *cp = u[0];
        return 1;
    }
    if (u[0] >= 0xc2 && u[0] <= 0xdf) {
        k = 2;
        c = u[0] & 0x1f;
    } else if (u[0] >= 0xe0 && u[0] <= 0xef) {
        k = 3;
        c = u[0] & 0x0f;
    } else if (u[0] >= 0xf0 && u[0] <= 0xf4) {
        k = 4;
        c = u[0] & 0x07;
    } else
        return 0;
    if (n < k)
        return 0;
    for (i = 1; i < k; i++) {
        if ((u[i] & 0xc0) != 0x80)
            return 0;
        c = (c << 6) | (u[i] & 0x3f);
    }
    if ((k == 3 && (c < 0x800 || (c >= 0xd800 && c < 0xe000))) || (k == 4 && (c < 0x10000 || c > 0x10ffff)))
        return 0;
    *cp = c;
    return k;
}

#define RE_MAX_INST 8000
#define RE_MAX_NODE 4000
#define RE_MAX_DEPTH 40

enum { N_CHAR, N_ANY, N_CLASS, N_BOL, N_EOL, N_WB, N_NWB, N_CAT, N_ALT, N_REP, N_EMPTY, N_GROUP };
enum { I_CHAR, I_ANY, I_CLASS, I_BOL, I_EOL, I_WB, I_NWB, I_SPLIT, I_JMP, I_MATCH, I_SAVE };

typedef struct node {
    int type;
    long a, b;                  /* children, or the code point / class index */
    int min, max;               /* N_REP; max -1 unbounded */
} node;

typedef struct rclass {
    int neg;
    int shneg;                  /* bits: 1 \D, 2 \W, 4 \S inside the class */
    long first, count;          /* ranges in the range pool */
} rclass;

typedef struct thr {
    long pc, start;
} thr;

typedef struct inst {
    int op;
    long x, y;
} inst;

struct cl_re {
    int flags;
    int ngroups;                /* capture groups, group 0 (the whole match) not counted */
    inst *prog;
    long np;
    rclass *cls;
    int ncls;
    unsigned long *rng;         /* lo, hi pairs */
    long nrng;
    /* the VM's work space, made once (a search per line allocates nothing) */
    long *mark, *stack, gen;
    struct thr *l1, *l2;
};

typedef struct parser {
    const char *p, *e;
    node *nodes;
    long nn;
    rclass *cls;
    int ncls, ccap;
    unsigned long *rng;
    long nrng, rcap;
    int flags;
    int depth;
    int ngroups;
    char *err;
    long errcap;
    int bad;
} parser;

static int fail(parser *ps, const char *why)
{
    if (!ps->bad)
        cl_copy(ps->err, why, ps->errcap);
    ps->bad = 1;
    return -1;
}

static long mk(parser *ps, int type, long a, long b)
{
    node *n;
    if (ps->nn >= RE_MAX_NODE) {
        fail(ps, "the pattern is too long");
        return -1;
    }
    n = &ps->nodes[ps->nn];
    n->type = type;
    n->a = a;
    n->b = b;
    n->min = n->max = 0;
    return ps->nn++;
}

/* the next code point of the pattern */
static unsigned long pchar(parser *ps)
{
    unsigned long cp;
    int l = json_utf8(ps->p, (long)(ps->e - ps->p), &cp);
    if (!l) {
        cp = (unsigned char)*ps->p;
        l = 1;
    }
    ps->p += l;
    return cp;
}

static int add_range(parser *ps, unsigned long lo, unsigned long hi)
{
    if (ps->nrng + 2 > ps->rcap) {
        long c = ps->rcap ? ps->rcap * 2 : 64;
        unsigned long *r = (unsigned long *)realloc(ps->rng, (size_t)c * sizeof(unsigned long));
        if (!r)
            return fail(ps, "out of memory");
        ps->rng = r;
        ps->rcap = c;
    }
    ps->rng[ps->nrng++] = lo;
    ps->rng[ps->nrng++] = hi;
    ps->cls[ps->ncls - 1].count++;
    return 0;
}

static int new_class(parser *ps, int neg)
{
    rclass *c;
    if (ps->ncls == ps->ccap) {
        int nc = ps->ccap ? ps->ccap * 2 : 8;
        rclass *x = (rclass *)realloc(ps->cls, (size_t)nc * sizeof(rclass));
        if (!x)
            return fail(ps, "out of memory");
        ps->cls = x;
        ps->ccap = nc;
    }
    c = &ps->cls[ps->ncls++];
    c->neg = neg;
    c->shneg = 0;
    c->first = ps->nrng / 2;       /* in pairs */
    c->count = 0;
    return ps->ncls - 1;
}

/* \d \w \s into the class being built */
static int shorthand(parser *ps, int c)
{
    switch (c) {
    case 'd':
        return add_range(ps, '0', '9');
    case 'w':
        return add_range(ps, 'a', 'z') || add_range(ps, 'A', 'Z') || add_range(ps, '0', '9') ||
               add_range(ps, '_', '_') || add_range(ps, 0xc0, 0xd6) || add_range(ps, 0xd8, 0xf6) ||
               add_range(ps, 0xf8, 0x10ffff);
    case 's':
        return add_range(ps, ' ', ' ') || add_range(ps, '\t', '\r');
    }
    return -1;
}

static int posix_class(parser *ps)
{
    static const char *const names[] = { "alpha", "digit", "alnum", "space", "upper", "lower", "punct",
                                         "xdigit", "word", "blank", 0 };
    int i;
    for (i = 0; names[i]; i++) {
        long l = (long)strlen(names[i]);
        if (ps->e - ps->p >= l + 4 && !memcmp(ps->p + 2, names[i], (size_t)l) && ps->p[2 + l] == ':' &&
            ps->p[3 + l] == ']') {
            ps->p += 4 + l;
            switch (i) {
            case 0:
                return add_range(ps, 'a', 'z') || add_range(ps, 'A', 'Z');
            case 1:
                return add_range(ps, '0', '9');
            case 2:
                return add_range(ps, 'a', 'z') || add_range(ps, 'A', 'Z') || add_range(ps, '0', '9');
            case 3:
                return shorthand(ps, 's');
            case 4:
                return add_range(ps, 'A', 'Z');
            case 5:
                return add_range(ps, 'a', 'z');
            case 6:
                return add_range(ps, '!', '/') || add_range(ps, ':', '@') || add_range(ps, '[', '`') ||
                       add_range(ps, '{', '~');
            case 7:
                return add_range(ps, '0', '9') || add_range(ps, 'a', 'f') || add_range(ps, 'A', 'F');
            case 8:
                return shorthand(ps, 'w');
            default:
                return add_range(ps, ' ', ' ') || add_range(ps, '\t', '\t');
            }
        }
    }
    return fail(ps, "unknown [:class:] in a bracket expression");
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* a simple escape's code point, or -1 */
static long esc_char(parser *ps, int c)
{
    switch (c) {
    case 't':
        return '\t';
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 'f':
        return '\f';
    case 'v':
        return '\v';
    case 'a':
        return 7;
    case 'e':
        return 27;
    case 'x': {
        long v = 0;
        int k = 0;
        if (ps->p < ps->e && *ps->p == '{') {
            ps->p++;
            while (ps->p < ps->e && *ps->p != '}' && hexval((unsigned char)*ps->p) >= 0 && k < 6) {
                v = v * 16 + hexval((unsigned char)*ps->p++);
                k++;
            }
            if (ps->p >= ps->e || *ps->p != '}')
                return -2;
            ps->p++;
            return v;
        }
        while (k < 2 && ps->p < ps->e && hexval((unsigned char)*ps->p) >= 0) {
            v = v * 16 + hexval((unsigned char)*ps->p++);
            k++;
        }
        return k ? v : -2;
    }
    }
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
        return -1;
    return c;
}

static long parse_class(parser *ps)
{
    int idx, first = 1;
    unsigned long lo;
    ps->p++;                        /* '[' */
    idx = new_class(ps, ps->p < ps->e && *ps->p == '^');
    if (idx < 0)
        return -1;
    if (ps->cls[idx].neg)
        ps->p++;
    for (;;) {
        if (ps->p >= ps->e) {
            fail(ps, "a [ without its ]");
            return -1;
        }
        if (*ps->p == ']' && !first)
            break;
        first = 0;
        if (*ps->p == '[' && ps->p + 1 < ps->e && ps->p[1] == ':') {
            if (posix_class(ps))
                return -1;
            continue;
        }
        if (*ps->p == '\\' && ps->p + 1 < ps->e && !(ps->flags & RE_POSIX)) {
            int c = (unsigned char)ps->p[1];
            long v;
            ps->p += 2;
            if (c == 'd' || c == 'w' || c == 's') {
                if (shorthand(ps, c))
                    return -1;
                continue;
            }
            if (c == 'D' || c == 'W' || c == 'S') {
                ps->cls[idx].shneg |= c == 'D' ? 1 : c == 'W' ? 2 : 4;
                continue;
            }
            v = esc_char(ps, c);
            if (v < 0) {
                fail(ps, "an unknown escape in a bracket expression");
                return -1;
            }
            lo = (unsigned long)v;
        } else
            lo = pchar(ps);
        if (ps->p + 1 < ps->e && *ps->p == '-' && ps->p[1] != ']') {
            unsigned long hi;
            ps->p++;
            if (*ps->p == '\\' && ps->p + 1 < ps->e && !(ps->flags & RE_POSIX)) {
                long v;
                int c = (unsigned char)ps->p[1];
                ps->p += 2;
                v = esc_char(ps, c);
                if (v < 0) {
                    fail(ps, "an unknown escape in a range");
                    return -1;
                }
                hi = (unsigned long)v;
            } else
                hi = pchar(ps);
            if (hi < lo) {
                fail(ps, "a range out of order in a bracket expression");
                return -1;
            }
            if (add_range(ps, lo, hi))
                return -1;
        } else if (add_range(ps, lo, lo))
            return -1;
    }
    ps->p++;                        /* ']' */
    return mk(ps, N_CLASS, idx, 0);
}

static long parse_alt(parser *ps);

static long parse_atom(parser *ps)
{
    int c = (unsigned char)*ps->p;
    if (c == '(') {
        long n;
        int grp = 0;
        ps->p++;
        if (ps->p < ps->e && *ps->p == '?' && !(ps->flags & RE_POSIX)) {
            if (ps->p + 1 < ps->e && ps->p[1] == ':')
                ps->p += 2;
            else if (ps->p + 2 < ps->e && ps->p[1] == 'i' && ps->p[2] == ')') {
                ps->p += 3;
                ps->flags |= RE_ICASE;
                return mk(ps, N_EMPTY, 0, 0);
            } else if (ps->p + 1 < ps->e && (ps->p[1] == '=' || ps->p[1] == '!' || ps->p[1] == '<')) {
                fail(ps, "look-around ((?=, (?!, (?<) is not supported");
                return -1;
            } else {
                fail(ps, "an unsupported (? group");
                return -1;
            }
        } else
            grp = ++ps->ngroups;
        if (++ps->depth > RE_MAX_DEPTH) {
            fail(ps, "the groups nest too deep");
            return -1;
        }
        n = parse_alt(ps);
        ps->depth--;
        if (n < 0)
            return -1;
        if (ps->p >= ps->e || *ps->p != ')') {
            fail(ps, "a ( without its )");
            return -1;
        }
        ps->p++;
        return grp ? mk(ps, N_GROUP, n, grp) : n;
    }
    if (c == '[')
        return parse_class(ps);
    if (c == '.') {
        ps->p++;
        return mk(ps, N_ANY, 0, 0);
    }
    if (c == '^') {
        ps->p++;
        return mk(ps, N_BOL, 0, 0);
    }
    if (c == '$') {
        ps->p++;
        return mk(ps, N_EOL, 0, 0);
    }
    if (c == '\\') {
        long v;
        if (ps->p + 1 >= ps->e) {
            fail(ps, "a trailing backslash");
            return -1;
        }
        c = (unsigned char)ps->p[1];
        ps->p += 2;
        if ((ps->flags & RE_POSIX) && !(c >= '1' && c <= '9'))
            return mk(ps, N_CHAR, c, 0);   /* POSIX ERE: a backslash makes the next character itself */
        if (c == 'b')
            return mk(ps, N_WB, 0, 0);
        if (c == 'B')
            return mk(ps, N_NWB, 0, 0);
        if (c == 'd' || c == 'w' || c == 's' || c == 'D' || c == 'W' || c == 'S') {
            int idx = new_class(ps, c == 'D' || c == 'W' || c == 'S');
            if (idx < 0 || shorthand(ps, c | 0x20))
                return -1;
            return mk(ps, N_CLASS, idx, 0);
        }
        if (c >= '1' && c <= '9') {
            fail(ps, "back references (\\1) are not supported");
            return -1;
        }
        v = esc_char(ps, c);
        if (v < 0) {
            fail(ps, "an unknown escape");
            return -1;
        }
        return mk(ps, N_CHAR, v, 0);
    }
    if (c == '*' || c == '+' || c == '?') {
        fail(ps, "a repetition with nothing before it");
        return -1;
    }
    return mk(ps, N_CHAR, (long)pchar(ps), 0);
}

static int number(parser *ps, int *v)
{
    int k = 0;
    *v = 0;
    while (ps->p < ps->e && *ps->p >= '0' && *ps->p <= '9' && k < 5) {
        *v = *v * 10 + (*ps->p++ - '0');
        k++;
    }
    return k;
}

static long parse_repeat(parser *ps)
{
    long a = parse_atom(ps);
    while (a >= 0 && ps->p < ps->e) {
        int min, max;
        char c = *ps->p;
        if (c == '*') {
            min = 0;
            max = -1;
            ps->p++;
        } else if (c == '+') {
            min = 1;
            max = -1;
            ps->p++;
        } else if (c == '?') {
            min = 0;
            max = 1;
            ps->p++;
        } else if (c == '{') {
            const char *save = ps->p;
            ps->p++;
            if (!number(ps, &min)) {
                ps->p = save;       /* a literal '{' */
                break;
            }
            max = min;
            if (ps->p < ps->e && *ps->p == ',') {
                ps->p++;
                if (!number(ps, &max))
                    max = -1;
            }
            if (ps->p >= ps->e || *ps->p != '}') {
                ps->p = save;
                break;
            }
            ps->p++;
            if ((max >= 0 && max < min) || min > 1000 || max > 1000) {
                fail(ps, "a {m,n} repetition out of range");
                return -1;
            }
        } else
            break;
        if (ps->p < ps->e && (*ps->p == '?' || *ps->p == '+') && !(ps->flags & RE_POSIX))
            ps->p++;                /* lazy and possessive forms: the same text */
        a = mk(ps, N_REP, a, 0);
        if (a >= 0) {
            ps->nodes[a].min = min;
            ps->nodes[a].max = max;
        }
    }
    return a;
}

static long parse_cat(parser *ps)
{
    long r = -2;
    while (ps->p < ps->e && *ps->p != '|' && *ps->p != ')') {
        long a = parse_repeat(ps);
        if (a < 0)
            return -1;
        r = r == -2 ? a : mk(ps, N_CAT, r, a);
        if (r < 0)
            return -1;
    }
    return r == -2 ? mk(ps, N_EMPTY, 0, 0) : r;
}

static long parse_alt(parser *ps)
{
    long a = parse_cat(ps);
    while (a >= 0 && ps->p < ps->e && *ps->p == '|') {
        long b;
        ps->p++;
        b = parse_cat(ps);
        if (b < 0)
            return -1;
        a = mk(ps, N_ALT, a, b);
    }
    return a;
}

/* ---- the program ---- */

typedef struct emitter {
    inst *prog;
    long n;
    const node *nodes;
    int bad;
    int depth;
} emitter;

static long emit1(emitter *e, int op, long x, long y)
{
    if (e->n >= RE_MAX_INST) {
        e->bad = 1;
        return 0;
    }
    e->prog[e->n].op = op;
    e->prog[e->n].x = x;
    e->prog[e->n].y = y;
    return e->n++;
}

static void emit(emitter *e, long ni)
{
    const node *n = &e->nodes[ni];
    long s, j;
    int k;
    if (e->bad || ++e->depth > 200) {
        e->bad = 1;
        return;
    }
    switch (n->type) {
    case N_CHAR:
        emit1(e, I_CHAR, n->a, 0);
        break;
    case N_ANY:
        emit1(e, I_ANY, 0, 0);
        break;
    case N_CLASS:
        emit1(e, I_CLASS, n->a, 0);
        break;
    case N_BOL:
        emit1(e, I_BOL, 0, 0);
        break;
    case N_EOL:
        emit1(e, I_EOL, 0, 0);
        break;
    case N_WB:
        emit1(e, I_WB, 0, 0);
        break;
    case N_NWB:
        emit1(e, I_NWB, 0, 0);
        break;
    case N_EMPTY:
        break;
    case N_GROUP:
        emit1(e, I_SAVE, 2 * n->b, 0);
        emit(e, n->a);
        emit1(e, I_SAVE, 2 * n->b + 1, 0);
        break;
    case N_CAT:
        emit(e, n->a);
        emit(e, n->b);
        break;
    case N_ALT:
        s = emit1(e, I_SPLIT, 0, 0);
        e->prog[s].x = e->n;
        emit(e, n->a);
        j = emit1(e, I_JMP, 0, 0);
        e->prog[s].y = e->n;
        emit(e, n->b);
        e->prog[j].x = e->n;
        break;
    case N_REP:
        for (k = 0; k < n->min && !e->bad; k++)
            emit(e, n->a);
        if (n->max < 0) {
            s = emit1(e, I_SPLIT, 0, 0);
            e->prog[s].x = e->n;
            emit(e, n->a);
            emit1(e, I_JMP, s, 0);
            e->prog[s].y = e->n;
        } else {
            /* the optional copies' splits, chained through y until the end is known */
            long chain = -1;
            for (k = n->min; k < n->max && !e->bad; k++) {
                s = emit1(e, I_SPLIT, 0, chain);
                chain = s;
                e->prog[s].x = e->n;
                emit(e, n->a);
            }
            while (chain >= 0 && !e->bad) {
                long next = e->prog[chain].y;
                e->prog[chain].y = e->n;
                chain = next;
            }
        }
        break;
    }
    e->depth--;
}

cl_re *re_compile(const char *pat, int flags, char *err, long cap)
{
    parser ps;
    emitter em;
    cl_re *re;
    long root;
    memset(&ps, 0, sizeof(ps));
    ps.p = pat;
    ps.e = pat + strlen(pat);
    ps.flags = flags;
    ps.err = err;
    ps.errcap = cap;
    err[0] = 0;
    ps.nodes = (node *)malloc(sizeof(node) * RE_MAX_NODE);
    if (!ps.nodes) {
        cl_copy(err, "out of memory", cap);
        return 0;
    }
    root = parse_alt(&ps);
    if (root >= 0 && ps.p < ps.e)
        fail(&ps, "a ) without its (");
    re = 0;
    if (root >= 0 && !ps.bad) {
        memset(&em, 0, sizeof(em));
        em.prog = (inst *)malloc(sizeof(inst) * RE_MAX_INST);
        em.nodes = ps.nodes;
        if (em.prog) {
            emit(&em, root);
            emit1(&em, I_MATCH, 0, 0);
        }
        if (!em.prog || em.bad) {
            free(em.prog);
            cl_copy(err, em.prog ? "the pattern is too large (its repetitions)" : "out of memory", cap);
        } else {
            re = (cl_re *)malloc(sizeof(cl_re));
            if (re) {
                re->flags = ps.flags;
                re->ngroups = ps.ngroups;
                re->prog = (inst *)realloc(em.prog, sizeof(inst) * (size_t)em.n);
                if (!re->prog)
                    re->prog = em.prog;
                re->np = em.n;
                re->cls = ps.cls;
                re->ncls = ps.ncls;
                re->rng = ps.rng;
                re->nrng = ps.nrng;
                ps.cls = 0;
                ps.rng = 0;
                re->gen = 0;
                re->mark = (long *)calloc((size_t)re->np, sizeof(long));
                re->stack = (long *)malloc(sizeof(long) * ((size_t)re->np * 2 + 8));
                re->l1 = (thr *)malloc(sizeof(thr) * (size_t)re->np);
                re->l2 = (thr *)malloc(sizeof(thr) * (size_t)re->np);
                if (!re->mark || !re->stack || !re->l1 || !re->l2) {
                    re_free(re);
                    re = 0;
                    cl_copy(err, "out of memory", cap);
                }
            } else {
                free(em.prog);
                cl_copy(err, "out of memory", cap);
            }
        }
    }
    free(ps.nodes);
    free(ps.cls);
    free(ps.rng);
    return re;
}

void re_free(cl_re *re)
{
    if (!re)
        return;
    free(re->prog);
    free(re->cls);
    free(re->rng);
    free(re->mark);
    free(re->stack);
    free(re->l1);
    free(re->l2);
    free(re);
}

/* ---- matching ---- */

static unsigned long fold(unsigned long c)
{
    if (c >= 'A' && c <= 'Z')
        return c + 32;
    if (c >= 0xc0 && c <= 0xde && c != 0xd7)
        return c + 32;
    return c;
}

static unsigned long upper(unsigned long c)
{
    if (c >= 'a' && c <= 'z')
        return c - 32;
    if (c >= 0xe0 && c <= 0xfe && c != 0xf7)
        return c - 32;
    return c;
}

static int is_word_cp(unsigned long c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
           (c >= 0xc0 && c != 0xd7 && c != 0xf7);
}

static int in_ranges(const cl_re *re, const rclass *c, unsigned long cp)
{
    long i;
    for (i = 0; i < c->count; i++)
        if (cp >= re->rng[(c->first + i) * 2] && cp <= re->rng[(c->first + i) * 2 + 1])
            return 1;
    if ((c->shneg & 1) && !(cp >= '0' && cp <= '9'))
        return 1;
    if ((c->shneg & 2) && !is_word_cp(cp))
        return 1;
    if ((c->shneg & 4) && !(cp == ' ' || (cp >= '\t' && cp <= '\r')))
        return 1;
    return 0;
}

static int class_has(const cl_re *re, int idx, unsigned long cp)
{
    const rclass *c = &re->cls[idx];
    int in = in_ranges(re, c, cp);
    if (!in && (re->flags & RE_ICASE))
        in = in_ranges(re, c, fold(cp)) || in_ranges(re, c, upper(cp));
    return c->neg ? !in : in;
}

/* the character at s[i]: its code point, its length */
static int cp_at(const char *s, long n, long i, unsigned long *cp)
{
    int l = json_utf8(s + i, n - i, cp);
    if (!l) {
        *cp = (unsigned char)s[i];
        l = 1;
    }
    return l;
}

/* the character before position i (byte view: enough for words and lines) */
static unsigned long cp_before(const char *s, long i)
{
    long k = i - 1;
    unsigned long cp;
    if (i <= 0)
        return 0;
    while (k > 0 && i - k < 4 && ((unsigned char)s[k] & 0xc0) == 0x80)
        k--;
    if (json_utf8(s + k, i - k, &cp) == (int)(i - k))
        return cp;
    return (unsigned char)s[i - 1];
}


typedef struct vm {
    cl_re *re;
    const char *s;
    long n;
} vm;

static int assert_ok(vm *v, int op, long pos)
{
    unsigned long a = cp_before(v->s, pos), b = 0;
    int wa, wb;
    if (op == I_BOL)
        return pos == 0 || (!(v->re->flags & RE_POSIX) && v->s[pos - 1] == '\n');
    if (op == I_EOL && (v->re->flags & RE_POSIX))
        return pos == v->n;
    if (op == I_EOL)
        return pos == v->n || v->s[pos] == '\n' || (v->s[pos] == '\r' && (pos + 1 == v->n || v->s[pos + 1] == '\n'));
    if (pos < v->n)
        cp_at(v->s, v->n, pos, &b);
    wa = pos > 0 && is_word_cp(a);
    wb = pos < v->n && is_word_cp(b);
    return op == I_WB ? wa != wb : wa == wb;
}

/* the thread at pc, its empty moves followed, into list l (priority order) */
static void add(vm *v, thr *l, long *ln, long pc0, long pos, long start)
{
    long sp = 0;
    v->re->stack[sp++] = pc0;
    while (sp) {
        long pc = v->re->stack[--sp];
        const inst *in;
        if (v->re->mark[pc] == v->re->gen)
            continue;
        v->re->mark[pc] = v->re->gen;
        in = &v->re->prog[pc];
        switch (in->op) {
        case I_JMP:
            v->re->stack[sp++] = in->x;
            break;
        case I_SPLIT:
            v->re->stack[sp++] = in->y;
            v->re->stack[sp++] = in->x;
            break;
        case I_SAVE:
            v->re->stack[sp++] = pc + 1;
            break;
        case I_BOL:
        case I_EOL:
        case I_WB:
        case I_NWB:
            if (assert_ok(v, in->op, pos))
                v->re->stack[sp++] = pc + 1;
            break;
        default:
            l[*ln].pc = pc;
            l[*ln].start = start;
            (*ln)++;
            break;
        }
    }
}

int re_search(const cl_re *cre, const char *s, long n, long from, long *ms, long *me)
{
    cl_re *re = (cl_re *)cre;      /* only its work space changes */
    vm v;
    thr *cl = re->l1, *nl = re->l2, *tmp;
    long cn = 0, nn, pos = from, i;
    int matched = 0;
    v.re = re;
    v.s = s;
    v.n = n;
    if (re->gen > 0x3fffffffL) {
        memset(re->mark, 0, sizeof(long) * (size_t)re->np);
        re->gen = 0;
    }
    re->gen++;
    for (;;) {
        unsigned long cp = 0, fc = 0;
        int len = 0;
        if (!matched)
            add(&v, cl, &cn, 0, pos, pos);
        if (pos < n) {
            len = cp_at(s, n, pos, &cp);
            fc = (re->flags & RE_ICASE) ? fold(cp) : cp;
        }
        if (!cn) {
            /* nothing alive: matched before (done), or no match starts here */
            if (matched || pos >= n)
                break;
            pos += len;
            re->gen++;
            continue;
        }
        re->gen++;
        nn = 0;
        for (i = 0; i < cn; i++) {
            const inst *in = &re->prog[cl[i].pc];
            int ok = 0;
            if (in->op == I_MATCH) {
                matched = 1;
                *ms = cl[i].start;
                *me = pos;
                break;              /* the lower-priority threads are cut */
            }
            if (pos >= n)
                continue;
            if (in->op == I_CHAR)
                ok = (re->flags & RE_ICASE) ? fold((unsigned long)in->x) == fc : (unsigned long)in->x == cp;
            else if (in->op == I_ANY)
                ok = cp != '\n' || (re->flags & (RE_DOTALL | RE_POSIX));
            else if (in->op == I_CLASS)
                ok = class_has(re, (int)in->x, cp);
            if (ok)
                add(&v, nl, &nn, cl[i].pc + 1, pos + len, cl[i].start);
        }
        tmp = cl;
        cl = nl;
        nl = tmp;
        cn = nn;
        if (pos >= n)
            break;
        pos += len;
    }
    return matched;
}

/* ---- captures: a Pike VM whose threads carry their group positions ---- */

typedef struct gthr {
    long pc;
    long *cap;
} gthr;

static void gadd(vm *v, gthr *l, long *pool, long c2, long *ln, long pc, long pos, long *cap)
{
    const inst *in;
    if (v->re->mark[pc] == v->re->gen)
        return;
    v->re->mark[pc] = v->re->gen;
    in = &v->re->prog[pc];
    switch (in->op) {
    case I_JMP:
        gadd(v, l, pool, c2, ln, in->x, pos, cap);
        break;
    case I_SPLIT:
        gadd(v, l, pool, c2, ln, in->x, pos, cap);
        gadd(v, l, pool, c2, ln, in->y, pos, cap);
        break;
    case I_SAVE: {
        long old = cap[in->x];
        cap[in->x] = pos;
        gadd(v, l, pool, c2, ln, pc + 1, pos, cap);
        cap[in->x] = old;
        break;
    }
    case I_BOL:
    case I_EOL:
    case I_WB:
    case I_NWB:
        if (assert_ok(v, in->op, pos))
            gadd(v, l, pool, c2, ln, pc + 1, pos, cap);
        break;
    default:
        l[*ln].pc = pc;
        l[*ln].cap = pool + *ln * c2;
        memcpy(l[*ln].cap, cap, (size_t)c2 * sizeof(long));
        (*ln)++;
        break;
    }
}

int re_ngroups(const cl_re *re)
{
    return re->ngroups;
}

int re_search_groups(const cl_re *cre, const char *s, long n, long from, long *caps, int ncap)
{
    cl_re *re = (cl_re *)cre;
    vm v;
    long c2 = 2L * (re->ngroups + 1), cn = 0, nn, pos = from, i;
    long *pa, *pb, *pcur, *pnext, *best, *seed;
    gthr *la, *lb, *cl, *nl, *gt;
    int matched = 0, posix = (re->flags & RE_POSIX) != 0, k;
    pa = (long *)malloc(sizeof(long) * (size_t)(c2 * re->np));
    pb = (long *)malloc(sizeof(long) * (size_t)(c2 * re->np));
    best = (long *)malloc(sizeof(long) * (size_t)(c2 * 2));
    la = (gthr *)malloc(sizeof(gthr) * (size_t)re->np);
    lb = (gthr *)malloc(sizeof(gthr) * (size_t)re->np);
    if (!pa || !pb || !best || !la || !lb) {
        free(pa);
        free(pb);
        free(best);
        free(la);
        free(lb);
        return -1;
    }
    seed = best + c2;
    cl = la;
    nl = lb;
    pcur = pa;
    pnext = pb;
    v.re = re;
    v.s = s;
    v.n = n;
    if (re->gen > 0x3fffffffL) {
        memset(re->mark, 0, sizeof(long) * (size_t)re->np);
        re->gen = 0;
    }
    re->gen++;
    for (;;) {
        unsigned long cp = 0, fc = 0;
        int len = 0;
        if (!matched) {
            for (k = 0; k < c2; k++)
                seed[k] = -1;
            seed[0] = pos;
            gadd(&v, cl, pcur, c2, &cn, 0, pos, seed);
        }
        if (pos < n) {
            len = cp_at(s, n, pos, &cp);
            fc = (re->flags & RE_ICASE) ? fold(cp) : cp;
        }
        if (!cn) {
            if (matched || pos >= n)
                break;
            pos += len;
            re->gen++;
            continue;
        }
        re->gen++;
        nn = 0;
        for (i = 0; i < cn; i++) {
            const inst *in = &re->prog[cl[i].pc];
            int ok = 0;
            gt = &cl[i];
            if (matched && posix && gt->cap[0] > best[0])
                continue;
            if (in->op == I_MATCH) {
                if (!posix || !matched || gt->cap[0] < best[0] || (gt->cap[0] == best[0] && pos > best[1])) {
                    memcpy(best, gt->cap, (size_t)c2 * sizeof(long));
                    best[1] = pos;
                    matched = 1;
                }
                if (!posix)
                    break;
                continue;
            }
            if (pos >= n)
                continue;
            if (in->op == I_CHAR)
                ok = (re->flags & RE_ICASE) ? fold((unsigned long)in->x) == fc : (unsigned long)in->x == cp;
            else if (in->op == I_ANY)
                ok = cp != '\n' || (re->flags & (RE_DOTALL | RE_POSIX));
            else if (in->op == I_CLASS)
                ok = class_has(re, (int)in->x, cp);
            if (ok)
                gadd(&v, nl, pnext, c2, &nn, gt->pc + 1, pos + len, gt->cap);
        }
        {
            gthr *t = cl;
            long *pt = pcur;
            cl = nl;
            nl = t;
            pcur = pnext;
            pnext = pt;
        }
        cn = nn;
        if (pos >= n)
            break;
        pos += len;
    }
    if (matched)
        for (k = 0; k < ncap * 2; k++)
            caps[k] = k < c2 ? best[k] : -1;
    free(la);
    free(lb);
    free(pa);
    free(pb);
    free(best);
    return matched;
}
