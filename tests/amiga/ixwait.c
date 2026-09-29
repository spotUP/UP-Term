/* ixwait -- prints a numbered dot a second for 20 seconds: a job to stop
 * with ^Z and resume with fg (P6 job control on the rig). */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

/* "ixwait self": stops itself with SIGTSTP after three dots (the default
 * action alone, no key and no shell forwarding involved). "ixwait catch":
 * catches SIGTSTP and prints [TSTP] (does the key's signal arrive?). */
static void tstp(int s)
{
    (void)s;
    write(1, "[TSTP]", 6);
}

int main(int argc, char **argv)
{
    int i;
    {
        /* its pid, for a rig script that signals it (its output may be a file in use) */
        FILE *f = fopen("/RAM/ixwait.pid", "w");
        if (f) {
            fprintf(f, "0x%lx\n", (unsigned long)getpid());
            fclose(f);
        }
    }
    if (argc > 1 && !strcmp(argv[1], "catch"))
        signal(SIGTSTP, tstp);
    for (i = 0; i < 20; i++) {
        printf("%d.", i);
        fflush(stdout);
        if (i == 3 && argc > 1 && !strcmp(argv[1], "self")) {
            printf("[self SIGTSTP]");
            fflush(stdout);
            kill(getpid(), SIGTSTP);
            printf("[resumed]");
            fflush(stdout);
        }
        sleep(1);
    }
    printf("done\n");
    return 0;
}
