/* vsh's executor (see sh_exec.h). */
#include <stdlib.h>
#include <string.h>
#include "sh_exec.h"

static char *sdup(const char *s)
{
    size_t n = strlen(s);
    char *r = (char *)malloc(n + 1);
    if (r)
        memcpy(r, s, n + 1);
    return r;
}

void sh_shell_init(sh_shell *sh)
{
    memset(&sh->ctx, 0, sizeof(sh->ctx));
    sh->funcs = 0;
    memset(&sh->aliases, 0, sizeof(sh->aliases));
    sh->exiting = 0;
    sh->exit_status = 0;
    sh->breaking = sh->continuing = sh->returning = 0;
    sh->loop_depth = sh->func_depth = 0;
    memset(sh->jobs, 0, sizeof(sh->jobs));
    memset(sh->job_text, 0, sizeof(sh->job_text));
    sh->n_kept = 0;
    sh->keep_parse = 0;
    sh->heredocs = 0;
}

void sh_shell_free(sh_shell *sh)
{
    int i;
    while (sh->funcs) {
        sh_func *f = sh->funcs;
        sh->funcs = f->next;
        free(f->name);
        free(f);
    }
    for (i = 0; i < sh->n_kept; i++)
        sh_parse_free(&sh->kept[i]);
    for (i = 0; i < 32; i++)
        free(sh->job_text[i]);
    sh_list_free(&sh->aliases);
    sh_ctx_free(&sh->ctx);
}

/* ---- output helpers ------------------------------------------------------------- */

static void say(sh_shell *sh, sh_fh fh, const char *s)
{
    sh->os.write(sh->os.data, fh, s, (long)strlen(s));
}

static void err2(sh_shell *sh, const sh_io *io, const char *a, const char *b)
{
    say(sh, io->err, "vsh: ");
    say(sh, io->err, a);
    if (b) {
        say(sh, io->err, ": ");
        say(sh, io->err, b);
    }
    say(sh, io->err, "\n");
}

static void num(char *out, long v)
{
    char t[24];
    int n = 0, k = 0;
    if (v < 0) {
        out[k++] = '-';
        v = -v;
    }
    do
        t[n++] = (char)('0' + v % 10);
    while ((v /= 10) > 0);
    while (n)
        out[k++] = t[--n];
    out[k] = 0;
}

static void close_owned(sh_shell *sh, const sh_io *io)
{
    if ((io->owned & SH_OWN_IN) && io->in)
        sh->os.close(sh->os.data, io->in);
    if ((io->owned & SH_OWN_OUT) && io->out)
        sh->os.close(sh->os.data, io->out);
    if ((io->owned & SH_OWN_ERR) && io->err && io->err != io->out)
        sh->os.close(sh->os.data, io->err);
}

/* ---- expansion of a command's words --------------------------------------------- */

static int expand_words(sh_shell *sh, const sh_word *w, sh_list *out, const sh_io *io)
{
    for (; w; w = w->next) {
        const char *err = 0;
        if (sh_expand(&sh->ctx, w->text, 0, out, &err)) {
            err2(sh, io, w->text, err);
            return -1;
        }
    }
    return 0;
}

static char *expand_one(sh_shell *sh, const char *text, const sh_io *io)
{
    sh_list l;
    const char *err = 0;
    char *r;
    memset(&l, 0, sizeof(l));
    if (sh_expand(&sh->ctx, text, SH_NO_SPLIT | SH_NO_GLOB, &l, &err)) {
        err2(sh, io, text, err);
        sh_list_free(&l);
        return 0;
    }
    r = sdup(l.n ? l.v[0] : "");
    sh_list_free(&l);
    return r;
}

/* ---- redirections ------------------------------------------------------------- */

/* The streams a command runs with: the parent's, with the redirections
 * applied. Streams opened here are marked owned. 0 = ok. */
static int redirect(sh_shell *sh, const sh_redir *r, const sh_io *parent, sh_io *io)
{
    *io = *parent;
    io->owned = 0;
    for (; r; r = r->next) {
        sh_fh fh = SH_NOFH;
        if (r->kind == SH_R_DUPOUT || r->kind == SH_R_DUPIN) {
            /* 2>&1, 1>&2 */
            sh_fh src = !strcmp(r->target, "1") ? io->out : !strcmp(r->target, "2") ? io->err
                      : !strcmp(r->target, "0") ? io->in : SH_NOFH;
            if (!src) {
                err2(sh, parent, r->target, "bad file descriptor");
                return -1;
            }
            if (r->fd == 2)
                io->err = src, io->owned &= ~SH_OWN_ERR;
            else if (r->fd == 1)
                io->out = src, io->owned &= ~SH_OWN_OUT;
            else if (r->fd == 0)
                io->in = src, io->owned &= ~SH_OWN_IN;
            continue;
        }
        if (r->kind == SH_R_HEREDOC) {
            /* the document into a temp file, then read from it */
            char path[40], n[16];
            char *text = r->quoted ? sdup(r->target) : expand_one(sh, r->target, parent);
            sh_fh w;
            num(n, ++sh->heredocs);
            strcpy(path, "T:vsh-here.");
            strcat(path, n);
            w = sh->os.open(sh->os.data, path, SH_OPEN_WRITE);
            if (!w || !text) {
                err2(sh, parent, path, "cannot write the here-document");
                free(text);
                return -1;
            }
            sh->os.write(sh->os.data, w, text, (long)strlen(text));
            sh->os.close(sh->os.data, w);
            free(text);
            fh = sh->os.open(sh->os.data, path, SH_OPEN_READ);
        } else {
            char *path = expand_one(sh, r->target, parent);
            int mode = r->kind == SH_R_IN ? SH_OPEN_READ
                     : r->kind == SH_R_APPEND ? SH_OPEN_APPEND : SH_OPEN_WRITE;
            if (!path)
                return -1;
            fh = sh->os.open(sh->os.data, path, mode);
            if (!fh)
                err2(sh, parent, path, mode == SH_OPEN_READ ? "cannot open" : "cannot create");
            free(path);
        }
        if (!fh)
            return -1;
        if (r->kind == SH_R_BOTH) {
            if (io->owned & SH_OWN_OUT)
                sh->os.close(sh->os.data, io->out);
            io->out = io->err = fh;
            io->owned |= SH_OWN_OUT;
            io->owned &= ~SH_OWN_ERR;
        } else if (r->fd == 0) {
            if (io->owned & SH_OWN_IN)
                sh->os.close(sh->os.data, io->in);
            io->in = fh;
            io->owned |= SH_OWN_IN;
        } else if (r->fd == 2) {
            if (io->owned & SH_OWN_ERR)
                sh->os.close(sh->os.data, io->err);
            io->err = fh;
            io->owned |= SH_OWN_ERR;
        } else {
            if (io->owned & SH_OWN_OUT)
                sh->os.close(sh->os.data, io->out);
            io->out = fh;
            io->owned |= SH_OWN_OUT;
        }
    }
    return 0;
}

/* ---- builtins ---------------------------------------------------------------- */

typedef long (*builtin_fn)(sh_shell *sh, int argc, char **argv, const sh_io *io);

static long b_true(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    (void)sh; (void)argc; (void)argv; (void)io;
    return 0;
}

static long b_false(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    (void)sh; (void)argc; (void)argv; (void)io;
    return 1;
}

static long b_echo(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i = 1, nl = 1;
    if (argc > 1 && !strcmp(argv[1], "-n")) {
        nl = 0;
        i++;
    }
    for (; i < argc; i++) {
        say(sh, io->out, argv[i]);
        if (i + 1 < argc)
            say(sh, io->out, " ");
    }
    if (nl)
        say(sh, io->out, "\n");
    return 0;
}

static long b_cd(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    const char *dir = argc > 1 ? argv[1] : sh_get(&sh->ctx, "HOME");
    char *old = sh->os.cwd(sh->os.data);
    if (!dir)
        dir = "SYS:";
    if (!strcmp(dir, "-"))
        dir = sh_get(&sh->ctx, "OLDPWD") ? sh_get(&sh->ctx, "OLDPWD") : "";
    if (sh->os.chdir(sh->os.data, dir)) {
        err2(sh, io, "cd", dir);
        free(old);
        return 1;
    }
    if (old)
        sh_set(&sh->ctx, "OLDPWD", old);
    free(old);
    old = sh->os.cwd(sh->os.data);
    if (old)
        sh_set(&sh->ctx, "PWD", old);
    free(old);
    return 0;
}

static long b_pwd(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    char *d = sh->os.cwd(sh->os.data);
    (void)argc; (void)argv;
    say(sh, io->out, d ? d : "");
    say(sh, io->out, "\n");
    free(d);
    return 0;
}

static void assign(sh_shell *sh, const char *a, int export)
{
    const char *eq = strchr(a, '=');
    char name[128];
    size_t n = eq ? (size_t)(eq - a) : strlen(a);
    if (n >= sizeof(name))
        return;
    memcpy(name, a, n);
    name[n] = 0;
    if (eq)
        sh_set(&sh->ctx, name, eq + 1);
    if (export)
        sh_export(&sh->ctx, name);
}

static long b_export(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i;
    (void)io;
    for (i = 1; i < argc; i++)
        assign(sh, argv[i], 1);
    return 0;
}

static long b_unset(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i;
    (void)io;
    for (i = 1; i < argc; i++)
        sh_unset(&sh->ctx, argv[i]);
    return 0;
}

static long b_set(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i;
    if (argc == 1) {
        sh_var *v;
        for (v = sh->ctx.vars; v; v = v->next) {
            say(sh, io->out, v->name);
            say(sh, io->out, "=");
            say(sh, io->out, v->value);
            say(sh, io->out, "\n");
        }
        return 0;
    }
    i = 1;
    if (!strcmp(argv[1], "--"))
        i = 2;
    sh_list_free(&sh->ctx.args);
    for (; i < argc; i++)
        sh_list_add(&sh->ctx.args, argv[i]);
    return 0;
}

static long b_shift(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int n = argc > 1 ? atoi(argv[1]) : 1, i;
    (void)io;
    if (n > sh->ctx.args.n)
        return 1;
    for (i = 0; i < n; i++)
        free(sh->ctx.args.v[i]);
    memmove(sh->ctx.args.v, sh->ctx.args.v + n, (sh->ctx.args.n - n + 1) * sizeof(char *));
    sh->ctx.args.n -= n;
    return 0;
}

static long b_exit(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    (void)io;
    sh->exiting = 1;
    sh->exit_status = argc > 1 ? atol(argv[1]) : sh->ctx.status;
    return sh->exit_status;
}

static long b_return(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    (void)io;
    sh->returning = 1;
    return argc > 1 ? atol(argv[1]) : sh->ctx.status;
}

static long b_break(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    (void)io;
    if (sh->loop_depth)
        sh->breaking = argc > 1 ? atoi(argv[1]) : 1;
    return 0;
}

static long b_continue(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    (void)io;
    (void)argv;
    (void)argc;
    if (sh->loop_depth)
        sh->continuing = 1;
    return 0;
}

static long b_read(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    char line[1024];
    long n = sh->os.read_line(sh->os.data, io->in, line, sizeof(line));
    int i;
    char *p = line;
    if (n < 0)
        return 1;
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        line[--n] = 0;
    for (i = 1; i < argc; i++) {
        char *start;
        while (*p == ' ' || *p == '\t')
            p++;
        start = p;
        if (i + 1 < argc) {
            while (*p && *p != ' ' && *p != '\t')
                p++;
            if (*p)
                *p++ = 0;
        }
        sh_set(&sh->ctx, argv[i], start);
    }
    return 0;
}

static long b_alias(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i, k;
    if (argc == 1) {
        for (k = 0; k < sh->aliases.n; k++) {
            say(sh, io->out, "alias ");
            say(sh, io->out, sh->aliases.v[k]);
            say(sh, io->out, "\n");
        }
        return 0;
    }
    for (i = 1; i < argc; i++) {
        const char *eq = strchr(argv[i], '=');
        size_t n = eq ? (size_t)(eq - argv[i]) : strlen(argv[i]);
        for (k = 0; k < sh->aliases.n; k++)
            if (!strncmp(sh->aliases.v[k], argv[i], n) && sh->aliases.v[k][n] == '=')
                break;
        if (!eq) {
            if (k < sh->aliases.n) {
                say(sh, io->out, sh->aliases.v[k]);
                say(sh, io->out, "\n");
            }
            continue;
        }
        if (k < sh->aliases.n) {
            free(sh->aliases.v[k]);
            sh->aliases.v[k] = sdup(argv[i]);
        } else {
            sh_list_add(&sh->aliases, argv[i]);
        }
    }
    return 0;
}

static long b_unalias(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i, k;
    (void)io;
    for (i = 1; i < argc; i++) {
        size_t n = strlen(argv[i]);
        for (k = 0; k < sh->aliases.n; k++)
            if (!strncmp(sh->aliases.v[k], argv[i], n) && sh->aliases.v[k][n] == '=') {
                free(sh->aliases.v[k]);
                memmove(sh->aliases.v + k, sh->aliases.v + k + 1, (sh->aliases.n - k) * sizeof(char *));
                sh->aliases.n--;
                break;
            }
    }
    return 0;
}

/* test / [ : the common forms. */
static long b_test(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int neg = 0, r;
    char **a = argv + 1;
    int n = argc - 1;
    if (!strcmp(argv[0], "[")) {
        if (n < 1 || strcmp(a[n - 1], "]")) {
            err2(sh, io, "[", "] is missing");
            return 2;
        }
        n--;
    }
    if (n > 0 && !strcmp(a[0], "!")) {
        neg = 1;
        a++;
        n--;
    }
    if (n == 0)
        r = 0;
    else if (n == 1)
        r = a[0][0] != 0;
    else if (n == 2) {
        if (!strcmp(a[0], "-z"))
            r = !a[1][0];
        else if (!strcmp(a[0], "-n"))
            r = a[1][0] != 0;
        else if (!strcmp(a[0], "-e"))
            r = sh->os.exists(sh->os.data, a[1], -1);
        else if (!strcmp(a[0], "-f"))
            r = sh->os.exists(sh->os.data, a[1], 0);
        else if (!strcmp(a[0], "-d"))
            r = sh->os.exists(sh->os.data, a[1], 1);
        else {
            err2(sh, io, "test", a[0]);
            return 2;
        }
    } else if (n == 3) {
        const char *op = a[1];
        long x = atol(a[0]), y = atol(a[2]);
        if (!strcmp(op, "=") || !strcmp(op, "=="))
            r = !strcmp(a[0], a[2]);
        else if (!strcmp(op, "!="))
            r = strcmp(a[0], a[2]) != 0;
        else if (!strcmp(op, "-eq"))
            r = x == y;
        else if (!strcmp(op, "-ne"))
            r = x != y;
        else if (!strcmp(op, "-lt"))
            r = x < y;
        else if (!strcmp(op, "-le"))
            r = x <= y;
        else if (!strcmp(op, "-gt"))
            r = x > y;
        else if (!strcmp(op, "-ge"))
            r = x >= y;
        else {
            err2(sh, io, "test", op);
            return 2;
        }
    } else {
        err2(sh, io, "test", "too many arguments");
        return 2;
    }
    return (r != neg) ? 0 : 1;
}

static long b_jobs(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i;
    char n[16];
    (void)argc; (void)argv;
    for (i = 0; i < 32; i++)
        if (sh->jobs[i]) {
            num(n, i + 1);
            say(sh, io->out, "[");
            say(sh, io->out, n);
            say(sh, io->out, "] Running  ");
            say(sh, io->out, sh->job_text[i] ? sh->job_text[i] : "");
            say(sh, io->out, "\n");
        }
    return 0;
}

/* fg / wait: wait for a job (fg %n / wait: the newest, or all). */
static long b_wait(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    long st = 0;
    int i, which = -1;
    (void)io;
    if (argc > 1)
        which = atoi(argv[1][0] == '%' ? argv[1] + 1 : argv[1]) - 1;
    for (i = 31; i >= 0; i--) {
        if (!sh->jobs[i] || (which >= 0 && i != which))
            continue;
        st = sh->os.wait(sh->os.data, sh->jobs[i]);
        sh->jobs[i] = 0;
        free(sh->job_text[i]);
        sh->job_text[i] = 0;
        if (!strcmp(argv[0], "fg"))
            break;
    }
    return st;
}

static long b_source(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_type(sh_shell *sh, int argc, char **argv, const sh_io *io);

static const struct {
    const char *name;
    builtin_fn fn;
} builtins[] = {
    { ":", b_true }, { "true", b_true }, { "false", b_false }, { "echo", b_echo },
    { "cd", b_cd }, { "pwd", b_pwd }, { "export", b_export }, { "unset", b_unset },
    { "set", b_set }, { "shift", b_shift }, { "exit", b_exit }, { "return", b_return },
    { "break", b_break }, { "continue", b_continue }, { "read", b_read }, { "alias", b_alias },
    { "unalias", b_unalias }, { "test", b_test }, { "[", b_test }, { "jobs", b_jobs },
    { "wait", b_wait }, { "fg", b_wait }, { "source", b_source }, { ".", b_source },
    { "which", b_type }, { 0, 0 } /* no "type": AmigaDOS Type prints files */
};

static builtin_fn find_builtin(const char *name)
{
    int i;
    for (i = 0; builtins[i].name; i++)
        if (!strcmp(builtins[i].name, name))
            return builtins[i].fn;
    return 0;
}

static sh_func *find_func(sh_shell *sh, const char *name)
{
    sh_func *f;
    for (f = sh->funcs; f; f = f->next)
        if (!strcmp(f->name, name))
            return f;
    return 0;
}

static long b_type(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i;
    for (i = 1; i < argc; i++) {
        say(sh, io->out, argv[i]);
        say(sh, io->out, find_func(sh, argv[i]) ? " is a function\n"
                          : find_builtin(argv[i]) ? " is a shell builtin\n" : " is a command\n");
    }
    return 0;
}

static long b_source(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    sh_fh fh;
    char line[1024];
    char *text = 0;
    long len = 0, n, st = 0;
    int incomplete = 0;
    if (argc < 2)
        return 2;
    fh = sh->os.open(sh->os.data, argv[1], SH_OPEN_READ);
    if (!fh) {
        err2(sh, io, argv[1], "cannot open");
        return 1;
    }
    while ((n = sh->os.read_line(sh->os.data, fh, line, sizeof(line))) >= 0) {
        char *t = (char *)realloc(text, len + n + 1);
        if (!t)
            break;
        text = t;
        memcpy(text + len, line, n);
        len += n;
        text[len] = 0;
    }
    sh->os.close(sh->os.data, fh);
    if (text)
        st = sh_run_text(sh, text, &incomplete);
    free(text);
    return st;
}

/* ---- commands ----------------------------------------------------------------- */

static long exec_node(sh_shell *sh, const sh_node *n, const sh_io *io);

/* Alias expansion of the first word: its value's words go in front. */
static void apply_alias(sh_shell *sh, sh_list *argv)
{
    int k, depth;
    for (depth = 0; depth < 8 && argv->n; depth++) {
        size_t n = strlen(argv->v[0]);
        sh_list nw;
        char *val, *p, *tok;
        for (k = 0; k < sh->aliases.n; k++)
            if (!strncmp(sh->aliases.v[k], argv->v[0], n) && sh->aliases.v[k][n] == '=')
                break;
        if (k == sh->aliases.n)
            return;
        val = sdup(sh->aliases.v[k] + n + 1);
        memset(&nw, 0, sizeof(nw));
        for (p = val; (tok = strtok(p, " \t")) != 0; p = 0)
            sh_list_add(&nw, tok);
        free(val);
        for (k = 1; k < argv->n; k++)
            sh_list_add(&nw, argv->v[k]);
        if (nw.n && !strcmp(nw.v[0], argv->v[0])) {
            sh_list_free(argv);
            *argv = nw;
            return; /* alias ls='ls -l': not expanded again */
        }
        sh_list_free(argv);
        *argv = nw;
    }
}

static long run_function(sh_shell *sh, sh_func *f, sh_list *argv, const sh_io *io)
{
    sh_list saved = sh->ctx.args;
    int i;
    long st;
    memset(&sh->ctx.args, 0, sizeof(sh->ctx.args));
    for (i = 1; i < argv->n; i++)
        sh_list_add(&sh->ctx.args, argv->v[i]);
    sh->func_depth++;
    st = exec_node(sh, f->body, io);
    if (sh->returning)
        st = sh->ctx.status;
    sh->returning = 0;
    sh->func_depth--;
    sh_list_free(&sh->ctx.args);
    sh->ctx.args = saved;
    return st;
}

/* A simple command. wait = 0: start it in the background if it is an
 * external command (*job gets its id), else run it now. */
static long exec_cmd(sh_shell *sh, const sh_node *n, const sh_io *parent, int wait, long *job)
{
    sh_list argv;
    sh_io io;
    const sh_word *a;
    builtin_fn b;
    sh_func *f;
    long st;
    memset(&argv, 0, sizeof(argv));
    if (job)
        *job = 0;
    if (expand_words(sh, n->words, &argv, parent)) {
        sh_list_free(&argv);
        return 1;
    }
    if (!argv.n) {
        /* only assignments (and redirections): they set shell variables */
        for (a = n->assigns; a; a = a->next) {
            char *v = expand_one(sh, strchr(a->text, '=') + 1, parent);
            char name[128];
            size_t len = (size_t)(strchr(a->text, '=') - a->text);
            if (v && len < sizeof(name)) {
                memcpy(name, a->text, len);
                name[len] = 0;
                sh_set(&sh->ctx, name, v);
            }
            free(v);
        }
        if (n->redirs && !redirect(sh, n->redirs, parent, &io))
            close_owned(sh, &io);
        sh_list_free(&argv);
        return 0;
    }
    apply_alias(sh, &argv);
    if (redirect(sh, n->redirs, parent, &io)) {
        sh_list_free(&argv);
        return 1;
    }
    /* assignments before a command: exported for it (and kept, simpler
     * than the POSIX "only for this command"; documented) */
    for (a = n->assigns; a; a = a->next) {
        char *v = expand_one(sh, strchr(a->text, '=') + 1, parent);
        char name[128];
        size_t len = (size_t)(strchr(a->text, '=') - a->text);
        if (v && len < sizeof(name)) {
            memcpy(name, a->text, len);
            name[len] = 0;
            sh_set(&sh->ctx, name, v);
            sh_export(&sh->ctx, name);
        }
        free(v);
    }
    if ((f = find_func(sh, argv.v[0])) != 0) {
        st = run_function(sh, f, &argv, &io);
        close_owned(sh, &io);
    } else if ((b = find_builtin(argv.v[0])) != 0) {
        st = b(sh, argv.n, argv.v, &io);
        close_owned(sh, &io);
    } else {
        if (!wait) {
            /* started in the background, the command keeps the streams the
             * pipeline opened for it (its pipe ends): the runner closes them
             * when it ends. The shell's own streams are never handed over. */
            if (io.in == parent->in && (parent->owned & SH_OWN_IN))
                io.owned |= SH_OWN_IN;
            if (io.out == parent->out && (parent->owned & SH_OWN_OUT))
                io.owned |= SH_OWN_OUT;
            if (io.err == parent->err && (parent->owned & SH_OWN_ERR))
                io.owned |= SH_OWN_ERR;
        }
        st = sh->os.run(sh->os.data, argv.v, &io, wait);
        if (!wait) {
            if (job)
                *job = st > 0 ? st : 0;
            st = st > 0 ? 0 : 1;
        } else if (st < 0) {
            err2(sh, &io, argv.v[0], "not found");
            st = 127;
        }
    }
    sh_list_free(&argv);
    return st;
}

/* A pipeline's stages, left to right. */
static int stages(const sh_node *n, const sh_node **out, int max)
{
    int k;
    if (n->kind != SH_PIPE) {
        out[0] = n;
        return 1;
    }
    k = stages(n->a, out, max - 1);
    if (k < max)
        out[k++] = n->b;
    return k;
}

static int is_external(sh_shell *sh, const sh_node *n)
{
    sh_list argv;
    int ext;
    const char *err;
    if (n->kind != SH_CMD || !n->words)
        return 0;
    memset(&argv, 0, sizeof(argv));
    if (sh_expand(&sh->ctx, n->words->text, 0, &argv, &err) || !argv.n) {
        sh_list_free(&argv);
        return 0;
    }
    ext = !find_builtin(argv.v[0]) && !find_func(sh, argv.v[0]);
    sh_list_free(&argv);
    return ext;
}

static long exec_pipeline(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    const sh_node *st[16];
    sh_io sio[16];
    long job[16];
    int k = stages(n, st, 16), i;
    long status = 0;
    for (i = 0; i < k; i++) {
        sio[i] = *io;
        sio[i].owned = 0;
    }
    for (i = 0; i + 1 < k; i++) {
        sh_fh rd, wr;
        if (sh->os.pipe(sh->os.data, &rd, &wr)) {
            err2(sh, io, "pipe", "cannot create");
            return 1;
        }
        sio[i].out = wr;
        sio[i].owned |= SH_OWN_OUT;
        sio[i + 1].in = rd;
        sio[i + 1].owned |= SH_OWN_IN;
    }
    /* external stages start first, all at once; then the in-process ones
     * (builtins, compound commands) run in order, feeding or draining them */
    for (i = 0; i < k; i++) {
        job[i] = 0;
        if (is_external(sh, st[i]))
            exec_cmd(sh, st[i], &sio[i], 0, &job[i]);
    }
    for (i = 0; i < k; i++) {
        if (job[i])
            continue;
        if (is_external(sh, st[i])) {
            /* it did not start; the OS layer took its streams all the same */
            char *name = expand_one(sh, st[i]->words->text, io);
            err2(sh, io, name ? name : "?", "not found");
            free(name);
            status = 127;
            continue;
        }
        status = exec_node(sh, st[i], &sio[i]);
        close_owned(sh, &sio[i]);
    }
    for (i = 0; i < k; i++)
        if (job[i]) {
            long s = sh->os.wait(sh->os.data, job[i]);
            if (i == k - 1)
                status = s;
        }
    return status;
}

static long exec_list_loop(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    long st = 0;
    int until = n->kind == SH_UNTIL;
    sh->loop_depth++;
    for (;;) {
        long c = exec_node(sh, n->a, io);
        if (sh->exiting || sh->returning)
            break;
        if ((c == 0) == until)
            break;
        st = exec_node(sh, n->b, io);
        if (sh->breaking) {
            sh->breaking--;
            break;
        }
        sh->continuing = 0;
        if (sh->exiting || sh->returning)
            break;
    }
    sh->loop_depth--;
    return st;
}

static long exec_for(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    sh_list items;
    long st = 0;
    int i;
    memset(&items, 0, sizeof(items));
    if (n->has_in) {
        if (expand_words(sh, n->words, &items, io)) {
            sh_list_free(&items);
            return 1;
        }
    } else {
        for (i = 0; i < sh->ctx.args.n; i++)
            sh_list_add(&items, sh->ctx.args.v[i]);
    }
    sh->loop_depth++;
    for (i = 0; i < items.n; i++) {
        sh_set(&sh->ctx, n->name, items.v[i]);
        st = exec_node(sh, n->a, io);
        if (sh->breaking) {
            sh->breaking--;
            break;
        }
        sh->continuing = 0;
        if (sh->exiting || sh->returning)
            break;
    }
    sh->loop_depth--;
    sh_list_free(&items);
    return st;
}

static long exec_case(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    char *subject = expand_one(sh, n->words->text, io);
    const sh_case *c;
    const sh_word *p;
    long st = 0;
    if (!subject)
        return 1;
    for (c = n->cases; c; c = c->next)
        for (p = c->patterns; p; p = p->next) {
            char *pat = expand_one(sh, p->text, io);
            int hit = pat && sh_match(pat, subject, 0);
            free(pat);
            if (hit) {
                st = exec_node(sh, c->body, io);
                free(subject);
                return st;
            }
        }
    free(subject);
    return st;
}

static long exec_node(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    long st = 0;
    sh_io rio;
    if (!n || sh->exiting)
        return sh->ctx.status;
    switch (n->kind) {
    case SH_CMD:
        st = exec_cmd(sh, n, io, 1, 0);
        break;
    case SH_SEQ:
        st = exec_node(sh, n->a, io);
        sh->ctx.status = st;
        if (!sh->exiting && !sh->breaking && !sh->continuing && !sh->returning && n->b)
            st = exec_node(sh, n->b, io);
        break;
    case SH_BG: {
        long job = 0;
        if (n->a && n->a->kind == SH_CMD) {
            exec_cmd(sh, n->a, io, 0, &job);
        } else {
            exec_node(sh, n->a, io); /* a compound command runs now (no fork) */
        }
        if (job) {
            int i;
            char nb[16];
            for (i = 0; i < 32 && sh->jobs[i]; i++)
                ;
            if (i < 32) {
                sh->jobs[i] = job;
                free(sh->job_text[i]);
                sh->job_text[i] = sdup(n->a->words ? n->a->words->text : "");
                num(nb, i + 1);
                say(sh, io->err, "[");
                say(sh, io->err, nb);
                say(sh, io->err, "] ");
                num(nb, job);
                say(sh, io->err, nb);
                say(sh, io->err, "\n");
            }
            sh->ctx.last_bg = job;
        }
        st = 0;
        sh->ctx.status = st;
        if (n->b)
            st = exec_node(sh, n->b, io);
        break;
    }
    case SH_AND:
        st = exec_node(sh, n->a, io);
        if (!st && !sh->exiting)
            st = exec_node(sh, n->b, io);
        break;
    case SH_OR:
        st = exec_node(sh, n->a, io);
        if (st && !sh->exiting)
            st = exec_node(sh, n->b, io);
        break;
    case SH_NOT:
        st = !exec_node(sh, n->a, io);
        break;
    case SH_PIPE:
        st = exec_pipeline(sh, n, io);
        break;
    case SH_SUBSHELL: {
        /* no fork: the directory is restored afterwards, variables are not
         * (documented limitation until subshells run as a process, S3.4) */
        char *dir = sh->os.cwd(sh->os.data);
        if (redirect(sh, n->redirs, io, &rio))
            st = 1;
        else {
            st = exec_node(sh, n->a, &rio);
            close_owned(sh, &rio);
        }
        if (dir)
            sh->os.chdir(sh->os.data, dir);
        free(dir);
        break;
    }
    case SH_GROUP:
        if (redirect(sh, n->redirs, io, &rio))
            st = 1;
        else {
            st = exec_node(sh, n->a, &rio);
            close_owned(sh, &rio);
        }
        break;
    case SH_IF:
        if (!exec_node(sh, n->a, io))
            st = exec_node(sh, n->b, io);
        else if (n->c)
            st = exec_node(sh, n->c, io);
        else
            st = 0;
        break;
    case SH_WHILE:
    case SH_UNTIL:
        st = exec_list_loop(sh, n, io);
        break;
    case SH_FOR:
        st = exec_for(sh, n, io);
        break;
    case SH_CASE:
        st = exec_case(sh, n, io);
        break;
    case SH_FUNC: {
        sh_func *f = find_func(sh, n->name);
        if (!f) {
            f = (sh_func *)calloc(1, sizeof(sh_func));
            if (!f)
                break;
            f->name = sdup(n->name);
            f->next = sh->funcs;
            sh->funcs = f;
        }
        f->body = n->a;
        sh->keep_parse = 1; /* the body lives in this parse: keep it */
        st = 0;
        break;
    }
    }
    sh->ctx.status = st;
    return st;
}

long sh_exec(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    return exec_node(sh, n, io);
}

long sh_run_text(sh_shell *sh, const char *text, int *incomplete)
{
    sh_parse p;
    long st;
    sh_parse_text(&p, text);
    if (incomplete)
        *incomplete = p.incomplete;
    if (p.error) {
        if (!p.incomplete)
            err2(sh, &sh->io, p.error, 0);
        sh_parse_free(&p);
        sh->ctx.status = p.incomplete ? sh->ctx.status : 2;
        return sh->ctx.status;
    }
    sh->keep_parse = 0;
    st = exec_node(sh, p.tree, &sh->io);
    if (sh->keep_parse && sh->n_kept < 16)
        sh->kept[sh->n_kept++] = p;
    else
        sh_parse_free(&p);
    return st;
}
