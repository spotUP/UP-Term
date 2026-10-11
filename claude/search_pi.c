/* search_pi -- pi's grep and find for amiga-pi (tools.h tools_pi_grep,
 * tools_pi_find): what pi gets from rg (--json --line-number --hidden) and
 * fd (--glob --hidden), done on a walk through sys.h with the ignore files
 * those tools honour (ignore.h), the text matched by regex.h and names by
 * glob.h. The walk goes in name order, depth first (rg --sort path); rg's
 * and fd's own order is unspecified (parallel). pi's row formats; the
 * notices and the byte cut are amiga-pi's (truncate.c), from the flags.
 * Its own file so C:Claude, which never calls it, does not link it
 * (sources.mk CLAUDE_PI). Portable C89, host-tested by amiga-pi
 * (tests/test_pi_tools.c) and against rg (tests/test_pi_search.py). */
#include <stdlib.h>
#include <string.h>
#include "tools_int.h"
#include "ignore.h"
#include "glob.h"
#include "regex.h"
#include "path.h"
#include "util.h"

#define SP_DEPTH     64
#define SP_FILE_MAX  (1024L * 1024)     /* a bigger file is not searched (memory on a 68k) */
#define SP_LINE_MAX  500                /* pi's GREP_MAX_LINE_LENGTH, in UTF-16 units */

typedef struct sp_ent {
    char *name;
    int dir;
    long size;
} sp_ent;

typedef struct sp_list {
    sp_ent *e;
    long n, cap;
    int oom;
} sp_list;

static int sp_one(void *c, const cl_dirent *d)
{
    sp_list *l = (sp_list *)c;
    if (l->n == l->cap) {
        long nc = l->cap ? l->cap * 2 : 32;
        sp_ent *ne = (sp_ent *)realloc(l->e, (size_t)nc * sizeof(sp_ent));
        if (!ne) {
            l->oom = 1;
            return 1;
        }
        l->e = ne;
        l->cap = nc;
    }
    l->e[l->n].name = (char *)malloc(strlen(d->name) + 1);
    if (!l->e[l->n].name) {
        l->oom = 1;
        return 1;
    }
    strcpy(l->e[l->n].name, d->name);
    l->e[l->n].dir = d->dir;
    l->e[l->n].size = d->size;
    l->n++;
    return 0;
}

static int sp_cmp(const void *a, const void *b)
{
    return strcmp(((const sp_ent *)a)->name, ((const sp_ent *)b)->name);
}

typedef struct sp_walk sp_walk;
/* an object the walk keeps: nonzero stops the walk */
typedef int (*sp_fn)(sp_walk *w, const char *full, const sp_ent *e);

struct sp_walk {
    cl_tools *t;
    const char *custom;         /* ".rgignore" / ".fdignore" */
    int require_git, skip_git;  /* fd skips .git itself */
    ig_set over;                /* rg --glob: overrides the ignore files */
    int over_on, over_white;
    const char *over_base;      /* --glob paths are below this */
    sp_fn fn;
    void *c;
    int stop, oom;
};

/* below dir: full's part after dir and its '/', 0 not below */
static const char *sp_below(const char *dir, const char *full)
{
    long k = (long)strlen(dir);
    if (!k || strncmp(dir, full, (size_t)k))
        return 0;
    if (full[k] == '/')
        return full + k + 1;
    if (dir[k - 1] == ':' || dir[k - 1] == '/')
        return full + k;
    return 0;
}

/* -1 skip, 1 keep without the ignore files, 0 the ignore files decide */
static int sp_override(sp_walk *w, const char *full, int dir)
{
    const char *rel;
    int m;
    if (!w->over_on)
        return 0;
    rel = sp_below(w->over_base, full);
    m = ig_match(&w->over, rel ? rel : full, dir);
    if (m > 0)
        return 1;               /* a plain glob: wanted */
    if (m < 0)
        return -1;              /* a '!' glob */
    return !dir && w->over_white ? -1 : 0;
}

static void sp_dir(sp_walk *w, const char *full, ig_dir *ig, int depth)
{
    sp_list l;
    long i;
    memset(&l, 0, sizeof(l));
    w->t->sys->list(w->t->sys->u, full, sp_one, &l);
    if (l.oom)
        w->oom = w->stop = 1;
    qsort(l.e, (size_t)l.n, sizeof(sp_ent), sp_cmp);
    for (i = 0; i < l.n && !w->stop; i++) {
        char p[600];
        int o;
        if (path_join(full, l.e[i].name, p, sizeof(p)))
            continue;
        if (w->skip_git && l.e[i].dir && !strcmp(l.e[i].name, ".git"))
            continue;
        o = sp_override(w, p, l.e[i].dir);
        if (o < 0 || (o == 0 && ig_ignored(ig, p, l.e[i].dir, w->require_git)))
            continue;
        if (w->fn(w, p, &l.e[i])) {
            w->stop = 1;
            break;
        }
        if (l.e[i].dir && depth < SP_DEPTH) {
            ig_dir sub;
            if (ig_load(&sub, w->t->sys, p, ig, w->custom)) {
                w->oom = w->stop = 1;
                break;
            }
            sp_dir(w, p, &sub, depth + 1);
            ig_free(&sub);
        }
    }
    for (i = 0; i < l.n; i++)
        free(l.e[i].name);
    free(l.e);
}

static void sp_walk_run(sp_walk *w, const char *root)
{
    ig_dir *ig = ig_chain(w->t->sys, root, w->custom);
    if (!ig) {
        w->oom = 1;
        return;
    }
    sp_dir(w, root, ig, 0);
    ig_free_chain(ig);
}

/* is full, or a directory above it, a git repository's (holds .git)? */
static int sp_in_git(cl_sys *sys, const char *full)
{
    char d[600], g[620];
    cl_copy(d, full, sizeof(d));
    for (;;) {
        char up[600];
        if (path_join(d, ".git", g, sizeof(g)) == 0 && sys->kind(sys->u, g) > 0)
            return 1;
        if (path_parent(d, up, sizeof(up)) || !strcmp(up, d) || !up[0])
            return 0;
        cl_copy(d, up, sizeof(d));
    }
}

/* the UTF-16 units of one UTF-8 character starting with byte c (a byte that
 * does not start one: one unit, as Latin-1) */
static int sp_units(unsigned char c)
{
    return c >= 0xF0 && c <= 0xF4 ? 2 : 1;
}

static long sp_char_len(const char *s, long n, long i)
{
    unsigned char c = (unsigned char)s[i];
    long k = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC2 ? 2 : 1, j;
    if (i + k > n)
        return 1;
    for (j = 1; j < k; j++)
        if (((unsigned char)s[i + j] & 0xC0) != 0x80)
            return 1;
    return k;
}

/* pi's truncateLine: at most 500 UTF-16 units, then "... [truncated]" */
static void sp_line(jw *o, const char *s, long n, int *cut)
{
    long i = 0, u = 0;
    while (i < n) {
        long k = sp_char_len(s, n, i);
        int du = k == 4 ? sp_units((unsigned char)s[i]) : 1;
        if (u + du > SP_LINE_MAX)
            break;
        u += du;
        i += k;
    }
    if (i < n) {
        /* JavaScript cuts between the halves of a pair: that half is lost too */
        jw_raw(o, s, i);
        jw_rawz(o, "... [truncated]");
        *cut = 1;
    } else
        jw_raw(o, s, n);
}

/* ---- grep ---- */

typedef struct sp_grep {
    cl_re *re;
    long ctx, limit, count;
    int one_file;
    const char *root;           /* rows are below it */
    pi_rows *r;
} sp_grep;

static void sp_name(sp_grep *g, jw *o, const char *full)
{
    const char *rel = g->one_file ? 0 : sp_below(g->root, full);
    if (!rel || !*rel) {
        const char *b = full + strlen(full);
        while (b > full && b[-1] != '/' && b[-1] != ':')
            b--;
        rel = b;
    }
    jw_rawz(o, rel);
}

static void sp_row(sp_grep *g, const char *full, char sep, long no, const char *s, long n)
{
    char num[16];
    long i;
    jw *o = &g->r->text;
    if (o->n)
        jw_raw(o, "\n", 1);
    sp_name(g, o, full);
    jw_raw(o, &sep, 1);
    cl_ltoa(no, num);
    jw_rawz(o, num);
    jw_raw(o, &sep, 1);
    jw_raw(o, " ", 1);
    /* the line without its CRs */
    {
        jw t;
        jw_init(&t);
        for (i = 0; i < n; i++)
            if (s[i] != '\r')
                jw_raw(&t, s + i, 1);
        sp_line(o, t.p ? t.p : "", t.n, &g->r->lines_cut);
        jw_free(&t);
    }
    g->r->rows++;
}

static void sp_grep_file(sp_grep *g, cl_tools *t, const char *full)
{
    char *b = 0, *lf = 0;
    long n = 0, i = 0, no = 1, *ls = 0, nl = 0;
    if (t->sys->read(t->sys->u, full, SP_FILE_MAX, &b, &n))
        return;
    if (tl_is_binary(b, n)) {
        free(b);
        return;
    }
    while (i < n && g->count < g->limit) {
        long e = i, ms, me;
        while (e < n && b[e] != '\n')
            e++;
        if (re_search(g->re, b + i, e - i, 0, &ms, &me) == 1) {
            g->count++;
            if (!g->ctx)
                sp_row(g, full, ':', no, b + i, e - i);
            else {
                long a, z, k;
                if (!ls) {
                    /* pi's lines for the context: CRLF and CR as LF, then split */
                    long fn = 0, j;
                    lf = (char *)malloc((size_t)n + 1);
                    ls = (long *)malloc(((size_t)n + 2) * sizeof(long));
                    if (!lf || !ls) {
                        g->r->oom = 1;
                        break;
                    }
                    for (j = 0; j < n; j++) {
                        if (b[j] == '\r') {
                            lf[fn++] = '\n';
                            if (j + 1 < n && b[j + 1] == '\n')
                                j++;
                        } else
                            lf[fn++] = b[j];
                    }
                    ls[nl++] = 0;
                    for (j = 0; j < fn; j++)
                        if (lf[j] == '\n')
                            ls[nl++] = j + 1;
                    ls[nl] = fn + 1;
                }
                a = no - g->ctx > 1 ? no - g->ctx : 1;
                z = no + g->ctx < nl ? no + g->ctx : nl;
                for (k = a; k <= z; k++) {
                    long s0 = k - 1 < nl ? ls[k - 1] : 0, e0 = k - 1 < nl ? ls[k] - 1 : 0;
                    sp_row(g, full, k == no ? ':' : '-', k, k - 1 < nl ? lf + s0 : "", e0 - s0);
                }
            }
        }
        i = e + 1;
        no++;
    }
    free(lf);
    free(ls);
    free(b);
}

static int sp_grep_hit(sp_walk *w, const char *full, const sp_ent *e)
{
    sp_grep *g = (sp_grep *)w->c;
    if (e->dir || e->size > SP_FILE_MAX)
        return 0;
    sp_grep_file(g, w->t, full);
    return g->count >= g->limit || g->r->oom;
}

/* rg's error for a pattern regex.h refuses */
static int sp_fail(pi_rows *r, const char *a, const char *b)
{
    jw_free(&r->text);
    jw_init(&r->text);
    jw_rawz(&r->text, a);
    if (b)
        jw_rawz(&r->text, b);
    return 1;
}

int tools_pi_grep(cl_tools *t, const char *raw, long rawn, pi_rows *r)
{
    char *pat = 0, *pathp = 0, *gl = 0, err[200], full[512], what[400];
    long pn = 0;
    int outside = 0, kind, rc = 0;
    sp_grep g;
    sp_walk w;
    jv in;
    memset(r, 0, sizeof(*r));
    jw_init(&r->text);
    memset(&g, 0, sizeof(g));
    memset(&w, 0, sizeof(w));
    t->cur = T_GREP;
    if (json_parse(raw, rawn, &in) || json_type(in) != J_OBJ)
        return sp_fail(r, "the tool input was not valid JSON", 0);
    pat = tl_prop(in, "pattern", &pn);
    pathp = tl_prop(in, "path", 0);
    gl = tl_prop(in, "glob", 0);
    if (!pat || !pathp || !gl) {
        rc = sp_fail(r, "out of memory", 0);
        goto done;
    }
    if (tl_bool(in, "literal")) {
        /* rg --fixed-strings: the pattern's specials escaped */
        jw e;
        long i;
        jw_init(&e);
        for (i = 0; i < pn; i++) {
            if (pat[i] && strchr("\\.^$|?*+()[]{}", pat[i]))
                jw_raw(&e, "\\", 1);
            jw_raw(&e, pat + i, 1);
        }
        g.re = re_compile(e.p ? e.p : "", tl_bool(in, "ignoreCase") ? RE_ICASE : 0, err, sizeof(err));
        jw_free(&e);
    } else
        g.re = re_compile(pat, tl_bool(in, "ignoreCase") ? RE_ICASE : 0, err, sizeof(err));
    if (!g.re) {
        rc = sp_fail(r, "regex parse error: ", err);
        goto done;
    }
    g.ctx = tl_num(in, "context", 0);
    if (g.ctx < 0)
        g.ctx = 0;
    g.limit = tl_num(in, "limit", 100);
    if (g.limit < 1)
        g.limit = 1;
    g.r = r;
    r->limit = g.limit;
    if (tl_resolve(t, *pathp ? pathp : ".", full, sizeof(full), &outside)) {
        rc = sp_fail(r, "Path not found: ", pathp);
        goto done;
    }
    tl_summary(what, sizeof(what), pat, full);
    if (t->show)
        t->show(t->u, tools_name(T_GREP), what);
    kind = t->sys->kind(t->sys->u, full);
    if (kind != 1 && kind != 2) {
        rc = sp_fail(r, "Path not found: ", full);
        goto done;
    }
    g.root = full;
    if (kind == 1) {
        g.one_file = 1;
        sp_grep_file(&g, t, full);
    } else {
        w.t = t;
        w.custom = ".rgignore";
        w.require_git = 1;      /* rg: .gitignore inside a git repository only */
        if (*gl) {
            if (ig_parse(&w.over, gl, (long)strlen(gl))) {
                rc = sp_fail(r, "out of memory", 0);
                goto done;
            }
            /* rg's --glob is the opposite of an ignore line: plain wants, '!' skips */
            w.over_on = w.over.n > 0;
            w.over_white = w.over.n > 0 && !w.over.r[0].neg;
            w.over_base = sp_below(t->root, full) ? t->root : full;
        }
        w.fn = sp_grep_hit;
        w.c = &g;
        sp_walk_run(&w, full);
        if (w.oom)
            r->oom = 1;
    }
    if (r->oom) {
        rc = sp_fail(r, "out of memory", 0);
        goto done;
    }
    r->limit_hit = g.count >= g.limit;
    r->matches = g.count;
done:
    if (w.over.r) {
        int i;
        for (i = 0; i < w.over.n; i++)
            free(w.over.r[i].pat);
        free(w.over.r);
    }
    re_free(g.re);
    free(pat);
    free(pathp);
    free(gl);
    return rc;
}

/* ---- find ---- */

typedef struct sp_find {
    const char *pat;
    int full_path;
    const char *root;
    long limit;
    pi_rows *r;
} sp_find;

static int sp_find_hit(sp_walk *w, const char *full, const sp_ent *e)
{
    sp_find *f = (sp_find *)w->c;
    jw *o = &f->r->text;
    const char *rel = sp_below(f->root, full);
    if (!glob_match(f->pat, f->full_path ? full : e->name))
        return 0;
    if (o->n)
        jw_raw(o, "\n", 1);
    jw_rawz(o, rel ? rel : full);
    if (e->dir)
        jw_raw(o, "/", 1);
    f->r->rows++;
    f->r->matches++;
    return f->r->matches >= f->limit;
}

/* fd's glob check: a class or a group left open */
static const char *sp_glob_bad(const char *p)
{
    int brace = 0;
    for (; *p; p++) {
        if (*p == '\\' && p[1])
            p++;
        else if (*p == '[') {
            const char *q = p + 1;
            if (*q == '!' || *q == '^')
                q++;
            if (*q == ']')
                q++;
            while (*q && *q != ']')
                q++;
            if (!*q)
                return "unclosed character class; missing ']'";
            p = q;
        } else if (*p == '{')
            brace++;
        else if (*p == '}') {
            if (!brace)
                return "unopened alternate group; missing '{' or extra '}'";
            brace--;
        }
    }
    return brace ? "unclosed alternate group; missing '}' or extra '{'" : 0;
}

int tools_pi_find(cl_tools *t, const char *raw, long rawn, pi_rows *r)
{
    char *pat = 0, *pathp = 0, full[512], what[400], eff[600];
    const char *bad;
    int outside = 0, rc = 0;
    sp_find f;
    sp_walk w;
    jv in;
    memset(r, 0, sizeof(*r));
    jw_init(&r->text);
    memset(&f, 0, sizeof(f));
    memset(&w, 0, sizeof(w));
    t->cur = T_GLOB;
    if (json_parse(raw, rawn, &in) || json_type(in) != J_OBJ)
        return sp_fail(r, "the tool input was not valid JSON", 0);
    pat = tl_prop(in, "pattern", 0);
    pathp = tl_prop(in, "path", 0);
    if (!pat || !pathp) {
        rc = sp_fail(r, "out of memory", 0);
        goto done;
    }
    f.limit = tl_num(in, "limit", 1000);
    r->limit = f.limit;
    if (tl_resolve(t, *pathp ? pathp : ".", full, sizeof(full), &outside) || t->sys->kind(t->sys->u, full) != 2) {
        jw_rawz(&r->text, "[fd error]: Search path '");
        jw_rawz(&r->text, *pathp ? pathp : ".");
        jw_rawz(&r->text, "' is not a directory.");
        rc = 1;
        goto done;
    }
    if ((bad = sp_glob_bad(pat)) != 0) {
        jw_rawz(&r->text, "[fd error]: error parsing glob '");
        jw_rawz(&r->text, pat);
        jw_rawz(&r->text, "': ");
        jw_rawz(&r->text, bad);
        rc = 1;
        goto done;
    }
    tl_summary(what, sizeof(what), pat, full);
    if (t->show)
        t->show(t->u, tools_name(T_GLOB), what);
    /* pi: a pattern with '/' matches the full path, from any depth */
    cl_copy(eff, pat, sizeof(eff));
    if (strchr(pat, '/')) {
        f.full_path = 1;
        if (pat[0] != '/' && strncmp(pat, "**/", 3) && strcmp(pat, "**")) {
            cl_copy(eff, "**/", sizeof(eff));
            cl_cat(eff, pat, sizeof(eff));
        }
    }
    f.pat = eff;
    f.root = full;
    f.r = r;
    if (f.limit <= 0)
        goto done;              /* fd --max-results 0: nothing */
    w.t = t;
    w.custom = ".fdignore";
    w.require_git = sp_in_git(t->sys, full);    /* --no-require-git outside a repository */
    w.skip_git = 1;
    w.fn = sp_find_hit;
    w.c = &f;
    sp_walk_run(&w, full);
    if (w.oom)
        rc = sp_fail(r, "out of memory", 0);
    r->limit_hit = r->matches >= f.limit;
done:
    free(pat);
    free(pathp);
    return rc;
}
