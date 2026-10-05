/* search -- Glob and Grep (ledger A4 WP2): a walk over a directory tree
 * through sys.h, names matched by glob.h, contents by regex.h, the output
 * shaped as ripgrep's is for Claude Code. */
#include <stdlib.h>
#include <string.h>
#include "tools_int.h"
#include "glob.h"
#include "regex.h"
#include "path.h"
#include "util.h"

#define WALK_DEPTH   16
#define WALK_ENTRIES 20000L
#define GLOB_SHOW    100
#define GLOB_KEEP    2000
#define GREP_FILE_MAX (1024L * 1024)
#define GREP_KEEP    5000
#define CTX_MAX      50

typedef struct hit {
    char *path;
    long t;
    long count;
} hit;

typedef struct hits {
    hit *h;
    long n, cap;
    int more;                   /* the keep limit was reached */
} hits;

static int hits_add(hits *l, const char *p, long t, long count, long keep)
{
    char *c;
    if (l->n >= keep) {
        l->more = 1;
        return 0;
    }
    if (l->n == l->cap) {
        long nc = l->cap ? l->cap * 2 : 64;
        hit *nh = (hit *)realloc(l->h, (size_t)nc * sizeof(hit));
        if (!nh)
            return -1;
        l->h = nh;
        l->cap = nc;
    }
    c = (char *)malloc(strlen(p) + 1);
    if (!c)
        return -1;
    strcpy(c, p);
    l->h[l->n].path = c;
    l->h[l->n].t = t;
    l->h[l->n].count = count;
    l->n++;
    return 0;
}

static void hits_free(hits *l)
{
    long i;
    for (i = 0; i < l->n; i++)
        free(l->h[i].path);
    free(l->h);
    memset(l, 0, sizeof(*l));
}

static int newest_first(const void *a, const void *b)
{
    long x = ((const hit *)a)->t, y = ((const hit *)b)->t;
    if (x != y)
        return x > y ? -1 : 1;
    return strcmp(((const hit *)a)->path, ((const hit *)b)->path);
}

void search_sort_newest(char **paths, long *times, long n)
{
    hit *h = (hit *)malloc(sizeof(hit) * (size_t)(n ? n : 1));
    long i;
    if (!h)
        return;
    for (i = 0; i < n; i++) {
        h[i].path = paths[i];
        h[i].t = times[i];
    }
    qsort(h, (size_t)n, sizeof(hit), newest_first);
    for (i = 0; i < n; i++) {
        paths[i] = h[i].path;
        times[i] = h[i].t;
    }
    free(h);
}

/* ---- the walk ---- */

typedef struct walk walk;
/* an entry: its full path, its path below the root, the entry; nonzero stops */
typedef int (*walk_fn)(walk *w, const char *full, const char *rel, const cl_dirent *e);

struct walk {
    cl_tools *t;
    walk_fn fn;
    void *c;
    int maxdepth;
    long seen;
    int stopped;
    /* the directories still to read: full path and path below the root */
    char **full, **rel;
    int *depth;
    int nd, cd;
    const char *cur_full, *cur_rel;
    int cur_depth;
};

static int push(walk *w, const char *full, const char *rel, int depth)
{
    char *f, *r;
    if (w->nd == w->cd) {
        int nc = w->cd ? w->cd * 2 : 16;
        char **a = (char **)realloc(w->full, (size_t)nc * sizeof(char *));
        char **b;
        int *d;
        if (!a)
            return -1;
        w->full = a;
        b = (char **)realloc(w->rel, (size_t)nc * sizeof(char *));
        if (!b)
            return -1;
        w->rel = b;
        d = (int *)realloc(w->depth, (size_t)nc * sizeof(int));
        if (!d)
            return -1;
        w->depth = d;
        w->cd = nc;
    }
    f = (char *)malloc(strlen(full) + 1);
    r = (char *)malloc(strlen(rel) + 1);
    if (!f || !r) {
        free(f);
        free(r);
        return -1;
    }
    strcpy(f, full);
    strcpy(r, rel);
    w->full[w->nd] = f;
    w->rel[w->nd] = r;
    w->depth[w->nd++] = depth;
    return 0;
}

static int skip_dir(const char *name)
{
    return !strcmp(name, ".git") || !strcmp(name, ".svn") || !strcmp(name, ".hg");
}

static int walk_entry(void *c, const cl_dirent *e)
{
    walk *w = (walk *)c;
    char full[512], rel[512];
    if (++w->seen > WALK_ENTRIES) {
        w->stopped = 1;
        return 1;
    }
    if (path_join(w->cur_full, e->name, full, sizeof(full)))
        return 0;
    cl_copy(rel, w->cur_rel, sizeof(rel));
    if (rel[0])
        cl_cat(rel, "/", sizeof(rel));
    cl_cat(rel, e->name, sizeof(rel));
    if (e->dir && !skip_dir(e->name) && (w->maxdepth < 0 || w->cur_depth < w->maxdepth))
        push(w, full, rel, w->cur_depth + 1);
    if (w->fn(w, full, rel, e)) {
        w->stopped = 1;
        return 1;
    }
    return 0;
}

static void walk_run(walk *w, const char *root)
{
    push(w, root, "", 0);
    while (w->nd && !w->stopped) {
        int k = --w->nd;
        char *f = w->full[k], *r = w->rel[k];
        w->cur_full = f;
        w->cur_rel = r;
        w->cur_depth = w->depth[k];
        w->t->sys->list(w->t->sys->u, f, walk_entry, w);
        free(f);
        free(r);
    }
    while (w->nd) {
        w->nd--;
        free(w->full[w->nd]);
        free(w->rel[w->nd]);
    }
    free(w->full);
    free(w->rel);
    free(w->depth);
}

/* a pattern argument as a glob: AmigaDOS forms converted; 0, -1 (err in out) */
static int to_glob(const char *in, char *out, long cap)
{
    if (strstr(in, "#?") || strchr(in, '#') || strchr(in, '~') || (strchr(in, '(') && strchr(in, '|')))
        return glob_from_amiga(in, out, cap);
    cl_copy(out, in, cap);
    return (long)strlen(in) >= cap ? -1 : 0;
}

/* ---- Glob ---- */

typedef struct gctx {
    const char *pat;
    hits l;
} gctx;

static int glob_hit(walk *w, const char *full, const char *rel, const cl_dirent *e)
{
    gctx *g = (gctx *)w->c;
    if (glob_match(g->pat, rel)) {
        char p[520];
        cl_copy(p, full, sizeof(p));
        if (e->dir)
            cl_cat(p, "/", sizeof(p));
        if (hits_add(&g->l, p, e->mtime, 0, GLOB_KEEP))
            return 1;
    }
    return 0;
}

void search_glob(cl_tools *t, jw *out, const char *id, jv in)
{
    char *pa = tl_prop(in, "pattern", 0), *pathp = tl_prop(in, "path", 0);
    char pat[512], base[512], full[512], what[400], rel[512];
    long i, k, wild = -1;
    int outside = 0;
    gctx g;
    walk w;
    jw r;
    memset(&g, 0, sizeof(g));
    if (!pa || !pathp) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    if (to_glob(pa, pat, sizeof(pat))) {
        tl_error(t, out, id, "not a usable pattern: ", pat);
        goto done;
    }
    /* the fixed directories in front of the first wildcard go into the base */
    base[0] = 0;
    for (i = 0; pat[i]; i++)
        if (pat[i] == '*' || pat[i] == '?' || pat[i] == '[' || pat[i] == '{') {
            wild = i;
            break;
        }
    k = -1;
    for (i = 0; pat[i] && (wild < 0 || i < wild); i++)
        if (pat[i] == '/' || pat[i] == ':')
            k = i;
    if (wild < 0) {
        /* no wildcard: the pattern names one object */
        k = -1;
        for (i = 0; pat[i]; i++)
            if (pat[i] == '/' || pat[i] == ':')
                k = i;
    }
    cl_copy(base, pathp, sizeof(base));
    if (k >= 0) {
        char head[512];
        memcpy(head, pat, (size_t)k + 1);
        head[pat[k] == '/' && k ? k : k + 1] = 0;
        if (strchr(head, ':') || !*base)
            cl_copy(base, head, sizeof(base));
        else {
            cl_cat(base, "/", sizeof(base));
            cl_cat(base, head, sizeof(base));
        }
        cl_copy(rel, pat + k + 1, sizeof(rel));
        cl_copy(pat, rel, sizeof(pat));
    }
    if (tl_resolve(t, base, full, sizeof(full), &outside)) {
        tl_error(t, out, id, "not a usable path (it climbs above a volume's root): ", base);
        goto done;
    }
    tl_summary(what, sizeof(what), pa, full);
    if (tl_gate(t, out, id, T_GLOB, what, outside, 1))
        goto done;
    if (t->sys->kind(t->sys->u, full) != 2) {
        tl_error(t, out, id, "no such directory: ", full);
        goto done;
    }
    g.pat = pat;
    memset(&w, 0, sizeof(w));
    w.t = t;
    w.fn = glob_hit;
    w.c = &g;
    w.maxdepth = glob_depth(pat) < 0 ? WALK_DEPTH : glob_depth(pat);
    walk_run(&w, full);
    jw_init(&r);
    if (!g.l.n)
        jw_rawz(&r, "No files found");
    else {
        qsort(g.l.h, (size_t)g.l.n, sizeof(hit), newest_first);
        for (i = 0; i < g.l.n && i < GLOB_SHOW; i++) {
            jw_rawz(&r, g.l.h[i].path);
            jw_raw(&r, "\n", 1);
        }
        if (g.l.n > GLOB_SHOW || g.l.more || w.stopped)
            jw_rawz(&r, "(Results are truncated. Consider using a more specific path or pattern.)\n");
    }
    tl_result(t, out, id, r.p, r.n, 0);
    jw_free(&r);
done:
    hits_free(&g.l);
    free(pa);
    free(pathp);
}

/* ---- Grep ---- */

static const char *const types[][2] = {
    { "c", "*.{c,h}" }, { "cpp", "*.{cpp,cc,cxx,hpp,hh,hxx,h}" }, { "h", "*.h" }, { "py", "*.{py,pyw}" },
    { "js", "*.{js,mjs,cjs,jsx}" }, { "ts", "*.{ts,tsx,mts,cts}" }, { "md", "*.{md,markdown}" },
    { "markdown", "*.{md,markdown}" }, { "asm", "*.{s,asm,i,a68,68k}" }, { "rexx", "*.{rexx,rx,ab}" },
    { "guide", "*.guide" }, { "json", "*.json" }, { "html", "*.{html,htm}" }, { "txt", "*.txt" },
    { "sh", "*.{sh,bash,zsh}" }, { "go", "*.go" }, { "rust", "*.rs" }, { "java", "*.java" },
    { "make", "{Makefile,makefile,GNUmakefile,*.mk,*.mak,smakefile}" }, { "yaml", "*.{yml,yaml}" },
    { "xml", "*.xml" }, { "css", "*.css" }, { "lua", "*.lua" }, { "pascal", "*.{pas,pp}" },
    { "e", "*.e" }, { "basic", "*.{bas,amos}" }, { 0, 0 }
};

typedef struct gr {
    cl_tools *t;
    cl_re *re;
    int mode;                   /* 0 files, 1 content, 2 count */
    int numbers, multi, one_file;
    long before, after;
    char glob[256];
    int glob_path;              /* the glob has a '/': it matches the path below the root */
    char type_glob[64];
    long skip, limit;           /* offset, head_limit (0: none) */
    long emitted;               /* output lines/entries counted (after skip) */
    int full;                   /* the limit or the output cap is reached */
    long total;                 /* matches (count mode) */
    jw r;
    hits files;
} gr;

/* one output line (content mode), counted against offset and head_limit */
static void emit_line(gr *g, const char *path, char sep, long no, const char *s, long n)
{
    char num[16];
    if (g->full)
        return;
    if (g->skip > 0) {
        g->skip--;
        return;
    }
    if (!g->one_file) {
        jw_rawz(&g->r, path);
        jw_raw(&g->r, &sep, 1);
    }
    if (g->numbers && no > 0) {
        cl_ltoa(no, num);
        jw_rawz(&g->r, num);
        jw_raw(&g->r, &sep, 1);
    }
    if (n && s[n - 1] == '\r')
        n--;
    jw_raw(&g->r, s, n > 2000 ? 2000 : n);
    jw_raw(&g->r, "\n", 1);
    g->emitted++;
    if ((g->limit && g->emitted >= g->limit) || g->r.n >= TL_OUT_MAX)
        g->full = 1;
}

/* the "--" between groups of context lines, counted like a line */
static void emit_sep(gr *g)
{
    if (g->full)
        return;
    if (g->skip > 0) {
        g->skip--;
        return;
    }
    jw_rawz(&g->r, "--\n");
    g->emitted++;
    if ((g->limit && g->emitted >= g->limit) || g->r.n >= TL_OUT_MAX)
        g->full = 1;
}

static int name_ok(gr *g, const char *rel, const char *name)
{
    if (g->glob[0] && !glob_match(g->glob, g->glob_path ? rel : name))
        return 0;
    if (g->type_glob[0] && !glob_match(g->type_glob, name))
        return 0;
    return 1;
}

/* the matches in one file */
static int grep_file(gr *g, const char *path, long mtime)
{
    char *b = 0;
    long n = 0, i = 0, lineno = 1, count = 0, last_out = 0, after_left = 0;
    long ring[CTX_MAX], ringno[CTX_MAX];
    int nring = 0, k;
    if (g->t->sys->read(g->t->sys->u, path, GREP_FILE_MAX, &b, &n))
        return 0;
    if (tl_is_binary(b, n)) {
        free(b);
        return 0;
    }
    if (g->multi) {
        /* the whole file at once: a match may cross lines */
        long from = 0, ms, me;
        while (from <= n && re_search(g->re, b, n, from, &ms, &me) == 1) {
            count++;
            if (g->mode == 1) {
                /* the lines the match touches */
                long ls = ms, le, no = 1, q;
                while (ls > 0 && b[ls - 1] != '\n')
                    ls--;
                for (q = 0; q < ls; q++)
                    no += b[q] == '\n';
                while (ls < n && ls <= (me > ms ? me - 1 : ms) && !g->full) {
                    le = ls;
                    while (le < n && b[le] != '\n')
                        le++;
                    if (no > last_out)
                        emit_line(g, path, ':', no, b + ls, le - ls);
                    last_out = no;
                    no++;
                    ls = le + 1;
                }
            } else if (g->mode == 0)
                break;
            from = me > ms ? me : ms + 1;
            if (g->full)
                break;
        }
    } else {
        while (i < n && !g->full) {
            long e = i, ms, me, len;
            int hit;
            while (e < n && b[e] != '\n')
                e++;
            len = e - i;
            if (len && b[i + len - 1] == '\r')
                len--;
            hit = re_search(g->re, b + i, len, 0, &ms, &me) == 1;
            if (hit) {
                count++;
                if (g->mode == 0)
                    break;
                if (g->mode == 1) {
                    /* the lines before it, a "--" between groups apart */
                    if (last_out && lineno - nring > last_out + 1 && (g->before || g->after))
                        emit_sep(g);
                    for (k = 0; k < nring; k++)
                        if (ringno[k] > last_out) {
                            long rs = ring[k], re2 = rs;
                            while (re2 < n && b[re2] != '\n')
                                re2++;
                            emit_line(g, path, '-', ringno[k], b + rs, re2 - rs);
                        }
                    nring = 0;
                    emit_line(g, path, ':', lineno, b + i, e - i);
                    last_out = lineno;
                    after_left = g->after;
                }
            } else if (g->mode == 1) {
                if (after_left > 0) {
                    emit_line(g, path, '-', lineno, b + i, e - i);
                    last_out = lineno;
                    after_left--;
                } else if (g->before) {
                    if (nring == g->before) {
                        memmove(ring, ring + 1, sizeof(long) * (size_t)(nring - 1));
                        memmove(ringno, ringno + 1, sizeof(long) * (size_t)(nring - 1));
                        nring--;
                    }
                    ring[nring] = i;
                    ringno[nring++] = lineno;
                }
            }
            i = e + 1;
            lineno++;
        }
    }
    free(b);
    if (count) {
        g->total += count;
        if (g->mode != 1)
            hits_add(&g->files, path, mtime, count, GREP_KEEP);
    }
    return 0;
}

static int grep_hit(walk *w, const char *full, const char *rel, const cl_dirent *e)
{
    gr *g = (gr *)w->c;
    if (e->dir || !name_ok(g, rel, e->name))
        return 0;
    if (e->size > GREP_FILE_MAX)
        return 0;
    grep_file(g, full, e->mtime);
    return g->full || g->files.more;
}

void search_grep(cl_tools *t, jw *out, const char *id, jv in)
{
    char *pat = tl_prop(in, "pattern", 0), *pathp = tl_prop(in, "path", 0), *gl = tl_prop(in, "glob", 0);
    char *ty = tl_prop(in, "type", 0), *om = tl_prop(in, "output_mode", 0);
    char full[512], what[400], err[200], num[16];
    int outside = 0, kind, i;
    long c;
    gr g;
    walk w;
    memset(&g, 0, sizeof(g));
    jw_init(&g.r);
    if (!pat || !pathp || !gl || !ty || !om) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    g.t = t;
    g.mode = !strcmp(om, "content") ? 1 : !strcmp(om, "count") ? 2 : 0;
    g.numbers = tl_bool(in, "-n");
    g.multi = tl_bool(in, "multiline");
    c = tl_num(in, "-C", 0);
    g.before = tl_num(in, "-B", c);
    g.after = tl_num(in, "-A", c);
    if (g.before > CTX_MAX)
        g.before = CTX_MAX;
    if (g.after > 1000)
        g.after = 1000;
    g.skip = tl_num(in, "offset", 0);
    g.limit = tl_num(in, "head_limit", 0);
    if (*gl && to_glob(gl, g.glob, sizeof(g.glob))) {
        tl_error(t, out, id, "not a usable glob: ", g.glob);
        goto done;
    }
    g.glob_path = strchr(g.glob, '/') != 0;
    if (*ty) {
        for (i = 0; types[i][0]; i++)
            if (!strcmp(ty, types[i][0]))
                break;
        if (!types[i][0]) {
            tl_error(t, out, id, "unknown file type (use glob instead): ", ty);
            goto done;
        }
        cl_copy(g.type_glob, types[i][1], sizeof(g.type_glob));
    }
    g.re = re_compile(pat, (tl_bool(in, "-i") ? RE_ICASE : 0) | (g.multi ? RE_DOTALL : 0), err, sizeof(err));
    if (!g.re) {
        tl_error(t, out, id, "the pattern is not a usable regular expression: ", err);
        goto done;
    }
    if (tl_resolve(t, pathp, full, sizeof(full), &outside)) {
        tl_error(t, out, id, "not a usable path (it climbs above a volume's root): ", pathp);
        goto done;
    }
    tl_summary(what, sizeof(what), pat, full);
    if (tl_gate(t, out, id, T_GREP, what, outside, 1))
        goto done;
    kind = t->sys->kind(t->sys->u, full);
    if (kind == 1) {
        g.one_file = 1;
        grep_file(&g, full, t->sys->mtime ? t->sys->mtime(t->sys->u, full) : 0);
    } else if (kind == 2) {
        memset(&w, 0, sizeof(w));
        w.t = t;
        w.fn = grep_hit;
        w.c = &g;
        w.maxdepth = WALK_DEPTH;
        walk_run(&w, full);
    } else {
        tl_error(t, out, id, "no such file or directory: ", full);
        goto done;
    }
    if (g.mode == 1) {
        if (!g.r.n)
            jw_rawz(&g.r, "No matches found");
        else if (g.full)
            jw_rawz(&g.r, g.limit && g.emitted >= g.limit ? "" : "(output cut at 30000 characters)\n");
    } else {
        long k, shown = 0;
        jw body;
        jw_init(&body);
        if (g.mode == 0)
            qsort(g.files.h, (size_t)g.files.n, sizeof(hit), newest_first);
        for (k = g.skip; k < g.files.n && (!g.limit || shown < g.limit) && body.n < TL_OUT_MAX; k++, shown++) {
            if (g.mode == 2 && g.one_file) {
                cl_ltoa(g.files.h[k].count, num);
                jw_rawz(&body, num);
            } else {
                jw_rawz(&body, g.files.h[k].path);
                if (g.mode == 2) {
                    jw_raw(&body, ":", 1);
                    cl_ltoa(g.files.h[k].count, num);
                    jw_rawz(&body, num);
                }
            }
            jw_raw(&body, "\n", 1);
        }
        if (!g.files.n)
            jw_rawz(&g.r, g.mode == 0 ? "No files found" : "No matches found");
        else if (g.mode == 0) {
            jw_rawz(&g.r, "Found ");
            cl_ltoa(shown, num);
            jw_rawz(&g.r, num);
            jw_rawz(&g.r, shown == 1 ? " file\n" : " files\n");
            jw_raw(&g.r, body.p, body.n);
        } else {
            jw_raw(&g.r, body.p, body.n);
            jw_rawz(&g.r, "\nFound ");
            cl_ltoa(g.total, num);
            jw_rawz(&g.r, num);
            jw_rawz(&g.r, g.total == 1 ? " total occurrence across " : " total occurrences across ");
            cl_ltoa(g.files.n, num);
            jw_rawz(&g.r, num);
            jw_rawz(&g.r, g.files.n == 1 ? " file." : " files.");
        }
        if (g.files.more)
            jw_rawz(&g.r, "\n(the search stopped at 5000 files)");
        jw_free(&body);
    }
    tl_result(t, out, id, g.r.p, g.r.n, 0);
done:
    re_free(g.re);
    hits_free(&g.files);
    jw_free(&g.r);
    free(pat);
    free(pathp);
    free(gl);
    free(ty);
    free(om);
}
