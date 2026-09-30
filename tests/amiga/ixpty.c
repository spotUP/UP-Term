/* ixpty -- BSD ptys through the patched ixemul (P6 part 4) on vtcon's
 * PTY:, the way screen and tmux will use them: scan /dev/ptyXY, open the
 * slave, termios, select, window size with SIGWINCH, ^C, hang-up. Every
 * line is "ok" or "FAIL"; each also goes to RAM:ixpty.log as it happens,
 * so a hang shows how far it got. tools/rig/ixpty_rig.py runs it. */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>

static int passed, total;
static volatile int winch, intr;

static void check(int ok, const char *what, const char *seen)
{
    FILE *f;
    char line[300];
    total++;
    if (ok)
        passed++;
    snprintf(line, sizeof(line), "%s %d %s%s%s\n", ok ? "ok" : "FAIL", total, what,
             seen ? ": " : "", seen ? seen : "");
    fputs(line, stdout);
    fflush(stdout);
    if ((f = fopen("/RAM/ixpty.log", "a"))) {
        fputs(line, f);
        fclose(f);
    }
}

static const char *show(const char *b, int n)
{
    static char s[200];
    int i, k = 0;
    for (i = 0; i < n && k < 190; i++) {
        unsigned char c = (unsigned char)b[i];
        if (c < 32) {
            s[k++] = '^';
            s[k++] = (char)(c + 64);
        } else {
            s[k++] = (char)c;
        }
    }
    s[k] = 0;
    return s;
}

static void expect(int fd, const char *want, const char *what)
{
    char buf[128];
    int n = read(fd, buf, sizeof(buf));
    char seen[240];
    snprintf(seen, sizeof(seen), "got %d [%s]", n, show(buf, n > 0 ? n : 0));
    check(n == (int)strlen(want) && !memcmp(buf, want, n), what, n == (int)strlen(want) && !memcmp(buf, want, n) ? 0 : seen);
}

static int readable(int fd, int usec)
{
    fd_set r;
    struct timeval tv;
    FD_ZERO(&r);
    FD_SET(fd, &r);
    tv.tv_sec = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    return select(fd + 1, &r, 0, 0, &tv) > 0 && FD_ISSET(fd, &r);
}

static void drain(int fd)
{
    char buf[128];
    while (readable(fd, 0))
        read(fd, buf, sizeof(buf));
}

static void on_winch(int sig)
{
    (void)sig;
    winch++;
}

static void on_int(int sig)
{
    (void)sig;
    intr++;
}

int main(void)
{
    static const char c1[] = "pqrstu", c2[] = "0123456789abcdef";
    char mname[16], sname[16], seen[80];
    int m = -1, s, busy, i, j, n;
    struct termios t, def;
    struct winsize ws;
    struct timeval t0, t1;
    long ms;

    unlink("/RAM/ixpty.log");
    /* the BSD way: the first master that opens is free */
    for (i = 0; c1[i] && m < 0; i++)
        for (j = 0; c2[j] && m < 0; j++) {
            sprintf(mname, "/dev/pty%c%c", c1[i], c2[j]);
            m = open(mname, O_RDWR);
        }
    check(m >= 0, "a free master among /dev/pty[p-u][0-f]", m >= 0 ? mname : strerror(errno));
    if (m < 0)
        return 20;
    busy = open(mname, O_RDWR);
    check(busy < 0, "the same master again is refused", 0);
    if (busy >= 0)
        close(busy);
    strcpy(sname, mname);
    sname[5] = 't';
    s = open(sname, O_RDWR);
    check(s >= 0, "its slave /dev/ttyXY opens", s >= 0 ? sname : strerror(errno));
    if (s < 0)
        return 20;
    check(isatty(m) && isatty(s), "both are ttys", 0);

    n = tcgetattr(s, &def);
    check(n == 0 && (def.c_lflag & ICANON) && (def.c_lflag & ECHO) && (def.c_lflag & ISIG) &&
          def.c_cc[VERASE] == 127,
          "tcgetattr is the handler's line discipline (ICANON ECHO ISIG, DEL erases)", 0);

    write(m, "hi\r", 3);
    expect(s, "hi\n", "the slave reads a line (ICRNL)");
    expect(m, "hi\r\n", "the master reads the echo");
    write(s, "a\nb", 3);
    expect(m, "a\r\nb", "slave output gets ONLCR");

    /* select for writing only: a DOS handle is always writable, and select
     * must say so at once (it slept to its timeout: tmux's output through
     * libevent never went out) */
    {
        fd_set w;
        struct timeval tv = { 3, 0 }, a, z;
        FD_ZERO(&w);
        FD_SET(s, &w);
        gettimeofday(&a, 0);
        n = select(s + 1, 0, &w, 0, &tv);
        gettimeofday(&z, 0);
        ms = (z.tv_sec - a.tv_sec) * 1000 + (z.tv_usec - a.tv_usec) / 1000;
        sprintf(seen, "%d ready after %ld ms", n, ms);
        check(n == 1 && FD_ISSET(s, &w) && ms < 1000, "select for writing on the slave returns at once", seen);
    }

    /* raw, as a full-screen program sets it */
    t = def;
    t.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
    t.c_iflag &= ~(ICRNL | IXON);
    t.c_oflag &= ~OPOST;
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    check(tcsetattr(s, TCSANOW, &t) == 0, "tcsetattr raw", 0);
    check(!readable(s, 0), "select: the slave has nothing", 0);
    write(m, "\r", 1);
    check(readable(s, 500000), "select: the slave is readable after a key", 0);
    expect(s, "\r", "raw: CR comes through as CR");
    check(!readable(m, 0), "select: no echo for the master", 0);
    write(s, "x\n", 2);
    expect(m, "x\n", "raw: no output processing");
    tcsetattr(s, TCSANOW, &def);

    /* window size: the master sets it, the slave's group hears SIGWINCH */
    signal(SIGWINCH, on_winch);
    n = ioctl(s, TIOCGWINSZ, &ws);
    check(n == 0 && ws.ws_col == 80 && ws.ws_row == 24, "TIOCGWINSZ on a new pair: 80 x 24", 0);
    ws.ws_col = 100;
    ws.ws_row = 30;
    winch = 0;
    check(ioctl(m, TIOCSWINSZ, &ws) == 0, "TIOCSWINSZ on the master", 0);
    usleep(100000);
    sprintf(seen, "%d signals", winch);
    check(winch == 1, "SIGWINCH reaches the process on the slave", winch == 1 ? 0 : seen);
    memset(&ws, 0, sizeof(ws));
    ioctl(s, TIOCGWINSZ, &ws);
    check(ws.ws_col == 100 && ws.ws_row == 30, "the slave sees 100 x 30", 0);
    winch = 0;
    ioctl(m, TIOCSWINSZ, &ws);
    usleep(100000);
    check(winch == 0, "the same size again sends no SIGWINCH", 0);

    /* ^C typed on the master: SIGINT for the process on the slave */
    signal(SIGINT, on_int);
    intr = 0;
    write(m, "\003", 1);
    usleep(100000);
    check(intr == 1, "^C on the master is SIGINT on the slave side", 0);
    drain(m);

    /* a select still out must not hold a close for its 10 s */
    {
        int s2 = open(sname, O_RDWR);
        readable(s2, 10000);
        gettimeofday(&t0, 0);
        close(s2);
        gettimeofday(&t1, 0);
        ms = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_usec - t0.tv_usec) / 1000;
        sprintf(seen, "%ld ms", ms);
        check(ms < 1000, "closing a slave after a select takes no 10 s", seen);
    }

    /* hang-up: the master's close is end of file for the slave */
    close(m);
    n = read(s, seen, sizeof(seen));
    check(n == 0, "after the master closes, the slave reads end of file", 0);
    close(s);
    m = open(mname, O_RDWR);
    check(m >= 0, "the pair is free again", 0);
    if (m >= 0)
        close(m);

    printf("ixpty: passed %d of %d\n", passed, total);
    return passed == total ? 0 : 10;
}
