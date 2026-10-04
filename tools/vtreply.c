/* vtreply COLS ROWS: the engine as a live terminal for a capture
 * (tools/capture_claude.py). Frames on stdin: a 4-byte big-endian length,
 * then that many bytes of program output. After each frame it writes a
 * frame back: the length and the bytes the engine answered (DA1, DSR,
 * OSC 11, DECRQM, ...), so a program that asks what the terminal is gets
 * UP-Term's own answers, not silence. A zero-length frame resizes to the
 * next two 4-byte numbers (cols, rows). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../engine/vtengine.h"

static vt_u8 rbuf[65536];
static long rlen;

static void on_reply(void *user, const vt_u8 *buf, long len)
{
    (void)user;
    if (rlen + len <= (long)sizeof(rbuf)) {
        memcpy(rbuf + rlen, buf, (size_t)len);
        rlen += len;
    }
}

static int get_u32(unsigned long *v)
{
    unsigned char b[4];
    if (fread(b, 1, 4, stdin) != 4)
        return 0;
    *v = ((unsigned long)b[0] << 24) | ((unsigned long)b[1] << 16) | ((unsigned long)b[2] << 8) | b[3];
    return 1;
}

static void put_u32(unsigned long v)
{
    unsigned char b[4];
    b[0] = (unsigned char)(v >> 24);
    b[1] = (unsigned char)(v >> 16);
    b[2] = (unsigned char)(v >> 8);
    b[3] = (unsigned char)v;
    fwrite(b, 1, 4, stdout);
}

int main(int argc, char **argv)
{
    static vt_u8 buf[1 << 20];
    static vt_callbacks cb;
    vt_term *t;
    unsigned long n;
    if (argc < 3)
        return 2;
    cb.reply = on_reply;
    t = vt_new(atoi(argv[1]), atoi(argv[2]), 0, &cb, 0);
    if (!t)
        return 1;
    while (get_u32(&n)) {
        if (n == 0) {
            unsigned long c, r;
            if (!get_u32(&c) || !get_u32(&r))
                break;
            vt_resize(t, (int)c, (int)r);
            continue;
        }
        if (n > sizeof(buf) || fread(buf, 1, n, stdin) != n)
            break;
        rlen = 0;
        vt_write(t, buf, (long)n);
        put_u32((unsigned long)rlen);
        fwrite(rbuf, 1, (size_t)rlen, stdout);
        fflush(stdout);
    }
    vt_free(t);
    return 0;
}
