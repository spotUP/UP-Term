/* tools -- see tools.h. */
#include <stdlib.h>
#include <string.h>
#include "tools.h"
#include "path.h"
#include "util.h"

#define READ_MAX   (256L * 1024)     /* read_file */
#define EDIT_MAX   (1024L * 1024)    /* edit_file, and grep per file */
#define LIST_MAX   2000              /* list_dir entries */
#define GREP_FILES 2000
#define GREP_HITS  200
#define GREP_DEPTH 8
#define OUT_MAX    (32L * 1024)      /* run_command output, grep output */

/* the tools array, in pieces under C89's 509-byte literal limit */
static const char *const tools_parts[] = {
    "[{\"name\":\"read_file\",\"description\":\"Read a text file. Paths are AmigaOS paths "
    "(SYS:S/Startup-Sequence, RAM:x, or relative to the start directory). Files up to 256 KB.\","
    "\"strict\":true,\"eager_input_streaming\":true,\"input_schema\":{\"type\":\"object\","
    "\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"],\"additionalProperties\":false}},",
    "{\"name\":\"list_dir\",\"description\":\"List a directory: names, sizes, and a trailing / for "
    "directories.\",\"strict\":true,\"eager_input_streaming\":true,\"input_schema\":{\"type\":\"object\","
    "\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"],\"additionalProperties\":false}},",
    "{\"name\":\"grep\",\"description\":\"Search files for lines containing a pattern: a plain "
    "substring, or with * and ? as wildcards. path is a file or a directory searched to a depth of 8 "
    "(default: the start directory). Up to 200 matching lines.\",\"strict\":true,"
    "\"eager_input_streaming\":true,\"input_schema\":{\"type\":\"object\",\"properties\":{"
    "\"pattern\":{\"type\":\"string\"},\"path\":{\"type\":\"string\"},\"ignore_case\":{\"type\":\"boolean\"}},"
    "\"required\":[\"pattern\"],\"additionalProperties\":false}},",
    "{\"name\":\"write_file\",\"description\":\"Create a file or replace its whole content.\","
    "\"strict\":true,\"eager_input_streaming\":true,\"input_schema\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"}},\"required\":[\"path\",\"content\"],"
    "\"additionalProperties\":false}},",
    "{\"name\":\"edit_file\",\"description\":\"Replace one exact occurrence of old_string in a file "
    "with new_string. old_string must occur exactly once: include enough surrounding text to make it "
    "unique. The file keeps its character set (Latin-1 or UTF-8).\",\"strict\":true,"
    "\"eager_input_streaming\":true,\"input_schema\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"},\"old_string\":{\"type\":\"string\"},\"new_string\":{\"type\":\"string\"}},"
    "\"required\":[\"path\",\"old_string\",\"new_string\"],\"additionalProperties\":false}},",
    "{\"name\":\"run_command\",\"description\":\"Run a command line through vsh (AmigaDOS commands "
    "and programs, pipes, redirection) in the start directory. Output and errors are returned, up "
    "to 32 KB, with the return code (5 warn, 10 error, 20 failure). Commands that wait for input "
    "get none. Time limit 60 seconds.\",\"strict\":true,\"eager_input_streaming\":true,"
    "\"input_schema\":{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"}},"
    "\"required\":[\"command\"],\"additionalProperties\":false}},",
    "{\"name\":\"todo_write\",\"description\":\"Keep a todo list for a task of several steps: send "
    "the whole list each time, each item pending, in_progress or completed, one in_progress at a "
    "time; mark an item completed as soon as it is done. The user sees it as a checklist. Skip it "
    "for small tasks.\",\"strict\":true,\"eager_input_streaming\":true,\"input_schema\":{\"type\":",
    "\"object\",\"properties\":{\"todos\":{\"type\":\"array\",\"items\":{\"type\":\"object\","
    "\"properties\":{\"content\":{\"type\":\"string\"},\"status\":{\"type\":\"string\",\"enum\":"
    "[\"pending\",\"in_progress\",\"completed\"]}},\"required\":[\"content\",\"status\"],"
    "\"additionalProperties\":false}}},\"required\":[\"todos\"],\"additionalProperties\":false}}]",
    0
};

const char *tools_json(void)
{
    static char all[4096];
    int i;
    if (!all[0])
        for (i = 0; tools_parts[i]; i++)
            cl_cat(all, tools_parts[i], sizeof(all));
    return all;
}

typedef struct spec {
    const char *name;
    const char *prop[3];
    char type[3];               /* 's' string, 'b' boolean, 'a' array */
    unsigned required;          /* bit per prop */
} spec;

static const spec specs[T_COUNT] = {
    { "read_file", { "path", 0, 0 }, { 's', 0, 0 }, 1 },
    { "list_dir", { "path", 0, 0 }, { 's', 0, 0 }, 1 },
    { "grep", { "pattern", "path", "ignore_case" }, { 's', 's', 'b' }, 1 },
    { "write_file", { "path", "content", 0 }, { 's', 's', 0 }, 3 },
    { "edit_file", { "path", "old_string", "new_string" }, { 's', 's', 's' }, 7 },
    { "run_command", { "command", 0, 0 }, { 's', 0, 0 }, 1 },
    { "todo_write", { "todos", 0, 0 }, { 'a', 0, 0 }, 1 },
};

int tools_id(const char *name)
{
    int i;
    for (i = 0; i < T_COUNT; i++)
        if (!strcmp(name, specs[i].name))
            return i;
    return -1;
}

int perm_read_only(int tool)
{
    return tool == T_READ_FILE || tool == T_LIST_DIR || tool == T_GREP;
}

int perm_must_ask(const cl_perm *p, int tool, int outside)
{
    if (tool == T_TODO_WRITE)
        return 0;
    if (outside)
        return 1;
    if (p->mode == PERM_ACCEPT && (tool == T_WRITE_FILE || tool == T_EDIT_FILE))
        return 0;
    return !(p->session & (1u << tool));
}

int perm_refused(const cl_perm *p, int tool)
{
    return p->mode == PERM_PLAN && !perm_read_only(tool) && tool != T_TODO_WRITE;
}

void perm_grant(cl_perm *p, int tool)
{
    if (perm_read_only(tool))
        p->session |= (1u << T_READ_FILE) | (1u << T_LIST_DIR) | (1u << T_GREP);
    else
        p->session |= 1u << tool;
}

/* todo_write's items: objects with a content string and a status */
static int todo_items(jv arr, char *err, long cap)
{
    jit it;
    jv e;
    json_iter(arr, &it);
    while (json_next(&it, 0, &e)) {
        jit m;
        jv k, v;
        int have = 0;
        if (json_type(e) != J_OBJ) {
            cl_copy(err, "each todo must be an object", cap);
            return -1;
        }
        json_iter(e, &m);
        while (json_next(&m, &k, &v)) {
            if (json_streq(k, "content") && json_type(v) == J_STR)
                have |= 1;
            else if (json_streq(k, "status") &&
                     (json_streq(v, "pending") || json_streq(v, "in_progress") || json_streq(v, "completed")))
                have |= 2;
            else {
                cl_copy(err, "a todo has content (a string) and status (pending, in_progress, completed) only",
                        cap);
                return -1;
            }
        }
        if (have != 3) {
            cl_copy(err, "a todo needs content and status", cap);
            return -1;
        }
    }
    return 0;
}

int tools_validate(int tool, jv in, char *err, long cap)
{
    const spec *s;
    jit it;
    jv k, v;
    unsigned seen = 0;
    int i;
    if (tool < 0 || tool >= T_COUNT) {
        cl_copy(err, "unknown tool", cap);
        return -1;
    }
    s = &specs[tool];
    if (json_type(in) != J_OBJ) {
        cl_copy(err, "the input must be a JSON object", cap);
        return -1;
    }
    json_iter(in, &it);
    while (json_next(&it, &k, &v)) {
        int t;
        for (i = 0; i < 3 && s->prop[i]; i++)
            if (json_streq(k, s->prop[i]))
                break;
        if (i == 3 || !s->prop[i]) {
            char kn[48];
            json_str(k, kn, sizeof(kn));
            cl_copy(err, "unknown property: ", cap);
            cl_cat(err, kn, cap);
            return -1;
        }
        t = json_type(v);
        if ((s->type[i] == 's' && t != J_STR) || (s->type[i] == 'b' && t != J_TRUE && t != J_FALSE) ||
            (s->type[i] == 'a' && t != J_ARR)) {
            cl_copy(err, s->prop[i], cap);
            cl_cat(err, s->type[i] == 's' ? " must be a string" : s->type[i] == 'a' ? " must be an array"
                                                                                     : " must be true or false",
                   cap);
            return -1;
        }
        if (s->type[i] == 'a' && todo_items(v, err, cap))
            return -1;
        seen |= 1u << i;
    }
    for (i = 0; i < 3 && s->prop[i]; i++)
        if ((s->required & (1u << i)) && !(seen & (1u << i))) {
            cl_copy(err, "missing property: ", cap);
            cl_cat(err, s->prop[i], cap);
            return -1;
        }
    return 0;
}

/* ---- helpers ---- */

/* a string property, malloc'ed ("" when absent); 0 out of memory */
static char *prop(jv in, const char *key, long *len)
{
    jv v;
    char *s;
    if (json_get(in, key, &v))
        return json_strdup(v, len);
    s = (char *)malloc(1);
    if (s)
        s[0] = 0;
    if (len)
        *len = 0;
    return s;
}

/* one line for the screen: control characters (escape sequences from the
 * model) shown as '?', cut to fit */
static void summary(char *out, long cap, const char *a, const char *b)
{
    long o = 0;
    const char *src[2];
    int k;
    src[0] = a;
    src[1] = b;
    for (k = 0; k < 2 && src[k]; k++) {
        const char *s = src[k];
        if (k && o < cap - 3) {
            out[o++] = ' ';
            out[o++] = ' ';
        }
        for (; *s && o < cap - 1; s++)
            out[o++] = ((unsigned char)*s < 0x20 || *s == 0x7f) ? '?' : *s;
    }
    out[o] = 0;
}

static int is_binary(const char *s, long n)
{
    long i;
    if (n > 4096)
        n = 4096;
    for (i = 0; i < n; i++)
        if (!s[i])
            return 1;
    return 0;
}

static int valid_utf8(const char *s, long n)
{
    long i = 0;
    unsigned long cp;
    while (i < n) {
        int k = json_utf8(s + i, n - i, &cp);
        if (!k)
            return 0;
        i += k;
    }
    return 1;
}

/* Latin-1 to UTF-8, malloc'ed */
static char *latin1_to_utf8(const char *s, long n, long *on)
{
    char *o = (char *)malloc((size_t)n * 2 + 1);
    long i, k = 0;
    if (!o)
        return 0;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80)
            o[k++] = (char)c;
        else {
            o[k++] = (char)(0xc0 | (c >> 6));
            o[k++] = (char)(0x80 | (c & 0x3f));
        }
    }
    o[k] = 0;
    *on = k;
    return o;
}

/* UTF-8 to Latin-1 in place ('?' for what Latin-1 lacks): the new length */
static long utf8_to_latin1(char *s, long n)
{
    long i = 0, k = 0;
    unsigned long cp;
    while (i < n) {
        int l = json_utf8(s + i, n - i, &cp);
        if (!l) {
            l = 1;
            cp = (unsigned char)s[i];
        }
        s[k++] = cp < 0x100 ? (char)cp : '?';
        i += l;
    }
    return k;
}

/* The full path for a tool's path argument, and whether it lies outside
 * the start directory (by canonical names: an assign into the start
 * directory is inside). 0, or -1 when the name is no path. */
static int resolve(cl_tools *t, const char *arg, char *full, long cap, int *outside)
{
    char canon[512], parent[512];
    const char *name;
    if (path_join(t->root, *arg ? arg : "", full, cap))
        return -1;
    if (t->sys->canon(t->sys->u, full, canon, sizeof(canon)) == 0) {
        *outside = !path_inside(t->root, canon);
        return 0;
    }
    /* not there yet (a new file): its directory's canonical name */
    if (path_parent(full, parent, sizeof(parent)) == 0 &&
        t->sys->canon(t->sys->u, parent[0] ? parent : t->root, canon, sizeof(canon)) == 0) {
        name = full + strlen(parent);
        while (*name == '/')
            name++;
        cl_cat(canon, canon[0] && canon[strlen(canon) - 1] != ':' && canon[strlen(canon) - 1] != '/' ? "/" : "",
               sizeof(canon));
        cl_cat(canon, name, sizeof(canon));
        *outside = !path_inside(t->root, canon);
        return 0;
    }
    *outside = !path_inside(t->root, full);
    return 0;
}

static void result(cl_tools *t, jw *out, const char *id, const char *text, long n, int is_error)
{
    if (t->result)
        t->result(t->u, t->cur, t->cur_in, t->cur_inn, is_error, text, n);
    jw_rawz(out, "{\"type\":\"tool_result\",\"tool_use_id\":");
    jw_strz(out, id);
    jw_rawz(out, ",\"content\":");
    jw_str(out, text, n);
    if (is_error)
        jw_rawz(out, ",\"is_error\":true");
    jw_raw(out, "}", 1);
}

static void error(cl_tools *t, jw *out, const char *id, const char *a, const char *b)
{
    char msg[600];
    cl_copy(msg, a, sizeof(msg));
    if (b)
        cl_cat(msg, b, sizeof(msg));
    result(t, out, id, msg, (long)strlen(msg), 1);
}

/* ---- the tools ---- */

static void do_read(cl_tools *t, const char *id, const char *path, jw *out)
{
    char *b = 0;
    long n = 0;
    int rc = t->sys->read(t->sys->u, path, READ_MAX, &b, &n);
    if (rc == SYS_TOO_BIG) {
        error(t, out, id, "the file is larger than 256 KB: ", path);
        return;
    }
    if (rc) {
        error(t, out, id, "cannot read the file: ", t->sys->err(t->sys->u));
        return;
    }
    if (is_binary(b, n))
        error(t, out, id, "this is a binary file, not text: ", path);
    else if (!n)
        result(t, out, id, "(the file is empty)", 19, 0);
    else
        result(t, out, id, b, n, 0);
    free(b);
}

typedef struct dlist {
    cl_dirent *e;
    int n, cap, more;
} dlist;

static int collect(void *c, const cl_dirent *e)
{
    dlist *l = (dlist *)c;
    if (l->n >= LIST_MAX) {
        l->more = 1;
        return 1;
    }
    if (l->n == l->cap) {
        int nc = l->cap ? l->cap * 2 : 64;
        cl_dirent *ne = (cl_dirent *)realloc(l->e, (size_t)nc * sizeof(cl_dirent));
        if (!ne)
            return 1;
        l->e = ne;
        l->cap = nc;
    }
    l->e[l->n++] = *e;
    return 0;
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static int by_name(const void *a, const void *b)
{
    const char *x = ((const cl_dirent *)a)->name, *y = ((const cl_dirent *)b)->name;
    while (*x && lower((unsigned char)*x) == lower((unsigned char)*y)) {
        x++;
        y++;
    }
    return lower((unsigned char)*x) - lower((unsigned char)*y);
}

static void do_list(cl_tools *t, const char *id, const char *path, jw *out)
{
    dlist l;
    jw r;
    int i;
    memset(&l, 0, sizeof(l));
    if (t->sys->kind(t->sys->u, path) != 2) {
        error(t, out, id, "not a directory: ", path);
        return;
    }
    if (t->sys->list(t->sys->u, path, collect, &l)) {
        free(l.e);
        error(t, out, id, "cannot list the directory: ", t->sys->err(t->sys->u));
        return;
    }
    if (l.n > 1)
        qsort(l.e, (size_t)l.n, sizeof(cl_dirent), by_name);
    jw_init(&r);
    for (i = 0; i < l.n; i++) {
        jw_rawz(&r, l.e[i].name);
        if (l.e[i].dir)
            jw_raw(&r, "/", 1);
        else {
            jw_raw(&r, "  ", 2);
            jw_long(&r, l.e[i].size);
        }
        jw_raw(&r, "\n", 1);
    }
    if (l.more)
        jw_rawz(&r, "(more entries not shown)\n");
    if (!l.n)
        jw_rawz(&r, "(the directory is empty)");
    result(t, out, id, r.p, r.n, 0);
    jw_free(&r);
    free(l.e);
}

/* wildcard match of the whole of s[0..n) against p (* and ?) */
static int glob(const char *p, const char *s, long n, int icase)
{
    const char *star = 0;
    long i = 0, mark = 0;
    while (i < n) {
        if (*p == '*') {
            star = p++;
            mark = i;
        } else if (*p && (*p == '?' || (icase ? lower((unsigned char)*p) == lower((unsigned char)s[i]) : *p == s[i]))) {
            p++;
            i++;
        } else if (star) {
            p = star + 1;
            i = ++mark;
        } else
            return 0;
    }
    while (*p == '*')
        p++;
    return !*p;
}

static int line_match(const char *pat, int wild, int icase, const char *s, long n)
{
    long pl = (long)strlen(pat), i, j;
    if (wild)
        return glob(pat, s, n, icase);
    for (i = 0; i + pl <= n; i++) {
        for (j = 0; j < pl; j++)
            if (icase ? lower((unsigned char)s[i + j]) != lower((unsigned char)pat[j]) : s[i + j] != pat[j])
                break;
        if (j == pl)
            return 1;
    }
    return 0;
}

typedef struct gstate {
    cl_tools *t;
    const char *pat;
    char *wpat;                 /* "*pat*" for a wildcard pattern */
    int icase;
    jw r;
    int hits, files;
    /* the directories still to search, a stack of malloc'ed names */
    char **dirs;
    int *depth;
    int nd, cd;
    int cur_depth;
    const char *cur;
} gstate;

static void grep_file(gstate *g, const char *path)
{
    char *b;
    long n, i = 0, lineno = 1;
    if (g->hits >= GREP_HITS || g->files >= GREP_FILES)
        return;
    g->files++;
    if (g->t->sys->read(g->t->sys->u, path, EDIT_MAX, &b, &n))
        return;
    if (is_binary(b, n)) {
        free(b);
        return;
    }
    while (i < n && g->hits < GREP_HITS && g->r.n < OUT_MAX) {
        long e = i;
        while (e < n && b[e] != '\n')
            e++;
        if (line_match(g->wpat ? g->wpat : g->pat, g->wpat != 0, g->icase, b + i, e - i)) {
            long show = e - i > 200 ? 200 : e - i;
            if (show && b[i + show - 1] == '\r')
                show--;
            jw_rawz(&g->r, path);
            jw_raw(&g->r, ":", 1);
            jw_long(&g->r, lineno);
            jw_raw(&g->r, ": ", 2);
            jw_raw(&g->r, b + i, show);
            jw_raw(&g->r, "\n", 1);
            g->hits++;
        }
        i = e + 1;
        lineno++;
    }
    free(b);
}

static int push_dir(gstate *g, const char *path, int depth)
{
    char *c;
    if (g->nd == g->cd) {
        int nc = g->cd ? g->cd * 2 : 16;
        char **nd = (char **)realloc(g->dirs, (size_t)nc * sizeof(char *));
        int *dp;
        if (!nd)
            return -1;
        g->dirs = nd;
        dp = (int *)realloc(g->depth, (size_t)nc * sizeof(int));
        if (!dp)
            return -1;
        g->depth = dp;
        g->cd = nc;
    }
    c = (char *)malloc(strlen(path) + 1);
    if (!c)
        return -1;
    strcpy(c, path);
    g->dirs[g->nd] = c;
    g->depth[g->nd++] = depth;
    return 0;
}

static int grep_entry(void *c, const cl_dirent *e)
{
    gstate *g = (gstate *)c;
    char p[512];
    if (path_join(g->cur, e->name, p, sizeof(p)))
        return 0;
    if (e->dir) {
        if (g->cur_depth < GREP_DEPTH)
            push_dir(g, p, g->cur_depth + 1);
    } else
        grep_file(g, p);
    return g->hits >= GREP_HITS || g->files >= GREP_FILES;
}

static void do_grep(cl_tools *t, const char *id, const char *path, const char *pat, int icase, jw *out)
{
    gstate g;
    int k = t->sys->kind(t->sys->u, path);
    memset(&g, 0, sizeof(g));
    g.t = t;
    g.pat = pat;
    g.icase = icase;
    jw_init(&g.r);
    if (!*pat) {
        error(t, out, id, "the pattern is empty", 0);
        return;
    }
    if (strchr(pat, '*') || strchr(pat, '?')) {
        g.wpat = (char *)malloc(strlen(pat) + 3);
        if (g.wpat) {
            strcpy(g.wpat, "*");
            strcat(g.wpat, pat);
            strcat(g.wpat, "*");
        }
    }
    if (k == 1)
        grep_file(&g, path);
    else if (k == 2) {
        push_dir(&g, path, 0);
        while (g.nd && g.hits < GREP_HITS && g.files < GREP_FILES) {
            char *d = g.dirs[--g.nd];
            g.cur_depth = g.depth[g.nd];
            g.cur = d;
            t->sys->list(t->sys->u, d, grep_entry, &g);
            free(d);
        }
        while (g.nd)
            free(g.dirs[--g.nd]);
    } else {
        free(g.wpat);
        error(t, out, id, "no such file or directory: ", path);
        return;
    }
    if (g.hits >= GREP_HITS || g.files >= GREP_FILES)
        jw_rawz(&g.r, "(the search stopped at its limit: 200 lines or 2000 files)\n");
    if (!g.hits)
        jw_rawz(&g.r, "(no matches)");
    result(t, out, id, g.r.p, g.r.n, 0);
    jw_free(&g.r);
    free(g.dirs);
    free(g.depth);
    free(g.wpat);
}

static void do_write(cl_tools *t, const char *id, const char *path, const char *s, long n, jw *out)
{
    char msg[600], num[16];
    if (t->sys->kind(t->sys->u, path) == 2) {
        error(t, out, id, "that is a directory: ", path);
        return;
    }
    if (t->sys->write(t->sys->u, path, s, n)) {
        error(t, out, id, "cannot write the file: ", t->sys->err(t->sys->u));
        return;
    }
    cl_ltoa(n, num);
    cl_copy(msg, "Wrote ", sizeof(msg));
    cl_cat(msg, num, sizeof(msg));
    cl_cat(msg, " bytes to ", sizeof(msg));
    cl_cat(msg, path, sizeof(msg));
    result(t, out, id, msg, (long)strlen(msg), 0);
}

static long count_of(const char *h, long hn, const char *nd, long nn, long *first)
{
    long i, c = 0;
    *first = -1;
    for (i = 0; i + nn <= hn; i++)
        if (!memcmp(h + i, nd, (size_t)nn)) {
            if (*first < 0)
                *first = i;
            c++;
            i += nn - 1;
        }
    return c;
}

/* an edit worked out before anything is asked: the file as UTF-8 before
 * and after (the preview shows them), and whether it is kept as Latin-1 */
typedef struct pedit {
    char *before, *after;
    long bn, an;
    int latin;
} pedit;

static int edit_prepare(cl_tools *t, const char *id, const char *path, const char *olds, long on,
                        const char *news, long nn, jw *out, pedit *e)
{
    char *b = 0;
    long n, c, at;
    int rc;
    memset(e, 0, sizeof(*e));
    if (!on) {
        error(t, out, id, "old_string is empty; use write_file to create a file", 0);
        return -1;
    }
    rc = t->sys->read(t->sys->u, path, EDIT_MAX, &b, &n);
    if (rc) {
        error(t, out, id, rc == SYS_TOO_BIG ? "the file is larger than 1 MB: " : "cannot read the file: ",
              rc == SYS_TOO_BIG ? path : t->sys->err(t->sys->u));
        return -1;
    }
    if (is_binary(b, n)) {
        free(b);
        error(t, out, id, "this is a binary file, not text: ", path);
        return -1;
    }
    e->latin = !valid_utf8(b, n);
    if (e->latin) {
        e->before = latin1_to_utf8(b, n, &e->bn);
        free(b);
        if (!e->before) {
            error(t, out, id, "out of memory", 0);
            return -1;
        }
    } else {
        e->before = b;
        e->bn = n;
    }
    c = count_of(e->before, e->bn, olds, on, &at);
    if (c != 1) {
        free(e->before);
        e->before = 0;
        error(t, out, id, c ? "old_string occurs more than once; include more surrounding text to make it unique"
                            : "old_string was not found in the file", 0);
        return -1;
    }
    e->an = e->bn - on + nn;
    e->after = (char *)malloc((size_t)e->an + 1);
    if (!e->after) {
        free(e->before);
        e->before = 0;
        error(t, out, id, "out of memory", 0);
        return -1;
    }
    memcpy(e->after, e->before, (size_t)at);
    memcpy(e->after + at, news, (size_t)nn);
    memcpy(e->after + at + nn, e->before + at + on, (size_t)(e->bn - at - on));
    e->after[e->an] = 0;
    return 0;
}

static void edit_free(pedit *e)
{
    free(e->before);
    free(e->after);
    memset(e, 0, sizeof(*e));
}

static void do_edit(cl_tools *t, const char *id, const char *path, pedit *e, jw *out)
{
    char msg[600];
    long rn = e->an;
    int rc;
    if (e->latin)
        rn = utf8_to_latin1(e->after, rn);
    rc = t->sys->write(t->sys->u, path, e->after, rn);
    if (rc) {
        error(t, out, id, "cannot write the file: ", t->sys->err(t->sys->u));
        return;
    }
    cl_copy(msg, "Edited ", sizeof(msg));
    cl_cat(msg, path, sizeof(msg));
    cl_cat(msg, e->latin ? ": one occurrence replaced (kept as Latin-1)" : ": one occurrence replaced", sizeof(msg));
    result(t, out, id, msg, (long)strlen(msg), 0);
}

static void do_todo(cl_tools *t, const char *id, jw *out)
{
    static const char ok[] = "Todos updated. Keep the list current as you work.";
    result(t, out, id, ok, (long)sizeof(ok) - 1, 0);
}

static void do_run(cl_tools *t, const char *id, const char *cmd, jw *out)
{
    char *buf = (char *)malloc(OUT_MAX + 1);
    long n = 0, rc = 0;
    int r;
    jw res;
    char num[16];
    if (!buf) {
        error(t, out, id, "out of memory", 0);
        return;
    }
    r = t->sys->run(t->sys->u, cmd, t->timeout_s, buf, OUT_MAX, &n, &rc);
    if (r == -1) {
        free(buf);
        error(t, out, id, "the command did not start: ", t->sys->err(t->sys->u));
        return;
    }
    jw_init(&res);
    if (r == SYS_TIMEOUT)
        jw_rawz(&res, "The command ran out of time and was sent a break (Ctrl+C).\n");
    else if (r == SYS_BREAK)
        jw_rawz(&res, "The user stopped the command (Ctrl+C).\n");
    jw_rawz(&res, "Return code ");
    cl_ltoa(rc, num);
    jw_rawz(&res, num);
    jw_rawz(&res, ".\n");
    jw_raw(&res, buf, n);
    if (n >= OUT_MAX)
        jw_rawz(&res, "\n(output cut at 32 KB)");
    result(t, out, id, res.p, res.n, r != 0);
    jw_free(&res);
    free(buf);
}

void tools_run(cl_tools *t, const char *id, const char *name, int input_ok,
               const char *raw, long rawn, jw *out)
{
    int tool = tools_id(name), outside = 0, ans;
    jv in;
    char err[200], what[300], full[512];
    char *a = 0, *b = 0, *c = 0;
    long al = 0, bl = 0, cl = 0;
    pedit pe;
    memset(&pe, 0, sizeof(pe));
    t->cur = tool;
    t->cur_in = raw ? raw : "";
    t->cur_inn = raw ? rawn : 0;
    if (tool < 0) {
        error(t, out, id, "there is no tool named ", name);
        return;
    }
    if (t->stop) {
        /* the user stopped this round at an earlier call of it */
        error(t, out, id, "not run: the user stopped at an earlier tool call and will say what to do instead", 0);
        return;
    }
    if (!input_ok || json_parse(raw, rawn, &in)) {
        char cut[300];
        long k = rawn < (long)sizeof(cut) - 1 ? rawn : (long)sizeof(cut) - 1;
        memcpy(cut, raw ? raw : "", (size_t)(raw ? k : 0));
        cut[raw ? k : 0] = 0;
        error(t, out, id, "the tool input was not valid JSON; send the call again. It began: ", cut);
        return;
    }
    if (tools_validate(tool, in, err, sizeof(err))) {
        error(t, out, id, "invalid input: ", err);
        return;
    }
    if (tool == T_TODO_WRITE) {
        do_todo(t, id, out);
        return;
    }
    if (perm_refused(&t->perm, tool)) {
        error(t, out, id, "plan mode is on: only read_file, list_dir and grep run now. Present your plan; the "
                          "user switches plan mode off (Shift+Tab) to let you carry it out", 0);
        return;
    }
    switch (tool) {
    case T_GREP:
        a = prop(in, "pattern", &al);
        b = prop(in, "path", &bl);
        break;
    case T_RUN_COMMAND:
        a = prop(in, "command", &al);
        break;
    case T_WRITE_FILE:
        b = prop(in, "path", &bl);
        a = prop(in, "content", &al);
        break;
    case T_EDIT_FILE:
        b = prop(in, "path", &bl);
        a = prop(in, "old_string", &al);
        c = prop(in, "new_string", &cl);
        break;
    default:
        b = prop(in, "path", &bl);
        break;
    }
    if ((tool != T_RUN_COMMAND && !b) || (tool != T_READ_FILE && tool != T_LIST_DIR && !a) ||
        (tool == T_EDIT_FILE && !c)) {
        error(t, out, id, "out of memory", 0);
        goto done;
    }
    if (tool != T_RUN_COMMAND && resolve(t, b, full, sizeof(full), &outside)) {
        error(t, out, id, "not a usable path (it climbs above a volume's root): ", b);
        goto done;
    }
    if (tool == T_RUN_COMMAND)
        summary(what, sizeof(what), a, 0);
    else if (tool == T_GREP)
        summary(what, sizeof(what), a, full);
    else
        summary(what, sizeof(what), full, 0);
    if (t->show)
        t->show(t->u, name, what);
    /* what a write or an edit will do, before the question */
    if (tool == T_EDIT_FILE) {
        if (edit_prepare(t, id, full, a, al, c, cl, out, &pe))
            goto done;
        if (t->preview)
            t->preview(t->u, tool, full, pe.before, pe.bn, pe.after, pe.an);
    } else if (tool == T_WRITE_FILE && t->preview) {
        char *old = 0;
        long on = 0;
        if (t->sys->kind(t->sys->u, full) != 1 || t->sys->read(t->sys->u, full, EDIT_MAX, &old, &on))
            old = 0;
        t->preview(t->u, tool, full, old, old ? on : 0, a, al);
        free(old);
    }
    if (perm_must_ask(&t->perm, tool, outside)) {
        ans = t->ask ? t->ask(t->u, name, what, outside) : ASK_NO;
        if (ans == ASK_STOP) {
            t->stop = 1;
            error(t, out, id, "the user stopped this tool call and will tell you what to do differently; "
                              "wait for their message", 0);
            goto done;
        }
        if (ans == ASK_NO) {
            error(t, out, id, "the user declined this tool call", 0);
            goto done;
        }
        if (ans == ASK_SESSION && !outside)
            perm_grant(&t->perm, tool);
    }
    switch (tool) {
    case T_READ_FILE:
        do_read(t, id, full, out);
        break;
    case T_LIST_DIR:
        do_list(t, id, full, out);
        break;
    case T_GREP: {
        jv ic;
        do_grep(t, id, full, a, json_get(in, "ignore_case", &ic) && json_type(ic) == J_TRUE, out);
        break;
    }
    case T_WRITE_FILE:
        do_write(t, id, full, a, al, out);
        break;
    case T_EDIT_FILE:
        do_edit(t, id, full, &pe, out);
        break;
    case T_RUN_COMMAND:
        do_run(t, id, a, out);
        break;
    }
done:
    edit_free(&pe);
    free(a);
    free(b);
    free(c);
}
