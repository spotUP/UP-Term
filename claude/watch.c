/* watch -- FileChanged and the async hooks' answers (A4 gaps 2).
 *
 * FileChanged: the files to watch are the FileChanged matchers' names
 * (split on |, each a file in the start directory, as Claude Code
 * registers them) and the dynamic list the hooks return as watchPaths
 * (SessionStart, CwdChanged, FileChanged replace it). Claude Code uses a
 * filesystem watcher; here the files' times are compared at most every
 * two seconds -- while the screen waits for keys and after each tool
 * round -- so a change made by anything (an Edit, a Bash command, another
 * program) is seen. A change runs the FileChanged hooks whose matcher
 * takes the file's name, with file_path and event (change, add, unlink).
 * Portable C89, host-tested through the REPL suite. */
#include <stdlib.h>
#include <string.h>
#include "repl_int.h"
#include "path.h"
#include "util.h"

#define WATCH_MAX 24
#define WATCH_EVERY_MS 2000UL

typedef struct wfile {
    char path[300];
    long mt;                    /* its time when last looked at, -1 absent */
    int dyn;                    /* from watchPaths (else a matcher's name) */
    int seen;                   /* looked at once (the first look only remembers) */
} wfile;

typedef struct cl_watch {
    wfile f[WATCH_MAX];
    int n;
    unsigned long last;
} cl_watch;

void watch_free(cl_repl *r)
{
    free(r->watch);
    r->watch = 0;
}

static cl_watch *get(cl_repl *r)
{
    if (!r->watch)
        r->watch = (cl_watch *)calloc(1, sizeof(cl_watch));
    return r->watch;
}

static void add(cl_watch *w, const char *path, int dyn, long mt)
{
    int i;
    for (i = 0; i < w->n; i++)
        if (cl_strieq(w->f[i].path, path)) {
            w->f[i].dyn |= dyn;
            return;
        }
    if (w->n >= WATCH_MAX)
        return;
    cl_copy(w->f[w->n].path, path, sizeof(w->f[0].path));
    w->f[w->n].dyn = dyn;
    w->f[w->n].mt = mt;
    w->f[w->n].seen = mt != -2;
    w->n++;
}

static long mtime_of(cl_repl *r, const char *p)
{
    if (!r->sys->mtime || r->sys->kind(r->sys->u, p) != 1)
        return -1;
    return r->sys->mtime(r->sys->u, p);
}

void watch_set(cl_repl *r, const char *json, long n)
{
    cl_watch *w = get(r);
    jv a, e;
    jit it;
    int i, k = 0;
    if (!w || json_parse(json, n, &a) || json_type(a) != J_ARR)
        return;
    /* watchPaths replaces the dynamic list (the matchers' names stay) */
    for (i = 0; i < w->n; i++)
        if (!w->f[i].dyn)
            w->f[k++] = w->f[i];
    w->n = k;
    json_iter(a, &it);
    while (json_next(&it, 0, &e)) {
        char p[300];
        if (json_type(e) == J_STR && json_str(e, p, sizeof(p)) > 0)
            add(w, p, 1, mtime_of(r, p));
    }
}

typedef struct names_ctx {
    cl_repl *r;
    cl_watch *w;
} names_ctx;

static void one_name(void *c, const char *name)
{
    names_ctx *x = (names_ctx *)c;
    char p[300];
    if (!strcmp(name, "*") || !name[0])
        return;
    if (path_join(x->r->tools.root, name, p, sizeof(p)) == 0)
        add(x->w, p, 0, -2);        /* -2: not looked at yet */
}

/* the FileChanged hooks for one file (a JSON answer's watchPaths, systemMessage) */
static void changed(cl_repl *r, const char *path, const char *event)
{
    cl_hookres h;
    jw ex;
    const char *base = path, *s;
    for (s = path; *s; s++)
        if (*s == '/' || *s == ':')
            base = s + 1;
    if (!hooks_any(&r->hooks, HK_FILE_CHANGED, base))
        return;
    hookres_init(&h);
    jw_init(&ex);
    jw_rawz(&ex, ",\"file_path\":");
    jw_strz(&ex, path);
    jw_rawz(&ex, ",\"event\":");
    jw_strz(&ex, event);
    pol_env_prepare(r);
    hooks_run(&r->hooks, HK_FILE_CHANGED, base, ex.p, &h);
    pol_env_apply(r);
    jw_free(&ex);
    r->n_file_changed++;
    if (h.shown.n)
        ui_line(&r->ui, h.shown.p);
    if (h.has_watch)
        watch_set(r, h.watch.p ? h.watch.p : "[]", h.watch.n ? h.watch.n : 2);
    hookres_free(&h);
}

int pol_files_changed(cl_repl *r, jw *w)
{
    cl_watch *ws;
    names_ctx c;
    unsigned long now = r->io->ms ? r->io->ms(r->io->u) : 0;
    int i, n = 0;
    (void)w;                        /* FileChanged tells Claude nothing itself */
    if (!r->hooks.cfg || !hooks_has(&r->hooks, HK_FILE_CHANGED))
        return 0;
    ws = get(r);
    if (!ws)
        return 0;
    if (ws->last && now - ws->last < WATCH_EVERY_MS)
        return 0;
    ws->last = now ? now : 1;
    c.r = r;
    c.w = ws;
    hooks_watch_names(&r->hooks, one_name, &c);
    for (i = 0; i < ws->n; i++) {
        wfile *f = &ws->f[i];
        long mt = mtime_of(r, f->path);
        const char *ev = 0;
        if (!f->seen) {
            f->seen = 1;            /* the first look only remembers */
            f->mt = mt;
            continue;
        }
        if (f->mt < 0 && mt >= 0)
            ev = "add";
        else if (f->mt >= 0 && mt < 0)
            ev = "unlink";
        else if (mt >= 0 && mt != f->mt)
            ev = "change";
        f->mt = mt;
        if (ev) {
            char p[300];
            cl_copy(p, f->path, sizeof(p));
            changed(r, p, ev);
            n++;
            ws = r->watch;          /* watchPaths may have changed the list */
            if (!ws)
                break;
        }
    }
    return n;
}

void watch_start(cl_repl *r)
{
    cl_watch *ws;
    names_ctx c;
    int i;
    if (!hooks_has(&r->hooks, HK_FILE_CHANGED))
        return;
    ws = get(r);
    if (!ws)
        return;
    c.r = r;
    c.w = ws;
    hooks_watch_names(&r->hooks, one_name, &c);
    for (i = 0; i < ws->n; i++)
        if (!ws->f[i].seen) {
            ws->f[i].mt = mtime_of(r, ws->f[i].path);
            ws->f[i].seen = 1;
        }
}

int pol_async_collect(cl_repl *r, jw *w)
{
    return r->hooks.async ? hooks_async_poll(&r->hooks, w) : 0;
}
