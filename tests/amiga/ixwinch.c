/* ixwinch -- does a resized XCON: window reach an ixemul program?
 * (tools/rig/winch_rig.py; vtcon ledger W47, ixemul-vtcon plan R7)
 *
 * Catches SIGWINCH the way less does (signal()), then for <secs> seconds
 * keeps VTC:winch.out current with: the size TIOCGWINSZ gave at the start,
 * the number of SIGWINCHs caught, and the size TIOCGWINSZ gives now. The
 * rig drags the window's size gadget and reads the file: the two halves
 * of the chain (signal delivered / size reported) are told apart.
 * With "read" as the second argument it waits in read(0) instead of
 * sleep(): a caught SIGWINCH must end that read with EINTR, as on Unix
 * (less 321 redraws only then). */
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

static volatile int winches;

static void on_winch(int sig)
{
    (void)sig;
    winches++;
}

static int rd = 1, rderr; /* the last read's result and errno (read mode) */

static void report(const struct winsize *a, const struct winsize *b, int n, int done)
{
    FILE *f = fopen("VTC:winch.out", "w");
    if (!f)
        return;
    fprintf(f, "start %dx%d winch %d now %dx%d read %d %d%s\n", a->ws_col, a->ws_row, n, b->ws_col,
            b->ws_row, rd, rderr, done ? " done" : "");
    fclose(f);
}

int main(int argc, char **argv)
{
    struct winsize a, b;
    int secs = argc > 1 ? atoi(argv[1]) : 40;
    int i, seen = -1, rmode = argc > 2 && !strcmp(argv[2], "read");
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_winch; /* no SA_RESTART: the read must end */
    sigaction(SIGWINCH, &sa, 0);
    if (ioctl(0, TIOCGWINSZ, &a) < 0)
        a.ws_col = a.ws_row = 0;
    b = a;
    report(&a, &b, 0, 0);
    printf("ixwinch ready %dx%d\n", a.ws_col, a.ws_row);
    fflush(stdout);
    for (i = 0; i < secs; i++) {
        struct winsize c;
        if (rmode) {
            char ch;
            rd = (int)read(0, &ch, 1);
            rderr = rd < 0 ? errno : 0;
        } else
            sleep(1);
        if (ioctl(0, TIOCGWINSZ, &c) < 0)
            c.ws_col = c.ws_row = 0;
        if (rmode || winches != seen || c.ws_col != b.ws_col || c.ws_row != b.ws_row) {
            seen = winches;
            b = c;
            report(&a, &b, seen, 0);
        }
    }
    report(&a, &b, winches, 1);
    printf("ixwinch winch %d now %dx%d\n", winches, b.ws_col, b.ws_row);
    return 0;
}
