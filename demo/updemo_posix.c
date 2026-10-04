/* updemo in a Unix terminal (the Mac's Terminal, iTerm2, xterm): the same
 * scenes UP-Term shows, for comparing. make demo-host;
 * build/updemo [scene | tour [scene]] */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include "updemo.h"

static void out(void *user, const char *buf, long len)
{
    (void)user;
    while (len > 0) {
        long n = (long)write(1, buf, (size_t)len);
        if (n <= 0)
            break;
        buf += n;
        len -= n;
    }
}

static int key(void *user, int wait_ms)
{
    fd_set r;
    struct timeval tv;
    unsigned char c;
    (void)user;
    FD_ZERO(&r);
    FD_SET(0, &r);
    tv.tv_sec = wait_ms / 1000;
    tv.tv_usec = wait_ms % 1000 * 1000;
    if (select(1, &r, 0, 0, &tv) > 0 && read(0, &c, 1) == 1)
        return c;
    return -1;
}

static long ticks(void *user)
{
    struct timeval tv;
    (void)user;
    gettimeofday(&tv, 0);
    return (long)(tv.tv_sec % 100000) * 50 + tv.tv_usec / 20000;
}

int main(int argc, char **argv)
{
    updemo_io io;
    struct termios was, raw;
    struct winsize ws;
    int rc;
    if (tcgetattr(0, &was) || ioctl(1, TIOCGWINSZ, &ws)) {
        fprintf(stderr, "updemo: not a terminal\n");
        return 20;
    }
    raw = was;
    raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &raw);
    io.write = out;
    io.key = key;
    io.ticks = ticks;
    io.user = 0;
    if (argc > 1 && argv[1][0] == 't')
        rc = updemo_tour(&io, ws.ws_col, ws.ws_row, argc > 2 ? atoi(argv[2]) - 1 : 0, 0);
    else
        rc = updemo_run(&io, ws.ws_col, ws.ws_row, argc > 1 ? atoi(argv[1]) - 1 : 0, 0);
    tcsetattr(0, TCSANOW, &was);
    if (rc)
        fprintf(stderr, "updemo: the window is %dx%d, it needs %dx%d\n", ws.ws_col, ws.ws_row, UPDEMO_MIN_COLS,
                UPDEMO_MIN_ROWS);
    return rc ? 10 : 0;
}
