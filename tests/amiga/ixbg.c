/* ixbg -- a job for the background-terminal rules (P6), run under tcsh:
 * "ixbg read": waits three seconds, then reads a line and prints it (in the
 * background: SIGTTIN, tcsh reports "Suspended (tty input)"); "ixbg
 * tostop": sets TOSTOP, waits three seconds, then prints (in the background:
 * SIGTTOU, "Suspended (tty output)"). The wait is the time to put it in
 * the background with ^Z and bg: tcsh's "&" needs fork(), which ixemul has
 * not ("No more processes"). */
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

static struct timeval t0;

/* "ixbg nap": catches SIGTSTP and prints when it came, sleeping 5 s:
 * does ^Z reach a program that is asleep? */
static void tstp(int s)
{
    struct timeval t1;
    char b[40];
    (void)s;
    gettimeofday(&t1, 0);
    sprintf(b, "[TSTP at %ld ms]", (long)((t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_usec - t0.tv_usec) / 1000));
    write(1, b, strlen(b));
}

int main(int argc, char **argv)
{
    char line[80];
    /* "ixbg napdfl": the same with SIGTSTP's default action (a stop) */
    if (argc > 1 && (!strcmp(argv[1], "nap") || !strcmp(argv[1], "napdfl"))) {
        gettimeofday(&t0, 0);
        if (!strcmp(argv[1], "nap"))
            signal(SIGTSTP, tstp);
        sleep(5);
        printf("woke\n");
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "tostop")) {
        struct termios t;
        tcgetattr(1, &t);
        t.c_lflag |= TOSTOP;
        tcsetattr(1, TCSANOW, &t);
        sleep(3);
        printf("tostop: printed\n");
        t.c_lflag &= ~TOSTOP;
        tcsetattr(1, TCSANOW, &t);
        return 0;
    }
    sleep(3);
    if (fgets(line, sizeof(line), stdin))
        printf("read: %s", line);
    return 0;
}
