/* trust -- what C:Claude remembers per project directory (A4 gaps 2), as
 * Claude Code keeps it in ~/.claude.json: <home>/claude.json,
 *   {"projects": {"Work:proj": {"hasTrustDialogAccepted": true,
 *                 "hasClaudeMdExternalIncludesApproved": false,
 *                 "hasClaudeMdExternalIncludesWarningShown": true}}}
 * Workspace trust: a directory is trusted when it or a directory above it
 * was accepted (Claude Code: outside a repository the trust covers the
 * subdirectories). The file's other keys and projects are kept as they
 * are on a write.
 * Portable C89 over sys.h, host-tested (tests/test_claude_config.c and the
 * REPL suite). */
#include <stdlib.h>
#include <string.h>
#include "trust.h"
#include "config.h"
#include "path.h"
#include "util.h"

static int file_of(const char *home, char *out, long cap)
{
    return path_join(home, "claude.json", out, cap);
}

/* the file's text (0 none) */
static char *load(cl_sys *sys, const char *home, long *n)
{
    char f[300];
    char *b = 0;
    if (file_of(home, f, sizeof(f)) || sys->kind(sys->u, f) != 1 || sys->read(sys->u, f, 256L * 1024, &b, n))
        return 0;
    return b;
}

int trust_get(cl_sys *sys, const char *home, const char *dir, const char *key, int inherit)
{
    long n = 0;
    char *b = load(sys, home, &n);
    jv o, p, k, e, x;
    jit it;
    int got = -1;
    if (!b)
        return -1;
    if (json_parse(b, n, &o) == 0 && json_get(o, "projects", &p) && json_type(p) == J_OBJ) {
        json_iter(p, &it);
        while (json_next(&it, &k, &e)) {
            char path[300];
            json_str(k, path, sizeof(path));
            if (!(cl_strieq(path, dir) || (inherit && path_inside(path, dir))))
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
    char *b = load(sys, home, &n), f[300];
    jv o, p, k, e;
    jit it;
    jw w;
    int done = 0, first = 1, rc;
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
