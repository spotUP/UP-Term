/* trust -- what C:Claude remembers per project directory (A4 gaps 2), as
 * Claude Code keeps it in ~/.claude.json: <home>/claude.json,
 *   {"projects": {"Work:proj": {"hasTrustDialogAccepted": true,
 *                 "hasClaudeMdExternalIncludesApproved": false,
 *                 "hasClaudeMdExternalIncludesWarningShown": true}}}
 * Workspace trust: a directory is trusted when it or a directory above it
 * was accepted (Claude Code: outside a repository the trust covers the
 * subdirectories). The file's other keys and projects are kept as they
 * are on a write.
 * amiga-pi keeps pi's project trust in its own file, pi's format (pi f10993bc
 * core/trust-manager.ts): <agent dir>/trust.json, {"Work:proj": true} --
 * trust_near and trust_put below, on the same loading.
 * Portable C89 over sys.h, host-tested (tests/test_claude_config.c and the
 * REPL suite). */
#include <stdlib.h>
#include <string.h>
#include "trust.h"
#include "config.h"
#include "path.h"
#include "util.h"

/* A path as the files store it: UTF-8 (jw_strz writes a Latin-1 folder name of the
 * Amiga as UTF-8, json_str reads it back so). Every comparison against a stored key is
 * made in this form, once converted here; ASCII letters fold, as everywhere in the client. */
static int stored(const char *path, char *out, long cap)
{
    return json_utf8_of(path, out, cap) < 0 ? -1 : 0;
}

static int file_of(const char *home, char *out, long cap)
{
    return path_join(home, "claude.json", out, cap);
}

/* a file's text (0 none) */
static char *load_file(cl_sys *sys, const char *f, long *n)
{
    char *b = 0;
    if (sys->kind(sys->u, f) != 1 || sys->read(sys->u, f, 256L * 1024, &b, n))
        return 0;
    return b;
}

static char *load(cl_sys *sys, const char *home, long *n)
{
    char f[300];
    return file_of(home, f, sizeof(f)) ? 0 : load_file(sys, f, n);
}

int trust_get(cl_sys *sys, const char *home, const char *dir, const char *key, int inherit)
{
    long n = 0;
    char *b = load(sys, home, &n);
    jv o, p, k, e, x;
    jit it;
    char sdir[300];
    int got = -1;
    if (!b)
        return -1;
    if (stored(dir, sdir, sizeof(sdir))) {
        free(b);
        return -1;
    }
    if (json_parse(b, n, &o) == 0 && json_get(o, "projects", &p) && json_type(p) == J_OBJ) {
        json_iter(p, &it);
        while (json_next(&it, &k, &e)) {
            char path[300];
            json_str(k, path, sizeof(path));
            if (!(cl_strieq(path, sdir) || (inherit && path_inside(path, sdir))))
                continue;
            if (json_type(e) == J_OBJ && json_get(e, key, &x)) {
                if (json_type(x) == J_TRUE)
                    got = 1;
                else if (json_type(x) == J_FALSE && got < 0)
                    got = 0;
            }
        }
    }
    free(b);
    return got;
}

int trust_set(cl_sys *sys, const char *home, const char *dir, const char *key, int value)
{
    long n = 0;
    char *b = load(sys, home, &n), f[300], sdir[300];
    jv o, p, k, e;
    jit it;
    jw w;
    int done = 0, first = 1, rc;
    if (stored(dir, sdir, sizeof(sdir))) {
        free(b);
        return -1;
    }
    dir = sdir;
    jw_init(&w);
    jw_raw(&w, "{", 1);
    if (b && json_parse(b, n, &o) == 0 && json_get(o, "projects", &p) && json_type(p) == J_OBJ) {
        json_iter(p, &it);
        while (json_next(&it, &k, &e)) {
            char path[300];
            json_str(k, path, sizeof(path));
            if (!first)
                jw_raw(&w, ",", 1);
            first = 0;
            jw_raw(&w, k.p, k.n);
            jw_raw(&w, ":", 1);
            if (cl_strieq(path, dir) && json_type(e) == J_OBJ) {
                /* this project's entry: its other members kept, the key set */
                jit mi;
                jv mk, mv;
                int mf = 1;
                jw_raw(&w, "{", 1);
                json_iter(e, &mi);
                while (json_next(&mi, &mk, &mv)) {
                    if (json_streq(mk, key))
                        continue;
                    if (!mf)
                        jw_raw(&w, ",", 1);
                    mf = 0;
                    jw_raw(&w, mk.p, mk.n);
                    jw_raw(&w, ":", 1);
                    jw_raw(&w, mv.p, mv.n);
                }
                if (!mf)
                    jw_raw(&w, ",", 1);
                jw_strz(&w, key);
                jw_rawz(&w, value ? ":true}" : ":false}");
                done = 1;
            } else
                jw_raw(&w, e.p, e.n);
        }
    }
    if (!done) {
        if (!first)
            jw_raw(&w, ",", 1);
        jw_strz(&w, dir);
        jw_rawz(&w, ":{");
        jw_strz(&w, key);
        jw_rawz(&w, value ? ":true}" : ":false}");
    }
    jw_raw(&w, "}", 1);
    free(b);
    if (w.oom || file_of(home, f, sizeof(f))) {
        jw_free(&w);
        return -1;
    }
    if (sys->mkdir)
        sys->mkdir(sys->u, home);
    rc = cfg_write_key(sys, f, "projects", w.p);
    jw_free(&w);
    return rc;
}

/* ---- pi's trust.json: one object, a canonical directory -> true | false | null ----
 * pi's readTrustFile (core/trust-manager.ts) throws on a file that is not JSON, not an
 * object, or holds a value other than true, false, null: pi stops with that error and
 * never rewrites the file. The functions below do the same: -2 / -1, the file untouched. */

static void why(char *err, long cap, const char *a, const char *file, const char *b)
{
    if (err && cap > 0) {
        cl_copy(err, a, cap);
        cl_cat(err, file, cap);
        cl_cat(err, b, cap);
    }
}

/* the store's text read and checked: 0 (b is 0 when there is no file), -1 with pi's words in err */
static int store_open(cl_sys *sys, const char *file, char **b, long *n, jv *o, char *err, long cap)
{
    jit it;
    jv k, e;
    char name[300], quoted[330];
    *b = 0;
    *n = 0;
    if (sys->kind(sys->u, file) != 1)
        return 0;
    if (sys->read(sys->u, file, 256L * 1024, b, n)) {
        *b = 0;
        why(err, cap, "Failed to read trust store ", file, ": the file cannot be read");
        return -1;
    }
    {
        const char *t = *b;
        long m = *n;
        if (m >= 3 && (unsigned char)t[0] == 0xef && (unsigned char)t[1] == 0xbb && (unsigned char)t[2] == 0xbf) {
            t += 3;                     /* pi's stripBom */
            m -= 3;
        }
        if (json_parse(t, m, o)) {
            why(err, cap, "Failed to read trust store ", file, ": the text is not valid JSON");
            free(*b);
            *b = 0;
            return -1;
        }
    }
    if (json_type(*o) != J_OBJ) {
        why(err, cap, "Invalid trust store ", file, ": expected an object");
        free(*b);
        *b = 0;
        return -1;
    }
    json_iter(*o, &it);
    while (json_next(&it, &k, &e)) {
        int ty = json_type(e);
        if (ty != J_TRUE && ty != J_FALSE && ty != J_NULL) {
            jw q;
            json_str(k, name, sizeof(name));
            jw_init(&q);
            jw_strz(&q, name);
            cl_copy(quoted, q.p ? q.p : "", sizeof(quoted));
            jw_free(&q);
            why(err, cap, "Invalid trust store ", file, ": value for ");
            if (err && cap > 0) {
                cl_cat(err, quoted, cap);
                cl_cat(err, " must be true, false, or null", cap);
            }
            free(*b);
            *b = 0;
            return -1;
        }
    }
    return 0;
}

int trust_check(cl_sys *sys, const char *file, char *err, long cap)
{
    char *b;
    long n;
    jv o;
    int rc;
    if (err && cap > 0)
        err[0] = 0;
    rc = store_open(sys, file, &b, &n, &o, err, cap);
    free(b);
    return rc;
}

int trust_near(cl_sys *sys, const char *file, const char *dir, char *where, long cap)
{
    long n = 0;
    char *b, cur[300], up[300], scur[300];
    jv o;
    int got = -1;
    if (where && cap > 0)
        where[0] = 0;
    if (store_open(sys, file, &b, &n, &o, 0, 0))
        return -2;
    if (!b)
        return -1;
    /* pi's findNearestTrustEntry: the directory, then each parent, the first true or false */
    cl_copy(cur, dir, sizeof(cur));
    while (got < 0 && cur[0]) {
        jit it;
        jv k, e;
        if (stored(cur, scur, sizeof(scur)))
            break;
        json_iter(o, &it);
        while (got < 0 && json_next(&it, &k, &e)) {
            char path[300];
            json_str(k, path, sizeof(path));
            if (cl_strieq(path, scur) && (json_type(e) == J_TRUE || json_type(e) == J_FALSE)) {
                got = json_type(e) == J_TRUE;
                if (where)
                    cl_copy(where, cur, cap);
            }
        }
        if (path_parent(cur, up, sizeof(up)))
            break;
        cl_copy(cur, up, sizeof(cur));
    }
    free(b);
    return got;
}

typedef struct tent {
    char key[300];
    const char *val;            /* "true", "false", "null" */
} tent;

static int tcmp(const void *a, const void *b)
{
    return strcmp(((const tent *)a)->key, ((const tent *)b)->key);
}

int trust_put(cl_sys *sys, const char *file, const char *path, int value)
{
    long n = 0;
    char *b, dir[300], spath[300];
    tent *t;
    int nt = 0, cap = 1, i, found = 0, rc = -1;
    jv o, k, e;
    jit it;
    jw w;
    if (stored(path, spath, sizeof(spath)))
        return -1;
    if (store_open(sys, file, &b, &n, &o, 0, 0))
        return -1;              /* pi refuses a file it cannot read: not overwritten */
    if (b) {
        json_iter(o, &it);
        while (json_next(&it, &k, &e))
            cap++;
    }
    t = (tent *)calloc((size_t)cap, sizeof(tent));
    if (!t) {
        free(b);
        return -1;
    }
    if (b) {
        json_iter(o, &it);
        while (json_next(&it, &k, &e)) {
            int ty = json_type(e);
            json_str(k, t[nt].key, sizeof(t[0].key));
            t[nt].val = ty == J_TRUE ? "true" : ty == J_FALSE ? "false" : "null";
            nt++;
        }
    }
    free(b);
    /* pi's setMany: a decision set, null deletes; the same directory in any case is one */
    for (i = 0; i < nt; i++)
        if (cl_strieq(t[i].key, spath)) {
            found = 1;
            if (value < 0) {
                t[i] = t[--nt];
                i--;
            } else {
                cl_copy(t[i].key, spath, sizeof(t[0].key));
                t[i].val = value ? "true" : "false";
            }
        }
    if (!found && value >= 0) {
        cl_copy(t[nt].key, spath, sizeof(t[0].key));
        t[nt].val = value ? "true" : "false";
        nt++;
    }
    /* writeTrustFile: keys sorted, JSON.stringify(data, null, 2) and a newline */
    qsort(t, (size_t)nt, sizeof(tent), tcmp);
    jw_init(&w);
    jw_rawz(&w, nt ? "{\n" : "{}\n");
    for (i = 0; i < nt; i++) {
        jw_rawz(&w, "  ");
        jw_strz(&w, t[i].key);
        jw_rawz(&w, ": ");
        jw_rawz(&w, t[i].val);
        jw_rawz(&w, i + 1 < nt ? ",\n" : "\n}\n");
    }
    free(t);
    if (!w.oom) {
        if (sys->mkdir && path_parent(file, dir, sizeof(dir)) == 0 && dir[0])
            sys->mkdir(sys->u, dir);
        rc = sys->write(sys->u, file, w.p, w.n) ? -1 : 0;
    }
    jw_free(&w);
    return rc;
}
