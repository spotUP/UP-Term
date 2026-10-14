/* sys_posix -- sys.h on POSIX, for the host tests (a temporary tree).
 * run is `sh -c` in its own process group with the timeout (a command past it is killed: SYS_TIMEOUT); background
 * jobs are sh -c in their own process group, output to a temporary file. */
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <poll.h>
#include "sys_posix.h"
#include "util.h"

static void set_err(sys_posix *p, const char *what)
{
    cl_copy(p->err, what, sizeof(p->err));
    cl_cat(p->err, ": ", sizeof(p->err));
    cl_cat(p->err, strerror(errno), sizeof(p->err));
}

static int x_read(void *u, const char *path, long max, char **out, long *n)
{
    sys_posix *p = (sys_posix *)u;
    FILE *f = fopen(path, "rb");
    long len;
    char *b;
    if (!f) {
        set_err(p, path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len > max) {
        fclose(f);
        return SYS_TOO_BIG;
    }
    b = (char *)malloc((size_t)len + 1);
    if (!b || (long)fread(b, 1, (size_t)len, f) != len) {
        free(b);
        fclose(f);
        cl_copy(p->err, "read failed", sizeof(p->err));
        return -1;
    }
    fclose(f);
    b[len] = 0;
    *out = b;
    *n = len;
    return 0;
}

static int x_pread(void *u, const char *path, long off, char *out, long cap, long *n)
{
    sys_posix *p = (sys_posix *)u;
    FILE *f = fopen(path, "rb");
    *n = 0;
    if (!f) {
        set_err(p, path);
        return -1;
    }
    if (fseek(f, off, SEEK_SET)) {
        set_err(p, path);
        fclose(f);
        return -1;
    }
    *n = (long)fread(out, 1, (size_t)cap, f);
    fclose(f);
    return 0;
}

static int x_write(void *u, const char *path, const char *s, long n)
{
    sys_posix *p = (sys_posix *)u;
    FILE *f = fopen(path, "wb");
    if (!f) {
        set_err(p, path);
        return -1;
    }
    if ((long)fwrite(s, 1, (size_t)n, f) != n) {
        fclose(f);
        set_err(p, path);
        return -1;
    }
    return fclose(f) ? -1 : 0;
}

static int x_list(void *u, const char *path, cl_dir_fn fn, void *c)
{
    sys_posix *p = (sys_posix *)u;
    DIR *d = opendir(path);
    struct dirent *e;
    if (!d) {
        set_err(p, path);
        return -1;
    }
    while ((e = readdir(d)) != 0) {
        cl_dirent de;
        struct stat st;
        char full[1024];
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        cl_copy(full, path, sizeof(full));
        cl_cat(full, "/", sizeof(full));
        cl_cat(full, e->d_name, sizeof(full));
        memset(&de, 0, sizeof(de));
        cl_copy(de.name, e->d_name, sizeof(de.name));
        if (!stat(full, &st)) {
            de.dir = S_ISDIR(st.st_mode);
            de.size = (long)st.st_size;
            de.mtime = (long)st.st_mtime;
        }
        if (fn(c, &de))
            break;
    }
    closedir(d);
    return 0;
}

static int x_kind(void *u, const char *path)
{
    struct stat st;
    (void)u;
    if (stat(path, &st))
        return 0;
    return S_ISDIR(st.st_mode) ? 2 : 1;
}

static int x_canon(void *u, const char *path, char *out, long cap)
{
    char buf[PATH_MAX];
    (void)u;
    if (!realpath(path, buf) || (long)strlen(buf) >= cap)
        return -1;
    strcpy(out, buf);
    return 0;
}

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* SIGKILL to a command's process group (its leader `pid`, the caller's child) until no member is left; the leader reaped
 * into *st. One kill(-pid) is not enough: the kernel signals the members the group has at that moment, and a child that one
 * of them is forking right then is not among them yet, so it lives on in the group (on macOS `while :; do (sleep 5); done`
 * kept a live member after a single group SIGKILL 153 times in 200). Each round kills those there, and a killed process
 * forks no more. Bounded at 2 s for a member that will not go (a zombie its new parent has not reaped yet). */
static void kill_group(pid_t pid, int *st)
{
    struct timespec ts;
    int k;
    kill(-pid, SIGKILL);
    waitpid(pid, st, 0);
    ts.tv_sec = 0;
    ts.tv_nsec = 5000000L;
    for (k = 0; k < 400 && kill(-pid, SIGKILL) == 0; k++)
        nanosleep(&ts, 0);
}

/* a command to its end or to its timeout (whole seconds, measured in milliseconds: a 1 s timeout is 1000 ms, not "until
 * the clock's next second"), its output read meanwhile. The timeout holds after the command closes its output too: the
 * wait for its exit is part of it. At the timeout its whole group is killed (kill_group). */
static int x_run(void *u, const char *cmd, int timeout_s, char *out, long cap, long *outn, long *rc)
{
    sys_posix *p = (sys_posix *)u;
    int fd[2], st = 0, timed_out = 0, eof = 0;
    pid_t pid;
    long n = 0;
    long end = timeout_s > 0 ? now_ms() + (long)timeout_s * 1000L : 0;
    if (pipe(fd)) {
        set_err(p, "pipe");
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        close(fd[0]);
        close(fd[1]);
        set_err(p, "fork");
        return -1;
    }
    if (pid == 0) {
        setpgid(0, 0);                  /* its own group: a timeout kills the command and what it started */
        dup2(fd[1], 1);
        dup2(fd[1], 2);
        close(fd[0]);
        close(fd[1]);
        execl("/bin/sh", "sh", "-c", cmd, (char *)0);
        _exit(127);
    }
    setpgid(pid, pid);                  /* in the parent too: the group exists before any kill, however late the child runs */
    close(fd[1]);
    for (;;) {
        struct pollfd pf;
        char buf[1024];
        ssize_t r;
        long wait_ms = 200;
        if (end) {
            long left = end - now_ms();
            if (left <= 0) {
                timed_out = 1;
                break;
            }
            if (left < wait_ms)
                wait_ms = left;
        }
        if (eof) {                      /* output closed: wait for its exit, still within the timeout */
            struct timespec ts;
            if (waitpid(pid, &st, end ? WNOHANG : 0) == pid)
                break;
            ts.tv_sec = 0;
            ts.tv_nsec = (wait_ms < 10 ? wait_ms : 10) * 1000000L;
            nanosleep(&ts, 0);
            continue;
        }
        pf.fd = fd[0];
        pf.events = POLLIN;
        pf.revents = 0;
        if (poll(&pf, 1, (int)wait_ms) <= 0)
            continue;
        r = read(fd[0], buf, sizeof(buf));
        if (r <= 0) {
            eof = 1;
            continue;
        }
        if (n < cap) {
            long take = (long)r < cap - n ? (long)r : cap - n;
            memcpy(out + n, buf, (size_t)take);
            n += take;
        }
    }
    close(fd[0]);
    *outn = n;
    if (timed_out) {
        kill_group(pid, &st);
        *rc = -1;
        return SYS_TIMEOUT;
    }
    *rc = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    return 0;
}

static long x_mtime(void *u, const char *path)
{
    struct stat st;
    (void)u;
    return stat(path, &st) ? -1 : (long)st.st_mtime;
}

/* ---- background jobs ---- */

static int x_bg_start(void *u, const char *cmd, long *job)
{
    sys_posix *p = (sys_posix *)u;
    int i, fd;
    pid_t pid;
    sp_job *j = 0;
    for (i = 0; i < SP_JOBS; i++)
        if (!p->jobs[i].used) {
            j = &p->jobs[i];
            break;
        }
    if (!j) {
        cl_copy(p->err, "too many background jobs", sizeof(p->err));
        return -1;
    }
    memset(j, 0, sizeof(*j));
    strcpy(j->file, "/tmp/claude_bg_XXXXXX");
    {
        const char *base = getenv("TMPDIR");
        if (base && *base && strlen(base) < sizeof(j->file) - 24) {
            strcpy(j->file, base);
            if (j->file[strlen(j->file) - 1] != '/')
                strcat(j->file, "/");
            strcat(j->file, "claude_bg_XXXXXX");
        }
    }
    fd = mkstemp(j->file);
    if (fd < 0) {
        set_err(p, "mkstemp");
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        close(fd);
        unlink(j->file);
        set_err(p, "fork");
        return -1;
    }
    if (!pid) {
        int nul = open("/dev/null", O_RDONLY);
        setpgid(0, 0);
        dup2(nul, 0);
        dup2(fd, 1);
        dup2(fd, 2);
        execl("/bin/sh", "sh", "-c", cmd, (char *)0);
        _exit(127);
    }
    close(fd);
    setpgid(pid, pid);
    j->used = 1;
    j->pid = (long)pid;
    *job = i;
    return 0;
}

static void reap(sp_job *j)
{
    int st;
    if (!j->ended && waitpid((pid_t)j->pid, &st, WNOHANG) == (pid_t)j->pid) {
        j->ended = 1;
        j->rc = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0);
    }
}

static int x_bg_read(void *u, long job, long from, char *out, long cap, long *outn, int *running, long *rc)
{
    sys_posix *p = (sys_posix *)u;
    sp_job *j;
    FILE *f;
    if (job < 0 || job >= SP_JOBS || !p->jobs[job].used) {
        cl_copy(p->err, "no such job", sizeof(p->err));
        return -1;
    }
    j = &p->jobs[job];
    reap(j);                        /* before the read: what it wrote before it ended is all there */
    *running = !j->ended || p->bg_hold;
    *rc = j->rc;
    *outn = 0;
    f = fopen(j->file, "rb");
    if (!f) {
        set_err(p, j->file);
        return -1;
    }
    if (!fseek(f, from, SEEK_SET))
        *outn = (long)fread(out, 1, (size_t)cap, f);
    fclose(f);
    return 0;
}

static int x_bg_kill(void *u, long job)
{
    sys_posix *p = (sys_posix *)u;
    sp_job *j;
    struct timespec ts;
    int k;
    if (job < 0 || job >= SP_JOBS || !p->jobs[job].used)
        return -1;
    j = &p->jobs[job];
    reap(j);
    if (j->ended)
        return 0;
    kill(-(pid_t)j->pid, SIGINT);
    ts.tv_sec = 0;
    ts.tv_nsec = 20000000L;
    for (k = 0; k < 25 && !j->ended; k++) {
        nanosleep(&ts, 0);
        reap(j);
    }
    if (!j->ended) {
        int st;
        kill_group((pid_t)j->pid, &st);
        j->ended = 1;
        j->rc = 137;
    }
    return 0;
}

static void x_bg_drop(void *u, long job)
{
    sys_posix *p = (sys_posix *)u;
    sp_job *j;
    if (job < 0 || job >= SP_JOBS || !p->jobs[job].used)
        return;
    j = &p->jobs[job];
    reap(j);
    if (!j->ended)
        return;
    unlink(j->file);
    j->used = 0;
}

static int x_append(void *u, const char *path, const char *s, long n)
{
    sys_posix *p = (sys_posix *)u;
    FILE *f = fopen(path, "ab");
    if (!f) {
        set_err(p, path);
        return -1;
    }
    if ((long)fwrite(s, 1, (size_t)n, f) != n) {
        fclose(f);
        set_err(p, path);
        return -1;
    }
    return fclose(f) ? -1 : 0;
}

static int x_mkdir(void *u, const char *path)
{
    sys_posix *p = (sys_posix *)u;
    struct stat st;
    if (!stat(path, &st) && S_ISDIR(st.st_mode))
        return 0;
    if (mkdir(path, 0700)) {
        set_err(p, path);
        return -1;
    }
    return 0;
}

static int x_remove(void *u, const char *path)
{
    sys_posix *p = (sys_posix *)u;
    if (remove(path)) {
        set_err(p, path);
        return -1;
    }
    return 0;
}

static long x_getenv(void *u, const char *name, char *out, long cap)
{
    const char *v = getenv(name);
    (void)u;
    if (!v)
        return -1;
    cl_copy(out, v, cap);
    return (long)strlen(out);
}

static int x_setenv(void *u, const char *name, const char *value)
{
    (void)u;
    return setenv(name, value, 1) ? -1 : 0;
}

static int x_clip(void *u, const char *s, long n)
{
    sys_posix *p = (sys_posix *)u;
    if (n > (long)sizeof(p->clip) - 1)
        n = (long)sizeof(p->clip) - 1;
    memcpy(p->clip, s, (size_t)n);
    p->clip[n] = 0;
    p->clipn = n;
    return 0;
}

static int x_info(void *u, const char *what, char *out, long cap)
{
    (void)u;
    if (!strcmp(what, "os")) {
        cl_copy(out, "a POSIX host (the test build)", cap);
        return 1;
    }
    cl_copy(out, "not on this machine", cap);
    return -1;
}

static const char *x_err(void *u)
{
    return ((sys_posix *)u)->err;
}

/* ---- A4 gaps 2 ---- */

/* the local time in seconds since 1978-01-01 (2922 days after 1970); a
 * test may set fake_now */
static long x_now(void *u)
{
    sys_posix *p = (sys_posix *)u;
    time_t t;
    struct tm g;
    long off;
    if (p->fake_now)
        return p->fake_now;
    t = time(0);
    if (!gmtime_r(&t, &g))
        return -1;
    g.tm_isdst = -1;
    off = (long)(t - mktime(&g));   /* the local zone's offset from UTC */
    return (long)t + off - 2922L * 86400L;
}

static int x_pause(void *u, long ms)
{
    sys_posix *p = (sys_posix *)u;
    struct timespec ts;
    if (p->brk) {
        p->brk = 0;
        return 1;
    }
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, 0);
    return 0;
}

static long x_bg_size(void *u, long job)
{
    sys_posix *p = (sys_posix *)u;
    struct stat st;
    if (job < 0 || job >= SP_JOBS || !p->jobs[job].used)
        return -1;
    return stat(p->jobs[job].file, &st) ? 0 : (long)st.st_size;
}

static const char *x_bg_file(void *u, long job)
{
    sys_posix *p = (sys_posix *)u;
    return job >= 0 && job < SP_JOBS && p->jobs[job].used ? p->jobs[job].file : "";
}

static int x_rename(void *u, const char *from, const char *to)
{
    sys_posix *p = (sys_posix *)u;
    if (rename(from, to)) {
        set_err(p, from);
        return -1;
    }
    return 0;
}

/* ---- spawn2: a child on two pipes ---- */
typedef struct sp_child {
    pid_t pid;
    int in, out;                /* our ends: its standard input, its standard output */
} sp_child;

static int x_spawn2(void *u, const char *const *argv, const char *const *env, const char *dir, void **h)
{
    sys_posix *p = (sys_posix *)u;
    int to[2], from[2];
    pid_t pid;
    sp_child *c;
    if (!argv || !argv[0]) {
        cl_copy(p->err, "no program", sizeof(p->err));
        return -1;
    }
    c = (sp_child *)malloc(sizeof(*c));
    if (!c) {
        cl_copy(p->err, "out of memory", sizeof(p->err));
        return -1;
    }
    if (pipe(to)) {
        set_err(p, "pipe");
        free(c);
        return -1;
    }
    if (pipe(from)) {
        set_err(p, "pipe");
        close(to[0]);
        close(to[1]);
        free(c);
        return -1;
    }
    signal(SIGPIPE, SIG_IGN);  /* a write to a child that has gone is -1, not the end of us */
    pid = fork();
    if (pid < 0) {
        set_err(p, "fork");
        close(to[0]);
        close(to[1]);
        close(from[0]);
        close(from[1]);
        free(c);
        return -1;
    }
    if (!pid) {
        int nul = open("/dev/null", O_WRONLY);
        setpgid(0, 0);
        dup2(to[0], 0);
        dup2(from[1], 1);
        dup2(nul, 2);
        close(to[0]);
        close(to[1]);
        close(from[0]);
        close(from[1]);
        if (dir && *dir && chdir(dir))
            _exit(127);
        for (; env && *env; env++)
            putenv((char *)*env);
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    setpgid(pid, pid);
    close(to[0]);
    close(from[1]);
    fcntl(to[1], F_SETFD, FD_CLOEXEC);
    fcntl(from[0], F_SETFD, FD_CLOEXEC);
    c->pid = pid;
    c->in = to[1];
    c->out = from[0];
    *h = c;
    return 0;
}

static int x_spawn2_write(void *u, void *h, const char *s, long n)
{
    sp_child *c = (sp_child *)h;
    (void)u;
    while (n > 0) {
        ssize_t k = write(c->in, s, (size_t)n);
        if (k < 0 && errno == EINTR)
            continue;
        if (k <= 0)
            return -1;
        s += k;
        n -= (long)k;
    }
    return 0;
}

static long x_spawn2_read(void *u, void *h, char *out, long cap, long ms)
{
    sp_child *c = (sp_child *)h;
    struct pollfd pf;
    ssize_t k;
    (void)u;
    pf.fd = c->out;
    pf.events = POLLIN;
    pf.revents = 0;
    k = poll(&pf, 1, (int)ms);
    if (k == 0 || (k < 0 && errno == EINTR))
        return -2;                  /* nothing came, or a signal: the caller looks at its abort key and asks again */
    if (k < 0)
        return -1;
    k = read(c->out, out, (size_t)cap);
    return k < 0 ? -1 : (long)k;
}

static long x_spawn2_close(void *u, void *h, long ms)
{
    sp_child *c = (sp_child *)h;
    int st, sig;
    long t, rc = -1;
    (void)u;
    close(c->in);
    /* the spec's shutdown, as pi's stdio transport: input closed, a grace time, SIGTERM, SIGKILL to its group */
    for (sig = 0; sig < 3; sig++) {
        if (sig == 1)
            kill(-c->pid, SIGTERM);
        else if (sig == 2) {
            kill_group(c->pid, &st);
            break;
        }
        for (t = 0; t <= (sig ? 2000 : ms); t += 10) {
            if (waitpid(c->pid, &st, WNOHANG) == c->pid) {
                rc = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
                sig = 3;
                break;
            }
            usleep(10000);
        }
    }
    close(c->out);
    free(c);
    return rc;
}

void sys_posix_init(sys_posix *p, cl_sys *s)
{
    memset(p, 0, sizeof(*p));
    s->u = p;
    s->read = x_read;
    s->write = x_write;
    s->list = x_list;
    s->kind = x_kind;
    s->canon = x_canon;
    s->run = x_run;
    s->err = x_err;
    s->mtime = x_mtime;
    s->bg_start = x_bg_start;
    s->bg_read = x_bg_read;
    s->bg_kill = x_bg_kill;
    s->bg_drop = x_bg_drop;
    s->append = x_append;
    s->mkdir = x_mkdir;
    s->remove = x_remove;
    s->getenv = x_getenv;
    s->setenv = x_setenv;
    s->clip = x_clip;
    s->info = x_info;
    s->now = x_now;
    s->pause = x_pause;
    s->bg_size = x_bg_size;
    s->bg_file = x_bg_file;
    s->rename = x_rename;
    s->quiet_kind = 0;          /* kind() asks nothing on the host */
    s->pread = x_pread;
    s->spawn2 = x_spawn2;
    s->spawn2_write = x_spawn2_write;
    s->spawn2_read = x_spawn2_read;
    s->spawn2_close = x_spawn2_close;
}
