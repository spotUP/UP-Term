/* ixwinch -- does a resized XCON: window reach an ixemul program?
 * (tools/rig/winch_rig.py; vtcon ledger W47, ixemul-vtcon plan R7)
 *
 * Catches SIGWINCH the way less does (signal()), then for <secs> seconds
 * keeps VTC:winch.out current with: the size TIOCGWINSZ gave at the start,
 * the number of SIGWINCHs caught, and the size TIOCGWINSZ gives now. The
 * rig drags the window's size gadget and reads the file: the two halves
 * of the chain (signal delivered / size reported) are told apart. */
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

static void report(const struct winsize *a, const struct winsize *b, int n, int done)
{
    FILE *f = fopen("VTC:winch.out", "w");
    if (!f)
        return;
    fprintf(f, "start %dx%d winch %d now %dx%d%s\n", a->ws_col, a->ws_row, n, b->ws_col, b->ws_row,
            done ? " done" : "");
    fclose(f);
}

int main(int argc, char **argv)
{
    struct winsize a, b;
    int secs = argc > 1 ? atoi(argv[1]) : 40;
    int i, seen = -1;

    signal(SIGWINCH, on_winch);
    if (ioctl(0, TIOCGWINSZ, &a) < 0)
        a.ws_col = a.ws_row = 0;
    b = a;
    report(&a, &b, 0, 0);
    printf("ixwinch ready %dx%d\n", a.ws_col, a.ws_row);
    fflush(stdout);
    for (i = 0; i < secs; i++) {
        struct winsize c;
        sleep(1);
        if (ioctl(0, TIOCGWINSZ, &c) < 0)
            c.ws_col = c.ws_row = 0;
        if (winches != seen || c.ws_col != b.ws_col || c.ws_row != b.ws_row) {
            seen = winches;
            b = c;
            report(&a, &b, seen, 0);
        }
    }
    report(&a, &b, winches, 1);
    printf("ixwinch winch %d now %dx%d\n", winches, b.ws_col, b.ws_row);
    return 0;
}
