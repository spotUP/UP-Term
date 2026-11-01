/* ixsigexit -- the AmigaDOS return code of a program ended by a signal
 * (tools/rig/sigexit_rc_rig.py).
 *
 * ixsigexit term | int   kill(getpid(), SIGTERM / SIGINT): rc must be 128+sig
 * ixsigexit exit3        exit(3): rc 3
 * ixsigexit sleep        sleep(30), for a shell to kill: rc 128+sig
 * ixemul's sig_exit ends the program with W_EXITCODE(0, sig); ix_startup
 * used to return WEXITSTATUS of that, 0. */
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

long __stack = 65536;
const char stackcookie[] = "$STACK:65536\n";

int main(int argc, char **argv)
{
    const char *m = argc > 1 ? argv[1] : "";
    if (!strcmp(m, "term")) { kill(getpid(), SIGTERM); sleep(5); }
    else if (!strcmp(m, "int")) { kill(getpid(), SIGINT); sleep(5); }
    else if (!strcmp(m, "exit3")) exit(3);
    else if (!strcmp(m, "sleep")) sleep(30);
    return 0;
}
