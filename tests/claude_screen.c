/* claude_screen -- see claude_screen.h. */
#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <time.h>
#include "claude_screen.h"

cscreen cs;

void cs_open(int cols, int rows, const char **script)
{
    cs_close();
    cs.vt = h_new(cols, rows, VT_XTERM);
    cs.cols = cols;
    cs.rows = rows;
    cs.script = script;
    jw_init(&cs.sent);
    h_reply_clear();
}

void cs_close(void)
{
    if (cs.vt)
        vt_free(cs.vt);
    jw_free(&cs.sent);
    memset(&cs, 0, sizeof(cs));
}

void cs_resize(int cols, int rows)
{
    vt_resize(cs.vt, cols, rows);
    cs.cols = cols;
    cs.rows = rows;
}

static void w(void *u, const char *s, long n)
{
    (void)u;
    jw_raw(&cs.sent, s, n);
    cs.writes++;
    vt_write(cs.vt, (const vt_u8 *)s, n);
}

static long rd(void *u, char *buf, long cap, long ms)
{
    const char *c;
    long n;
    (void)u;
    if (cs.before_read)
        cs.before_read();
    if (h_reply_len) {
        n = h_reply_len < cap ? h_reply_len : cap;
        memcpy(buf, h_reply, (size_t)n);
        memmove(h_reply, h_reply + n, (size_t)(h_reply_len - n));
        h_reply_len -= (int)n;
        return n;
    }
    if (!cs.script || !cs.script[cs.next])
        return ms > 0 ? -1 : 0;
    c = cs.script[cs.next];
    if (ms <= 0 && c[0] != '!')
        return 0;                   /* typed only once the program waits */
    if (c[0] == '!')
        c++;
    n = (long)strlen(c);
    if (n > cap)
        n = cap;
    memcpy(buf, c, (size_t)n);
    cs.next++;
    return n;
}

static int sz(void *u, int *c, int *r)
{
    (void)u;
    *c = cs.cols;
    *r = cs.rows;
    return 0;
}

static int raw(void *u, int on)
{
    (void)u;
    cs.raw_on = on;
    cs.raw_calls++;
    return 0;
}

static unsigned long ms(void *u)
{
    (void)u;
    return cs.clock += 10;
}

static int brk(void *u)
{
    int b = cs.brk;
    (void)u;
    cs.brk = 0;
    return b;
}

/* the clock moves on; a short wait (a command polled in the foreground,
 * at most 100 ms) also passes for real, so the child process it waits
 * for gets the time (a retry's seconds stay make-believe) */
static int slp(void *u, long n)
{
    (void)u;
    cs.clock += (unsigned long)n;
    if (n > 0 && n <= 100) {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = n * 1000000L;
        nanosleep(&ts, 0);
    }
    return 0;
}

void cs_io(cl_io *io)
{
    memset(io, 0, sizeof(*io));
    io->write = w;
    io->read = rd;
    io->size = sz;
    io->raw = raw;
    io->ms = ms;
    io->brk = brk;
    io->sleep = slp;
}

const char *cs_row(int r)
{
    return h_row(cs.vt, r);
}

int cs_find(const char *text)
{
    int r;
    for (r = 0; r < cs.rows; r++)
        if (strstr(cs_row(r), text))
            return r;
    return -1;
}
