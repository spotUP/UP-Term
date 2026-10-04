/* See complete_core.h. Portable C89: no OS calls. */
#include "complete_core.h"

int cc_is_command(long entry_type, unsigned long protection)
{
    if (entry_type >= 0)
        return 0; /* a directory (or no type): never a command */
    return !(protection & CC_FIBF_EXECUTE) || (protection & CC_FIBF_SCRIPT) != 0;
}

int cc_resident_listed(long seg_uc)
{
    return seg_uc >= 0 || seg_uc == CC_CMD_INTERNAL;
}

typedef struct cc_seen {
    long lock[CC_DIRS_MAX];
    int own[CC_DIRS_MAX];    /* a c_next lock: dropped at the end */
    int n;
} cc_seen;

/* one directory: skipped when already seen, else visited and remembered */
static int cc_step(const cc_dirs_os *os, void *u, cc_seen *s, long d, int own)
{
    int i, r;
    for (i = 0; i < s->n; i++)
        if (os->same(s->lock[i], d)) {
            if (own)
                os->drop(d);
            return 0;
        }
    r = os->visit(u, d);
    if (s->n < CC_DIRS_MAX) {
        s->lock[s->n] = d;
        s->own[s->n] = own;
        s->n++;
    } else if (own) {
        os->drop(d);
    }
    return r;
}

int cc_walk_command_dirs(const cc_dirs_os *os, void *u)
{
    cc_seen s;
    long d;
    int r = 0, i;
    s.n = 0;
    while (!r && (d = os->c_next(u)) != 0)
        r = cc_step(os, u, &s, d, 1);
    os->c_end(u);
    while (!r && (d = os->p_next(u)) != 0)
        r = cc_step(os, u, &s, d, 0);
    for (i = 0; i < s.n; i++)
        if (s.own[i])
            os->drop(s.lock[i]);
    return r;
}
