/* vsh's executor against a fake OS: in-memory files and pipes, and a
 * few fake commands (cat, upper, wc, fail, ls). */
#include <stdio.h>
#include <stdlib.h>
#include "harness.h"
#include "../shell/sh_exec.h"

#define NF 64
typedef struct fbuf {
    char name[64];
    char data[4096];
    int len, rpos, used, is_pipe;
    int closes;             /* times the shell closed this handle */
} fbuf;

static fbuf fs[NF];
static char cwd[64];
typedef struct pending {
    char argv[8][64];
    int argc;
    sh_io io;
    int used;
    sh_shell *child;        /* a subshell: runs tree with sh_run_child */
    sh_parse *tree;
    int ran;
    long status;
    int stopped;            /* "stopper", suspended until f_cont */
} pending;
static pending jobs[16];
static int last_owned[16];   /* io.owned each background start was given, in order */
static int n_started;

static fbuf *slot(sh_fh fh)
{
    return fh > 0 && fh <= NF ? &fs[fh - 1] : 0;
}

static sh_fh f_open(void *os, const char *path, int mode)
{
    int i, free_i = -1;
    (void)os;
    for (i = 0; i < NF; i++) {
        if (fs[i].used && !fs[i].is_pipe && !strcmp(fs[i].name, path))
            break;
        if (!fs[i].used && free_i < 0)
            free_i = i;
    }
    if (i == NF) {
        if (mode == SH_OPEN_READ || free_i < 0)
            return SH_NOFH;
        i = free_i;
        memset(&fs[i], 0, sizeof(fs[i]));
        strcpy(fs[i].name, path);
        fs[i].used = 1;
    }
    if (mode == SH_OPEN_WRITE)
        fs[i].len = 0;
    fs[i].rpos = 0;
    return i + 1;
}

static void f_close(void *os, sh_fh fh)
{
    (void)os;
    if (slot(fh))
        slot(fh)->closes++; /* files persist; pipes are read to the end */
}

static int f_pipe(void *os, sh_fh *rd, sh_fh *wr)
{
    int i;
    (void)os;
    for (i = 0; i < NF; i++)
        if (!fs[i].used) {
            memset(&fs[i], 0, sizeof(fs[i]));
            fs[i].used = 1;
            fs[i].is_pipe = 1;
            *rd = *wr = i + 1;
            return 0;
        }
    return -1;
}

static long f_write(void *os, sh_fh fh, const char *b, long n)
{
    fbuf *f = slot(fh);
    (void)os;
    if (!f || f->len + n >= (long)sizeof(f->data))
        return -1;
    memcpy(f->data + f->len, b, n);
    f->len += (int)n;
    f->data[f->len] = 0;
    return n;
}

static void run_pending(pending *p);

/* An empty pipe with a job still to write it: run that job first, as a
 * reader blocks until its writer has written (the fake runs jobs whole). */
static void feed(sh_fh fh)
{
    int i;
    fbuf *f = slot(fh);
    if (!f || !f->is_pipe || f->rpos < f->len)
        return;
    for (i = 0; i < 16; i++)
        if (jobs[i].used && !jobs[i].ran && jobs[i].io.out == fh)
            run_pending(&jobs[i]);
}

static long f_read(void *os, sh_fh fh, char *buf, long max)
{
    fbuf *f = slot(fh);
    long n;
    (void)os;
    feed(fh);
    if (!f || f->rpos >= f->len)
        return 0;
    n = f->len - f->rpos < max ? f->len - f->rpos : max;
    memcpy(buf, f->data + f->rpos, n);
    f->rpos += (int)n;
    return n;
}

static long f_read_line(void *os, sh_fh fh, char *buf, long max)
{
    fbuf *f = slot(fh);
    long n = 0;
    (void)os;
    feed(fh);
    if (!f || f->rpos >= f->len)
        return -1;
    while (f->rpos < f->len && n < max - 1) {
        char ch = f->data[f->rpos++];
        buf[n++] = ch;
        if (ch == '\n')
            break;
    }
    buf[n] = 0;
    return n;
}

static int is_vshrc_cmd(const char *n)
{
    /* what the vshrc reaches: the coreutils commands, Type, and
     * Dir/List, which no ls may turn into again */
    static const char *const names[] = { "Dir", "List", "Type", "cp", "mv", "rm", "mkdir",
                                         "touch", "hl", "mdv", "less", 0 };
    int i;
    for (i = 0; names[i]; i++)
        if (!strcmp(n, names[i]))
            return 1;
    return 0;
}

/* The fake commands, run when they are waited for. */
static sh_shell sh; /* defined below; the fake remote logins read its TERM */

static long run_now(char argv[][64], int argc, const sh_io *io)
{
    char line[256];
    long n;
    if (!strcmp(argv[0], "bebbossh") || !strcmp(argv[0], "telnet") || !strcmp(argv[0], "uptelnet") ||
        !strcmp(argv[0], "rlogin")) {
        /* a remote login: <name><TERM it was given><args...> */
        const char *term = sh_get(&sh.ctx, "TERM");
        int i;
        f_write(0, io->out, "<", 1);
        f_write(0, io->out, argv[0], (long)strlen(argv[0]));
        f_write(0, io->out, "><TERM=", 7);
        f_write(0, io->out, term ? term : "", term ? (long)strlen(term) : 0);
        f_write(0, io->out, ">", 1);
        for (i = 1; i < argc; i++) {
            f_write(0, io->out, "<", 1);
            f_write(0, io->out, argv[i], (long)strlen(argv[i]));
            f_write(0, io->out, ">", 1);
        }
        f_write(0, io->out, "\n", 1);
        return 0;
    }
    if (!strcmp(argv[0], "cat")) {
        while ((n = f_read_line(0, io->in, line, sizeof(line))) >= 0)
            f_write(0, io->out, line, n);
        return 0;
    }
    if (!strcmp(argv[0], "upper")) {
        while ((n = f_read_line(0, io->in, line, sizeof(line))) >= 0) {
            long k;
            for (k = 0; k < n; k++)
                if (line[k] >= 'a' && line[k] <= 'z')
                    line[k] -= 32;
            f_write(0, io->out, line, n);
        }
        return 0;
    }
    if (!strcmp(argv[0], "wc")) {
        int lines = 0;
        char out[16];
        while (f_read_line(0, io->in, line, sizeof(line)) >= 0)
            lines++;
        out[0] = (char)('0' + lines);
        out[1] = '\n';
        f_write(0, io->out, out, 2);
        return 0;
    }
    if (!strcmp(argv[0], "fail"))
        return argc > 1 ? atol(argv[1]) : 5;
    if (!strcmp(argv[0], "ls")) {
        /* Bare ls is the fake listing the pipeline tests count. Given
         * arguments it echoes them instead, so a test can see what vshrc's
         * alias ls expanded to -- that is how the real C:ls is checked to
         * be reached rather than a Dir/List function. */
        if (argc > 1) {
            int i;
            for (i = 1; i < argc; i++) {
                f_write(0, io->out, "<", 1);
                f_write(0, io->out, argv[i], (long)strlen(argv[i]));
                f_write(0, io->out, ">", 1);
            }
            f_write(0, io->out, "\n", 1);
            return 0;
        }
        f_write(0, io->out, "a.c\nb.c\n", 8);
        return 0;
    }
    if (!strcmp(argv[0], "stopper")) {  /* suspended by ^Z when it may be; then goes on */
        f_write(0, io->out, "resumed\n", 8);
        return 0;
    }
    if (is_vshrc_cmd(argv[0])) {  /* the commands the vshrc reaches: <Name><arg>... */
        int i;
        for (i = 0; i < argc; i++) {
            f_write(0, io->out, "<", 1);
            f_write(0, io->out, argv[i], (long)strlen(argv[i]));
            f_write(0, io->out, ">", 1);
        }
        f_write(0, io->out, "\n", 1);
        return 0;
    }
    if (!strcmp(argv[0], "args")) {  /* prints its arguments, one per line in <> */
        int i;
        for (i = 1; i < argc; i++) {
            f_write(0, io->out, "<", 1);
            f_write(0, io->out, argv[i], (long)strlen(argv[i]));
            f_write(0, io->out, ">", 1);
        }
        f_write(0, io->out, "\n", 1);
        return 0;
    }
    return -1;
}

static sh_shell sh;

/* "stopper" in the foreground where the shell allows a suspend: ^Z at
 * once, it stays as job id for f_cont/f_wait */
static long f_stop(pending *p)
{
    int i;
    for (i = 0; i < 16; i++)
        if (!jobs[i].used) {
            jobs[i] = *p;
            jobs[i].used = 1;
            jobs[i].stopped = 1;
            sh.os.stopped = i + 1;
            return SH_STOPPED;
        }
    return -1;
}

static long fake_stack = 4096;
static long f_stack(void *os, long bytes)
{
    (void)os;
    if (bytes > 0)
        fake_stack = bytes;
    return fake_stack;
}

/* the fake's terminal: the shell's own output stream */
static sh_fh OUT;
static int f_isatty(void *os, sh_fh fh)
{
    (void)os;
    return fh == OUT;
}

static int f_cont(void *os, long job)
{
    (void)os;
    if (!jobs[job - 1].used || !jobs[job - 1].stopped)
        return -1;
    jobs[job - 1].stopped = 0;
    return 0;
}

static long f_run(void *os, char **argv, const sh_io *io, int wait)
{
    pending p;
    int i;
    (void)os;
    memset(&p, 0, sizeof(p));
    for (i = 0; argv[i] && i < 8; i++)
        strcpy(p.argv[i], argv[i]);
    p.argc = i;
    p.io = *io;
    if (wait && !strcmp(argv[0], "stopper") && sh.os.suspendable)
        return f_stop(&p);
    if (wait)
        return run_now(p.argv, p.argc, &p.io);
    if (strcmp(argv[0], "cat") && strcmp(argv[0], "upper") && strcmp(argv[0], "wc") &&
        strcmp(argv[0], "fail") && strcmp(argv[0], "ls") && strcmp(argv[0], "args") &&
        strcmp(argv[0], "stopper") && !is_vshrc_cmd(argv[0]))
        return -1; /* as the real layer: not found, nothing started */
    if (n_started < 16)
        last_owned[n_started++] = io->owned;
    for (i = 0; i < 16; i++)
        if (!jobs[i].used) {
            jobs[i] = p;
            jobs[i].used = 1;
            return i + 1;
        }
    return -1;
}

/* A subshell "process": the fake's directory is one global, so the
 * child's own directory is put back when it ends. */
static long run_child(sh_shell *child, sh_parse *tree, const sh_io *io)
{
    char saved[64];
    long st;
    strcpy(saved, cwd);
    st = sh_run_child(child, tree, io);
    strcpy(cwd, saved);
    return st;
}

static void run_pending(pending *p)
{
    if (p->ran)
        return;
    p->ran = 1;
    p->status = p->child ? run_child(p->child, p->tree, &p->io)
                         : run_now(p->argv, p->argc, &p->io);
}

static long f_wait(void *os, long job)
{
    pending *p = &jobs[job - 1];
    (void)os;
    if (p->stopped)
        return SH_STOPPED; /* waited for while stopped: still stopped */
    run_pending(p);
    p->used = 0;
    return p->status;
}

static int n_spawned;

static long f_spawn(void *os, sh_shell *child, sh_parse *tree, const sh_io *io, int wait)
{
    int i;
    (void)os;
    n_spawned++;
    child->ctx.pid = 1000 + n_spawned;
    if (wait)
        return run_child(child, tree, io);
    for (i = 0; i < 16; i++)
        if (!jobs[i].used) {
            memset(&jobs[i], 0, sizeof(jobs[i]));
            jobs[i].used = 1;
            jobs[i].child = child;
            jobs[i].tree = tree;
            jobs[i].io = *io;
            return i + 1;
        }
    return -1;
}

static int f_done(void *os, long job)
{
    (void)os;
    if (jobs[job - 1].stopped)
        return 0;
    return jobs[job - 1].used; /* the fake runs a job when it is waited for */
}

static int f_chdir(void *os, const char *path)
{
    (void)os;
    if (!strcmp(path, "nowhere"))
        return -1;
    strcpy(cwd, path);
    return 0;
}

static char *f_cwd(void *os)
{
    char *r = (char *)malloc(64);
    (void)os;
    strcpy(r, cwd);
    return r;
}

static int f_stat(void *os, const char *path, sh_stat *st, int nofollow)
{
    int i;
    (void)os;
    (void)nofollow;
    memset(st, 0, sizeof(*st));
    st->mode = 0644;
    st->access = 6;
    if (!strcmp(path, "SYS:")) {
        st->type = SH_ST_DIR;
        return 0;
    }
    for (i = 0; i < NF; i++)
        if (fs[i].used && !fs[i].is_pipe && !strcmp(fs[i].name, path)) {
            st->type = SH_ST_FILE;
            st->size = 1;
            st->ino = i + 1;
            return 0;
        }
    return -1;
}

static void sprintf_num(char *out, int v)
{
    char t[16];
    int n = 0, k = 0;
    do
        t[n++] = (char)('0' + v % 10);
    while ((v /= 10) > 0);
    while (n)
        out[k++] = t[--n];
    out[k] = 0;
}

static sh_fh ERR, IN;

static void fresh(void)
{
    sh_shell_free(&sh);
    memset(fs, 0, sizeof(fs));
    memset(jobs, 0, sizeof(jobs));
    memset(last_owned, 0, sizeof(last_owned));
    n_started = 0;
    strcpy(cwd, "RAM:");
    sh_shell_init(&sh);
    sh.os.open = f_open;
    sh.os.close = f_close;
    sh.os.pipe = f_pipe;
    sh.os.run = f_run;
    sh.os.wait = f_wait;
    sh.os.done = f_done;
    sh.os.cont = f_cont;
    sh.os.isatty = f_isatty;
    sh.os.stack = f_stack;
    fake_stack = 4096;
    sh.os.suspendable = 0;
    sh.os.spawn = f_spawn;
    sh.os.read = f_read;
    n_spawned = 0;
    sh.os.write = f_write;
    sh.os.read_line = f_read_line;
    sh.os.chdir = f_chdir;
    sh.os.cwd = f_cwd;
    sh.os.stat = f_stat;
    OUT = f_open(0, "<out>", SH_OPEN_WRITE);
    ERR = f_open(0, "<err>", SH_OPEN_WRITE);
    IN = f_open(0, "<in>", SH_OPEN_WRITE);
    sh.io.in = IN;
    sh.io.out = OUT;
    sh.io.err = ERR;
    sh.io.owned = 0;
}

/* Run text in a fresh shell; the stdout text. */
static const char *run(const char *text)
{
    int inc = 0;
    fresh();
    sh_run_text(&sh, text, &inc);
    return slot(OUT)->data;
}

static void basics(void)
{
    CHECK_STR(run("echo hello world"), "hello world\n");
    CHECK_STR(run("A=x; echo $A ${A}y"), "x xy\n");
    CHECK_STR(run("echo -n a; echo b"), "ab\n");
    CHECK_STR(run("true && echo yes || echo no"), "yes\n");
    CHECK_STR(run("false && echo yes || echo no"), "no\n");
    CHECK_STR(run("fail 7; echo $?"), "7\n");
    CHECK_STR(run("! fail; echo $?"), "0\n");
    run("nosuch arg");
    CHECK_STR(slot(ERR)->data, "vsh: nosuch: not found\n");
    CHECK_INT(sh.ctx.status, 127);
    /* a kit command that is not on PATH says which drawer it lives in */
    run("ssh host");
    CHECK_STR(slot(ERR)->data, "vsh: ssh: not found (it lives in UP-Term:bin: put that drawer on PATH, or run the UP-Term Install)\n");
    CHECK_INT(sh.ctx.status, 127);
    run("nosuch arg");
    CHECK_STR(slot(ERR)->data, "vsh: nosuch: not found\n");
    CHECK_STR(run("args \"a b\" c* 'd e'"), "<a b><c*><d e>\n");
}

/* A subshell's shell is malloc memory (sh_shell_clone): sh_shell_init leaves no field as it
 * found it. nclosed and wfail were left, and on the rig `echo hello | wc -c` hung in the
 * subshell's echo (put walked a garbage count of closed streams). */
/* V86: a login shell reads ENV:vsh/profile then $HOME/.vsh_profile and logout leaves it; a posix shell
 * reads $ENV when it is interactive and BASH_ENV never */
static void a_login_shell_reads_its_profiles_and_a_posix_shell_reads_ENV(void)
{
    run("echo 'echo sys' > ENV:vsh/profile; echo 'echo user' > RAM:h/.vsh_profile; HOME=RAM:h");
    sh_startup_login(&sh);
    CHECK_STR(slot(OUT)->data, "sys\nuser\n");
    run("logout; echo not_reached");
    CHECK_STR(slot(OUT)->data, "not_reached\n");
    run("echo 'echo sys' > ENV:vsh/profile; HOME=RAM:h");
    sh_startup_login(&sh);
    sh_run_text(&sh, "logout; echo not_reached", 0);
    CHECK_STR(slot(OUT)->data, "sys\n");
    run("echo 'echo envfile' > RAM:envf; echo 'echo benv' > RAM:benv; ENV=RAM:envf; BASH_ENV=RAM:benv");
    sh.opts |= SO_POSIX;
    sh_startup_env(&sh);
    CHECK_STR(slot(OUT)->data, "");
    sh.opts |= SO_INTERACTIVE;
    sh_startup_env(&sh);
    CHECK_STR(slot(OUT)->data, "envfile\n");
}

static void a_shell_made_on_dirty_memory_is_a_fresh_shell(void)
{
    /* one place for both, so the fields pointing into the shell itself agree */
    static sh_shell place, clean;
    memset(&place, 0, sizeof(place));
    sh_shell_init(&place);
    clean = place;
    sh_shell_free(&place);
    memset(&place, 0xA5, sizeof(place));
    sh_shell_init(&place);
    CHECK_INT(memcmp(&clean, &place, sizeof(clean)) == 0, 1);
    sh_shell_free(&place);
}

static void control_flow(void)
{
    CHECK_STR(run("if fail; then echo t; elif true; then echo e; else echo f; fi"), "e\n");
    CHECK_STR(run("i=0; while [ $i -lt 3 ]; do echo $i; i=$((i+1)); done"), "0\n1\n2\n");
    CHECK_STR(run("i=0; until [ $i -ge 2 ]; do echo u$i; i=$((i+1)); done"), "u0\nu1\n");
    CHECK_STR(run("for x in a b c; do echo -n $x; done; echo"), "abc\n");
    CHECK_STR(run("for x in 1 2 3 4; do if [ $x = 3 ]; then break; fi; echo $x; done"), "1\n2\n");
    CHECK_STR(run("for x in 1 2 3; do if [ $x = 2 ]; then continue; fi; echo $x; done"), "1\n3\n");
    CHECK_STR(run("case foo.c in *.h) echo h;; *.c) echo c;; esac"), "c\n");
    CHECK_STR(run("f() { echo arg:$1; return 3; }; f one; echo $?"), "arg:one\n3\n");
    CHECK_STR(run("set -- x y; for a; do echo $a; done"), "x\ny\n");
    CHECK_STR(run("exit 4; echo never"), "");
    CHECK_INT(sh.exit_status, 4);
}

static void pipes_and_redirection(void)
{
    CHECK_STR(run("echo hi there | upper"), "HI THERE\n");
    /* /dev/null is NIL: (the vshrc's 2>/dev/null printed "cannot create"
     * on every vsh start), /dev/tty the console */
    CHECK_STR(run("echo gone >/dev/null; cat <NIL:"), "gone\n");
    CHECK_STR(run("echo here >/dev/tty; cat <'*'"), "here\n");
    CHECK_STR(run("ls | cat | wc"), "2\n");
    /* bash's default (lastpipe off): the last stage is a subshell too, x and y stay unset */
    CHECK_STR(run("echo a b | read x y; echo $y$x"), "\n");
    CHECK_STR(run("echo one >f; echo two >>f; cat <f"), "one\ntwo\n");
    CHECK_STR(run("echo gone >f; echo new >f; cat <f"), "new\n");
    CHECK_STR(run("cat <<EOF\nline $A\nEOF\n"), "line \n");
    CHECK_STR(run("A=v; cat <<EOF\n$A\nEOF\n"), "v\n");
    CHECK_STR(run("A=v; cat <<'EOF'\n$A\nEOF\n"), "$A\n");
    CHECK_STR(run("{ echo a; echo b; } | wc"), "2\n");
    /* redirections on compound commands (POSIX 2.9.4): the whole loop, if
     * or case reads and writes the file. vsh ignored them -- a
     * "while read l; do ...; done <in >out" read the shell's own input
     * (Uninstall's User-Startup filter wrote nothing) */
    CHECK_STR(run("echo one >f; echo two >>f; while read l; do echo got $l; done <f >g1; echo --; cat <g1"),
              "--\ngot one\ngot two\n");
    CHECK_STR(run("echo x >f; if read l; then echo in:$l; fi <f"), "in:x\n");
    CHECK_STR(run("if true; then echo a; echo b; fi >g2; echo --; cat <g2"), "--\na\nb\n");
    CHECK_STR(run("for i in 1 2; do echo $i; done >g3; echo --; cat <g3"), "--\n1\n2\n");
    CHECK_STR(run("case z in z) echo zed ;; esac >g4; echo --; cat <g4"), "--\nzed\n");
    CHECK_STR(run("n=0; until [ $n = 2 ]; do n=$((n+1)); echo $n; done >g5; echo --; cat <g5"),
              "--\n1\n2\n");
    run("nosuch 2>&1");
    CHECK_STR(slot(OUT)->data, "vsh: nosuch: not found\n");
    CHECK_STR(run("echo x >&2"), "");
    CHECK_STR(slot(ERR)->data, "x\n");
}

static void jobs_aliases_and_dirs(void)
{
    run("fail 3 & wait; echo $?");
    CHECK_STR(slot(OUT)->data, "0\n"); /* bash: wait with no argument is 0 */
    CHECK_STR(slot(ERR)->data, ""); /* only an interactive shell announces a job */
    CHECK_STR(run("shopt -s expand_aliases; alias ll='args -l'; ll x"), "<-l><x>\n");
    CHECK_STR(run("cd Work:; pwd; (cd SYS:; pwd); pwd"), "Work:\nSYS:\nWork:\n");
    CHECK_STR(run("cd nowhere; echo $?"), "1\n");
    CHECK_STR(run("[ -d SYS: ] && echo dir"), "dir\n");
    CHECK_STR(run("stack; stack 100000; stack; stack 12 || echo refused"),
              "stack 4096\nstack 100000\nrefused\n");
    CHECK_STR(run("[ -t 1 ] && echo tty; [ -t 1 ] >f || echo redirected; [ -t 7 ] || echo no"),
              "tty\nredirected\nno\n");
    CHECK_STR(run("which echo f; f() { :; }; which f"), "echo is a shell builtin\nf is a function\n");
    /* A background stage gets its pipe ends to keep (rig: list's output
     * went to the console, the runner had been given no pipe) */
    run("ls | nosuch | cat");  /* a stage that cannot start says so */
    CHECK_STR(slot(ERR)->data, "vsh: nosuch: not found\n");
    run("ls | cat");
    CHECK_INT(last_owned[0] & SH_OWN_OUT, SH_OWN_OUT);
    CHECK_INT(last_owned[1] & SH_OWN_IN, SH_OWN_IN);
}

static void job_notices(void)
{
    run("fail 3 &");
    sh_notify(&sh);
    CHECK_STR(slot(ERR)->data, "[1]+  Exit 3                     fail 3\n");
    run("args a b &");
    sh_notify(&sh);
    CHECK_STR(slot(ERR)->data, "[1]+  Done                       args a b\n");
    sh_notify(&sh);  /* reported once */
    CHECK_STR(slot(ERR)->data, "[1]+  Done                       args a b\n");
    CHECK_STR(run("fail 2 & jobs"), "[1]+  Exit 2                     fail 2\n");
    CHECK_STR(run("fail 4 & fg; echo $?"), "fail 4\n4\n");
    run("fg");
    CHECK_STR(slot(ERR)->data, "vsh: fg: no such job\n");
    CHECK_INT(sh.ctx.status, 1);
}

/* ^Z: a foreground command stops and waits in the table; bg and fg
 * continue it; exit warns once about stopped jobs (S8). */
static void suspend(void)
{
    int inc = 0;
    run("stopper; echo $?");
    CHECK_STR(slot(OUT)->data, "146\n");
    CHECK_STR(slot(ERR)->data, "\n[1]+  Stopped                    stopper\n");
    CHECK_STR(run("stopper; jobs"), "[1]+  Stopped                    stopper\n");
    /* (the fake runs a job when it is reaped: bg's job is done at once) */
    CHECK_STR(run("stopper; bg; jobs"), "[1] stopper &\nresumed\n[1]+  Done                       stopper\n");
    CHECK_STR(run("stopper; fg; echo $?; jobs"), "stopper\nresumed\n0\n");
    CHECK_STR(run("stopper; bg; wait; jobs"), "[1] stopper &\nresumed\n");
    CHECK_STR(run("stopper x; stopper y; fg %1; jobs"), "stopper x\nresumed\n[2]+  Stopped                    stopper y\n");
    run("bg");
    CHECK_STR(slot(ERR)->data, "vsh: bg: no stopped job\n");
    run("stopper; exit");
    CHECK_INT(sh.exiting, 0);
    CHECK_STR(slot(ERR)->data, "\n[1]+  Stopped                    stopper\nvsh: exit: there are stopped jobs (fg or bg them; exit again to leave them)\n");
    sh_run_text(&sh, "exit", &inc);
    CHECK_INT(sh.exiting, 1);
    /* only a simple command in the foreground stops: a pipeline stage and
     * a background job run on; without job control nothing stops */
    CHECK_STR(run("stopper | cat"), "resumed\n");
    fresh();
    sh.os.cont = 0;
    sh_run_text(&sh, "stopper", &inc);
    CHECK_STR(slot(OUT)->data, "resumed\n");
}

/* Function bodies outlive the line that defined them: only 16 parses were
 * kept, so the 17th function's body pointed into a freed parse. */
static void functions_outlive_their_lines(void)
{
    int i, inc = 0;
    char line[64];
    fresh();
    for (i = 1; i <= 20; i++) {
        strcpy(line, "f");
        sprintf_num(line + 1, i);
        strcat(line, "() { echo body; }");
        sh_run_text(&sh, line, &inc);
    }
    sh_run_text(&sh, "f1; f20", &inc);
    CHECK_STR(slot(OUT)->data, "body\nbody\n");
    /* a function that redefines itself while it runs */
    sh_run_text(&sh, "g() { g() { echo new; }; echo old; }; g; g", &inc);
    CHECK_STR(slot(OUT)->data, "body\nbody\nold\nnew\n");
}

static int last_umask;

static void f_umask(void *os, int mask)
{
    (void)os;
    last_umask = mask;
}

static int intr_after;  /* the fake's Ctrl-C arrives after this many polls (0: never) */

static int f_interrupted(void *os)
{
    (void)os;
    return intr_after > 0 && --intr_after == 0;
}

/* Subshells run as processes of their own (S3.4): what they change stays
 * in them; builtin pipeline stages run at the same time; & takes any
 * command; $( ) is a subshell too. */
/* how often the shell closed the file of that name, and what it holds */
static int closes_of(const char *name)
{
    int i;
    for (i = 0; i < NF; i++)
        if (fs[i].used && !strcmp(fs[i].name, name))
            return fs[i].closes;
    return -1;
}

static const char *data_of(const char *name)
{
    int i;
    for (i = 0; i < NF; i++)
        if (fs[i].used && !strcmp(fs[i].name, name))
            return fs[i].data;
    return "";
}

/* A handle behind several descriptors is closed once, by its last user (V70). */
static void fd_handles_are_counted(void)
{
    /* dup: closing the original keeps the copy open; the copy's close is the last */
    run("exec 3>f; exec 4>&3; exec 3>&-; echo x >&4");
    CHECK_INT(closes_of("f"), 0);
    CHECK_STR(data_of("f"), "x\n");
    sh_run_text(&sh, "exec 4>&-", 0);
    CHECK_INT(closes_of("f"), 1);
    /* a fd 1 that shares the handle stays open when the table slot goes */
    run("exec 3>f; echo a >&3; exec 3>&-");
    CHECK_INT(closes_of("f"), 1);
    /* unwind: a command's own redirections are closed when it ends, once, even when shared */
    run("echo hi 3>f 4>&3");
    CHECK_INT(closes_of("f"), 1);
    run("{ echo a >&3; } 3>f; echo b");
    CHECK_INT(closes_of("f"), 1);
    run("exec 3>f; echo c 3>&-; echo d >&3");
    CHECK_INT(closes_of("f"), 0);
    CHECK_STR(data_of("f"), "d\n");
    /* exec commit: the handle a new exec 3> replaces is closed then, the new one stays */
    run("exec 3>f; exec 3>g");
    CHECK_INT(closes_of("f"), 1);
    CHECK_INT(closes_of("g"), 0);
    run("exec 3>f 4>&3; exec 3>g; echo x >&4");
    CHECK_INT(closes_of("f"), 0);
    CHECK_STR(data_of("f"), "x\n");
    /* subshell: it shares the parent's handles and closes none of them */
    run("exec 3>f; ( echo s >&3; exec 3>&- ); echo t >&3");
    CHECK_INT(closes_of("f"), 0);
    CHECK_STR(data_of("f"), "s\nt\n");
    run("exec 3>f; ( echo s >&3 ) 3>&- 2>/dev/null; echo t >&3");
    CHECK_INT(closes_of("f"), 0);
    CHECK_STR(data_of("f"), "t\n");
    /* background and pipeline subshells: the parent's table is as it was at once, the handle the
     * child uses is closed once, when the job is waited for */
    run("( echo a >&3 ) 3>f & wait; echo x >&3");
    CHECK_STR(slot(ERR)->data, "vsh: 3: bad file descriptor\n");
    CHECK_INT(closes_of("f"), 1);
    CHECK_STR(data_of("f"), "a\n");
    /* an external command: it gets 0-2 only, its table changes are undone when it ends; a handle
     * it still writes to in the background is closed when the job is waited for */
    run("args 3>f; echo y >&3");
    CHECK_STR(slot(ERR)->data, "vsh: 3: bad file descriptor\n");
    CHECK_INT(closes_of("f"), 1);
    run("args q 3>f >&3 &");
    CHECK_INT(closes_of("f"), 0);
    sh_run_text(&sh, "echo z >&3; wait", 0);
    CHECK_INT(closes_of("f"), 1);
    CHECK_STR(data_of("f"), "<q>\n");
    run("( echo b >&3 ) 3>f | cat; echo y >&3");
    CHECK_STR(slot(ERR)->data, "vsh: 3: bad file descriptor\n");
    CHECK_INT(closes_of("f"), 1);
    run("{ echo c >&3; } 3>f | cat; echo z >&3");
    CHECK_STR(slot(ERR)->data, "vsh: 3: bad file descriptor\n");
    CHECK_INT(closes_of("f"), 1);
}

static void subshells(void)
{
    int inc = 0;
    CHECK_STR(run("A=1; (A=2; cd Work:); echo $A; pwd"), "1\nRAM:\n");
    CHECK_STR(run("(exit 3); echo $? after"), "3 after\n");
    CHECK_STR(run("A=x; f() { echo f$1; }; (echo $A; f 1)"), "x\nf1\n");
    CHECK_STR(run("(g() { :; }); which g; echo $?"), "1\n");
    CHECK_STR(run("echo $(cd Work:; echo hi); pwd"), "hi\nRAM:\n");
    CHECK_STR(run("x=$(exit 3); echo after"), "after\n");
    CHECK_STR(run("echo [$(echo a; echo b)]"), "[a b]\n");
    CHECK_STR(run("{ echo a; echo b; } | while read x; do echo got$x; done"), "gota\ngotb\n");
    CHECK_INT(n_spawned, 2);   /* both stages (lastpipe is off, as in bash) */
    CHECK_STR(run("echo a b | read x y; echo $y$x"), "\n");
    run("{ echo bg; fail 2; } & wait; echo $?");
    CHECK_STR(slot(OUT)->data, "bg\n0\n");
    CHECK_STR(slot(ERR)->data, "");
    CHECK_STR(run("echo x | upper & wait"), "X\n");
    CHECK_STR(run("{ echo a; } & jobs"), "a\n[1]+  Done                       { echo a; }\n");
    /* Ctrl-C ends a loop that only runs builtins, and what follows it */
    fresh();
    sh.os.interrupted = f_interrupted;
    intr_after = 50;
    sh_run_text(&sh, "while true; do :; done; echo never", &inc);
    CHECK_STR(slot(OUT)->data, "");
    CHECK_INT(sh.ctx.status, 130);
    sh_run_text(&sh, "echo next line runs", &inc);
    CHECK_STR(slot(OUT)->data, "next line runs\n");
    intr_after = 0;
}

/* $? after assignments only is the last $( )'s status; NAME=v cmd sets
 * NAME for cmd alone. */
static void assignment_status_and_scope(void)
{
    CHECK_STR(run("x=$(exit 3); echo $?"), "3\n");
    CHECK_STR(run("fail 2; x=$(true); echo $?"), "0\n");
    CHECK_STR(run("fail 2; x=1; echo $?"), "0\n");
    CHECK_STR(run("B=1; B=2 true; echo $B"), "1\n");
    CHECK_STR(run("C=5 true; echo [$C]"), "[]\n");
    CHECK_STR(run("f() { echo $D; }; D=4 f; echo [$D]"), "4\n[]\n");
    CHECK_STR(run("A=1; A=2 args $A; echo $A"), "<1>\n1\n");
}

/* read: IFS decides the splitting (IFS= keeps the line whole), -r keeps
 * backslashes; without -r a backslash quotes the next character. */
static void read_builtin(void)
{
    CHECK_STR(run("echo '  lead  tail  ' | { IFS= read -r l; echo \"[$l]\"; }"), "[  lead  tail  ]\n");
    CHECK_STR(run("echo 'a:b:c d' | { IFS=: read x y; echo \"$x|$y\"; }"), "a|b:c d\n");
    CHECK_STR(run("echo 'a\\b c' | { read -r x y; echo \"$x|$y\"; }"), "a\\b|c\n");
    CHECK_STR(run("echo 'a\\ b c' | { read x y; echo \"$x|$y\"; }"), "a b|c\n");
    CHECK_STR(run("echo '  one  two  ' | { read x; echo \"[$x]\"; }"), "[one  two]\n");
    CHECK_STR(run("IFS=:; echo 'p:q' | { read x y; echo \"$x $y\"; }"), "p q\n");
}

/* dist/vshrc: the Unix names reach coreutils' commands as typed (the fake
 * commands print what they got). */

static const char *vshrc_pre; /* run before the vshrc (a user's setting) */

/* the switch file UP-Term Prefs writes (UP_HLCAT_FILE points the vshrc at
 * one in the fake file system): "on" or "off" in it, or 0: none saved */
static void hlcat_file(const char *text)
{
    static char pre[96];
    strcpy(pre, "UP_HLCAT_FILE=hlcat_sw\n");
    if (text) {
        strcat(pre, "echo ");
        strcat(pre, text);
        strcat(pre, " > hlcat_sw\n");
    }
    vshrc_pre = pre;
}

static const char *with_vshrc(const char *text)
{
    static char rc[8192];
    FILE *f = fopen("dist/vshrc", "r");
    size_t n = f ? fread(rc, 1, sizeof(rc) - 1, f) : 0;
    int inc = 0;
    if (f)
        fclose(f);
    rc[n] = 0;
    fresh();
    sh_run_text(&sh, "shopt -s expand_aliases\n", &inc);   /* the interactive shell has aliases on */
    if (vshrc_pre)
        sh_run_text(&sh, vshrc_pre, &inc);
    sh_run_text(&sh, rc, &inc);
    CHECK_STR(slot(ERR)->data, "");      /* it parses and runs clean */
    sh_run_text(&sh, text, &inc);
    return slot(OUT)->data;
}

/* $PATH in Unix form, as ixemul programs read it, walked as AmigaDOS names */
static const char *path_dirs(const char *path)
{
    static char out[256];
    char d[64];
    const char *p = path;
    out[0] = 0;
    while (sh_path_next(&p, d, sizeof(d))) {
        strcat(out, "<");
        strcat(out, d);
        strcat(out, ">");
    }
    return out;
}

static void path_entries_are_amigados_dirs(void)
{
    CHECK_STR(path_dirs("/UP-Term/bin:/gg/bin:/c"), "<UP-Term:bin><gg:bin><c:>");
    CHECK_STR(path_dirs("bin::.:/"), "<bin><><>");      /* relative, current twice, no volume list */
    CHECK_STR(path_dirs("/c:"), "<c:><>");              /* a trailing : is the current directory */
    CHECK_STR(path_dirs(""), "<>");
    CHECK_STR(path_dirs("/a/b/c/d"), "<a:b/c/d>");
    CHECK_STR(path_dirs(0), "");
}

static void vshrc_unix_names(void)
{
    /* ls is no longer a Dir/List wrapper: the command is reached (a function
     * would have printed <Dir>/<List>), and the alias is what adds colour.
     * The ../ names stay as written -- translating them was the shim's job,
     * and ixemul resolves them itself (ls .. lists the parent on the rig). */
    CHECK_STR(with_vshrc("ls"), "<--color=auto>\n");
    CHECK_STR(with_vshrc("ls -la ../x ./y"), "<--color=auto><-la><../x><./y>\n");
    CHECK_STR(with_vshrc("ll ../../z"), "<--color=auto><-l><../../z>\n");
    /* cp, mv, rm, mkdir, touch and cat are coreutils' commands, not
     * functions over Copy/Rename/Delete/MakeDir/SetDate/Type: flags and
     * names reach them as typed (../b stays ../b, ixemul resolves it) */
    CHECK_STR(with_vshrc("mkdir -p RAM:a/b"), "<mkdir><-p><RAM:a/b>\n");
    CHECK_STR(with_vshrc("rm -rf old"), "<rm><-rf><old>\n");
    CHECK_STR(with_vshrc("cp -R a ../b"), "<cp><-R><a><../b>\n");
    CHECK_STR(with_vshrc("mv a 'two words'"), "<mv><a><two words>\n");
    CHECK_STR(with_vshrc("touch new"), "<touch><new>\n");
    hlcat_file(0);
    /* no switch file saved: on, cat is hl -p; the AmigaDOS Type is not aliased */
    CHECK_STR(with_vshrc("cat a.md"), "<hl><-p><a.md>\n");
    CHECK_STR(with_vshrc("Type a.md"), "<Type><a.md>\n");
    vshrc_pre = 0;
    hlcat_file("off");               /* Prefs saved the switch off: the plain command */
    CHECK_STR(with_vshrc("echo '  x y' | cat"), "  x y\n");  /* the command, not hl -p */
    hlcat_file("on");
    CHECK_STR(with_vshrc("cat a.md"), "<hl><-p><a.md>\n");
    CHECK_STR(with_vshrc("echo $hlcat"), "\n");   /* nothing left behind */
    vshrc_pre = 0;
    /* the Unix $PATH: UP-Term's bin first, kept when the user set one */
    CHECK_STR(with_vshrc("echo $PATH"), "/UP-Term/bin:/UP-Term/Python3/bin:/UP-Term/nvim/bin:/gg/bin:/c\n");
    vshrc_pre = "PATH=/mine";
    CHECK_STR(with_vshrc("echo $PATH"), "/mine\n");
    vshrc_pre = 0;
    /* hlp and mdp: hl / mdv in colour into less -R, or into $PAGER */
    CHECK_STR(with_vshrc("hlp x.c"), "<less><-R>\n");
    CHECK_STR(with_vshrc("PAGER='command cat'; hlp 'a b.c' y.s"), "<hl><--color=always><-n><a b.c><y.s>\n");
    CHECK_STR(with_vshrc("PAGER='command cat'; mdp README.md"), "<mdv><--color=always><README.md>\n");
}

/* G3-04: a remote host has no vtcon entry, so ssh, telnet and rlogin give
 * it xterm-256color (what the window is to a Unix machine) -- unless the
 * user named another (UP_REMOTE_TERM), or the TERM is not vtcon (screen's
 * windows are "screen"). vsh's own TERM stays vtcon. */
static void remote_logins_send_xterm_256color(void)
{
    vshrc_pre = "TERM=vtcon";
    CHECK_STR(with_vshrc("ssh -l me host"), "<bebbossh><TERM=xterm-256color><-l><me><host>\n");
    CHECK_STR(with_vshrc("telnet bbs.example 23"), "<uptelnet><TERM=vtcon><bbs.example><23><TERM><xterm-256color>\n");
    CHECK_STR(with_vshrc("rlogin box"), "<rlogin><TERM=xterm-256color><box>\n");
    CHECK_STR(with_vshrc("ssh h; echo $TERM"), "<bebbossh><TERM=xterm-256color><h>\nvtcon\n");
    vshrc_pre = "TERM=vtcon; UP_REMOTE_TERM=xterm-amiga";
    CHECK_STR(with_vshrc("ssh h"), "<bebbossh><TERM=xterm-amiga><h>\n");
    vshrc_pre = "TERM=screen";
    CHECK_STR(with_vshrc("telnet h"), "<uptelnet><TERM=screen><h><TERM><screen>\n");
    vshrc_pre = 0;
}

/* command NAME: the builtin or program, never a function of that name
 * (how the vshrc's telnet() reaches the real telnet) */
static void command_skips_functions(void)
{
    CHECK_STR(run("args() { echo function; }; command args x y"), "<x><y>\n");
    CHECK_STR(run("echo() { :; }; command echo hi"), "hi\n");
    CHECK_STR(run("command"), "");
}

static void deep_recursion(void)
{
    int inc = 0;
    char here;
    CHECK_STR(run("f() { if [ $1 -gt 0 ]; then f $(( $1 - 1 )); fi; }; f 40; echo deep ok"), "deep ok\n");
    /* endless recursion stops with an error when the stack runs low, and
     * the next line runs */
    fresh();
    sh.stack_limit = (unsigned long)&here - 200000;
    sh_run_text(&sh, "g() { g; }; g; echo never", &inc);
    CHECK_STR(slot(OUT)->data, "");
    CHECK_INT(strncmp(slot(ERR)->data, "vsh: nested too deeply (", 24), 0);
    CHECK_INT(sh.ctx.status, 2);   /* an error, not a Ctrl-C (130) */
    sh_run_text(&sh, "echo next", &inc);
    CHECK_STR(slot(OUT)->data, "next\n");
}

/* The words vsh tells the console about (S9). */
static int has_word(const char *list, long n, const char *w)
{
    long k = 0;
    while (k < n) {
        if (!strcmp(list + k, w))
            return 1;
        k += (long)strlen(list + k) + 1;
    }
    return 0;
}

static void word_lists(void)
{
    char buf[4096];
    long n;
    run("myfn() { :; }; alias ll='ls -l'; MYVAR=1");
    n = sh_word_list(&sh, SH_WORDS_COMMANDS, buf, sizeof(buf));
    CHECK_INT(has_word(buf, n, "myfn"), 1);
    CHECK_INT(has_word(buf, n, "ll"), 1);
    CHECK_INT(has_word(buf, n, "fg"), 1);
    CHECK_INT(has_word(buf, n, "printf"), 1);
    CHECK_INT(has_word(buf, n, "["), 0);
    CHECK_INT(has_word(buf, n, "MYVAR"), 0);
    n = sh_word_list(&sh, SH_WORDS_VARIABLES, buf, sizeof(buf));
    CHECK_INT(has_word(buf, n, "MYVAR"), 1);
    CHECK_INT(has_word(buf, n, "myfn"), 0);
    /* whole names only when it does not all fit */
    n = sh_word_list(&sh, SH_WORDS_COMMANDS, buf, 7);
    CHECK_INT(n > 0 && n <= 7 && buf[n - 1] == 0, 1);
}

static void incomplete_input(void)
{
    int inc = 0;
    fresh();
    sh_run_text(&sh, "if true; then", &inc);
    CHECK_INT(inc, 1);
    CHECK_STR(slot(ERR)->data, ""); /* not an error: the shell reads on */
    sh_run_text(&sh, "if true; then\n echo in\nfi", &inc);
    CHECK_INT(inc, 0);
    CHECK_STR(slot(OUT)->data, "in\n");
}

/* The prompt of the shell after text ran. */
static char *prompt_after(const char *text, const char *ps)
{
    static char *last;
    free(last);
    run(text);
    last = sh_prompt(&sh, ps);
    return last;
}

static void prompts(void)
{
    CHECK_STR(prompt_after("HOME=RAM:", "%~> "), "~> ");
    CHECK_STR(prompt_after("HOME=Work:; cd work:src/vtcon", "\\w \\W"), "~/src/vtcon vtcon");
    /* only at a name boundary */
    CHECK_STR(prompt_after("HOME=Work:x; cd Work:xy", "%~"), "Work:xy");
    CHECK_STR(prompt_after("HOME=Work:x; cd Work:x/y", "%~ %c"), "~/y y");
    CHECK_STR(prompt_after("", "%F{red}a%f%K{blue}b%k"), "\033[31ma\033[39m\033[44mb\033[49m");
    CHECK_STR(prompt_after("", "%F{#ff8000}x%F{208}y%B%bz"),
              "\033[38;2;255;128;0mx\033[38;5;208my\033[1m\033[22mz");
    CHECK_STR(prompt_after("A=hi; fail 2", "$A %? $((1+2))"), "hi 2 3");
    /* a directory's name is text, not expanded */
    CHECK_STR(prompt_after("cd 'RAM:$A'; A=no", "\\w"), "RAM:$A");
    CHECK_STR(prompt_after("", "\\u@\\h %n"), "amiga@amiga amiga");
    CHECK_STR(prompt_after("USER=spot HOST=a1200; export USER", "%n@%m"), "spot@a1200");
    CHECK_STR(prompt_after("", "say \"hi\" 100%% \\$ a\\"), "say \"hi\" 100% $ a\\");
    CHECK_STR(prompt_after("", "\\[\\e[1m\\]x"), "\033[1mx");
}

static void printf_builtin(void)
{
    CHECK_STR(run("printf 'a %s b\\n' x"), "a x b\n");
    CHECK_STR(run("printf '%d-%5d|%-5d|%05d\\n' 1 2 3 4"), "1-    2|3    |00004\n");
    CHECK_STR(run("printf '%x %X %o %c %%\\n' 255 255 8 hello"), "ff FF 10 h %\n");
    CHECK_STR(run("printf '%s\\n' a b c"), "a\nb\nc\n");   /* the format again while arguments remain */
    CHECK_STR(run("printf '%.3s|%5.1s|\\n' abcdef xyz"), "abc|    x|\n");
    CHECK_STR(run("printf 'x\\ty\\n'"), "x\ty\n");
    CHECK_STR(run("printf '%b|\\n' 'a\\tb'"), "a\tb|\n");
    CHECK_STR(run("printf '%s %s|\\n' a"), "a |\n");
    CHECK_STR(run("printf '\\101\\x42\\n'"), "AB\n");
    CHECK_STR(run("printf '%+d % d %i\\n' 5 5 -7"), "+5  5 -7\n");
    CHECK_STR(run("printf '%d\\n' \"'A\""), "65\n");
    CHECK_STR(run("printf '%*d|%-*s|\\n' 4 7 3 a"), "   7|a  |\n");
    CHECK_STR(run("printf '%#x %#o\\n' 255 8"), "0xff 010\n");
    CHECK_STR(run("printf '%.3d|%5.2x|%-4c|\\n' 7 10 z"), "007|   0a|z   |\n");
    CHECK_STR(run("printf '%b%s\\n' 'x\\cy' never"), "x");  /* \c in %b stops everything */
    run("printf '%d\\n' abc; echo $?");
    CHECK_STR(slot(OUT)->data, "0\n1\n");
    CHECK_STR(slot(ERR)->data, "vsh: printf: abc: invalid number\n");
    CHECK_STR(run("printf"), "");
    CHECK_INT(sh.ctx.status, 2);
}

/* V2: "/VTC/bin/nvim" is vol:rest when the Amiga meaning has nothing */
static void a_unix_absolute_name_maps_to_its_volume(void)
{
    char out[64];
    CHECK(sh_unix_root("/VTC/nvim-test/nvim", out, sizeof(out)));
    CHECK_STR(out, "VTC:nvim-test/nvim");
    CHECK(sh_unix_root("/RAM", out, sizeof(out)));
    CHECK_STR(out, "RAM:");
    CHECK(sh_unix_root("/RAM/", out, sizeof(out)));
    CHECK_STR(out, "RAM:");
    CHECK(!sh_unix_root("/", out, sizeof(out)));      /* the parent, as AmigaDOS has it */
    CHECK(!sh_unix_root("//x", out, sizeof(out)));    /* the grandparent's x */
    CHECK(!sh_unix_root("c/dir", out, sizeof(out)));  /* relative */
    CHECK(!sh_unix_root("/a/very/long/name/here", out, 8)); /* does not fit */
}

/* Uninstall runs dist/unstartup.sh with vsh to take Install's blocks out of
 * S:User-Startup. The Replay kept ";BEGIN UP-Term python" after an
 * Uninstall: the script it ran (an older copy in ENVARC:up-term) listed the
 * block names it knew. Every ";BEGIN UP-Term <name>" block goes, a name no
 * kit has used yet included; the user's own lines stay byte for byte. */
static void uninstall_removes_every_up_term_block(void)
{
    FILE *f = fopen("dist/unstartup.sh", "rb");
    char script[4096];
    char text[8192];
    size_t n;
    CHECK(f != NULL);
    if (!f)
        return;
    n = fread(script, 1, sizeof(script) - 1, f);
    fclose(f);
    script[n] = 0;
    snprintf(text, sizeof(text),
             "printf '%%s\\n' 'Run >NIL: amiagent TOKEN=x' ';BEGIN UP-Term' 'Assign GG: UP-Term:' ';END UP-Term'"
             " ';BEGIN UP-Term python' 'Assign Python3: UP-Term:Python3' ';END UP-Term python'"
             " ';BEGIN UP-Term future' 'C:Future' ';END UP-Term future'"
             " ';BEGIN UP-Terminal' '  spaced \\\\ line  ' >S:User-Startup\n"
             "%s\ncat <T:User-Startup.up-term\n", script);
    CHECK_STR(run(text), "Run >NIL: amiagent TOKEN=x\n;BEGIN UP-Terminal\n  spaced \\\\ line  \n");
}

/* eval, exec, trap, local, getopts, umask (plan unix-tool-ports row 0.6) */
static const char *errs(void)
{
    return slot(ERR)->data;
}

static void eval_builtin(void)
{
    CHECK_STR(run("eval echo hi there"), "hi there\n");
    CHECK_STR(run("v=A; n=v; eval \"echo \\$$n\""), "A\n");
    CHECK_STR(run("eval 'x=5; echo $x'; echo $x"), "5\n5\n");
    CHECK_STR(run("eval; echo $?"), "0\n");
    CHECK_STR(run("eval fail 4; echo $?"), "4\n");
    CHECK_STR(run("eval 'if true; then' ; echo $?"), "2\n");
    CHECK_STR(errs(), "vsh: eval: unexpected end of input\n");
    CHECK_STR(run("eval 'echo ('; echo $?"), "2\n");
    CHECK_INT(strncmp(errs(), "vsh: eval: ", 11), 0);
    CHECK_STR(run("f() { eval 'return 3'; echo not; }; f; echo $?"), "3\n");
}

static void exec_builtin(void)
{
    int inc_unused = 0;
    CHECK_STR(run("exec echo bye; echo never"), "bye\n");
    CHECK_INT(sh.exiting, 1);
    CHECK_INT(sh.exit_status, 0);
    run("exec fail 6; echo never");
    CHECK_INT(sh.exiting, 1);
    CHECK_INT(sh.exit_status, 6);
    CHECK_STR(run("exec cat <<x\nfrom exec\nx\necho never"), "from exec\n");
    /* bash: exec of a missing command ends a shell that is not interactive with 127 (posix mode or not) */
    CHECK_STR(run("exec nosuchcmd; echo never"), "");
    CHECK_STR(errs(), "vsh: nosuchcmd: not found\n");
    CHECK_INT(sh.exiting, 1);
    CHECK_INT(sh.exit_status, 127);
    /* shopt execfail, an interactive shell and a subshell go on */
    CHECK_STR(run("shopt -s execfail; exec nosuchcmd; echo still $?"), "still 127\n");
    CHECK_INT(sh.exiting, 0);
    CHECK_STR(run("(exec nosuchcmd); echo sub $?"), "sub 127\n");
    CHECK_INT(sh.exiting, 0);
    run("");
    sh.opts |= SO_INTERACTIVE;
    sh_run_text(&sh, "exec nosuchcmd; echo still $?", &inc_unused);
    CHECK_STR(slot(OUT)->data, "still 127\n");
    CHECK_INT(sh.exiting, 0);
    CHECK_STR(run("exec; echo plain $?"), "plain 0\n");
    CHECK_STR(run("(exec echo sub); echo after"), "sub\nafter\n");
}

static void trap_builtin(void)
{
    int inc = 0;
    CHECK_STR(run("trap 'echo bye' EXIT; echo body"), "body\n"); /* the shell runs it where it ends */
    fresh();
    sh_run_text(&sh, "trap 'echo bye' EXIT; echo body", &inc);
    sh_exit_trap(&sh);
    CHECK_STR(slot(OUT)->data, "body\nbye\n");
    sh_exit_trap(&sh);       /* once */
    CHECK_STR(slot(OUT)->data, "body\nbye\n");
    /* after exit the status is the exit's, the trap does not change it */
    fresh();
    sh_run_text(&sh, "trap 'echo t; fail 9' EXIT; exit 3", &inc);
    sh_exit_trap(&sh);
    CHECK_STR(slot(OUT)->data, "t\n");
    CHECK_INT(sh.exiting, 1);
    CHECK_INT(sh.exit_status, 3);
    /* ... unless the trap calls exit itself */
    fresh();
    sh_run_text(&sh, "trap 'exit 7' 0; exit 3", &inc);
    sh_exit_trap(&sh);
    CHECK_INT(sh.exit_status, 7);
    /* a subshell runs its own EXIT trap and does not inherit the parent's */
    CHECK_STR(run("trap 'echo parent' EXIT; (echo in); (trap 'echo child' EXIT; echo c)"), "in\nc\nchild\n");
    /* listing, resetting, ignoring */
    CHECK_STR(run("trap 'echo a b' INT; trap '' TERM; trap"), "trap -- 'echo a b' SIGINT\ntrap -- '' SIGTERM\n");
    CHECK_STR(run("trap 'echo x' INT; trap - INT; trap"), "");
    CHECK_STR(run("trap 'echo x' SIGINT 2; trap 2; trap"), "");
    /* Ctrl-C runs the INT trap and the script goes on; without a trap it unwinds */
    fresh();
    sh.os.interrupted = f_interrupted;
    intr_after = 20;
    sh_run_text(&sh, "trap 'echo caught' INT; i=0; while [ $i -lt 40 ]; do i=$((i+1)); done; echo done $i", &inc);
    CHECK_STR(slot(OUT)->data, "caught\ndone 40\n");
    fresh();
    sh.os.interrupted = f_interrupted;
    intr_after = 20;
    sh_run_text(&sh, "trap '' INT; i=0; while [ $i -lt 40 ]; do i=$((i+1)); done; echo done $i", &inc);
    CHECK_STR(slot(OUT)->data, "done 40\n");
    fresh();
    sh.os.interrupted = f_interrupted;
    intr_after = 20;
    sh_run_text(&sh, "trap - INT; i=0; while [ $i -lt 40 ]; do i=$((i+1)); done; echo done $i", &inc);
    CHECK_STR(slot(OUT)->data, "");
    CHECK_INT(sh.ctx.status, 130);
    intr_after = 0;
    /* TERM arrives from the OS layer */
    fresh();
    CHECK_INT(sh_trap_signal(&sh, 15), 0);
    sh_run_text(&sh, "trap 'echo term' TERM", &inc);
    CHECK_INT(sh_trap_signal(&sh, 15), 1);
    CHECK_STR(slot(OUT)->data, "term\n");
    CHECK_INT(sh_trap_signal(&sh, 2), 0);
    /* errors */
    run("trap 'echo x' NOSUCH; echo $?");
    CHECK_STR(slot(OUT)->data, "1\n");
    CHECK_STR(errs(), "vsh: NOSUCH: invalid signal specification\n");
    run("trap 'echo x'; echo $?");
    CHECK_STR(slot(OUT)->data, "2\n");
    CHECK_STR(errs(), "vsh: trap: usage: trap [action] signal ...\n");
}

static void local_builtin(void)
{
    CHECK_STR(run("x=out; f() { local x=in; echo $x; }; f; echo $x"), "in\nout\n");
    CHECK_STR(run("f() { local x; echo [$x]; x=set; }; x=out; f; echo $x"), "[]\nout\n");
    CHECK_STR(run("f() { local y=1; }; f; echo [$y]"), "[]\n");
    CHECK_STR(run("f() { local x=f; g; echo $x; }; g() { local x=g; echo $x; }; x=top; f; echo $x"), "g\nf\ntop\n");
    CHECK_STR(run("f() { local a=1 b=2; echo $a$b; }; f"), "12\n");
    CHECK_STR(run("f() { local x=1; return 4; }; x=o; f; echo $? $x"), "4 o\n");
    CHECK_STR(run("f() { local x=1; x=2; }; x=o; f; f; echo $x"), "o\n");
    CHECK_STR(run("f() { local x=1; (echo $x); }; f"), "1\n");
    run("local x=1; echo $?");
    CHECK_STR(slot(OUT)->data, "1\n");
    CHECK_STR(errs(), "vsh: local: can only be used in a function\n");
    run("f() { local =1; echo $?; }; f");
    CHECK_STR(errs(), "vsh: =1: not a valid identifier\n");
}

static void getopts_builtin(void)
{
    const char *loop = "while getopts ab:c o; do echo \"o=$o arg=$OPTARG ind=$OPTIND\"; done; shift $((OPTIND-1)); echo \"rest=$*\"";
    char text[512];
    snprintf(text, sizeof(text), "set -- -a -b val file; %s", loop);
    CHECK_STR(run(text), "o=a arg= ind=2\no=b arg=val ind=4\nrest=file\n");
    snprintf(text, sizeof(text), "set -- -abval -c x y; %s", loop);
    CHECK_STR(run(text), "o=a arg= ind=1\no=b arg=val ind=2\no=c arg= ind=3\nrest=x y\n");
    snprintf(text, sizeof(text), "set -- -ac -- -a; %s", loop);
    CHECK_STR(run(text), "o=a arg= ind=1\no=c arg= ind=2\nrest=-a\n");
    CHECK_STR(run("getopts a o -a x; echo $o $OPTIND $?; getopts a o -a x; echo $o $OPTIND $?"), "a 2 0\n? 2 1\n");
    /* explicit arguments, the positional ones left alone */
    CHECK_STR(run("set -- keep; getopts a o -a; echo $o $1"), "a keep\n");
    /* a script resets OPTIND to scan again */
    CHECK_STR(run("getopts a o -a; getopts a o -a; OPTIND=1; getopts a o -a; echo $o $OPTIND"), "a 2\n");
    /* errors: illegal option, missing argument -- loud and silent */
    CHECK_STR(run("getopts a o -z; echo $o [$OPTARG] $?"), "? [] 0\n");
    CHECK_STR(errs(), "vsh: getopts: illegal option -- z\n");
    CHECK_STR(run("getopts :a o -z; echo $o [$OPTARG]"), "? [z]\n");
    CHECK_STR(errs(), "");
    CHECK_STR(run("getopts b: o -b; echo $o [$OPTARG] $OPTIND"), "? [] 2\n");
    CHECK_STR(errs(), "vsh: getopts: option requires an argument -- b\n");
    CHECK_STR(run("getopts :b: o -b; echo $o [$OPTARG] $OPTIND"), ": [b] 2\n");
    CHECK_STR(run("getopts a; echo $?"), "2\n");
    CHECK_STR(errs(), "vsh: getopts: usage: getopts optstring name [arg ...]\n");
    CHECK_STR(run("getopts a o plain; echo $o $OPTIND $?"), "? 1 1\n");
}

/* type is an alias of which; a command prints the file $PATH resolves it to */
static void which_type(void)
{
    /* every run() starts a fresh shell and file system: the files and $PATH come first */
#define WT "echo x > gg:bin/tool; echo x > c:other; PATH=/c:/gg/bin:/nowhere; "
    CHECK_STR(run("type echo; type nosuch; echo $?"), "echo is a shell builtin\n1\n");
    CHECK_STR(errs(), "vsh: nosuch: not found\n");
    CHECK_STR(run("f() { :; }; type f"), "f is a function\nf () \n{ \n    :\n}\n");
    CHECK_STR(run(WT "which tool; type tool; echo $?"), "gg:bin/tool\ntool is gg:bin/tool\n0\n");
    CHECK_STR(run(WT "which nosuch tool; echo $?"), "gg:bin/tool\n1\n");   /* quiet, status 1 */
    CHECK_STR(errs(), "");
    CHECK_STR(run(WT "which other"), "c:other\n");      /* /c in $PATH */
    CHECK_STR(run(WT "echo x > C:more; PATH=/gg/bin; which more"), "C:more\n");   /* C: when $PATH has none */
    CHECK_STR(run(WT "which gg:bin/tool"), "gg:bin/tool\n");   /* a path as given */
    CHECK_STR(run(WT "which gg:bin/nosuch; echo $?"), "1\n");
    CHECK_STR(run(WT "echo x > tool; which tool"), "tool\n");  /* the current directory comes first */
#undef WT
}

static void umask_builtin(void)
{
    CHECK_STR(run("umask"), "0022\n");
    CHECK_STR(run("umask 077; umask; umask -S"), "0077\nu=rwx,g=,o=\n");
    CHECK_STR(run("umask 7; umask"), "0007\n");
    CHECK_STR(run("umask -S"), "u=rwx,g=rx,o=rx\n");
    CHECK_STR(run("umask 077; (umask 0; umask); umask"), "0000\n0077\n");
    CHECK_STR(run("umask 077; (umask)"), "0077\n");   /* a subshell starts with the shell's mask */
    run("umask 089; echo $?");
    CHECK_STR(slot(OUT)->data, "1\n");
    CHECK_STR(errs(), "vsh: umask: not an octal mask\n");
    run("umask 1000; echo $?");
    CHECK_STR(errs(), "vsh: umask: mask out of range (000 to 777)\n");
    run("umask 0777; umask 1 2");
    CHECK_STR(errs(), "vsh: umask: too many arguments\n");
    run("umask -S 022");
    CHECK_STR(errs(), "vsh: umask: -S only prints the mask\n");
    /* the OS layer is told, so the commands it starts get the mask */
    fresh();
    sh.os.umask = f_umask;
    last_umask = -1;
    sh_run_text(&sh, "umask 027", 0);
    CHECK_INT(last_umask, 027);
    sh.os.umask = 0;
}

void suite_sh_exec(void)
{
    eval_builtin();
    exec_builtin();
    trap_builtin();
    local_builtin();
    getopts_builtin();
    which_type();
    umask_builtin();
    a_unix_absolute_name_maps_to_its_volume();
    printf_builtin();
    prompts();
    basics();
    a_shell_made_on_dirty_memory_is_a_fresh_shell();
    a_login_shell_reads_its_profiles_and_a_posix_shell_reads_ENV();
    control_flow();
    pipes_and_redirection();
    jobs_aliases_and_dirs();
    incomplete_input();
    job_notices();
    suspend();
    functions_outlive_their_lines();
    subshells();
    fd_handles_are_counted();
    assignment_status_and_scope();
    read_builtin();
    path_entries_are_amigados_dirs();
    vshrc_unix_names();
    remote_logins_send_xterm_256color();
    command_skips_functions();
    deep_recursion();
    word_lists();
    uninstall_removes_every_up_term_block();
    sh_shell_free(&sh);
}
