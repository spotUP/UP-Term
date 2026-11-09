/* RIP-7b: level-9 block transfer. One reachability test through vt_write (the engine raises
 * cb.rip_block for `!|9<ESC>`), the received file through zm/zmodem.c into a fake vt_dl_sink;
 * the 8.3 reduction and the size cap where they live (ripxfer.c). */
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
#include "../handler/ripxfer.h"

#define FILE_LEN 10000L

/* ---- what the engine reported ---- */
static int cb_calls, cb_mode, cb_proto, cb_type;
static char cb_name[64];

static void on_block(void *u, int mode, int proto, int type, const char *name)
{
    (void)u;
    cb_calls++;
    cb_mode = mode;
    cb_proto = proto;
    cb_type = type;
    strncpy(cb_name, name, sizeof cb_name - 1);
}

/* ---- the fake sink ---- */
static int sink_calls;
static char sink_host[64], sink_name[64];
static unsigned char sink_data[FILE_LEN + 16];
static long sink_len;

static int fake_sink(const char *host, const char *name, const vt_u8 *data, long len)
{
    sink_calls++;
    strncpy(sink_host, host, sizeof sink_host - 1);
    strncpy(sink_name, name, sizeof sink_name - 1);
    sink_len = len;
    if (len <= (long)sizeof sink_data)
        memcpy(sink_data, data, (size_t)len);
    return 1;
}

/* ---- the line: a socket ---- */
static int fd_getc(void *u, int timeout)
{
    struct pollfd p;
    unsigned char c;
    p.fd = *(int *)u;
    p.events = POLLIN;
    if (poll(&p, 1, timeout) <= 0)
        return ZM_TIMEOUT;
    return read(p.fd, &c, 1) == 1 ? c : ZM_LINE_ERROR;
}

static int fd_write(void *u, const unsigned char *b, long n)
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

/* ---- the sender's file: FILE_LEN bytes of i % 251 ---- */
static long tx_size = FILE_LEN;
static void *tx_open(void *u, const char *name, long *size, zm_u32 *mtime)
{
    static long pos;
    (void)u;
    (void)name;
    pos = 0;
    *size = tx_size;
    *mtime = 0;
    return &pos;
}
static long tx_read(void *u, void *f, unsigned char *b, long n)
{
    long *pos = (long *)f, i, k = tx_size - *pos < n ? tx_size - *pos : n;
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

/* fork a ZMODEM sender of the file called zname; returns our end of the line */
static int start_sender(const char *zname, pid_t *pid)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv))
        return -1;
    *pid = fork();
    if (*pid == 0) {
        zm_line l;
        zm_files f;
        const char *names[1];
        int fd = sv[1];
        close(sv[0]);
        l.getc = fd_getc;
        l.write = fd_write;
        l.user = &fd;
        memset(&f, 0, sizeof f);
        f.open_read = tx_open;
        f.read = tx_read;
        f.seek = tx_seek;
        f.close = tx_close;
        names[0] = zname;
        _exit(zm_send(&l, &f, names, 1));
    }
    close(sv[1]);
    return sv[0];
}

static int receive(const char *zname, const char *rip_name, int *delivered)
{
    pid_t pid;
    int st, fd = start_sender(zname, &pid), rc;
    zm_line l;
    l.getc = fd_getc;
    l.write = fd_write;
    l.user = &fd;
    rc = ripxfer_receive(&l, fake_sink, "bbs.example.org", rip_name, delivered);
    close(fd);
    waitpid(pid, &st, 0);
    return rc;
}

static void reset(void)
{
    sink_calls = 0;
    sink_len = 0;
    sink_name[0] = sink_host[0] = 0;
}

static void reachability(void)
{
    vt_callbacks cb;
    vt_term *t;
    int got = 0, rc, i, ok = 1;
    const char *seq = "\033[!" "!|9\033" "0" "5" "00" "0000" "C:BBS/My Picture.Icon" "\r\n";
    memset(&cb, 0, sizeof cb);
    cb.rip_block = on_block;
    t = vt_new(80, 25, 100, &cb, 0);
    vt_set_cell_pixels(t, 8, 14);
    vt_set_rip(t, 1);
    reset();
    vt_write(t, (const vt_u8 *)seq, (long)strlen(seq));
    CHECK_INT(cb_calls, 1); /* the callback ran */
    CHECK_INT(vt_rip_blocks(t), 1);
    CHECK_INT(cb_mode, 0);
    CHECK_INT(cb_proto, 5);
    CHECK_INT(cb_type, 0);
    CHECK_STR(cb_name, "My Picture.Icon"); /* the path is gone */
    /* the host now runs the transfer with the name the engine gave */
    rc = receive("whatever.bin", cb_name, &got);
    CHECK_INT(rc, ZM_OK);
    CHECK_INT(got, 1);
    CHECK_INT(sink_calls, 1);
    CHECK_STR(sink_host, "bbs.example.org");
    CHECK_STR(sink_name, "My_Pictu.Ico");
    CHECK_INT(sink_len, FILE_LEN);
    for (i = 0; i < FILE_LEN; i++)
        if (sink_data[i] != (unsigned char)(i % 251))
            ok = 0;
    CHECK(ok);
    vt_free(t);
}

static void name_and_size(void)
{
    char o[RIPXFER_NAME + 4];
    int got = 0;
    CHECK(ripxfer_name83("../../etc/passwd.conf.bak", 25, o));
    CHECK_STR(o, "passwd_c.bak"); /* the last dot splits; the stem's own dot is not legal */
    CHECK(ripxfer_name83("A:B\\lower.TXT", 13, o));
    CHECK_STR(o, "lower.TXT");
    CHECK(!ripxfer_name83("..", 2, o));
    CHECK(!ripxfer_name83(".profile", 8, o));
    CHECK(!ripxfer_name83("", 0, o));
    /* no RIP name: the sender's ZMODEM name, reduced the same way */
    reset();
    CHECK_INT(receive("../x/Readme.markdown", "", &got), ZM_OK);
    CHECK_STR(sink_name, "Readme.mar");
    /* over the cap: skipped, nothing reaches the sink */
    reset();
    tx_size = RIPXFER_MAX + 1;
    receive("big.bin", "", &got);
    CHECK_INT(got, 0);
    CHECK_INT(sink_calls, 0);
    tx_size = FILE_LEN;
}

void suite_ripxfer(void)
{
    reachability();
    name_and_size();
}
