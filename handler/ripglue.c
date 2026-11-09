/* ripglue.c -- see ripglue.h */
#include <string.h>
#include "ripglue.h"

void ripglue_init(ripglue *g, const ripglue_io *io)
{
    memset(g, 0, sizeof *g);
    g->io = *io;
}

int ripglue_arm(ripglue *g, int mode, int proto, const char *name)
{
    (void)proto;
    if (mode != 0 || g->xfer) {
        g->refused++;
        return 0;
    }
    strncpy(g->name, name ? name : "", RIPGLUE_NAME - 1);
    g->name[RIPGLUE_NAME - 1] = 0;
    g->armed++;
    g->pending = 1;
    return 1;
}

int ripglue_write(ripglue *g, ripglue_feed feed, void *user, const unsigned char *b, long n,
                  const unsigned char **rest, long *restn)
{
    long fed;
    if (g->xfer)
        return RIPGLUE_HELD;
    g->pending = 0;
    fed = feed(user, b, n);
    if (!g->pending)
        return RIPGLUE_FED;
    g->pending = 0;
    g->xfer = 1;
    if (fed < 0)
        fed = 0;
    if (fed > n)
        fed = n;
    *rest = b + fed;
    *restn = n - fed;
    return RIPGLUE_START;
}

void ripglue_done(ripglue *g)
{
    g->xfer = 0;
    g->pending = 0;
    g->cur = 0;
    g->len = 0;
}

void ripglue_begin(ripglue *g, const unsigned char *rest, long n)
{
    g->cur = rest;
    g->len = n;
}

static int glue_getc(void *u, int timeout_ms)
{
    ripglue *g = (ripglue *)u;
    while (g->len <= 0) {
        int r = g->io.next(g->io.user, timeout_ms, &g->cur, &g->len);
        if (r == 0)
            return ZM_TIMEOUT;
        if (r < 0)
            return ZM_LINE_ERROR;
    }
    g->len--;
    return *g->cur++;
}

static int glue_write(void *u, const unsigned char *b, long n)
{
    ripglue *g = (ripglue *)u;
    return g->io.send(g->io.user, b, n);
}

void ripglue_line(ripglue *g, zm_line *l)
{
    l->getc = glue_getc;
    l->write = glue_write;
    l->user = g;
}

int ripglue_run(ripglue *g, vt_dl_sink sink, const char *host, int *delivered)
{
    zm_line l;
    ripglue_line(g, &l);
    return ripxfer_receive(&l, sink, host, g->name, delivered);
}
