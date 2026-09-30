/* ixreply -- a terminal's answer to a query, in termios raw mode, is input
 * like a typed key: select() sees it, read() gets it, and afterwards select
 * says nothing is waiting. The XCON: handler put answers in its cooked
 * buffer, which termios reads never take: select stayed "readable" and the
 * next read blocked until a real key -- tmux (which asks for DA at start)
 * showed every key one key late. Run in an XCON: window; the result also
 * goes to RAM:ixreply.log. */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>

static int passed, total;
static FILE *logf_;

static void check(int ok, const char *what)
{
    total++;
    passed += ok != 0;
    fprintf(logf_, "%s %d %s\n", ok ? "ok" : "FAIL", total, what);
}

static int readable(int fd, int usec)
{
    fd_set r;
    struct timeval tv;
    FD_ZERO(&r);
    FD_SET(fd, &r);
    tv.tv_sec = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    return select(fd + 1, &r, 0, 0, &tv) > 0;
}

int main(void)
{
    struct termios old, raw;
    char buf[64];
    int fd = open("/dev/tty", O_RDWR), n = 0, k;

    logf_ = fopen("/RAM/ixreply.log", "w");
    if (!logf_)
        return 20;
    check(fd >= 0, "open /dev/tty");
    tcgetattr(fd, &old);
    raw = old;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    check(tcsetattr(fd, TCSANOW, &raw) == 0, "raw mode");
    write(fd, "\033[c", 3);             /* device attributes */
    check(readable(fd, 2000000), "the answer is readable");
    {
        /* FIONREAD counts the whole answer, and one read takes it: libevent
         * reads what FIONREAD says (1 at a time split the answer) */
        int q = -1, m;
        usleep(200000);
        ioctl(fd, FIONREAD, &q);
        m = read(fd, buf, sizeof buf - 1);
        n = m > 0 ? m : 0;
        check(q > 3 && m == q, "FIONREAD counts the whole answer and one read takes it");
    }
    for (k = 0; k < 10 && readable(fd, 300000); k++) {
        int m = read(fd, buf + n, sizeof buf - 1 - n);
        if (m <= 0)
            break;
        n += m;
    }
    buf[n] = 0;
    check(n > 3 && buf[0] == 033 && buf[1] == '[' && buf[2] == '?', "read gets the DA answer");
    check(!readable(fd, 1000000), "afterwards select says nothing is waiting");
    tcsetattr(fd, TCSANOW, &old);
    fprintf(logf_, "ixreply: passed %d of %d\n", passed, total);
    fclose(logf_);
    return passed == total ? 0 : 10;
}
