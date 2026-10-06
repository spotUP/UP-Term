/* vsh's executor (see sh_exec.h). */
#include <stdlib.h>
#include <string.h>
#include "sh_exec.h"
#include "../tty/bmsg.h"


/* The Unix device names scripts use, as AmigaDOS has them: /dev/null is
 * NIL:, /dev/tty the console ("*"). Any other name is passed as it is. */
static const char *dev_name(const char *path)
{
    if (!strcmp(path, "/dev/null"))
        return "NIL:";
    if (!strcmp(path, "/dev/tty"))
        return "*";
    return path;
}
int sh_unix_root(const char *in, char *out, long max)
{
    long i = 1, o = 0, n = (long)strlen(in);
    if (n < 2 || in[0] != '/' || in[1] == '/' || n + 2 > max)
        return 0;
    while (i < n && in[i] != '/')
        out[o++] = in[i++];
    out[o++] = ':';
    if (i < n)
        i++;
    while (i < n)
        out[o++] = in[i++];
    out[o] = 0;
    return 1;
}

int sh_path_next(const char **p, char *dir, long max)
{
    while (*p) {
        const char *e = *p, *end = strchr(e, ':');
        long n = end ? (long)(end - e) : (long)strlen(e);
        long o = 0;
        *p = end ? end + 1 : 0;
        if (n == 1 && (*e == '/' || *e == '.')) {
            if (*e == '/')
                continue;     /* the volume list holds no commands */
            n = 0;            /* "." */
        }
        if (n + 2 > max)
            continue;
        if (n && *e == '/') { /* /vol/rest: vol:rest */
            char entry[256];
            if (n >= (long)sizeof(entry))
                continue;
            memcpy(entry, e, (size_t)n);
            entry[n] = 0;
            if (!sh_unix_root(entry, dir, max))
                continue;
            o = (long)strlen(dir);
        } else {
            memcpy(dir, e, (size_t)n);
            o = n;
        }
        dir[o] = 0;
        return 1;
    }
    return 0;
}

static char *sdup(const char *s)
{
    size_t n = strlen(s);
    char *r = (char *)malloc(n + 1);
    if (r)
        memcpy(r, s, n + 1);
    return r;
}

/* A growing string. */
typedef struct pbuf {
    char *s;
    long n, cap;
} pbuf;

static void pb_add(pbuf *b, const char *s, long n)
{
    if (b->n + n + 1 > b->cap) {
        long cap = (b->n + n + 1) * 2;
        char *t = (char *)realloc(b->s, cap);
        if (!t)
            return;
        b->s = t;
        b->cap = cap;
    }
    memcpy(b->s + b->n, s, n);
    b->n += n;
    b->s[b->n] = 0;
}

static void pb_str(pbuf *b, const char *s)
{
    pb_add(b, s, (long)strlen(s));
}

static char *core_subst(sh_ctx *c, const char *cmd);

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
    memset(sh->job_stopped, 0, sizeof(sh->job_stopped));
    sh->warned_stopped = 0;
    sh->retired = 0;
    sh->intr = 0;
    sh->stack_limit = 0; /* the OS layer sets it for each process */
    sh->subst_ran = 0;
    sh->subst_status = 0;
    sh->ctx.subst = core_subst;
    sh->ctx.user = sh;
    sh->heredocs = 0;
    memset(sh->traps, 0, sizeof(sh->traps));
    sh->in_trap = 0;
    sh->exit_trap_ran = 0;
    sh->locals = 0;
    sh->n_locals = sh->cap_locals = 0;
    sh->umask = 022;
    sh->optpos = 0;
    sh->optind_seen = 0;
}

/* A variable as it was before NAME=value cmd or local NAME, to put back after. */
typedef struct saved_var {
    char *name, *value;     /* value 0: it was not set */
    int exported;
} saved_var;
static void restore_var(sh_shell *sh, saved_var *s);

void sh_shell_free(sh_shell *sh)
{
    int i;
    while (sh->funcs) {
        sh_func *f = sh->funcs;
        sh->funcs = f->next;
        free(f->name);
        sh_parse_free(&f->body);
        free(f);
    }
    while (sh->retired) {
        sh_retired *r = sh->retired;
        sh->retired = r->next;
        sh_parse_free(&r->p);
        free(r);
    }
    for (i = 0; i < 32; i++)
        free(sh->job_text[i]);
    for (i = 0; i < 3; i++)
        free(sh->traps[i]);
    while (sh->n_locals > 0) /* a shell ended inside a function: the values go with it */
        restore_var(sh, (saved_var *)sh->locals + --sh->n_locals);
    free(sh->locals);
    sh_list_free(&sh->aliases);
    sh_ctx_free(&sh->ctx);
}

sh_shell *sh_shell_clone(const sh_shell *sh)
{
    sh_shell *c = (sh_shell *)malloc(sizeof(sh_shell));
    const sh_var *v;
    const sh_func *f;
    sh_func **tail;
    int i;
    if (!c)
        return 0;
    sh_shell_init(c);
    c->os = sh->os;
    c->io = sh->io;
    c->io.owned = 0;
    for (v = sh->ctx.vars; v; v = v->next) {
        sh_set(&c->ctx, v->name, v->value);
        if (v->exported)
            sh_export(&c->ctx, v->name);
    }
    for (i = 0; i < sh->ctx.args.n; i++)
        sh_list_add(&c->ctx.args, sh->ctx.args.v[i]);
    c->ctx.arg0 = sh->ctx.arg0;  /* not owned by a ctx */
    c->ctx.status = sh->ctx.status;
    c->ctx.pid = sh->ctx.pid;
    c->ctx.last_bg = sh->ctx.last_bg;
    c->umask = sh->umask;  /* traps are not inherited (POSIX) */
    c->ctx.nocase = sh->ctx.nocase;
    c->ctx.listdir = sh->ctx.listdir;
    c->ctx.subst = sh->ctx.subst;
    c->ctx.user = sh->ctx.user == (const void *)sh ? (void *)c : sh->ctx.user;
    tail = &c->funcs;
    for (f = sh->funcs; f; f = f->next) {
        sh_func *nf = (sh_func *)calloc(1, sizeof(sh_func));
        if (!nf)
            break;
        nf->name = sdup(f->name);
        sh_parse_copy(f->body.tree, &nf->body);
        *tail = nf;
        tail = &nf->next;
    }
    for (i = 0; i < sh->aliases.n; i++)
        sh_list_add(&c->aliases, sh->aliases.v[i]);
    return c;
}

static long exec_node(sh_shell *sh, const sh_node *n, const sh_io *io);
static void close_owned(sh_shell *sh, const sh_io *io);

static long intr_status(const sh_shell *sh);

long sh_run_child(sh_shell *child, sh_parse *tree, const sh_io *io)
{
    long st = exec_node(child, tree ? tree->tree : 0, io);
    int i;
    if (child->exiting)
        st = child->exit_status;
    else if (child->intr)
        st = intr_status(child);
    child->ctx.status = st;
    sh_exit_trap(child);
    if (child->exiting)
        st = child->exit_status;
    close_owned(child, io);
    /* its own background jobs report to it: it waits for them */
    for (i = 0; i < 32; i++)
        if (child->jobs[i])
            child->os.wait(child->os.data, child->jobs[i]);
    if (tree) {
        sh_parse_free(tree);
        free(tree);
    }
    sh_shell_free(child);
    free(child);
    return st;
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

/* A command that did not start. One of the kit's own (ssh, sort, sz, ...)
 * says which drawer it lives in, so the fix is in the message. */
static void err_not_found(sh_shell *sh, const sh_io *io, const char *name)
{
    const char *drawer = bmsg_kit_drawer(name);
    say(sh, io->err, "vsh: ");
    say(sh, io->err, name);
    say(sh, io->err, ": not found");
    if (drawer) {
        say(sh, io->err, " (it lives in ");
        say(sh, io->err, drawer);
        say(sh, io->err, ": put that drawer on PATH, or run the UP-Term Install)");
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

/* A simple command's words as written, for jobs and notices. */
static char *words_text(const sh_node *c)
{
    pbuf b = { 0, 0, 0 };
    const sh_word *w;
    for (w = c->words; w; w = w->next) {
        if (b.n)
            pb_add(&b, " ", 1);
        pb_str(&b, w->text);
    }
    return b.s ? b.s : sdup("");
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
            fh = sh->os.open(sh->os.data, dev_name(path), mode);
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

/* printf: one backslash escape at *pp (on the backslash); %b's octal
 * form is \0nnn. Returns 1 at \c (stop all output). */
static int pf_escape(pbuf *b, const char **pp, int is_b)
{
    const char *p = *pp + 1;
    char c = *p, out;
    int n = 0, v = 0;
    switch (c) {
    case 'a': out = 7; break;
    case 'b': out = 8; break;
    case 'e': out = 27; break;
    case 'f': out = 12; break;
    case 'n': out = 10; break;
    case 'r': out = 13; break;
    case 't': out = 9; break;
    case 'v': out = 11; break;
    case 'c':
        if (is_b) {
            *pp = p;
            return 1;
        }
        out = c;
        break;
    case 'x':
        while (n < 2 && ((p[1] >= '0' && p[1] <= '9') || ((p[1] | 32) >= 'a' && (p[1] | 32) <= 'f'))) {
            p++;
            v = v * 16 + (*p <= '9' ? *p - '0' : (*p | 32) - 'a' + 10);
            n++;
        }
        out = (char)v;
        if (!n) {
            pb_add(b, "\\", 1);
            out = 'x';
        }
        break;
    case 0:
        pb_add(b, "\\", 1);
        *pp = p - 1;
        return 0;
    default:
        if (c >= '0' && c <= '7') {
            if (is_b && c == '0')
                p++;
            else
                p--;
            while (n < 3 && p[1] >= '0' && p[1] <= '7') {
                p++;
                v = v * 8 + (*p - '0');
                n++;
            }
            out = (char)v;
        } else if (c == '\\' || c == '"' || c == '\'') {
            out = c;
        } else {
            pb_add(b, "\\", 1);
            out = c;
        }
    }
    pb_add(b, &out, 1);
    *pp = p;
    return 0;
}

/* printf: a numeric argument ('c gives the character's value) */
static long pf_number(sh_shell *sh, const sh_io *io, const char *a, int *bad)
{
    char *end;
    long v;
    if (!a || !*a)
        return 0;
    if (*a == '\'' || *a == '"')
        return (unsigned char)a[1];
    v = strtol(a, &end, 0);
    if (*end || end == a) {
        char *m = (char *)malloc(strlen(a) + 10);
        if (m) {
            strcpy(m, "printf: ");
            strcat(m, a);
            err2(sh, io, m, "invalid number");
            free(m);
        }
        *bad = 1;
    }
    return v;
}

/* printf: body (after its prefix of pl characters: sign, 0x) in a field */
static void pf_field(pbuf *b, const char *body, int pl, long width, int left, int zero)
{
    long len = (long)strlen(body), pad = width > len ? width - len : 0;
    if (left) {
        pb_str(b, body);
        for (; pad > 0; pad--)
            pb_add(b, " ", 1);
        return;
    }
    if (zero) {
        pb_add(b, body, pl);
        for (; pad > 0; pad--)
            pb_add(b, "0", 1);
        pb_str(b, body + pl);
        return;
    }
    for (; pad > 0; pad--)
        pb_add(b, " ", 1);
    pb_str(b, body);
}

static long b_printf(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    pbuf b = { 0, 0, 0 };
    int ai = 2, bad = 0, stop = 0;
    const char *p;
    if (argc < 2) {
        err2(sh, io, "printf", "usage: printf format [arguments]");
        return 2;
    }
    do {
        int used = ai;
        for (p = argv[1]; *p && !stop; p++) {
            const char *arg;
            char spec, body[80], digits[24];
            int left = 0, plus = 0, space = 0, alt = 0, zero = 0, pl = 0;
            long width = 0, prec = -1;
            if (*p == '\\') {
                pf_escape(&b, &p, 0);
                continue;
            }
            if (*p != '%') {
                pb_add(&b, p, 1);
                continue;
            }
            if (p[1] == '%') {
                pb_add(&b, "%", 1);
                p++;
                continue;
            }
            for (p++; *p && strchr("-+ #0", *p); p++)
                switch (*p) {
                case '-': left = 1; break;
                case '+': plus = 1; break;
                case ' ': space = 1; break;
                case '#': alt = 1; break;
                default: zero = 1; break;
                }
            if (*p == '*') {
                width = pf_number(sh, io, ai < argc ? argv[ai] : 0, &bad);
                ai++;
                p++;
                if (width < 0) {
                    left = 1;
                    width = -width;
                }
            } else
                for (; *p >= '0' && *p <= '9'; p++)
                    width = width * 10 + (*p - '0');
            if (*p == '.') {
                prec = 0;
                if (*++p == '*') {
                    prec = pf_number(sh, io, ai < argc ? argv[ai] : 0, &bad);
                    ai++;
                    p++;
                } else
                    for (; *p >= '0' && *p <= '9'; p++)
                        prec = prec * 10 + (*p - '0');
            }
            if (width > 4096)
                width = 4096;
            spec = *p;
            if (!spec)
                break;
            arg = ai < argc ? argv[ai] : 0;
            ai++;
            if (spec == 's' || spec == 'b' || spec == 'c') {
                pbuf t = { 0, 0, 0 };
                const char *a = arg ? arg : "";
                if (spec == 'b') {
                    for (; *a && !stop; a++)
                        if (*a == '\\')
                            stop = pf_escape(&t, &a, 1);
                        else
                            pb_add(&t, a, 1);
                } else
                    pb_add(&t, a, spec == 'c' ? (*a ? 1 : 0) : (long)strlen(a));
                if (!t.s)
                    pb_add(&t, "", 0);
                if (t.s && prec >= 0 && prec < t.n && spec != 'c')
                    t.s[prec] = 0;
                if (t.s)
                    pf_field(&b, t.s, 0, width, left, 0);
                free(t.s);
            } else if (strchr("diouxX", spec)) {
                long v = pf_number(sh, io, arg, &bad);
                unsigned long u = (unsigned long)v;
                int base = spec == 'o' ? 8 : (spec == 'x' || spec == 'X') ? 16 : 10;
                int n = 0, k;
                if (spec == 'd' || spec == 'i') {
                    if (v < 0) {
                        body[pl++] = '-';
                        u = (unsigned long)-v;
                    } else if (plus)
                        body[pl++] = '+';
                    else if (space)
                        body[pl++] = ' ';
                } else if (alt && spec != 'o' && u) {
                    body[pl++] = '0';
                    body[pl++] = spec;
                }
                do {
                    int d = (int)(u % base);
                    digits[n++] = (char)(d < 10 ? '0' + d : (spec == 'X' ? 'A' : 'a') + d - 10);
                } while ((u /= base) > 0);
                if (alt && spec == 'o' && digits[n - 1] != '0')
                    digits[n++] = '0';
                for (k = n; k < prec && k < 40; k++)
                    body[pl + k - n] = '0';
                k = pl + (prec > n && prec < 40 ? (int)prec - n : 0);
                while (n)
                    body[k++] = digits[--n];
                body[k] = 0;
                pf_field(&b, body, pl, width, left, zero && prec < 0);
            } else {
                body[0] = '%';
                body[1] = spec;
                body[2] = 0;
                pb_str(&b, body);  /* not a conversion: printed as it is */
                ai--;
            }
        }
        if (ai == used)
            break;  /* the format takes no arguments: once */
    } while (ai < argc && !stop);
    if (b.s)
        sh->os.write(sh->os.data, io->out, b.s, b.n);
    free(b.s);
    return bad;
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
    int i;
    /* a stopped job would wait for a SIGCONT for ever: say so once */
    for (i = 0; i < 32; i++)
        if (sh->jobs[i] && sh->job_stopped[i] && !sh->warned_stopped) {
            sh->warned_stopped = 1;
            err2(sh, io, "exit", "there are stopped jobs (fg or bg them; exit again to leave them)");
            return 1;
        }
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

static int in_ifs(const char *ifs, char c)
{
    return c && strchr(ifs, c) != 0;
}

static int ifs_space(const char *ifs, char c)
{
    return (c == ' ' || c == '\t' || c == '\n') && in_ifs(ifs, c);
}

static void set_part(sh_shell *sh, const char *name, const char *s, long n)
{
    char *v = (char *)malloc(n + 1);
    if (!v)
        return;
    memcpy(v, s, n);
    v[n] = 0;
    sh_set(&sh->ctx, name, v);
    free(v);
}

/* read [-r] [name ...]: one line, split by $IFS (IFS whitespace trimmed
 * and collapsed; other IFS characters end one field each), the last name
 * takes the rest. Without -r a backslash quotes the next character (and
 * joins the next line at the end). No names: REPLY. */
static long b_read(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    char line[1024], quoted[1024], buf[1024];
    const char *ifs = sh_get(&sh->ctx, "IFS");
    const char *reply[1];
    char **names = argv;
    long n = 0, m, k, p = 0;
    int raw = 0, a = 1, i, got = 0;
    if (!ifs)
        ifs = " \t\n";
    while (a < argc && !strcmp(argv[a], "-r")) {
        raw = 1;
        a++;
    }
    if (a == argc) {
        reply[0] = "REPLY";
        names = (char **)reply;
        a = 0;
        argc = 1;
    }
    for (;;) {
        int more = 0;
        m = sh->os.read_line(sh->os.data, io->in, buf, sizeof(buf));
        if (m < 0)
            break;
        got = 1;
        while (m > 0 && (buf[m - 1] == '\n' || buf[m - 1] == '\r'))
            m--;
        for (k = 0; k < m && n < (long)sizeof(line) - 1; k++) {
            if (!raw && buf[k] == '\\') {
                if (k + 1 == m) {
                    more = 1; /* backslash-newline: the line goes on */
                    break;
                }
                line[n] = buf[++k];
                quoted[n++] = 1;
            } else {
                line[n] = buf[k];
                quoted[n++] = 0;
            }
        }
        if (!more)
            break;
    }
    line[n] = 0;
    for (i = a; i < argc; i++) {
        long start;
        while (p < n && !quoted[p] && ifs_space(ifs, line[p]))
            p++;
        start = p;
        if (i + 1 == argc) {
            long e = n;
            while (e > p && !quoted[e - 1] && ifs_space(ifs, line[e - 1]))
                e--;
            set_part(sh, names[i], line + start, e - start);
            break;
        }
        while (p < n && (quoted[p] || !in_ifs(ifs, line[p])))
            p++;
        set_part(sh, names[i], line + start, p - start);
        while (p < n && !quoted[p] && ifs_space(ifs, line[p]))
            p++;
        if (p < n && !quoted[p] && in_ifs(ifs, line[p]))
            p++; /* one non-space IFS character ends the field */
    }
    return got ? 0 : 1;
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
        else if (!strcmp(a[0], "-t")) {
            /* the test command's own stream 0, 1 or 2 is a terminal */
            int fd = atoi(a[1]);
            sh_fh fh = fd == 0 ? io->in : fd == 1 ? io->out : fd == 2 ? io->err : SH_NOFH;
            r = fh != SH_NOFH && sh->os.isatty && sh->os.isatty(sh->os.data, fh);
        } else {
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

/* A job's line in jobs and notices: "[n] <state>  <command>". */
static void job_line(sh_shell *sh, sh_fh fh, int i, const char *state, long st)
{
    char n[16];
    num(n, i + 1);
    say(sh, fh, "[");
    say(sh, fh, n);
    say(sh, fh, "] ");
    say(sh, fh, state);
    if (st > 0) {
        num(n, st);
        say(sh, fh, " ");
        say(sh, fh, n);
    }
    say(sh, fh, "  ");
    say(sh, fh, sh->job_text[i] ? sh->job_text[i] : "");
    say(sh, fh, "\n");
}

static void job_forget(sh_shell *sh, int i)
{
    sh->jobs[i] = 0;
    sh->job_stopped[i] = 0;
    free(sh->job_text[i]);
    sh->job_text[i] = 0;
}

/* A job that has ended: its status collected, reported, forgotten. */
static int job_reap(sh_shell *sh, int i, sh_fh fh)
{
    long st;
    if (!sh->jobs[i] || !sh->os.done || !sh->os.done(sh->os.data, sh->jobs[i]))
        return 0;
    st = sh->os.wait(sh->os.data, sh->jobs[i]);
    job_line(sh, fh, i, st ? "Exit" : "Done", st);
    job_forget(sh, i);
    return 1;
}

void sh_notify(sh_shell *sh)
{
    int i;
    for (i = 0; i < 32; i++)
        job_reap(sh, i, sh->io.err);
}

static long b_jobs(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i;
    (void)argc; (void)argv;
    for (i = 0; i < 32; i++)
        if (sh->jobs[i] && !job_reap(sh, i, io->out))
            job_line(sh, io->out, i, sh->job_stopped[i] ? "Stopped" : "Running", 0);
    return 0;
}

/* A command in the foreground: its status, or, when it was suspended,
 * into the table as a stopped job ("[n] Stopped  cmd") and $? 146.
 * slot: its place in the table already (fg), or -1. */
static long fg_status(sh_shell *sh, long st, int slot, char *text, const sh_io *io)
{
    int i = slot;
    if (st != SH_STOPPED) {
        free(text);
        return st;
    }
    if (i < 0) {
        for (i = 0; i < 32 && sh->jobs[i]; i++)
            ;
        if (i == 32) {
            free(text);
            return SH_STATUS_STOPPED; /* no room: it stays stopped, unlisted */
        }
        sh->job_text[i] = text;
    } else {
        free(text);
    }
    sh->jobs[i] = sh->os.stopped;
    sh->job_stopped[i] = 1;
    sh->warned_stopped = 0;
    say(sh, io->err, "\n");
    job_line(sh, io->err, i, "Stopped", 0);
    return SH_STATUS_STOPPED;
}

/* The job a job argument (%n or n) names, or the newest one for which
 * want(sh, i) holds; -1: none. */
static int job_arg(sh_shell *sh, int argc, char **argv, int stopped_only)
{
    int i;
    if (argc > 1) {
        i = atoi(argv[1][0] == '%' ? argv[1] + 1 : argv[1]) - 1;
        return i >= 0 && i < 32 && sh->jobs[i] && (!stopped_only || sh->job_stopped[i]) ? i : -1;
    }
    for (i = 31; i >= 0; i--)
        if (sh->jobs[i] && (!stopped_only || sh->job_stopped[i]))
            return i;
    return -1;
}

/* stack [bytes]: the stack the commands vsh starts get, as the AmigaDOS
 * Stack command sets it for a Shell (which inside vsh would only set it
 * for the command's own process). A command whose file asks for more
 * ($STACK: in it) gets that. */
static long b_stack(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    long n;
    char nb[16];
    if (!sh->os.stack) {
        err2(sh, io, "stack", "not here");
        return 1;
    }
    if (argc > 1) {
        n = atol(argv[1]);
        if (n < 1600) {
            err2(sh, io, "stack", "at least 1600 bytes");
            return 1;
        }
        sh->os.stack(sh->os.data, n);
        return 0;
    }
    num(nb, sh->os.stack(sh->os.data, 0));
    say(sh, io->out, "stack ");
    say(sh, io->out, nb);
    say(sh, io->out, "\n");
    return 0;
}

/* bg: a stopped job goes on, in the background ("[n] cmd &"). */
static long b_bg(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i = job_arg(sh, argc, argv, 1);
    char nb[16];
    if (i < 0 || !sh->os.cont) {
        err2(sh, io, "bg", "no stopped job");
        return 1;
    }
    if (sh->os.cont(sh->os.data, sh->jobs[i])) {
        err2(sh, io, "bg", "cannot continue it");
        return 1;
    }
    sh->job_stopped[i] = 0;
    num(nb, i + 1);
    say(sh, io->out, "[");
    say(sh, io->out, nb);
    say(sh, io->out, "] ");
    say(sh, io->out, sh->job_text[i] ? sh->job_text[i] : "");
    say(sh, io->out, " &\n");
    return 0;
}

/* fg / wait: wait for a job (fg %n / wait: the newest, or all). fg shows
 * the command it brings back, as the Unix shells do. */
static long b_wait(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    long st = 0;
    int i, which = -1, fg = !strcmp(argv[0], "fg");
    if (fg) {
        /* the named job, or the newest; a stopped one is continued first,
         * and may be suspended again */
        i = job_arg(sh, argc, argv, 0);
        if (i < 0) {
            err2(sh, io, "fg", "no such job");
            return 1;
        }
        say(sh, io->out, sh->job_text[i] ? sh->job_text[i] : "");
        say(sh, io->out, "\n");
        if (sh->job_stopped[i]) {
            if (!sh->os.cont || sh->os.cont(sh->os.data, sh->jobs[i])) {
                err2(sh, io, "fg", "cannot continue it");
                return 1;
            }
            sh->job_stopped[i] = 0;
        }
        sh->os.suspendable = sh->os.cont != 0;
        st = sh->os.wait(sh->os.data, sh->jobs[i]);
        sh->os.suspendable = 0;
        if (st == SH_STOPPED)
            return fg_status(sh, st, i, 0, io);
        job_forget(sh, i);
        return st;
    }
    if (argc > 1)
        which = atoi(argv[1][0] == '%' ? argv[1] + 1 : argv[1]) - 1;
    for (i = 31; i >= 0; i--) {
        /* a stopped job is not waited for: it would never end */
        if (!sh->jobs[i] || sh->job_stopped[i] || (which >= 0 && i != which))
            continue;
        st = sh->os.wait(sh->os.data, sh->jobs[i]);
        job_forget(sh, i);
    }
    return st;
}

static long b_source(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_type(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_eval(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_exec(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_trap(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_local(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_getopts(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_umask(sh_shell *sh, int argc, char **argv, const sh_io *io);

static const struct {
    const char *name;
    builtin_fn fn;
} builtins[] = {
    { ":", b_true }, { "true", b_true }, { "false", b_false }, { "echo", b_echo },
    { "printf", b_printf },
    { "cd", b_cd }, { "pwd", b_pwd }, { "export", b_export }, { "unset", b_unset },
    { "set", b_set }, { "shift", b_shift }, { "exit", b_exit }, { "return", b_return },
    { "break", b_break }, { "continue", b_continue }, { "read", b_read }, { "alias", b_alias },
    { "unalias", b_unalias }, { "test", b_test }, { "[", b_test }, { "jobs", b_jobs },
    { "wait", b_wait }, { "fg", b_wait }, { "bg", b_bg }, { "stack", b_stack }, { "source", b_source }, { ".", b_source },
    { "eval", b_eval }, { "exec", b_exec }, { "trap", b_trap }, { "local", b_local },
    { "getopts", b_getopts }, { "umask", b_umask },
    { "which", b_type }, { "type", b_type }, { 0, 0 }
};

static long add_word(char *out, long n, long max, const char *w)
{
    long l = (long)strlen(w) + 1;
    if (n + l > max)
        return n;
    memcpy(out + n, w, l);
    return n + l;
}

long sh_word_list(const sh_shell *sh, int kind, char *out, long max)
{
    long n = 0;
    int i;
    if (kind == SH_WORDS_VARIABLES) {
        const sh_var *v;
        for (v = sh->ctx.vars; v; v = v->next)
            n = add_word(out, n, max, v->name);
        return n;
    }
    for (i = 0; builtins[i].name; i++) {
        char c = builtins[i].name[0];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))  /* not : . [ */
            n = add_word(out, n, max, builtins[i].name);
    }
    {
        const sh_func *f;
        for (f = sh->funcs; f; f = f->next)
            n = add_word(out, n, max, f->name);
    }
    for (i = 0; i < sh->aliases.n; i++) {
        char name[64];
        const char *eq = strchr(sh->aliases.v[i], '=');
        size_t l = eq ? (size_t)(eq - sh->aliases.v[i]) : strlen(sh->aliases.v[i]);
        if (l < sizeof(name)) {
            memcpy(name, sh->aliases.v[i], l);
            name[l] = 0;
            n = add_word(out, n, max, name);
        }
    }
    return n;
}

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

/* The file a command name stands for, the way vsh's resolve() looks: a name
 * with a path as given, else the current directory, then the directories of
 * $PATH, then C:. 1 and the name in out, 0 when no such file. */
static int find_command_file(sh_shell *sh, const char *name, char *out, long max)
{
    const char *p;
    char dir[256];
    if (!sh->os.exists || (long)strlen(name) + 3 > max)
        return 0;
    if (strchr(name, ':') || strchr(name, '/') || sh->os.exists(sh->os.data, name, 0)) {
        if (!sh->os.exists(sh->os.data, name, 0))
            return 0;
        strcpy(out, name);
        return 1;
    }
    p = sh_get(&sh->ctx, "PATH");
    while (p && sh_path_next(&p, dir, sizeof(dir))) {
        long n = (long)strlen(dir);
        if (!n)
            continue;   /* the current directory: looked at above */
        if (n + (long)strlen(name) + 2 > max)
            continue;
        strcpy(out, dir);
        if (dir[n - 1] != ':' && dir[n - 1] != '/')
            strcat(out, "/");
        strcat(out, name);
        if (sh->os.exists(sh->os.data, out, 0))
            return 1;
    }
    strcpy(out, "C:");
    strcat(out, name);
    return sh->os.exists(sh->os.data, out, 0);
}

/* type NAME ... ("NAME is a function", "is a shell builtin", "is /path"),
 * and which: the same, but a command prints just its path. Status 1 when
 * a name is none of them (type says so on stderr, which stays quiet). */
static long b_type(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i, bare = !strcmp(argv[0], "which");
    long st = 0;
    for (i = 1; i < argc; i++) {
        char path[512];
        if (find_func(sh, argv[i]) || find_builtin(argv[i])) {
            say(sh, io->out, argv[i]);
            say(sh, io->out, find_func(sh, argv[i]) ? " is a function\n" : " is a shell builtin\n");
        } else if (find_command_file(sh, argv[i], path, sizeof(path))) {
            if (!bare) {
                say(sh, io->out, argv[i]);
                say(sh, io->out, " is ");
            }
            say(sh, io->out, path);
            say(sh, io->out, "\n");
        } else {
            if (!bare)
                err2(sh, io, argv[i], "not found");
            st = 1;
        }
    }
    return st;
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
    int i, mark = sh->n_locals;
    long st;
    memset(&sh->ctx.args, 0, sizeof(sh->ctx.args));
    for (i = 1; i < argv->n; i++)
        sh_list_add(&sh->ctx.args, argv->v[i]);
    sh->func_depth++;
    f->busy++;
    st = exec_node(sh, f->body.tree, io);
    f->busy--;
    if (sh->returning)
        st = sh->ctx.status;
    sh->returning = 0;
    sh->func_depth--;
    while (sh->n_locals > mark) /* its locals end with it */
        restore_var(sh, (saved_var *)sh->locals + --sh->n_locals);
    sh_list_free(&sh->ctx.args);
    sh->ctx.args = saved;
    return st;
}

/* A simple command. wait = 0: start it in the background if it is an
 * external command (*job gets its id), else run it now. */
/* NAME of an assignment word NAME=value, malloc'ed (on the heap, not in
 * exec_cmd's frame: that frame is on every level of a recursion). */
static char *assign_name(const char *word)
{
    size_t len = (size_t)(strchr(word, '=') - word);
    char *name = (char *)malloc(len + 1);
    if (name) {
        memcpy(name, word, len);
        name[len] = 0;
    }
    return name;
}

static void save_var(sh_shell *sh, const char *name, saved_var *s)
{
    const sh_var *v;
    s->name = sdup(name);
    s->value = 0;
    s->exported = 0;
    for (v = sh->ctx.vars; v; v = v->next)
        if (!strcmp(v->name, name)) {
            s->value = sdup(v->value);
            s->exported = v->exported;
        }
}

static void restore_var(sh_shell *sh, saved_var *s)
{
    if (s->name) {
        sh_unset(&sh->ctx, s->name);
        if (s->value) {
            sh_set(&sh->ctx, s->name, s->value);
            if (s->exported)
                sh_export(&sh->ctx, s->name);
        }
    }
    free(s->name);
    free(s->value);
}

/* ---- eval, exec, trap, local, getopts, umask ------------------------------------- */

/* eval [arg ...]: the arguments joined by spaces, parsed and run in this shell. */
static long b_eval(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    pbuf t = { 0, 0, 0 };
    sh_parse p;
    long st = 0;
    int i;
    for (i = 1; i < argc; i++) {
        if (i > 1)
            pb_add(&t, " ", 1);
        pb_str(&t, argv[i]);
    }
    if (!t.s)
        return 0;
    sh_parse_text(&p, t.s);
    free(t.s);
    if (p.error) {
        err2(sh, io, "eval", p.incomplete ? "unexpected end of input" : p.error);
        sh_parse_free(&p);
        return 2;
    }
    st = exec_node(sh, p.tree, io);
    sh_parse_free(&p);
    return st;
}

/* exec command [arg ...]: AmigaDOS cannot replace a process, so the command
 * runs (a builtin, or a program) and the shell ends with its status; a
 * command that is not found leaves the shell running (status 127). Without
 * a command, exec would make its redirections permanent: the core closes a
 * command's redirections when it ends, so that form does nothing. */
static long b_exec(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    builtin_fn b;
    long st;
    if (argc < 2)
        return 0;
    if ((b = find_builtin(argv[1])) != 0)
        st = b(sh, argc - 1, argv + 1, io);
    else {
        st = sh->os.run(sh->os.data, argv + 1, io, 1);
        if (st < 0) {
            err_not_found(sh, io, argv[1]);
            return 127;
        }
    }
    if (!sh->exiting) {
        sh->exiting = 1;
        sh->exit_status = st;
    }
    return st;
}

/* The signals trap knows: EXIT, INT, TERM (ixemul's numbers, as scripts type them). */
static int trap_index(const char *name)
{
    if (!strncmp(name, "SIG", 3))
        name += 3;
    if (!strcmp(name, "EXIT") || !strcmp(name, "0"))
        return 0;
    if (!strcmp(name, "INT") || !strcmp(name, "2"))
        return 1;
    if (!strcmp(name, "TERM") || !strcmp(name, "15"))
        return 2;
    return -1;
}

static const char *const trap_names[3] = { "EXIT", "INT", "TERM" };

/* trap, trap action sig ..., trap - sig ..., trap '' sig ...: no arguments
 * lists the traps; "-" (or a first operand that is a number) resets, an
 * empty action ignores the signal. */
static long b_trap(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    const char *action;
    long st = 0;
    int i, first = 2;
    if (argc < 2) {
        for (i = 0; i < 3; i++)
            if (sh->traps[i]) {
                say(sh, io->out, "trap -- '");
                say(sh, io->out, sh->traps[i]);
                say(sh, io->out, "' ");
                say(sh, io->out, trap_names[i]);
                say(sh, io->out, "\n");
            }
        return 0;
    }
    action = argv[1];
    if (argv[1][0] >= '0' && argv[1][0] <= '9' && !argv[1][strspn(argv[1], "0123456789")]) {
        action = "-";    /* trap 2: reset INT */
        first = 1;
    }
    if (first >= argc) {
        err2(sh, io, "trap", "usage: trap [action] signal ...");
        return 2;
    }
    for (i = first; i < argc; i++) {
        int k = trap_index(argv[i]);
        if (k < 0) {
            err2(sh, io, "trap", "bad signal (EXIT, INT and TERM are known)");
            st = 1;
            continue;
        }
        free(sh->traps[k]);
        sh->traps[k] = strcmp(action, "-") ? sdup(action) : 0;
    }
    return st;
}

static void run_trap_text(sh_shell *sh, const char *text)
{
    sh_parse p;
    long st = sh->ctx.status;
    sh->in_trap = 1;
    sh_parse_text(&p, text);
    if (p.error)
        err2(sh, &sh->io, "trap", p.incomplete ? "unexpected end of input" : p.error);
    else
        exec_node(sh, p.tree, &sh->io);
    sh_parse_free(&p);
    sh->in_trap = 0;
    sh->ctx.status = st;   /* the trap does not change $? */
}

int sh_trap_signal(sh_shell *sh, int sig)
{
    int k = sig == 2 ? 1 : sig == 15 ? 2 : -1;
    if (k < 0 || !sh->traps[k] || sh->in_trap)
        return 0;   /* a trap that is running is broken like any command */
    if (sh->traps[k][0])
        run_trap_text(sh, sh->traps[k]);
    return 1;
}

void sh_exit_trap(sh_shell *sh)
{
    int exiting = sh->exiting;
    long status = sh->exit_status;
    if (sh->exit_trap_ran || !sh->traps[0])
        return;
    sh->exit_trap_ran = 1;
    sh->exiting = 0;       /* exec_node does nothing in a shell that is exiting */
    if (sh->traps[0][0])
        run_trap_text(sh, sh->traps[0]);
    if (!sh->exiting) {    /* the action did not call exit: the shell's own status stands */
        sh->exiting = exiting;
        sh->exit_status = status;
    }
}

/* local name[=value] ...: variables of the running function, put back (or
 * unset again) when it returns. */
static long b_local(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i;
    if (!sh->func_depth) {
        err2(sh, io, "local", "can only be used in a function");
        return 1;
    }
    for (i = 1; i < argc; i++) {
        const char *eq = strchr(argv[i], '=');
        size_t n = eq ? (size_t)(eq - argv[i]) : strlen(argv[i]);
        char name[128];
        if (!n || n >= sizeof(name)) {
            err2(sh, io, "local", "not a valid name");
            return 1;
        }
        memcpy(name, argv[i], n);
        name[n] = 0;
        if (sh->n_locals == sh->cap_locals) {
            int cap = sh->cap_locals ? sh->cap_locals * 2 : 8;
            void *t = realloc(sh->locals, cap * sizeof(saved_var));
            if (!t) {
                err2(sh, io, "local", "out of memory");
                return 1;
            }
            sh->locals = t;
            sh->cap_locals = cap;
        }
        save_var(sh, name, (saved_var *)sh->locals + sh->n_locals++);
        if (eq)
            sh_set(&sh->ctx, name, eq + 1);
        else
            sh_unset(&sh->ctx, name);
    }
    return 0;
}

/* getopts optstring name [arg ...]: the next option of the arguments (the
 * positional ones when none are given) into name, its argument into OPTARG,
 * the index of the next argument to look at in OPTIND. 0 while there is an
 * option, 1 at the end. A leading ':' in optstring keeps getopts silent:
 * a bad option comes as name ? with OPTARG the letter, a missing argument
 * as name : likewise. */
static long b_getopts(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    const char *opts, *oi = sh_get(&sh->ctx, "OPTIND");
    char **args;
    const char *arg, *p;
    char c, val[2], nb[24], msg[40];
    long optind = oi ? atol(oi) : 1;
    int n, silent, end = 0, bad = 0;
    if (argc < 3) {
        err2(sh, io, "getopts", "usage: getopts optstring name [arg ...]");
        return 2;
    }
    if (optind < 1)
        optind = 1;
    if (optind != sh->optind_seen)
        sh->optpos = 0;     /* the script set OPTIND: start over there */
    opts = argv[1];
    silent = opts[0] == ':';
    if (silent)
        opts++;
    args = argc > 3 ? argv + 3 : sh->ctx.args.v;
    n = argc > 3 ? argc - 3 : sh->ctx.args.n;
    val[1] = 0;
    sh_unset(&sh->ctx, "OPTARG");
    if (optind > n)
        end = 1;
    else {
        arg = args[optind - 1];
        if (!sh->optpos) {
            if (arg[0] != '-' || !arg[1])
                end = 1;
            else if (!strcmp(arg, "--")) {
                optind++;
                end = 1;
            } else
                sh->optpos = 1;
        }
    }
    if (end) {
        sh->optpos = 0;
        sh_set(&sh->ctx, argv[2], "?");
        num(nb, optind);
        sh_set(&sh->ctx, "OPTIND", nb);
        sh->optind_seen = optind;
        return 1;
    }
    arg = args[optind - 1];
    c = arg[sh->optpos++];
    p = c == ':' ? 0 : strchr(opts, c);
    val[0] = c;
    if (!p) {
        bad = 1;
        if (silent)
            sh_set(&sh->ctx, "OPTARG", val);
        else {
            strcpy(msg, "illegal option -- ");
            msg[strlen(msg) + 1] = 0;
            msg[strlen(msg)] = c;
            err2(sh, io, "getopts", msg);
        }
    } else if (p[1] == ':') {
        if (arg[sh->optpos]) {            /* -ofile */
            sh_set(&sh->ctx, "OPTARG", arg + sh->optpos);
            sh->optpos = 0;
            optind++;
        } else if (optind < n) {          /* -o file */
            sh_set(&sh->ctx, "OPTARG", args[optind]);
            sh->optpos = 0;
            optind += 2;
        } else {
            sh->optpos = 0;
            optind++;
            bad = 2;
            if (silent)
                sh_set(&sh->ctx, "OPTARG", val);
            else {
                strcpy(msg, "option requires an argument -- ");
                msg[strlen(msg) + 1] = 0;
                msg[strlen(msg)] = c;
                err2(sh, io, "getopts", msg);
            }
        }
    }
    if (sh->optpos && !arg[sh->optpos]) {  /* the cluster is used up */
        sh->optpos = 0;
        optind++;
    }
    num(nb, optind);
    sh_set(&sh->ctx, "OPTIND", nb);
    sh->optind_seen = optind;
    val[0] = bad == 2 && silent ? ':' : bad ? '?' : c;
    sh_set(&sh->ctx, argv[2], val);
    return 0;
}

/* umask [-S] [mask]: print or set the file creation mask (octal; -S prints
 * it as the permissions that stay). The shell keeps it and hands it to the
 * OS layer (sh_os.umask) for the commands it starts. */
static long b_umask(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int symbolic = argc > 1 && !strcmp(argv[1], "-S");
    const char *a = argc > 1 + symbolic ? argv[1 + symbolic] : 0;
    if (argc > 2 + symbolic) {
        err2(sh, io, "umask", "too many arguments");
        return 1;
    }
    if (a) {
        long m = 0;
        const char *q;
        if (!*a || strlen(a) > 4 || symbolic) {
            err2(sh, io, "umask", symbolic ? "-S only prints the mask" : "not an octal mask");
            return 1;
        }
        for (q = a; *q; q++) {
            if (*q < '0' || *q > '7') {
                err2(sh, io, "umask", "not an octal mask");
                return 1;
            }
            m = m * 8 + (*q - '0');
        }
        if (m > 0777) {
            err2(sh, io, "umask", "mask out of range (000 to 777)");
            return 1;
        }
        sh->umask = (int)m;
        if (sh->os.umask)
            sh->os.umask(sh->os.data, sh->umask);
        return 0;
    }
    {
        char out[32];
        int m = sh->umask, k;
        if (symbolic) {
            static const char who[3] = { 'u', 'g', 'o' };
            int o = 0;
            for (k = 0; k < 3; k++) {
                int keep = ~(m >> (6 - 3 * k)) & 7;
                out[o++] = who[k];
                out[o++] = '=';
                if (keep & 4)
                    out[o++] = 'r';
                if (keep & 2)
                    out[o++] = 'w';
                if (keep & 1)
                    out[o++] = 'x';
                if (k < 2)
                    out[o++] = ',';
            }
            out[o] = 0;
        } else {
            out[0] = '0';
            out[1] = (char)('0' + ((m >> 6) & 7));
            out[2] = (char)('0' + ((m >> 3) & 7));
            out[3] = (char)('0' + (m & 7));
            out[4] = 0;
        }
        say(sh, io->out, out);
        say(sh, io->out, "\n");
    }
    return 0;
}

static long exec_cmd(sh_shell *sh, const sh_node *n, const sh_io *parent, int wait, long *job)
{
    sh_list argv;
    sh_io io;
    const sh_word *a;
    builtin_fn b;
    sh_func *f;
    long st;
    saved_var *saved = 0;   /* on the heap: this frame is on every recursion level */
    int n_saved = 0, n_assigns = 0, nofunc = 0;
    memset(&argv, 0, sizeof(argv));
    if (job)
        *job = 0;
    sh->subst_ran = 0;
    if (expand_words(sh, n->words, &argv, parent)) {
        sh_list_free(&argv);
        return 1;
    }
    if (!argv.n) {
        /* only assignments (and redirections): they set shell variables */
        for (a = n->assigns; a; a = a->next) {
            char *v = expand_one(sh, strchr(a->text, '=') + 1, parent);
            char *name = assign_name(a->text);
            if (v && name)
                sh_set(&sh->ctx, name, v);
            free(name);
            free(v);
        }
        if (n->redirs && !redirect(sh, n->redirs, parent, &io))
            close_owned(sh, &io);
        sh_list_free(&argv);
        return sh->subst_ran ? sh->subst_status : 0;
    }
    apply_alias(sh, &argv);
    if (!strcmp(argv.v[0], "command")) {
        /* command NAME ...: the builtin or program NAME, never a function
         * of that name (the vshrc's telnet() runs the real telnet) */
        free(argv.v[0]);
        memmove(argv.v, argv.v + 1, (size_t)argv.n * sizeof(char *)); /* and the NULL */
        if (!--argv.n) {
            sh_list_free(&argv);
            return 0;
        }
        nofunc = 1;
    }
    if (redirect(sh, n->redirs, parent, &io)) {
        sh_list_free(&argv);
        return 1;
    }
    /* assignments before a command: exported for that command only; the
     * old values come back after it (IFS=: read a b leaves IFS alone) */
    for (a = n->assigns; a; a = a->next)
        n_assigns++;
    if (n_assigns)
        saved = (saved_var *)malloc(n_assigns * sizeof(saved_var));
    for (a = n->assigns; a; a = a->next) {
        char *v = expand_one(sh, strchr(a->text, '=') + 1, parent);
        char *name = assign_name(a->text);
        if (v && name) {
            if (saved)
                save_var(sh, name, &saved[n_saved++]);
            sh_set(&sh->ctx, name, v);
            sh_export(&sh->ctx, name);
        }
        free(name);
        free(v);
    }
    if (!nofunc && (f = find_func(sh, argv.v[0])) != 0) {
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
        sh->os.suspendable = wait && sh->os.cont;
        st = sh->os.run(sh->os.data, argv.v, &io, wait);
        sh->os.suspendable = 0;
        if (wait && st == SH_STOPPED) {
            st = fg_status(sh, st, -1, words_text(n), &io);
        } else if (!wait) {
            if (job)
                *job = st > 0 ? st : 0;
            st = st > 0 ? 0 : 1;
        } else if (st < 0) {
            err_not_found(sh, &io, argv.v[0]);
            st = 127;
        }
    }
    while (n_saved > 0)
        restore_var(sh, &saved[--n_saved]);
    free(saved);
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

/* Run n as a subshell: a process of its own when the OS layer can make
 * one (then the shell's variables, directory and functions are safe from
 * it), else here with the directory restored. io's owned streams go with
 * it. wait: its status; else *job (0: it ran here, or did not start). */
static long subshell(sh_shell *sh, const sh_node *n, const sh_io *io, int wait, long *job)
{
    sh_shell *c;
    sh_parse *t;
    long r;
    if (job)
        *job = 0;
    if (!sh->os.spawn) {
        char *dir = sh->os.cwd(sh->os.data);
        r = exec_node(sh, n, io);
        close_owned(sh, io);
        if (dir)
            sh->os.chdir(sh->os.data, dir);
        free(dir);
        return r;
    }
    c = sh_shell_clone(sh);
    t = (sh_parse *)malloc(sizeof(sh_parse));
    if (t)
        sh_parse_copy(n, t);
    if (!c || !t || (n && !t->tree)) {
        if (c) {
            sh_shell_free(c);
            free(c);
        }
        if (t) {
            sh_parse_free(t);
            free(t);
        }
        close_owned(sh, io);
        err2(sh, io, "subshell", "out of memory");
        return 1;
    }
    r = sh->os.spawn(sh->os.data, c, t, io, wait);
    if (r < 0 && (!wait || r == -1)) {
        /* it did not start: child and tree are still ours (the streams are not) */
        sh_shell_free(c);
        free(c);
        sh_parse_free(t);
        free(t);
        err2(sh, io, "subshell", "cannot start");
        return 1;
    }
    if (wait)
        return r;
    *job = r;
    return 0;
}

/* A command's text for jobs: its words; a pipeline's stages joined by |. */
static char *node_text(const sh_node *n)
{
    pbuf b = { 0, 0, 0 };
    char *t;
    if (n && n->kind == SH_CMD)
        return words_text(n);
    if (n && n->kind == SH_PIPE) {
        t = node_text(n->a);
        if (t)
            pb_str(&b, t);
        free(t);
        pb_str(&b, " | ");
        t = node_text(n->b);
        if (t)
            pb_str(&b, t);
        free(t);
        return b.s ? b.s : sdup("");
    }
    return sdup(n && n->kind == SH_SUBSHELL ? "( ... )" : "{ ... }");
}

/* A background job into the table, announced as "[n] id". */
static void add_job(sh_shell *sh, long job, char *text, const sh_io *io)
{
    int i;
    char nb[16];
    for (i = 0; i < 32 && sh->jobs[i]; i++)
        ;
    if (i == 32) {
        free(text);
        return;
    }
    sh->jobs[i] = job;
    free(sh->job_text[i]);
    sh->job_text[i] = text;
    num(nb, i + 1);
    say(sh, io->err, "[");
    say(sh, io->err, nb);
    say(sh, io->err, "] ");
    num(nb, job);
    say(sh, io->err, nb);
    say(sh, io->err, "\n");
}

static long exec_pipeline(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    const sh_node *st[16];
    sh_io sio[16];
    long job[16];
    int started[16];
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
    /* every stage starts at once: external commands in their runners, the
     * others (builtins, functions, compound commands) as subshells -- all
     * but the last, which runs in the shell itself (zsh: `echo a | read x`
     * sets x) */
    for (i = 0; i < k; i++) {
        job[i] = 0;
        started[i] = 0;
        if (is_external(sh, st[i])) {
            exec_cmd(sh, st[i], &sio[i], 0, &job[i]);
        } else if (sh->os.spawn && i + 1 < k) {
            subshell(sh, st[i], &sio[i], 0, &job[i]);
            started[i] = 1; /* or failed: its streams are gone either way */
        }
    }
    for (i = 0; i < k; i++) {
        if (job[i] || started[i])
            continue;
        if (is_external(sh, st[i])) {
            /* it did not start; the OS layer took its streams all the same */
            char *name = expand_one(sh, st[i]->words->text, io);
            err_not_found(sh, io, name ? name : "?");
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
        if (sh->exiting || sh->returning || sh->intr)
            break;
        if ((c == 0) == until)
            break;
        st = exec_node(sh, n->b, io);
        if (sh->breaking) {
            sh->breaking--;
            break;
        }
        sh->continuing = 0;
        if (sh->exiting || sh->returning || sh->intr)
            break;
    }
    sh->loop_depth--;
    return sh->intr ? intr_status(sh) : st;
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
        if (sh->exiting || sh->returning || sh->intr)
            break;
    }
    sh->loop_depth--;
    sh_list_free(&items);
    return sh->intr ? intr_status(sh) : st;
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

/* The status an unwinding line ends with: 130 after Ctrl-C, 2 after
 * "nested too deeply". */
static long intr_status(const sh_shell *sh)
{
    return sh->intr == 2 ? 2 : 130;
}

/* Ctrl-C: once it arrives, everything unwinds to the prompt (status 130). */
static int poll_break(sh_shell *sh)
{
    if (!sh->intr && sh->os.interrupted && sh->os.interrupted(sh->os.data) && !sh_trap_signal(sh, 2))
        sh->intr = 1;
    return sh->intr;
}

/* if, while, until, for, case: the body, once their redirections (if any)
 * are in io */
static long exec_compound(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    switch (n->kind) {
    case SH_IF:
        if (!exec_node(sh, n->a, io))
            return exec_node(sh, n->b, io);
        return n->c ? exec_node(sh, n->c, io) : 0;
    case SH_WHILE:
    case SH_UNTIL:
        return exec_list_loop(sh, n, io);
    case SH_FOR:
        return exec_for(sh, n, io);
    case SH_CASE:
        return exec_case(sh, n, io);
    default:
        return 0;
    }
}

static long exec_node(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    long st = 0;
    sh_io rio;
    if (!n || sh->exiting)
        return sh->ctx.status;
    if (poll_break(sh))
        return 130;
    if (sh->stack_limit && (unsigned long)&rio < sh->stack_limit) {
        /* the stack is nearly used up: stop here, as a clean error,
         * rather than run past it (on the Amiga that corrupts memory) */
        char d[16];
        num(d, sh->func_depth);
        say(sh, io->err, "vsh: nested too deeply (");
        say(sh, io->err, d);
        say(sh, io->err, " function levels)\n");
        sh->intr = 2; /* unwind like Ctrl-C, but as an error: status 2 */
        return 2;
    }
    switch (n->kind) {
    case SH_CMD:
        st = exec_cmd(sh, n, io, 1, 0);
        break;
    case SH_SEQ:
        st = exec_node(sh, n->a, io);
        sh->ctx.status = st;
        if (!sh->exiting && !sh->breaking && !sh->continuing && !sh->returning && !sh->intr && n->b)
            st = exec_node(sh, n->b, io);
        break;
    case SH_BG: {
        long job = 0;
        if (n->a && is_external(sh, n->a))
            exec_cmd(sh, n->a, io, 0, &job);
        else if (n->a && sh->os.spawn) {
            sh_io bio = *io;
            bio.owned = 0;
            subshell(sh, n->a, &bio, 0, &job);
        } else {
            exec_node(sh, n->a, io); /* no subshell processes: it runs now */
        }
        if (job) {
            add_job(sh, job, node_text(n->a), io);
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
    case SH_SUBSHELL:
        if (redirect(sh, n->redirs, io, &rio))
            st = 1;
        else
            st = subshell(sh, n->a, &rio, 1, 0);
        break;
    case SH_GROUP:
        if (redirect(sh, n->redirs, io, &rio))
            st = 1;
        else {
            st = exec_node(sh, n->a, &rio);
            close_owned(sh, &rio);
        }
        break;
    case SH_IF:
    case SH_WHILE:
    case SH_UNTIL:
    case SH_FOR:
    case SH_CASE:
        /* a compound command's redirections are for all of it (POSIX
         * 2.9.4), as a group's: "while read l; ...; done <in >out" */
        if (n->redirs) {
            if (redirect(sh, n->redirs, io, &rio)) {
                st = 1;
                break;
            }
            st = exec_compound(sh, n, &rio);
            close_owned(sh, &rio);
        } else {
            st = exec_compound(sh, n, io);
        }
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
        if (f->body.tree && f->busy) {
            /* it is running: its old body stays until the shell ends */
            sh_retired *r = (sh_retired *)malloc(sizeof(sh_retired));
            if (r) {
                r->p = f->body;
                r->next = sh->retired;
                sh->retired = r;
            }
        } else {
            sh_parse_free(&f->body);
        }
        /* its own copy: the line that defined it is freed after it runs */
        sh_parse_copy(n->a, &f->body);
        st = f->body.tree || !n->a ? 0 : 1;
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
    st = exec_node(sh, p.tree, &sh->io);
    sh_parse_free(&p);
    if (sh->intr) {
        st = sh->ctx.status = intr_status(sh);
        sh->intr = 0;
    }
    return st;
}

/* $(cmd): cmd runs as a subshell writing into a pipe the shell reads
 * (without subshell processes: here, into a temporary file). */
static char *core_subst(sh_ctx *c, const char *cmd)
{
    sh_shell *sh = (sh_shell *)c->user;
    pbuf out = { 0, 0, 0 };
    sh_parse p;
    char buf[512];
    long n;
    sh_parse_text(&p, cmd);
    if (p.error) {
        err2(sh, &sh->io, p.error, 0);
        sh_parse_free(&p);
        sh->ctx.status = 2;
        return sdup("");
    }
    if (sh->os.spawn && sh->os.read) {
        sh_fh rd, wr;
        sh_io io = sh->io;
        long job = 0, st;
        if (sh->os.pipe(sh->os.data, &rd, &wr)) {
            sh_parse_free(&p);
            err2(sh, &sh->io, "pipe", "cannot create");
            return sdup("");
        }
        io.out = wr;
        io.owned = SH_OWN_OUT;
        st = subshell(sh, p.tree, &io, 0, &job);
        sh_parse_free(&p);
        while ((n = sh->os.read(sh->os.data, rd, buf, sizeof(buf))) > 0)
            pb_add(&out, buf, n);
        sh->os.close(sh->os.data, rd);
        if (job)
            st = sh->os.wait(sh->os.data, job);
        sh->ctx.status = st;
        sh->subst_ran = 1;
        sh->subst_status = st;
    } else {
        char path[48], nb[24];
        sh_io io = sh->io;
        sh_fh fh;
        num(nb, ++sh->heredocs);
        strcpy(path, "T:vsh-subst.");
        strcat(path, nb);
        fh = sh->os.open(sh->os.data, path, SH_OPEN_WRITE);
        if (fh) {
            io.out = fh;
            io.owned = 0;
            sh->ctx.status = exec_node(sh, p.tree, &io);
            sh->subst_ran = 1;
            sh->subst_status = sh->ctx.status;
            sh->os.close(sh->os.data, fh);
            fh = sh->os.open(sh->os.data, path, SH_OPEN_READ);
        }
        sh_parse_free(&p);
        if (fh) {
            char line[512];
            while ((n = sh->os.read_line(sh->os.data, fh, line, sizeof(line))) >= 0)
                pb_add(&out, line, n);
            sh->os.close(sh->os.data, fh);
        }
    }
    return out.s ? out.s : sdup("");
}

/* ---- the prompt ---------------------------------------------------------------- */

/* Text an escape produced, protected from the expansion that follows
 * (a directory named "$x" stays "$x"). */
static void pb_literal(pbuf *b, const char *s)
{
    for (; *s; s++) {
        if (*s == '$' || *s == '`' || *s == '"' || *s == '\\')
            pb_add(b, "\\", 1);
        pb_add(b, s, 1);
    }
}

static int prefix_nocase(const char *s, const char *p)
{
    for (; *p; s++, p++) {
        char a = *s, c = *p;
        if (a >= 'A' && a <= 'Z')
            a = (char)(a - 'A' + 'a');
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        if (a != c)
            return 0;
    }
    return 1;
}

/* The directory with $HOME shown as ~ (Amiga names ignore case). */
static void pb_dir(sh_shell *sh, pbuf *b, int base_only)
{
    char *cwd = sh->os.cwd ? sh->os.cwd(sh->os.data) : 0;
    const char *home = sh_get(&sh->ctx, "HOME");
    const char *d = cwd ? cwd : "?";
    long hl = home ? (long)strlen(home) : 0;
    if (hl && prefix_nocase(d, home)) {
        const char *rest = d + hl;
        /* only at a name boundary: HOME "Work:x" is not a prefix of "Work:xy" */
        if (!*rest || *rest == '/' || home[hl - 1] == ':' || home[hl - 1] == '/') {
            if (base_only && *rest) {
                const char *s = strrchr(rest, '/');
                pb_literal(b, s ? s + 1 : rest);
            } else {
                pb_add(b, "~", 1);
                if (*rest && *rest != '/')
                    pb_add(b, "/", 1);
                pb_literal(b, rest);
            }
            free(cwd);
            return;
        }
    }
    if (base_only) {
        const char *s = strrchr(d, '/');
        if (!s)
            s = strrchr(d, ':');
        if (s && s[1])
            d = s + 1;
    }
    pb_literal(b, d);
    free(cwd);
}

static const char *var_or(sh_shell *sh, const char *a, const char *b, const char *dflt)
{
    const char *v = sh_get(&sh->ctx, a);
    if ((!v || !*v) && b)
        v = sh_get(&sh->ctx, b);
    return v && *v ? v : dflt;
}

/* ESC [ n1 ; n2 ... m */
static void pb_sgr(pbuf *b, const long *v, int n)
{
    char t[24];
    int i;
    pb_str(b, "\033[");
    for (i = 0; i < n; i++) {
        if (i)
            pb_add(b, ";", 1);
        num(t, v[i]);
        pb_str(b, t);
    }
    pb_add(b, "m", 1);
}

/* %F{..} / %K{..}: a colour name, a palette index, or #rrggbb */
static const char *pb_colour(pbuf *b, const char *p, int bg)
{
    static const char *const names[] = { "black", "red", "green", "yellow", "blue",
                                         "magenta", "cyan", "white" };
    char spec[16];
    long v[5];
    int n = 0, i;
    if (*p != '{') {
        v[0] = bg ? 49 : 39;
        pb_sgr(b, v, 1);
        return p;
    }
    for (p++; *p && *p != '}'; p++)
        if (n < (int)sizeof(spec) - 1)
            spec[n++] = *p;
    spec[n] = 0;
    if (*p == '}')
        p++;
    for (i = 0; i < 8; i++)
        if (!strcmp(spec, names[i]))
            break;
    if (i < 8) {
        v[0] = (bg ? 40 : 30) + i;
        pb_sgr(b, v, 1);
    } else if (!strcmp(spec, "default")) {
        v[0] = bg ? 49 : 39;
        pb_sgr(b, v, 1);
    } else if (spec[0] == '#' && n == 7) {
        long rgb = strtol(spec + 1, 0, 16);
        v[0] = bg ? 48 : 38;
        v[1] = 2;
        v[2] = (rgb >> 16) & 255;
        v[3] = (rgb >> 8) & 255;
        v[4] = rgb & 255;
        pb_sgr(b, v, 5);
    } else if (spec[0] >= '0' && spec[0] <= '9') {
        v[0] = bg ? 48 : 38;
        v[1] = 5;
        v[2] = atol(spec) & 255;
        pb_sgr(b, v, 3);
    }
    /* an unknown name draws nothing, as in zsh */
    return p;
}

/* bash's \c escape c */
static void pb_bash(sh_shell *sh, pbuf *b, char c)
{
    switch (c) {
    case 'w': pb_dir(sh, b, 0); break;
    case 'W': pb_dir(sh, b, 1); break;
    case 'u': pb_literal(b, var_or(sh, "USER", "USERNAME", "amiga")); break;
    case 'h': case 'H': pb_literal(b, var_or(sh, "HOST", "HOSTNAME", "amiga")); break;
    case '$': pb_add(b, "\\$", 2); break;  /* no superuser: always $ */
    case 'e': pb_add(b, "\033", 1); break;
    case 'n': pb_add(b, "\n", 1); break;
    case 'a': pb_add(b, "\007", 1); break;
    case '[': case ']': break;  /* readline's non-printing marks: nothing to mark here */
    case '\\': pb_add(b, "\\\\", 2); break;
    default: pb_add(b, "\\", 1); pb_add(b, &c, 1); break;
    }
}

/* zsh's %c escape at p (p[-1] is the %); returns the last character used */
static const char *pb_zsh(sh_shell *sh, pbuf *b, const char *p)
{
    static const char *const simple[] = { "f\033[39m", "k\033[49m", "B\033[1m", "b\033[22m",
                                          "U\033[4m", "u\033[24m", "S\033[7m", "s\033[27m",
                                          "#%", "%%" };
    char digits[24];
    int i;
    for (i = 0; i < (int)(sizeof(simple) / sizeof(simple[0])); i++)
        if (simple[i][0] == *p) {
            pb_str(b, simple[i] + 1);
            return p;
        }
    switch (*p) {
    case '~': pb_dir(sh, b, 0); break;
    case '/': case 'd': {
        char *cwd = sh->os.cwd ? sh->os.cwd(sh->os.data) : 0;
        pb_literal(b, cwd ? cwd : "?");
        free(cwd);
        break;
    }
    case 'c': case '.': case '1': pb_dir(sh, b, 1); break;
    case 'n': pb_literal(b, var_or(sh, "USER", "USERNAME", "amiga")); break;
    case 'm': case 'M': pb_literal(b, var_or(sh, "HOST", "HOSTNAME", "amiga")); break;
    case '?': num(digits, sh->ctx.status); pb_str(b, digits); break;
    case 'F': return pb_colour(b, p + 1, 0) - 1;
    case 'K': return pb_colour(b, p + 1, 1) - 1;
    default: pb_add(b, "%", 1); pb_add(b, p, 1); break;
    }
    return p;
}

char *sh_prompt(sh_shell *sh, const char *ps)
{
    pbuf b = { 0, 0, 0 };
    sh_list out;
    const char *err = 0;
    char *r;
    pb_add(&b, "\"", 1);
    for (; *ps; ps++) {
        if (*ps == '"' || (*ps == '\\' && !ps[1])) {
            pb_add(&b, "\\", 1);  /* a quote, or a last backslash, stays text */
            pb_add(&b, ps, 1);
        } else if (*ps == '\\')
            pb_bash(sh, &b, *++ps);
        else if (*ps == '%' && ps[1])
            ps = pb_zsh(sh, &b, ps + 1);
        else
            pb_add(&b, ps, 1);
    }
    pb_add(&b, "\"", 1);
    if (!b.s)
        return sdup("");
    /* then parameter, command and arithmetic expansion (POSIX PS1; zsh's
     * PROMPT_SUBST), as one double-quoted word */
    memset(&out, 0, sizeof(out));
    if (sh_expand(&sh->ctx, b.s, SH_NO_SPLIT | SH_NO_GLOB, &out, &err) || out.n < 1) {
        b.s[b.n - 1] = 0;
        r = sdup(b.s + 1);  /* unexpanded, rather than nothing */
    } else
        r = sdup(out.v[0]);
    sh_list_free(&out);
    free(b.s);
    return r;
}
