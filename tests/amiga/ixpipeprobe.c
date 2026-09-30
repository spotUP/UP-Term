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
#include <stdlib.h>
#include <sys/socket.h>
#include <fcntl.h>
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

/* D: a socketpair as the child's stdin and stdout (tmux's run-shell and
 * #() jobs): the parent reads what vsh -c prints, then end of file, and
 * the child's exit */
static int tmuxlike;  /* E: the steps tmux's job start adds (see main) */

static int run_sock(void)
{
    static char *d[] = {"/bin/sh", "-c", "VTC:forkprobe job-output", 0};
    sigset_t all, old;
    int sv[2], pid, st = -1, i, n, got = 0, eof = 0;
    char buf[128];
    plog("case D", 0);
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) {
        plog("socketpair failed", errno);
        return 1;
    }
    if (tmuxlike & 1) {
        sigfillset(&all);
        sigprocmask(SIG_BLOCK, &all, &old);
    }
    pid = vfork();
    if (pid == 0) {
        int sig, fd;
        if (tmuxlike & 2)
            for (sig = 1; sig < NSIG; sig++)
                signal(sig, SIG_DFL);
        dup2(sv[1], 0);
        dup2(sv[1], 1);
        if (tmuxlike & 4) {
            fd = open("/dev/null", O_RDWR);
            dup2(fd, 2);
            close(fd);
        }
        close(sv[0]);
        if (tmuxlike & 8)
            for (fd = 3; fd < 64; fd++)
                close(fd);
        else
            close(sv[1]);
        if (tmuxlike & 1)
            sigprocmask(SIG_SETMASK, &old, NULL);
        execv(d[0], d);
        _exit(127);
    }
    if (tmuxlike & 1)
        sigprocmask(SIG_SETMASK, &old, NULL);
    close(sv[1]);
    plog("vfork returned", pid);
    if (tmuxlike & 16) {
        /* straight into select, as libevent does */
        fd_set r;
        struct timeval tv = { 3, 0 };
        FD_ZERO(&r);
        FD_SET(sv[0], &r);
        plog("select returned", select(sv[0] + 1, &r, 0, 0, &tv));
    }
    fcntl(sv[0], F_SETFL, O_NONBLOCK);
    for (i = 0; i < 100 && !eof; i++) {
        n = read(sv[0], buf + got, sizeof buf - 1 - got);
        if (n > 0)
            got += n;
        else if (n == 0)
            eof = 1;
        else
            usleep(100000);
    }
    buf[got] = 0;
    plog(eof ? "end of file after bytes" : "no end of file, bytes", got);
    {
        FILE *f = fopen("/RAM/ixpipeprobe.log", "a");
        if (f) { fprintf(f, "output [%s]\n", buf); fclose(f); }
    }
    for (i = 0; i < 50; i++) {
        if (waitpid(pid, &st, WNOHANG) == pid)
            break;
        usleep(100000);
    }
    plog(i < 50 ? "child done, status" : "child still running after 5 s", st);
    return !(eof && strstr(buf, "forkprobe") && i < 50);
}

/* F: exec of a program on a volume that is not there (tmux's /gg/bin/sh
 * without GG:): the child must fail at once (errno, _exit), not hang */
static int run_missing(void)
{
    static char *f[] = {"/nosuchvol/sh", "-c", "echo x", 0};
    int pid, st = -1, i;
    plog("case F", 0);
    pid = vfork();
    if (pid == 0) {
        if (getenv("PROBE_CHDIR"))
            chdir(getenv("PROBE_CHDIR"));   /* as tmux's job child does */
        execv(f[0], f);
        _exit(127);
    }
    plog("vfork returned", pid);
    for (i = 0; i < 50; i++) {
        if (waitpid(pid, &st, WNOHANG) == pid)
            break;
        usleep(100000);
    }
    plog(i < 50 ? "child done, status" : "child still running after 5 s", st);
    return i >= 50;
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
    if (strchr(which, 'D'))
        rc |= run_sock();
    if (strchr(which, 'F'))
        rc |= run_missing();
    if (strchr(which, 'E')) {
        /* argv[2]: which of tmux's steps, bits: 1 signals blocked across
         * vfork, 2 SIG_DFL all, 4 stderr /dev/null, 8 closefrom, 16 parent
         * selects at once */
        tmuxlike = argc > 2 ? atoi(argv[2]) : 31;
        plog("case E steps", tmuxlike);
        rc |= run_sock();
    }
    return rc ? 10 : 0;
}
