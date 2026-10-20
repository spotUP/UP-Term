/* ixmouse -- an ixemul program that asks for mouse reports and logs the bytes it reads
 * (tools/rig/splits_rig.py case mouse_report, NS4.2).
 *
 * ixmouse OUTFILE [SECS]: puts the tty in raw mode, enables ?1000 + ?1006 (SGR button reports), then reads
 * stdin byte by byte. After every byte OUTFILE holds all bytes read so far as hex ("1b 5b 3c ..."), so the rig
 * can read it at any time. Ends on the byte 'q', on EOF, or after SECS seconds (default 60, SIGALRM ends the
 * blocking read). Restores the tty and turns the modes off. */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static void on_alarm(int sig)
{
    (void)sig;
}

static void dump(const char *path, const unsigned char *b, int n)
{
    FILE *f = fopen(path, "w");
    int i;
    if (!f)
        return;
    for (i = 0; i < n; i++)
        fprintf(f, "%02x ", b[i]);
    fprintf(f, "\n");
    fclose(f);
}

int main(int argc, char **argv)
{
    struct termios old, raw;
    struct sigaction sa;
    unsigned char buf[4096];
    int n = 0;
    unsigned char ch;
    if (argc < 2)
        return 20;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_alarm; /* no SA_RESTART: the read must end */
    sigaction(SIGALRM, &sa, 0);
    if (tcgetattr(0, &old) < 0)
        return 20;
    raw = old;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_iflag &= ~(ICRNL | IXON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &raw);
    dump(argv[1], buf, 0);
    printf("\033[?1000h\033[?1006h");
    fflush(stdout);
    alarm(argc > 2 ? atoi(argv[2]) : 60);
    while (n < (int)sizeof(buf) && read(0, &ch, 1) == 1 && ch != 'q') {
        buf[n++] = ch;
        dump(argv[1], buf, n);
    }
    printf("\033[?1006l\033[?1000l");
    fflush(stdout);
    tcsetattr(0, TCSANOW, &old);
    return 0;
}
