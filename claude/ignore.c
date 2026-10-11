/* see ignore.h */
#include <stdlib.h>
#include <string.h>
#include "ignore.h"
#include "glob.h"
#include "path.h"
#include "util.h"

#define IG_FILE_MAX (256L * 1024)

static void set_free(ig_set *s)
{
    int i;
    for (i = 0; i < s->n; i++)
        free(s->r[i].pat);
    free(s->r);
    s->r = 0;
    s->n = 0;
}

int ig_parse(ig_set *s, const char *t, long n)
{
    long i = 0, cap = 0;
    while (i < n) {
        long a = i, e, k;
        ig_rule r;
        char *p;
        while (i < n && t[i] != '\n')
            i++;
        e = i++;
        if (e > a && t[e - 1] == '\r')
            e--;
        /* trailing blanks go unless escaped */
        while (e > a && t[e - 1] == ' ' && !(e - 2 >= a && t[e - 2] == '\\'))
            e--;
        if (e == a || t[a] == '#')
            continue;
        memset(&r, 0, sizeof(r));
        if (t[a] == '!') {
            r.neg = 1;
            a++;
        } else if (t[a] == '\\' && a + 1 < e && (t[a + 1] == '#' || t[a + 1] == '!'))
            a++;
        if (e > a && t[e - 1] == '/') {
            r.dir_only = 1;
            e--;
        }
        if (e == a)
            continue;
        /* a '/' at the start or inside anchors the pattern to its directory */
        for (k = a; k < e; k++)
            if (t[k] == '/')
                r.anchored = 1;
        if (t[a] == '/')
            a++;
        /* "**" + "/x" is anchored by its slash but matches at any depth: the same */
        p = (char *)malloc((size_t)(e - a) + 1);
        if (!p)
            return -1;
        memcpy(p, t + a, (size_t)(e - a));
        p[e - a] = 0;
        r.pat = p;
        if (s->n == cap) {
            long nc = cap ? cap * 2 : 16;
            ig_rule *nr = (ig_rule *)realloc(s->r, (size_t)nc * sizeof(ig_rule));
            if (!nr) {
                free(p);
                return -1;
            }
            s->r = nr;
            cap = nc;
        }
        s->r[s->n++] = r;
    }
    return 0;
}

int ig_match(const ig_set *s, const char *rel, int is_dir)
{
    int i;
    const char *base = strrchr(rel, '/');
    base = base ? base + 1 : rel;
    for (i = s->n - 1; i >= 0; i--) {
        const ig_rule *r = &s->r[i];
        if (r->dir_only && !is_dir)
            continue;
        if (glob_match(r->pat, r->anchored ? rel : base))
            return r->neg ? -1 : 1;
    }
    return 0;
}

static void load_file(ig_set *s, cl_sys *sys, const char *dir, const char *name)
{
    char p[600], *b = 0;
    long n = 0;
    if (path_join(dir, name, p, sizeof(p)) || sys->kind(sys->u, p) != 1)
        return;
    if (sys->read(sys->u, p, IG_FILE_MAX, &b, &n) == 0) {
        ig_parse(s, b, n);
        free(b);
    }
}

int ig_load(ig_dir *d, cl_sys *sys, const char *full, ig_dir *parent, const char *custom)
{
    char g[600];
    memset(d, 0, sizeof(*d));
    d->parent = parent;
    d->path = (char *)malloc(strlen(full) + 1);
    if (!d->path)
        return -1;
    strcpy(d->path, full);
    if (custom)
        load_file(&d->custom, sys, full, custom);
    load_file(&d->ignore, sys, full, ".ignore");
    load_file(&d->git, sys, full, ".gitignore");
    if (path_join(full, ".git", g, sizeof(g)) == 0 && sys->kind(sys->u, g) > 0) {
        d->has_git = 1;
        if (sys->kind(sys->u, g) == 2)
            load_file(&d->exclude, sys, g, "info/exclude");
    }
    return 0;
}

void ig_free(ig_dir *d)
{
    set_free(&d->custom);
    set_free(&d->ignore);
    set_free(&d->git);
    set_free(&d->exclude);
    free(d->path);
    d->path = 0;
}

ig_dir *ig_chain(cl_sys *sys, const char *full, const char *custom)
{
    char up[600];
    ig_dir *top = 0, *d;
    if (path_parent(full, up, sizeof(up)) == 0 && strcmp(up, full)) {
        top = ig_chain(sys, up, custom);
        if (!top)
            return 0;
    }
    d = (ig_dir *)malloc(sizeof(ig_dir));
    if (!d || ig_load(d, sys, full, top, custom)) {
        free(d);
        ig_free_chain(top);
        return 0;
    }
    return d;
}

void ig_free_chain(ig_dir *d)
{
    while (d) {
        ig_dir *p = d->parent;
        ig_free(d);
        free(d);
        d = p;
    }
}

/* full below dir: the part after dir's name and its '/' (0 not below) */
static const char *below(const char *dir, const char *full)
{
    long k = (long)strlen(dir);
    if (strncmp(dir, full, (size_t)k))
        return 0;
    if (full[k] == '/')
        return full + k + 1;
    if (k && (dir[k - 1] == ':' || dir[k - 1] == '/'))
        return full + k;
    return 0;
}

/* the ignore crate's Ignore::matched_ignore: per kind of file the deepest
 * match counts; a parent's .gitignore stops at a repository's top (one with
 * .git) only where git is required (rg; fd inside a repository) */
int ig_ignored(const ig_dir *d, const char *full, int is_dir, int require_git)
{
    const ig_dir *q;
    int m_custom = 0, m_ignore = 0, m_git = 0, m_ex = 0, any_git = !require_git, saw_git = 0;
    for (q = d; q && !any_git; q = q->parent)
        any_git = q->has_git;
    for (q = d; q; q = q->parent) {
        const char *rel = below(q->path, full);
        if (!rel || !*rel)
            continue;
        if (!m_custom)
            m_custom = ig_match(&q->custom, rel, is_dir);
        if (!m_ignore)
            m_ignore = ig_match(&q->ignore, rel, is_dir);
        if (any_git && !saw_git && !m_git)
            m_git = ig_match(&q->git, rel, is_dir);
        if (any_git && !saw_git && !m_ex)
            m_ex = ig_match(&q->exclude, rel, is_dir);
        saw_git = saw_git || (q->has_git && require_git);
    }
    if (m_custom)
        return m_custom > 0;
    if (m_ignore)
        return m_ignore > 0;
    if (m_git)
        return m_git > 0;
    return m_ex > 0;
}
