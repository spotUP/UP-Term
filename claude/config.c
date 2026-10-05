/* config -- see config.h. */
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "path.h"
#include "util.h"

const char *const cfg_hook_events[HK_COUNT] = {
    "PreToolUse", "PostToolUse", "UserPromptSubmit", "Stop", "SubagentStop", "SessionStart", "SessionEnd",
    "PreCompact", "Notification"
};

static char *dupn(const char *s, long n)
{
    char *d = (char *)malloc((size_t)n + 1);
    if (d) {
        memcpy(d, s, (size_t)n);
        d[n] = 0;
    }
    return d;
}

static char *dupz(const char *s)
{
    return dupn(s, (long)strlen(s));
}

void cfg_init(cl_settings *s)
{
    memset(s, 0, sizeof(*s));
    s->auto_compact = -1;
    s->web_search = -1;
}

void cfg_free(cl_settings *s)
{
    int i;
    for (i = 0; i < s->nrules; i++)
        free(s->rules[i].text);
    for (i = 0; i < s->nhooks; i++) {
        free(s->hooks[i].matcher);
        free(s->hooks[i].cmd);
    }
    for (i = 0; i < s->nenv; i++) {
        free(s->env[i].k);
        free(s->env[i].v);
    }
    for (i = 0; i < s->ndirs; i++)
        free(s->dirs[i]);
    free(s->rules);
    free(s->hooks);
    free(s->env);
    free(s->dirs);
    cfg_init(s);
}

/* grows an array of n used, *cap allocated, by one element of size sz */
static int grow(void **p, int n, int *cap, size_t sz)
{
    void *q;
    int nc;
    if (n < *cap)
        return 0;
    nc = *cap ? *cap * 2 : 8;
    q = realloc(*p, (size_t)nc * sz);
    if (!q)
        return -1;
    *p = q;
    *cap = nc;
    return 0;
}

int cfg_add_rule(cl_settings *s, int kind, int src, const char *text)
{
    int i;
    for (i = 0; i < s->nrules; i++)
        if (s->rules[i].kind == kind && !strcmp(s->rules[i].text, text))
            return 0;
    if (grow((void **)&s->rules, s->nrules, &s->caprules, sizeof(cl_rule)))
        return -1;
    s->rules[s->nrules].kind = kind;
    s->rules[s->nrules].src = src;
    s->rules[s->nrules].text = dupz(text);
    if (!s->rules[s->nrules].text)
        return -1;
    s->nrules++;
    return 0;
}

const char *cfg_kind_name(int kind)
{
    return kind == RULE_DENY ? "deny" : kind == RULE_ASK ? "ask" : "allow";
}

static void str_into(jv v, char *out, long cap)
{
    if (json_type(v) == J_STR)
        json_str(v, out, cap);
}

static void rules_of(cl_settings *s, jv perm, const char *key, int kind, int src)
{
    jv arr, e;
    jit it;
    if (!json_get(perm, key, &arr) || json_type(arr) != J_ARR)
        return;
    json_iter(arr, &it);
    while (json_next(&it, 0, &e)) {
        char r[300];
        if (json_type(e) == J_STR && json_str(e, r, sizeof(r)) > 0)
            cfg_add_rule(s, kind, src, r);
    }
}

static void hooks_of(cl_settings *s, jv hooks, int src)
{
    int ev;
    for (ev = 0; ev < HK_COUNT; ev++) {
        jv arr, grp;
        jit it;
        if (!json_get(hooks, cfg_hook_events[ev], &arr) || json_type(arr) != J_ARR)
            continue;
        json_iter(arr, &it);
        while (json_next(&it, 0, &grp)) {
            jv m, hs, h, x;
            jit hi;
            char matcher[128];
            matcher[0] = 0;
            if (json_get(grp, "matcher", &m))
                str_into(m, matcher, sizeof(matcher));
            if (!json_get(grp, "hooks", &hs) || json_type(hs) != J_ARR)
                continue;
            json_iter(hs, &hi);
            while (json_next(&hi, 0, &h)) {
                long cl;
                char *cmd;
                cl_hook *k;
                if (json_get(h, "type", &x) && !json_streq(x, "command"))
                    continue;
                if (!json_get(h, "command", &x) || (cmd = json_strdup(x, &cl)) == 0)
                    continue;
                if (grow((void **)&s->hooks, s->nhooks, &s->caphooks, sizeof(cl_hook))) {
                    free(cmd);
                    return;
                }
                k = &s->hooks[s->nhooks];
                k->event = ev;
                k->src = src;
                k->cmd = cmd;
                k->matcher = dupz(matcher);
                k->timeout_s = json_get(h, "timeout", &x) ? (int)json_long(x, 60) : 60;
                if (k->timeout_s <= 0)
                    k->timeout_s = 60;
                if (!k->matcher) {
                    free(cmd);
                    return;
                }
                s->nhooks++;
            }
        }
    }
}

static void env_of(cl_settings *s, jv env)
{
    jit it;
    jv k, v;
    json_iter(env, &it);
    while (json_next(&it, &k, &v)) {
        long kl, vl;
        char *ks, *vs;
        int i;
        if (json_type(v) != J_STR)
            continue;
        ks = json_strdup(k, &kl);
        vs = json_strdup(v, &vl);
        if (!ks || !vs) {
            free(ks);
            free(vs);
            return;
        }
        for (i = 0; i < s->nenv; i++)
            if (!strcmp(s->env[i].k, ks))
                break;
        if (i < s->nenv) {
            free(s->env[i].v);
            s->env[i].v = vs;
            free(ks);
            continue;
        }
        if (grow((void **)&s->env, s->nenv, &s->capenv, sizeof(cl_kv))) {
            free(ks);
            free(vs);
            return;
        }
        s->env[s->nenv].k = ks;
        s->env[s->nenv].v = vs;
        s->nenv++;
    }
}

int cfg_merge(cl_settings *s, int src, const char *json, long n, const char *name)
{
    jv o, x, p;
    if (json_parse(json, n, &o) || json_type(o) != J_OBJ) {
        cl_copy(s->err, name, sizeof(s->err));
        cl_cat(s->err, " is not a JSON object; it was skipped", sizeof(s->err));
        return -1;
    }
    if (json_get(o, "model", &x))
        str_into(x, s->model, sizeof(s->model));
    if (json_get(o, "effortLevel", &x) || json_get(o, "effort", &x))
        str_into(x, s->effort, sizeof(s->effort));
    if (json_get(o, "outputStyle", &x))
        str_into(x, s->output_style, sizeof(s->output_style));
    if (json_get(o, "editorMode", &x))
        str_into(x, s->editor_mode, sizeof(s->editor_mode));
    if (json_get(o, "theme", &x))
        str_into(x, s->theme, sizeof(s->theme));
    if (json_get(o, "fallbackModel", &x))
        str_into(x, s->fallback_model, sizeof(s->fallback_model));
    if (json_get(o, "autoCompactEnabled", &x) && (json_type(x) == J_TRUE || json_type(x) == J_FALSE))
        s->auto_compact = json_type(x) == J_TRUE;
    if (json_get(o, "webSearch", &x) && (json_type(x) == J_TRUE || json_type(x) == J_FALSE))
        s->web_search = json_type(x) == J_TRUE;
    if (json_get(o, "statusLine", &x)) {
        jv c;
        if (json_type(x) == J_OBJ && json_get(x, "command", &c))
            str_into(c, s->status_cmd, sizeof(s->status_cmd));
        if (json_type(x) == J_OBJ && json_get(x, "padding", &c) && json_type(c) == J_NUM)
            s->status_pad = (int)json_long(c, 0);
        if (json_type(x) == J_OBJ && json_get(x, "refreshInterval", &c) && json_type(c) == J_NUM)
            s->status_refresh_s = (int)json_long(c, 0);
        if (s->status_pad < 0 || s->status_pad > 40)
            s->status_pad = 0;
        if (s->status_refresh_s < 0)
            s->status_refresh_s = 0;
    }
    if (json_get(o, "env", &x) && json_type(x) == J_OBJ)
        env_of(s, x);
    if (json_get(o, "hooks", &x) && json_type(x) == J_OBJ)
        hooks_of(s, x, src);
    if (json_get(o, "permissions", &p) && json_type(p) == J_OBJ) {
        rules_of(s, p, "allow", RULE_ALLOW, src);
        rules_of(s, p, "ask", RULE_ASK, src);
        rules_of(s, p, "deny", RULE_DENY, src);
        if (json_get(p, "defaultMode", &x))
            str_into(x, s->default_mode, sizeof(s->default_mode));
        if (json_get(p, "additionalDirectories", &x) && json_type(x) == J_ARR) {
            jit it;
            jv e;
            json_iter(x, &it);
            while (json_next(&it, 0, &e)) {
                long l;
                char *d = json_strdup(e, &l);
                if (!d)
                    continue;
                if (grow((void **)&s->dirs, s->ndirs, &s->capdirs, sizeof(char *))) {
                    free(d);
                    break;
                }
                s->dirs[s->ndirs++] = d;
            }
        }
    }
    return 0;
}

const char *cfg_file(const cl_settings *s, int src)
{
    return src >= 0 && src < CFG_NSRC ? s->path[src] : "";
}

int cfg_load(cl_settings *s, cl_sys *sys, const char *home, const char *root)
{
    static const char *const rel[3] = { "settings.json", ".claude/settings.json", ".claude/settings.local.json" };
    int i, got = 0;
    for (i = 0; i < 3; i++) {
        char *b = 0;
        long n = 0;
        if (path_join(i == CFG_USER ? home : root, rel[i], s->path[i], sizeof(s->path[i])))
            continue;
        if (sys->kind(sys->u, s->path[i]) != 1 || sys->read(sys->u, s->path[i], 256L * 1024, &b, &n))
            continue;
        if (cfg_merge(s, i, b, n, s->path[i]) == 0) {
            s->found[i] = 1;
            got++;
        }
        free(b);
    }
    return got;
}

/* ---- writing ---- */

/* obj with key set to value (raw JSON) or removed (value 0), into w, one
 * member per line */
static void obj_with(jv obj, const char *key, const char *value, jw *w, const char *ind)
{
    jit it;
    jv k, v;
    int first = 1, done = 0;
    jw_raw(w, "{", 1);
    if (json_type(obj) == J_OBJ) {
        json_iter(obj, &it);
        while (json_next(&it, &k, &v)) {
            int same = json_streq(k, key);
            if (same && (!value || done))
                continue;
            jw_rawz(w, first ? "\n" : ",\n");
            jw_rawz(w, ind);
            jw_rawz(w, "  ");
            jw_raw(w, k.p, k.n);
            jw_rawz(w, ": ");
            if (same) {
                jw_rawz(w, value);
                done = 1;
            } else
                jw_raw(w, v.p, v.n);
            first = 0;
        }
    }
    if (value && !done) {
        jw_rawz(w, first ? "\n" : ",\n");
        jw_rawz(w, ind);
        jw_rawz(w, "  ");
        jw_strz(w, key);
        jw_rawz(w, ": ");
        jw_rawz(w, value);
        first = 0;
    }
    if (!first) {
        jw_raw(w, "\n", 1);
        jw_rawz(w, ind);
    }
    jw_raw(w, "}", 1);
}

/* the file's object, "{}" when it is missing; -1 when it is there but
 * not an object (it is not overwritten then) */
static int read_obj(cl_sys *sys, const char *file, char **buf, jv *o)
{
    long n = 0;
    *buf = 0;
    if (sys->kind(sys->u, file) != 1) {
        o->p = "{}";
        o->n = 2;
        return 0;
    }
    if (sys->read(sys->u, file, 256L * 1024, buf, &n))
        return -1;
    if (json_parse(*buf, n, o) || json_type(*o) != J_OBJ) {
        free(*buf);
        *buf = 0;
        return -1;
    }
    return 0;
}

/* the directory part of a file name made, when it is missing (.claude/) */
static void make_parent(cl_sys *sys, const char *file)
{
    char d[300];
    if (sys->mkdir && path_parent(file, d, sizeof(d)) == 0 && sys->kind(sys->u, d) == 0)
        sys->mkdir(sys->u, d);
}

int cfg_write_key(cl_sys *sys, const char *file, const char *key, const char *value)
{
    char *b;
    jv o;
    jw w;
    int rc;
    if (read_obj(sys, file, &b, &o))
        return -1;
    jw_init(&w);
    obj_with(o, key, value, &w, "");
    jw_raw(&w, "\n", 1);
    free(b);
    make_parent(sys, file);
    rc = w.oom ? -1 : sys->write(sys->u, file, w.p, w.n);
    jw_free(&w);
    return rc;
}

int cfg_write_rule(cl_sys *sys, const char *file, int kind, const char *rule, int add)
{
    char *b;
    jv o, p, arr, e;
    jit it;
    jw na, np, w;
    int rc = 0, found = 0, first = 1;
    const char *kn = cfg_kind_name(kind);
    if (read_obj(sys, file, &b, &o))
        return -1;
    if (!json_get(o, "permissions", &p) || json_type(p) != J_OBJ) {
        p.p = "{}";
        p.n = 2;
    }
    jw_init(&na);
    jw_raw(&na, "[", 1);
    if (json_get(p, kn, &arr) && json_type(arr) == J_ARR) {
        json_iter(arr, &it);
        while (json_next(&it, 0, &e)) {
            if (json_streq(e, rule)) {
                found = 1;
                if (!add)
                    continue;
            }
            if (!first)
                jw_rawz(&na, ", ");
            jw_raw(&na, e.p, e.n);
            first = 0;
        }
    }
    if (add && !found) {
        if (!first)
            jw_rawz(&na, ", ");
        jw_strz(&na, rule);
    }
    jw_raw(&na, "]", 1);
    jw_init(&np);
    obj_with(p, kn, na.p, &np, "  ");
    jw_init(&w);
    obj_with(o, "permissions", np.p, &w, "");
    jw_raw(&w, "\n", 1);
    free(b);
    if (!add && !found)
        rc = -1;
    else if (na.oom || np.oom || w.oom)
        rc = -1;
    else {
        make_parent(sys, file);
        rc = sys->write(sys->u, file, w.p, w.n);
    }
    jw_free(&na);
    jw_free(&np);
    jw_free(&w);
    return rc;
}

/* ---- rules ---- */

int cfg_rule_parse(const char *rule, char *tool, long tcap, char *pat, long pcap)
{
    const char *o = strchr(rule, '(');
    long tl = o ? (long)(o - rule) : (long)strlen(rule);
    while (tl && rule[tl - 1] == ' ')
        tl--;
    if (!tl || tl >= tcap)
        return -1;
    memcpy(tool, rule, (size_t)tl);
    tool[tl] = 0;
    pat[0] = 0;
    if (o) {
        long n = (long)strlen(o + 1);
        if (!n || o[n] != ')')
            return -1;
        n--;
        if (n >= pcap)
            return -1;
        memcpy(pat, o + 1, (size_t)n);
        pat[n] = 0;
        if (!strcmp(pat, "*"))
            pat[0] = 0;
    }
    return 0;
}

const char *cfg_cc_tool(const char *name)
{
    static const char *const map[][2] = {
        { "read_file", "Read" }, { "list_dir", "LS" }, { "grep", "Grep" }, { "write_file", "Write" },
        { "edit_file", "Edit" }, { "run_command", "Bash" }, { "todo_write", "TodoWrite" }, { 0, 0 }
    };
    int i;
    for (i = 0; map[i][0]; i++)
        if (!strcmp(name, map[i][0]))
            return map[i][1];
    return name;
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

int cfg_glob(const char *p, const char *s, int icase)
{
    while (*p) {
        if (p[0] == '*' && p[1] == '*') {
            p += 2;
            if (*p == '/')
                p++;        /* "**" and "**" + "/" match zero or more whole directories */
            for (;; s++) {
                if (cfg_glob(p, s, icase))
                    return 1;
                if (!*s)
                    return 0;
            }
        }
        if (*p == '*' || (p[0] == '#' && p[1] == '?')) {
            p += *p == '*' ? 1 : 2;
            for (;; s++) {
                if (cfg_glob(p, s, icase))
                    return 1;
                if (!*s || *s == '/')
                    return 0;
            }
        }
        if (!*s)
            return 0;
        if (*p == '?') {
            if (*s == '/')
                return 0;
        } else if (*p == '[') {
            const char *q = p + 1;
            int neg = *q == '!' || *q == '^', hit = 0;
            if (neg)
                q++;
            while (*q && *q != ']') {
                int a = (unsigned char)*q, b = a;
                if (q[1] == '-' && q[2] && q[2] != ']') {
                    b = (unsigned char)q[2];
                    q += 2;
                }
                if ((unsigned char)*s >= a && (unsigned char)*s <= b)
                    hit = 1;
                if (icase && lower((unsigned char)*s) >= lower(a) && lower((unsigned char)*s) <= lower(b))
                    hit = 1;
                q++;
            }
            if (!*q || hit == neg)
                return 0;
            p = q;
        } else if (icase ? lower((unsigned char)*p) != lower((unsigned char)*s) : *p != *s)
            return 0;
        p++;
        s++;
    }
    return !*s;
}

/* a string member of the input, decoded */
static int in_str(jv in, const char *key, char *out, long cap)
{
    jv x;
    out[0] = 0;
    return json_get(in, key, &x) && json_type(x) == J_STR && json_str(x, out, cap) >= 0;
}

/* is the tool one the rule's tool covers? */
static int tool_covers(const char *rt, const char *ct)
{
    if (!strcmp(rt, ct))
        return 1;
    if (!strcmp(rt, "Edit"))
        return !strcmp(ct, "Write") || !strcmp(ct, "MultiEdit") || !strcmp(ct, "NotebookEdit");
    if (!strcmp(rt, "Read"))
        return !strcmp(ct, "LS") || !strcmp(ct, "Grep") || !strcmp(ct, "Glob");
    return 0;
}

static int is_space(int c)
{
    return c == ' ' || c == '\t' || c == '\n';
}

/* a command pattern: * is any text, / included */
static int wild(const char *p, const char *s)
{
    while (*p) {
        if (*p == '*') {
            while (*p == '*')
                p++;
            for (;; s++) {
                if (wild(p, s))
                    return 1;
                if (!*s)
                    return 0;
            }
        }
        if (*p != *s)
            return 0;
        p++;
        s++;
    }
    return !*s;
}

/* one simple command against a Bash pattern */
static int bash_one(const char *pat, const char *cmd, long n)
{
    char c[1024], p[300];
    long pl = (long)strlen(pat);
    while (n && is_space((unsigned char)*cmd)) {
        cmd++;
        n--;
    }
    while (n && is_space((unsigned char)cmd[n - 1]))
        n--;
    if (n >= (long)sizeof(c) || pl >= (long)sizeof(p))
        return 0;
    memcpy(c, cmd, (size_t)n);
    c[n] = 0;
    cl_copy(p, pat, sizeof(p));
    if (pl >= 2 && p[pl - 2] == ':' && p[pl - 1] == '*') {
        /* the legacy prefix form: "make:*" is "make" or "make ..." */
        p[pl - 2] = 0;
        pl -= 2;
        return !strncmp(c, p, (size_t)pl) && (!c[pl] || is_space((unsigned char)c[pl]));
    }
    if (strchr(p, '*')) {
        /* "make *" also covers a bare "make" */
        if (pl >= 2 && p[pl - 2] == ' ' && p[pl - 1] == '*' && !strncmp(c, p, (size_t)pl - 2) && !c[pl - 2])
            return 1;
        return wild(p, c);
    }
    return !strcmp(c, p);
}

/* a compound line: for an allow every part must match (all 1); for a
 * deny or an ask any part (all 0) */
static int bash_match(const char *pat, const char *cmd, int all)
{
    const char *s = cmd, *e;
    int any = 0, every = 1, parts = 0;
    for (;;) {
        for (e = s; *e && *e != ';' && *e != '|' && *e != '&' && *e != '\n'; e++)
            ;
        if (e > s) {
            long n = (long)(e - s), k = 0;
            while (k < n && is_space((unsigned char)s[k]))
                k++;
            if (k < n) {
                int m = bash_one(pat, s, n);
                parts++;
                any |= m;
                every &= m;
            }
        }
        if (!*e)
            break;
        s = e + 1;
        while (*s == '|' || *s == '&')
            s++;
    }
    /* one simple command: the pattern against the whole of it */
    if (parts <= 1)
        return bash_one(pat, cmd, (long)strlen(cmd));
    return all ? every : any;
}

/* a path pattern against a call's path, both resolved against root */
static int path_match(const char *pat, const char *path, const char *root)
{
    char full[600], pp[600];
    const char *p = pat;
    if (!strchr(p, ':')) {
        if (!strncmp(p, "./", 2))
            p += 2;
        else if (p[0] == '/' && p[1] == '/')
            p += 2;
        else if (p[0] == '/')
            p += 1;
        else if (!strchr(p, '/')) {
            /* a bare name: at any depth */
            cl_copy(pp, root, sizeof(pp));
            cl_cat(pp, pp[0] && pp[strlen(pp) - 1] != ':' && pp[strlen(pp) - 1] != '/' ? "/**/" : "**/",
                   sizeof(pp));
            cl_cat(pp, p, sizeof(pp));
            p = 0;
        }
        if (p) {
            cl_copy(pp, root, sizeof(pp));
            if (pp[0] && pp[strlen(pp) - 1] != ':' && pp[strlen(pp) - 1] != '/')
                cl_cat(pp, "/", sizeof(pp));
            cl_cat(pp, p, sizeof(pp));
        }
    } else
        cl_copy(pp, p, sizeof(pp));
    if (path_join(root, path, full, sizeof(full)))
        return 0;
    if (cfg_glob(pp, full, 1))
        return 1;
    /* a directory pattern covers what is below it, at any depth */
    cl_cat(pp, "/**", sizeof(pp));
    return cfg_glob(pp, full, 1);
}

static int rule_match(const char *rule, const char *tool, jv in, const char *root, int all)
{
    char rt[64], pat[300], v[1024];
    const char *ct = cfg_cc_tool(tool);
    if (cfg_rule_parse(rule, rt, sizeof(rt), pat, sizeof(pat)))
        return 0;
    if (!tool_covers(cfg_cc_tool(rt), ct))
        return 0;
    if (!pat[0])
        return 1;
    if (!strcmp(ct, "Bash"))
        return in_str(in, "command", v, sizeof(v)) && bash_match(pat, v, all);
    if (!strcmp(ct, "WebFetch")) {
        const char *h, *e;
        char host[256];
        long hl;
        if (strncmp(pat, "domain:", 7) || !in_str(in, "url", v, sizeof(v)))
            return 0;
        h = strstr(v, "://");
        h = h ? h + 3 : v;
        for (e = h; *e && *e != '/' && *e != ':' && *e != '?'; e++)
            ;
        hl = (long)(e - h);
        if (hl >= (long)sizeof(host))
            return 0;
        memcpy(host, h, (size_t)hl);
        host[hl] = 0;
        if (cl_strieq(host, pat + 7))
            return 1;
        /* a subdomain of it */
        hl = (long)strlen(host) - (long)strlen(pat + 7);
        return hl > 0 && host[hl - 1] == '.' && cl_strieq(host + hl, pat + 7);
    }
    if (tool_covers("Read", ct) || tool_covers("Edit", ct)) {
        if (!in_str(in, "file_path", v, sizeof(v)) && !in_str(in, "path", v, sizeof(v)) &&
            !in_str(in, "notebook_path", v, sizeof(v)))
            cl_copy(v, "", sizeof(v));
        return path_match(pat, v, root);
    }
    /* another tool: its first naming input */
    if (in_str(in, "subagent_type", v, sizeof(v)) || in_str(in, "skill", v, sizeof(v)) ||
        in_str(in, "command", v, sizeof(v)) || in_str(in, "url", v, sizeof(v)) ||
        in_str(in, "query", v, sizeof(v)))
        return cfg_glob(pat, v, 0);
    return 0;
}

int cfg_rule_match(const char *rule, const char *tool, jv input, const char *root)
{
    return rule_match(rule, tool, input, root, 1);
}

int cfg_decide(const cl_settings *s, const char *tool, jv input, const char *root, const cl_rule **which)
{
    static const int order[3] = { RULE_DENY, RULE_ASK, RULE_ALLOW };
    int k, i;
    if (which)
        *which = 0;
    for (k = 0; k < 3; k++)
        for (i = 0; i < s->nrules; i++)
            if (s->rules[i].kind == order[k] &&
                rule_match(s->rules[i].text, tool, input, root, order[k] == RULE_ALLOW)) {
                if (which)
                    *which = &s->rules[i];
                return order[k];
            }
    return RULE_NONE;
}

int cfg_web_search(const cl_settings *s)
{
    int i;
    if (s->web_search == 0)
        return 0;
    for (i = 0; i < s->nrules; i++) {
        char tool[40], pat[200];
        if (s->rules[i].kind == RULE_DENY &&
            cfg_rule_parse(s->rules[i].text, tool, sizeof(tool), pat, sizeof(pat)) == 0 &&
            !strcmp(tool, "WebSearch") && (!pat[0] || !strcmp(pat, "*")))
            return 0;
    }
    return 1;
}

const char *cfg_model(const char *name)
{
    static const char *const al[][2] = {
        { "opus", "claude-opus-5-5" },  { "sonnet", "claude-sonnet-5-5" },
        { "haiku", "claude-haiku-4-5-20251001" }, { "fable", "claude-fable-5-1" },
        { "default", "claude-opus-5-5" }, { "opusplan", "claude-opus-5-5" },
        { "best", "claude-fable-5-1" }, { 0, 0 }
    };
    int i;
    for (i = 0; al[i][0]; i++)
        if (cl_strieq(name, al[i][0]))
            return al[i][1];
    return name;
}
