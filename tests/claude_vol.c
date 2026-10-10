/* claude_vol -- see claude_vol.h. */
#include <string.h>
#include "claude_vol.h"
#include "../claude/util.h"

static void map(cl_vol *v, const char *path, char *out, long cap)
{
    static const char *const pre[2] = { "ENVARC:", "UP-Term:" };
    int i;
    for (i = 0; i < 2; i++) {
        long n = (long)strlen(pre[i]);
        const char *host = i ? v->kit : v->envarc;
        if (cl_strnieq(path, pre[i], n)) {
            if (!*host) {
                cl_copy(out, "/nonexistent-volume", cap);    /* the assign is not there */
                return;
            }
            cl_copy(out, host, cap);
            if (path[n]) {
                cl_cat(out, "/", cap);
                cl_cat(out, path + n, cap);
            }
            return;
        }
    }
    cl_copy(out, path, cap);
}

#define V ((cl_vol *)u)
#define M(p, b) char b[600]; map(V, p, b, sizeof(b))

static int v_read(void *u, const char *path, long max, char **o, long *n)
{
    M(path, p);
    return V->inner.read(V->inner.u, p, max, o, n);
}

static int v_write(void *u, const char *path, const char *s, long n)
{
    M(path, p);
    return V->inner.write(V->inner.u, p, s, n);
}

static int v_list(void *u, const char *path, cl_dir_fn fn, void *c)
{
    M(path, p);
    return V->inner.list(V->inner.u, p, fn, c);
}

static int v_kind(void *u, const char *path)
{
    M(path, p);
    return V->inner.kind(V->inner.u, p);
}

static long v_mtime(void *u, const char *path)
{
    M(path, p);
    return V->inner.mtime ? V->inner.mtime(V->inner.u, p) : -1;
}

static int v_append(void *u, const char *path, const char *s, long n)
{
    M(path, p);
    return V->inner.append(V->inner.u, p, s, n);
}

static int v_mkdir(void *u, const char *path)
{
    M(path, p);
    return V->inner.mkdir(V->inner.u, p);
}

static int v_remove(void *u, const char *path)
{
    M(path, p);
    return V->inner.remove(V->inner.u, p);
}

static int v_rename(void *u, const char *from, const char *to)
{
    int rc;
    M(from, a);
    {
        M(to, b);
        if (V->no_rename)
            return -1;
        rc = V->inner.rename(V->inner.u, a, b);
    }
    if (rc == 0)
        V->renames++;
    return rc;
}

static int v_quiet_kind(void *u, const char *path)
{
    V->quiet_kinds++;
    return v_kind(u, path);
}

static int v_canon(void *u, const char *path, char *out, long cap)
{
    M(path, p);
    return V->inner.canon(V->inner.u, p, out, cap);
}

/* the rest take no path: handed on with the inner u */
static int v_run(void *u, const char *cmd, int t, char *out, long cap, long *outn, long *rc)
{
    return V->inner.run(V->inner.u, cmd, t, out, cap, outn, rc);
}

static const char *v_err(void *u)
{
    return V->inner.err(V->inner.u);
}

static int v_bg_start(void *u, const char *cmd, long *job)
{
    return V->inner.bg_start(V->inner.u, cmd, job);
}

static int v_bg_read(void *u, long job, long from, char *out, long cap, long *outn, int *running, long *rc)
{
    return V->inner.bg_read(V->inner.u, job, from, out, cap, outn, running, rc);
}

static int v_bg_kill(void *u, long job)
{
    return V->inner.bg_kill(V->inner.u, job);
}

static void v_bg_drop(void *u, long job)
{
    V->inner.bg_drop(V->inner.u, job);
}

static long v_getenv(void *u, const char *name, char *out, long cap)
{
    return V->inner.getenv(V->inner.u, name, out, cap);
}

static int v_setenv(void *u, const char *name, const char *value)
{
    return V->inner.setenv(V->inner.u, name, value);
}

static int v_clip(void *u, const char *s, long n)
{
    return V->inner.clip(V->inner.u, s, n);
}

static int v_info(void *u, const char *what, char *out, long cap)
{
    return V->inner.info(V->inner.u, what, out, cap);
}

static long v_now(void *u)
{
    return V->inner.now(V->inner.u);
}

static int v_pause(void *u, long ms)
{
    return V->inner.pause(V->inner.u, ms);
}

static long v_bg_size(void *u, long job)
{
    return V->inner.bg_size(V->inner.u, job);
}

static const char *v_bg_file(void *u, long job)
{
    return V->inner.bg_file(V->inner.u, job);
}

void vol_init(cl_vol *v, const cl_sys *inner, cl_sys *out)
{
    v->inner = *inner;
    *out = *inner;
    out->u = v;
    out->read = v_read;
    out->write = v_write;
    out->list = v_list;
    out->kind = v_kind;
    out->mtime = inner->mtime ? v_mtime : 0;
    out->append = inner->append ? v_append : 0;
    out->mkdir = inner->mkdir ? v_mkdir : 0;
    out->remove = inner->remove ? v_remove : 0;
    out->rename = inner->rename ? v_rename : 0;
    out->canon = inner->canon ? v_canon : 0;
    out->quiet_kind = v_quiet_kind;     /* the Amiga's has one: so must the stand-in */
    out->run = inner->run ? v_run : 0;
    out->err = inner->err ? v_err : 0;
    out->bg_start = inner->bg_start ? v_bg_start : 0;
    out->bg_read = inner->bg_read ? v_bg_read : 0;
    out->bg_kill = inner->bg_kill ? v_bg_kill : 0;
    out->bg_drop = inner->bg_drop ? v_bg_drop : 0;
    out->getenv = inner->getenv ? v_getenv : 0;
    out->setenv = inner->setenv ? v_setenv : 0;
    out->clip = inner->clip ? v_clip : 0;
    out->info = inner->info ? v_info : 0;
    out->now = inner->now ? v_now : 0;
    out->pause = inner->pause ? v_pause : 0;
    out->bg_size = inner->bg_size ? v_bg_size : 0;
    out->bg_file = inner->bg_file ? v_bg_file : 0;
}
