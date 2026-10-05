/* checkpoint -- see checkpoint.h. */
#include <stdlib.h>
#include <string.h>
#include "checkpoint.h"
#include "path.h"
#include "util.h"

static cl_checkpoints *current;

void checkpoint_use(cl_checkpoints *c)
{
    current = c;
}

int checkpoint_before_write(const char *path)
{
    return current ? cp_before_write(current, path) : 0;
}

void cp_init(cl_checkpoints *c, cl_sys *sys, const char *dir)
{
    memset(c, 0, sizeof(*c));
    c->sys = sys;
    cl_copy(c->dir, dir, sizeof(c->dir));
}

static void drop(cl_checkpoints *c, int i)
{
    if (c->e[i].kept && c->sys->remove)
        c->sys->remove(c->sys->u, c->e[i].snap);
    if (c->e[i].kept)
        c->bytes -= c->e[i].size;
    memmove(&c->e[i], &c->e[i + 1], (size_t)(c->n - i - 1) * sizeof(cl_cpent));
    c->n--;
}

void cp_free(cl_checkpoints *c)
{
    while (c->n)
        drop(c, c->n - 1);
    free(c->e);
    c->e = 0;
    c->cap = 0;
    if (current == c)
        current = 0;
}

void cp_turn(cl_checkpoints *c, int turn)
{
    c->turn = turn;
}

int cp_before_write(cl_checkpoints *c, const char *path)
{
    cl_cpent *e;
    char num[16];
    int i, k;
    for (i = 0; i < c->n; i++)
        if (c->e[i].turn == c->turn && cl_strieq(c->e[i].path, path))
            return 0;
    if (c->n == c->cap) {
        int nc = c->cap ? c->cap * 2 : 16;
        cl_cpent *q = (cl_cpent *)realloc(c->e, (size_t)nc * sizeof(cl_cpent));
        if (!q)
            return -1;
        c->e = q;
        c->cap = nc;
    }
    e = &c->e[c->n];
    memset(e, 0, sizeof(*e));
    e->turn = c->turn;
    cl_copy(e->path, path, sizeof(e->path));
    k = c->sys->kind(c->sys->u, path);
    e->existed = k == 1;
    if (k == 1) {
        char *b = 0;
        long n = 0;
        int rc = c->sys->read(c->sys->u, path, CP_BYTES / 2, &b, &n);
        if (rc == 0) {
            /* room: the oldest turns' snapshots go */
            while (c->bytes + n > CP_BYTES && c->n && c->e[0].turn != c->turn)
                drop(c, 0);
            e = &c->e[c->n];
            memset(e, 0, sizeof(*e));
            e->turn = c->turn;
            e->existed = 1;
            cl_copy(e->path, path, sizeof(e->path));
            if (c->sys->mkdir)
                c->sys->mkdir(c->sys->u, c->dir);
            cl_ltoa(++c->seq, num);
            if (path_join(c->dir, "cp", e->snap, sizeof(e->snap)) == 0) {
                cl_cat(e->snap, num, sizeof(e->snap));
                if (c->bytes + n <= CP_BYTES && c->sys->write(c->sys->u, e->snap, b, n) == 0) {
                    e->kept = 1;
                    e->size = n;
                    c->bytes += n;
                    c->n_snaps++;
                }
            }
        }
        free(b);
    } else
        c->n_snaps++;               /* new: undone by deleting it */
    c->n++;
    return e->existed && !e->kept ? -1 : 0;
}

int cp_files_since(const cl_checkpoints *c, int turn)
{
    int i, j, k = 0;
    for (i = 0; i < c->n; i++) {
        if (c->e[i].turn < turn)
            continue;
        for (j = 0; j < i; j++)
            if (c->e[j].turn >= turn && cl_strieq(c->e[j].path, c->e[i].path))
                break;
        if (j == i)
            k++;
    }
    return k;
}

int cp_restore(cl_checkpoints *c, int turn, int *lost, char *report, long cap)
{
    int i, j, done = 0;
    *lost = 0;
    if (report && cap)
        report[0] = 0;
    for (i = 0; i < c->n; i++) {
        cl_cpent *e = &c->e[i];
        int ok;
        if (e->turn < turn)
            continue;
        for (j = 0; j < i; j++)
            if (c->e[j].turn >= turn && cl_strieq(c->e[j].path, e->path))
                break;
        if (j < i)
            continue;               /* an earlier snapshot of it is the one */
        if (!e->existed)
            ok = c->sys->kind(c->sys->u, e->path) == 0 ||
                 (c->sys->remove && c->sys->remove(c->sys->u, e->path) == 0);
        else if (e->kept) {
            char *b = 0;
            long n = 0;
            ok = c->sys->read(c->sys->u, e->snap, CP_BYTES, &b, &n) == 0 &&
                 c->sys->write(c->sys->u, e->path, b, n) == 0;
            free(b);
        } else
            ok = 0;
        if (ok)
            done++;
        else
            (*lost)++;
        if (report) {
            cl_cat(report, ok ? (e->existed ? "restored " : "deleted ") : "could not restore ", cap);
            cl_cat(report, e->path, cap);
            cl_cat(report, "\n", cap);
        }
    }
    for (i = c->n - 1; i >= 0; i--)
        if (c->e[i].turn >= turn)
            drop(c, i);
    return done;
}
