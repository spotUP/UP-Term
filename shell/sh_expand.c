/* vsh's word expansion (see sh_expand.h). */
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

const char *sh_get(const sh_ctx *c, const char *name)
{
    sh_var *v = find(c, name);
    return v ? v->value : 0;
}

void sh_set(sh_ctx *c, const char *name, const char *value)
{
    sh_var *v = find(c, name);
    if (!v) {
        v = (sh_var *)calloc(1, sizeof(sh_var));
        if (!v)
            return;
        v->name = sdup(name);
        v->next = c->vars;
        c->vars = v;
    }
    free(v->value);
    v->value = sdup(value);
}

void sh_unset(sh_ctx *c, const char *name)
{
    sh_var **p;
    for (p = &c->vars; *p; p = &(*p)->next)
        if (!strcmp((*p)->name, name)) {
            sh_var *v = *p;
            *p = v->next;
            free(v->name);
            free(v->value);
            free(v);
            return;
        }
}

void sh_export(sh_ctx *c, const char *name)
{
    sh_var *v = find(c, name);
    if (!v) {
        sh_set(c, name, "");
        v = find(c, name);
    }
    if (v)
        v->exported = 1;
}

void sh_ctx_free(sh_ctx *c)
{
    while (c->vars)
        sh_unset(c, c->vars->name);
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
    if (name[0] >= '0' && name[0] <= '9') {
        int i = atoi(name);
        if (!i)
            return c->arg0 ? c->arg0 : "vsh";
        return i <= c->args.n ? c->args.v[i - 1] : 0;
    }
    return sh_get(c, name);
}

/* $@ and $*: the positional parameters, fields apart ("$@") or joined. */
static void put_args(ex *e, cbuf *b, int at, int dquote)
{
    int i;
    for (i = 0; i < e->c->args.n; i++) {
        if (i) {
            if (at && dquote)
                b->f[b->n - 1] |= F_BREAK;
            else
                cput(b, ' ', dquote ? F_QUOTED : F_SPLIT);
        }
        cputs(b, e->c->args.v[i], dquote ? F_QUOTED : F_SPLIT);
    }
    if (at && dquote && !e->c->args.n)
        b->force_field = -1; /* "$@" of nothing: no field at all */
}

/* ${...}: name, and an operator :- := :+ :? (or - = + ?) with its word. */
static long brace(ex *e, const char *w, long len, cbuf *b, int dquote)
{
    long i = 2, depth = 1, end;
    char name[64];
    int k = 0, len_op = 0, colon = 0;
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
        val = param(e, name);
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
    val = param(e, name);
    if (len_op) {
        char n[16];
        long l = val ? (long)strlen(val) : 0;
        int d = 0;
        char t[16];
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
        put_args(e, b, name[0] == '@', dquote);
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
            put_args(e, b, w[1] == '@', dquote);
        else {
            const char *v = param(e, name);
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
    int i;
    if (!plain)
        return;
    if (to > from)
        memcpy(plain, b->s + from, to - from); /* an empty field may have no buffer */
    plain[to - from] = 0;
    if (!(flags & SH_NO_GLOB) && has_glob(b, from, to)) {
        /* the pattern: quoted characters escaped, so they match themselves */
        char *pat = (char *)malloc(2 * (to - from) + 1), *start;
        sh_list found;
        int k = 0, before = out->n;
        const char *colon;
        char root[256];
        for (i = from; i < to; i++) {
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
