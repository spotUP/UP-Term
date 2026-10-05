/* memory -- see memory.h. */
#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "path.h"
#include "util.h"

void mem_init(cl_memory *m)
{
    memset(m, 0, sizeof(*m));
    jw_init(&m->text);
}

void mem_free(cl_memory *m)
{
    jw_free(&m->text);
    mem_init(m);
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
    default:
        return "project instructions, checked into the codebase";
    }
}

static int add_file(cl_memory *m, cl_sys *sys, const char *path, int kind, int depth, jw *out);

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
    if (m->n >= MEM_FILES || seen(m, path) || sys->kind(sys->u, path) != 1)
        return 0;
    if (sys->read(sys->u, path, MEM_FILE_MAX, &b, &n))
        return 0;
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
    for (i = na - 1; i >= 0; i--) {
        got += add_in(m, sys, anc[i], "CLAUDE.md", MEM_PROJECT, &m->text);
        got += add_in(m, sys, anc[i], "AMIGA.md", MEM_PROJECT, &m->text);
    }
    got += add_in(m, sys, root, "CLAUDE.md", MEM_PROJECT, &m->text);
    got += add_in(m, sys, root, "AMIGA.md", MEM_PROJECT, &m->text);
    got += add_in(m, sys, root, "AGENTS.md", MEM_PROJECT, &m->text);
    got += add_in(m, sys, root, ".claude/CLAUDE.md", MEM_PROJECT, &m->text);
    got += add_in(m, sys, root, "CLAUDE.local.md", MEM_LOCAL, &m->text);
    (void)got;
    return m->n;
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
    for (i = nd - 1; i >= 0; i--) {
        got += add_in(m, sys, dirs[i], "CLAUDE.md", MEM_NESTED, out);
        got += add_in(m, sys, dirs[i], "AMIGA.md", MEM_NESTED, out);
    }
    return got;
}
