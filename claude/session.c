/* session -- see session.h. */
#include <stdlib.h>
#include <string.h>
#include "session.h"
#include "path.h"
#include "util.h"

static const char hexd[] = "0123456789abcdef";

void sess_slug(const char *root, char *out, long cap)
{
    char s[300];
    long n = 0, i;
    unsigned long h = 5381;
    for (i = 0; root[i] && n < (long)sizeof(s) - 1; i++) {
        unsigned char c = (unsigned char)root[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        h = h * 33 + c;
        s[n++] = ok ? (char)c : '-';
    }
    s[n] = 0;
    while (n && s[n - 1] == '-')
        s[--n] = 0;
    if (!n)
        cl_copy(s, "root", sizeof(s));
    if (n > 30) {
        /* the end (the project's own name) and a hash of the whole */
        char t[32];
        memcpy(t, s + n - 21, 21);
        t[21] = '-';
        for (i = 0; i < 8; i++)
            t[22 + i] = hexd[(h >> (28 - 4 * i)) & 15];
        t[30] = 0;
        cl_copy(s, t, sizeof(s));
    }
    cl_copy(out, s, cap);
}

void sess_init(cl_session *s, cl_sys *sys, const char *home, const char *root)
{
    char slug[40], p[300];
    memset(s, 0, sizeof(*s));
    s->sys = sys;
    sess_slug(root, slug, sizeof(slug));
    if (path_join(home, "projects", p, sizeof(p)) || path_join(p, slug, s->dir, sizeof(s->dir)))
        s->off = 1;
}

static void set_id(cl_session *s, const char *id)
{
    cl_copy(s->id, id, sizeof(s->id));
    cl_copy(s->file, s->dir, sizeof(s->file));
    cl_cat(s->file, "/", sizeof(s->file));
    cl_cat(s->file, id, sizeof(s->file));
    cl_cat(s->file, ".jsonl", sizeof(s->file));
}

void sess_new(cl_session *s, unsigned long ms)
{
    unsigned long t = ms / 1000;
    char id[16];
    int i;
    for (;;) {
        for (i = 0; i < 8; i++)
            id[i] = hexd[(t >> (28 - 4 * i)) & 15];
        id[8] = 0;
        set_id(s, id);
        if (s->sys->kind(s->sys->u, s->file) == 0)
            break;
        t++;                        /* that second has a session already */
    }
    s->title[0] = 0;
    s->saved = 0;
    s->last_n = 0;
    s->started = 0;
}

void sess_truncate(cl_session *s, int k)
{
    if (k < s->saved) {
        s->saved = k;
        s->last_n = -1;
    }
}

static int dirs(cl_session *s)
{
    char p[300], h[300];
    if (!s->sys->mkdir || !s->sys->append)
        return -1;
    /* ENVARC:Claude, its projects/, the root's own */
    if (path_parent(s->dir, p, sizeof(p)) == 0) {
        if (path_parent(p, h, sizeof(h)) == 0)
            s->sys->mkdir(s->sys->u, h);
        s->sys->mkdir(s->sys->u, p);
    }
    return s->sys->mkdir(s->sys->u, s->dir);
}

/* the first prompt's text (its first text block), cut */
static void first_prompt(const cl_conv *c, char *out, long cap)
{
    jv v, b, x;
    jit it;
    out[0] = 0;
    if (!c->n || json_parse(c->m[0].json, c->m[0].n, &v))
        return;
    json_iter(v, &it);
    while (json_next(&it, 0, &b))
        if (json_get(b, "type", &x) && json_streq(x, "text") && json_get(b, "text", &x)) {
            long i;
            json_str(x, out, cap);
            for (i = 0; out[i]; i++)
                if (out[i] == '\n' || out[i] == '\r' || out[i] == '\t')
                    out[i] = ' ';
            return;
        }
}

static int index_line(cl_session *s, const char *first)
{
    char p[340];
    jw w;
    int rc;
    cl_copy(p, s->dir, sizeof(p));
    cl_cat(p, "/sessions", sizeof(p));
    jw_init(&w);
    jw_rawz(&w, "{\"id\":");
    jw_strz(&w, s->id);
    jw_rawz(&w, ",\"title\":");
    jw_strz(&w, s->title);
    jw_rawz(&w, ",\"first\":");
    jw_strz(&w, first);
    jw_rawz(&w, "}\n");
    rc = w.oom ? -1 : s->sys->append(s->sys->u, p, w.p, w.n);
    jw_free(&w);
    return rc;
}

static void msg_line(jw *w, const cl_conv *c, int i)
{
    jw_rawz(w, c->m[i].user ? "{\"type\":\"user\",\"index\":" : "{\"type\":\"assistant\",\"index\":");
    jw_long(w, i);
    jw_rawz(w, c->m[i].user ? ",\"message\":{\"role\":\"user\",\"content\":" : ",\"message\":{\"role\":\"assistant\",\"content\":");
    jw_raw(w, c->m[i].json, c->m[i].n);
    jw_rawz(w, "}}\n");
}

int sess_save(cl_session *s, const cl_conv *c)
{
    jw w;
    int from = s->saved, i, rc;
    if (s->off || !s->id[0] || !c->n)
        return 0;
    if (from > c->n)
        from = c->n;
    /* the last message written may have changed since (a prompt added to a
     * trailing tool_result message, or that taken back) */
    if (from > 0 && (from > c->n || c->m[from - 1].n != s->last_n))
        from--;
    if (from == c->n && c->n == s->saved)
        return 0;
    jw_init(&w);
    if (!s->started) {
        char first[96];
        if (dirs(s)) {
            jw_free(&w);
            return -1;
        }
        first_prompt(c, first, sizeof(first));
        jw_rawz(&w, "{\"type\":\"session\",\"id\":");
        jw_strz(&w, s->id);
        jw_rawz(&w, ",\"version\":1}\n");
        if (index_line(s, first)) {
            jw_free(&w);
            return -1;
        }
    }
    for (i = from; i < c->n; i++)
        msg_line(&w, c, i);
    rc = w.oom ? -1 : s->sys->append(s->sys->u, s->file, w.p, w.n);
    jw_free(&w);
    if (rc)
        return -1;
    s->n_appends++;
    s->started = 1;
    s->saved = c->n;
    s->last_n = c->m[c->n - 1].n;
    return 0;
}

int sess_rename(cl_session *s, const char *title)
{
    jw w;
    int rc;
    cl_copy(s->title, title, sizeof(s->title));
    if (s->off || !s->started)
        return 0;                   /* the index line comes with the first save */
    jw_init(&w);
    jw_rawz(&w, "{\"type\":\"title\",\"title\":");
    jw_strz(&w, title);
    jw_rawz(&w, "}\n");
    rc = w.oom ? -1 : s->sys->append(s->sys->u, s->file, w.p, w.n);
    jw_free(&w);
    return rc ? -1 : index_line(s, "");
}

int sess_branch(cl_session *s, const cl_conv *c, unsigned long ms)
{
    char title[96];
    cl_copy(title, s->title[0] ? s->title : "", sizeof(title));
    sess_new(s, ms);
    if (title[0]) {
        cl_cat(title, " (branch)", sizeof(title));
        cl_copy(s->title, title, sizeof(s->title));
    }
    return sess_save(s, c);
}

/* the index read: entries, most recently used first */
int sess_list(cl_session *s, cl_sess_info *out, int max)
{
    char p[340], *b = 0;
    long n = 0, i, e;
    int k = 0, j;
    cl_copy(p, s->dir, sizeof(p));
    cl_cat(p, "/sessions", sizeof(p));
    if (s->sys->kind(s->sys->u, p) != 1 || s->sys->read(s->sys->u, p, 512L * 1024, &b, &n))
        return 0;
    /* backwards: the last line of an id is its newest */
    for (e = n; e > 0 && k < max; e = i) {
        long st;
        jv v, x;
        char id[16];
        for (i = e - 1; i > 0 && b[i - 1] != '\n'; i--)
            ;
        st = i;
        if (i > 0)
            i--;
        if (json_parse(b + st, e - st, &v) || !json_get(v, "id", &x) || json_str(x, id, sizeof(id)) < 1)
            continue;
        for (j = 0; j < k; j++)
            if (!strcmp(out[j].id, id))
                break;
        if (j < k) {
            /* an older line: a first prompt the newer one lacks */
            if (!out[j].first[0] && json_get(v, "first", &x))
                json_str(x, out[j].first, sizeof(out[j].first));
            continue;
        }
        {
            char f[340];
            cl_copy(f, s->dir, sizeof(f));
            cl_cat(f, "/", sizeof(f));
            cl_cat(f, id, sizeof(f));
            cl_cat(f, ".jsonl", sizeof(f));
            if (s->sys->kind(s->sys->u, f) != 1)
                continue;           /* deleted */
        }
        memset(&out[k], 0, sizeof(out[k]));
        cl_copy(out[k].id, id, sizeof(out[k].id));
        if (json_get(v, "title", &x))
            json_str(x, out[k].title, sizeof(out[k].title));
        if (json_get(v, "first", &x))
            json_str(x, out[k].first, sizeof(out[k].first));
        k++;
    }
    /* first prompts from older lines of the same ids */
    for (e = n; e > 0; e = i) {
        long st;
        jv v, x;
        char id[16];
        for (i = e - 1; i > 0 && b[i - 1] != '\n'; i--)
            ;
        st = i;
        if (i > 0)
            i--;
        if (json_parse(b + st, e - st, &v) || !json_get(v, "id", &x) || json_str(x, id, sizeof(id)) < 1)
            continue;
        for (j = 0; j < k; j++)
            if (!strcmp(out[j].id, id) && !out[j].first[0] && json_get(v, "first", &x))
                json_str(x, out[j].first, sizeof(out[j].first));
    }
    free(b);
    return k;
}

int sess_find(cl_session *s, const char *name, char *id, long cap)
{
    cl_sess_info *l = (cl_sess_info *)malloc(SESS_LIST * sizeof(cl_sess_info));
    int n, i, hit = -1, hits = 0;
    long nl = (long)strlen(name);
    if (!l)
        return -1;
    n = sess_list(s, l, SESS_LIST);
    for (i = 0; i < n; i++)
        if (!strcmp(l[i].id, name) || cl_strieq(l[i].title, name)) {
            cl_copy(id, l[i].id, cap);
            free(l);
            return 0;
        }
    for (i = 0; i < n; i++)
        if (cl_strnieq(l[i].id, name, nl) || cl_strnieq(l[i].title, name, nl)) {
            hit = i;
            hits++;
        }
    if (hits == 1)
        cl_copy(id, l[hit].id, cap);
    free(l);
    return hits == 1 ? 0 : hits ? -2 : -1;
}

int sess_load(cl_session *s, const char *id, cl_conv *c)
{
    char f[340], *b = 0;
    long n = 0, i = 0;
    cl_conv t;
    char title[96];
    title[0] = 0;
    cl_copy(f, s->dir, sizeof(f));
    cl_cat(f, "/", sizeof(f));
    cl_cat(f, id, sizeof(f));
    cl_cat(f, ".jsonl", sizeof(f));
    if (s->sys->kind(s->sys->u, f) != 1 || s->sys->read(s->sys->u, f, 16L * 1024 * 1024, &b, &n))
        return -1;
    conv_init(&t);
    while (i < n) {
        long e = i;
        jv v, x, m, ct;
        while (e < n && b[e] != '\n')
            e++;
        /* a line cut by a crash is skipped */
        if (e > i && json_parse(b + i, e - i, &v) == 0 && json_get(v, "type", &x)) {
            if (json_streq(x, "title") && json_get(v, "title", &m))
                json_str(m, title, sizeof(title));
            else if ((json_streq(x, "user") || json_streq(x, "assistant")) && json_get(v, "index", &m) &&
                     json_get(v, "message", &ct) && json_get(ct, "content", &ct) && json_type(ct) == J_ARR) {
                long k = json_long(m, -1);
                if (k >= 0 && k <= t.n) {
                    cl_mark mk;
                    mk.n = (int)k;
                    mk.last = k ? t.m[k - 1].n : 0;
                    conv_rollback(&t, mk);
                    if (conv_add(&t, json_streq(x, "user"), ct.p, ct.n)) {
                        conv_free(&t);
                        free(b);
                        return -1;
                    }
                }
            }
        }
        i = e + 1;
    }
    free(b);
    if (!t.n) {
        conv_free(&t);
        return -1;
    }
    conv_clear(c);
    free(c->m);
    c->m = t.m;
    c->n = t.n;
    c->cap = t.cap;
    set_id(s, id);
    cl_copy(s->title, title, sizeof(s->title));
    s->started = 1;
    s->saved = c->n;
    s->last_n = c->m[c->n - 1].n;
    if (!s->off)
        index_line(s, "");
    return 0;
}
