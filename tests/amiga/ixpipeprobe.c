/* ixpipeprobe -- a pipe from an ixemul program into a child started with
 * vfork + exec, the way GNU screen's printcmd does it (P7.1 (e)). The
 * parent writes one line, closes its end and waits up to 10 s for the
 * child. Cases:
 *   A  VTC:forkprobe read                    (ixemul child: no IXPIPE:)
 *   B  VTC:vsh -c "VTC:forkprobe read"       (native vsh on IXPIPE:, ixemul grandchild)
 *   C  VTC:vsh -c "read l; echo got=$l >>RAM:ixpipeprobe.log"   (vsh itself reads)
 * Each step is appended to RAM:ixpipeprobe.log as it happens, so a hang
 * shows where it stopped. */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

static void plog(const char *what, long v)
{
    FILE *f = fopen("/RAM/ixpipeprobe.log", "a");
    struct timeval tv;
    gettimeofday(&tv, 0);
    if (f) {
        fprintf(f, "%ld.%03ld %s %ld\n", (long)tv.tv_sec % 1000, (long)tv.tv_usec / 1000, what, v);
        fclose(f);
    }
}

static int run(const char *name, char *const argv[])
{
    int pi[2], pid, st = -1, i;
    plog(name, 0);
    if (pipe(pi)) {
        plog("pipe failed", errno);
        return 1;
    }
    pid = vfork();
    if (pid == 0) {
        close(0);
        dup(pi[0]);
        close(pi[0]);
        close(pi[1]);
        execv(argv[0], argv);
        _exit(127);
    }
    close(pi[0]);
    plog("vfork returned", pid);
    if (write(pi[1], "printed\n", 8) != 8)
        plog("write failed", errno);
    plog("written", 0);
    close(pi[1]);
    plog("closed", 0);
    for (i = 0; i < 100; i++) {
        int r = waitpid(pid, &st, WNOHANG);
        if (r == pid)
            break;
        usleep(100000);
    }
    plog(i < 100 ? "child done, status" : "child still running after 10 s", st);
    return i < 100 ? 0 : 1;
}

int main(int argc, char **argv)
{
    static char *a[] = {"VTC:forkprobe", "read", 0};
    static char *b[] = {"VTC:vsh", "-c", "VTC:forkprobe read", 0};
    static char *c[] = {"VTC:vsh", "-c", "read l; echo got=$l >>RAM:ixpipeprobe.log", 0};
    const char *which = argc > 1 ? argv[1] : "ABC";
    int rc = 0;
    if (strchr(which, 'A'))
        rc |= run("case A", a);
    if (strchr(which, 'B'))
        rc |= run("case B", b);
    if (strchr(which, 'C'))
        rc |= run("case C", c);
    return rc ? 10 : 0;
}
