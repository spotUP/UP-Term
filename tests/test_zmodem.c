/* zm/zmodem: the checksums, our sender against our receiver, and both
 * against the host's lrzsz (lsz / lrz, when installed), over a socketpair;
 * one run with a byte corrupted on the way must still deliver the file
 * whole (ZRPOS from the receiver's last good byte). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "harness.h"
#include "../zm/zmodem.h"

/* a, b and c joined into d (no sprintf: the host's libc deprecates it) */
static char *join(char *d, const char *a, const char *b, const char *c)
{
    strcpy(d, a);
    strcat(d, b);
    strcat(d, c);
    return d;
}

/* ---- the line over a file descriptor ---------------------------------------------- */

typedef struct fdline {
    int fd;
    long sent;               /* bytes written so far */
    long corrupt_at;         /* flip the byte written at this offset (-1: none) */
} fdline;

static int fd_getc(void *u, int timeout)
{
    fdline *l = (fdline *)u;
    struct pollfd p;
    unsigned char c;
    p.fd = l->fd;
    p.events = POLLIN;
    if (poll(&p, 1, timeout) <= 0)
        return ZM_TIMEOUT;
    if (read(l->fd, &c, 1) != 1)
        return ZM_LINE_ERROR;
    return c;
}

static int fd_write(void *u, const unsigned char *b, long n)
{
    fdline *l = (fdline *)u;
    unsigned char t[4096];
    long done = 0;
    while (done < n) {
        long k = n - done > (long)sizeof(t) ? (long)sizeof(t) : n - done, w;
        memcpy(t, b + done, k);
        if (l->corrupt_at >= l->sent && l->corrupt_at < l->sent + k)
            t[l->corrupt_at - l->sent] ^= 0x55;
        w = write(l->fd, t, k);
        if (w <= 0)
            return 1;
        done += w;
        l->sent += w;
    }
    return 0;
}

/* ---- files in a directory --------------------------------------------------------- */

static char dir_in[256], dir_out[256];

static void *f_open_read(void *u, const char *name, long *size, zm_u32 *mtime)
{
    char p[512];
    FILE *f;
    (void)u;
    join(p, dir_in, "/", name);
    f = fopen(p, "rb");
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *mtime = 0x5F000000UL;
    return f;
}

static long f_read(void *u, void *f, unsigned char *b, long n)
{
    (void)u;
    return (long)fread(b, 1, (size_t)n, (FILE *)f);
}

static int f_seek(void *u, void *f, long pos)
{
    (void)u;
    return fseek((FILE *)f, pos, SEEK_SET);
}

static void *f_open_write(void *u, const char *name, long size, zm_u32 mtime)
{
    char p[512];
    (void)u;
    (void)size;
    (void)mtime;
    if (strchr(name, '/'))
        return 0;
    join(p, dir_out, "/", name);
    return fopen(p, "wb");
}

static long f_write(void *u, void *f, const unsigned char *b, long n)
{
    (void)u;
    return (long)fwrite(b, 1, (size_t)n, (FILE *)f);
}

static void f_close(void *u, void *f, int complete)
{
    (void)u;
    (void)complete;
    fclose((FILE *)f);
}

static const zm_files FILES = { f_open_read, f_read, f_seek, f_open_write, f_write, f_close, 0 };

/* ---- fixtures -------------------------------------------------------------------- */

static const char *const NAMES[] = { "hello.txt", "empty.bin", "all-bytes.bin" };

static void make_files(void)
{
    char p[512];
    FILE *f;
    long i;
    join(p, dir_in, "/hello.txt", "");
    f = fopen(p, "wb");
    fputs("Hello from UP-Term's zmodem\n", f);
    fclose(f);
    join(p, dir_in, "/empty.bin", "");
    f = fopen(p, "wb");
    fclose(f);
    /* every byte value, ZDLE, XON and XOFF among them, 70 KB: several
     * subpackets and a file larger than the 64 KB a 16-bit length holds */
    join(p, dir_in, "/all-bytes.bin", "");
    f = fopen(p, "wb");
    for (i = 0; i < 70000L; i++)
        fputc((int)((i * 7 + i / 256) & 0xFF), f);
    fclose(f);
}

static int same_file(const char *name)
{
    char a[512], b[512];
    FILE *fa, *fb;
    int ca, cb, same = 1;
    join(a, dir_in, "/", name);
    join(b, dir_out, "/", name);
    fa = fopen(a, "rb");
    fb = fopen(b, "rb");
    if (!fa || !fb) {
        if (fa)
            fclose(fa);
        if (fb)
            fclose(fb);
        return 0;
    }
    do {
        ca = fgetc(fa);
        cb = fgetc(fb);
        if (ca != cb)
            same = 0;
    } while (same && ca != EOF);
    fclose(fa);
    fclose(fb);
    return same;
}

static void clear_out(void)
{
    char p[600];
    join(p, "rm -f ", dir_out, "/*");
    if (system(p)) {
        /* nothing to remove */
    }
}

static int have(const char *prog)
{
    char p[300];
    join(p, "command -v ", prog, " >/dev/null 2>&1");
    return system(p) == 0;
}

/* ---- the runs -------------------------------------------------------------------- */

/* ours to ours over a socketpair; the receiver in a child process */
static void ours_to_ours(long corrupt_at)
{
    int sv[2], status = 0, got = 0, rc;
    pid_t pid;
    fdline tx, rx;
    clear_out();
    CHECK_INT(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    pid = fork();
    if (pid == 0) {
        zm_line l;
        close(sv[0]);
        rx.fd = sv[1];
        rx.sent = 0;
        rx.corrupt_at = -1;
        l.getc = fd_getc;
        l.write = fd_write;
        l.user = &rx;
        rc = zm_receive(&l, &FILES, &got);
        _exit(rc == ZM_OK && got == 3 ? 0 : 10 + rc);
    }
    close(sv[1]);
    {
        zm_line l;
        tx.fd = sv[0];
        tx.sent = 0;
        tx.corrupt_at = corrupt_at;
        l.getc = fd_getc;
        l.write = fd_write;
        l.user = &tx;
        rc = zm_send(&l, &FILES, NAMES, 3);
    }
    close(sv[0]);
    waitpid(pid, &status, 0);
    CHECK_INT(rc, ZM_OK);
    CHECK_INT(WIFEXITED(status) ? WEXITSTATUS(status) : -1, 0);
    CHECK(same_file("hello.txt"));
    CHECK(same_file("empty.bin"));
    CHECK(same_file("all-bytes.bin"));
}

/* a host program on the other end of the socketpair, in dir_out */
static pid_t spawn(const char *cmd, int fd, const char *cwd)
{
    pid_t pid = fork();
    if (pid == 0) {
        dup2(fd, 0);
        dup2(fd, 1);
        if (chdir(cwd)) {
            _exit(99);
        }
        execl("/bin/sh", "sh", "-c", cmd, (char *)0);
        _exit(98);
    }
    return pid;
}

static void ours_to_lrz(void)
{
    int sv[2], status = 0, rc;
    pid_t pid;
    fdline tx;
    zm_line l;
    clear_out();
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    pid = spawn("lrz -y -q 2>/dev/null", sv[1], dir_out);
    close(sv[1]);
    tx.fd = sv[0];
    tx.sent = 0;
    tx.corrupt_at = -1;
    l.getc = fd_getc;
    l.write = fd_write;
    l.user = &tx;
    rc = zm_send(&l, &FILES, NAMES, 3);
    close(sv[0]);
    waitpid(pid, &status, 0);
    CHECK_INT(rc, ZM_OK);
    CHECK(same_file("hello.txt"));
    CHECK(same_file("empty.bin"));
    CHECK(same_file("all-bytes.bin"));
}

static void lsz_to_ours(void)
{
    int sv[2], status = 0, rc, got = 0;
    pid_t pid;
    fdline rx;
    zm_line l;
    char cmd[900];
    clear_out();
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    strcpy(cmd, "lsz -q hello.txt empty.bin all-bytes.bin 2>/dev/null");
    pid = spawn(cmd, sv[1], dir_in);
    close(sv[1]);
    rx.fd = sv[0];
    rx.sent = 0;
    rx.corrupt_at = -1;
    l.getc = fd_getc;
    l.write = fd_write;
    l.user = &rx;
    rc = zm_receive(&l, &FILES, &got);
    close(sv[0]);
    waitpid(pid, &status, 0);
    CHECK_INT(rc, ZM_OK);
    CHECK_INT(got, 3);
    CHECK(same_file("hello.txt"));
    CHECK(same_file("empty.bin"));
    CHECK(same_file("all-bytes.bin"));
}

static void checksums(void)
{
    const unsigned char *v = (const unsigned char *)"123456789";
    CHECK_INT((long)zm_crc16(0, v, 9), 0x31C3);                                  /* CRC-16/XMODEM */
    CHECK_INT((long)(~zm_crc32(0xFFFFFFFFUL, v, 9) & 0xFFFFFFFFUL), (long)0xCBF43926UL); /* CRC-32 */
}

void suite_zmodem(void)
{
    char t[] = "/tmp/vtcon-zm-XXXXXX";
    char *d = mkdtemp(t);
    signal(SIGPIPE, SIG_IGN);
    checksums();
    if (!d) {
        CHECK(0);
        return;
    }
    join(dir_in, d, "/in", "");
    join(dir_out, d, "/out", "");
    mkdir(dir_in, 0755);
    mkdir(dir_out, 0755);
    make_files();
    ours_to_ours(-1);
    ours_to_ours(30000); /* a byte flipped in the data: ZRPOS, then the rest */
    if (have("lrz") && have("lsz")) {
        ours_to_lrz();
        lsz_to_ours();
    } else {
        printf("  (lrzsz not installed: the runs against lsz / lrz skipped)\n");
    }
    {
        char p[600];
        join(p, "rm -rf ", d, "");
        if (system(p)) {
            /* left behind in /tmp */
        }
    }
}
