/* vsh's word expansion (see sh_expand.h). */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "sh_expand.h"
#include "sh_hits.h"

/* ---- lists and variables ------------------------------------------------------ */

static char *sdup(const char *s)
{
    size_t n = strlen(s);
    char *r = (char *)malloc(n + 1);
    if (r)
        memcpy(r, s, n + 1);
    return r;
}

void sh_list_add(sh_list *l, const char *s)
{
    if (l->n + 1 >= l->cap) {
        int cap = l->cap ? l->cap * 2 : 8;
        char **v = (char **)realloc(l->v, cap * sizeof(char *));
        if (!v)
            return;
        l->v = v;
        l->cap = cap;
    }
    l->v[l->n++] = sdup(s);
    l->v[l->n] = 0;
}

void sh_list_free(sh_list *l)
{
    int i;
    for (i = 0; i < l->n; i++)
        free(l->v[i]);
    free(l->v);
    l->v = 0;
    l->n = l->cap = 0;
}

static sh_var *find(const sh_ctx *c, const char *name)
{
    sh_var *v;
    for (v = c->vars; v; v = v->next)
        if (!strcmp(v->name, name))
            return v;
    return 0;
}

/* The name a reference leads to (declare -n): at most 8 links. */
static const char *resolve(const sh_ctx *c, const char *name)
{
    int hop;
    for (hop = 0; hop < 8; hop++) {
        sh_var *v = find(c, name);
        if (!v || !(v->attr & SH_ATTR_NAMEREF) || !v->sv || !*v->sv)
            break;
        name = v->sv;
    }
    return name;
}

sh_var *sh_lookup_raw(const sh_ctx *c, const char *name)
{
    return find(c, name);
}

const char *sh_resolve(const sh_ctx *c, const char *name)
{
    return resolve(c, name);
}

sh_var *sh_lookup(const sh_ctx *c, const char *name)
{
    return find(c, resolve(c, name));
}

/* ---- arrays --------------------------------------------------------------------- */

static int is_assoc(const sh_var *v)
{
    return (v->attr & SH_ATTR_ASSOC) != 0;
}

/* where the element idx / key is, or would go */
static long arr_pos(const sh_var *v, long idx, const char *key, int *found)
{
    const sh_arr *a = v->arr;
    long lo = 0, hi = a->n;
    while (lo < hi) {
        long m = (lo + hi) / 2, d;
        if (is_assoc(v))
            d = strcmp(a->e[m].key, key);
        else
            d = a->e[m].idx < idx ? -1 : a->e[m].idx > idx;
        if (!d) {
            *found = 1;
            return m;
        }
        if (d < 0)
            lo = m + 1;
        else
            hi = m;
    }
    *found = 0;
    return lo;
}

static void elem_free(sh_elem *e)
{
    free(e->key);
    free(e->val);
}

static void var_free(sh_var *v)
{
    free(v->name);
    free(v->sv);
    if (v->arr) {
        long i;
        for (i = 0; i < v->arr->n; i++)
            elem_free(v->arr->e + i);
        free(v->arr->e);
        free(v->arr);
    }
    free(v);
}

/* the scalar becomes element 0 of a new array */
static int make_array(sh_var *v, int assoc)
{
    v->arr = (sh_arr *)calloc(1, sizeof(sh_arr));
    if (!v->arr)
        return 1;
    v->attr = (unsigned short)((v->attr & ~(SH_ATTR_ARRAY | SH_ATTR_ASSOC)) | (assoc ? SH_ATTR_ASSOC : SH_ATTR_ARRAY));
    if (v->sv) {
        v->arr->e = (sh_elem *)calloc(1, sizeof(sh_elem));
        if (!v->arr->e)
            return 1;
        v->arr->n = v->arr->cap = 1;
        v->arr->e[0].val = v->sv;
        if (assoc)
            v->arr->e[0].key = sdup("0");
        v->sv = 0;
    }
    return 0;
}

/* The slot of the element at sub, created when make; 0: a bad subscript or no memory.
 * Negative indexes count from the end (the last index + 1). */
static char **elem_slot(sh_ctx *c, sh_var *v, const char *sub, int make)
{
    long idx = 0, pos;
    int found;
    sh_arr *a;
    if (is_assoc(v)) {
        if (!sub)
            sub = "0";
    } else if (sub) {
        const char *err = 0;
        idx = sh_arith(c, sub, &err);
        if (err)
            return 0;
        if (idx < 0 && v->arr && v->arr->n)
            idx += v->arr->e[v->arr->n - 1].idx + 1;
        if (idx < 0)
            return 0;
    }
    if (!v->arr && make && make_array(v, 0))
        return 0;
    if (!v->arr)
        return 0;
    a = v->arr;
    pos = arr_pos(v, idx, sub, &found);
    if (found)
        return &a->e[pos].val;
    if (!make)
        return 0;
    if (a->n == a->cap) {
        long cap = a->cap ? a->cap * 2 : 4;
        sh_elem *t = (sh_elem *)realloc(a->e, (size_t)cap * sizeof(sh_elem));
        if (!t)
            return 0;
        a->e = t;
        a->cap = cap;
    }
    memmove(a->e + pos + 1, a->e + pos, (size_t)(a->n - pos) * sizeof(sh_elem));
    a->e[pos].idx = idx;
    a->e[pos].key = is_assoc(v) ? sdup(sub) : 0;
    a->e[pos].val = 0;
    a->n++;
    return &a->e[pos].val;
}

const char *sh_var_str(const sh_var *v)
{
    int f;
    long p;
    if (!v)
        return 0;
    if (!v->arr)
        return v->sv;
    p = arr_pos(v, 0, "0", &f);
    return f ? v->arr->e[p].val : 0;
}

const char *sh_get(const sh_ctx *c, const char *name)
{
    return sh_var_str(sh_lookup(c, name));
}

const char *sh_get_elem(sh_ctx *c, const char *name, const char *sub)
{
    sh_var *v = sh_lookup(c, name);
    char **s;
    if (!v)
        return 0;
    if (!v->arr) {
        const char *err = 0;
        return sh_arith(c, sub, &err) == 0 && !err ? v->sv : 0;
    }
    s = elem_slot(c, v, sub, 0);
    return s ? *s : 0;
}

char **sh_values(const sh_ctx *c, const char *name, long *n)
{
    sh_var *v = sh_lookup(c, name);
    long i, k = 0, cnt = v ? (v->arr ? v->arr->n : 1) : 0;
    char **r = (char **)malloc((size_t)(cnt + 1) * sizeof(char *));
    if (!r) {
        *n = 0;
        return 0;
    }
    if (v && !v->arr && v->sv)
        r[k++] = v->sv;
    else if (v && v->arr)
        for (i = 0; i < v->arr->n; i++)
            if (v->arr->e[i].val)
                r[k++] = v->arr->e[i].val;
    *n = k;
    return r;
}

void sh_keys(const sh_ctx *c, const char *name, sh_list *out)
{
    sh_var *v = sh_lookup(c, name);
    long i;
    char d[24];
    if (!v || (!v->arr && !v->sv))
        return;
    if (!v->arr) {
        sh_list_add(out, "0");
        return;
    }
    for (i = 0; i < v->arr->n; i++) {
        if (is_assoc(v))
            sh_list_add(out, v->arr->e[i].key);
        else {
            sh_ltoa(v->arr->e[i].idx, d);
            sh_list_add(out, d);
        }
    }
}

void sh_ltoa(long v, char *out)
{
    char d[24];
    int n = 0, k = 0;
    unsigned long u = v < 0 ? 0UL - (unsigned long)v : (unsigned long)v;
    if (v < 0)
        out[k++] = '-';
    do
        d[n++] = (char)('0' + u % 10);
    while ((u /= 10) > 0);
    while (n)
        out[k++] = d[--n];
    out[k] = 0;
}

/* value as the attributes of v make it: integer (evaluated), upper, lower; malloc'ed */
static char *conv(sh_ctx *c, const sh_var *v, const char *value)
{
    char *t = sdup(value), *q;
    if (t && (v->attr & SH_ATTR_INTEGER)) {
        const char *err = 0;
        long n = sh_arith(c, value, &err);
        char d[24];
        sh_ltoa(err ? 0L : n, d);
        free(t);
        t = sdup(d);
    }
    for (q = t; q && *q; q++)
        if ((v->attr & SH_ATTR_UPPER) && *q >= 'a' && *q <= 'z')
            *q -= 32;
        else if ((v->attr & SH_ATTR_LOWER) && *q >= 'A' && *q <= 'Z')
            *q += 32;
    return t;
}

int sh_assign(sh_ctx *c, const char *name, const char *sub, const char *value, int append)
{
    sh_var *v;
    char **slot, *nv;
    name = resolve(c, name);
    v = find(c, name);
    if (v && (v->attr & SH_ATTR_READONLY))
        return 1;
    if (v)
        v->attr &= (unsigned short)~SH_ATTR_NOVALUE;
    if (!v) {
        v = (sh_var *)calloc(1, sizeof(sh_var));
        if (!v)
            return 1;
        v->name = sdup(name);
        v->next = c->vars;
        c->vars = v;
    }
    if (sub || v->arr) {
        slot = elem_slot(c, v, sub, 1);
        if (!slot)
            return 1;
        SH_HIT(ARRAY_ELEM_SET);
    } else
        slot = &v->sv;
    if (append && *slot) {
        if (v->attr & SH_ATTR_INTEGER) {
            const char *e = 0;
            char d[24];
            sh_ltoa(sh_arith(c, *slot, &e) + sh_arith(c, value, &e), d);
            nv = sdup(d);
        } else {
            nv = (char *)malloc(strlen(*slot) + strlen(value) + 1);
            if (nv) {
                strcpy(nv, *slot);
                strcat(nv, value);
            }
            if (nv) {
                char *t = conv(c, v, nv);
                free(nv);
                nv = t;
            }
        }
    } else
        nv = conv(c, v, value);
    if (!nv)
        return 1;
    free(*slot);
    *slot = nv;
    if (c->allexport)
        v->attr |= SH_ATTR_EXPORT;
    return 0;
}

int sh_set(sh_ctx *c, const char *name, const char *value)
{
    return sh_assign(c, name, 0, value, 0);
}

unsigned sh_attr(const sh_ctx *c, const char *name)
{
    sh_var *v = find(c, name);
    return v ? v->attr : 0;
}

void sh_attr_change(sh_ctx *c, const char *name, unsigned set, unsigned clear)
{
    sh_var *v = find(c, name);
    if (!v) {
        sh_set(c, name, "");
        v = find(c, name);
        if (v && (set & (SH_ATTR_ARRAY | SH_ATTR_ASSOC))) {
            free(v->sv);
            v->sv = 0;
            v->attr |= SH_ATTR_NOVALUE;
        }
    }
    if (!v)
        return;
    if ((set & (SH_ATTR_ARRAY | SH_ATTR_ASSOC)) && !v->arr)
        make_array(v, (set & SH_ATTR_ASSOC) != 0);
    set &= ~(unsigned)(v->arr ? SH_ATTR_ARRAY | SH_ATTR_ASSOC : 0);
    v->attr = (unsigned short)((v->attr & ~clear) | set);
}

long sh_next_index(const sh_ctx *c, const char *name)
{
    const sh_var *v = sh_lookup(c, name);
    if (!v)
        return 0;
    if (!v->arr)
        return v->sv ? 1 : 0;
    return v->arr->n ? v->arr->e[v->arr->n - 1].idx + 1 : 0;
}

int sh_array_reset(sh_ctx *c, const char *name, int assoc)
{
    sh_var *v;
    name = resolve(c, name);
    v = find(c, name);
    if (v && (v->attr & SH_ATTR_READONLY))
        return 1;
    if (v)
        v->attr &= (unsigned short)~SH_ATTR_NOVALUE;
    if (v && v->arr && is_assoc(v) == assoc) {
        long i;
        for (i = 0; i < v->arr->n; i++)
            elem_free(v->arr->e + i);
        v->arr->n = 0;
        return 0;
    }
    if (v) {
        unsigned short keep = (unsigned short)(v->attr & ~(SH_ATTR_ARRAY | SH_ATTR_ASSOC));
        free(v->sv);
        v->sv = 0;
        if (v->arr) {
            long i;
            for (i = 0; i < v->arr->n; i++)
                elem_free(v->arr->e + i);
            free(v->arr->e);
            free(v->arr);
            v->arr = 0;
        }
        v->attr = keep;
    } else {
        sh_attr_change(c, name, assoc ? SH_ATTR_ASSOC : SH_ATTR_ARRAY, 0);
        v = find(c, name);
        if (v)
            v->attr &= (unsigned short)~SH_ATTR_NOVALUE;
        return 0;
    }
    return make_array(v, assoc);
}

sh_var *sh_var_copy(const sh_var *v)
{
    sh_var *r = (sh_var *)calloc(1, sizeof(sh_var));
    if (!r)
        return 0;
    r->name = sdup(v->name);
    r->attr = v->attr;
    if (v->sv)
        r->sv = sdup(v->sv);
    if (v->arr) {
        long i;
        r->arr = (sh_arr *)calloc(1, sizeof(sh_arr));
        if (!r->arr)
            return r;
        r->arr->e = (sh_elem *)calloc((size_t)(v->arr->n ? v->arr->n : 1), sizeof(sh_elem));
        if (!r->arr->e)
            return r;
        r->arr->n = r->arr->cap = v->arr->n;
        for (i = 0; i < v->arr->n; i++) {
            r->arr->e[i].idx = v->arr->e[i].idx;
            if (v->arr->e[i].key)
                r->arr->e[i].key = sdup(v->arr->e[i].key);
            if (v->arr->e[i].val)
                r->arr->e[i].val = sdup(v->arr->e[i].val);
        }
    }
    return r;
}

/* remove the variable NAME whatever its attributes */
static void drop(sh_ctx *c, const char *name)
{
    sh_var **p;
    for (p = &c->vars; *p; p = &(*p)->next)
        if (!strcmp((*p)->name, name)) {
            sh_var *v = *p;
            *p = v->next;
            var_free(v);
            return;
        }
}

sh_var *sh_var_save(const sh_ctx *c, const char *name)
{
    const sh_var *v = find(c, name);
    return v ? sh_var_copy(v) : 0;
}

void sh_var_link(sh_ctx *c, sh_var *v)
{
    drop(c, v->name);
    v->next = c->vars;
    c->vars = v;
}

void sh_var_restore(sh_ctx *c, const char *name, sh_var *saved)
{
    if (saved) {
        if (saved->arr)
            SH_HIT(LOCAL_RESTORE_ARRAY);
        sh_var_link(c, saved);
    } else
        drop(c, name);
}

void sh_pstat(sh_ctx *c, const long *st, int n)
{
    long *t;
    if (n == 1 && c->npstat == 1) {
        c->pstat[0] = st[0];
        return;
    }
    t = (long *)realloc(c->pstat, (size_t)(n ? n : 1) * sizeof(long));
    if (!t)
        return;
    memcpy(t, st, (size_t)n * sizeof(long));
    c->pstat = t;
    c->npstat = n;
}

/* a byte bash leaves unquoted */
static int q_safe(unsigned char ch)
{
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch >= 128 ||
           (ch && strchr("_-./:,@%+=", ch));
}

/* bash's quoting: see sh_expand.h */
char *sh_quote(const char *s, int style)
{
    size_t n = strlen(s), k = 0, i;
    char *o = (char *)malloc(n * 4 + 8);
    int ctl = 0, plain = 1;
    if (!o)
        return 0;
    for (i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch == 127)
            ctl = 1;
        if (!q_safe(ch))
            plain = 0;
    }
    if (!n) {
        strcpy(o, "''");
        return o;
    }
    if (!ctl && plain)
        return strcpy(o, s);
    if (ctl) {
        o[k++] = '$';
        o[k++] = '\'';
        for (i = 0; i < n; i++) {
            unsigned char ch = (unsigned char)s[i];
            if (ch == '\n') { o[k++] = '\\'; o[k++] = 'n'; }
            else if (ch == '\t') { o[k++] = '\\'; o[k++] = 't'; }
            else if (ch == '\r') { o[k++] = '\\'; o[k++] = 'r'; }
            else if (ch == 27) { o[k++] = '\\'; o[k++] = 'E'; }
            else if (ch == '\'' || ch == '\\') { o[k++] = '\\'; o[k++] = (char)ch; }
            else if (ch < 32 || ch == 127) { o[k++] = '\\'; o[k++] = (char)('0' + (ch >> 6)); o[k++] = (char)('0' + ((ch >> 3) & 7)); o[k++] = (char)('0' + (ch & 7)); }
            else o[k++] = (char)ch;
        }
        o[k++] = '\'';
    } else if (style == SH_Q_BACKSLASH) {
        for (i = 0; i < n; i++) {
            unsigned char ch = (unsigned char)s[i];
            if (!q_safe(ch) || ch == '\'')
                o[k++] = '\\';
            o[k++] = (char)ch;
        }
    } else {
        o[k++] = '\'';
        for (i = 0; i < n; i++) {
            if (s[i] == '\'') { o[k++] = '\''; o[k++] = '\\'; o[k++] = '\''; }
            o[k++] = s[i];
        }
        o[k++] = '\'';
    }
    o[k] = 0;
    return o;
}

int sh_unset(sh_ctx *c, const char *name)
{
    sh_var *v = sh_lookup(c, name);
    if (!v)
        return 0;
    if (v->attr & SH_ATTR_READONLY)
        return 1;
    drop(c, v->name);
    return 0;
}

int sh_unset_elem(sh_ctx *c, const char *name, const char *sub)
{
    sh_var *v = sh_lookup(c, name);
    char **slot;
    long pos;
    if (!v)
        return 0;
    if (v->attr & SH_ATTR_READONLY)
        return 1;
    if (!v->arr) {
        const char *err = 0;
        if (sh_arith(c, sub, &err) == 0 && !err)
            drop(c, v->name);
        return 0;
    }
    slot = elem_slot(c, v, sub, 0);
    if (!slot)
        return 0;
    pos = (sh_elem *)((char *)slot - offsetof(sh_elem, val)) - v->arr->e;
    elem_free(v->arr->e + pos);
    memmove(v->arr->e + pos, v->arr->e + pos + 1, (size_t)(v->arr->n - pos - 1) * sizeof(sh_elem));
    v->arr->n--;
    return 0;
}

void sh_export(sh_ctx *c, const char *name)
{
    sh_var *v = find(c, name);
    if (!v) {
        sh_set(c, name, "");
        v = find(c, name);
    }
    if (v)
        v->attr |= SH_ATTR_EXPORT;
}

void sh_ctx_free(sh_ctx *c)
{
    while (c->vars) {
        drop(c, c->vars->name);
    }
    free(c->pstat);
    c->pstat = 0;
    sh_list_free(&c->args);
}

/* ---- pattern matching ---------------------------------------------------------- */

static int lower(int ch)
{
    return ch >= 'A' && ch <= 'Z' ? ch + 32 : ch;
}

int sh_match(const char *p, const char *s, int nocase)
{
    for (; *p; p++, s++) {
        if (*p == '*') {
            while (p[1] == '*')
                p++;
            if (!p[1])
                return 1;
            for (; *s; s++)
                if (sh_match(p + 1, s, nocase))
                    return 1;
            return sh_match(p + 1, s, nocase);
        }
        if (!*s)
            return 0;
        if (*p == '?')
            continue;
        if (*p == '[') {
            const char *q = p + 1;
            int neg = 0, ok = 0;
            if (*q == '!' || *q == '^') {
                neg = 1;
                q++;
            }
            do {
                int lo = *q, hi = *q;
                if (!*q)
                    return 0; /* no ]: not a bracket; treat as unmatched */
                if (q[1] == '-' && q[2] && q[2] != ']') {
                    hi = q[2];
                    q += 2;
                }
                if (nocase ? (lower(*s) >= lower(lo) && lower(*s) <= lower(hi))
                           : (*s >= lo && *s <= hi))
                    ok = 1;
                q++;
            } while (*q != ']');
            if (ok == neg)
                return 0;
            p = q;
            continue;
        }
        if (*p == '\\' && p[1])
            p++;
        if (nocase ? lower(*p) != lower(*s) : *p != *s)
            return 0;
    }
    return !*s;
}

/* ---- arithmetic ---------------------------------------------------------------- */

typedef struct arith {
    sh_ctx *c;
    const char *s;
    const char *err;
} arith;

static long a_expr(arith *a);

static void a_space(arith *a)
{
    while (*a->s == ' ' || *a->s == '\t' || *a->s == '\n')
        a->s++;
}

static long a_atom(arith *a)
{
    long v = 0;
    a_space(a);
    if (*a->s == '(') {
        a->s++;
        v = a_expr(a);
        a_space(a);
        if (*a->s == ')')
            a->s++;
        else
            a->err = "arithmetic: ) is missing";
        return v;
    }
    if (*a->s == '-') {
        a->s++;
        return -a_atom(a);
    }
    if (*a->s == '+') {
        a->s++;
        return a_atom(a);
    }
    if (*a->s == '$')
        a->s++;
    if (*a->s >= '0' && *a->s <= '9') {
        while (*a->s >= '0' && *a->s <= '9')
            v = v * 10 + (*a->s++ - '0');
        return v;
    }
    if ((*a->s >= 'A' && *a->s <= 'Z') || (*a->s >= 'a' && *a->s <= 'z') || *a->s == '_') {
        char name[64];
        int k = 0;
        const char *val;
        while (((*a->s >= 'A' && *a->s <= 'Z') || (*a->s >= 'a' && *a->s <= 'z') || *a->s == '_' ||
                (*a->s >= '0' && *a->s <= '9')) && k < 63)
            name[k++] = *a->s++;
        name[k] = 0;
        if (*a->s == '[') {
            char sb[64];
            int m = 0, d = 0;
            a->s++;
            while (*a->s && (*a->s != ']' || d) && m < 63) {
                d += *a->s == '[' ? 1 : *a->s == ']' ? -1 : 0;
                sb[m++] = *a->s++;
            }
            sb[m] = 0;
            if (*a->s == ']')
                a->s++;
            val = sh_get_elem(a->c, name, sb);
        } else
            val = sh_get(a->c, name);
        return val ? atol(val) : 0;
    }
    a->err = "arithmetic: a number is missing";
    return 0;
}

static long a_term(arith *a)
{
    long v = a_atom(a);
    for (;;) {
        char op;
        long r;
        a_space(a);
        op = *a->s;
        if (op != '*' && op != '/' && op != '%')
            return v;
        a->s++;
        r = a_atom(a);
        if (op == '*')
            v *= r;
        else if (!r)
            a->err = "arithmetic: division by zero";
        else
            v = op == '/' ? v / r : v % r;
    }
}

static long a_expr(arith *a)
{
    long v = a_term(a);
    for (;;) {
        char op;
        a_space(a);
        op = *a->s;
        if (op != '+' && op != '-')
            return v;
        a->s++;
        v = op == '+' ? v + a_term(a) : v - a_term(a);
    }
}

long sh_arith(sh_ctx *c, const char *expr, const char **err)
{
    arith a;
    long v;
    a.c = c;
    a.s = expr;
    a.err = 0;
    v = a_expr(&a);
    a_space(&a);
    if (!a.err && *a.s)
        a.err = "arithmetic: unexpected text";
    if (err)
        *err = a.err;
    return a.err ? 0 : v;
}

/* ---- the expansion buffer --------------------------------------------------- */

/* Each character carries where it came from: */
#define F_QUOTED 1   /* inside quotes, or escaped: never split, never a glob character */
#define F_SPLIT  2   /* the result of an unquoted expansion: split on IFS */
#define F_BREAK  4   /* "$@": a field ends after this character */
#define F_EMPTY  8   /* placeholder: an empty field of "$@" or "${a[@]}"; it adds no text */

typedef struct cbuf {
    char *s;
    unsigned char *f;
    int n, cap;
    int had_quotes;     /* the word had quoting: an empty result is still one field */
    int force_field;    /* "$@" with no arguments must give no field; with some, one each */
} cbuf;

static void cput(cbuf *b, char ch, int flags)
{
    if (b->n + 1 >= b->cap) {
        int cap = b->cap ? b->cap * 2 : 64;
        char *s = (char *)realloc(b->s, cap);
        unsigned char *f = (unsigned char *)realloc(b->f, cap);
        if (!s || !f)
            return;
        b->s = s;
        b->f = f;
        b->cap = cap;
    }
    b->s[b->n] = ch;
    b->f[b->n++] = (unsigned char)flags;
    b->s[b->n] = 0;
}

static void cputs(cbuf *b, const char *s, int flags)
{
    while (*s)
        cput(b, *s++, flags);
}

static void cfree(cbuf *b)
{
    free(b->s);
    free(b->f);
}

typedef struct ex {
    sh_ctx *c;
    const char *err;
} ex;

static int expand_into(ex *e, const char *w, long len, cbuf *b, int dquote);

/* Expand text (an operand of ${x:-text}) to a plain string. */
static char *expand_string(ex *e, const char *w, long len, int dquote)
{
    cbuf b;
    char *r;
    memset(&b, 0, sizeof(b));
    expand_into(e, w, len, &b, dquote);
    r = sdup(b.s ? b.s : "");
    cfree(&b);
    return r;
}

static int is_name_char(char ch, int first)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_' ||
           (!first && ch >= '0' && ch <= '9');
}

/* The value of a parameter name ("?", "#", "1", "HOME"); 0 when unset.
 * The result is valid until the next call. */
static const char *param(ex *e, const char *name)
{
    static char num[24];
    sh_ctx *c = e->c;
    long v;
    int k;
    if (!name[1] && (name[0] == '?' || name[0] == '$' || name[0] == '!' || name[0] == '#')) {
        v = name[0] == '?' ? c->status : name[0] == '$' ? c->pid : name[0] == '!' ? c->last_bg : c->args.n;
        if (name[0] == '!' && !c->last_bg)
            return 0;
        k = 0;
        if (v < 0) {
            num[k++] = '-';
            v = -v;
        }
        {
            char d[20];
            int n = 0;
            do
                d[n++] = (char)('0' + v % 10);
            while ((v /= 10) > 0);
            while (n)
                num[k++] = d[--n];
        }
        num[k] = 0;
        return num;
    }
    if (!name[1] && name[0] == '-')
        return c->flags ? c->flags : "";
    if (c->npstat && !strcmp(name, "PIPESTATUS")) {
        sh_ltoa(c->pstat[0], num);
        return num;
    }
    if (name[0] >= '0' && name[0] <= '9') {
        int i = atoi(name);
        if (!i)
            return c->arg0 ? c->arg0 : "vsh";
        return i <= c->args.n ? c->args.v[i - 1] : 0;
    }
    return sh_get(c, name);
}

/* set -u: an unset NAME is an error (not $@ $*) */
static int unbound(ex *e, const char *name, const char *val)
{
    static char msg[80];
    if (val || !e->c->nounset || !strcmp(name, "@") || !strcmp(name, "*"))
        return 0;
    strncpy(msg, name, 60);
    msg[60] = 0;
    strcat(msg, ": unbound variable");
    e->err = msg;
    return 1;
}

/* One seam for the value list of $@ / $* and of an array NAME[@] (PIPESTATUS included), so the
 * operators that work on lists are written once. */
typedef struct pv {
    char **v;
    long n;
    int own, deep;      /* v is malloc'ed; the strings are too */
} pv;

static void pv_get(ex *e, const char *name, pv *p)
{
    memset(p, 0, sizeof(*p));
    if (name[0] == '@' || name[0] == '*') {
        p->v = e->c->args.v;
        p->n = e->c->args.n;
    } else if (!strcmp(name, "PIPESTATUS") && e->c->npstat) {
        int i;
        char t[24];
        p->v = (char **)calloc((size_t)e->c->npstat, sizeof(char *));
        p->own = p->deep = 1;
        for (i = 0; p->v && i < e->c->npstat; i++) {
            sh_ltoa(e->c->pstat[i], t);
            p->v[p->n++] = sdup(t);
        }
    } else {
        p->v = sh_values(e->c, name, &p->n);
        p->own = 1;
        SH_HIT(PARAM_VALUES_ARRAY);
    }
}

static void pv_free(pv *p)
{
    long i;
    for (i = 0; p->deep && i < p->n; i++)
        free(p->v[i]);
    if (p->own)
        free(p->v);
}

/* $@ and $*, ${a[@]} and ${a[*]}: the values, fields apart ("$@") or joined. */
static void put_values(ex *e, cbuf *b, const char *name, int at, int dquote)
{
    pv p;
    long i;
    const char *ifs = sh_get(e->c, "IFS");
    pv_get(e, name, &p);
    for (i = 0; i < p.n; i++) {
        if (i) {
            if (at && dquote)
                b->f[b->n - 1] |= F_BREAK;
            else if (!dquote)
                cput(b, ' ', F_SPLIT);
            else if (!ifs || *ifs)
                cput(b, ifs ? *ifs : ' ', F_QUOTED);
        }
        if (at && dquote && !p.v[i][0])
            cput(b, ' ', F_QUOTED | F_EMPTY);
        else
            cputs(b, p.v[i], dquote ? F_QUOTED : F_SPLIT);
    }
    if (at && dquote && !p.n)
        b->force_field = -1; /* "$@" of nothing: no field at all */
    pv_free(&p);
}

/* the value of NAME[sub] (sub raw text, expanded here); 0 when unset. @ and * give element 0. */
static const char *pval(ex *e, const char *name, int hassub, const char *sub)
{
    static char num[24];
    char *k;
    const char *r;
    if (!hassub)
        return param(e, name);
    if (sub[0] == '@' || sub[0] == '*')
        sub = "0";
    k = expand_string(e, sub, (long)strlen(sub), 0);
    if (!k)
        return 0;
    if (!strcmp(name, "PIPESTATUS") && e->c->npstat) {
        long i = atol(k);
        r = i >= 0 && i < e->c->npstat ? (sh_ltoa(e->c->pstat[i], num), num) : 0;
    } else
        r = sh_get_elem(e->c, name, k);
    free(k);
    return r;
}

/* ${...}: name, and an operator :- := :+ :? (or - = + ?) with its word. */
static long brace(ex *e, const char *w, long len, cbuf *b, int dquote)
{
    long i = 2, depth = 1, end;
    char name[64], sub[96];
    int k = 0, len_op = 0, colon = 0, hassub = 0, indirect = 0, all = 0;
    char op = 0;
    const char *val;
    while (i < len && depth) {
        if (w[i] == '{')
            depth++;
        else if (w[i] == '}')
            depth--;
        if (depth)
            i++;
    }
    end = i;
    if (depth) {
        e->err = "bad substitution";
        return len;
    }
    i = 2;
    if (w[i] == '#' && end > 3) {
        len_op = 1;
        i++;
    }
    if (w[i] == '!' && i + 1 < end && (is_name_char(w[i + 1], 1))) {
        indirect = 1;
        i++;
    }
    if (strchr("?$!#@*-", w[i]) && k == 0) {
        name[k++] = w[i++];
    } else {
        while (i < end && k < 63 && (is_name_char(w[i], !k) || (w[i] >= '0' && w[i] <= '9')))
            name[k++] = w[i++];
    }
    name[k] = 0;
    if (!k) {
        e->err = "bad substitution";
        return end + 1;
    }
    if (i < end && w[i] == '[') {
        long j = i + 1;
        int d = 1, m = 0;
        while (j < end && d) {
            d += w[j] == '[' ? 1 : w[j] == ']' ? -1 : 0;
            if (d && m < 95)
                sub[m++] = w[j];
            j++;
        }
        sub[m] = 0;
        hassub = 1;
        i = j;
        all = !strcmp(sub, "@") || !strcmp(sub, "*");
    }
    if (indirect) {
        const sh_var *rv = find(e->c, name);
        if (all) {
            /* ${!a[@]}: the keys, as a list */
            sh_list ks;
            int q;
            memset(&ks, 0, sizeof(ks));
            sh_keys(e->c, name, &ks);
            for (q = 0; q < ks.n; q++) {
                if (q) {
                    if (sub[0] == '@' && dquote)
                        b->f[b->n - 1] |= F_BREAK;
                    else
                        cput(b, ' ', dquote ? F_QUOTED : F_SPLIT);
                }
                cputs(b, ks.v[q], dquote ? F_QUOTED : F_SPLIT);
            }
            if (sub[0] == '@' && dquote && !ks.n)
                b->force_field = -1;
            sh_list_free(&ks);
            return end + 1;
        }
        if (rv && (rv->attr & SH_ATTR_NAMEREF)) {
            if (rv->sv)
                cputs(b, rv->sv, dquote ? F_QUOTED : F_SPLIT);
            return end + 1;
        }
        val = pval(e, name, hassub, sub);
        if (!val || !*val)
            return end + 1;
        strncpy(name, val, 63);
        name[63] = 0;
        hassub = all = 0;
        {
            char *br = strchr(name, '[');
            size_t nl = strlen(name);
            if (br && name[nl - 1] == ']') {
                *br = 0;
                name[nl - 1] = 0;
                strncpy(sub, br + 1, 95);
                sub[95] = 0;
                hassub = 1;
                all = !strcmp(sub, "@") || !strcmp(sub, "*");
            }
        }
    }
    if (i < end && (w[i] == '#' || w[i] == '%') && !len_op) {
        /* ${X#p} ${X##p}: without the shortest / longest prefix matching p;
         * ${X%p} ${X%%p}: the same for a suffix */
        char kind = w[i++], *pat, *tmp;
        int longest = 0;
        long l, k, from = 0, to;
        if (i < end && w[i] == kind) {
            longest = 1;
            i++;
        }
        val = pval(e, name, hassub, sub);
        if (unbound(e, name, val))
            return end + 1;
        if (!val)
            val = "";
        l = (long)strlen(val);
        to = l;
        pat = expand_string(e, w + i, end - i, dquote);
        tmp = (char *)malloc(l + 1);
        if (pat && tmp) {
            if (kind == '#') {
                for (k = longest ? l : 0; longest ? k >= 0 : k <= l; k += longest ? -1 : 1) {
                    memcpy(tmp, val, k);
                    tmp[k] = 0;
                    if (sh_match(pat, tmp, 0)) {
                        from = k;
                        break;
                    }
                }
            } else {
                for (k = longest ? 0 : l; longest ? k <= l : k >= 0; k += longest ? 1 : -1)
                    if (sh_match(pat, val + k, 0)) {
                        to = k;
                        break;
                    }
            }
            memcpy(tmp, val + from, to - from);
            tmp[to - from] = 0;
            cputs(b, tmp, dquote ? F_QUOTED : F_SPLIT);
        }
        free(pat);
        free(tmp);
        return end + 1;
    }
    if (i < end && w[i] == ':') {
        colon = 1;
        i++;
    }
    if (i < end && strchr("-=+?", w[i]))
        op = w[i++];
    else if (i < end) {
        e->err = "bad substitution";
        return end + 1;
    }
    val = pval(e, name, hassub, sub);
    if (all) {
        pv p;
        pv_get(e, name, &p);
        val = p.n ? "x" : 0;
        pv_free(&p);
    }
    if (!op && !len_op && unbound(e, name, val))
        return end + 1;
    if (len_op && !all && unbound(e, name, val))
        return end + 1;
    if (len_op) {
        char n[16];
        long l = val ? (long)strlen(val) : 0;
        int d = 0;
        char t[16];
        if (all) {
            pv p;
            pv_get(e, name, &p);
            l = p.n;
            pv_free(&p);
        }
        do
            t[d++] = (char)('0' + l % 10);
        while ((l /= 10) > 0);
        k = 0;
        while (d)
            n[k++] = t[--d];
        n[k] = 0;
        cputs(b, n, dquote ? F_QUOTED : F_SPLIT);
        return end + 1;
    }
    {
        int empty = !val || (colon && !*val);
        if (op == '-' && empty) {
            char *v = expand_string(e, w + i, end - i, dquote);
            cputs(b, v, dquote ? F_QUOTED : F_SPLIT);
            free(v);
            return end + 1;
        }
        if (op == '=' && empty) {
            char *v = expand_string(e, w + i, end - i, dquote);
            sh_set(e->c, name, v);
            cputs(b, v, dquote ? F_QUOTED : F_SPLIT);
            free(v);
            return end + 1;
        }
        if (op == '?' && empty) {
            e->err = "parameter not set";
            return end + 1;
        }
        if (op == '+') {
            if (!empty) {
                char *v = expand_string(e, w + i, end - i, dquote);
                cputs(b, v, dquote ? F_QUOTED : F_SPLIT);
                free(v);
            }
            return end + 1;
        }
    }
    if (!strcmp(name, "@") || !strcmp(name, "*"))
        put_values(e, b, name, name[0] == '@', dquote);
    else if (all)
        put_values(e, b, name, sub[0] == '@', dquote);
    else if (val)
        cputs(b, val, dquote ? F_QUOTED : F_SPLIT);
    return end + 1;
}

/* $(cmd) or `cmd`: the command's output, trailing newlines removed. */
static void command_output(ex *e, const char *cmd, long n, cbuf *b, int dquote)
{
    char *text = (char *)malloc(n + 1), *out;
    long l;
    if (!text)
        return;
    memcpy(text, cmd, n);
    text[n] = 0;
    out = e->c->subst ? e->c->subst(e->c, text) : 0;
    free(text);
    if (!out)
        return;
    l = (long)strlen(out);
    while (l && out[l - 1] == '\n')
        out[--l] = 0;
    cputs(b, out, dquote ? F_QUOTED : F_SPLIT);
    free(out);
}

/* $... at w: expand it into b; the characters consumed. */
static long dollar(ex *e, const char *w, long len, cbuf *b, int dquote)
{
    if (len >= 3 && w[1] == '(' && w[2] == '(') {
        long i = 3, depth = 2;
        while (i < len && depth) {
            if (w[i] == '(')
                depth++;
            else if (w[i] == ')')
                depth--;
            i++;
        }
        {
            char *expr = expand_string(e, w + 3, i - 5 > 0 ? i - 5 : 0, 1);
            const char *err = 0;
            long v = sh_arith(e->c, expr, &err);
            char n[24], t[24];
            int k = 0, d = 0, neg = v < 0;
            free(expr);
            if (err) {
                e->err = err;
                return i;
            }
            if (neg)
                v = -v;
            do
                t[d++] = (char)('0' + v % 10);
            while ((v /= 10) > 0);
            if (neg)
                n[k++] = '-';
            while (d)
                n[k++] = t[--d];
            n[k] = 0;
            cputs(b, n, dquote ? F_QUOTED : F_SPLIT);
        }
        return i;
    }
    if (len >= 2 && w[1] == '(') {
        long i = 2, depth = 1;
        while (i < len && depth) {
            if (w[i] == '(')
                depth++;
            else if (w[i] == ')')
                depth--;
            if (depth)
                i++;
        }
        command_output(e, w + 2, i - 2, b, dquote);
        return i + 1;
    }
    if (len >= 2 && w[1] == '{')
        return brace(e, w, len, b, dquote);
    if (len >= 2 && strchr("?$!#@*-0123456789", w[1])) {
        char name[2];
        name[0] = w[1];
        name[1] = 0;
        if (w[1] == '@' || w[1] == '*')
            put_values(e, b, name, w[1] == '@', dquote);
        else {
            const char *v = param(e, name);
            if (unbound(e, name, v))
                return 2;
            if (v)
                cputs(b, v, dquote ? F_QUOTED : F_SPLIT);
        }
        return 2;
    }
    if (len >= 2 && is_name_char(w[1], 1)) {
        char name[64];
        long i = 1;
        int k = 0;
        const char *v;
        while (i < len && k < 63 && is_name_char(w[i], !k))
            name[k++] = w[i++];
        name[k] = 0;
        v = param(e, name);
        if (unbound(e, name, v))
            return i;
        if (v)
            cputs(b, v, dquote ? F_QUOTED : F_SPLIT);
        return i;
    }
    cput(b, '$', dquote ? F_QUOTED : 0); /* a lone $ is itself */
    return 1;
}

/* Expand w[0..len) into b: quotes and escapes resolved, expansions done. */
static int expand_into(ex *e, const char *w, long len, cbuf *b, int dquote)
{
    long i = 0;
    while (i < len && !e->err) {
        char ch = w[i];
        if (!dquote && ch == '\'') {
            long j = i + 1;
            b->had_quotes = 1;
            while (j < len && w[j] != '\'')
                cput(b, w[j++], F_QUOTED);
            i = j + 1;
        } else if (!dquote && ch == '"') {
            long j = i + 1, depth = 0;
            b->had_quotes = 1;
            /* the closing quote, skipping over $( ) and ${ } */
            while (j < len && (w[j] != '"' || depth)) {
                if (w[j] == '\\' && j + 1 < len)
                    j++;
                else if (w[j] == '$' && j + 1 < len && (w[j + 1] == '(' || w[j + 1] == '{'))
                    depth++, j++;
                else if (depth && (w[j] == ')' || w[j] == '}'))
                    depth--;
                j++;
            }
            expand_into(e, w + i + 1, j - i - 1, b, 1);
            i = j + 1;
        } else if (ch == '\\' && i + 1 < len) {
            char nx = w[i + 1];
            if (dquote && !strchr("$`\"\\\n", nx)) {
                cput(b, '\\', F_QUOTED); /* inside "...": \ stays before others */
                cput(b, nx, F_QUOTED);
            } else {
                cput(b, nx, F_QUOTED);
            }
            b->had_quotes = 1;
            i += 2;
        } else if (ch == '$') {
            i += dollar(e, w + i, len - i, b, dquote);
        } else if (ch == '`') {
            long j = i + 1;
            while (j < len && w[j] != '`') {
                if (w[j] == '\\' && j + 1 < len)
                    j++;
                j++;
            }
            command_output(e, w + i + 1, j - i - 1, b, dquote);
            i = j + 1;
        } else if (!dquote && ch == '~' && i == 0) {
            const char *home = sh_get(e->c, "HOME");
            cputs(b, home ? home : "SYS:", F_QUOTED);
            i++;
        } else {
            cput(b, ch, dquote ? F_QUOTED : 0);
            i++;
        }
    }
    return e->err ? -1 : 0;
}

/* ---- globbing ------------------------------------------------------------------- */

/* Does the bracket at s[i] close within s[..to)? A ']' right after '[' or
 * '[!' is a member, not the end (POSIX). Without its ']', '[' is literal:
 * the word "[" of [ $i -lt 9 ] is no pattern (vsh listed the directory
 * for every one -- 85 ms per loop turn on the rig). quoted: the flags,
 * or 0 to take every character as unquoted. */
static int bracket_closes(const char *s, const unsigned char *quoted, int i, int to)
{
    int j = i + 1;
    if (j < to && (s[j] == '!' || s[j] == '^'))
        j++;
    if (j < to && s[j] == ']')
        j++;
    for (; j < to; j++)
        if (s[j] == ']' && !(quoted && (quoted[j] & F_QUOTED)))
            return 1;
    return 0;
}

static int has_glob(const cbuf *b, int from, int to)
{
    int i;
    for (i = from; i < to; i++)
        if (!(b->f[i] & F_QUOTED) &&
            (b->s[i] == '*' || b->s[i] == '?' ||
             (b->s[i] == '[' && bracket_closes(b->s, b->f, i, to))))
            return 1;
    return 0;
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Expand pattern (glob characters active, \ escapes the rest) below dir,
 * component by component. The separator is '/', a leading "vol:" is kept. */
static void glob_rec(sh_ctx *c, const char *dir, const char *pat, sh_list *out)
{
    const char *slash = strchr(pat, '/');
    size_t clen = slash ? (size_t)(slash - pat) : strlen(pat);
    char comp[256];
    int i, magic = 0;
    sh_list names;
    if (clen >= sizeof(comp))
        return;
    memcpy(comp, pat, clen);
    comp[clen] = 0;
    for (i = 0; comp[i]; i++)
        if (comp[i] == '*' || comp[i] == '?' ||
            (comp[i] == '[' && bracket_closes(comp, 0, i, (int)clen)))
            magic = 1;
    if (!magic) {
        /* a literal component: no listing, the escapes removed */
        char lit[256], path[512];
        int k = 0;
        for (i = 0; comp[i]; i++) {
            if (comp[i] == '\\' && comp[i + 1])
                i++;
            lit[k++] = comp[i];
        }
        lit[k] = 0;
        strcpy(path, dir);
        if (*dir && dir[strlen(dir) - 1] != ':' && dir[strlen(dir) - 1] != '/')
            strcat(path, "/");
        strcat(path, lit);
        if (slash)
            glob_rec(c, path, slash + 1, out);
        else
            sh_list_add(out, path);
        return;
    }
    memset(&names, 0, sizeof(names));
    SH_HIT(GLOB);
    if (!c->listdir || c->listdir(c, dir, &names))
        return;
    for (i = 0; i < names.n; i++) {
        char path[512];
        if (names.v[i][0] == '.' && comp[0] != '.')
            continue; /* dot files only when asked for */
        if (!sh_match(comp, names.v[i], c->nocase))
            continue;
        strcpy(path, dir);
        if (*dir && dir[strlen(dir) - 1] != ':' && dir[strlen(dir) - 1] != '/')
            strcat(path, "/");
        strcat(path, names.v[i]);
        if (slash)
            glob_rec(c, path, slash + 1, out);
        else
            sh_list_add(out, path);
    }
    sh_list_free(&names);
}

/* One field b[from..to): globbed if it has unquoted glob characters,
 * else added with quotes removed (they already are: b holds the text). */
static void add_field(sh_ctx *c, cbuf *b, int from, int to, int flags, sh_list *out)
{
    char *plain = (char *)malloc(to - from + 1);
    int i, m = 0;
    if (!plain)
        return;
    for (i = from; i < to; i++)
        if (!(b->f[i] & F_EMPTY))
            plain[m++] = b->s[i];
    plain[m] = 0;
    if (!(flags & SH_NO_GLOB) && has_glob(b, from, to)) {
        /* the pattern: quoted characters escaped, so they match themselves */
        char *pat = (char *)malloc(2 * (to - from) + 1), *start;
        sh_list found;
        int k = 0, before = out->n;
        const char *colon;
        char root[256];
        for (i = from; i < to; i++) {
            if (b->f[i] & F_EMPTY)
                continue;
            if ((b->f[i] & F_QUOTED) && strchr("*?[]\\", b->s[i]))
                pat[k++] = '\\';
            pat[k++] = b->s[i];
        }
        pat[k] = 0;
        memset(&found, 0, sizeof(found));
        start = pat;
        root[0] = 0;
        colon = strchr(pat, ':');
        if (colon && (!strchr(pat, '/') || colon < strchr(pat, '/'))) {
            size_t rl = (size_t)(colon - pat) + 1;
            memcpy(root, pat, rl);
            root[rl] = 0;
            start = pat + rl;
        } else if (pat[0] == '/') {
            strcpy(root, "/");
            start = pat + 1;
        }
        glob_rec(c, root, start, &found);
        if (found.n) {
            qsort(found.v, found.n, sizeof(char *), cmp_str);
            for (i = 0; i < found.n; i++)
                sh_list_add(out, found.v[i]);
        }
        sh_list_free(&found);
        free(pat);
        if (out->n > before) {
            free(plain);
            return;
        }
        /* no match: the word stays as it was (POSIX) */
    }
    sh_list_add(out, plain);
    free(plain);
}

int sh_expand(sh_ctx *c, const char *word, int flags, sh_list *out, const char **err)
{
    ex e;
    cbuf b;
    const char *ifs = sh_get(c, "IFS");
    int i, start = 0, in_field = 0, fields = 0;
    if (c->noglob)
        flags |= SH_NO_GLOB;
    e.c = c;
    e.err = 0;
    memset(&b, 0, sizeof(b));
    if (!ifs)
        ifs = " \t\n";
    if (expand_into(&e, word, (long)strlen(word), &b, 0) < 0) {
        if (err)
            *err = e.err;
        cfree(&b);
        return -1;
    }
    if (flags & SH_NO_SPLIT) {
        add_field(c, &b, 0, b.n, flags, out);
        cfree(&b);
        return 0;
    }
    for (i = 0; i <= b.n; i++) {
        int end = i == b.n;
        int sep = !end && (b.f[i] & F_SPLIT) && strchr(ifs, b.s[i]) && b.s[i];
        if (end || sep) {
            if (in_field) {
                add_field(c, &b, start, i, flags, out);
                fields++;
            }
            in_field = 0;
            continue;
        }
        if (!in_field) {
            in_field = 1;
            start = i;
        }
        if (b.f[i] & F_BREAK) {
            add_field(c, &b, start, i + 1, flags, out);
            fields++;
            in_field = 0;
        }
    }
    if (!fields && b.had_quotes && b.force_field >= 0)
        sh_list_add(out, ""); /* "" and "$empty" are one empty field */
    cfree(&b);
    return 0;
}

char *sh_unquote(const char *word)
{
    size_t n = strlen(word);
    char *r = (char *)malloc(n + 1), *o = r;
    int single = 0, dbl = 0;
    if (!r)
        return 0;
    for (; *word; word++) {
        if (*word == '\'' && !dbl) {
            single = !single;
            continue;
        }
        if (*word == '"' && !single) {
            dbl = !dbl;
            continue;
        }
        if (*word == '\\' && !single && word[1])
            word++;
        *o++ = *word;
    }
    *o = 0;
    return r;
}
