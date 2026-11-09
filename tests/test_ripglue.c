/* RIP-7b handler glue. Step 1: the engine gives back the bytes that are not its own
 * (vt_feed_stop / vt_fed). The bytes after a level-9 block are protocol, not terminal output. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include "harness.h"
#include "../engine/vtengine.h"
#include "../handler/ripglue.h"

static int blocks, do_stop = 1;

static void on_block(void *u, int mode, int proto, int type, const char *name)
{
    vt_term **tp = (vt_term **)u;
    (void)mode;
    (void)proto;
    (void)type;
    (void)name;
    blocks++;
    if (do_stop)
        vt_feed_stop(*tp);
}

static int grid_has(vt_term *t, const char *s)
{
    static char scr[8192];
    vt_screen_text(t, scr, (long)sizeof scr);
    return strstr(scr, s) != 0;
}

/* rip: vt_set_rip argument; prefix: bytes before the `!|9` line (RIP on needs ESC [ 2 ! in mode 1) */
static void stop_case(int rip, const char *prefix)
{
    static const char proto[] = "**\030B00000000000000\r\x8a\x11" "garbage-after-block";
    char buf[512];
    long cmd_end, total;
    vt_callbacks cb;
    vt_term *t = 0;
    memset(&cb, 0, sizeof cb);
    cb.rip_block = on_block;
    t = vt_new(80, 25, 100, &cb, &t);
    vt_set_cell_pixels(t, 8, 14);
    vt_set_rip(t, rip);
    blocks = 0;
    strcpy(buf, prefix);
    strcat(buf, "!|9\033" "0" "5" "00" "0000" "X.ICN" "\r\n");
    cmd_end = (long)strlen(buf);
    memcpy(buf + cmd_end, proto, sizeof proto - 1);
    total = cmd_end + (long)sizeof proto - 1;
    vt_feed(t, (const vt_u8 *)buf, total);
    CHECK_INT(blocks, 1);
    CHECK_INT(vt_rip_blocks(t), 1);
    CHECK(vt_fed(t) == cmd_end - 1 || vt_fed(t) == cmd_end); /* after the CR (LF may remain) */
    CHECK(vt_fed(t) < total);
    CHECK(!grid_has(t, "B00"));
    CHECK(!grid_has(t, "garbage"));
    /* the same buffer with no stop: the remainder is drawn (the test can tell) */
    do_stop = 0;
    vt_free(t);
    t = vt_new(80, 25, 100, &cb, &t);
    vt_set_cell_pixels(t, 8, 14);
    vt_set_rip(t, rip);
    vt_feed(t, (const vt_u8 *)buf, total);
    CHECK_INT(vt_fed(t), total);
    CHECK(grid_has(t, "garbage"));
    do_stop = 1;
    vt_free(t);
}

/* ---- arm ---- */
static void arm_case(void)
{
    ripglue g;
    ripglue_io io;
    memset(&io, 0, sizeof io);
    ripglue_init(&g, &io);
    CHECK(ripglue_arm(&g, 0, 7, "x.icn"));
    CHECK_STR(g.name, "x.icn");
    CHECK(ripglue_arm(&g, 0, 5, "y.icn")); /* any protocol id (plan U2) */
    CHECK(!ripglue_arm(&g, 1, 7, "up.icn")); /* an upload is refused */
    CHECK_INT(g.armed, 2);
    CHECK_INT(g.refused, 1);
}

/* ---- the full loop: engine stop, the held remainder, later chunks from a ZMODEM sender ---- */
#define GFILE 6000L
static int g_sink_calls;
static char g_sink_host[64], g_sink_name[64];
static unsigned char g_sink_data[GFILE + 16];
static long g_sink_len, g_chunks;

static int g_sink(const char *host, const char *name, const vt_u8 *data, long len)
{
    g_sink_calls++;
    strncpy(g_sink_host, host, sizeof g_sink_host - 1);
    strncpy(g_sink_name, name, sizeof g_sink_name - 1);
    g_sink_len = len;
    if (len <= (long)sizeof g_sink_data)
        memcpy(g_sink_data, data, (size_t)len);
    return 1;
}

static int sk_getc(void *u, int timeout)
{
    struct pollfd p;
    unsigned char c;
    p.fd = *(int *)u;
    p.events = POLLIN;
    if (poll(&p, 1, timeout) <= 0)
        return ZM_TIMEOUT;
    return read(p.fd, &c, 1) == 1 ? c : ZM_LINE_ERROR;
}

static int sk_write(void *u, const unsigned char *b, long n)
{
    while (n > 0) {
        long w = write(*(int *)u, b, (size_t)n);
        if (w <= 0)
            return 1;
        b += w;
        n -= w;
    }
    return 0;
}

static void *tx_open(void *u, const char *name, long *size, zm_u32 *mtime)
{
    static long pos;
    (void)u;
    (void)name;
    pos = 0;
    *size = GFILE;
    *mtime = 0;
    return &pos;
}
static long tx_read(void *u, void *f, unsigned char *b, long n)
{
    long *pos = (long *)f, i, k = GFILE - *pos < n ? GFILE - *pos : n;
    (void)u;
    for (i = 0; i < k; i++)
        b[i] = (unsigned char)((*pos + i) % 251);
    *pos += k;
    return k;
}
static int tx_seek(void *u, void *f, long p)
{
    (void)u;
    *(long *)f = p;
    return 0;
}
static void tx_close(void *u, void *f, int c)
{
    (void)u;
    (void)f;
    (void)c;
}

/* the chunk supplier: whatever the sender has put on the socket is the next held packet */
static unsigned char chunk_buf[2048];
static int io_next(void *u, int timeout, const unsigned char **p, long *n)
{
    struct pollfd pf;
    long r;
    pf.fd = *(int *)u;
    pf.events = POLLIN;
    if (poll(&pf, 1, timeout) <= 0)
        return 0;
    r = (long)read(pf.fd, chunk_buf, sizeof chunk_buf);
    if (r <= 0)
        return -1;
    g_chunks++;
    *p = chunk_buf;
    *n = r;
    return 1;
}
static int io_send(void *u, const unsigned char *b, long n)
{
    return sk_write(u, b, n);
}

static void glue_loop(int rip, const char *prefix)
{
    int sv[2], st, fd, delivered = 0, rc, i, ok = 1;
    pid_t pid;
    unsigned char buf[2048];
    long cmd_end, total, got = 0, fed;
    vt_callbacks cb;
    vt_term *t = 0;
    ripglue g;
    ripglue_io io;
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv))
        return;
    pid = fork();
    if (pid == 0) {
        zm_line l;
        zm_files f;
        const char *names[1];
        int sfd = sv[1];
        close(sv[0]);
        l.getc = sk_getc;
        l.write = sk_write;
        l.user = &sfd;
        memset(&f, 0, sizeof f);
        f.open_read = tx_open;
        f.read = tx_read;
        f.seek = tx_seek;
        f.close = tx_close;
        names[0] = "BBSNAME.BIN";
        _exit(zm_send(&l, &f, names, 1));
    }
    close(sv[1]);
    fd = sv[0];
    /* the buffer the console holds: the RIP command and the sender's opening bytes */
    strcpy((char *)buf, prefix);
    strcat((char *)buf, "!|9\033" "0" "7" "00" "0000" "MYPICTUR.ICO" "\r\n");
    cmd_end = (long)strlen((char *)buf);
    total = cmd_end;
    {
        struct pollfd pf;
        pf.fd = fd;
        pf.events = POLLIN;
        while (poll(&pf, 1, 500) > 0 && total < (long)sizeof buf) {
            long r = (long)read(fd, buf + total, sizeof buf - (size_t)total);
            if (r <= 0)
                break;
            total += r;
        }
    }
    CHECK(total > cmd_end); /* the sender did speak first */
    memset(&cb, 0, sizeof cb);
    cb.rip_block = on_block;
    t = vt_new(80, 25, 100, &cb, &t);
    vt_set_cell_pixels(t, 8, 14);
    vt_set_rip(t, rip);
    blocks = 0;
    do_stop = 1;
    memset(&io, 0, sizeof io);
    io.next = io_next;
    io.send = io_send;
    io.user = &fd;
    ripglue_init(&g, &io);
    vt_feed(t, buf, total);
    fed = vt_fed(t);
    CHECK_INT(blocks, 1);
    CHECK_INT(vt_rip_blocks(t), 1);
    CHECK(fed < total);
    CHECK(!grid_has(t, "B00"));
    CHECK(ripglue_arm(&g, 0, 7, "MYPICTUR.ICO"));
    ripglue_begin(&g, buf + fed, total - fed);
    g_sink_calls = 0;
    g_chunks = 0;
    rc = ripglue_run(&g, g_sink, "bbs.example.org", &delivered);
    close(fd);
    waitpid(pid, &st, 0);
    CHECK_INT(rc, ZM_OK);
    CHECK_INT(delivered, 1);
    CHECK_INT(g_sink_calls, 1);
    CHECK(g_chunks > 0); /* later chunks were drained too */
    CHECK_STR(g_sink_host, "bbs.example.org");
    CHECK_STR(g_sink_name, "MYPICTUR.ICO");
    CHECK_INT(g_sink_len, GFILE);
    for (i = 0; i < GFILE; i++)
        if (g_sink_data[i] != (unsigned char)(i % 251))
            ok = 0;
    CHECK(ok);
    got = grid_has(t, "B00") || grid_has(t, "**");
    CHECK(!got); /* none of the transfer reached the screen */
    vt_free(t);
}

void suite_ripglue(void)
{
    stop_case(1, "\033[2!");
    stop_case(2, "");
    arm_case();
    glue_loop(1, "\033[2!");
    glue_loop(2, "");
}
