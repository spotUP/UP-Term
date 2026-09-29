/* ixkill -- send a Unix signal to ixemul processes:
 *   ixkill [-SIG | -n] pid ...     (default TERM)
 * pid is the process as ixemul numbers it (its address: decimal, or hex
 * with 0x). vsh runs it to suspend (TSTP) and continue (CONT) its jobs:
 * vsh is an Amiga program and cannot send Unix signals itself, and
 * ixemul's kill() checks the pid is a live ixemul process. Exit 0 when
 * every signal went out, 1 when a pid is no ixemul process (a native
 * command cannot be stopped), 2 for a usage error. Built against ixemul
 * (bebbo's gcc -mcrt=ixemul); in the kit as C:ixkill. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <strings.h>

/* ixemul's own startup wants 16 KB at least (its STACKSIZE) */
const char stack_cookie[] __attribute__((used)) = "$STACK: 16384";

static const struct { const char *name; int sig; } names[] = {
    { "HUP", SIGHUP }, { "INT", SIGINT }, { "QUIT", SIGQUIT }, { "KILL", SIGKILL },
    { "TERM", SIGTERM }, { "STOP", SIGSTOP }, { "TSTP", SIGTSTP }, { "CONT", SIGCONT },
    { "TTIN", SIGTTIN }, { "TTOU", SIGTTOU }, { "USR1", SIGUSR1 }, { "USR2", SIGUSR2 },
    { "WINCH", SIGWINCH }, { "ALRM", SIGALRM }, { 0, 0 }
};

int main(int argc, char **argv)
{
    int sig = SIGTERM, i = 1, k, rc = 0;
    if (i < argc && argv[i][0] == '-') {
        const char *s = argv[i] + 1;
        if (!strncasecmp(s, "SIG", 3))
            s += 3;
        if (*s >= '0' && *s <= '9') {
            sig = atoi(s);
        } else {
            for (k = 0; names[k].name && strcasecmp(names[k].name, s); k++)
                ;
            if (!names[k].name) {
                fprintf(stderr, "ixkill: %s: no such signal\n", argv[i]);
                return 2;
            }
            sig = names[k].sig;
        }
        i++;
    }
    if (i >= argc) {
        fprintf(stderr, "usage: ixkill [-SIG | -n] pid ...\n");
        return 2;
    }
    for (; i < argc; i++) {
        long pid = strtol(argv[i], 0, 0);
        if (!pid || kill((pid_t)pid, sig) < 0) {
            fprintf(stderr, "ixkill: %s: no such ixemul process\n", argv[i]);
            rc = 1;
        }
    }
    return rc;
}
