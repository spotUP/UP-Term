/* commands -- see commands.h. */
#include <stdlib.h>
#include <string.h>
#include "commands.h"
#include "config.h"
#include "path.h"
#include "util.h"

static char *dupn(const char *s, long n)
{
    char *d = (char *)malloc((size_t)n + 1);
    if (d) {
        memcpy(d, s, (size_t)n);
        d[n] = 0;
    }
    return d;
}

void defs_init(cl_defs *s)
{
    memset(s, 0, sizeof(*s));
}

void def_free(cl_def *d)
{
    free(d->description);
    free(d->body);
    free(d->tools);
    free(d->model);
    free(d->hint);
    d->description = d->body = d->tools = d->model = d->hint = 0;
}

void defs_free(cl_defs *s)
{
    int i;
    for (i = 0; i < s->n; i++)
        def_free(&s->d[i]);
    free(s->d);
    defs_init(s);
}

static int is_sp(int c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

/* a frontmatter value: trimmed, quotes taken off */
static char *value(const char *s, long n)
{
    while (n && is_sp((unsigned char)*s)) {
        s++;
        n--;
    }
    while (n && is_sp((unsigned char)s[n - 1]))
        n--;
    if (n >= 2 && (s[0] == '"' || s[0] == '\'') && s[n - 1] == s[0]) {
        s++;
        n -= 2;
    }
    return dupn(s, n);
}

static void set(char **f, char *v)
{
    if (!v)
        return;
    free(*f);
    *f = v;
}

int defs_parse(const char *t, long n, cl_def *d)
{
    long i = 0, body = 0;
    jw lw;
    char lkey[32];
    jw_init(&lw);
    lkey[0] = 0;
    if (!d->description)
        d->description = dupn("", 0);
    if (!d->tools)
        d->tools = dupn("", 0);
    if (!d->model)
        d->model = dupn("", 0);
    if (!d->hint)
        d->hint = dupn("", 0);
    if (!d->description || !d->tools || !d->model || !d->hint)
        return -1;
    if (n >= 4 && !strncmp(t, "---", 3) && (t[3] == '\n' || t[3] == '\r')) {
        i = t[3] == '\r' ? 5 : 4;
        while (i < n) {
            long e = i, c;
            while (e < n && t[e] != '\n')
                e++;
            if (e - i >= 3 && !strncmp(t + i, "---", 3)) {
                body = e < n ? e + 1 : e;
                break;
            }
            for (c = i; c < e && t[c] != ':'; c++)
                ;
            if (lkey[0] && c == e) {
                /* "  - item" under a key with no value */
                long k = i;
                while (k < e && is_sp((unsigned char)t[k]))
                    k++;
                if (k < e && t[k] == '-') {
                    char *v = value(t + k + 1, e - k - 1);
                    if (v) {
                        if (lw.n)
                            jw_rawz(&lw, ", ");
                        jw_rawz(&lw, v);
                        free(v);
                    }
                }
            } else if (c < e) {
                char key[32];
                long kl = c - i;
                char *v;
                while (kl && is_sp((unsigned char)t[i + kl - 1]))
                    kl--;
                if (kl >= (long)sizeof(key))
                    kl = (long)sizeof(key) - 1;
                memcpy(key, t + i, (size_t)kl);
                key[kl] = 0;
                if (lkey[0] && lw.n) {
                    if (!strcmp(lkey, "tools") || !strcmp(lkey, "allowed-tools"))
                        set(&d->tools, dupn(lw.p, lw.n));
                }
                jw_reset(&lw);
                lkey[0] = 0;
                v = value(t + c + 1, e - c - 1);
                if (!v) {
                    jw_free(&lw);
                    return -1;
                }
                if (!*v)
                    cl_copy(lkey, key, sizeof(lkey));
                if (!strcmp(key, "name") && *v)
                    cl_copy(d->name, v, sizeof(d->name));
                if (!strcmp(key, "description"))
                    set(&d->description, v);
                else if (!strcmp(key, "allowed-tools") || !strcmp(key, "tools"))
                    set(&d->tools, v);
                else if (!strcmp(key, "model"))
                    set(&d->model, v);
                else if (!strcmp(key, "argument-hint"))
                    set(&d->hint, v);
                else {
                    if (!strcmp(key, "keep-coding-instructions"))
                        d->keep_coding = !strcmp(v, "true");
                    else if (!strcmp(key, "disable-model-invocation"))
                        d->no_model = !strcmp(v, "true");
                    free(v);
                }
            }
            i = e + 1;
        }
        if (lkey[0] && lw.n && (!strcmp(lkey, "tools") || !strcmp(lkey, "allowed-tools")))
            set(&d->tools, dupn(lw.p, lw.n));
    }
    jw_free(&lw);
    while (body < n && (t[body] == '\n' || t[body] == '\r'))
        body++;
    set(&d->body, dupn(t + body, n - body));
    return d->body ? 0 : -1;
}

static int add(cl_defs *s, cl_def *d)
{
    if (s->n == s->cap) {
        int nc = s->cap ? s->cap * 2 : 16;
        cl_def *q = (cl_def *)realloc(s->d, (size_t)nc * sizeof(cl_def));
        if (!q)
            return -1;
        s->d = q;
        s->cap = nc;
    }
    s->d[s->n++] = *d;
    return 0;
}

/* one file read and parsed; name the default name */
static void load_file(cl_defs *s, cl_sys *sys, int type, int src, const char *path, const char *name)
{
    char *b = 0;
    long n = 0;
    cl_def d;
    memset(&d, 0, sizeof(d));
    d.type = type;
    d.src = src;
    cl_copy(d.name, name, sizeof(d.name));
    cl_copy(d.path, path, sizeof(d.path));
    if (sys->read(sys->u, path, 128L * 1024, &b, &n))
        return;
    if (defs_parse(b, n, &d) || add(s, &d))
        def_free(&d);
    free(b);
}

typedef struct scan {
    cl_dirent e[64];
    int n;
} scan;

static int collect(void *c, const cl_dirent *e)
{
    scan *sc = (scan *)c;
    if (sc->n < 64)
        sc->e[sc->n++] = *e;
    return 0;
}

static int md_name(const char *f, char *name, long cap)
{
    long l = (long)strlen(f);
    if (l < 4 || !cl_strieq(f + l - 3, ".md") || l - 3 >= cap)
        return -1;
    memcpy(name, f, (size_t)(l - 3));
    name[l - 3] = 0;
    return 0;
}

static void load_dir(cl_defs *s, cl_sys *sys, int type, int src, const char *dir, int depth)
{
    scan *sc;
    int i;
    if (sys->kind(sys->u, dir) != 2)
        return;
    sc = (scan *)malloc(sizeof(scan));
    if (!sc)
        return;
    sc->n = 0;
    sys->list(sys->u, dir, collect, sc);
    for (i = 0; i < sc->n; i++) {
        char p[300], name[64];
        if (path_join(dir, sc->e[i].name, p, sizeof(p)))
            continue;
        if (type == DEF_SKILL) {
            char sk[300];
            if (sc->e[i].dir && path_join(p, "SKILL.md", sk, sizeof(sk)) == 0 && sys->kind(sys->u, sk) == 1)
                load_file(s, sys, type, src, sk, sc->e[i].name);
        } else if (sc->e[i].dir) {
            if (depth < 2)
                load_dir(s, sys, type, src, p, depth + 1);
        } else if (md_name(sc->e[i].name, name, sizeof(name)) == 0)
            load_file(s, sys, type, src, p, name);
    }
    free(sc);
}

static const char explanatory[] =
    "Besides doing the work, explain it: before and after writing code, give short \"Insight\" notes on "
    "the choices made -- why this approach, the trade-offs, the patterns of this codebase and of "
    "AmigaOS involved. Keep the notes brief and specific to the code at hand.";
static const char learning[] =
    "Teach while working: share short insights on the choices you make, and where a small, meaningful "
    "piece of code (five to ten lines: a decision, a condition, a key function body) would teach the "
    "user something, leave it to them -- write the surrounding code, mark the spot with a TODO(human) "
    "comment and ask them to fill it in, then review what they wrote.";

static void builtin(cl_defs *s, const char *name, const char *desc, const char *body)
{
    cl_def d;
    memset(&d, 0, sizeof(d));
    d.type = DEF_STYLE;
    d.src = DEF_BUILTIN;
    d.keep_coding = 1;
    cl_copy(d.name, name, sizeof(d.name));
    d.description = dupn(desc, (long)strlen(desc));
    d.body = dupn(body, (long)strlen(body));
    d.tools = dupn("", 0);
    d.model = dupn("", 0);
    d.hint = dupn("", 0);
    if (!d.description || !d.body || !d.tools || !d.model || !d.hint || add(s, &d))
        def_free(&d);
}

int defs_load(cl_defs *s, cl_sys *sys, const char *home, const char *root)
{
    static const char *const sub[DEF_NTYPES] = { "commands", "agents", "skills", "output-styles" };
    char base[2][300], p[300];
    int src, t;
    builtin(s, "Default", "Claude Code's own: concise, for doing software work", "");
    builtin(s, "Explanatory", "Explains the choices and the codebase while it works", explanatory);
    builtin(s, "Learning", "Works with you: leaves small pieces for you to write (TODO(human))", learning);
    cl_copy(base[0], home ? home : "", sizeof(base[0]));
    if (path_join(root, ".claude", base[1], sizeof(base[1])))
        base[1][0] = 0;
    /* the project's first: they hide the user's of the same name */
    for (src = 1; src >= 0; src--) {
        if (!base[src][0])
            continue;
        for (t = 0; t < DEF_NTYPES; t++)
            if (path_join(base[src], sub[t], p, sizeof(p)) == 0)
                load_dir(s, sys, t, src ? CFG_PROJECT : CFG_USER, p, 0);
    }
    return s->n;
}

const cl_def *defs_find(const cl_defs *s, int type, const char *name)
{
    int i;
    for (i = 0; i < s->n; i++)
        if (s->d[i].type == type && cl_strieq(s->d[i].name, name))
            return &s->d[i];
    return 0;
}

/* the i-th of a type, hidden duplicates skipped */
const cl_def *defs_nth(const cl_defs *s, int type, int k)
{
    int i;
    for (i = 0; i < s->n; i++)
        if (s->d[i].type == type && defs_find(s, type, s->d[i].name) == &s->d[i] && k-- == 0)
            return &s->d[i];
    return 0;
}

int defs_count(const cl_defs *s, int type)
{
    int k = 0;
    while (defs_nth(s, type, k))
        k++;
    return k;
}

int def_has_tool(const cl_def *d, const char *tool)
{
    const char *p = d->tools;
    long tl = (long)strlen(tool);
    while (p && *p) {
        const char *e = p;
        int depth = 0;
        while (*e && (depth || (*e != ',' && *e != ' '))) {
            if (*e == '(')
                depth++;
            else if (*e == ')')
                depth--;
            e++;
        }
        if (e - p >= tl && !strncmp(p, tool, (size_t)tl) && (e - p == tl || p[tl] == '('))
            return 1;
        p = *e ? e + 1 : e;
        while (*p == ' ' || *p == ',')
            p++;
    }
    return 0;
}

/* the arguments split like a shell's words ("a b" is one) */
static int split(const char *a, char w[9][256])
{
    int n = 0;
    while (*a && n < 9) {
        long k = 0;
        char q = 0;
        while (*a == ' ' || *a == '\t')
            a++;
        if (!*a)
            break;
        if (*a == '"' || *a == '\'')
            q = *a++;
        while (*a && (q ? *a != q : (*a != ' ' && *a != '\t'))) {
            if (k < 255)
                w[n][k++] = *a;
            a++;
        }
        if (q && *a)
            a++;
        w[n][k] = 0;
        n++;
    }
    return n;
}

int cmd_expand(const cl_def *d, const char *args, cl_sys *sys, const char *root, jw *out, char *err, long cap)
{
    const char *s = d->body;
    char w[9][256];
    int nw = split(args ? args : "", w), had_args = 0;
    err[0] = 0;
    while (*s) {
        if (s[0] == '$' && !strncmp(s, "$ARGUMENTS", 10)) {
            jw_rawz(out, args ? args : "");
            had_args = 1;
            s += 10;
        } else if (s[0] == '$' && s[1] >= '1' && s[1] <= '9') {
            int k = s[1] - '1';
            if (k < nw)
                jw_rawz(out, w[k]);
            had_args = 1;
            s += 2;
        } else if (s[0] == '!' && s[1] == '`') {
            const char *e = strchr(s + 2, '`');
            char cmd[512];
            long cl;
            if (!e) {
                jw_raw(out, s, 1);
                s++;
                continue;
            }
            cl = (long)(e - s - 2);
            if (cl >= (long)sizeof(cmd))
                cl = (long)sizeof(cmd) - 1;
            memcpy(cmd, s + 2, (size_t)cl);
            cmd[cl] = 0;
            if (!def_has_tool(d, "Bash") && !def_has_tool(d, "run_command")) {
                cl_copy(err, "the command runs !`", cap);
                cl_cat(err, cmd, cap);
                cl_cat(err, "` but its allowed-tools has no Bash", cap);
                return -1;
            }
            {
                char *o = (char *)malloc(16384);
                long on = 0, rc = 0;
                if (!o) {
                    cl_copy(err, "out of memory", cap);
                    return -1;
                }
                if (sys->run(sys->u, cmd, 30, o, 16383, &on, &rc) < 0) {
                    cl_copy(err, "!`", cap);
                    cl_cat(err, cmd, cap);
                    cl_cat(err, "` did not run: ", cap);
                    cl_cat(err, sys->err(sys->u), cap);
                    free(o);
                    return -1;
                }
                while (on && (o[on - 1] == '\n' || o[on - 1] == '\r'))
                    on--;
                jw_raw(out, o, on);
                free(o);
            }
            s = e + 1;
        } else if (s[0] == '@' && (s == d->body || s[-1] == ' ' || s[-1] == '\n' || s[-1] == '\t') && s[1] &&
                   s[1] != ' ' && s[1] != '\n') {
            /* @file: the file's text, inlined */
            char name[300], full[300];
            long k = 0;
            const char *e = s + 1;
            char *b = 0;
            long bn = 0;
            while (*e && *e != ' ' && *e != '\n' && *e != '\t' && k < 299)
                name[k++] = *e++;
            name[k] = 0;
            while (k && strchr(".,;:)!?", name[k - 1])) {
                name[--k] = 0;
                e--;
            }
            if (path_join(root, name, full, sizeof(full)) == 0 && sys->kind(sys->u, full) == 1 &&
                sys->read(sys->u, full, 64L * 1024, &b, &bn) == 0) {
                jw_rawz(out, name);
                jw_rawz(out, ":\n```\n");
                jw_raw(out, b, bn);
                if (bn && b[bn - 1] != '\n')
                    jw_raw(out, "\n", 1);
                jw_rawz(out, "```\n");
                free(b);
                s = e;
            } else {
                jw_raw(out, s, 1);
                s++;
            }
        } else {
            jw_raw(out, s, 1);
            s++;
        }
    }
    if (!had_args && args && *args) {
        jw_rawz(out, "\n\nARGUMENTS: ");
        jw_rawz(out, args);
    }
    if (out->oom) {
        cl_copy(err, "out of memory", cap);
        return -1;
    }
    return 0;
}
