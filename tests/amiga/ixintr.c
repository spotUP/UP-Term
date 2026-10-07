/* ixintr -- the characters typed after a Ctrl-C that ended a read
 * (tools/rig/intr_rig.py; todos: "lost first character after Ctrl-C").
 *
 * Catches SIGINT without SA_RESTART, reads stdin (default line mode, or
 * one byte at a time with "raw": VMIN 1, no ICANON, no ECHO; "die": the
 * default SIGINT action, for a shell to run as a child) and appends
 * each read's result to VTC:intr.out: "read <n> <errno> <hex bytes>". It
 * stops after a read that returned a newline, or after <reads> reads. */
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

static volatile int ints;
static void on_int(int sig) { (void)sig; ints++; }

int main(int argc, char **argv)
{
    struct sigaction sa;
    struct termios t;
    int reads = argc > 1 ? atoi(argv[1]) : 20, raw = argc > 2 && !strcmp(argv[2], "raw");
    int i;
    FILE *f;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_int;
    if (!(argc > 2 && !strcmp(argv[2], "die")))
        sigaction(SIGINT, &sa, 0); /* "die": SIGINT ends the program */
    if (raw && tcgetattr(0, &t) == 0) {
        t.c_lflag &= ~(ICANON | ECHO);
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
        tcsetattr(0, TCSANOW, &t);
    }
    f = fopen("VTC:intr.out", "w");
    if (f) { fprintf(f, "ready\n"); fclose(f); }
    printf("ixintr ready\n");
    fflush(stdout);
    for (i = 0; i < reads; i++) {
        char buf[64];
        int n = (int)read(0, buf, raw ? 1 : sizeof(buf)), e = n < 0 ? errno : 0, j, nl = 0;
        f = fopen("VTC:intr.out", "a");
        if (f) {
            fprintf(f, "read %d %d ints %d", n, e, ints);
            for (j = 0; j < n; j++) {
                fprintf(f, " %02x", (unsigned char)buf[j]);
                if (buf[j] == '\n') nl = 1;
            }
            fprintf(f, "\n");
            fclose(f);
        }
        if (nl) break;
    }
    f = fopen("VTC:intr.out", "a");
    if (f) { fprintf(f, "done\n"); fclose(f); }
    return 0;
}
