/* ixsig -- signal keys in a vtcon window reach an ixemul program (P6):
 * ^C SIGINT, ^\ SIGQUIT, ^Z SIGTSTP are caught and reported; 'q' ends. */
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>

static volatile int got[32];

static void on(int s) { got[s]++; }

int main(void)
{
    struct termios t, c;
    char ch;
    signal(SIGINT, on);
    signal(SIGQUIT, on);
    signal(SIGTSTP, on);
    tcgetattr(0, &t);
    c = t;
    c.c_lflag &= ~(ICANON | ECHO);  /* ISIG stays on: the keys are signals */
    c.c_cc[VMIN] = 1;
    c.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &c);
    printf("press ^C ^\\ ^Z, then q\n");
    fflush(stdout);
    while (read(0, &ch, 1) == 1 && ch != 'q')
        ;
    tcsetattr(0, TCSANOW, &t);
    printf("SIGINT %d SIGQUIT %d SIGTSTP %d\n", got[SIGINT], got[SIGQUIT], got[SIGTSTP]);
    return 0;
}
