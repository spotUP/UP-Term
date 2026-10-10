/* datadir -- see datadir.h. */
#include <string.h>
#include <stdlib.h>
#include "datadir.h"
#include "path.h"
#include "util.h"

#define DD_MAXFILE (32L * 1024 * 1024)   /* one file copied across volumes is read whole */

int dd_dir(cl_sys *sys, const char *prog, char *out, long cap)
{
    int disk = sys && (sys->quiet_kind ? sys->quiet_kind(sys->u, DD_KIT) : sys->kind(sys->u, DD_KIT)) == 2;
    const char *base = disk ? DD_DISK "/" : DD_CONF;
    if ((long)(strlen(base) + strlen(prog)) >= cap)
        return -1;
    strcpy(out, base);
    strcat(out, prog);
    return disk;
}

int dd_mkdirs(cl_sys *sys, const char *path)
{
    char up[400];
    if (!sys->mkdir)
        return -1;
    if (sys->kind(sys->u, path) == 2)
        return 0;
    if (path_parent(path, up, sizeof(up)) == 0 && strcmp(up, path) && dd_mkdirs(sys, up))
        return -1;
    return sys->mkdir(sys->u, path) == 0 && sys->kind(sys->u, path) == 2 ? 0 : -1;
}

/* ---- a directory's entries, all of them (the list is read first, then
 * acted on: the directory changes underneath) ---- */

typedef struct dd_list {
    cl_dirent *e;
    int n, cap, oom;
} dd_list;

static int dd_one(void *c, const cl_dirent *e)
{
    dd_list *l = (dd_list *)c;
    if (l->n == l->cap) {
        int nc = l->cap ? l->cap * 2 : 32;
        cl_dirent *g = (cl_dirent *)realloc(l->e, (size_t)nc * sizeof(cl_dirent));
        if (!g) {
            l->oom = 1;
            return 1;
        }
        l->e = g;
        l->cap = nc;
    }
    l->e[l->n++] = *e;
    return 0;
}

static int dd_entries(cl_sys *sys, const char *dir, dd_list *l)
{
    memset(l, 0, sizeof(*l));
    if (!sys->list || sys->list(sys->u, dir, dd_one, l) < 0 || l->oom) {
        free(l->e);
        l->e = 0;
        return -1;
    }
    return 0;
}

long dd_remove_tree(cl_sys *sys, const char *path)
{
    long gone = 0;
    int i;
    dd_list l;
    if (!sys->remove)
        return 0;
    if (sys->kind(sys->u, path) == 2 && dd_entries(sys, path, &l) == 0) {
        for (i = 0; i < l.n; i++) {
            char p[400];
            if (path_join(path, l.e[i].name, p, sizeof(p)) == 0)
                gone += dd_remove_tree(sys, p);
        }
        free(l.e);
    }
    if (sys->kind(sys->u, path) != 0 && sys->remove(sys->u, path) == 0)
        gone++;
    return gone;
}

/* from copied to to (a file, or a tree): 0, -1 */
static int dd_copy(cl_sys *sys, const char *from, const char *to)
{
    int k = sys->kind(sys->u, from), i, rc = 0;
    dd_list l;
    if (k == 1) {
        char *b = 0;
        long n = 0;
        if (sys->read(sys->u, from, DD_MAXFILE, &b, &n))
            return -1;
        rc = sys->write(sys->u, to, b, n);
        free(b);
        return rc ? -1 : 0;
    }
    if (k != 2 || sys->mkdir(sys->u, to) || dd_entries(sys, from, &l))
        return -1;
    for (i = 0; i < l.n && !rc; i++) {
        char a[400], b[400];
        if (path_join(from, l.e[i].name, a, sizeof(a)) || path_join(to, l.e[i].name, b, sizeof(b)))
            rc = -1;
        else
            rc = dd_copy(sys, a, b);
    }
    free(l.e);
    return rc;
}

int dd_migrate(cl_sys *sys, const char *from, const char *to)
{
    char up[400];
    if (!from || !to || !*from || !*to || cl_strieq(from, to) || sys->kind(sys->u, from) == 0)
        return 0;
    if (sys->kind(sys->u, to) != 0)
        return 2;
    if (!sys->write || !sys->read || !sys->mkdir || !sys->remove)
        return -1;
    if (path_parent(to, up, sizeof(up)) || dd_mkdirs(sys, up))
        return -1;
    if (sys->rename && sys->rename(sys->u, from, to) == 0)
        return 1;
    if (dd_copy(sys, from, to)) {
        dd_remove_tree(sys, to);    /* half a copy is no copy: from stays the one */
        return -1;
    }
    dd_remove_tree(sys, from);
    return 1;
}
