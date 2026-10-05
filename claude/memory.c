/* memory -- see memory.h. */
#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "path.h"
#include "config.h"
#include "util.h"

void mem_init(cl_memory *m)
{
    memset(m, 0, sizeof(*m));
    jw_init(&m->text);
}

void mem_free(cl_memory *m)
{
    jw_free(&m->text);
    free(m->rules);
    mem_init(m);
}

/* Claude Code leaves block-level HTML comments out of memory files (a
 * comment a line starts with, to its end; code blocks untouched): the
 * new length */
static long strip_comments(char *b, long n)
{
    long i = 0, o = 0;
    int fence = 0;
    while (i < n) {
        long e = i, j = i;
        while (e < n && b[e] != '\n')
            e++;
        while (j < e && (b[j] == ' ' || b[j] == '\t'))
            j++;
        if (e - j >= 3 && !strncmp(b + j, "```", 3))
            fence = !fence;
        if (!fence && e - j >= 4 && !strncmp(b + j, "<!--", 4)) {
            /* to the comment's end, maybe lines later; what follows it on its line stays */
            long k = j + 4;
            while (k + 2 < n && strncmp(b + k, "-->", 3))
                k++;
            if (k + 2 >= n) {
                i = n;
                break;
            }
            k += 3;
            while (k < n && (b[k] == ' ' || b[k] == '\t'))
                k++;
            if (k < n && b[k] == '\n')
                k++;
            i = k;
            continue;
        }
        if (e < n)
            e++;
        memmove(b + o, b + i, (size_t)(e - i));
        o += e - i;
        i = e;
    }
    return o;
}

static int excluded(const cl_memory *m, const char *path)
{
    int i;
    for (i = 0; i < m->nexcl; i++)
        if (cfg_glob(m->excl[i], path, 1))
            return 1;
    return 0;
}

static int seen(const cl_memory *m, const char *path)
{
    int i;
    for (i = 0; i < m->n; i++)
        if (cl_strieq(m->f[i].path, path))
            return 1;
    return 0;
}

static const char *label(int kind)
{
    switch (kind) {
    case MEM_USER:
        return "user's private global instructions for all projects";
    case MEM_LOCAL:
        return "user's private project instructions, not checked in";
    case MEM_IMPORT:
        return "imported";
    case MEM_RULE:
        return "project rules";
    case MEM_AUTO:
        return "auto memory: what you saved for this project in earlier sessions";
    default:
        return "project instructions, checked into the codebase";
    }
}

static int add_file(cl_memory *m, cl_sys *sys, const char *path, int kind, int depth, jw *out);
static void rules_in(cl_memory *m, cl_sys *sys, const char *dir, int depth);

/* the @imports of a file's text, each added after it */
static void imports(cl_memory *m, cl_sys *sys, const char *file, const char *s, long n, int depth, jw *out)
{
    long i = 0;
    int fence = 0;
    char dir[300];
    if (depth >= MEM_DEPTH || path_parent(file, dir, sizeof(dir)))
        return;
    while (i < n) {
        long e = i, j;
        int tick = 0;
        while (e < n && s[e] != '\n')
            e++;
        j = i;
        while (j < e && (s[j] == ' ' || s[j] == '\t'))
            j++;
        if (e - j >= 3 && !strncmp(s + j, "```", 3)) {
            fence = !fence;
            i = e + 1;
            continue;
        }
        for (j = i; !fence && j < e; j++) {
            char name[300], full[300];
            long k, l;
            if (s[j] == '`')
                tick = !tick;
            if (tick || s[j] != '@' || (j > i && s[j - 1] != ' ' && s[j - 1] != '\t' && s[j - 1] != '('))
                continue;
            for (k = j + 1; k < e && s[k] != ' ' && s[k] != '\t' && s[k] != '\r'; k++)
                ;
            l = k - j - 1;
            if (l <= 0 || l >= (long)sizeof(name))
                continue;
            memcpy(name, s + j + 1, (size_t)l);
            name[l] = 0;
            /* trailing punctuation is the sentence's, unless the name has it */
            for (;;) {
                int ok;
                if (name[0] == '~' && name[1] == '/')
                    ok = m->home && path_join(m->home, name + 2, full, sizeof(full)) == 0;
                else
                    ok = path_join(dir, name, full, sizeof(full)) == 0;
                if (ok && sys->kind(sys->u, full) == 1) {
                    if (!seen(m, full))
                        add_file(m, sys, full, MEM_IMPORT, depth + 1, out);
                    break;
                }
                if (!l || !strchr(".,;:)!?'\"", name[l - 1]))
                    break;
                name[--l] = 0;
            }
            j = k;
        }
        i = e + 1;
    }
}

static int add_file(cl_memory *m, cl_sys *sys, const char *path, int kind, int depth, jw *out)
{
    char *b = 0;
    long n = 0;
    if (m->n >= MEM_FILES || seen(m, path) || sys->kind(sys->u, path) != 1 || excluded(m, path))
        return 0;
    if (sys->read(sys->u, path, MEM_FILE_MAX, &b, &n))
        return 0;
    n = strip_comments(b, n);
    if (kind == MEM_RULE && n >= 4 && !strncmp(b, "---", 3)) {
        /* a rule's frontmatter is not its text */
        char *e = strstr(b + 3, "\n---");
        if (e) {
            e += 4;
            while (e < b + n && (*e == '\n' || *e == '\r'))
                e++;
            n -= (long)(e - b);
            memmove(b, e, (size_t)n);
        }
    }
    cl_copy(m->f[m->n].path, path, sizeof(m->f[0].path));
    m->f[m->n].kind = kind;
    m->f[m->n].size = n;
    m->n++;
    if (out->n)
        jw_rawz(out, "\n\n");
    jw_rawz(out, "Contents of ");
    jw_rawz(out, path);
    jw_rawz(out, " (");
    jw_rawz(out, label(kind));
    jw_rawz(out, "):\n\n");
    jw_raw(out, b, n);
    imports(m, sys, path, b, n, depth, out);
    free(b);
    return 1;
}

static int add_in(cl_memory *m, cl_sys *sys, const char *dir, const char *name, int kind, jw *out)
{
    char p[300];
    return path_join(dir, name, p, sizeof(p)) == 0 ? add_file(m, sys, p, kind, 0, out) : 0;
}

/* One directory's memory: CLAUDE.md and AMIGA.md, AGENTS.md only when
 * neither is there (Claude Code reads AGENTS.md in place of a missing
 * CLAUDE.md), and the private CLAUDE.local.md */
static int dir_files(cl_memory *m, cl_sys *sys, const char *dir, int kind, jw *out)
{
    int got = add_in(m, sys, dir, "CLAUDE.md", kind, out);
    got += add_in(m, sys, dir, "AMIGA.md", kind, out);
    if (!got)
        got += add_in(m, sys, dir, "AGENTS.md", kind, out);
    got += add_in(m, sys, dir, "CLAUDE.local.md", MEM_LOCAL, out);
    return got;
}

void mem_file_of(int kind, const char *home, const char *root, char *out, long cap)
{
    if (kind == MEM_USER)
        path_join(home, "CLAUDE.md", out, cap);
    else if (kind == MEM_LOCAL)
        path_join(root, "CLAUDE.local.md", out, cap);
    else
        path_join(root, "CLAUDE.md", out, cap);
}

int mem_load(cl_memory *m, cl_sys *sys, const char *home, const char *root)
{
    char anc[8][300], p[300];
    int na = 0, i, got = 0;
    m->home = home;
    if (home && *home)
        got += add_in(m, sys, home, "CLAUDE.md", MEM_USER, &m->text);
    /* the ancestors, the volume's root first */
    cl_copy(p, root, sizeof(p));
    while (na < 8 && path_parent(p, anc[na], sizeof(anc[0])) == 0 && anc[na][0]) {
        cl_copy(p, anc[na], sizeof(p));
        na++;
    }
    for (i = na - 1; i >= 0; i--)
        got += dir_files(m, sys, anc[i], MEM_PROJECT, &m->text);
    got += dir_files(m, sys, root, MEM_PROJECT, &m->text);
    got += add_in(m, sys, root, ".claude/CLAUDE.md", MEM_PROJECT, &m->text);
    /* the rules: the user's, then the project's */
    if (home && *home && path_join(home, "rules", p, sizeof(p)) == 0)
        rules_in(m, sys, p, 0);
    if (path_join(root, ".claude/rules", p, sizeof(p)) == 0)
        rules_in(m, sys, p, 0);
    (void)got;
    return m->n;
}

/* a rule file's "paths:" globs (a list, [a, b] or one string) into out: 1 when it has them */
static int rule_paths(const char *t, long n, char *out, long cap)
{
    long i, e;
    out[0] = 0;
    if (n < 4 || strncmp(t, "---", 3))
        return 0;
    for (i = 4; i < n;) {
        for (e = i; e < n && t[e] != '\n'; e++)
            ;
        if (e - i >= 3 && !strncmp(t + i, "---", 3))
            break;
        if (!strncmp(t + i, "paths:", 6)) {
            long j = i + 6, k;
            char item[200];
            while (j < e && (t[j] == ' ' || t[j] == '[' || t[j] == '"' || t[j] == '\''))
                j++;
            if (j < e) {
                /* inline: "a, b" or [a, b] */
                for (k = 0; j < e && t[j] != ']' && k < (long)sizeof(item) - 1; j++)
                    if (t[j] != '"' && t[j] != '\'' && t[j] != '\r')
                        item[k++] = t[j];
                item[k] = 0;
                cl_cat(out, item, cap);
            } else {
                /* a list: "  - glob" lines */
                long a = e + 1;
                while (a < n) {
                    long b2 = a, c;
                    for (c = a; c < n && t[c] != '\n'; c++)
                        ;
                    while (b2 < c && (t[b2] == ' ' || t[b2] == '\t'))
                        b2++;
                    if (b2 >= c || t[b2] != '-')
                        break;
                    b2++;
                    while (b2 < c && (t[b2] == ' ' || t[b2] == '"' || t[b2] == '\''))
                        b2++;
                    for (k = 0; b2 < c && t[b2] != '"' && t[b2] != '\'' && t[b2] != '\r' && k < (long)sizeof(item) - 1;
                         b2++)
                        item[k++] = t[b2];
                    item[k] = 0;
                    if (out[0])
                        cl_cat(out, ", ", cap);
                    cl_cat(out, item, cap);
                    a = c + 1;
                }
            }
            return out[0] != 0;
        }
        i = e + 1;
    }
    return 0;
}

typedef struct rscan {
    cl_dirent e[64];
    int n;
} rscan;

static int rcollect(void *c, const cl_dirent *e)
{
    rscan *s = (rscan *)c;
    if (s->n < 64)
        s->e[s->n++] = *e;
    return 0;
}

/* the *.md files of a rules directory (and its subdirectories): those
 * without paths: now, the others kept for mem_rules */
static void rules_in(cl_memory *m, cl_sys *sys, const char *dir, int depth)
{
    rscan *s;
    int i;
    if (depth > 3 || sys->kind(sys->u, dir) != 2 || !sys->list)
        return;
    s = (rscan *)malloc(sizeof(rscan));
    if (!s)
        return;
    s->n = 0;
    sys->list(sys->u, dir, rcollect, s);
    for (i = 0; i < s->n; i++) {
        char p[300], globs[200], *b = 0;
        long l = (long)strlen(s->e[i].name), n = 0;
        if (path_join(dir, s->e[i].name, p, sizeof(p)))
            continue;
        if (s->e[i].dir) {
            rules_in(m, sys, p, depth + 1);
            continue;
        }
        if (l < 4 || !cl_strieq(s->e[i].name + l - 3, ".md") || excluded(m, p))
            continue;
        if (sys->read(sys->u, p, MEM_FILE_MAX, &b, &n))
            continue;
        if (rule_paths(b, n, globs, sizeof(globs))) {
            if (!m->rules)
                m->rules = (cl_memrule *)calloc(MEM_RULES, sizeof(cl_memrule));
            if (m->rules && m->nrules < MEM_RULES) {
                cl_copy(m->rules[m->nrules].path, p, sizeof(m->rules[0].path));
                cl_copy(m->rules[m->nrules].globs, globs, sizeof(m->rules[0].globs));
                m->nrules++;
            }
        } else
            add_file(m, sys, p, MEM_RULE, 0, &m->text);
        free(b);
    }
    free(s);
}

int mem_rules(cl_memory *m, cl_sys *sys, const char *root, const char *path, jw *out)
{
    int i, got = 0;
    long rl = (long)strlen(root);
    const char *rel = path;
    if (path_inside(root, path) && (long)strlen(path) > rl) {
        rel = path + rl;
        if (*rel == '/')
            rel++;
    }
    for (i = 0; i < m->nrules; i++) {
        const char *g = m->rules[i].globs;
        if (m->rules[i].done)
            continue;
        while (*g) {
            char one[200];
            int k = 0;
            while (*g == ' ' || *g == ',')
                g++;
            while (*g && *g != ',' && k < (int)sizeof(one) - 1)
                one[k++] = *g++;
            while (k && one[k - 1] == ' ')
                k--;
            one[k] = 0;
            if (k && (cfg_glob(one, rel, 1) || cfg_glob(one, path, 1))) {
                m->rules[i].done = 1;
                got += add_file(m, sys, m->rules[i].path, MEM_RULE, 0, out);
                break;
            }
        }
    }
    return got;
}

int mem_auto(cl_memory *m, cl_sys *sys, const char *dir)
{
    char p[300], *b = 0;
    long n = 0, i, lines = 0;
    cl_copy(m->auto_dir, dir, sizeof(m->auto_dir));
    if (path_join(dir, "MEMORY.md", p, sizeof(p)) || sys->kind(sys->u, p) != 1 ||
        sys->read(sys->u, p, MEM_FILE_MAX, &b, &n))
        return 0;
    for (i = 0; i < n && lines < MEM_AUTO_LINES; i++)
        lines += b[i] == '\n';
    if (m->n < MEM_FILES) {
        cl_copy(m->f[m->n].path, p, sizeof(m->f[0].path));
        m->f[m->n].kind = MEM_AUTO;
        m->f[m->n].size = i;
        m->n++;
    }
    if (m->text.n)
        jw_rawz(&m->text, "\n\n");
    jw_rawz(&m->text, "Contents of ");
    jw_rawz(&m->text, p);
    jw_rawz(&m->text, " (");
    jw_rawz(&m->text, label(MEM_AUTO));
    jw_rawz(&m->text, "):\n\n");
    jw_raw(&m->text, b, i);
    free(b);
    return 1;
}

int mem_nested(cl_memory *m, cl_sys *sys, const char *root, const char *path, jw *out)
{
    char dirs[8][300], p[300];
    int nd = 0, i, got = 0;
    if (!path_inside(root, path) || cl_strieq(root, path))
        return 0;
    if (sys->kind(sys->u, path) == 2)
        cl_copy(p, path, sizeof(p));
    else if (path_parent(path, p, sizeof(p)))
        return 0;
    /* from the file's directory up to (not including) the root */
    while (nd < 8 && path_inside(root, p) && !cl_strieq(root, p)) {
        cl_copy(dirs[nd++], p, sizeof(dirs[0]));
        if (path_parent(dirs[nd - 1], p, sizeof(p)))
            break;
    }
    for (i = nd - 1; i >= 0; i--)
        got += dir_files(m, sys, dirs[i], MEM_NESTED, out);
    return got;
}
