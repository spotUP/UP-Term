/* ptyprobe -- the P3 baseline: what ixemul's BSD ptys (on FIFO:) do today.
 * Built with bebbo's gcc against ixemul (tools/rig/ptyprobe.sh); needs
 * fifo.library and a running fifo-handler on the rig. Prints one line per
 * check; research/2026-09-29_unix-tty-layer-and-shell.md A2 predicts the
 * results (open, isatty, raw/cooked, ^C yes; TIOCGWINSZ, SIGWINCH no). */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

int main(void)
{
    int m, s, n;
    char buf[64];
    struct termios t;
    struct winsize ws;
    m = open("/dev/ptyp0", O_RDWR);
    printf("1 master open: %d (errno %d)\n", m, m < 0 ? errno : 0);
    s = open("/dev/ttyp0", O_RDWR);
    printf("2 slave open: %d (errno %d)\n", s, s < 0 ? errno : 0);
    if (m < 0 || s < 0)
        return 10;
    printf("3 isatty: master %d slave %d\n", isatty(m), isatty(s));
    n = tcgetattr(s, &t);
    printf("4 tcgetattr slave: %d lflag 0x%lx\n", n, (unsigned long)t.c_lflag);
    n = ioctl(s, TIOCGWINSZ, &ws);
    printf("5 TIOCGWINSZ slave: %d (errno %d) %d x %d\n", n, n < 0 ? errno : 0,
           n < 0 ? 0 : ws.ws_col, n < 0 ? 0 : ws.ws_row);
    ws.ws_col = 100;
    ws.ws_row = 30;
    n = ioctl(m, TIOCSWINSZ, &ws);
    printf("6 TIOCSWINSZ master: %d (errno %d)\n", n, n < 0 ? errno : 0);
    fflush(stdout);
    write(m, "hello\n", 6);
    n = read(s, buf, sizeof(buf) - 1);
    buf[n > 0 ? n : 0] = 0;
    printf("7 master->slave: %d bytes [%s]\n", n, buf);
    write(s, "back", 4);
    n = read(m, buf, sizeof(buf) - 1);
    buf[n > 0 ? n : 0] = 0;
    printf("8 slave->master: %d bytes [%s]\n", n, buf);
    close(s);
    close(m);
    printf("9 done\n");
    return 0;
}
