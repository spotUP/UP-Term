/* ixtty -- an ixemul program's terminal calls, for P6 on the rig: with
 * the patched ixemul they reach vtcon's line discipline by packet
 * (tcgetattr/tcsetattr, TIOCGWINSZ); with the original they map to
 * SetMode and DISK_INFO. Prints what it sees. */
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

int main(void)
{
    struct termios t, raw;
    struct winsize ws;
    char c;
    int n = tcgetattr(0, &t);
    printf("1 tcgetattr %d: icanon %d echo %d isig %d vmin %d vtime %d verase %d\n", n,
           (t.c_lflag & ICANON) != 0, (t.c_lflag & ECHO) != 0, (t.c_lflag & ISIG) != 0,
           t.c_cc[VMIN], t.c_cc[VTIME], t.c_cc[VERASE]);
    n = ioctl(1, TIOCGWINSZ, &ws);
    printf("2 TIOCGWINSZ %d: %d x %d\n", n, n ? 0 : ws.ws_col, n ? 0 : ws.ws_row);
    fflush(stdout);
    raw = t;
    cfmakeraw(&raw);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &raw);
    printf("3 raw: press a key\r\n");
    fflush(stdout);
    n = read(0, &c, 1);
    tcsetattr(0, TCSANOW, &t);
    printf("3 got %d byte 0x%02x\n", n, n > 0 ? (unsigned char)c : 0);
    printf("4 line A\nline B (column 0 if OPOST is back on)\n");
    return 0;
}
