/* vsh on the host: the real shell core (sh_parse, sh_expand, sh_exec) behind
 * a POSIX sh_os, so tools/bashdiff.py can run a probe under bash and under vsh
 * and compare. The Amiga's OS layer is shell/vsh.c; this one mirrors every
 * callback it sets (vsh.c vsh_main) with fork/exec/pipe/stat/readdir.
 *
 *   vsh_host [--hits] -c COMMAND [NAME [ARG ...]]
 *   vsh_host [--hits] FILE [ARG ...]
 *
 * (what vsh.c accepts today; plan V2.) External commands get an environment
 * built from vsh's exported variables, never the host's. --hits prints the
 * SH_HITS reachability counters to stderr at the end. Streams: an sh_fh is
 * the file descriptor plus one (SH_NOFH is 0).
 * "T:" names (here-document and substitution temp files) map to $TMPDIR. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/resource.h>
#include <signal.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <poll.h>
#include <spawn.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "../shell/sh_exec.h"
#include "../shell/sh_hits.h"

#ifdef SH_HITS
unsigned long sh_hits[SH_HIT_COUNT];
#endif

typedef struct hproc {
    sh_shell *sh;
} hproc;

#define FD(fh) ((int)((fh)-1))
#define FH(fd) ((sh_fh)((fd) + 1))

/* background jobs: a job id is the pid; the status is kept once reaped */
#define MAXJOB 256
static struct {
    pid_t pid;
    int reaped;
    long status;
} jobs[MAXJOB];

static long decode(int st)
{
    if (WIFEXITED(st))
        return WEXITSTATUS(st);
    if (WIFSIGNALED(st))
        return 128 + WTERMSIG(st);
    return 1;
}

static void cloexec(int fd)
{
    fcntl(fd, F_SETFD, FD_CLOEXEC);
}

static void host_path(const char *path, char *out, size_t max)
{
    if (path[0] == 'T' && path[1] == ':') {
        const char *t = getenv("TMPDIR");
        snprintf(out, max, "%s/%s", t && *t ? t : "/tmp", path + 2);
    } else if (!strcmp(path, "NIL:"))
        snprintf(out, max, "/dev/null");
    else if (path[0] != '/' && strchr(path, ':') && strchr(path, ':') != path) {
        /* sh_path_next turns a Unix absolute $PATH entry /vol/rest into vol:rest */
        const char *c = strchr(path, ':');
        snprintf(out, max, "/%.*s/%s", (int)(c - path), path, c + 1);
    } else
        snprintf(out, max, "%s", path);
}

static sh_fh h_open(void *os, const char *path, int mode)
{
    char p[1024];
    int fd, flags;
    (void)os;
    host_path(path, p, sizeof(p));
    if (mode == SH_OPEN_READ)
        flags = O_RDONLY;
    else if (mode == SH_OPEN_APPEND)
        flags = O_WRONLY | O_CREAT | O_APPEND;
    else if (mode == SH_OPEN_RDWR)
        flags = O_RDWR | O_CREAT;
    else
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    fd = open(p, flags, 0666);
    if (fd < 0)
        return SH_NOFH;
    cloexec(fd);
    return FH(fd);
}

static int h_remove(void *os, const char *path)
{
    char p[1024];
    (void)os;
    host_path(path, p, sizeof(p));
    return unlink(p);
}

static const char *h_tmpdir(void *os)
{
    static char d[600];
    const char *t = getenv("TMPDIR");
    (void)os;
    snprintf(d, sizeof(d), "%s/", t && *t ? t : "/tmp");
    return d;
}

static void h_close(void *os, sh_fh fh)
{
    (void)os;
    if (fh)
        close(FD(fh));
}

static int h_pipe(void *os, sh_fh *rd, sh_fh *wr)
{
    int fds[2];
    (void)os;
    if (pipe(fds))
        return -1;
    cloexec(fds[0]);
    cloexec(fds[1]);
    *rd = FH(fds[0]);
    *wr = FH(fds[1]);
    return 0;
}

static void close_owned(const sh_io *io)
{
    if ((io->owned & SH_OWN_IN) && io->in)
        close(FD(io->in));
    if ((io->owned & SH_OWN_OUT) && io->out)
        close(FD(io->out));
    if ((io->owned & SH_OWN_ERR) && io->err && io->err != io->out)
        close(FD(io->err));
}

/* the environment of vsh's exported variables */
static char **build_envp(const sh_shell *sh)
{
    const sh_var *v;
    sh_var_iter it;
    char **envp;
    int n = 0, i = 0;
    for (v = sh_var_first(&sh->ctx, &it); v; v = sh_var_next(&sh->ctx, &it))
        if ((v->attr & SH_ATTR_EXPORT) && !v->arr && sh_var_str(v))
            n++;
    envp = (char **)calloc((size_t)n + 1, sizeof(char *));
    if (!envp)
        return 0;
    for (v = sh_var_first(&sh->ctx, &it); v; v = sh_var_next(&sh->ctx, &it))
        if ((v->attr & SH_ATTR_EXPORT) && !v->arr && sh_var_str(v)) {
            size_t len = strlen(v->name) + strlen(sh_var_str(v)) + 2;
            envp[i] = (char *)malloc(len);
            if (envp[i]) {
                snprintf(envp[i], len, "%s=%s", v->name, sh_var_str(v));
                i++;
            }
        }
    envp[i] = 0;
    return envp;
}

/* this program's own path (realpath of argv[0]): $BASH ("bash") names the shell itself, as the vshrc
 * alias does on the Amiga, so a probe that starts "$BASH -c ..." runs vsh and not the host's bash */
static char self_exe[1024];

/* argv[0] by vsh's $PATH (a name with a slash is taken as it is) */
static int resolve(const sh_shell *sh, const char *name, char *out, size_t max)
{
    const char *path = sh_get(&sh->ctx, "PATH");
    if (self_exe[0] && (!strcmp(name, "bash") || (strlen(name) > 5 && !strcmp(name + strlen(name) - 5, "/bash")) ||
                         (strlen(name) > 5 && !strcmp(name + strlen(name) - 5, ":bash")))) {
        /* also what the path hash gives for it: bin:bash */
        snprintf(out, max, "%s", self_exe);
        return 0;
    }
    if (strchr(name, ':') && strchr(name, ':') != name) {
        /* a hashed command: the shell's vol:rest form of an absolute path */
        host_path(name, out, max);
        return access(out, X_OK) == 0 ? 0 : -1;
    }
    if (strchr(name, '/')) {
        snprintf(out, max, "%s", name);
        return access(out, X_OK) == 0 ? 0 : -1;
    }
    while (path && *path) {
        size_t n = strcspn(path, ":");
        char dir[512];
        struct stat st;
        snprintf(dir, sizeof(dir), "%.*s", (int)n, path);
        snprintf(out, max, "%s/%s", n ? dir : ".", name);
        if (access(out, X_OK) == 0 && stat(out, &st) == 0 && S_ISREG(st.st_mode))
            return 0;
        path += n;
        if (*path == ':')
            path++;
    }
    return -1;
}

static long reap(pid_t pid, int wait)
{
    int st = 0;
    pid_t r;
    do
        r = waitpid(pid, &st, wait ? 0 : WNOHANG);
    while (r < 0 && errno == EINTR);
    return r == pid ? decode(st) : -2;
}

static long h_run(void *os, char **argv, const sh_io *io, int wait)
{
    sh_shell *sh = ((hproc *)os)->sh;
    char exe[1024];
    char **envp;
    pid_t pid;
    int i;
    if (resolve(sh, argv[0], exe, sizeof(exe)) < 0) {
        close_owned(io);
        return -1;
    }
    envp = build_envp(sh);
    /* posix_spawn, not fork: this test build runs under ASan, whose fork copies its shadow mappings
     * (about 65 ms a command on macOS, 3 ms spawned), so posix/special_errors (29 `$BASH -c`) took
     * 4-5 s of its 5 s. The same dup2s in the same order; fork stays the way when spawning fails (a
     * file execve refuses: the child's 127, as before) */
    {
        posix_spawn_file_actions_t fa;
        int ok = posix_spawn_file_actions_init(&fa) == 0;
        if (ok && io->in)
            ok = posix_spawn_file_actions_adddup2(&fa, FD(io->in), 0) == 0;
        if (ok && io->out)
            ok = posix_spawn_file_actions_adddup2(&fa, FD(io->out), 1) == 0;
        if (ok && io->err)
            ok = posix_spawn_file_actions_adddup2(&fa, FD(io->err), 2) == 0;
        if (!ok || posix_spawn(&pid, exe, &fa, 0, argv, envp ? envp : (char **)0) != 0)
            pid = 0;
        posix_spawn_file_actions_destroy(&fa);
    }
    if (!pid)
        pid = fork();
    if (pid < 0) {
        close_owned(io);
        return -1;
    }
    if (pid == 0) {
        if (io->in)
            dup2(FD(io->in), 0);
        if (io->out)
            dup2(FD(io->out), 1);
        if (io->err)
            dup2(FD(io->err), 2);
        execve(exe, argv, envp ? envp : (char **)0);
        _exit(127);
    }
    if (envp) {
        for (i = 0; envp[i]; i++)
            free(envp[i]);
        free(envp);
    }
    close_owned(io);
    if (wait)
        return reap(pid, 1);
    for (i = 0; i < MAXJOB; i++)
        if (!jobs[i].pid) {
            jobs[i].pid = pid;
            jobs[i].reaped = 0;
            return (long)pid;
        }
    return reap(pid, 1) >= 0 ? -1 : -1;
}

static long h_wait(void *os, long job)
{
    int i;
    (void)os;
    for (i = 0; i < MAXJOB; i++)
        if (jobs[i].pid == (pid_t)job) {
            long st;
            if (!jobs[i].reaped) {
                jobs[i].status = reap((pid_t)job, 1);
                jobs[i].reaped = 1;
            }
            st = jobs[i].status;
            jobs[i].pid = 0;
            return st < 0 ? 1 : st;
        }
    return 1;
}

static int h_done(void *os, long job)
{
    int i;
    (void)os;
    for (i = 0; i < MAXJOB; i++)
        if (jobs[i].pid == (pid_t)job) {
            if (!jobs[i].reaped) {
                long st = reap((pid_t)job, 0);
                if (st == -2)
                    return 0;
                jobs[i].status = st;
                jobs[i].reaped = 1;
            }
            return 1;
        }
    return 1;
}

static long h_spawn(void *os, sh_shell *child, sh_parse *tree, const sh_io *io, int wait)
{
    pid_t pid;
    int i;
    (void)os;
    pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        hproc *hp = (hproc *)calloc(1, sizeof(*hp));
        long st;
        hp->sh = child;
        child->os.data = hp;
        child->ctx.pid = (long)getpid();
        for (i = 0; i < MAXJOB; i++)
            jobs[i].pid = 0;
        {
            int k;
            for (k = 0; k < 2; k++)
                if (child->cp_close[k])
                    close(FD(child->cp_close[k])); /* a coproc's process does not hold the shell's pipe ends */
        }
        st = sh_run_child(child, tree, io);
        _exit((int)(st & 255));
    }
    close_owned(io);
    /* the child process owns the clone and the tree; this process's copies end here */
    sh_parse_free(tree);
    free(tree);
    sh_shell_free(child);
    free(child);
    if (wait == SH_SPAWN_FG)
        return reap(pid, 1);
    for (i = 0; i < MAXJOB; i++)
        if (!jobs[i].pid) {
            jobs[i].pid = pid;
            jobs[i].reaped = 0;
            return (long)pid;
        }
    return -1;
}

static long h_read(void *os, sh_fh fh, char *buf, long max)
{
    ssize_t n;
    (void)os;
    do
        n = read(FD(fh), buf, (size_t)max);
    while (n < 0 && errno == EINTR);
    return n > 0 ? (long)n : 0;
}

static void h_cpu(void *os, long *t)
{
    struct rusage a, b;
    (void)os;
    getrusage(RUSAGE_SELF, &a);
    getrusage(RUSAGE_CHILDREN, &b);
    t[0] = a.ru_utime.tv_sec * 1000000L + a.ru_utime.tv_usec;
    t[1] = a.ru_stime.tv_sec * 1000000L + a.ru_stime.tv_usec;
    t[2] = b.ru_utime.tv_sec * 1000000L + b.ru_utime.tv_usec;
    t[3] = b.ru_stime.tv_sec * 1000000L + b.ru_stime.tv_usec;
}

static long h_now(void *os, long *usec)
{
    struct timeval tv;
    (void)os;
    gettimeofday(&tv, 0);
    *usec = (long)tv.tv_usec;
    return (long)tv.tv_sec;
}

static long h_sysid(void *os, int what)
{
    (void)os;
    return what == SH_ID_PPID ? (long)getppid() : what == SH_ID_UID ? (long)getuid() : (long)geteuid();
}

static int h_ready(void *os, sh_fh fh, long ms)
{
    struct pollfd pf;
    (void)os;
    pf.fd = FD(fh);
    pf.events = POLLIN;
    pf.revents = 0;
    return poll(&pf, 1, (int)ms) != 0;
}

static void h_echo(void *os, sh_fh fh, int on)
{
    struct termios t;
    (void)os;
    if (tcgetattr(FD(fh), &t))
        return;
    if (on)
        t.c_lflag |= ECHO;
    else
        t.c_lflag &= ~(tcflag_t)ECHO;
    tcsetattr(FD(fh), TCSANOW, &t);
}

static int h_interrupted(void *os)
{
    (void)os;
    return 0;
}

static long h_write(void *os, sh_fh fh, const char *buf, long n)
{
    long done = 0;
    (void)os;
    while (done < n) {
        ssize_t w = write(FD(fh), buf + done, (size_t)(n - done));
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return done ? done : -1;
        }
        done += (long)w;
    }
    return done;
}

/* a line with its newline, byte by byte so a following reader on the same
 * descriptor loses nothing (the shell reads scripts and `read` this way) */
static long h_read_line(void *os, sh_fh fh, char *buf, long max)
{
    long n = 0;
    (void)os;
    while (n < max - 1) {
        char c;
        ssize_t r = read(FD(fh), &c, 1);
        if (r < 0 && errno == EINTR)
            continue;
        if (r <= 0)
            break;
        buf[n++] = c;
        if (c == '\n')
            break;
    }
    buf[n] = 0;
    return n ? n : -1;
}

static int h_chdir(void *os, const char *path)
{
    (void)os;
    return chdir(path);
}

static char *h_cwd(void *os)
{
    char *p = (char *)malloc(1024);
    (void)os;
    if (p && !getcwd(p, 1024))
        p[0] = 0;
    return p;
}

static char *h_realpath(void *os, const char *path)
{
    char *p = (char *)malloc(1024);
    (void)os;
    if (p && !realpath(path, p)) {
        free(p);
        return 0;
    }
    return p;
}

static int h_stat(void *os, const char *path, sh_stat *o, int nofollow)
{
    struct stat st;
    char p[1024];
    (void)os;
    host_path(path, p, sizeof(p));
    if (nofollow ? lstat(p, &st) : stat(p, &st))
        return -1;
    o->link = S_ISLNK(st.st_mode) != 0;
    o->type = S_ISDIR(st.st_mode) ? SH_ST_DIR : S_ISREG(st.st_mode) ? SH_ST_FILE : S_ISCHR(st.st_mode) ? SH_ST_CHAR
            : S_ISBLK(st.st_mode) ? SH_ST_BLOCK : S_ISFIFO(st.st_mode) ? SH_ST_FIFO : S_ISSOCK(st.st_mode) ? SH_ST_SOCK
            : SH_ST_OTHER;
    o->size = (long)st.st_size;
    o->mtime = (long)st.st_mtime;
    o->atime = (long)st.st_atime;
    o->mode = (unsigned)(st.st_mode & 07777);
    o->access = (access(p, R_OK) == 0 ? 4u : 0u) | (access(p, W_OK) == 0 ? 2u : 0u) | (access(p, X_OK) == 0 ? 1u : 0u);
    o->owned = st.st_uid == geteuid();
    o->group = st.st_gid == getegid();
    o->dev = (long)st.st_dev;
    o->ino = (long)st.st_ino;
    return 0;
}

static int h_signal(void *os, long target, int sig, int is_job)
{
    (void)os;
    return is_job ? -1 : kill((pid_t)target, sig);
}

static int h_winsize(void *os, long *cols, long *rows)
{
    struct winsize w;
    (void)os;
    if (ioctl(0, TIOCGWINSZ, &w) != 0)
        return 0;
    *cols = w.ws_col;
    *rows = w.ws_row;
    return 1;
}

static int h_isatty(void *os, sh_fh fh)
{
    (void)os;
    return fh && isatty(FD(fh));
}

static long h_stack(void *os, long bytes)
{
    (void)os;
    (void)bytes;
    return 0;
}

static void h_umask(void *os, int mask)
{
    (void)os;
    umask((mode_t)mask);
}

static int h_listdir(sh_ctx *c, const char *dir, sh_list *out)
{
    DIR *d = opendir(*dir ? dir : ".");
    struct dirent *e;
    (void)c;
    if (!d)
        return -1;
    while ((e = readdir(d)) != 0)
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
            sh_list_add(out, e->d_name);
    closedir(d);
    return 0;
}

/* Leak check (V44): built with -DVH_COUNT (build/vsh_host_leak), tests/vh_alloc.c counts every block
 * the shell core and this driver allocate; with VSH_LEAKCHECK set, the blocks still live after
 * sh_shell_free are printed to stderr as "leak blocks=N". The baseline is 0. */
#ifdef VH_COUNT
extern long vh_live(void);
static long leak_blocks0;
static void leak_mark(void) { leak_blocks0 = vh_live(); }
static void leak_report(void)
{
    if (getenv("VSH_LEAKCHECK"))
        fprintf(stderr, "leak blocks=%ld\n", vh_live() - leak_blocks0);
}
#else
static void leak_mark(void) {}
static void leak_report(void) {}
#endif

static int run_main(int argc, char **argv)
{
    static sh_shell sh;
    static hproc hp;
    const char *command = 0;
    int first = 1, i, hits = 0, a = 1;
    long st = 0;
    extern char **environ;
    char **e;
    if (argc > 1 && !strcmp(argv[1], "--hits")) {
        hits = 1;
        a = 2;
    }
    leak_mark();
    if (!realpath(argv[0], self_exe))
        self_exe[0] = 0;
    sh_shell_init(&sh);
    hp.sh = &sh;
    sh.os.open = h_open;
    sh.os.close = h_close;
    sh.os.pipe = h_pipe;
    sh.os.run = h_run;
    sh.os.wait = h_wait;
    sh.os.done = h_done;
    sh.os.cont = 0;
    sh.os.isatty = h_isatty;
    sh.os.winsize = h_winsize;
    sh.os.stack = h_stack;
    sh.os.umask = h_umask;
    sh.os.spawn = h_spawn;
    sh.os.read = h_read;
    sh.os.ready = h_ready;
    sh.os.now = h_now;
    sh.os.cpu = h_cpu;
    sh.os.remove = h_remove;
    sh.os.tmpdir = h_tmpdir;
    sh.os.sysid = h_sysid;
    sh.os.echo = h_echo;
    sh.os.interrupted = h_interrupted;
    sh.os.write = h_write;
    sh.os.read_line = h_read_line;
    sh.os.chdir = h_chdir;
    sh.os.cwd = h_cwd;
    sh.os.realpath = h_realpath;
    sh.os.stat = h_stat;
    sh.os.signal = h_signal;
    sh.os.data = &hp;
    sh.ctx.listdir = h_listdir;
    sh.ctx.nocase = 0;
    sh.ctx.pid = (long)getpid();
    sh.io.in = FH(0);
    sh.io.out = FH(1);
    sh.io.err = FH(2);
    sh.io.owned = 0;
    /* the shell starts with the host's variables (as vsh imports its
     * environment); bashdiff.py gives both shells the same clean one */
    for (e = environ; *e; e++) {
        const char *eq = strchr(*e, '=');
        if (eq && eq != *e) {
            char name[256];
            size_t n = (size_t)(eq - *e);
            if (n < sizeof(name)) {
                memcpy(name, *e, n);
                name[n] = 0;
                sh_set(&sh.ctx, name, eq + 1);
                sh_export(&sh.ctx, name);
            }
        }
    }
    {
        sh_invoke_info inf;
        argv[a - 1] = argv[0];
        sh_invoke(&sh, argc - (a - 1), argv + (a - 1), isatty(0), &inf);
        if (inf.exit_now)
            return inf.status;
        command = inf.command;
        if (inf.login)
            sh_startup_login(&sh);
        if (command || inf.script)
            sh_startup_env(&sh);
        if (command)
            sh_run_text(&sh, command, 0);
        else if (inf.script)
            sh_run_script(&sh); /* the arguments stay $1 ... */
        else {
            /* a script on standard input, a line at a time as vsh does (h_read_line: a byte at a
             * time, so a command that reads the same input starts where the shell's line ended:
             * `head -n 1` in `vsh -s` reads the script's next line, as in bash). An interactive
             * shell (-i) also runs the prompt hook, history expansion, PS0, the history list */
            char *text = 0, buf[4096];
            int incomplete = 0;
            size_t tl = 0;
            if (sh.opts & SO_INTERACTIVE)
                sh_inputrc(&sh);
            while (!sh.exiting) {
                size_t n;
                int inter = (sh.opts & SO_INTERACTIVE) != 0;
                if (!text && inter) {
                    sh_check_winsize(&sh);
                    sh_prompt_command(&sh);
                }
                if (h_read_line(0, FH(0), buf, sizeof(buf)) < 0)
                    break;
                if (!text && inter) {
                    char *ex;
                    int po, r = sh_hist_expand(&sh, buf, &ex, &po);
                    if (r < 0)
                        continue;
                    if (r > 0) {
                        fputs(ex, stderr);
                        if (po) {
                            sh_hist_replace(&sh, ex, 1);
                            free(ex);
                            continue;
                        }
                        snprintf(buf, sizeof(buf), "%s", ex);
                        free(ex);
                    }
                }
                n = strlen(buf);
                text = (char *)realloc(text, tl + n + 1);
                memcpy(text + tl, buf, n + 1);
                tl += n;
                sh.ps0_on = inter;
                sh_run_text(&sh, text, &incomplete);
                sh.ps0_on = 0;
                if (incomplete)
                    continue;
                free(text);
                text = 0;
                tl = 0;
            }
            free(text);
        }
    }
    (void)first;
    (void)i;
    sh_exit_trap(&sh); /* end of input, exit, a script's end */
    st = sh.exiting ? sh.exit_status : sh.ctx.status;
    sh_shell_free(&sh);
    leak_report();
#ifdef SH_HITS
    if (hits) {
#define SH_HIT_NAME(n) #n,
        static const char *const names[] = { SH_HIT_LIST(SH_HIT_NAME) 0 };
        int k;
        for (k = 0; k < SH_HIT_COUNT; k++)
            fprintf(stderr, "hits %s %lu\n", names[k], sh_hits[k]);
    }
#else
    (void)hits;
#endif
    return (int)(st & 255);
}

int main(int argc, char **argv)
{
    return run_main(argc, argv);
}
