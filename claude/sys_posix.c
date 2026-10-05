/* sys_posix -- sys.h on POSIX, for the host tests (a temporary tree).
 * run uses popen (no timeout: the tests run short commands); background
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

static int x_run(void *u, const char *cmd, int timeout_s, char *out, long cap, long *outn, long *rc)
{
    sys_posix *p = (sys_posix *)u;
    char line[1100];
    FILE *f;
    long n = 0;
    int st;
    (void)timeout_s;
    cl_copy(line, cmd, 1000);
    cl_cat(line, " 2>&1", sizeof(line));
    f = popen(line, "r");
    if (!f) {
        set_err(p, "popen");
        return -1;
    }
    n = (long)fread(out, 1, (size_t)cap, f);
    st = pclose(f);
    *outn = n;
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
    *running = !j->ended;
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
        kill(-(pid_t)j->pid, SIGKILL);
        waitpid((pid_t)j->pid, &st, 0);
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
}
