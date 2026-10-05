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
    free(d->deny_tools);
    free(d->skills);
    free(d->when);
    d->description = d->body = d->tools = d->model = d->hint = 0;
    free(d->arg_names);
    d->deny_tools = d->skills = d->when = d->arg_names = 0;
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

/* the field a YAML list under key goes to (tools, disallowedTools,
 * skills), 0 for a key that holds no list */
static char **list_field(cl_def *d, const char *key)
{
    if (!strcmp(key, "tools") || !strcmp(key, "allowed-tools"))
        return &d->tools;
    if (!strcmp(key, "disallowedTools") || !strcmp(key, "disallowed-tools"))
        return &d->deny_tools;
    if (!strcmp(key, "skills"))
        return &d->skills;
    if (!strcmp(key, "arguments"))
        return &d->arg_names;
    return 0;
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
    if (!d->deny_tools)
        d->deny_tools = dupn("", 0);
    if (!d->skills)
        d->skills = dupn("", 0);
    if (!d->arg_names)
        d->arg_names = dupn("", 0);
    if (!d->when)
        d->when = dupn("", 0);
    if (!d->description || !d->tools || !d->model || !d->hint || !d->deny_tools || !d->skills || !d->when || !d->arg_names)
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
                if (lkey[0] && lw.n && list_field(d, lkey))
                    set(list_field(d, lkey), dupn(lw.p, lw.n));
                jw_reset(&lw);
                lkey[0] = 0;
                v = value(t + c + 1, e - c - 1);
                if (!v) {
                    jw_free(&lw);
                    return -1;
                }
                if (!*v)
                    cl_copy(lkey, key, sizeof(lkey));
                if (list_field(d, key) && v[0] == '[') {
                    /* a flow list: [a, b] */
                    long vl = (long)strlen(v);
                    memmove(v, v + 1, (size_t)vl);
                    if (vl > 1 && v[vl - 2] == ']')
                        v[vl - 2] = 0;
                }
                if (!strcmp(key, "name") && *v)
                    cl_copy(d->name, v, sizeof(d->name));
                if (!strcmp(key, "description"))
                    set(&d->description, v);
                else if (!strcmp(key, "arguments"))
                    set(&d->arg_names, v);
                else if (!strcmp(key, "allowed-tools") || !strcmp(key, "tools"))
                    set(&d->tools, v);
                else if (!strcmp(key, "model"))
                    set(&d->model, v);
                else if (!strcmp(key, "argument-hint"))
                    set(&d->hint, v);
                else if (!strcmp(key, "disallowedTools") || !strcmp(key, "disallowed-tools"))
                    set(&d->deny_tools, v);
                else if (!strcmp(key, "skills"))
                    set(&d->skills, v);
                else if (!strcmp(key, "when_to_use"))
                    set(&d->when, v);
                else {
                    if (!strcmp(key, "keep-coding-instructions"))
                        d->keep_coding = !strcmp(v, "true");
                    else if (!strcmp(key, "disable-model-invocation"))
                        d->no_model = !strcmp(v, "true");
                    else if (!strcmp(key, "user-invocable"))
                        d->no_user = !strcmp(v, "false");
                    else if (!strcmp(key, "maxTurns"))
                        d->max_turns = atoi(v);
                    else if (!strcmp(key, "effort"))
                        cl_copy(d->effort, v, sizeof(d->effort));
                    else if (!strcmp(key, "permissionMode"))
                        cl_copy(d->perm_mode, v, sizeof(d->perm_mode));
                    else if (!strcmp(key, "context"))
                        d->fork = !strcmp(v, "fork");
                    else if (!strcmp(key, "agent"))
                        cl_copy(d->agent, v, sizeof(d->agent));
                    free(v);
                }
            }
            i = e + 1;
        }
        if (lkey[0] && lw.n && list_field(d, lkey))
            set(list_field(d, lkey), dupn(lw.p, lw.n));
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
    d.deny_tools = dupn("", 0);
    d.skills = dupn("", 0);
    d.when = dupn("", 0);
    d.arg_names = dupn("", 0);
    if (!d.description || !d.body || !d.tools || !d.model || !d.hint || !d.deny_tools || !d.skills || !d.when || !d.arg_names ||
        add(s, &d))
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

void defs_drop(cl_defs *s, int type)
{
    int i, k = 0;
    for (i = 0; i < s->n; i++) {
        int go = type < 0 ? s->d[i].src != DEF_BUILTIN : s->d[i].type == type;
        if (go)
            def_free(&s->d[i]);
        else
            s->d[k++] = s->d[i];
    }
    s->n = k;
}

/* a JSON string or array of strings as "a, b" (0 when it is neither) */
static char *str_or_list(jv v)
{
    long l;
    if (json_type(v) == J_STR)
        return json_strdup(v, &l);
    if (json_type(v) == J_ARR) {
        jw w;
        jv e;
        jit it;
        jw_init(&w);
        json_iter(v, &it);
        while (json_next(&it, 0, &e)) {
            char *t = json_type(e) == J_STR ? json_strdup(e, &l) : 0;
            if (!t)
                continue;
            if (w.n)
                jw_rawz(&w, ", ");
            jw_rawz(&w, t);
            free(t);
        }
        if (w.oom) {
            jw_free(&w);
            return 0;
        }
        return w.p ? w.p : dupn("", 0);
    }
    return 0;
}

void def_extra_json(cl_def *d, jv a)
{
    jv x;
    if (json_get(a, "disallowedTools", &x))
        set(&d->deny_tools, str_or_list(x));
    if (json_get(a, "skills", &x))
        set(&d->skills, str_or_list(x));
    if (json_get(a, "maxTurns", &x))
        d->max_turns = (int)json_long(x, 0);
    if (json_get(a, "effort", &x))
        json_str(x, d->effort, sizeof(d->effort));
    if (json_get(a, "permissionMode", &x))
        json_str(x, d->perm_mode, sizeof(d->perm_mode));
}

int defs_add_agents_json(cl_defs *s, const char *json, char *err, long cap)
{
    jv o, a, x;
    jit it;
    char name[64];
    if (json_parse(json, (long)strlen(json), &o) || json_type(o) != J_OBJ) {
        cl_copy(err, "--agents is not a JSON object of agents", cap);
        return -1;
    }
    json_iter(o, &it);
    while (json_next(&it, &x, &a)) {
        cl_def d;
        long l;
        char *t;
        if (json_type(a) != J_OBJ || json_str(x, name, sizeof(name)) < 1) {
            cl_copy(err, "--agents: each agent is \"name\": {\"description\": ..., \"prompt\": ...}", cap);
            return -1;
        }
        memset(&d, 0, sizeof(d));
        d.type = DEF_AGENT;
        d.src = CFG_SESSION;
        cl_copy(d.name, name, sizeof(d.name));
        cl_copy(d.path, "--agents", sizeof(d.path));
        if (defs_parse("", 0, &d)) {
            def_free(&d);
            return -1;
        }
        if (!json_get(a, "description", &x) || !json_get(a, "prompt", &x)) {
            def_free(&d);
            cl_copy(err, "--agents: agent ", cap);
            cl_cat(err, name, cap);
            cl_cat(err, " needs a description and a prompt", cap);
            return -1;
        }
        if (json_get(a, "description", &x) && (t = json_strdup(x, &l)) != 0)
            set(&d.description, t);
        if (json_get(a, "prompt", &x) && (t = json_strdup(x, &l)) != 0)
            set(&d.body, t);
        if (json_get(a, "tools", &x))
            set(&d.tools, str_or_list(x));
        if (json_get(a, "model", &x) && (t = json_strdup(x, &l)) != 0)
            set(&d.model, t);
        def_extra_json(&d, a);
        /* the command line's agents come first: they hide the files' of the same name */
        if (add(s, &d)) {
            def_free(&d);
            return -1;
        }
        memmove(&s->d[1], &s->d[0], (size_t)(s->n - 1) * sizeof(cl_def));
        s->d[0] = d;
    }
    return 0;
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
#define NWORDS 16
static int split(const char *a, char w[NWORDS][256])
{
    int n = 0;
    while (*a && n < NWORDS) {
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

/* ${CLAUDE_...} at s: its value appended, the length of the name taken
 * (0: not one of ours) */
static long claude_var(const char *s, const cl_cmd_vars *v, jw *out)
{
    static const char *const names[] = { "${CLAUDE_SESSION_ID}", "${CLAUDE_EFFORT}", "${CLAUDE_SKILL_DIR}",
                                         "${CLAUDE_PROJECT_DIR}", 0 };
    int i;
    for (i = 0; names[i]; i++) {
        long l = (long)strlen(names[i]);
        if (!strncmp(s, names[i], (size_t)l)) {
            const char *val = !v ? "" : i == 0 ? v->session_id : i == 1 ? v->effort : i == 2 ? v->skill_dir
                                                                                             : v->project_dir;
            jw_rawz(out, val ? val : "");
            return l;
        }
    }
    return 0;
}

int cmd_subst_vars(const char *in, const cl_cmd_vars *v, jw *out)
{
    while (*in) {
        long l = *in == '$' ? claude_var(in, v, out) : 0;
        if (l)
            in += l;
        else
            jw_raw(out, in++, 1);
    }
    return out->oom ? -1 : 0;
}

/* $name of the arguments frontmatter at s: its index, *len its length; -1 none */
static int named_arg(const cl_def *d, const char *s, long *len)
{
    const char *p = d->arg_names;
    int k = 0;
    while (p && *p) {
        const char *e;
        while (*p == ' ' || *p == ',')
            p++;
        e = p;
        while (*e && *e != ' ' && *e != ',')
            e++;
        if (e > p && !strncmp(s + 1, p, (size_t)(e - p))) {
            char c = s[1 + (e - p)];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
                *len = 1 + (long)(e - p);
                return k;
            }
        }
        if (e > p)
            k++;
        p = e;
    }
    return -1;
}

int cmd_expand_vars(const cl_def *d, const char *args, const cl_cmd_vars *v, cl_sys *sys, const char *root,
                    jw *out, char *err, long cap)
{
    const char *s = d->body;
    char (*w)[256] = (char (*)[256])malloc(NWORDS * 256);
    int nw, had_args = 0;
    long l;
    err[0] = 0;
    if (!w) {
        cl_copy(err, "out of memory", cap);
        return -1;
    }
    nw = split(args ? args : "", w);
    while (*s) {
        if (s[0] == '\\' && s[1] == '$') {
            jw_raw(out, "$", 1);        /* \$ is a dollar sign */
            s += 2;
        } else if (s[0] == '$' && !strncmp(s, "$ARGUMENTS[", 11) && s[11] >= '0' && s[11] <= '9') {
            /* $ARGUMENTS[N], 0-based */
            const char *e = s + 11;
            int k = 0;
            while (*e >= '0' && *e <= '9')
                k = k * 10 + (*e++ - '0');
            if (*e == ']') {
                if (k < nw)
                    jw_rawz(out, w[k]);
                else
                    jw_raw(out, s, (long)(e + 1 - s));  /* no such argument: as it is */
                had_args = 1;
                s = e + 1;
            } else {
                jw_raw(out, s, 1);
                s++;
            }
        } else if (s[0] == '$' && !strncmp(s, "$ARGUMENTS", 10)) {
            jw_rawz(out, args ? args : "");
            had_args = 1;
            s += 10;
        } else if (s[0] == '$' && s[1] >= '0' && s[1] <= '9') {
            /* $N: 0-based (Claude Code: $0 is the first argument) */
            const char *e = s + 1;
            int k = 0;
            while (*e >= '0' && *e <= '9')
                k = k * 10 + (*e++ - '0');
            if (k < nw)
                jw_rawz(out, w[k]);
            else
                jw_raw(out, s, (long)(e - s));  /* no such argument: stays unchanged */
            had_args = 1;
            s = e;
        } else if (s[0] == '$' && s[1] == '{' && (l = claude_var(s, v, out)) != 0) {
            s += l;
        } else if (s[0] == '$' && named_arg(d, s, &l) >= 0) {
            int k = named_arg(d, s, &l);
            if (k < nw)
                jw_rawz(out, w[k]);         /* a named one with no argument: empty */
            had_args = 1;
            s += l;
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
                free(w);
                return -1;
            }
            {
                char *o = (char *)malloc(16384);
                long on = 0, rc = 0;
                jw c;
                if (!o) {
                    cl_copy(err, "out of memory", cap);
                    free(w);
                    return -1;
                }
                jw_init(&c);
                cmd_subst_vars(cmd, v, &c);     /* ${CLAUDE_SKILL_DIR}/scripts/x */
                if (c.oom || sys->run(sys->u, c.p ? c.p : cmd, 30, o, 16383, &on, &rc) < 0) {
                    cl_copy(err, "!`", cap);
                    cl_cat(err, cmd, cap);
                    cl_cat(err, "` did not run: ", cap);
                    cl_cat(err, sys->err(sys->u), cap);
                    free(o);
                    jw_free(&c);
                    free(w);
                    return -1;
                }
                jw_free(&c);
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
    free(w);
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

int cmd_expand(const cl_def *d, const char *args, cl_sys *sys, const char *root, jw *out, char *err, long cap)
{
    return cmd_expand_vars(d, args, 0, sys, root, out, err, cap);
}
