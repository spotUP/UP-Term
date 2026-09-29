/* vsh's executor against a fake OS: in-memory files and pipes, and a
 * few fake commands (cat, upper, wc, fail, ls). */
#include <stdlib.h>
#include "harness.h"
#include "../shell/sh_exec.h"

#define NF 64
typedef struct fbuf {
    char name[64];
    char data[4096];
    int len, rpos, used, is_pipe;
} fbuf;

static fbuf fs[NF];
static char cwd[64];
typedef struct pending {
    char argv[8][64];
    int argc;
    sh_io io;
    int used;
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
    (void)fh; /* files persist; pipes are read to the end */
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

static long f_read_line(void *os, sh_fh fh, char *buf, long max)
{
    fbuf *f = slot(fh);
    long n = 0;
    (void)os;
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

/* The fake commands, run when they are waited for. */
static long run_now(char argv[][64], int argc, const sh_io *io)
{
    char line[256];
    long n;
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
        f_write(0, io->out, "a.c\nb.c\n", 8);
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
    if (wait)
        return run_now(p.argv, p.argc, &p.io);
    if (strcmp(argv[0], "cat") && strcmp(argv[0], "upper") && strcmp(argv[0], "wc") &&
        strcmp(argv[0], "fail") && strcmp(argv[0], "ls") && strcmp(argv[0], "args"))
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

static long f_wait(void *os, long job)
{
    pending *p = &jobs[job - 1];
    long st;
    (void)os;
    st = run_now(p->argv, p->argc, &p->io);
    p->used = 0;
    return st;
}

static int f_done(void *os, long job)
{
    (void)os;
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

static int f_exists(void *os, const char *path, int want_dir)
{
    int i;
    (void)os;
    if (want_dir == 1)
        return !strcmp(path, "SYS:");
    for (i = 0; i < NF; i++)
        if (fs[i].used && !fs[i].is_pipe && !strcmp(fs[i].name, path))
            return 1;
    return 0;
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

static sh_shell sh;
static sh_fh OUT, ERR, IN;

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
    sh.os.write = f_write;
    sh.os.read_line = f_read_line;
    sh.os.chdir = f_chdir;
    sh.os.cwd = f_cwd;
    sh.os.exists = f_exists;
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
    CHECK_STR(run("args \"a b\" c* 'd e'"), "<a b><c*><d e>\n");
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
    CHECK_STR(run("ls | cat | wc"), "2\n");
    CHECK_STR(run("echo a b | read x y; echo $y$x"), "ba\n");
    CHECK_STR(run("echo one >f; echo two >>f; cat <f"), "one\ntwo\n");
    CHECK_STR(run("echo gone >f; echo new >f; cat <f"), "new\n");
    CHECK_STR(run("cat <<EOF\nline $A\nEOF\n"), "line \n");
    CHECK_STR(run("A=v; cat <<EOF\n$A\nEOF\n"), "v\n");
    CHECK_STR(run("A=v; cat <<'EOF'\n$A\nEOF\n"), "$A\n");
    CHECK_STR(run("{ echo a; echo b; } | wc"), "2\n");
    run("nosuch 2>&1");
    CHECK_STR(slot(OUT)->data, "vsh: nosuch: not found\n");
    CHECK_STR(run("echo x >&2"), "");
    CHECK_STR(slot(ERR)->data, "x\n");
}

static void jobs_aliases_and_dirs(void)
{
    run("fail 3 & wait; echo $?");
    CHECK_STR(slot(OUT)->data, "3\n");
    CHECK_STR(slot(ERR)->data, "[1] 1\n");
    CHECK_STR(run("alias ll='args -l'; ll x"), "<-l><x>\n");
    CHECK_STR(run("cd Work:; pwd; (cd SYS:; pwd); pwd"), "Work:\nSYS:\nWork:\n");
    CHECK_STR(run("cd nowhere; echo $?"), "1\n");
    CHECK_STR(run("[ -d SYS: ] && echo dir"), "dir\n");
    CHECK_STR(run("which echo f; f() { :; }; which f"), "echo is a shell builtin\nf is a command\nf is a function\n");
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
    CHECK_STR(slot(ERR)->data, "[1] 1\n[1] Exit 3  fail 3\n");
    run("args a b &");
    sh_notify(&sh);
    CHECK_STR(slot(ERR)->data, "[1] 1\n[1] Done  args a b\n");
    sh_notify(&sh);  /* reported once */
    CHECK_STR(slot(ERR)->data, "[1] 1\n[1] Done  args a b\n");
    CHECK_STR(run("fail 2 & jobs"), "[1] Exit 2  fail 2\n");
    CHECK_STR(run("fail 4 & fg; echo $?"), "fail 4\n4\n");
    run("fg");
    CHECK_STR(slot(ERR)->data, "vsh: fg: no such job\n");
    CHECK_INT(sh.ctx.status, 1);
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

void suite_sh_exec(void)
{
    printf_builtin();
    prompts();
    basics();
    control_flow();
    pipes_and_redirection();
    jobs_aliases_and_dirs();
    incomplete_input();
    job_notices();
    functions_outlive_their_lines();
    sh_shell_free(&sh);
}
