/* vsh's executor (see sh_exec.h). */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "sh_exec.h"
#include "../claude/regex.h"
#include "sh_hits.h"
#include "sh_float.h"
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
static char *core_procsub(sh_ctx *c, const char *cmd, int out);

/* ---- options: the one table ------------------------------------------------- */

static const struct sh_opt {
    const char *name;
    char letter;
    unsigned long bit;
} sh_optab[] = {
    { "allexport", 'a', SO_ALLEXPORT }, { "braceexpand", 'B', SO_BRACEEXPAND }, { "emacs", 0, SO_INERT },
    { "errexit", 'e', SO_ERREXIT }, { "errtrace", 'E', SO_ERRTRACE }, { "functrace", 'T', SO_FUNCTRACE },
    { "hashall", 'h', SO_HASHALL }, { "histexpand", 'H', SO_INERT }, { "history", 0, SO_INERT },
    { "ignoreeof", 0, SO_INERT }, { "interactive-comments", 0, SO_ICOMMENTS }, { "keyword", 'k', SO_INERT },
    { "monitor", 'm', SO_INERT }, { "noclobber", 'C', SO_NOCLOBBER }, { "noexec", 'n', SO_NOEXEC },
    { "noglob", 'f', SO_NOGLOB }, { "nolog", 0, SO_INERT }, { "notify", 'b', SO_INERT },
    { "nounset", 'u', SO_NOUNSET }, { "onecmd", 't', SO_INERT }, { "physical", 'P', SO_INERT },
    { "pipefail", 0, SO_PIPEFAIL }, { "posix", 0, SO_POSIX }, { "privileged", 'p', SO_INERT },
    { "verbose", 'v', SO_VERBOSE }, { "vi", 0, SO_INERT }, { "xtrace", 'x', SO_XTRACE }
};
#define N_SHOPT ((int)(sizeof(sh_optab) / sizeof(sh_optab[0])))
/* the order of the letters in $-, as bash prints them */
static const char sh_flag_order[] = "abefhikmnptuvxBCEHPT";

/* an option with no effect keeps its own state (set -o vi; set -o: vi on) */
static unsigned long inert_state;
static int shopt_get(sh_shell *sh, const char *name);
static int core_pathkind(sh_ctx *c, const char *path);

/* $- and the context's copies of the flags the expander looks at */
static void opts_apply(sh_shell *sh)
{
    char *p = sh->flagbuf;
    const char *o;
    int i;
    for (o = sh_flag_order; *o; o++) {
        unsigned long bit = *o == 'i' ? SO_INTERACTIVE : 0;
        for (i = 0; i < N_SHOPT && !bit; i++)
            if (sh_optab[i].letter == *o)
                bit = sh_optab[i].bit;
        if (bit && (bit == SO_INERT ? 0 : (sh->opts & bit)))
            *p++ = *o;
    }
    if (sh->opts & SO_STDIN)
        *p++ = 's';
    if (sh->opts & SO_COMMAND)
        *p++ = 'c';
    *p = 0;
    sh->ctx.flags = sh->flagbuf;
    sh->ctx.nounset = (sh->opts & SO_NOUNSET) != 0;
    sh->ctx.noglob = (sh->opts & SO_NOGLOB) != 0;
    sh->ctx.allexport = (sh->opts & SO_ALLEXPORT) != 0;
}

/* set -x / +x ... by letter: 0 = no such option */
static int opt_letter(sh_shell *sh, char ch, int on)
{
    int i;
    for (i = 0; i < N_SHOPT; i++)
        if (sh_optab[i].letter == ch) {
            if (sh_optab[i].bit != SO_INERT) {
                if (on)
                    sh->opts |= sh_optab[i].bit;
                else
                    sh->opts &= ~sh_optab[i].bit;
            } else {
                if (on)
                    inert_state |= 1UL << i;
                else
                    inert_state &= ~(1UL << i);
            }
            opts_apply(sh);
            return 1;
        }
    return 0;
}

static int opt_name(sh_shell *sh, const char *name, int on)
{
    int i;
    for (i = 0; i < N_SHOPT; i++)
        if (!strcmp(sh_optab[i].name, name)) {
            if (sh_optab[i].bit != SO_INERT) {
                if (on)
                    sh->opts |= sh_optab[i].bit;
                else
                    sh->opts &= ~sh_optab[i].bit;
            } else {
                if (on)
                    inert_state |= 1UL << i;
                else
                    inert_state &= ~(1UL << i);
            }
            opts_apply(sh);
            if (sh_optab[i].bit == SO_POSIX) {
                /* bash keeps POSIXLY_CORRECT in step with the mode: set to y on entering, unset on leaving */
                if (!on)
                    sh_unset(&sh->ctx, "POSIXLY_CORRECT");
                else if (!sh_get(&sh->ctx, "POSIXLY_CORRECT"))
                    sh_set(&sh->ctx, "POSIXLY_CORRECT", "y");
            }
            return 1;
        }
    return 0;
}

static int opt_on(const sh_shell *sh, int i)
{
    return sh_optab[i].bit == SO_INERT ? (inert_state >> i) & 1 : (sh->opts & sh_optab[i].bit) != 0;
}

static void core_warn(sh_ctx *c, const char *name, const char *msg);
static void core_refresh(sh_ctx *c, const char *name);
static char *core_unescape(sh_ctx *c, const char *s);
static char *core_prompt(sh_ctx *c, const char *ps);
static char *core_declared(sh_ctx *c, const char *name, int flags_only, int whole);
static void core_on_assign(sh_ctx *c, const char *name, const char *value);

void sh_shell_init(sh_shell *sh)
{
    /* every field starts zero: a clone is malloc memory, and a field added later must not
     * start as garbage there (nclosed and wfail did: a subshell's echo spun or failed) */
    memset(sh, 0, sizeof(*sh));
    memset(sh->shopt_v, -1, sizeof(sh->shopt_v));
    sh->ctx.pathkind = core_pathkind;
    sh_set_extglob(0);
    sh->ctx.subst = core_subst;
    sh->ctx.procsub = core_procsub;
    sh->ctx.user = sh;
    sh->ctx.warn = core_warn;
    sh->ctx.refresh = core_refresh;
    sh->ctx.on_assign = core_on_assign;
    sh->ctx.unescape = core_unescape;
    sh->ctx.prompt = core_prompt;
    sh->ctx.declared = core_declared;
    sh->umask = 022;
    sh->opts = SO_BRACEEXPAND | SO_HASHALL | SO_ICOMMENTS;
    opts_apply(sh);
}

/* A variable as it was before NAME=value cmd or local NAME, to put back after. */
typedef struct saved_var {
    char *name;
    sh_var *var;            /* the whole variable as it was (attributes, value or array); 0: it was not set */
} saved_var;
static void restore_var(sh_shell *sh, saved_var *s);

static void frame_pop(sh_shell *sh);

static void tmp_sweep(sh_shell *sh, int mark);

static void fd_defer_release(sh_shell *sh, long job);
static long job_wait(sh_shell *sh, long job);
static long b_eval(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_disown(sh_shell *sh, int argc, char **argv, const sh_io *io);
static void pseudo_trap(sh_shell *sh, int k, const sh_io *io);
static void debug_trap(sh_shell *sh, const sh_node *n, const sh_io *io);
static void debug_forarith(sh_shell *sh, const char *text, const sh_io *io);
static void debug_trap_set(sh_shell *sh, char *t, const sh_io *io);

void sh_shell_free(sh_shell *sh)
{
    int i;
    fd_defer_release(sh, 0);
    for (i = 0; i < SH_FDMAX; i++) {
        int k;
        for (k = 0; k < i && sh->fdt[k].fh != sh->fdt[i].fh; k++)
            ;
        if (sh->fdt[i].fh && sh->fdt[i].own && k == i)
            sh->os.close(sh->os.data, sh->fdt[i].fh);
    }
    sh->intr = 1; /* the files go, a >( ) command does not run */
    tmp_sweep(sh, 0);
    free(sh->tmps);
    sh->tmps = 0;
    while (sh->funcs) {
        sh_func *f = sh->funcs;
        sh->funcs = f->next;
        free(f->name);
        free(f->src);
        sh_parse_free(&f->body);
        free(f);
    }
    while (sh->nframes > 0)
        frame_pop(sh);
    free(sh->frames);
    while (sh->retired) {
        sh_retired *r = sh->retired;
        sh->retired = r->next;
        sh_parse_free(&r->p);
        free(r);
    }
    for (i = 0; i < 32; i++)
        free(sh->job_text[i]);
    for (i = 0; i < sh->ndirstk; i++)
        free(sh->dirstk[i]);
    free(sh->dirstk);
    for (i = 0; i < SH_NTRAP; i++)
        free(sh->traps[i]);
    while (sh->n_locals > 0) /* a shell ended inside a function: the values go with it */
        restore_var(sh, (saved_var *)sh->locals + --sh->n_locals);
    free(sh->locals);
    sh_list_free(&sh->aliases);
    sh_list_free(&sh->hist);
    sh_list_free(&sh->hashtab);
    free(sh->hashpath);
    sh_list_free(&sh->disabled);
    sh_ctx_free(&sh->ctx);
}

static int frame_push(sh_shell *sh, const char *name, const char *src);

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
    for (i = 0; i < SH_FDMAX; i++) {
        c->fdt[i].fh = sh->fdt[i].fh; /* shared with the parent, which closes it */
        c->fdt[i].own = 0;
    }
    for (v = sh->ctx.vars; v; v = v->next) {
        sh_var *cp = sh_var_copy(v);
        if (cp)
            sh_var_link(&c->ctx, cp);
    }
    for (i = 0; i < sh->ctx.args.n; i++)
        sh_list_add(&c->ctx.args, sh->ctx.args.v[i]);
    c->ctx.arg0 = sh->ctx.arg0;  /* not owned by a ctx */
    c->ctx.status = sh->ctx.status;
    c->ctx.pid = sh->ctx.pid;
    c->ctx.last_bg = sh->ctx.last_bg;
    for (i = 0; i < 32; i++)
        if (sh->jobs[i]) { /* the subshell lists its parent's jobs (jobs | cat, $(jobs)) but is not their parent */
            c->jobs[i] = sh->jobs[i];
            c->job_text[i] = sh->job_text[i] ? sdup(sh->job_text[i]) : 0;
            c->job_seq[i] = sh->job_seq[i];
            c->job_stopped[i] = sh->job_stopped[i];
            c->job_foreign[i] = 1;
        }
    c->job_seqno = sh->job_seqno;
    if (sh->ndirstk && (c->dirstk = (char **)malloc((size_t)sh->ndirstk * sizeof(char *)))) { /* the directory stack is inherited */
        for (i = 0; i < sh->ndirstk; i++)
            c->dirstk[i] = sdup(sh->dirstk[i]);
        c->ndirstk = sh->ndirstk;
    }
    c->dirstack_gone = sh->dirstack_gone;
    c->cp_close[0] = sh->cp_close[0];
    c->cp_close[1] = sh->cp_close[1];
    c->umask = sh->umask;  /* traps are not inherited (POSIX); set -E and -T hand the ERR, DEBUG and RETURN ones on */
    c->opts |= sh->opts & (SO_ERRTRACE | SO_FUNCTRACE);
    if ((sh->opts & SO_ERRTRACE) && sh->traps[TRAP_ERR])
        c->traps[TRAP_ERR] = sdup(sh->traps[TRAP_ERR]);
    if (sh->opts & SO_FUNCTRACE) {
        if (sh->traps[TRAP_DEBUG])
            c->traps[TRAP_DEBUG] = sdup(sh->traps[TRAP_DEBUG]);
        if (sh->traps[TRAP_RETURN])
            c->traps[TRAP_RETURN] = sdup(sh->traps[TRAP_RETURN]);
    }
    c->opts = sh->opts;
    c->cond_depth = sh->cond_depth;
    c->xlevel = sh->xlevel;
    opts_apply(c);
    if (sh->ctx.npstat)
        sh_pstat(&c->ctx, sh->ctx.pstat, sh->ctx.npstat);
    c->ctx.nocase = sh->ctx.nocase;
    c->ctx.nullglob = sh->ctx.nullglob;
    c->ctx.failglob = sh->ctx.failglob;
    c->ctx.dotglob = sh->ctx.dotglob;
    c->ctx.nocasematch = sh->ctx.nocasematch;
    c->ctx.globstar = sh->ctx.globstar;
    memcpy(c->shopt_v, sh->shopt_v, sizeof(c->shopt_v));
    c->ctx.listdir = sh->ctx.listdir;
    c->ctx.subst = sh->ctx.subst;
    c->ctx.procsub = sh->ctx.procsub;
    c->ctx.warn = sh->ctx.warn;
    c->ctx.refresh = sh->ctx.refresh;
    c->ctx.on_assign = sh->ctx.on_assign;
    c->ctx.unescape = sh->ctx.unescape;
    c->ctx.prompt = sh->ctx.prompt;
    c->ctx.declared = sh->ctx.declared;
    c->lineno = sh->lineno;
    c->main_src = sh->main_src;
    c->main_run = sh->main_run;
    c->func_depth = sh->func_depth;
    c->cur_src = sh->cur_src;
    c->rseed = sh->rseed;
    c->srnd = sh->srnd;
    c->last_rand = sh->last_rand;
    c->seeded = sh->seeded;
    c->secs0 = sh->secs0;
    for (i = 0; i < sh->nframes; i++) {
        const sh_frame *fr = sh->frames + i;
        if (frame_push(c, fr->name, fr->src))
            break;
        c->frames[c->nframes - 1].line = fr->line;
    }
    c->ctx.user = sh->ctx.user == (const void *)sh ? (void *)c : sh->ctx.user;
    tail = &c->funcs;
    for (f = sh->funcs; f; f = f->next) {
        sh_func *nf = (sh_func *)calloc(1, sizeof(sh_func));
        if (!nf)
            break;
        nf->name = sdup(f->name);
        nf->src = sdup(f->src ? f->src : "");
        sh_parse_copy(f->body.tree, &nf->body);
        *tail = nf;
        tail = &nf->next;
    }
    for (i = 0; i < sh->aliases.n; i++)
        sh_list_add(&c->aliases, sh->aliases.v[i]);
    for (i = 0; i < sh->hashtab.n; i++)
        sh_list_add(&c->hashtab, sh->hashtab.v[i]);
    for (i = 0; i < sh->hist.n; i++)
        sh_list_add(&c->hist, sh->hist.v[i]);
    c->hist_native = sh->hist_native;
    c->hist_saved = sh->hist_saved;
    c->hist_base = sh->hist_base;
    c->hashpath = sh->hashpath ? sdup(sh->hashpath) : 0;
    for (i = 0; i < sh->disabled.n; i++)
        sh_list_add(&c->disabled, sh->disabled.v[i]);
    return c;
}

static long exec_node(sh_shell *sh, const sh_node *n, const sh_io *io);
static void close_owned(sh_shell *sh, const sh_io *io);
static void fd_unwind(sh_shell *sh, int mark, const sh_io *io);

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
        if (child->jobs[i] && !child->job_foreign[i])
            job_wait(child, child->jobs[i]);
    if (tree) {
        sh_parse_free(tree);
        free(tree);
    }
    sh_shell_free(child);
    free(child);
    return st;
}

/* ---- output helpers ------------------------------------------------------------- */

/* Output to fh. A stream closed with n>&- is a null device handle in sh->closed: bash's write
 * fails there (status 1 for a builtin), so the write is noted. */
static void put(sh_shell *sh, sh_fh fh, const char *s, long n)
{
    int i;
    for (i = 0; i < sh->nclosed; i++)
        if (sh->closed[i] == fh)
            sh->wfail = 1;
    sh->os.write(sh->os.data, fh, s, n);
}

static void say(sh_shell *sh, sh_fh fh, const char *s)
{
    put(sh, fh, s, (long)strlen(s));
}

static void closed_del(sh_shell *sh, sh_fh fh)
{
    int i;
    for (i = 0; i < sh->nclosed; i++)
        if (sh->closed[i] == fh) {
            sh->closed[i] = sh->closed[--sh->nclosed];
            return;
        }
}

/* say() of every string up to the NULL: one call where a message needs several */
static void sayl(sh_shell *sh, sh_fh fh, ...)
{
    va_list ap;
    const char *s;
    va_start(ap, fh);
    while ((s = va_arg(ap, const char *)) != NULL)
        say(sh, fh, s);
    va_end(ap);
}

/* the store's warnings go to the shell's own error stream, as "vsh: warning: NAME: text" */
static void core_warn(sh_ctx *c, const char *name, const char *msg)
{
    sh_shell *sh = (sh_shell *)c->user;
    sayl(sh, sh->io.err, "vsh: warning: ", name, ": ", msg, "\n", NULL);
}

/* ---- special variables and the call stack ---------------------------------------- */

static long sh_now(sh_shell *sh, long *usec)
{
    long us = 0, t = sh->os.now ? sh->os.now(sh->os.data, &us) : 0;
    *usec = us;
    return t;
}

static int frame_push(sh_shell *sh, const char *name, const char *src)
{
    sh_frame *fr;
    if (sh->nframes == sh->capframes) {
        int cap = sh->capframes ? sh->capframes * 2 : 16;
        sh_frame *t = (sh_frame *)realloc(sh->frames, (size_t)cap * sizeof(sh_frame));
        if (!t)
            return 1;
        sh->frames = t;
        sh->capframes = cap;
    }
    fr = sh->frames + sh->nframes++;
    fr->name = sdup(name);
    fr->src = sdup(src ? src : "");
    fr->line = sh->lineno;
    return 0;
}

static void frame_pop(sh_shell *sh)
{
    sh_frame *fr = sh->frames + --sh->nframes;
    free(fr->name);
    free(fr->src);
}

/* FUNCNAME (which 0), BASH_SOURCE (1), BASH_LINENO (2): the frames innermost first, then the
 * script's own bottom frame ("main", the file, line 0). FUNCNAME exists only inside a function. */
static void frame_array(sh_shell *sh, const char *name, int which)
{
    int i, k = 0, total = sh->nframes + (sh->main_src != 0);
    char d[24], ix[24];
    if (!total || (which == 0 && !sh->func_depth)) {
        sh_unset(&sh->ctx, name);
        return;
    }
    sh_array_reset(&sh->ctx, name, 0);
    for (i = 0; i < total; i++) {
        const char *v;
        if (i == sh->nframes)
            v = which == 0 ? "main" : which == 1 ? sh->main_src : "0";
        else {
            const sh_frame *fr = sh->frames + (sh->nframes - 1 - i);
            if (which == 2)
                sh_ltoa(fr->line, d);
            v = which == 0 ? fr->name : which == 1 ? fr->src : d;
        }
        sh_ltoa(k++, ix);
        sh_assign(&sh->ctx, name, ix, v, 0);
    }
}

/* bash 5's RANDOM: the Park-Miller minimal standard generator (16807 mod 2^31-1, in 32 bits as bash
 * computes it), its two 16-bit halves folded, 15 bits kept, never the same value twice running */
static int next_random(sh_shell *sh)
{
    int rv;
    if (!sh->seeded) {
        long us, t = sh_now(sh, &us);
        sh->rseed = (unsigned long)(t ^ us ^ sh->ctx.pid);
        sh->seeded = 1;
    }
    do {
        unsigned long x = sh->rseed & 0xffffffffUL;
        long t;
        if (!x)
            x = 123459876UL;
        t = (long)(16807UL * (x % 127773UL)) - (long)(2836UL * (x / 127773UL));
        if (t < 0)
            t += 2147483647L;
        sh->rseed = (unsigned long)t;
        rv = (int)(((t >> 16) ^ (t & 65535L)) & 32767L);
    } while (rv == sh->last_rand);
    sh->last_rand = rv;
    return rv;
}

enum { SP_RANDOM, SP_SRANDOM, SP_SECONDS, SP_EPOCHSECONDS, SP_EPOCHREALTIME, SP_LINENO, SP_BASHPID,
       SP_FUNCNAME, SP_BASH_SOURCE, SP_BASH_LINENO, SP_DIRSTACK, SP_N };
static const char *const special_names[SP_N] = {
    "RANDOM", "SRANDOM", "SECONDS", "EPOCHSECONDS", "EPOCHREALTIME", "LINENO", "BASHPID",
    "FUNCNAME", "BASH_SOURCE", "BASH_LINENO", "DIRSTACK"
};

static int special_id(const char *name)
{
    int k;
    if (name[0] < 'B' || name[0] > 'S')
        return -1;
    for (k = 0; k < SP_N; k++)
        if (!strcmp(name, special_names[k]))
            return k;
    return -1;
}

/* the store's copy of a special variable is brought up to date just before it is read */
static void core_refresh(sh_ctx *c, const char *name)
{
    sh_shell *sh = (sh_shell *)c->user;
    long us = 0, t = 0;
    int k = special_id(name);
    char d[40];
    if (k < 0 || sh->special_busy)
        return;
    sh->special_busy = 1;
    switch (k) {
    case SP_RANDOM:
        sh_ltoa(next_random(sh), d);
        break;
    case SP_SRANDOM:
        if (!sh->srnd)
            sh->srnd = (unsigned long)(sh_now(sh, &us) ^ us ^ sh->ctx.pid) | 1UL;
        sh->srnd ^= sh->srnd << 13;
        sh->srnd ^= sh->srnd >> 17;
        sh->srnd ^= sh->srnd << 5;
        sh_ltoa((long)(sh->srnd & 0x7fffffffUL), d);
        break;
    case SP_SECONDS:
        sh_ltoa(sh_now(sh, &us) - sh->secs0, d);
        break;
    case SP_EPOCHSECONDS:
        sh_ltoa(sh_now(sh, &us), d);
        break;
    case SP_EPOCHREALTIME: {
        char u[24];
        int pad;
        t = sh_now(sh, &us);
        sh_ltoa(t, d);
        sh_ltoa(us, u);
        pad = 6 - (int)strlen(u);
        strcat(d, ".");
        while (pad-- > 0)
            strcat(d, "0");
        strcat(d, u);
        break;
    }
    case SP_LINENO:
        sh_ltoa(sh->lineno, d);
        break;
    case SP_BASHPID:
        sh_ltoa(sh->ctx.pid, d); /* a subshell's $$ is its own pid here, so BASHPID equals it */
        break;
    case SP_DIRSTACK: {
        /* DIRSTACK[0] is the current directory, then the stack pushd and popd keep; writes to the elements
         * 1 and up reach the stack (dirstack_write) */
        char *cwd, ix[24];
        int i;
        if (sh->dirstack_gone) { /* unset DIRSTACK: an ordinary array from then on */
            sh->special_busy = 0;
            return;
        }
        cwd = sh->os.cwd(sh->os.data);
        sh_array_reset(&sh->ctx, name, 0);
        sh_assign(&sh->ctx, name, "0", cwd ? cwd : "", 0);
        for (i = 0; i < sh->ndirstk; i++) {
            sh_ltoa(i + 1, ix);
            sh_assign(&sh->ctx, name, ix, sh->dirstk[i], 0);
        }
        free(cwd);
        sh->special_busy = 0;
        return;
    }
    default:
        frame_array(sh, name, k - SP_FUNCNAME);
        sh->special_busy = 0;
        return;
    }
    sh_set(c, name, d);
    sh->special_busy = 0;
}

/* RANDOM=n seeds the generator, SECONDS=n restarts the count at n */
static void core_on_assign(sh_ctx *c, const char *name, const char *value)
{
    sh_shell *sh = (sh_shell *)c->user;
    int k = special_id(name);
    long us;
    if (!strcmp(name, "POSIXLY_CORRECT")) {
        sh->opts |= SO_POSIX;  /* assigning it, to anything, turns posix mode on */
        SH_HIT(POSIX_VAR);
        return;
    }
    if (sh->special_busy || (k != SP_RANDOM && k != SP_SECONDS))
        return;
    if (k == SP_RANDOM) {
        sh->rseed = (unsigned long)atol(value);
        sh->last_rand = 0;
        sh->seeded = 1;
    } else
        sh->secs0 = sh_now(sh, &us) - atol(value);
}

/* a name exists (want_dir -1), is a directory (1) or not (0) */
static int sh_exists(sh_shell *sh, const char *path, int want_dir)
{
    sh_stat st;
    if (!sh->os.stat || sh->os.stat(sh->os.data, path, &st, 0))
        return 0;
    if (want_dir < 0)
        return 1;
    return want_dir ? st.type == SH_ST_DIR : st.type != SH_ST_DIR;
}

static void err2(sh_shell *sh, const sh_io *io, const char *a, const char *b)
{
    sh->bi_errs++;
    sayl(sh, io->err, "vsh: ", a, NULL);
    if (b) {
        sayl(sh, io->err, ": ", b, NULL);
    }
    say(sh, io->err, "\n");
}

/* the special builtins of POSIX (and bash's source) */
static int special_bi(const char *name)
{
    static const char *const sp[] = { ".", ":", "break", "continue", "eval", "exec", "exit", "export", "readonly",
                                      "return", "set", "shift", "source", "times", "trap", "unset", 0 };
    int k;
    for (k = 0; sp[k]; k++)
        if (!strcmp(name, sp[k]))
            return 1;
    return 0;
}

/* posix mode ends a shell that is not interactive on the error of a special builtin or of an
 * assignment: status st */
static int posix_fatal(sh_shell *sh, long st)
{
    if ((sh->opts & (SO_POSIX | SO_INTERACTIVE)) != SO_POSIX)
        return 0;
    SH_HIT(POSIX_FATAL);
    if (!sh->exiting) {
        sh->exiting = 1;
        sh->exit_status = st & 255;
    }
    return 1;
}

/* A command that did not start. One of the kit's own (ssh, sort, sz, ...)
 * says which drawer it lives in, so the fix is in the message. */
static void err_not_found(sh_shell *sh, const sh_io *io, const char *name)
{
    const char *drawer = bmsg_kit_drawer(name);
    sayl(sh, io->err, "vsh: ", name, ": not found", NULL);
    if (drawer) {
        sayl(sh, io->err, " (it lives in ", drawer, ": put that drawer on PATH, or run the UP-Term Install)", NULL);
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
    if (io->owned & SH_OWN_FDS)
        fd_unwind(sh, io->fdmark, io);
    if ((io->owned & SH_OWN_IN) && io->in) {
        closed_del(sh, io->in);
        sh->os.close(sh->os.data, io->in);
    }
    if ((io->owned & SH_OWN_OUT) && io->out) {
        closed_del(sh, io->out);
        sh->os.close(sh->os.data, io->out);
    }
    if ((io->owned & SH_OWN_ERR) && io->err && io->err != io->out) {
        closed_del(sh, io->err);
        sh->os.close(sh->os.data, io->err);
    }
}

/* ---- expansion of a command's words --------------------------------------------- */

/* an expansion error (unbound variable, ${x:?}) ends a shell that is not interactive, with status 1,
 * as bash does; a bad substitution or a negative substring length only drops the rest of the line */
static void expand_fatal(sh_shell *sh, const char *err)
{
    if (err && (strstr(err, "substring expression") || strstr(err, "bad substitution"))) {
        /* bash does not exit: it drops the rest of the command line (status 1) */
        if (!sh->intr)
            sh->intr = 3;
        return;
    }
    if (!(sh->opts & SO_INTERACTIVE)) {
        sh->exiting = 1;
        /* bash: an unbound variable in a -c string ends with 127, anywhere else 1 */
        sh->exit_status = (sh->opts & SO_COMMAND) && err && strstr(err, "unbound") ? 127 : 1;
    }
}

/* DIRSTACK[n]=dir (and the elements of DIRSTACK=(...)): bash sets the n-th directory of the stack; element 0
 * is the current directory and an index beyond the stack is dropped */
static void dirstack_write(sh_shell *sh, const char *name, const char *idx, const char *value)
{
    long i;
    const char *e = 0;
    char *nv;
    if (strcmp(name, "DIRSTACK") || sh->dirstack_gone || !idx)
        return;
    i = sh_arith(&sh->ctx, idx, &e);
    if (e || i < 1 || i > sh->ndirstk || !(nv = sdup(value)))
        return;
    SH_HIT(DIRSTACK_WRITE);
    free(sh->dirstk[i - 1]);
    sh->dirstk[i - 1] = nv;
}

/* NAME=... or NAME+=... (the argument of declare and the like) */
static int valid_name(const char *s, size_t n);
static int decl_assign(const char *t)
{
    const char *eq = strchr(t, '=');
    return eq && valid_name(t, (size_t)(eq - t) - (eq > t && eq[-1] == '+'));
}

static int expand_words(sh_shell *sh, const sh_word *w, sh_list *out, const sh_io *io)
{
    static const char *const decl[] = { "declare", "typeset", "local", "readonly", "export", 0 };
    int isdecl = 0, k;
    for (k = 0; w && decl[k]; k++)
        if (!strcmp(w->text, decl[k]))
            isdecl = 1;
    for (; w; w = w->next) {
        const char *err = 0;
        if (isdecl && w->text[strcspn(w->text, "=([ ")] == '=' && w->text[strcspn(w->text, "=([ ") + 1] == '(' &&
            w->text[0] != '-') {
            /* declare -a a=(x "y z"): the list is expanded by the assignment, word by word */
            char *raw = (char *)malloc(strlen(w->text) + 2);
            if (raw) {
                raw[0] = '\1';
                strcpy(raw + 1, w->text);
                sh_list_add(out, raw);
                free(raw);
            }
            continue;
        }
        if (sh_expand(&sh->ctx, w->text, isdecl && decl_assign(w->text) ? SH_ASSIGN : 0, out, &err)) {
            if (!strncmp(err, "no match: ", 10)) {
                sayl(sh, io->err, "vsh: ", err, "\n", NULL); /* failglob: the rest of the line is dropped, status 1 */
                if (!sh->intr)
                    sh->intr = 3;
                return -1;
            }
            err2(sh, io, w->text, err);
            expand_fatal(sh, err);
            return -1;
        }
    }
    return 0;
}

static char *expand_val(sh_shell *sh, const char *text, const sh_io *io, int flags)
{
    sh_list l;
    const char *err = 0;
    char *r;
    memset(&l, 0, sizeof(l));
    if (sh_expand(&sh->ctx, text, SH_NO_SPLIT | SH_NO_GLOB | flags, &l, &err)) {
        err2(sh, io, text, err);
        expand_fatal(sh, err);
        sh_list_free(&l);
        return 0;
    }
    r = sdup(l.n ? l.v[0] : "");
    sh_list_free(&l);
    return r;
}

static char *expand_one(sh_shell *sh, const char *text, const sh_io *io)
{
    return expand_val(sh, text, io, 0);
}

/* ---- redirections ------------------------------------------------------------- */

static void tmp_add(sh_shell *sh, const char *path, const char *cmd);

/* ---- the shell's fd table (fds 3 and up) ---------------------------------------- */

/* Descriptors above 2 live in sh->fdt, a table of the shell itself: native commands still get
 * 0-2 only. AmigaDOS has no dup, so slots share handles and a handle is closed when the last
 * user is gone (`own` slots only; a handle that stands for fd 0-2 is never closed here).
 * A command's redirections write to the table and push what they replace on sh->fdundo;
 * close_owned() puts it back when the command ends, `exec` keeps the changes (fd_commit). */

static int fd_used(const sh_shell *sh, sh_fh fh, const sh_io *io)
{
    int i;
    if (!fh)
        return 0;
    for (i = 0; i < SH_FDMAX; i++)
        if (sh->fdt[i].fh == fh)
            return 1;
    if (sh->io.in == fh || sh->io.out == fh || sh->io.err == fh)
        return 1;
    return io && (io->in == fh || io->out == fh || io->err == fh);
}

static int fd_set(sh_shell *sh, int slot, sh_fh fh, int own)
{
    if (sh->nundo >= SH_FDUNDO)
        return -1;
    sh->fdundo[sh->nundo].slot = slot;
    sh->fdundo[sh->nundo].fh = sh->fdt[slot].fh;
    sh->fdundo[sh->nundo].own = sh->fdt[slot].own;
    sh->nundo++;
    sh->fdt[slot].fh = fh;
    sh->fdt[slot].own = own;
    return 0;
}

typedef struct sh_fddefer {
    struct sh_fddefer *next;
    long job;
    sh_fh fh;
} sh_fddefer;

/* a handle a background job may still use: closed when the job is waited for */
static void fd_defer(sh_shell *sh, long job, sh_fh fh)
{
    sh_fddefer *d = (sh_fddefer *)malloc(sizeof(sh_fddefer));
    if (!d) {
        sh->os.close(sh->os.data, fh); /* no memory to remember it: closed now */
        return;
    }
    d->job = job;
    d->fh = fh;
    d->next = sh->fddefer;
    sh->fddefer = d;
}

/* job ended (0: every deferred handle, the shell is going): its deferred handles are closed
 * unless a slot or stream of the shell uses them again */
static void fd_defer_release(sh_shell *sh, long job)
{
    sh_fddefer **p = &sh->fddefer;
    while (*p) {
        sh_fddefer *d = *p;
        if (job && d->job != job) {
            p = &d->next;
            continue;
        }
        *p = d->next;
        if (!fd_used(sh, d->fh, 0))
            sh->os.close(sh->os.data, d->fh);
        free(d);
    }
}

/* the command ended: its table changes are undone, what only it used is closed. A handle of
 * the streams in `io` goes to the still running background job (job != 0) instead. */
static void fd_unwind_job(sh_shell *sh, int mark, const sh_io *io, long job)
{
    while (sh->nundo > mark) {
        int i = --sh->nundo;
        int slot = sh->fdundo[i].slot;
        sh_fh cur = sh->fdt[slot].fh;
        int cown = sh->fdt[slot].own;
        sh->fdt[slot].fh = sh->fdundo[i].fh;
        sh->fdt[slot].own = sh->fdundo[i].own;
        if (cur && cown && cur != sh->fdundo[i].fh && !fd_used(sh, cur, 0)) {
            if (job && io && (io->in == cur || io->out == cur || io->err == cur))
                fd_defer(sh, job, cur);
            else if (!io || !(io->in == cur || io->out == cur || io->err == cur))
                sh->os.close(sh->os.data, cur);
        }
    }
}

static void fd_unwind(sh_shell *sh, int mark, const sh_io *io)
{
    fd_unwind_job(sh, mark, io, 0);
}

/* a job's status; once it has ended, the handles deferred for it are closed */
static long job_wait(sh_shell *sh, long job)
{
    long st = sh->os.wait(sh->os.data, job);
    if (st != SH_STOPPED)
        fd_defer_release(sh, job);
    return st;
}

/* exec kept the changes: the handles they replaced or closed go now */
static void fd_commit(sh_shell *sh, int mark)
{
    int i;
    for (i = mark; i < sh->nundo; i++)
        if (sh->fdundo[i].fh && sh->fdundo[i].own && !fd_used(sh, sh->fdundo[i].fh, 0))
            sh->os.close(sh->os.data, sh->fdundo[i].fh);
    sh->nundo = mark;
}

/* the handle behind descriptor n (0 when it is not open) */
static sh_fh fd_get(const sh_shell *sh, const sh_io *io, long n)
{
    if (n == 0)
        return io->in;
    if (n == 1)
        return io->out;
    if (n == 2)
        return io->err;
    if (n >= 3 && n < 3 + SH_FDMAX)
        return sh->fdt[n - 3].fh;
    return SH_NOFH;
}

/* the whole text is a descriptor number: its value, else -1 */
static long fd_number(const char *t)
{
    long v = 0;
    if (!t || !*t)
        return -1;
    for (; *t; t++) {
        if (*t < '0' || *t > '9' || v > 100000)
            return -1;
        v = v * 10 + (*t - '0');
    }
    return v;
}

/* /dev/stdin, /dev/stdout, /dev/stderr and /dev/fd/N name a descriptor of the shell: the
 * number, else -1. Redirection targets, test -e and source take them through the fd table. */
static long dev_fd(const char *path)
{
    if (strncmp(path, "/dev/", 5))
        return -1;
    if (!strcmp(path + 5, "stdin"))
        return 0;
    if (!strcmp(path + 5, "stdout"))
        return 1;
    if (!strcmp(path + 5, "stderr"))
        return 2;
    if (!strncmp(path + 5, "fd/", 3))
        return fd_number(path + 8);
    return -1;
}

/* a free slot for {var}> (from fd 10, as bash), or -1 */
static int fd_alloc(const sh_shell *sh)
{
    int i;
    for (i = 7; i < SH_FDMAX; i++)
        if (!sh->fdt[i].fh)
            return i;
    return -1;
}

/* The streams a command runs with: the parent's, with the redirections
 * applied. Streams opened here are marked owned. 0 = ok. */
static int redirect_one(sh_shell *sh, const sh_redir *r, const sh_io *parent, sh_io *io);

static int redirect(sh_shell *sh, const sh_redir *r, const sh_io *parent, sh_io *io)
{
    int mark = sh->nundo;
    *io = *parent;
    io->owned = 0;
    io->fdmark = mark;
    for (; r; r = r->next)
        if (redirect_one(sh, r, parent, io)) {
            if (sh->nundo > mark)
                fd_unwind(sh, mark, io);
            return -1;
        }
    if (sh->nundo > mark)
        io->owned |= SH_OWN_FDS;
    return 0;
}

/* descriptor dest (slot >= 0: a table slot) becomes another name for handle src, which was
 * descriptor sn: n>&m, n<&m, and a redirection to /dev/fd/m */
static int redir_dup(sh_shell *sh, const sh_redir *r, sh_io *io, long dest, int slot, long sn, sh_fh src)
{
    if (sn >= 3)
        SH_HIT(FD_HIGH);
    if (slot >= 0) {
        if (r->var)
            SH_HIT(FDVAR_ALLOC);
        if (fd_set(sh, slot, src, sn >= 3) < 0)
            return -1;
        if (r->var) {
            char nb[16];
            num(nb, dest);
            sh_set(&sh->ctx, r->var, nb);
        }
        return 0;
    }
    if (r->kind == SH_R_BOTH || r->kind == SH_R_BOTHAPP) {
        io->out = io->err = src;
        io->owned &= ~(SH_OWN_OUT | SH_OWN_ERR);
    } else if (r->fd == 2)
        io->err = src, io->owned &= ~SH_OWN_ERR;
    else if (r->fd == 1)
        io->out = src, io->owned &= ~SH_OWN_OUT;
    else if (r->fd == 0)
        io->in = src, io->owned &= ~SH_OWN_IN;
    return 0;
}

static int redirect_one(sh_shell *sh, const sh_redir *r, const sh_io *parent, sh_io *io)
{
    {
        sh_fh fh = SH_NOFH;
        long dest = r->fd;
        int slot = -1;
        if (r->var && r->kind == SH_R_CLOSE) {
            /* {var}>&-: the descriptor the variable holds */
            const char *v = sh_get(&sh->ctx, r->var);
            long n = fd_number(v);
            if (n < 3 || n >= 3 + SH_FDMAX) {
                err2(sh, parent, r->var, "bad file descriptor");
                return -1;
            }
            dest = n;
        }
        if (r->var && r->kind != SH_R_CLOSE) {
            slot = fd_alloc(sh);
            if (slot < 0) {
                err2(sh, parent, r->var, "too many open files");
                return -1;
            }
            dest = 3 + slot;
        } else if (dest >= 3) {
            if (dest >= 3 + SH_FDMAX) {
                char nb[16];
                num(nb, dest);
                err2(sh, parent, nb, "bad file descriptor");
                return -1;
            }
            slot = (int)(dest - 3);
        }
        if (r->kind == SH_R_DUPOUT || r->kind == SH_R_DUPIN) {
            /* 2>&1, 1>&2, 3>&1, 1>&3: the word is expanded, a descriptor number */
            char *tw = expand_one(sh, r->target, parent);
            long sn = fd_number(tw);
            sh_fh src = sn >= 0 ? fd_get(sh, io, sn) : SH_NOFH;
            if (!src) {
                err2(sh, parent, tw ? tw : r->target, "bad file descriptor");
                free(tw);
                return -1;
            }
            free(tw);
            return redir_dup(sh, r, io, dest, slot, sn, src);
        }
        if (r->kind == SH_R_CLOSE && slot >= 0)
            return sh->fdt[slot].fh ? fd_set(sh, slot, SH_NOFH, 0) : 0;
        if (r->kind == SH_R_CLOSE) {
            /* n>&- n<&-: the stream is closed; here it is the null device (a documented difference) */
            fh = sh->os.open(sh->os.data, dev_name("/dev/null"), r->fd == 0 ? SH_OPEN_READ : SH_OPEN_WRITE);
            if (!fh)
                return -1;
            if (r->fd != 0 && sh->nclosed < 8)
                sh->closed[sh->nclosed++] = fh;
        } else if (r->kind == SH_R_HEREDOC || r->kind == SH_R_HERESTR) {
            /* the document (here-string: the word and a newline) into a temp file, then read from it */
            char path[40], n[16];
            char *text = r->quoted ? sdup(r->target) : expand_one(sh, r->target, parent);
            sh_fh w;
            if (text && r->kind == SH_R_HERESTR) {
                char *t = (char *)malloc(strlen(text) + 2);
                if (t) {
                    strcpy(t, text);
                    strcat(t, "\n");
                }
                free(text);
                text = t;
                SH_HIT(HERESTRING);
            }
            SH_HIT(HEREDOC);
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
            tmp_add(sh, path, 0);
            fh = sh->os.open(sh->os.data, path, SH_OPEN_READ);
        } else {
            char *path = expand_one(sh, r->target, parent);
            int mode = r->kind == SH_R_IN ? SH_OPEN_READ
                     : r->kind == SH_R_RDWR ? SH_OPEN_RDWR
                     : r->kind == SH_R_APPEND || r->kind == SH_R_BOTHAPP ? SH_OPEN_APPEND : SH_OPEN_WRITE;
            long dn;
            if (!path)
                return -1;
            dn = dev_fd(path);
            if (dn >= 0) {
                /* /dev/fd/N and friends: another name for the shell's descriptor, not a file */
                sh_fh src = fd_get(sh, io, dn);
                if (!src) {
                    err2(sh, parent, path, "bad file descriptor");
                    free(path);
                    return -1;
                }
                free(path);
                return redir_dup(sh, r, io, dest, slot, dn, src);
            }
            if (mode == SH_OPEN_WRITE && r->kind != SH_R_CLOBBER && (sh->opts & SO_NOCLOBBER) && strncmp(path, "/dev/", 5) &&
                sh->os.stat && sh_exists(sh, path, 0)) {
                err2(sh, parent, path, "cannot overwrite existing file");
                free(path);
                return -1;
            }
            fh = sh->os.open(sh->os.data, dev_name(path), mode);
            if (!fh)
                err2(sh, parent, path, mode == SH_OPEN_READ ? "cannot open" : "cannot create");
            free(path);
        }
        if (!fh)
            return -1;
        if (slot >= 0) {
            /* a new handle for descriptor 3 and up, closed when its last slot is */
            if (fd_set(sh, slot, fh, 1) < 0) {
                sh->os.close(sh->os.data, fh);
                return -1;
            }
            if (r->var) {
                char nb[16];
                SH_HIT(FDVAR_ALLOC);
                num(nb, dest);
                sh_set(&sh->ctx, r->var, nb);
            }
            return 0;
        }
        if (r->kind == SH_R_BOTH || r->kind == SH_R_BOTHAPP) {
            if (io->owned & SH_OWN_OUT)
                closed_del(sh, io->out), sh->os.close(sh->os.data, io->out);
            io->out = io->err = fh;
            io->owned |= SH_OWN_OUT;
            io->owned &= ~SH_OWN_ERR;
        } else if (r->fd == 0) {
            if (io->owned & SH_OWN_IN)
                closed_del(sh, io->in), sh->os.close(sh->os.data, io->in);
            io->in = fh;
            io->owned |= SH_OWN_IN;
        } else if (r->fd == 2) {
            if (io->owned & SH_OWN_ERR)
                closed_del(sh, io->err), sh->os.close(sh->os.data, io->err);
            io->err = fh;
            io->owned |= SH_OWN_ERR;
        } else {
            if (io->owned & SH_OWN_OUT)
                closed_del(sh, io->out), sh->os.close(sh->os.data, io->out);
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

static int pf_escape(pbuf *b, const char **pp, int is_b);

static long b_echo(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i = 1, nl = 1, esc = shopt_get(sh, "xpg_echo"), stop = 0;
    for (; i < argc; i++) {
        const char *a = argv[i];
        int k;
        if (a[0] != '-' || !a[1])
            break;
        for (k = 1; a[k] && strchr("neE", a[k]); k++)
            ;
        if (a[k])
            break;
        for (k = 1; a[k]; k++) {
            if (a[k] == 'n')
                nl = 0;
            else
                esc = a[k] == 'e';
        }
    }
    for (; i < argc && !stop; i++) {
        if (!esc) {
            say(sh, io->out, argv[i]);
        } else {
            pbuf b = { 0, 0, 0 };
            const char *p;
            for (p = argv[i]; *p; p++) {
                if (*p == '\\' && p[1] && p[1] != '\'' && p[1] != '"' && !(p[1] >= '1' && p[1] <= '7')) {
                    if (pf_escape(&b, &p, 1)) {
                        stop = 1;
                        break;
                    }
                } else {
                    pb_add(&b, p, 1);
                }
            }
            if (b.s)
                sh->os.write(sh->os.data, io->out, b.s, b.n);
            free(b.s);
        }
        if (i + 1 < argc && !stop)
            say(sh, io->out, " ");
    }
    if (nl && !stop)
        say(sh, io->out, "\n");
    return 0;
}

/* printf: one backslash escape at *pp (on the backslash); %b's octal
 * form is \0nnn. Returns 1 at \c (stop all output). is_b: 0 printf format, 1 %b, 2 $'...' and
 * ${x@E} (\E, \cX, \? as bash reads them). */
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
    case 'E':
        if (is_b != 2) {
            pb_add(b, "\\", 1);
            out = c;
        } else
            out = 27;
        break;
    case '?':
        if (is_b != 2)
            pb_add(b, "\\", 1);
        out = c;
        break;
    case 'c':
        if (is_b == 1) {
            *pp = p;
            return 1;
        }
        if (is_b == 2 && p[1]) {
            p++;
            out = *p == '?' ? 127 : (char)(*p & 31);
            break;
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
    case 'u':
    case 'U': {
        int want = c == 'u' ? 4 : 8;
        unsigned long cp = 0;
        char u[4];
        int ul;
        while (n < want && ((p[1] >= '0' && p[1] <= '9') || ((p[1] | 32) >= 'a' && (p[1] | 32) <= 'f'))) {
            p++;
            cp = cp * 16 + (unsigned long)(*p <= '9' ? *p - '0' : (*p | 32) - 'a' + 10);
            n++;
        }
        if (!n) {
            pb_add(b, "\\", 1);
            out = c;
            break;
        }
        if (cp < 0x80) {
            u[0] = (char)cp, ul = 1;
        } else if (cp < 0x800) {
            u[0] = (char)(0xC0 | (cp >> 6)), u[1] = (char)(0x80 | (cp & 0x3F)), ul = 2;
        } else if (cp < 0x10000) {
            u[0] = (char)(0xE0 | (cp >> 12)), u[1] = (char)(0x80 | ((cp >> 6) & 0x3F)), u[2] = (char)(0x80 | (cp & 0x3F)), ul = 3;
        } else {
            u[0] = (char)(0xF0 | (cp >> 18)), u[1] = (char)(0x80 | ((cp >> 12) & 0x3F)), u[2] = (char)(0x80 | ((cp >> 6) & 0x3F)), u[3] = (char)(0x80 | (cp & 0x3F)), ul = 4;
        }
        pb_add(b, u, ul);
        *pp = p;
        return 0;
    }
    case 0:
        pb_add(b, "\\", 1);
        *pp = p - 1;
        return 0;
    default:
        if (c >= '0' && c <= '7') {
            if (is_b != 1 || c != '0')
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

static void pf_badnum(sh_shell *sh, const sh_io *io, const char *a, const char *why)
{
    char *m = (char *)malloc(strlen(a) + 10);
    if (m) {
        strcpy(m, "printf: ");
        strcat(m, a);
        err2(sh, io, m, why);
        free(m);
    }
}

/* printf: a numeric argument ('c gives the character's value) */
static long pf_number(sh_shell *sh, const sh_io *io, const char *a, int *bad)
{
    char *end;
    long v;
    if (!a)
        return 0;
    if (*a == '\'' || *a == '"')
        return (unsigned char)a[1];
    v = strtol(a, &end, 0);
    if (*end || end == a) {
        pf_badnum(sh, io, a, "invalid number");
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

/* strftime for printf '%(fmt)T', the C locale, UTC (the Amiga has no zone here): out has max bytes */
static int tf_leap(long y)
{
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

static int tf_weeks(long y)
{
    long a = (y + y / 4 - y / 100 + y / 400) % 7, b = (y - 1 + (y - 1) / 4 - (y - 1) / 100 + (y - 1) / 400) % 7;
    return a == 4 || b == 3 ? 53 : 52;
}

static void sh_strftime(pbuf *o, const char *fmt, long t)
{
    static const char *const wd[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static const char *const mn[] = {"January", "February", "March", "April", "May", "June", "July", "August",
                                     "September", "October", "November", "December"};
    static const int cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    long days = t >= 0 ? t / 86400 : -((-t + 86399) / 86400), sec = t - days * 86400;
    long z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097, doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, y = yoe + era * 400;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1;
    long mo = mp < 10 ? mp + 3 : mp - 9, H = sec / 3600, M = sec / 60 % 60, S = sec % 60;
    int w = (int)(((days + 4) % 7 + 7) % 7), yd, iw, k;
    long iy = y;
    char n[24];
    if (mo <= 2)
        y++;
    iy = y;
    yd = cum[mo - 1] + (int)d - 1 + (mo > 2 && tf_leap(y));
    iw = (yd - (w + 6) % 7 + 10) / 7;
    if (iw < 1) {
        iy--;
        iw = tf_weeks(iy);
    } else if (iw > tf_weeks(y)) {
        iw = 1;
        iy++;
    }
    for (; *fmt; fmt++) {
        long v = -1;
        int pad = 2;
        char c, padc = '0';
        const char *str = 0;
        if (*fmt != '%' || !fmt[1]) {
            pb_add(o, fmt, 1);
            continue;
        }
        c = *++fmt;
        while ((c == '-' || c == '_' || c == '^' || c == '#' || c == '0') && fmt[1])
            c = *++fmt;
        switch (c) {
        case 'a': str = wd[w]; pad = -3; break;
        case 'A': str = wd[w]; break;
        case 'b': case 'h': str = mn[mo - 1]; pad = -3; break;
        case 'B': str = mn[mo - 1]; break;
        case 'p': str = H < 12 ? "AM" : "PM"; break;
        case 'P': str = H < 12 ? "am" : "pm"; break;
        case 'Z': str = "UTC"; break;
        case 'z': str = "+0000"; break;
        case 'n': str = "\n"; break;
        case 't': str = "\t"; break;
        case '%': str = "%"; break;
        case 'C': v = y / 100; break;
        case 'd': v = d; break;
        case 'e': v = d; padc = ' '; break;
        case 'H': v = H; break;
        case 'k': v = H; padc = ' '; break;
        case 'I': v = H % 12 ? H % 12 : 12; break;
        case 'l': v = H % 12 ? H % 12 : 12; padc = ' '; break;
        case 'j': v = yd + 1; pad = 3; break;
        case 'm': v = mo; break;
        case 'M': v = M; break;
        case 'S': v = S; break;
        case 's': v = t; pad = 1; break;
        case 'u': v = w ? w : 7; pad = 1; break;
        case 'w': v = w; pad = 1; break;
        case 'U': v = (yd + 7 - w) / 7; break;
        case 'W': v = (yd + 7 - (w + 6) % 7) / 7; break;
        case 'V': v = iw; break;
        case 'G': v = iy; pad = 1; break;
        case 'g': v = iy % 100; break;
        case 'y': v = y % 100; break;
        case 'Y': v = y; pad = 1; break;
        case 'c': sh_strftime(o, "%a %b %e %H:%M:%S %Y", t); continue;
        case 'D': case 'x': sh_strftime(o, "%m/%d/%y", t); continue;
        case 'F': sh_strftime(o, "%Y-%m-%d", t); continue;
        case 'T': case 'X': sh_strftime(o, "%H:%M:%S", t); continue;
        case 'R': sh_strftime(o, "%H:%M", t); continue;
        case 'r': sh_strftime(o, "%I:%M:%S %p", t); continue;
        default:
            pb_add(o, fmt - 1, 2);
            continue;
        }
        if (str) {
            pb_add(o, str, pad < 0 ? -pad : (long)strlen(str));
            continue;
        }
        sh_ltoa(v, n);
        for (k = (int)strlen(n); k < pad; k++)
            pb_add(o, &padc, 1);
        pb_add(o, n, (long)strlen(n));
    }
}

static long b_printf(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    pbuf b = { 0, 0, 0 };
    int bad = 0, stop = 0;
    const char *p, *vname = 0;
    if (argc > 2 && !strcmp(argv[1], "-v")) {
        vname = argv[2];
        argc -= 2;
        argv += 2;
    }
    if (argc > 1 && !strcmp(argv[1], "--")) {
        argc--;
        argv++;
    }
    if (argc < 2) {
        err2(sh, io, "printf", "usage: printf [-v var] format [arguments]");
        return 2;
    }
    {
    int ai = 2;
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
            if (*p == '(' && strchr(p, ')') && strchr(p, ')')[1] == 'T') {
                /* %(fmt)T: the seconds since the epoch of the argument (-1 or none: now, -2: the shell's start) */
                const char *e = strchr(p, ')');
                char *fmt = (char *)malloc((size_t)(e - p));
                pbuf t = { 0, 0, 0 };
                long us, secs;
                if (fmt) {
                    memcpy(fmt, p + 1, (size_t)(e - p - 1));
                    fmt[e - p - 1] = 0;
                    secs = ai < argc && argv[ai][0] ? pf_number(sh, io, argv[ai], &bad) : -1;
                    ai++;
                    if (secs == -1)
                        secs = sh_now(sh, &us);
                    else if (secs == -2)
                        secs = sh->secs0;
                    sh_strftime(&t, fmt, secs);
                    if (!t.s)
                        pb_add(&t, "", 0);
                    if (t.s && prec >= 0 && prec < t.n)
                        t.s[prec] = 0;
                    if (t.s)
                        pf_field(&b, t.s, 0, width, left, 0);
                    free(t.s);
                    free(fmt);
                }
                p = e + 1;
                continue;
            }
            while (*p == 'l' || *p == 'L' || *p == 'h')
                p++;
            spec = *p;
            if (!spec)
                break;
            arg = ai < argc ? argv[ai] : 0;
            ai++;
            if (spec == 's' || spec == 'b' || spec == 'c' || spec == 'q') {
                pbuf t = { 0, 0, 0 };
                const char *a = arg ? arg : "";
                if (spec == 'q') {
                    char *q = sh_quote(a, SH_Q_BACKSLASH);
                    if (q)
                        pb_str(&t, q);
                    free(q);
                } else if (spec == 'b') {
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
            } else if (spec && strchr("fFeEgG", spec)) {
                char *fb = (char *)malloc(SH_FLOAT_BODY);
                int fpl = 0, fz = 0;
                if (fb) {
                    int fe = sh_float_format(arg, spec, plus, space, alt, prec, fb, &fpl, &fz);
                    if (fe) {
                        pf_badnum(sh, io, arg, fe == 2 ? "Result too large" : "invalid number");
                        bad = 1;
                    }
                    pf_field(&b, fb, fpl, width, left, zero && fz);
                    free(fb);
                }
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
    }
    if (vname) {
        if (sh_set(&sh->ctx, vname, b.s ? b.s : ""))
            bad = 1;
    } else if (b.s)
        put(sh, io->out, b.s, b.n);
    free(b.s);
    return bad;
}

static long b_cd(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int a = 1;
    const char *dir;
    char *old, cp[512];
    int printit = 0, physical = 0;
    /* -P: PWD is the physical path (the OS layer's realpath); -L, the default, keeps what the OS layer's cwd says */
    for (; a < argc && argv[a][0] == '-' && argv[a][1] && strcmp(argv[a], "--"); a++) {
        const char *o = argv[a] + 1;
        for (; *o; o++) {
            if (*o == 'P')
                physical = 1;
            else if (*o == 'L')
                physical = 0;
            if (*o != 'L' && *o != 'P' && *o != 'e' && *o != '@') {
                char opt[3];
                opt[0] = '-';
                opt[1] = *o;
                opt[2] = 0;
                sayl(sh, io->err, "vsh: cd: ", opt, ": invalid option\n", NULL);
                say(sh, io->err, "cd: usage: cd [-L|[-P [-e]] [-@]] [dir]\n");
                return 2;
            }
        }
    }
    if (a < argc && !strcmp(argv[a], "--"))
        a++;
    if (argc - a > 1) {
        err2(sh, io, "cd", "too many arguments");
        return 2;
    }
    dir = a < argc ? argv[a] : sh_get(&sh->ctx, "HOME");
    old = sh->os.cwd(sh->os.data);
    if (!dir)
        dir = "SYS:";
    if (!strcmp(dir, "-")) {
        dir = sh_get(&sh->ctx, "OLDPWD") ? sh_get(&sh->ctx, "OLDPWD") : "";
        printit = 1;
    } else if (dir[0] && dir[0] != '/' && !strchr(dir, ':') && strncmp(dir, "./", 2) && strncmp(dir, "../", 3)
               && strcmp(dir, ".") && strcmp(dir, "..")) {
        /* CDPATH: each directory of it is tried before the current one (an empty entry is the current one) */
        const char *p = sh_get(&sh->ctx, "CDPATH");
        while (p) {
            const char *e = strchr(p, ':');
            long n = e ? (long)(e - p) : (long)strlen(p);
            if (n && n + (long)strlen(dir) + 2 < (long)sizeof(cp)) {
                memcpy(cp, p, (size_t)n);
                cp[n] = 0;
                if (cp[n - 1] != '/' && cp[n - 1] != ':')
                    strcat(cp, "/");
                strcat(cp, dir);
                if (sh_exists(sh, cp, 1) && !sh->os.chdir(sh->os.data, cp)) {
                    SH_HIT(CDPATH_USED);
                    printit = 1;
                    dir = 0;
                    break;
                }
            }
            p = e ? e + 1 : 0;
        }
    }
    if (dir && sh->os.chdir(sh->os.data, dir)) {
        err2(sh, io, "cd", dir);
        free(old);
        return 1;
    }
    if (old)
        sh_set(&sh->ctx, "OLDPWD", old);
    free(old);
    old = sh->os.cwd(sh->os.data);
    if (old && physical && sh->os.realpath) {
        char *rp = sh->os.realpath(sh->os.data, old);
        if (rp) {
            SH_HIT(REALPATH);
            free(old);
            old = rp;
        }
    }
    if (old) {
        sh_set(&sh->ctx, "PWD", old);
        if (printit)
            sayl(sh, io->out, old, "\n", NULL);
    }
    free(old);
    return 0;
}

static long b_pwd(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    char *d;
    int a;
    for (a = 1; a < argc && argv[a][0] == '-' && argv[a][1]; a++) {
        if (!strcmp(argv[a], "--")) {
            a++;
            break;
        }
        if (strcmp(argv[a], "-L") && strcmp(argv[a], "-P") && strcmp(argv[a], "-LP") && strcmp(argv[a], "-PL")) {
            sayl(sh, io->err, "vsh: pwd: ", argv[a], ": invalid option\n", NULL);
            say(sh, io->err, "pwd: usage: pwd [-LP]\n");
            return 2;
        }
    }
    d = sh->os.cwd(sh->os.data);
    /* pwd -P (the last of -L -P wins): the physical path */
    if (d && sh->os.realpath) {
        int phys = 0, b;
        for (b = 1; b < a; b++) {
            const char *o = argv[b] + 1;
            for (; argv[b][0] == '-' && *o; o++)
                if (*o == 'P' || *o == 'L')
                    phys = *o == 'P';
        }
        if (phys) {
            char *rp = sh->os.realpath(sh->os.data, d);
            if (rp) {
                SH_HIT(REALPATH);
                free(d);
                d = rp;
            }
        }
    }
    sayl(sh, io->out, d ? d : "", "\n", NULL);
    free(d);
    return 0;
}

/* "text" with \ " $ ` escaped, as declare -p prints a value */
static void pb_dq(pbuf *o, const char *t)
{
    char *q = sh_dquote(t);
    pb_str(o, q ? q : "");
    free(q);
}

/* ([0]="x" [1]="y"): an associative array has a space before the closing paren (bash) */
static void pb_array(pbuf *o, const sh_var *v)
{
    long i;
    char d[24];
    pb_add(o, "(", 1);
    for (i = 0; i < v->arr->n; i++) {
        const sh_elem *el = v->arr->e + i;
        if (i)
            pb_add(o, " ", 1);
        pb_add(o, "[", 1);
        if (v->attr & SH_ATTR_ASSOC) {
            const char *k;
            int plain = 1;
            for (k = el->key; *k; k++)
                if (!((*k >= 'a' && *k <= 'z') || (*k >= 'A' && *k <= 'Z') || (*k >= '0' && *k <= '9') || *k == '_'))
                    plain = 0;
            if (plain && *el->key)
                pb_str(o, el->key);
            else
                pb_dq(o, el->key);
        } else {
            sh_ltoa(el->idx, d);
            pb_str(o, d);
        }
        pb_add(o, "]=", 2);
        pb_dq(o, el->val ? el->val : "");
    }
    pb_str(o, (v->attr & SH_ATTR_ASSOC) && v->arr->n ? " )" : ")");
}

static void say_dq(sh_shell *sh, const sh_io *io, const char *t)
{
    pbuf o = { 0, 0, 0 };
    pb_dq(&o, t);
    if (o.s)
        say(sh, io->out, o.s);
    free(o.s);
}

static void say_array(sh_shell *sh, const sh_io *io, const sh_var *v)
{
    pbuf o = { 0, 0, 0 };
    pb_array(&o, v);
    if (o.s)
        say(sh, io->out, o.s);
    free(o.s);
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int cmp_var(const void *a, const void *b)
{
    return strcmp((*(const sh_var *const *)a)->name, (*(const sh_var *const *)b)->name);
}

/* every variable, by name, in a malloc'd vector (NULL when out of memory); *n is its length */
static sh_var **sorted_vars(sh_shell *sh, int *n)
{
    sh_var *v, **all;
    int k = 0;
    for (v = sh->ctx.vars; v; v = v->next)
        k++;
    all = (sh_var **)malloc((size_t)(k ? k : 1) * sizeof(sh_var *));
    if (!all)
        return NULL;
    for (v = sh->ctx.vars, k = 0; v; v = v->next)
        all[k++] = v;
    qsort(all, (size_t)k, sizeof(sh_var *), cmp_var);
    *n = k;
    return all;
}

/* set -o (or +o) with no name: the option list, as bash prints it */
static void list_opts(sh_shell *sh, const sh_io *io, int as_commands)
{
    int i, k;
    for (i = 0; i < N_SHOPT; i++) {
        const char *nm = sh_optab[i].name;
        if (as_commands) {
            sayl(sh, io->out, opt_on(sh, i) ? "set -o " : "set +o ", nm, NULL);
        } else {
            say(sh, io->out, nm);
            for (k = (int)strlen(nm); k < 15; k++)
                say(sh, io->out, " ");
            say(sh, io->out, opt_on(sh, i) ? "\ton" : "\toff");
        }
        say(sh, io->out, "\n");
    }
}

static long b_set(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i = 1, positional = 0;
    if (argc == 1) {
        sh_var **all;
        int n, k;
        all = sorted_vars(sh, &n);
        if (!all)
            return 1;
        for (k = 0; k < n; k++) {
            const char *sv = sh_var_str(all[k]);
            char *q = sv && sv[0] ? sh_quote(sv, SH_Q_SINGLE) : sdup("");
            sayl(sh, io->out, all[k]->name, "=", NULL);
            if (all[k]->arr)
                say_array(sh, io, all[k]);
            else
                say(sh, io->out, q ? q : "");
            say(sh, io->out, "\n");
            free(q);
        }
        free(all);
        return 0;
    }
    for (; i < argc; i++) {
        const char *a = argv[i], *p;
        int on;
        if (a[0] != '-' && a[0] != '+')
            break;
        if (!strcmp(a, "--")) {
            i++;
            positional = 1;
            break;
        }
        if (!strcmp(a, "-")) {
            sh->opts &= ~(SO_XTRACE | SO_VERBOSE);
            opts_apply(sh);
            i++;
            positional = 1;
            break;
        }
        on = a[0] == '-';
        for (p = a + 1; *p; p++) {
            if (*p == 'o') {
                if (i + 1 < argc && argv[i + 1][0] != '-' && argv[i + 1][0] != '+') {
                    i++;
                    if (!opt_name(sh, argv[i], on)) {
                        err2(sh, io, argv[i], "invalid option name");
                        return 2;
                    }
                } else {
                    list_opts(sh, io, !on);
                }
            } else if (!opt_letter(sh, *p, on)) {
                char o[3];
                o[0] = a[0];
                o[1] = *p;
                o[2] = 0;
                err2(sh, io, o, "invalid option");
                return 2;
            }
        }
    }
    if (positional || i < argc) {
        sh_list_free(&sh->ctx.args);
        for (; i < argc; i++)
            sh_list_add(&sh->ctx.args, argv[i]);
    }
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
    if (argc > 1) {
        const char *q = argv[1] + (argv[1][0] == '-' || argv[1][0] == '+');
        if (!*q || strspn(q, "0123456789") != strlen(q)) {
            /* bash: not a number: the message and status 2; only posix mode also ends the shell */
            err2(sh, io, "exit", "numeric argument required");
            posix_fatal(sh, 2);
            return 2;
        }
    }
    sh->exiting = 1;
    sh->exit_status = (argc > 1 ? atol(argv[1]) : sh->ctx.status) & 255;
    return sh->exit_status;
}

static long b_return(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    if (!sh->nframes && !sh->func_depth && (sh->opts & SO_POSIX)) {
        /* bash --posix: return outside a function or a sourced file is an error */
        err2(sh, io, "return", "can only `return' from a function or sourced script");
        posix_fatal(sh, 2);
        return 2;
    }
    sh->returning = 1;
    return (argc > 1 ? atol(argv[1]) : sh->ctx.status) & 255;
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

/* read [-rs] [-d delim] [-n count] [-N count] [-p prompt] [-t seconds] [-u fd] [-e] [name ...]: one
 * line (or up to delim, or count characters), split by $IFS (IFS whitespace trimmed and collapsed;
 * other IFS characters end one field each), the last name takes the rest. Without -r a backslash
 * quotes the next character (and joins the next line at the end). No names: REPLY. -d, -n, -N and -s
 * read byte by byte; -t waits per byte (sh_os.ready); -e is accepted and ignored (the console edits).
 * Status: 1 at the end of input, 142 on a timeout, 2 for a bad option. */
#define RD_MAX 1024

static long b_read(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    char line[RD_MAX], quoted[RD_MAX], buf[RD_MAX];
    const char *ifs = sh_get(&sh->ctx, "IFS");
    const char *reply[1], *prompt = 0, *aname = 0;
    char **names = argv;
    sh_fh in = io->in;
    long n = 0, m, k, p = 0, nch = -1, tmo = -1;
    int raw = 0, a = 1, i, got = 0, delim = '\n', exact = 0, silent = 0, chars = 0, tout = 0;
    if (!ifs)
        ifs = " \t\n";
    for (; a < argc && argv[a][0] == '-' && argv[a][1]; a++) {
        const char *o = argv[a] + 1;
        if (!strcmp(argv[a], "--")) {
            a++;
            break;
        }
        for (; *o; o++) {
            const char *arg = 0;
            if (strchr("adnNptu", *o)) {
                if (o[1])
                    arg = o + 1;
                else if (a + 1 < argc)
                    arg = argv[++a];
                else {
                    char opt[3];
                    opt[0] = '-';
                    opt[1] = *o;
                    opt[2] = 0;
                    err2(sh, io, opt, "option requires an argument");
                    return 2;
                }
            }
            switch (*o) {
            case 'r': raw = 1; break;
            case 'a': aname = arg; break;
            case 's': silent = chars = 1; break;
            case 'e': break;
            case 'd': delim = arg[0] ? (unsigned char)arg[0] : -1; chars = 1; break;
            case 'n': case 'N':
                nch = atol(arg);
                exact = *o == 'N';
                chars = 1;
                break;
            case 'p': prompt = arg; break;
            case 't': tmo = atol(arg) * 1000; break;
            case 'u': {
                long fd = fd_number(arg);
                in = fd >= 0 ? fd_get(sh, io, fd) : SH_NOFH;
                if (!in) {
                    err2(sh, io, arg, fd >= 0 ? "invalid file descriptor: bad file descriptor" : "invalid file descriptor");
                    return 1;
                }
                if (fd >= 3)
                    SH_HIT(FD_HIGH);
                break;
            }
            default: {
                char opt[3];
                opt[0] = '-';
                opt[1] = *o;
                opt[2] = 0;
                err2(sh, io, opt, "invalid option");
                return 2;
            }
            }
            if (arg)
                break;
        }
    }
    if (a == argc) {
        reply[0] = "REPLY";
        names = (char **)reply;
        a = 0;
        argc = 1;
    }
    if (prompt && sh->os.isatty && sh->os.isatty(sh->os.data, in))
        sh->os.write(sh->os.data, io->err, prompt, (long)strlen(prompt));
    if (nch == 0) {
        got = 1;
    } else if (chars) {
        int esc = 0;
        if (silent && sh->os.echo && sh->os.isatty(sh->os.data, in))
            sh->os.echo(sh->os.data, in, 0);
        else
            silent = 0;
        for (;;) {
            char c;
            if (tmo >= 0 && sh->os.ready && !sh->os.ready(sh->os.data, in, tmo)) {
                tout = 1;
                break;
            }
            if (sh->os.read(sh->os.data, in, &c, 1) <= 0) {
                got = got ? 2 : 0;
                break;
            }
            got = 1;
            if (silent && c == '\r')
                c = '\n';
            if (esc) {
                esc = 0;
                if (c != '\n' && n < RD_MAX - 1) {
                    line[n] = c;
                    quoted[n++] = 1;
                }
            } else if (!exact && (delim < 0 ? c == 0 : (unsigned char)c == (unsigned char)delim)) {
                break;
            } else if (!raw && !exact && c == '\\') {
                esc = 1;
                continue;
            } else if (n < RD_MAX - 1) {
                line[n] = c;
                quoted[n++] = 0;
            }
            if (nch > 0 && n >= nch)
                break;
        }
        if (silent) {
            sh->os.echo(sh->os.data, in, 1);
            sh->os.write(sh->os.data, io->err, "\n", 1);
        }
    } else
        for (;;) {
            int more = 0;
            if (tmo >= 0 && sh->os.ready && !sh->os.ready(sh->os.data, in, tmo)) {
                tout = 1;
                break;
            }
            m = sh->os.read_line(sh->os.data, in, buf, sizeof(buf));
            if (m < 0)
                break;
            got = buf[m - 1] == '\n' || m >= (long)sizeof(buf) - 1 ? 1 : 2; /* no newline: the input ended in the line */
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
    if (aname) { /* read -a: every field is an element of the array */
        long idx = 0;
        char d[24];
        if (sh_array_reset(&sh->ctx, aname, 0)) {
            err2(sh, io, aname, "readonly variable");
            return 1;
        }
        for (;;) {
            long st0;
            char sv;
            while (p < n && !quoted[p] && ifs_space(ifs, line[p]))
                p++;
            if (p >= n)
                break;
            st0 = p;
            while (p < n && (quoted[p] || !in_ifs(ifs, line[p])))
                p++;
            sv = line[p];
            line[p] = 0;
            sh_ltoa(idx++, d);
            sh_assign(&sh->ctx, aname, d, line + st0, 0);
            line[p] = sv;
            while (p < n && !quoted[p] && ifs_space(ifs, line[p]))
                p++;
            if (p < n && !quoted[p] && in_ifs(ifs, line[p]))
                p++;
        }
        return tout ? 142 : got == 1 ? 0 : 1;
    }
    for (i = a; i < argc; i++) {
        long start;
        if (exact && nch > 0) { /* -N: the characters as they are, no splitting */
            set_part(sh, names[i], line, n);
            for (i++; i < argc; i++)
                set_part(sh, names[i], "", 0);
            break;
        }
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
    if (tout)
        return 142;
    return got == 1 ? 0 : 1;
}

static long b_alias(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i, k;
    if (argc == 1) {
        for (k = 0; k < sh->aliases.n; k++) {
            sayl(sh, io->out, "alias ", sh->aliases.v[k], "\n", NULL);
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
                sayl(sh, io->out, sh->aliases.v[k], "\n", NULL);
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

/* ---- test / [ --------------------------------------------------------------- */

typedef struct tctx {
    sh_shell *sh;
    const sh_io *io;
    int err;               /* 2: a syntax error was reported */
} tctx;

static int t_fail(tctx *t, const char *what)
{
    if (!t->err)
        err2(t->sh, t->io, "test", what);
    t->err = 2;
    return 0;
}

static int t_unary_op(const char *s)
{
    return s[0] == '-' && s[1] && !s[2] && strchr("abcdefghknoprstuvwxzGLNORS", s[1]);
}

static const char *const t_binops[] = { "=", "==", "!=", "<", ">", "-eq", "-ne", "-lt", "-le", "-gt", "-ge",
                                        "-nt", "-ot", "-ef", 0 };

static int t_binary_op(const char *s)
{
    int i;
    for (i = 0; t_binops[i]; i++)
        if (!strcmp(s, t_binops[i]))
            return 1;
    return 0;
}

static int t_int(tctx *t, const char *s, long *v)
{
    const char *p = s;
    int neg = 0;
    long r = 0;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '-' || *p == '+')
        neg = *p++ == '-';
    if (*p < '0' || *p > '9') {
        char m[96];
        strcpy(m, s);
        strcat(m, ": integer expression expected");
        t_fail(t, m);
        return 0;
    }
    while (*p >= '0' && *p <= '9')
        r = r * 10 + (*p++ - '0');
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p) {
        char m[96];
        strcpy(m, s);
        strcat(m, ": integer expression expected");
        t_fail(t, m);
        return 0;
    }
    *v = neg ? -r : r;
    return 1;
}

/* one unary operator (-e FILE, -z STR, -t FD, -v NAME, -o OPTION ...) */
static int sh_test_unary(tctx *t, char op, const char *a)
{
    sh_shell *sh = t->sh;
    sh_stat st;
    switch (op) {
    case 'z': return !a[0];
    case 'n': return a[0] != 0;
    case 't': {
        int fd = atoi(a);
        sh_fh fh = fd == 0 ? t->io->in : fd == 1 ? t->io->out : fd == 2 ? t->io->err : SH_NOFH;
        return fh != SH_NOFH && sh->os.isatty && sh->os.isatty(sh->os.data, fh);
    }
    case 'v':
        return sh_get(&sh->ctx, a) != 0 || (a[0] >= '1' && a[0] <= '9' && atoi(a) <= sh->ctx.args.n);
    case 'R': return 0;
    case 'o': {
        int i;
        for (i = 0; i < N_SHOPT; i++)
            if (!strcmp(sh_optab[i].name, a))
                return opt_on(sh, i);
        return 0;
    }
    }
    if ((op == 'e' || op == 'a' || op == 'r' || op == 'w') && dev_fd(a) >= 0)
        return fd_get(sh, t->io, dev_fd(a)) != SH_NOFH; /* /dev/fd/N: open in the shell's table */
    if (!sh->os.stat || sh->os.stat(sh->os.data, a, &st, op == 'L' || op == 'h'))
        return 0;
    switch (op) {
    case 'a': case 'e': return 1;
    case 'f': return st.type == SH_ST_FILE;
    case 'd': return st.type == SH_ST_DIR;
    case 'b': return st.type == SH_ST_BLOCK;
    case 'c': return st.type == SH_ST_CHAR;
    case 'p': return st.type == SH_ST_FIFO;
    case 'S': return st.type == SH_ST_SOCK;
    case 'L': case 'h': return st.link;
    case 's': return st.size > 0;
    case 'r': return (st.access & 4) != 0;
    case 'w': return (st.access & 2) != 0;
    case 'x': return (st.access & 1) != 0;
    case 'u': return (st.mode & 04000) != 0;
    case 'g': return (st.mode & 02000) != 0;
    case 'k': return (st.mode & 01000) != 0;
    case 'O': return st.owned;
    case 'G': return st.group;
    case 'N': return st.mtime >= st.atime;
    }
    return 0;
}

static int sh_test_binary(tctx *t, const char *a, const char *op, const char *b)
{
    long x, y;
    SH_HIT(TEST_BINARY);
    if (!strcmp(op, "=") || !strcmp(op, "=="))
        return !strcmp(a, b);
    if (!strcmp(op, "!="))
        return strcmp(a, b) != 0;
    if (!strcmp(op, "<"))
        return strcmp(a, b) < 0;
    if (!strcmp(op, ">"))
        return strcmp(a, b) > 0;
    if (!strcmp(op, "-nt") || !strcmp(op, "-ot") || !strcmp(op, "-ef")) {
        sh_stat sa, sb;
        int ea = t->sh->os.stat && !t->sh->os.stat(t->sh->os.data, a, &sa, 0);
        int eb = t->sh->os.stat && !t->sh->os.stat(t->sh->os.data, b, &sb, 0);
        if (!strcmp(op, "-nt"))
            return ea && (!eb || sa.mtime > sb.mtime);
        if (!strcmp(op, "-ot"))
            return eb && (!ea || sa.mtime < sb.mtime);
        return ea && eb && sa.dev == sb.dev && sa.ino == sb.ino;
    }
    if (!t_int(t, a, &x) || !t_int(t, b, &y))
        return 0;
    if (!strcmp(op, "-eq")) return x == y;
    if (!strcmp(op, "-ne")) return x != y;
    if (!strcmp(op, "-lt")) return x < y;
    if (!strcmp(op, "-le")) return x <= y;
    if (!strcmp(op, "-gt")) return x > y;
    return x >= y;
}

static int t_or(tctx *t, char **a, int n, int *pos);

/* primary: ( expr ), unary arg, arg binop arg, or a string (true when not empty) */
static int t_primary(tctx *t, char **a, int n, int *pos)
{
    int p = *pos, r;
    if (p >= n)
        return t_fail(t, "argument expected");
    if (!strcmp(a[p], "(") && p + 1 < n) {
        *pos = p + 1;
        r = t_or(t, a, n, pos);
        if (*pos < n && !strcmp(a[*pos], ")"))
            (*pos)++;
        else
            t_fail(t, "`)' expected");
        return r;
    }
    if (p + 2 < n + 0 && t_binary_op(a[p + 1]) && !(p + 2 >= n)) {
        *pos = p + 3;
        return sh_test_binary(t, a[p], a[p + 1], a[p + 2]);
    }
    if (t_unary_op(a[p]) && p + 1 < n && !(p + 2 < n && t_binary_op(a[p + 1]))) {
        *pos = p + 2;
        return sh_test_unary(t, a[p][1], a[p + 1]);
    }
    *pos = p + 1;
    return a[p][0] != 0;
}

static int t_not(tctx *t, char **a, int n, int *pos)
{
    if (*pos < n && !strcmp(a[*pos], "!") && *pos + 1 < n) {
        (*pos)++;
        return !t_not(t, a, n, pos);
    }
    return t_primary(t, a, n, pos);
}

static int t_and(tctx *t, char **a, int n, int *pos)
{
    int r = t_not(t, a, n, pos);
    while (*pos < n && !strcmp(a[*pos], "-a")) {
        int q;
        (*pos)++;
        q = t_not(t, a, n, pos);
        r = r && q;
    }
    return r;
}

static int t_or(tctx *t, char **a, int n, int *pos)
{
    int r = t_and(t, a, n, pos);
    while (*pos < n && !strcmp(a[*pos], "-o")) {
        int q;
        (*pos)++;
        q = t_and(t, a, n, pos);
        r = r || q;
    }
    return r;
}

/* the argument-count rules of POSIX (0 to 4 arguments), bash's -a -o ! ( ) beyond */
static int t_eval(tctx *t, char **a, int n)
{
    int pos = 0, r;
    if (n == 0)
        return 0;
    if (n == 1)
        return a[0][0] != 0;
    if (n == 2) {
        if (!strcmp(a[0], "!"))
            return !a[1][0];
        if (t_unary_op(a[0]))
            return sh_test_unary(t, a[0][1], a[1]);
        return t_fail(t, "unary operator expected");
    }
    if (n == 3) {
        if (t_binary_op(a[1]))
            return sh_test_binary(t, a[0], a[1], a[2]);
        if (!strcmp(a[1], "-a"))
            return a[0][0] && a[2][0];
        if (!strcmp(a[1], "-o"))
            return a[0][0] || a[2][0];
        if (!strcmp(a[0], "!"))
            return !t_eval(t, a + 1, 2);
        if (!strcmp(a[0], "(") && !strcmp(a[2], ")"))
            return a[1][0] != 0;
        return t_fail(t, "binary operator expected");
    }
    if (n == 4) {
        if (!strcmp(a[0], "!"))
            return !t_eval(t, a + 1, 3);
        if (!strcmp(a[0], "(") && !strcmp(a[3], ")"))
            return t_eval(t, a + 1, 2);
    }
    r = t_or(t, a, n, &pos);
    if (!t->err && pos < n)
        t_fail(t, "too many arguments");
    return r;
}

static long b_test(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    tctx t;
    int n = argc - 1, r;
    char **a = argv + 1;
    t.sh = sh;
    t.io = io;
    t.err = 0;
    if (!strcmp(argv[0], "[")) {
        if (n < 1 || strcmp(a[n - 1], "]")) {
            err2(sh, io, "[", "] is missing");
            return 2;
        }
        n--;
    }
    r = t_eval(&t, a, n);
    if (t.err)
        return 2;
    return r ? 0 : 1;
}

/* the current (%+) and previous (%-) job: stopped jobs first, newest first (bash's rule) */
static void job_cur_prev(const sh_shell *sh, int *cur, int *prev)
{
    int i, pass;
    *cur = *prev = -1;
    for (pass = 1; pass >= 0; pass--) {
        for (;;) {
            int best = -1;
            for (i = 0; i < 32; i++)
                if (sh->jobs[i] && sh->job_stopped[i] == pass && i != *cur && i != *prev &&
                    (best < 0 || sh->job_seq[i] > sh->job_seq[best]))
                    best = i;
            if (best < 0)
                break;
            if (*cur < 0)
                *cur = best;
            else if (*prev < 0)
                *prev = best;
            else
                return;
        }
    }
}

/* A job's line in jobs and notices: "[n]+  <state>  <command>"; bash pads the state to 27 columns,
 * -l puts the process id after the marker, an ended job's state is "Done" or "Exit n" */
static void job_line_l(sh_shell *sh, sh_fh fh, int i, const char *state, long st, int longfmt)
{
    char n[16], pad[32];
    int cur, prev, l, k;
    job_cur_prev(sh, &cur, &prev);
    num(n, i + 1);
    sayl(sh, fh, "[", n, "]", i == cur ? "+" : i == prev ? "-" : " ", " ", NULL);
    if (longfmt) {
        num(n, sh->jobs[i]);
        for (k = (int)strlen(n); k < 5; k++) /* bash prints the pid as %5ld */
            say(sh, fh, " ");
        sayl(sh, fh, n, " ", NULL);
    } else
        say(sh, fh, " ");
    sayl(sh, fh, state, NULL);
    l = (int)strlen(state);
    if (st > 0) {
        num(n, st);
        sayl(sh, fh, " ", n, NULL);
        l += 1 + (int)strlen(n);
    }
    for (k = 0; l + k < 26 && k < 30; k++)
        pad[k] = ' ';
    pad[k] = 0;
    sayl(sh, fh, pad, " ", sh->job_text[i] ? sh->job_text[i] : "", !strcmp(state, "Running") ? " &" : "", "\n", NULL);
}

static void job_line(sh_shell *sh, sh_fh fh, int i, const char *state, long st)
{
    job_line_l(sh, fh, i, state, st, 0);
}

static void job_forget(sh_shell *sh, int i);

/* wait collects every job that has ended too: a non-interactive bash drops them from the table silently */
static void job_sweep(sh_shell *sh)
{
    int i;
    for (i = 0; i < 32; i++)
        if (sh->jobs[i] && !sh->job_foreign[i] && sh->os.done && sh->os.done(sh->os.data, sh->jobs[i])) {
            job_wait(sh, sh->jobs[i]);
            job_forget(sh, i);
        }
}

static void job_forget(sh_shell *sh, int i)
{
    sh->jobs[i] = 0;
    sh->job_stopped[i] = 0;
    sh->job_nohup[i] = 0;
    sh->job_foreign[i] = 0;
    free(sh->job_text[i]);
    sh->job_text[i] = 0;
}

/* A job that has ended: its status collected, reported, forgotten. */
static int job_reap(sh_shell *sh, int i, sh_fh fh)
{
    long st;
    if (!sh->jobs[i] || sh->job_foreign[i] || !sh->os.done || !sh->os.done(sh->os.data, sh->jobs[i]))
        return 0;
    st = job_wait(sh, sh->jobs[i]);
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

/* A job argument: %% %+ (current) %- (previous) %n %string (command starts with it) %?string (contains it).
 * bare: a number is also a job number (jobs, fg, bg, disown) when nopid. -1: none, -2: ambiguous */
static int job_spec(sh_shell *sh, const char *a)
{
    int i, cur, prev, hit = -1;
    const char *t = a[0] == '%' ? a + 1 : a;
    job_cur_prev(sh, &cur, &prev);
    if (a[0] == '%' && (!*t || !strcmp(t, "%") || !strcmp(t, "+")))
        return cur;
    if (a[0] == '%' && !strcmp(t, "-"))
        return prev;
    if (*t >= '0' && *t <= '9') {
        i = atoi(t) - 1;
        return i >= 0 && i < 32 && sh->jobs[i] ? i : -1;
    }
    if (a[0] != '%')
        return -1;
    for (i = 0; i < 32; i++) {
        const char *x = sh->job_text[i];
        int ok;
        if (!sh->jobs[i] || !x)
            continue;
        ok = *t == '?' ? strstr(x, t + 1) != 0 : !strncmp(x, t, strlen(t));
        if (ok) {
            if (hit >= 0)
                return -2;
            hit = i;
        }
    }
    return hit;
}

/* the job a jobs/fg/bg/disown argument names, with bash's complaint; -1 when there is none */
static int job_spec_err(sh_shell *sh, const char *cmd, const char *a, const sh_io *io)
{
    int i = job_spec(sh, a[0] == '%' || (a[0] >= '0' && a[0] <= '9') ? a : "%?\001");
    if (i == -2)
        sayl(sh, io->err, "vsh: ", cmd, ": ", a[0] == '%' ? a + 1 : a, ": ambiguous job spec\n", NULL);
    if (i < 0)
        sayl(sh, io->err, "vsh: ", cmd, ": ", a, ": no such job\n", NULL);
    return i;
}

static long b_jobs(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i, a = 1, longfmt = 0, pids = 0, running = 0, stopped = 0, xmode = 0, status = 0;
    for (; a < argc && argv[a][0] == '-' && argv[a][1]; a++) {
        const char *p;
        if (!strcmp(argv[a], "--")) {
            a++;
            break;
        }
        for (p = argv[a] + 1; *p; p++) {
            switch (*p) {
            case 'l': longfmt = 1; break;
            case 'p': pids = 1; break;
            case 'r': running = 1; break;
            case 's': stopped = 1; break;
            case 'n': break;
            case 'x': xmode = 1; break;
            default: {
                char o[3];
                o[0] = '-';
                o[1] = *p;
                o[2] = 0;
                err2(sh, io, "jobs", o);
                sayl(sh, io->err, "jobs: usage: jobs [-lnprs] [jobspec ...] or jobs -x command [args]\n", NULL);
                return 2;
            }
            }
        }
        if (xmode) {
            a++;
            break;
        }
    }
    if (xmode) {
        /* jobs -x command args: each job argument becomes its process id */
        char **nv = (char **)calloc((size_t)(argc - a + 3), sizeof(char *));
        char nb[16];
        int k, n = 0;
        long st;
        if (!nv)
            return 1;
        nv[n++] = (char *)"eval";
        for (k = a; k < argc; k++) {
            int j = argv[k][0] == '%' && k > a ? job_spec(sh, argv[k]) : -1;
            if (j >= 0) {
                num(nb, sh->jobs[j]);
                nv[n++] = sdup(nb);
            } else
                nv[n++] = argv[k];
        }
        st = n > 1 ? b_eval(sh, n, nv, io) : 0;
        for (k = a; k < argc; k++)
            if (nv[k - a + 1] != argv[k])
                free(nv[k - a + 1]);
        free(nv);
        return st;
    }
    for (i = 0; i < 32; i++) {
        int want = a >= argc, k;
        if (!sh->jobs[i])
            continue;
        for (k = a; k < argc; k++)
            if (job_spec(sh, argv[k]) == i)
                want = 1;
        if (!want)
            continue;
        if ((running && sh->job_stopped[i]) || (stopped && !sh->job_stopped[i]))
            continue;
        if (job_reap(sh, i, io->out) && !pids)
            continue;
        if (!sh->jobs[i])
            continue;
        if (pids) {
            char nb[16];
            num(nb, sh->jobs[i]);
            sayl(sh, io->out, nb, "\n", NULL);
        } else
            job_line_l(sh, io->out, i, sh->job_stopped[i] ? "Stopped" : "Running", 0, longfmt);
    }
    for (i = a; i < argc; i++)
        if (job_spec(sh, argv[i]) < 0 && (job_spec_err(sh, "jobs", argv[i], io) < 0))
            status = 1;
    return status;
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
    sh->job_seq[i] = ++sh->job_seqno;
    sh->warned_stopped = 0;
    say(sh, io->err, "\n");
    job_line(sh, io->err, i, "Stopped", 0);
    return SH_STATUS_STOPPED;
}

/* The job a job argument (%n or n) names, or the newest one for which
 * want(sh, i) holds; -1: none. */
static int job_arg(sh_shell *sh, int argc, char **argv, int stopped_only)
{
    int i, cur, prev;
    if (argc > 1) {
        i = job_spec(sh, argv[1][0] == '%' ? argv[1] : "%?");
        if (argv[1][0] != '%' && argv[1][0] >= '0' && argv[1][0] <= '9')
            i = atoi(argv[1]) - 1 >= 0 && atoi(argv[1]) - 1 < 32 && sh->jobs[atoi(argv[1]) - 1] ? atoi(argv[1]) - 1 : -1;
        return i >= 0 && (!stopped_only || sh->job_stopped[i]) ? i : -1;
    }
    job_cur_prev(sh, &cur, &prev);
    if (stopped_only) {
        for (i = 31; i >= 0; i--)
            if (sh->jobs[i] && sh->job_stopped[i])
                return i;
        return -1;
    }
    return cur;
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
    sayl(sh, io->out, "stack ", nb, "\n", NULL);
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
    sayl(sh, io->out, "[", nb, "] ", sh->job_text[i] ? sh->job_text[i] : "", " &\n", NULL);
    return 0;
}

/* fg / wait: wait for a job (fg %n / wait: the newest, or all). fg shows
 * the command it brings back, as the Unix shells do. */
static long b_wait(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    long st = 0;
    int i, fg = !strcmp(argv[0], "fg");
    if (fg) {
        /* the named job, or the newest; a stopped one is continued first,
         * and may be suspended again */
        i = job_arg(sh, argc, argv, 0);
        if (i < 0) {
            err2(sh, io, "fg", "no such job");
            return 1;
        }
        sayl(sh, io->out, sh->job_text[i] ? sh->job_text[i] : "", "\n", NULL);
        if (sh->job_stopped[i]) {
            if (!sh->os.cont || sh->os.cont(sh->os.data, sh->jobs[i])) {
                err2(sh, io, "fg", "cannot continue it");
                return 1;
            }
            sh->job_stopped[i] = 0;
        }
        sh->os.suspendable = sh->os.cont != 0;
        st = job_wait(sh, sh->jobs[i]);
        sh->os.suspendable = 0;
        if (st == SH_STOPPED)
            return fg_status(sh, st, i, 0, io);
        job_forget(sh, i);
        return st;
    }
    {
        /* wait [-n] [-p var] [id ...]: an id is a process id (a number) or a job spec */
        int k = 1, any_n = 0, gone;
        const char *pvar = 0;
        for (; k < argc && argv[k][0] == '-' && argv[k][1]; k++) {
            if (!strcmp(argv[k], "--")) {
                k++;
                break;
            }
            if (!strcmp(argv[k], "-n"))
                any_n = 1;
            else if (!strcmp(argv[k], "-p") && k + 1 < argc)
                pvar = argv[++k];
            else if (!strcmp(argv[k], "-f"))
                ;
            else {
                err2(sh, io, "wait", "invalid option");
                return 2;
            }
        }
        if (any_n) {
            /* the first job (of the ids, or any) that has ended */
            for (;;) {
                int found = -1, live = 0;
                for (i = 0; i < 32 && found < 0; i++) {
                    int want = k >= argc, j;
                    if (!sh->jobs[i] || sh->job_stopped[i] || sh->job_foreign[i])
                        continue;
                    for (j = k; j < argc; j++)
                        if (argv[j][0] == '%' ? job_spec(sh, argv[j]) == i : atol(argv[j]) == sh->jobs[i])
                            want = 1;
                    if (!want)
                        continue;
                    live++;
                    if (sh->os.done && sh->os.done(sh->os.data, sh->jobs[i]))
                        found = i;
                }
                if (!live)
                    return 127;
                if (found < 0) /* none ended yet: wait for the oldest of them */
                    for (i = 0; i < 32 && found < 0; i++)
                        if (sh->jobs[i] && !sh->job_stopped[i])
                            found = i;
                if (pvar) {
                    char nb[16];
                    num(nb, sh->jobs[found]);
                    sh_set(&sh->ctx, pvar, nb);
                }
                st = job_wait(sh, sh->jobs[found]);
                job_forget(sh, found);
                return st;
            }
        }
        if (k < argc) {
            long last = 0;
            for (; k < argc; k++) {
                if (argv[k][0] == '%') {
                    i = job_spec(sh, argv[k]);
                    if (i < 0) {
                        sayl(sh, io->err, "vsh: wait: ", argv[k], i == -2 ? ": ambiguous job spec\n" : ": no such job\n", NULL);
                        last = 127;
                        continue;
                    }
                } else {
                    long pid = atol(argv[k]);
                    for (i = 0; i < 32 && !(sh->jobs[i] && sh->jobs[i] == pid); i++)
                        ;
                    if (i == 32) {
                        sayl(sh, io->err, "vsh: wait: pid ", argv[k], " is not a child of this shell\n", NULL);
                        last = 127;
                        continue;
                    }
                }
                if (pvar) {
                    char nb[16];
                    num(nb, sh->jobs[i]);
                    sh_set(&sh->ctx, pvar, nb);
                }
                last = job_wait(sh, sh->jobs[i]);
                job_forget(sh, i);
            }
            if (!(sh->opts & SO_INTERACTIVE))
                job_sweep(sh);
            return last;
        }
        gone = 0;
        for (i = 31; i >= 0; i--) {
            /* a stopped job is not waited for: it would never end */
            if (!sh->jobs[i] || sh->job_stopped[i] || sh->job_foreign[i])
                continue;
            job_wait(sh, sh->jobs[i]);
            job_forget(sh, i);
            gone++;
        }
        (void)gone;
        return 0;
    }
}

/* disown [-h] [-ar] [job ...]: forget jobs (-h: keep them, marked not to get SIGHUP) */
static long b_disown(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int k = 1, hflag = 0, all = 0, run = 0, i, st = 0;
    for (; k < argc && argv[k][0] == '-' && argv[k][1]; k++) {
        const char *p;
        if (!strcmp(argv[k], "--")) {
            k++;
            break;
        }
        for (p = argv[k] + 1; *p; p++) {
            if (*p == 'h') hflag = 1;
            else if (*p == 'a') all = 1;
            else if (*p == 'r') run = 1;
            else {
                err2(sh, io, "disown", "invalid option");
                return 2;
            }
        }
    }
    if (k >= argc) {
        int cur, prev;
        if (all || run) {
            for (i = 0; i < 32; i++)
                if (sh->jobs[i] && (all || !sh->job_stopped[i])) {
                    if (hflag)
                        sh->job_nohup[i] = 1;
                    else
                        job_forget(sh, i);
                }
            return 0;
        }
        job_cur_prev(sh, &cur, &prev);
        if (cur < 0) {
            err2(sh, io, "disown", "current: no such job");
            return 1;
        }
        if (hflag)
            sh->job_nohup[cur] = 1;
        else
            job_forget(sh, cur);
        return 0;
    }
    for (; k < argc; k++) {
        i = job_spec(sh, argv[k][0] == '%' ? argv[k] : "%?");
        if (argv[k][0] != '%' && argv[k][0] >= '0' && argv[k][0] <= '9')
            for (i = 0; i < 32 && !(sh->jobs[i] && sh->jobs[i] == atol(argv[k])); i++)
                ;
        if (i < 0 || i >= 32) {
            sayl(sh, io->err, "vsh: disown: ", argv[k], ": no such job\n", NULL);
            st = 1;
            continue;
        }
        if (hflag)
            sh->job_nohup[i] = 1;
        else
            job_forget(sh, i);
    }
    return st;
}

static sh_func *find_func(sh_shell *sh, const char *name);
static builtin_fn find_builtin(const char *name);
static builtin_fn find_bi(const sh_shell *sh, const char *name);
static int find_command_file(sh_shell *sh, const char *name, char *out, long max);
static void save_var(sh_shell *sh, const char *name, saved_var *s);

/* ---- variables with attributes: declare, local, readonly, export, unset --------- */

static int valid_name(const char *s, size_t n)
{
    size_t i;
    if (!n || (s[0] >= '0' && s[0] <= '9'))
        return 0;
    for (i = 0; i < n; i++)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') || s[i] == '_' || (s[i] >= '0' && s[i] <= '9')))
            return 0;
    return 1;
}

/* NAME=(...) or NAME+=(...): body is the text between the parentheses */
static int compound_assign(sh_shell *sh, const char *name, int append, const char *body, const sh_io *io);

/* the attribute letters of declare, in the order declare -p prints them */
static const struct { char c; unsigned bit; } decl_attrs[] = {
    { 'a', SH_ATTR_ARRAY }, { 'A', SH_ATTR_ASSOC }, { 'i', SH_ATTR_INTEGER },
    { 'l', SH_ATTR_LOWER }, { 'n', SH_ATTR_NAMEREF }, { 'r', SH_ATTR_READONLY },
    { 'u', SH_ATTR_UPPER }, { 'x', SH_ATTR_EXPORT }
};

static void decl_print(sh_shell *sh, const sh_io *io, const sh_var *v, int mode)
{
    char at[12];
    int k = 0, j;
    const char *sv = sh_var_str(v);
    if ((sh->opts & SO_POSIX) && (mode == 2 || mode == 3) && !v->arr) {
        /* posix mode: export -p and readonly -p print the command that recreates them */
        SH_HIT(POSIX_FORMAT);
        sayl(sh, io->out, mode == 2 ? "readonly " : "export ", v->name, NULL);
        if (!(v->attr & SH_ATTR_NOVALUE) && sv) {
            say(sh, io->out, "=");
            say_dq(sh, io, sv);
        }
        say(sh, io->out, "\n");
        return;
    }
    for (j = 0; j < (int)(sizeof decl_attrs / sizeof decl_attrs[0]); j++)
        if (v->attr & decl_attrs[j].bit)
            at[k++] = decl_attrs[j].c;
    if (!k)
        at[k++] = '-';
    at[k] = 0;
    sayl(sh, io->out, "declare -", at, " ", v->name, NULL);
    if (v->arr && !v->arr->n && (v->attr & SH_ATTR_NOVALUE)) {
        say(sh, io->out, "\n");
        return;
    }
    say(sh, io->out, "=");
    if (v->arr)
        say_array(sh, io, v);
    else
        say_dq(sh, io, sv ? sv : "");
    say(sh, io->out, "\n");
}

/* ${x@A} and ${x@a}, see sh_ctx.declared */
static char *core_declared(sh_ctx *c, const char *name, int flags_only, int whole)
{
    const sh_var *v = sh_lookup(c, name);
    pbuf o = { 0, 0, 0 };
    char at[12];
    int k = 0, j;
    const char *sv;
    if (!v)
        return sdup("");
    sv = sh_var_str(v);
    for (j = 0; j < (int)(sizeof decl_attrs / sizeof decl_attrs[0]); j++)
        if ((v->attr & decl_attrs[j].bit) && (flags_only || decl_attrs[j].c != 'n'))
            at[k++] = decl_attrs[j].c;
    at[k] = 0;
    if (flags_only)
        return sdup(at);
    if (!k && !v->arr) {
        pb_str(&o, v->name);
        pb_str(&o, "=");
    } else {
        pb_str(&o, "declare -");
        pb_str(&o, k ? at : "-");
        pb_str(&o, " ");
        pb_str(&o, v->name);
        pb_str(&o, "=");
    }
    if (v->arr && whole)
        pb_array(&o, v);
    else {
        char *q = sh_quote(sv ? sv : "", SH_Q_ALWAYS);
        pb_str(&o, q ? q : "''");
        free(q);
    }
    return o.s ? o.s : sdup("");
}

/* declare, typeset, local, readonly, export: mode 0, 0, 1, 2, 3 */
static int cmp_func(const void *a, const void *b)
{
    return strcmp((*(sh_func *const *)a)->name, (*(sh_func *const *)b)->name);
}

/* declare -f NAME: the function as bash prints it; -F: "declare -f NAME" */
static void func_print(sh_shell *sh, const sh_io *io, const sh_func *f, int names_only)
{
    char *t;
    if (names_only) {
        sayl(sh, io->out, "declare -f ", f->name, "\n", NULL);
        return;
    }
    t = sh_unparse_func(f->name, f->body.tree);
    if (t) {
        sayl(sh, io->out, t, "\n", NULL);
        free(t);
    }
}

static long declare_main(sh_shell *sh, int mode, int argc, char **argv, const sh_io *io)
{
    unsigned set = 0, clear = 0;
    int i = 1, print = 0, global = 0, fn = 0, fnames = 0;
    long st = 0;
    if (mode == 1 && !sh->func_depth) {
        err2(sh, io, "local", "can only be used in a function");
        return 1;
    }
    if (mode == 2)
        set |= SH_ATTR_READONLY;
    if (mode == 3)
        set |= SH_ATTR_EXPORT;
    for (; i < argc && (argv[i][0] == '-' || argv[i][0] == '+') && argv[i][1]; i++) {
        const char *p;
        int on = argv[i][0] == '-';
        if (!strcmp(argv[i], "--")) {
            i++;
            break;
        }
        for (p = argv[i] + 1; *p; p++) {
            unsigned bit = 0;
            int j;
            for (j = 0; j < (int)(sizeof decl_attrs / sizeof decl_attrs[0]); j++)
                if (decl_attrs[j].c == *p)
                    bit = decl_attrs[j].bit;
            if (bit == SH_ATTR_NAMEREF && mode == 3) { /* export -n */
                clear |= SH_ATTR_EXPORT;
                set &= ~SH_ATTR_EXPORT;
                bit = 0;
            } else if (!bit) {
                switch (*p) {
                case 'p': print = 1; break;
                case 'g': global = 1; break;
                case 'F': fnames = 1; fn = 1; break;
                case 'f': fn = 1; break;
                case 't': break;
                default: {
                    char o[3];
                    o[0] = argv[i][0];
                    o[1] = *p;
                    o[2] = 0;
                    err2(sh, io, o, "invalid option");
                    return 2;
                }
                }
            }
            if (bit) {
                if (on) { set |= bit; clear &= ~bit; }
                else { clear |= bit; set &= ~bit; }
            }
        }
    }
    if (fn) {
        sh_func *f, **fl;
        int nf = 0, k;
        if (mode != 0) { /* export -f, readonly -f: found or not, nothing printed */
            for (; i < argc; i++)
                if (!find_func(sh, argv[i]))
                    st = 1;
            return st;
        }
        if (i >= argc) {
            for (f = sh->funcs; f; f = f->next)
                nf++;
            fl = (sh_func **)malloc((size_t)(nf ? nf : 1) * sizeof(sh_func *));
            if (!fl)
                return 1;
            for (k = 0, f = sh->funcs; f; f = f->next)
                fl[k++] = f;
            qsort(fl, (size_t)nf, sizeof(sh_func *), cmp_func);
            for (k = 0; k < nf; k++)
                func_print(sh, io, fl[k], fnames);
            free(fl);
            return 0;
        }
        for (; i < argc; i++) {
            f = find_func(sh, argv[i]);
            if (!f)
                st = 1;
            else if (fnames)
                sayl(sh, io->out, f->name, "\n", NULL);
            else
                func_print(sh, io, f, 0);
        }
        return st;
    }
    if (i >= argc) {
        sh_var **all;
        int n, k;
        unsigned want = set & ~SH_ATTR_READONLY;
        if (mode == 1) { /* local alone: the running function's locals, by name */
            const saved_var *sv = (const saved_var *)sh->locals;
            const char **nm;
            int j, cnt = sh->n_locals - sh->local_mark;
            if (!sh->func_depth) {
                err2(sh, io, "local", "can only be used in a function");
                return 1;
            }
            nm = (const char **)malloc((size_t)(cnt ? cnt : 1) * sizeof(char *));
            if (!nm)
                return 1;
            for (j = 0; j < cnt; j++)
                nm[j] = sv[sh->local_mark + j].name;
            qsort(nm, (size_t)cnt, sizeof(char *), cmp_str);
            for (j = 0; j < cnt; j++) {
                const sh_var *lv;
                if (j && !strcmp(nm[j], nm[j - 1]))
                    continue;
                for (lv = sh->ctx.vars; lv && strcmp(lv->name, nm[j]); lv = lv->next)
                    ;
                if (lv)
                    decl_print(sh, io, lv, mode);
                else {
                    sayl(sh, io->out, "declare -- ", nm[j], "\n", NULL);
                }
            }
            free(nm);
            return 0;
        }
        if (mode == 0 && !print && !set && !clear)
            return b_set(sh, 1, argv, io);
        if (mode == 2)
            want = SH_ATTR_READONLY;
        else if (mode == 3)
            want = SH_ATTR_EXPORT;
        else
            want = set;
        all = sorted_vars(sh, &n);
        if (!all)
            return 1;
        for (k = 0; k < n; k++)
            if (!want || (all[k]->attr & want) == want)
                decl_print(sh, io, all[k], mode);
        free(all);
        return 0;
    }
    for (; i < argc; i++) {
        int comp = argv[i][0] == '\1';
        const char *arg = argv[i] + comp, *eq = strchr(arg, '=');
        size_t n = eq ? (size_t)(eq - arg) : strlen(arg);
        int app = n && arg[n - 1] == '+';
        char name[128];
        int local = mode == 1 || (mode == 0 && sh->func_depth && !global);
        if (app)
            n--;
        if (!valid_name(arg, n) || n >= sizeof(name)) {
            err2(sh, io, arg, "not a valid identifier");
            st = 1;
            continue;
        }
        memcpy(name, arg, n);
        name[n] = 0;
        if (print && mode == 0) {
            const sh_var *v = sh_lookup_raw(&sh->ctx, name);
            if (v)
                decl_print(sh, io, v, mode);
            else {
                err2(sh, io, name, "not found");
                st = 1;
            }
            continue;
        }
        if (local) {
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
            if (!eq)
                sh_unset(&sh->ctx, name);
        }
        if (eq && (sh_attr(&sh->ctx, name) & SH_ATTR_READONLY)) {
            err2(sh, io, name, "readonly variable");
            st = 1;
            continue;
        }
        if ((set & SH_ATTR_ASSOC) && (sh_attr(&sh->ctx, name) & SH_ATTR_ARRAY)) {
            err2(sh, io, name, "cannot convert indexed to associative array");
            st = 1;
            continue;
        }
        if (set || clear || (!eq && mode != 1))
            sh_attr_change(&sh->ctx, name, set & ~SH_ATTR_READONLY, clear);
        if (eq && (set & SH_ATTR_NAMEREF)) {
            sh_attr_change(&sh->ctx, name, 0, SH_ATTR_NAMEREF);
            sh_set(&sh->ctx, name, eq + 1);
            sh_attr_change(&sh->ctx, name, SH_ATTR_NAMEREF, 0);
        } else if (eq && (comp || ((set & (SH_ATTR_ARRAY | SH_ATTR_ASSOC)) && eq[1] == '(' && arg[strlen(arg) - 1] == ')'))) {
            char *body = sdup(eq + 2);
            if (body) {
                body[strlen(body) - 1] = 0;
                if (compound_assign(sh, name, app, body, io)) {
                    err2(sh, io, name, "readonly variable");
                    st = 1;
                }
                free(body);
            }
        } else if (eq && sh_assign(&sh->ctx, name, 0, eq + 1, app)) {
            err2(sh, io, name, "readonly variable");
            st = 1;
        }
        if (set & SH_ATTR_READONLY)
            sh_attr_change(&sh->ctx, name, SH_ATTR_READONLY, 0);
    }
    return st;
}

static long b_declare(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    return declare_main(sh, 0, argc, argv, io);
}

static long b_local(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    return declare_main(sh, 1, argc, argv, io);
}

/* mapfile / readarray [-t] [-n count] [-s skip] [-O origin] [-d delim] [-u fd] [array] */
static long b_mapfile(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int trim = 0, delim = '\n', a = 1, have_o = 0;
    long maxn = 0, skip = 0, org = 0, count = 0, put = 0;
    sh_fh in = io->in;
    const char *name = "MAPFILE";
    char buf[RD_MAX];
    for (; a < argc && argv[a][0] == '-' && argv[a][1]; a++) {
        const char *o = argv[a] + 1, *arg;
        if (!strcmp(argv[a], "--")) {
            a++;
            break;
        }
        for (; *o; o++) {
            arg = 0;
            if (strchr("dnsOu", *o)) {
                if (o[1])
                    arg = o + 1;
                else if (a + 1 < argc)
                    arg = argv[++a];
                else {
                    err2(sh, io, argv[a], "option requires an argument");
                    return 2;
                }
            }
            switch (*o) {
            case 't': trim = 1; break;
            case 'd': delim = arg[0] ? (unsigned char)arg[0] : 0; break;
            case 'n': maxn = atol(arg); break;
            case 's': skip = atol(arg); break;
            case 'O': org = atol(arg); have_o = 1; break;
            case 'u': {
                long fd = fd_number(arg);
                in = fd >= 0 ? fd_get(sh, io, fd) : SH_NOFH;
                if (!in) {
                    err2(sh, io, arg, "invalid file descriptor");
                    return 1;
                }
                break;
            }
            default:
                err2(sh, io, argv[a], "invalid option");
                return 2;
            }
            if (arg)
                break;
        }
    }
    if (a < argc)
        name = argv[a];
    if (!valid_name(name, strlen(name))) {
        err2(sh, io, name, "not a valid identifier");
        return 1;
    }
    if (!have_o && sh_array_reset(&sh->ctx, name, 0)) {
        err2(sh, io, name, "readonly variable");
        return 1;
    }
    put = org;
    for (;;) {
        pbuf rec = { 0, 0, 0 };
        int end = 0, any = 0;
        char d[24];
        for (;;) {
            long m;
            if (delim == '\n') {
                m = sh->os.read_line(sh->os.data, in, buf, sizeof(buf));
                if (m < 0) {
                    end = 1;
                    break;
                }
                any = 1;
                pb_add(&rec, buf, m);
                if (m > 0 && buf[m - 1] == '\n')
                    break;
                if (m < (long)sizeof(buf) - 1) {
                    end = 1;
                    break;
                }
            } else {
                char c;
                if (sh->os.read(sh->os.data, in, &c, 1) <= 0) {
                    end = 1;
                    break;
                }
                any = 1;
                pb_add(&rec, &c, 1);
                if ((unsigned char)c == (unsigned char)delim)
                    break;
            }
        }
        if (!any || (end && (!rec.s || !rec.n))) {
            free(rec.s);
            break;
        }
        count++;
        if (count > skip) {
            if (trim && rec.n && (unsigned char)rec.s[rec.n - 1] == (unsigned char)delim)
                rec.n--;
            pb_add(&rec, "", 1); /* the NUL */
            sh_ltoa(put++, d);
            sh_assign(&sh->ctx, name, d, rec.s, 0);
            if (maxn && put - org >= maxn)
                end = 1;
        }
        free(rec.s);
        if (end)
            break;
    }
    return 0;
}

static long b_readonly(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    return declare_main(sh, 2, argc, argv, io);
}

static long b_export(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    return declare_main(sh, 3, argc, argv, io);
}

static long b_unset(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i = 1, fn = 0, nameref = 0;
    long st = 0;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) {
            i++;
            break;
        }
        if (!strcmp(argv[i], "-f"))
            fn = 1;
        else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "-n"))
            fn = 0, nameref = argv[i][1] == 'n';
        else {
            err2(sh, io, argv[i], "invalid option");
            return 2;
        }
    }
    for (; i < argc; i++) {
        if (fn) {
            sh_func **p;
            for (p = &sh->funcs; *p; p = &(*p)->next)
                if (!strcmp((*p)->name, argv[i]) && !(*p)->busy) {
                    sh_func *f = *p;
                    *p = f->next;
                    free(f->name);
                    free(f->src);
                    sh_parse_free(&f->body);
                    free(f);
                    break;
                }
        } else if (nameref) {
            sh_var_restore(&sh->ctx, argv[i], 0);
        } else {
            char *br = strchr(argv[i], '['), *nm = 0, *sub;
            int bad;
            if (br && argv[i][strlen(argv[i]) - 1] == ']' && valid_name(argv[i], (size_t)(br - argv[i])))
                nm = sdup(argv[i]);
            if (nm) {
                nm[br - argv[i]] = 0;
                nm[strlen(argv[i]) - 1] = 0;
                sub = nm + (br - argv[i]) + 1;
                if (!strcmp(sub, "@") || !strcmp(sub, "*"))
                    bad = sh_unset(&sh->ctx, nm);
                else {
                    char *k = expand_one(sh, sub, io);
                    bad = k ? sh_unset_elem(&sh->ctx, nm, k) : 0;
                    free(k);
                }
                free(nm);
            } else {
                bad = sh_unset(&sh->ctx, argv[i]);
                if (!bad && !strcmp(argv[i], "DIRSTACK"))
                    sh->dirstack_gone = 1;
                if (!bad && !strcmp(argv[i], "POSIXLY_CORRECT")) {
                    sh->opts &= ~SO_POSIX;  /* unsetting it leaves posix mode */
                    SH_HIT(POSIX_VAR);
                    opts_apply(sh);
                }
            }
            if (bad) {
                err2(sh, io, argv[i], "cannot unset: readonly variable");
                st = 1;
            }
        }
    }
    return st;
}

/* ---- type, command -v, builtin ----------------------------------------------- */

static const char *const sh_keywords[] = { "!", "[[", "]]", "{", "}", "case", "do", "done", "elif", "else", "esac",
                                           "fi", "for", "function", "if", "in", "select", "then", "time", "until",
                                           "while", 0 };

static const char *alias_value(sh_shell *sh, const char *name)
{
    int i;
    size_t n = strlen(name);
    for (i = 0; i < sh->aliases.n; i++)
        if (!strncmp(sh->aliases.v[i], name, n) && sh->aliases.v[i][n] == '=')
            return sh->aliases.v[i] + n + 1;
    return 0;
}

/* one name for type (mode 0), type -t (1), command -v (2), command -V (3). 0 = found */
static int type_one(sh_shell *sh, const sh_io *io, const char *name, int mode, int all, int pathonly)
{
    int found = 0, k;
    char path[512];
    const char *av = shopt_get(sh, "expand_aliases") ? alias_value(sh, name) : 0;  /* bash: off in a script */
    int kw = 0;
    for (k = 0; sh_keywords[k]; k++)
        if (!strcmp(sh_keywords[k], name))
            kw = 1;
#define SAY_KIND(kind, text_long, text_cmdv) \
    do { \
        found = 1; \
        if (mode == 1) { say(sh, io->out, kind); say(sh, io->out, "\n"); } \
        else if (mode == 2) { say(sh, io->out, text_cmdv); say(sh, io->out, "\n"); } \
        else { say(sh, io->out, name); say(sh, io->out, text_long); say(sh, io->out, "\n"); } \
    } while (0)
    if (!pathonly) {
        if (av) {
            pbuf b = { 0, 0, 0 };
            if (mode == 2) {
                pb_str(&b, "alias ");
                pb_str(&b, name);
                pb_str(&b, "='");
                pb_str(&b, av);
                pb_str(&b, "'");
            } else {
                pb_str(&b, " is aliased to `");
                pb_str(&b, av);
                pb_str(&b, "'");
            }
            SAY_KIND("alias", b.s, b.s);
            free(b.s);
            if (!all)
                return 0;
        }
        if (kw) {
            SAY_KIND("keyword", " is a shell keyword", name);
            if (!all)
                return 0;
        }
        if (find_func(sh, name)) {
            SAY_KIND("function", " is a function", name);
            if (mode == 0) {
                char *ft = sh_unparse_func(name, find_func(sh, name)->body.tree);
                if (ft) {
                    sayl(sh, io->out, ft, "\n", NULL);
                    free(ft);
                }
            }
            if (!all)
                return 0;
        }
        if (find_bi(sh, name)) {
            if ((sh->opts & SO_POSIX) && special_bi(name))
                SH_HIT(POSIX_FORMAT);
            SAY_KIND("builtin", (sh->opts & SO_POSIX) && special_bi(name) ? " is a special shell builtin" : " is a shell builtin", name);
            if (!all)
                return 0;
        }
    }
    if (find_command_file(sh, name, path, sizeof(path))) {
        pbuf b = { 0, 0, 0 };
        pb_str(&b, " is ");
        pb_str(&b, path);
        if (mode == 0 && pathonly) {
            sayl(sh, io->out, path, "\n", NULL);
        } else {
            SAY_KIND("file", b.s, path);
        }
        found = 1;
        free(b.s);
    }
    if (!found && (mode == 0 || mode == 3))
        err2(sh, io, name, "not found");
    return found ? 0 : 1;
}

static long b_type(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i = 1, mode = 0, all = 0, pathonly = 0;
    long st = 0;
    if (!strcmp(argv[0], "which")) {
        for (; i < argc; i++) {
            char path[512];
            if (find_func(sh, argv[i]) || find_bi(sh, argv[i])) {
                sayl(sh, io->out, argv[i], find_func(sh, argv[i]) ? " is a function\n" : " is a shell builtin\n", NULL);
            } else if (find_command_file(sh, argv[i], path, sizeof(path))) {
                sayl(sh, io->out, path, "\n", NULL);
            } else {
                st = 1;
            }
        }
        return st;
    }
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        const char *p;
        if (!strcmp(argv[i], "--")) {
            i++;
            break;
        }
        for (p = argv[i] + 1; *p; p++) {
            if (*p == 't') mode = 1;
            else if (*p == 'a') all = 1;
            else if (*p == 'p' || *p == 'P') pathonly = 1;
        }
    }
    for (; i < argc; i++)
        if (type_one(sh, io, argv[i], mode, all, pathonly))
            st = 1;
    return st;
}

static long b_builtin(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    builtin_fn b;
    if (argc < 2)
        return 0;
    b = find_bi(sh, argv[1]);
    if (!b) {
        err2(sh, io, argv[1], "not a shell builtin");
        return 1;
    }
    return b(sh, argc - 1, argv + 1, io);
}

/* ---- shopt ---------------------------------------------------------------------- */

/* bash 5.3's shopt names, in its order; def is the value a shell starts with (expand_aliases: on only in an
 * interactive shell). Those with a flag of their own are found in shopt_flag; the rest only hold their value
 * (shopt_v) so that a script that sets them runs: they change nothing here, README lists them. */
static const struct { const char *name; int def; } shopt_tab[] = {
    { "array_expand_once", 0 }, { "assoc_expand_once", 0 }, { "autocd", 0 }, { "bash_source_fullpath", 0 },
    { "cdable_vars", 0 }, { "cdspell", 0 }, { "checkhash", 0 }, { "checkjobs", 0 }, { "checkwinsize", 1 },
    { "cmdhist", 1 }, { "compat31", 0 }, { "compat32", 0 }, { "compat40", 0 }, { "compat41", 0 },
    { "compat42", 0 }, { "compat43", 0 }, { "compat44", 0 }, { "complete_fullquote", 1 }, { "direxpand", 0 },
    { "dirspell", 0 }, { "dotglob", 0 }, { "execfail", 0 }, { "expand_aliases", 0 }, { "extdebug", 0 },
    { "extglob", 0 }, { "extquote", 1 }, { "failglob", 0 }, { "force_fignore", 1 }, { "globasciiranges", 1 },
    { "globskipdots", 1 }, { "globstar", 0 }, { "gnu_errfmt", 0 }, { "histappend", 0 }, { "histreedit", 0 },
    { "histverify", 0 }, { "hostcomplete", 1 }, { "huponexit", 0 }, { "inherit_errexit", 0 },
    { "interactive_comments", 1 }, { "lastpipe", 0 }, { "lithist", 0 }, { "localvar_inherit", 0 },
    { "localvar_unset", 0 }, { "login_shell", 0 }, { "mailwarn", 0 }, { "no_empty_cmd_completion", 0 },
    { "nocaseglob", 0 }, { "nocasematch", 0 }, { "noexpand_translation", 0 }, { "nullglob", 0 },
    { "patsub_replacement", 1 }, { "progcomp", 1 }, { "progcomp_alias", 0 }, { "promptvars", 1 },
    { "restricted_shell", 0 }, { "shift_verbose", 0 }, { "sourcepath", 1 }, { "varredir_close", 0 },
    { "xpg_echo", 0 }, { 0, 0 }
};

static int shopt_index(const char *name)
{
    int k;
    for (k = 0; shopt_tab[k].name; k++)
        if (!strcmp(shopt_tab[k].name, name))
            return k;
    return -1;
}

/* the options with a flag of their own; the pointer is the flag the code reads */
static int *shopt_flag(sh_shell *sh, const char *name, unsigned long *bit)
{
    *bit = 0;
    if (!strcmp(name, "nullglob")) return &sh->ctx.nullglob;
    if (!strcmp(name, "failglob")) return &sh->ctx.failglob;
    if (!strcmp(name, "dotglob")) return &sh->ctx.dotglob;
    if (!strcmp(name, "globstar")) return &sh->ctx.globstar;
    if (!strcmp(name, "nocaseglob")) return &sh->ctx.nocase;
    if (!strcmp(name, "nocasematch")) return &sh->ctx.nocasematch;
    if (!strcmp(name, "lastpipe")) { *bit = SO_LASTPIPE; return 0; }
    return 0;
}

/* an option's value; the generic ones fall back to their default (aliases: the shell is interactive) */
static int shopt_get(sh_shell *sh, const char *name)
{
    unsigned long bit;
    int *f = shopt_flag(sh, name, &bit), k;
    if (f)
        return *f != 0;
    if (bit)
        return (sh->opts & bit) != 0;
    if (!strcmp(name, "extglob"))
        return sh_get_extglob();
    k = shopt_index(name);
    if (k < 0)
        return 0;
    if (sh->shopt_v[k] >= 0)
        return sh->shopt_v[k];
    return !strcmp(name, "expand_aliases") ? (sh->opts & SO_INTERACTIVE) != 0 : shopt_tab[k].def;
}

static void shopt_put(sh_shell *sh, const char *name, int on)
{
    unsigned long bit;
    int *f = shopt_flag(sh, name, &bit), k;
    SH_HIT(SHOPT_SET);
    if (f)
        *f = on;
    else if (bit) {
        if (on)
            sh->opts |= bit;
        else
            sh->opts &= ~bit;
    } else if (!strcmp(name, "extglob")) {
        sh_set_extglob(on);
    } else if ((k = shopt_index(name)) >= 0)
        sh->shopt_v[k] = (signed char)on;
}

/* shopt [-pqsuo] [name ...]: -s sets, -u unsets, -p prints as commands, -q is silent (status only),
 * -o works on the set -o options */
static long b_shopt(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i = 1, set = 0, unset = 0, print = 0, quiet = 0, oopt = 0, k;
    long st = 0;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        const char *f;
        if (!strcmp(argv[i], "--")) {
            i++;
            break;
        }
        for (f = argv[i] + 1; *f; f++) {
            if (*f == 's') set = 1;
            else if (*f == 'u') unset = 1;
            else if (*f == 'p') print = 1;
            else if (*f == 'q') quiet = 1;
            else if (*f == 'o') oopt = 1;
            else {
                char o[3];
                o[0] = '-'; o[1] = *f; o[2] = 0;
                err2(sh, io, o, "invalid option");
                return 2;
            }
        }
    }
    if (set && unset) {
        err2(sh, io, "shopt", "cannot set and unset shell options simultaneously");
        return 1;
    }
    if (oopt) {
        /* the set -o options: -s and -u go through set itself, no names lists them all */
        char *a[3];
        a[0] = (char *)"set";
        a[2] = 0;
        if (i == argc) {
            a[1] = (char *)(print ? "+o" : "-o");
            return b_set(sh, 2, a, io);
        }
        if (!set && !unset) {
            /* shopt -o [-pq] NAME: the state of a set -o option, as shopt prints it */
            for (k = i; k < argc; k++) {
                int j, on;
                for (j = 0; j < N_SHOPT; j++)
                    if (!strcmp(sh_optab[j].name, argv[k]))
                        break;
                if (j == N_SHOPT) {
                    err2(sh, io, argv[k], "invalid option name");
                    st = 1;
                    continue;
                }
                on = opt_on(sh, j) != 0;
                if (!on)
                    st = 1;
                if (!quiet) {
                    if (print)
                        sayl(sh, io->out, "set ", on ? "-o " : "+o ", argv[k], "\n", NULL);
                    else {
                        char pad[24];
                        size_t l = strlen(argv[k]);
                        memset(pad, ' ', sizeof(pad));
                        pad[l < 20 ? 20 - l : 0] = 0;
                        sayl(sh, io->out, argv[k], pad, "\t", on ? "on" : "off", "\n", NULL);
                    }
                }
            }
            return st;
        }
        for (k = i; k < argc; k++) {
            a[1] = (char *)(set ? "-o" : "+o");
            a[2] = argv[k];
            if (!set && !unset) {
                err2(sh, io, "shopt", "-o: a name needs -s or -u");
                return 2;
            }
            if (b_set(sh, 3, a, io))
                st = 1;
        }
        return st;
    }
    if (i == argc && (set || unset)) {
        for (k = 0; shopt_tab[k].name; k++)
            if (shopt_get(sh, shopt_tab[k].name) == (set != 0) && !quiet)
                sayl(sh, io->out, shopt_tab[k].name, "\t", set ? "on" : "off", "\n", NULL);
        return 0;
    }
    if (i == argc) {
        for (k = 0; shopt_tab[k].name; k++)
            if (!quiet) {
                int on = shopt_get(sh, shopt_tab[k].name);
                if (print)
                    sayl(sh, io->out, "shopt -", on ? "s " : "u ", shopt_tab[k].name, "\n", NULL);
                else {
                    char pad[24];
                    size_t l = strlen(shopt_tab[k].name);
                    memset(pad, ' ', sizeof(pad));
                    pad[l < 20 ? 20 - l : 0] = 0;
                    sayl(sh, io->out, shopt_tab[k].name, pad, "\t", on ? "on" : "off", "\n", NULL);
                }
            }
        return 0;
    }
    for (k = i; k < argc; k++) {
        int j, known = 0;
        for (j = 0; shopt_tab[j].name; j++)
            if (!strcmp(shopt_tab[j].name, argv[k]))
                known = 1;
        if (!known) {
            err2(sh, io, argv[k], "invalid shell option name");
            st = 1;
            continue;
        }
        if (set || unset) {
            shopt_put(sh, argv[k], set);
        } else {
            int on = shopt_get(sh, argv[k]);
            if (!on)
                st = 1;
            if (!quiet) {
                if (print)
                    sayl(sh, io->out, "shopt -", on ? "s " : "u ", argv[k], "\n", NULL);
                else {
                    char pad[24];
                    size_t l = strlen(argv[k]);
                    memset(pad, ' ', sizeof(pad));
                    pad[l < 20 ? 20 - l : 0] = 0;
                    sayl(sh, io->out, argv[k], pad, "\t", on ? "on" : "off", "\n", NULL);
                }
            }
        }
    }
    return st;
}

/* ---- kill and the signal table ------------------------------------------------ */

/* ixemul's numbering, which is BSD's */
static const char *const sh_signames[] = { 0, "HUP", "INT", "QUIT", "ILL", "TRAP", "ABRT", "EMT", "FPE", "KILL", "BUS",
    "SEGV", "SYS", "PIPE", "ALRM", "TERM", "URG", "STOP", "TSTP", "CONT", "CHLD", "TTIN", "TTOU", "IO", "XCPU", "XFSZ",
    "VTALRM", "PROF", "WINCH", "INFO", "USR1", "USR2" };
#define N_SIG SH_NSIG

/* names compare without regard to case, as bash's decode_signal: 0 when the first n characters match */
static int sig_ieq(const char *s, const char *name, size_t n)
{
    for (; n && *name; n--, s++, name++)
        if ((*s | 0x20) != (*name | 0x20))
            return 1;
    return n && *s != *name ? 1 : 0;
}

static int sig_number(const char *s)
{
    int i;
    if (s[0] >= '0' && s[0] <= '9')
        return atoi(s);
    if (!sig_ieq(s, "SIG", 3))
        s += 3;
    for (i = 1; i < N_SIG; i++)
        if (!sig_ieq(s, sh_signames[i], (size_t)-1) && !s[strlen(sh_signames[i])])
            return i;
    return -1;
}

/* kill -l and trap -l: the numbered list, five to a line */
static void print_siglist(sh_shell *sh, const sh_io *io)
{
    int i;
    for (i = 1; i < N_SIG; i++) {
        char d[24];
        sh_ltoa(i, d);
        if (i < 10)
            say(sh, io->out, " ");
        sayl(sh, io->out, d, ") SIG", sh_signames[i], i % 5 == 0 || i == N_SIG - 1 ? "\n" : "\t", NULL);
    }
}

static long b_kill(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i = 1, sig = 15;
    long st = 0;
    if (argc > 1 && (!strcmp(argv[1], "-l") || !strcmp(argv[1], "-L"))) {
        if (argc == 2) {
            print_siglist(sh, io);
            return 0;
        }
        for (i = 2; i < argc; i++) {
            int n = sig_number(argv[i]);
            if (argv[i][0] >= '0' && argv[i][0] <= '9' && n > 128)
                n -= 128;
            if (n < 1 || n >= N_SIG) {
                err2(sh, io, argv[i], "invalid signal specification");
                st = 1;
            } else if (argv[i][0] >= '0' && argv[i][0] <= '9') {
                sayl(sh, io->out, sh_signames[n], "\n", NULL);
            } else {
                char d[24];
                sh_ltoa(n, d);
                sayl(sh, io->out, d, "\n", NULL);
            }
        }
        return st;
    }
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        if (!strcmp(argv[i], "--")) {
            i++;
            break;
        }
        if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "-n")) {
            if (i + 1 >= argc) {
                err2(sh, io, argv[i], "option requires an argument");
                return 2;
            }
            sig = sig_number(argv[++i]);
        } else {
            sig = sig_number(argv[i] + 1);
        }
        if (sig < 0 || sig >= N_SIG) {
            err2(sh, io, argv[i], "invalid signal specification");
            return 1;
        }
    }
    if (i >= argc) {
        err2(sh, io, "kill", "usage: kill [-s sigspec | -n signum | -sigspec] pid | jobspec ... or kill -l [sigspec]");
        return 2;
    }
    for (; i < argc; i++) {
        long target = atol(argv[i]);
        int job = argv[i][0] == '%';
        if (job)
            target = atol(argv[i] + 1) > 0 && atol(argv[i] + 1) <= 32 ? sh->jobs[atol(argv[i] + 1) - 1] : 0;
        if (!job && target == sh->ctx.pid && sig > 0 && sh_trap_signal(sh, sig))
            continue;
        if (!target || !sh->os.signal || sh->os.signal(sh->os.data, target, sig, job)) {
            err2(sh, io, argv[i], job ? "no such job" : "No such process");
            st = 1;
        }
    }
    return st;
}

static long b_source(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_type(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_export(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_unset(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_readonly(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_declare(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_builtin(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_kill(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_eval(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_exec(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_trap(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_shopt(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_local(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_getopts(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_umask(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_let(sh_shell *sh, int argc, char **argv, const sh_io *io);

/* ---- pushd, popd, dirs ----------------------------------------------------------- */

/* the directory list as dirs prints it: [0] is the current directory, then the stack, top first */
static void dirs_print(sh_shell *sh, const sh_io *io, int full, int perline, int verbose)
{
    char *cwd = sh->os.cwd(sh->os.data);
    const char *home = sh_get(&sh->ctx, "HOME");
    int i;
    for (i = 0; i <= sh->ndirstk; i++) {
        const char *d = i ? sh->dirstk[i - 1] : cwd;
        pbuf b = { 0, 0, 0 };
        char nb[16];
        if (!full && home && *home && !strncmp(d, home, strlen(home)) && (!d[strlen(home)] || d[strlen(home)] == '/')) {
            pb_str(&b, "~");
            pb_str(&b, d + strlen(home));
        } else
            pb_str(&b, d);
        if (verbose) {
            num(nb, i);
            sayl(sh, io->out, i < 10 ? " " : "", nb, "  ", b.s ? b.s : "", "\n", NULL);
        } else
            sayl(sh, io->out, i && !perline ? " " : "", b.s ? b.s : "", perline ? "\n" : (i == sh->ndirstk ? "\n" : ""), NULL);
        free(b.s);
    }
    free(cwd);
}

static int dirs_arg(sh_shell *sh, const sh_io *io, const char *cmd, const char *a, int *n)
{
    int k = atoi(a + 1);
    if ((a[0] != '+' && a[0] != '-') || a[1] < '0' || a[1] > '9') {
        err2(sh, io, cmd, "invalid argument");
        return 1;
    }
    if (a[0] == '-')
        k = sh->ndirstk - k;
    if (k < 0 || k > sh->ndirstk) {
        sayl(sh, io->err, "vsh: ", cmd, ": ", a, ": directory stack index out of range\n", NULL);
        return 1;
    }
    *n = k;
    return 0;
}

static long b_dirs(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i, full = 0, perline = 0, verbose = 0;
    for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        const char *p;
        for (p = argv[i] + 1; *p; p++) {
            if (*p == 'c') {
                for (; sh->ndirstk > 0; sh->ndirstk--)
                    free(sh->dirstk[sh->ndirstk - 1]);
                return 0;
            } else if (*p == 'l') full = 1;
            else if (*p == 'p') perline = 1;
            else if (*p == 'v') verbose = 1;
            else {
                err2(sh, io, "dirs", "invalid option");
                return 2;
            }
        }
    }
    dirs_print(sh, io, full, perline, verbose);
    return 0;
}

static long b_pushd(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i, nocd = 0, n;
    const char *arg = 0;
    char *cwd;
    char *cdargv[3];
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-n")) nocd = 1;
        else arg = argv[i];
    }
    cwd = sh->os.cwd(sh->os.data);
    if (arg && (arg[0] == '+' || arg[0] == '-') && arg[1] >= '0' && arg[1] <= '9') {
        /* rotate: element n becomes the current directory */
        char **all;
        int tot = sh->ndirstk + 1, k;
        if (dirs_arg(sh, io, "pushd", arg, &n)) {
            free(cwd);
            return 1;
        }
        all = (char **)malloc((size_t)tot * sizeof(char *));
        if (!all) {
            free(cwd);
            return 1;
        }
        all[0] = cwd;
        for (k = 1; k < tot; k++)
            all[k] = sh->dirstk[k - 1];
        cdargv[0] = (char *)"cd";
        cdargv[1] = all[n];
        cdargv[2] = 0;
        if (n && !nocd && b_cd(sh, 2, cdargv, io)) {
            free(all);
            free(cwd);
            return 1;
        }
        {
            char **rot = (char **)malloc((size_t)tot * sizeof(char *));
            if (!rot) {
                free(all);
                free(cwd);
                return 1;
            }
            for (k = 0; k < tot; k++)
                rot[k] = all[(k + n) % tot];
            /* rot[0] is the new current directory: its text goes, the others become the stack */
            free(rot[0]);
            for (k = 1; k < tot; k++)
                sh->dirstk[k - 1] = rot[k];
            free(rot);
        }
        free(all);
        dirs_print(sh, io, 0, 0, 0);
        return 0;
    }
    if (!arg) {
        if (!sh->ndirstk) {
            err2(sh, io, "pushd", "no other directory");
            free(cwd);
            return 1;
        }
        cdargv[0] = (char *)"cd";
        cdargv[1] = sh->dirstk[0];
        cdargv[2] = 0;
        if (!nocd && b_cd(sh, 2, cdargv, io)) {
            free(cwd);
            return 1;
        }
        free(sh->dirstk[0]);
        sh->dirstk[0] = cwd;
        dirs_print(sh, io, 0, 0, 0);
        return 0;
    }
    cdargv[0] = (char *)"cd";
    cdargv[1] = (char *)arg;
    cdargv[2] = 0;
    if (!nocd && b_cd(sh, 2, cdargv, io)) {
        free(cwd);
        return 1;
    }
    {
        char **ns = (char **)realloc(sh->dirstk, (size_t)(sh->ndirstk + 1) * sizeof(char *));
        if (!ns) {
            free(cwd);
            return 1;
        }
        sh->dirstk = ns;
        memmove(ns + 1, ns, (size_t)sh->ndirstk * sizeof(char *));
        if (nocd) { /* pushd -n: the directory joins the stack, the current one stays */
            ns[0] = sdup(arg);
            free(cwd);
        } else
            ns[0] = cwd;
        sh->ndirstk++;
    }
    dirs_print(sh, io, 0, 0, 0);
    return 0;
}

static long b_popd(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int i, nocd = 0, n = 0, have = 0;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-n")) nocd = 1;
        else {
            if (dirs_arg(sh, io, "popd", argv[i], &n))
                return 1;
            have = 1;
        }
    }
    if (!sh->ndirstk) {
        err2(sh, io, "popd", "directory stack empty");
        return 1;
    }
    if (n == 0 && !nocd) {
        char *cdargv[3];
        cdargv[0] = (char *)"cd";
        cdargv[1] = sh->dirstk[0];
        cdargv[2] = 0;
        if (b_cd(sh, 2, cdargv, io))
            return 1;
        n = 1;
    } else if (n == 0)
        n = 1;
    (void)have;
    free(sh->dirstk[n - 1]);
    memmove(sh->dirstk + n - 1, sh->dirstk + n, (size_t)(sh->ndirstk - n) * sizeof(char *));
    sh->ndirstk--;
    dirs_print(sh, io, 0, 0, 0);
    return 0;
}

static long b_hash(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_history(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_enable(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_times(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_caller(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_ulimit(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_help(sh_shell *sh, int argc, char **argv, const sh_io *io);
static long b_logout(sh_shell *sh, int argc, char **argv, const sh_io *io);
static builtin_fn find_bi(const sh_shell *sh, const char *name);
static void time_part(pbuf *o, long us, int prec, int lng);
static int hash_note_run(sh_shell *sh, const char *name, char *path, long max);
static int hash_find(const sh_shell *sh, const char *name);
static void hash_sync(sh_shell *sh);

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
    { "wait", b_wait }, { "disown", b_disown }, { "pushd", b_pushd }, { "popd", b_popd }, { "dirs", b_dirs }, { "fg", b_wait }, { "bg", b_bg }, { "stack", b_stack }, { "source", b_source }, { ".", b_source },
    { "eval", b_eval }, { "exec", b_exec }, { "trap", b_trap }, { "shopt", b_shopt }, { "local", b_local },
    { "getopts", b_getopts }, { "umask", b_umask }, { "let", b_let },
    { "which", b_type }, { "type", b_type }, { "readonly", b_readonly }, { "declare", b_declare },
    { "typeset", b_declare }, { "mapfile", b_mapfile }, { "readarray", b_mapfile }, { "builtin", b_builtin }, { "kill", b_kill },
    { "hash", b_hash }, { "history", b_history }, { "enable", b_enable }, { "times", b_times }, { "caller", b_caller }, { "ulimit", b_ulimit }, { "help", b_help }, { "logout", b_logout }, { 0, 0 }
};

/* ---- hash, enable, times, caller, ulimit, help ---------------------------------- */

/* The hash table of commands found on PATH: sh->hashtab holds "name\tpath\thits" in insertion order. bash lists
 * it by bucket of a 256-bucket FNV-1 table, newest first within a bucket: hash_bucket gives the same order. */
static unsigned hash_bucket(const char *s)
{
    unsigned long v = 2166136261UL;
    for (; *s; s++) {
        v = (v + (v << 1) + (v << 4) + (v << 7) + (v << 8) + (v << 24)) & 0xffffffffUL;
        v ^= (unsigned char)*s;
    }
    return (unsigned)(v & 255);
}

static int hash_find(const sh_shell *sh, const char *name)
{
    int i;
    size_t n = strlen(name);
    for (i = 0; i < sh->hashtab.n; i++)
        if (!strncmp(sh->hashtab.v[i], name, n) && sh->hashtab.v[i][n] == '\t')
            return i;
    return -1;
}

static const char *hash_path(const sh_shell *sh, int i, char *buf, long max)
{
    const char *p = strchr(sh->hashtab.v[i], '\t') + 1;
    const char *e = strchr(p, '\t');
    long n = (long)(e - p);
    if (n >= max)
        n = max - 1;
    memcpy(buf, p, (size_t)n);
    buf[n] = 0;
    return buf;
}

static long hash_hits(const sh_shell *sh, int i)
{
    return atol(strrchr(sh->hashtab.v[i], '\t') + 1);
}

static void hash_put(sh_shell *sh, const char *name, const char *path, long hits)
{
    pbuf b = { 0, 0, 0 };
    char d[24];
    int i = hash_find(sh, name);
    pb_str(&b, name);
    pb_add(&b, "\t", 1);
    pb_str(&b, path);
    pb_add(&b, "\t", 1);
    num(d, hits);
    pb_str(&b, d);
    if (!b.s)
        return;
    if (i >= 0) {
        free(sh->hashtab.v[i]);
        sh->hashtab.v[i] = b.s;
    } else {
        sh_list_add(&sh->hashtab, b.s);
        free(b.s);
    }
}

static void hash_del(sh_shell *sh, int i)
{
    free(sh->hashtab.v[i]);
    memmove(sh->hashtab.v + i, sh->hashtab.v + i + 1, (size_t)(sh->hashtab.n - i - 1) * sizeof(char *));
    sh->hashtab.n--;
}

/* the PATH directories only (bash does not look in the current directory for a command name) */
static int hash_search(sh_shell *sh, const char *name, char *out, long max)
{
    const char *p = sh_get(&sh->ctx, "PATH");
    char dir[256];
    if (!sh->os.stat || strchr(name, '/') || strchr(name, ':'))
        return 0;
    while (p && sh_path_next(&p, dir, sizeof(dir))) {
        long n = (long)strlen(dir);
        if (!n || n + (long)strlen(name) + 2 > max)
            continue;
        strcpy(out, dir);
        if (dir[n - 1] != ':' && dir[n - 1] != '/')
            strcat(out, "/");
        strcat(out, name);
        if (sh_exists(sh, out, 0))
            return 1;
    }
    return 0;
}

/* assigning PATH empties the table: the value it was built for is kept, a different one flushes it */
static void hash_sync(sh_shell *sh)
{
    const char *cur = sh_get(&sh->ctx, "PATH");
    if (!cur)
        cur = "";
    if (!sh->hashpath || strcmp(sh->hashpath, cur)) {
        sh_list_free(&sh->hashtab);
        free(sh->hashpath);
        sh->hashpath = sdup(cur);
    }
}

/* an external command is about to run: bash remembers where it found it and counts the runs */
static int hash_note_run(sh_shell *sh, const char *name, char *path, long max)
{
    int i;
    hash_sync(sh);
    if (!(sh->opts & SO_HASHALL) || strchr(name, '/') || strchr(name, ':'))
        return 0;
    i = hash_find(sh, name);
    if (i >= 0) {
        SH_HIT(HASH_RUN);
        hash_path(sh, i, path, max);
        hash_put(sh, name, path, hash_hits(sh, i) + 1);
        return 1;
    } else if (hash_search(sh, name, path, max))
        hash_put(sh, name, path, 1);
    return 0;
}

static void hash_usage(sh_shell *sh, const sh_io *io)
{
    say(sh, io->err, "hash: usage: hash [-lr] [-p pathname] [-dt] [name ...]\n");
}

/* ---- history ----------------------------------------------------------------------
 * The list has one owner. On a console that keeps it (vtcon: the line editor's list, loaded from and appended
 * to ENVARC:vtcon.history) the shell asks through os.hist and holds nothing; on any other console (a ROM CON:
 * window, a script, the host) it keeps sh->hist itself, and asks the console only the once. */

static int hist_own(sh_shell *sh)
{
    if (sh->hist_native == 0) {
        long n = sh->os.hist ? sh->os.hist(sh->os.data, SH_HIST_COUNT, 0, 0, 0) : -1;
        sh->hist_native = n >= 0 ? 1 : -1;
    }
    return sh->hist_native < 0;
}

static long hist_count(sh_shell *sh)
{
    return hist_own(sh) ? sh->hist.n : sh->os.hist(sh->os.data, SH_HIST_COUNT, 0, 0, 0);
}

/* line i (0 = oldest), malloc'ed; 0 when there is none */
static char *hist_get(sh_shell *sh, long i)
{
    char buf[1024];
    if (i < 0 || i >= hist_count(sh))
        return 0;
    if (hist_own(sh))
        return sdup(sh->hist.v[i]);
    if (sh->os.hist(sh->os.data, SH_HIST_GET, i, buf, sizeof(buf)) < 0)
        return 0;
    return sdup(buf);
}

static long hist_size_limit(sh_shell *sh, const char *name, long dflt)
{
    const char *v = sh_get(&sh->ctx, name);
    char *e;
    long n;
    if (!v || !*v)
        return dflt;
    n = strtol(v, &e, 10);
    return *e ? dflt : n;   /* bash: not a number means no limit; negative means unlimited too */
}

static void hist_add(sh_shell *sh, const char *line)
{
    long max;
    if (!hist_own(sh)) {
        sh->os.hist(sh->os.data, SH_HIST_ADD, 0, (char *)line, 0);
        return;
    }
    sh_list_add(&sh->hist, line);
    max = hist_size_limit(sh, "HISTSIZE", 500);
    while (max >= 0 && sh->hist.n > max && sh->hist.n) {
        free(sh->hist.v[0]);
        memmove(sh->hist.v, sh->hist.v + 1, (sh->hist.n - 1) * sizeof(char *));
        sh->hist.n--;
        sh->hist_base++;
        if (sh->hist_saved > 0)
            sh->hist_saved--;
    }
}

static int hist_del(sh_shell *sh, long i)
{
    if (i < 0 || i >= hist_count(sh))
        return -1;
    if (!hist_own(sh))
        return sh->os.hist(sh->os.data, SH_HIST_DEL, i, 0, 0) ? 0 : -1;
    free(sh->hist.v[i]);
    memmove(sh->hist.v + i, sh->hist.v + i + 1, (sh->hist.n - i - 1) * sizeof(char *));
    sh->hist.n--;
    if (sh->hist_saved > i)
        sh->hist_saved--;
    return 0;
}

static void hist_clear(sh_shell *sh)
{
    if (!hist_own(sh)) {
        sh->os.hist(sh->os.data, SH_HIST_CLEAR, 0, 0, 0);
        return;
    }
    sh->hist_base = 0;
    sh_list_free(&sh->hist);
    sh->hist_saved = 0;
}

void sh_hist_note(sh_shell *sh, const char *text)
{
    char *t;
    size_t n;
    const char *ctl;
    long last;
    if (!(sh->opts & SO_INTERACTIVE) || !hist_own(sh))
        return;   /* a console with a list has the line already */
    while (*text == '\n')
        text++;
    n = strlen(text);
    while (n && (text[n - 1] == '\n' || text[n - 1] == '\r'))
        n--;
    if (!n)
        return;
    t = (char *)malloc(n + 1);
    if (!t)
        return;
    memcpy(t, text, n);
    t[n] = 0;
    ctl = sh_get(&sh->ctx, "HISTCONTROL");
    last = sh->hist.n;
    if (ctl && (strstr(ctl, "ignorespace") || strstr(ctl, "ignoreboth")) && t[0] == ' ') {
        free(t);
        return;
    }
    if (ctl && (strstr(ctl, "ignoredups") || strstr(ctl, "ignoreboth")) && last && !strcmp(sh->hist.v[last - 1], t)) {
        free(t);
        return;
    }
    if (ctl && strstr(ctl, "erasedups")) {
        long k;
        for (k = sh->hist.n - 1; k >= 0; k--)
            if (!strcmp(sh->hist.v[k], t))
                hist_del(sh, k);
    }
    hist_add(sh, t);
    free(t);
}

static const char *hist_file(sh_shell *sh, int argc, char **argv, int a)
{
    const char *f = a < argc ? argv[a] : sh_get(&sh->ctx, "HISTFILE");
    return f && *f ? f : 0;
}

/* history -r: the file's lines appended to the list; from: skip the first lines (history -n) */
static int hist_read_file(sh_shell *sh, const char *file, long from, long *nread)
{
    sh_fh fh = sh->os.open(sh->os.data, file, SH_OPEN_READ);
    char line[1024];
    long n, k = 0;
    if (!fh)
        return -1;
    while ((n = sh->os.read_line(sh->os.data, fh, line, sizeof(line) - 1)) >= 0) {
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            n--;
        line[n] = 0;
        if (k++ < from || !n)
            continue;
        hist_add(sh, line);
    }
    sh->os.close(sh->os.data, fh);
    if (nread)
        *nread = k;
    return 0;
}

static long b_history(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int a = 1, c = 0, d = 0, w = 0, r = 0, ap = 0, nn = 0, p = 0, s = 0, i;
    const char *dval = 0;
    long total, st = 0;
    for (; a < argc && argv[a][0] == '-' && argv[a][1]; a++) {
        const char *q = argv[a] + 1;
        if (!strcmp(argv[a], "--")) {
            a++;
            break;
        }
        for (; *q; q++) {
            if (*q == 'c') c = 1;
            else if (*q == 'w') w = 1;
            else if (*q == 'r') r = 1;
            else if (*q == 'a') ap = 1;
            else if (*q == 'n') nn = 1;
            else if (*q == 'p') p = 1;
            else if (*q == 's') s = 1;
            else if (*q == 'd') {
                d = 1;
                if (q[1]) {
                    dval = q + 1;
                    q += strlen(q) - 1;
                } else if (a + 1 < argc) {
                    dval = argv[++a];
                } else {
                    err2(sh, io, "-d", "option requires an argument");
                    return 2;
                }
            } else {
                char opt[3];
                opt[0] = '-';
                opt[1] = *q;
                opt[2] = 0;
                err2(sh, io, opt, "invalid option");
                say(sh, io->err, "history: usage: history [-c] [-d offset] [n] or history -anrw [filename] or history -ps arg [arg...]\n");
                return 2;
            }
        }
    }
    if ((w + r + ap + nn) > 1 || ((w || r || ap || nn) && (p || s)) || (p && s)) {
        err2(sh, io, "history", "cannot use more than one of -anrw");
        return 1;
    }
    if (c)
        hist_clear(sh);
    if (d) {
        char *e;
        long off = strtol(dval, &e, 10), cnt = hist_count(sh), idx;
        if (*e || !*dval) {
            err2(sh, io, dval, "history position out of range");
            return 1;
        }
        idx = off < 0 ? cnt + off : off - 1 - (hist_own(sh) ? sh->hist_base : 0);
        if (off == 0 || hist_del(sh, idx) < 0) {
            err2(sh, io, dval, "history position out of range");
            return 1;
        }
        return 0;
    }
    if (c && a >= argc)
        return 0;
    if (p) {
        for (i = a; i < argc; i++)
            sayl(sh, io->out, argv[i], "\n", NULL);
        return 0;
    }
    if (s) {
        size_t len = 0;
        char *joined;
        for (i = a; i < argc; i++)
            len += strlen(argv[i]) + 1;
        if (!a || a >= argc)
            return 0;
        joined = (char *)malloc(len + 1);
        if (!joined)
            return 1;
        joined[0] = 0;
        for (i = a; i < argc; i++) {
            if (i > a)
                strcat(joined, " ");
            strcat(joined, argv[i]);
        }
        hist_add(sh, joined);
        free(joined);
        return 0;
    }
    if (r || nn) {
        const char *f = hist_file(sh, argc, argv, a);
        long nread = 0;
        if (!f || hist_read_file(sh, f, nn ? sh->hist_saved : 0, &nread) < 0) {
            err2(sh, io, f ? f : "history", f ? "cannot open" : "no history file");
            return 1;
        }
        sh->hist_saved = hist_count(sh);
        return 0;
    }
    if (w || ap) {
        const char *f = hist_file(sh, argc, argv, a);
        long cnt = hist_count(sh), from = 0;
        sh_fh fh;
        if (!f) {
            err2(sh, io, "history", "no history file");
            return 1;
        }
        if (ap && !hist_own(sh))
            return 0;   /* the console appended each line as it was entered */
        if (ap)
            from = sh->hist_saved;
        fh = sh->os.open(sh->os.data, f, ap ? SH_OPEN_APPEND : SH_OPEN_WRITE);
        if (!fh) {
            err2(sh, io, f, "cannot open");
            return 1;
        }
        for (i = (int)from; i < cnt; i++) {
            char *l = hist_get(sh, i);
            if (l) {
                put(sh, fh, l, (long)strlen(l));
                put(sh, fh, "\n", 1);
                free(l);
            }
        }
        sh->os.close(sh->os.data, fh);
        sh->hist_saved = cnt;
        return 0;
    }
    total = hist_count(sh);
    if (a < argc) {
        char *e;
        long n = strtol(argv[a], &e, 10);
        if (*e || !*argv[a]) {
            err2(sh, io, argv[a], "numeric argument required");
            return 2;
        }
        if (a + 1 < argc) {
            err2(sh, io, "history", "too many arguments");
            return 1;
        }
        if (n < 0) {
            err2(sh, io, argv[a], "history position out of range");
            return 1;
        }
        st = total - n;
        if (st < 0)
            st = 0;
        if (n == 0)
            st = total;
    }
    for (i = (int)st; i < total; i++) {
        char *l = hist_get(sh, i), num[24];
        if (!l)
            continue;
        {   /* the number right-aligned in 5 columns, then two spaces */
            long v = i + 1 + (hist_own(sh) ? sh->hist_base : 0);
            int k = 4;
            memset(num, ' ', 7);
            num[7] = 0;
            do {
                num[k--] = (char)('0' + v % 10);
                v /= 10;
            } while (v && k >= 0);
        }
        sayl(sh, io->out, num, l, "\n", NULL);
        free(l);
    }
    return 0;
}

void sh_hist_load(sh_shell *sh)
{
    const char *f = sh_get(&sh->ctx, "HISTFILE");
    if (!(sh->opts & SO_INTERACTIVE) || !f || !*f || !hist_own(sh))
        return;
    hist_read_file(sh, f, 0, 0);
    sh->hist_saved = sh->hist.n;
}

void sh_hist_save(sh_shell *sh)
{
    const char *f = sh_get(&sh->ctx, "HISTFILE");
    long keep = hist_size_limit(sh, "HISTFILESIZE", 500), i, first;
    sh_list all;
    sh_fh fh;
    if (!(sh->opts & SO_INTERACTIVE) || !f || !*f || !hist_own(sh) || sh->hist_saved >= sh->hist.n)
        return;
    memset(&all, 0, sizeof(all));
    fh = sh->os.open(sh->os.data, f, SH_OPEN_READ);   /* what other windows added meanwhile stays */
    if (fh) {
        char line[1024];
        long n;
        while ((n = sh->os.read_line(sh->os.data, fh, line, sizeof(line) - 1)) >= 0) {
            while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
                n--;
            line[n] = 0;
            if (n)
                sh_list_add(&all, line);
        }
        sh->os.close(sh->os.data, fh);
    }
    for (i = sh->hist_saved; i < sh->hist.n; i++)
        sh_list_add(&all, sh->hist.v[i]);
    first = keep >= 0 && all.n > keep ? all.n - keep : 0;
    fh = sh->os.open(sh->os.data, f, SH_OPEN_WRITE);
    if (fh) {
        for (i = first; i < all.n; i++) {
            put(sh, fh, all.v[i], (long)strlen(all.v[i]));
            put(sh, fh, "\n", 1);
        }
        sh->os.close(sh->os.data, fh);
        sh->hist_saved = sh->hist.n;
    }
    sh_list_free(&all);
}

static long b_hash(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int a = 1, l = 0, d = 0, t = 0, r = 0, i, o;
    const char *pathname = 0;
    long st = 0, nnames;
    hash_sync(sh);
    if (!(sh->opts & SO_HASHALL)) {
        err2(sh, io, "hash", "hashing disabled");
        return 1;
    }
    for (; a < argc && argv[a][0] == '-' && argv[a][1]; a++) {
        const char *p = argv[a] + 1;
        if (!strcmp(argv[a], "--")) {
            a++;
            break;
        }
        for (; *p; p++) {
            if (*p == 'l') l = 1;
            else if (*p == 'd') d = 1;
            else if (*p == 't') t = 1;
            else if (*p == 'r') r = 1;
            else if (*p == 'p') {
                if (p[1])
                    pathname = p + 1;
                else if (a + 1 < argc)
                    pathname = argv[++a];
                else {
                    err2(sh, io, "hash", "-p: option requires an argument");
                    hash_usage(sh, io);
                    return 2;
                }
                break;
            } else {
                char opt[3];
                opt[0] = '-';
                opt[1] = *p;
                opt[2] = 0;
                sayl(sh, io->err, "vsh: hash: ", opt, ": invalid option\n", NULL);
                hash_usage(sh, io);
                return 2;
            }
        }
    }
    nnames = argc - a;
    if (r) {
        sh_list_free(&sh->hashtab);
        if (a == argc && !pathname)
            return 0;
    }
    if (pathname && a < argc) {
        hash_put(sh, argv[a], pathname, 0);
        return 0;
    }
    if (a == argc) {
        if (!sh->hashtab.n) {
            if (!l)
                say(sh, io->out, "hash: hash table empty\n");
            return 0;
        }
        if (!l)
            say(sh, io->out, "hits\tcommand\n");
        for (o = 0; o < 256; o++) {
            for (i = sh->hashtab.n - 1; i >= 0; i--) {
                char nm[256], path[512], hits[24];
                const char *e = strchr(sh->hashtab.v[i], '\t');
                long n = (long)(e - sh->hashtab.v[i]);
                if (n > 255)
                    n = 255;
                memcpy(nm, sh->hashtab.v[i], (size_t)n);
                nm[n] = 0;
                if ((int)hash_bucket(nm) != o)
                    continue;
                hash_path(sh, i, path, sizeof(path));
                if (l) {
                    sayl(sh, io->out, "builtin hash -p ", path, " ", nm, "\n", NULL);
                } else {
                    long h = hash_hits(sh, i);
                    int k;
                    num(hits, h);
                    for (k = (int)strlen(hits); k < 4; k++)
                        say(sh, io->out, " ");
                    sayl(sh, io->out, hits, "\t", path, "\n", NULL);
                }
            }
        }
        return 0;
    }
    for (; a < argc; a++) {
        char path[512];
        i = hash_find(sh, argv[a]);
        if (d) {
            if (i < 0) {
                sayl(sh, io->err, "vsh: hash: ", argv[a], ": not found\n", NULL);
                st = 1;
            } else
                hash_del(sh, i);
        } else if (t) {
            if (i < 0) {
                sayl(sh, io->err, "vsh: hash: ", argv[a], ": not found\n", NULL);
                st = 1;
            } else {
                hash_path(sh, i, path, sizeof(path));
                hash_put(sh, argv[a], path, hash_hits(sh, i) + 1);
                if (l)
                    sayl(sh, io->out, "builtin hash -p ", path, " ", argv[a], "\n", NULL);
                else if (nnames > 1)
                    sayl(sh, io->out, argv[a], "\t", path, "\n", NULL);
                else
                    sayl(sh, io->out, path, "\n", NULL);
            }
        } else if (strchr(argv[a], '/')) {
            continue;
        } else if (hash_search(sh, argv[a], path, sizeof(path))) {
            hash_put(sh, argv[a], path, 0);
        } else {
            sayl(sh, io->err, "vsh: hash: ", argv[a], ": not found\n", NULL);
            st = 1;
        }
    }
    return st;
}

/* enable: builtins switched off by name (sh->disabled); the listing is of the builtins vsh has */
static builtin_fn find_bi(const sh_shell *sh, const char *name)
{
    int i;
    for (i = 0; i < sh->disabled.n; i++)
        if (!strcmp(sh->disabled.v[i], name))
            return 0;
    return find_builtin(name);
}

static long b_enable(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int a = 1, n = 0, all = 0, k, i;
    long st = 0;
    for (; a < argc && argv[a][0] == '-' && argv[a][1]; a++) {
        const char *p = argv[a] + 1;
        if (!strcmp(argv[a], "--")) {
            a++;
            break;
        }
        for (; *p; p++) {
            if (*p == 'n') n = 1;
            else if (*p == 'a') all = 1;
            else if (*p == 'p' || *p == 's') ;
            else {
                char opt[3];
                opt[0] = '-';
                opt[1] = *p;
                opt[2] = 0;
                sayl(sh, io->err, "vsh: enable: ", opt, ": invalid option\n", NULL);
                say(sh, io->err, "enable: usage: enable [-a] [-dnps] [-f filename] [name ...]\n");
                return 2;
            }
        }
    }
    if (a == argc) {
        for (k = 0; builtins[k].name; k++) {
            int off = 0;
            for (i = 0; i < sh->disabled.n; i++)
                if (!strcmp(sh->disabled.v[i], builtins[k].name))
                    off = 1;
            if ((n && !off) || (!n && !all && off))
                continue;
                        sayl(sh, io->out, off ? "enable -n " : "enable ", builtins[k].name, "\n", NULL);
        }
        return 0;
    }
    for (; a < argc; a++) {
        if (!find_builtin(argv[a])) {
            sayl(sh, io->err, "vsh: enable: ", argv[a], ": not a shell builtin\n", NULL);
            st = 1;
            continue;
        }
        for (i = 0; i < sh->disabled.n; i++)
            if (!strcmp(sh->disabled.v[i], argv[a]))
                break;
        if (n && i == sh->disabled.n)
            sh_list_add(&sh->disabled, argv[a]);
        else if (!n && i < sh->disabled.n) {
            free(sh->disabled.v[i]);
            memmove(sh->disabled.v + i, sh->disabled.v + i + 1, (size_t)(sh->disabled.n - i - 1) * sizeof(char *));
            sh->disabled.n--;
        }
    }
    return st;
}

static void time_us(pbuf *o, long us)
{
    time_part(o, us, 3, 1);
}

static long b_times(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    long t[4];
    pbuf o = { 0, 0, 0 };
    (void)argc; (void)argv;
    memset(t, 0, sizeof(t));
    if (sh->os.cpu)
        sh->os.cpu(sh->os.data, t);
    time_us(&o, t[0]);
    pb_add(&o, " ", 1);
    time_us(&o, t[1]);
    pb_add(&o, "\n", 1);
    time_us(&o, t[2]);
    pb_add(&o, " ", 1);
    time_us(&o, t[3]);
    pb_add(&o, "\n", 1);
    if (o.s)
        put(sh, io->out, o.s, o.n);
    free(o.s);
    return 0;
}

/* caller [n]: the line, function and file of the call n levels up (BASH_LINENO[n], FUNCNAME[n+1], BASH_SOURCE[n+1]) */
static long b_caller(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    long k = argc > 1 ? atol(argv[1]) : 0, total = sh->nframes + (sh->main_src != 0);
    char d[24];
    const char *fn, *src;
    if (argc > 1 && (!argv[1][0] || strspn(argv[1], "0123456789") != strlen(argv[1]))) {
        sayl(sh, io->err, "vsh: caller: ", argv[1], ": invalid number\n", NULL);
        say(sh, io->err, "caller: usage: caller [expr]\n");
        return 2;
    }
    if (!sh->nframes && argc < 2) {
        say(sh, io->out, "0 NULL\n");  /* bash: the top level of a script has no caller to name */
        return 0;
    }
    if (!sh->nframes || k + 1 >= total)
        return 1;
    sh_ltoa(sh->frames[sh->nframes - 1 - k].line, d);
    if (k + 1 == sh->nframes) {
        fn = "main";
        src = sh->main_src;
    } else {
        fn = sh->frames[sh->nframes - 2 - k].name;
        src = sh->frames[sh->nframes - 2 - k].src;
    }
    if (argc > 1)
        sayl(sh, io->out, d, " ", fn, " ", src, "\n", NULL);
    else
        sayl(sh, io->out, d, " ", src, "\n", NULL);
    return 0;
}

/* ulimit: AmigaDOS has no resource limits; a limit asked for is "unlimited" (-s: the stack builtin's
 * business), one set is refused */
static long b_ulimit(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int a = 1, flag = 'f';
    for (; a < argc && argv[a][0] == '-' && argv[a][1]; a++) {
        const char *p = argv[a] + 1;
        if (!strcmp(argv[a], "--")) {
            a++;
            break;
        }
        for (; *p; p++) {
            if (strchr("SHaPbcdefiklmnpqrstuvxRT", *p)) {
                if (*p != 'S' && *p != 'H')
                    flag = *p;
            } else {
                char opt[3];
                opt[0] = '-';
                opt[1] = *p;
                opt[2] = 0;
                sayl(sh, io->err, "vsh: ulimit: ", opt, ": invalid option\n", NULL);
                say(sh, io->err, "ulimit: usage: ulimit [-SHabcdefiklmnpqrstuvxPRT] [limit]\n");
                return 2;
            }
        }
    }
    if (a < argc) {
        sayl(sh, io->err, "vsh: ulimit: ", argv[a], ": cannot modify limit: not supported on this system\n", NULL);
        return 1;
    }
    (void)flag;
    say(sh, io->out, "unlimited\n");
    return 0;
}

static long b_help(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int k, a, found = 0;
    long st = 0;
    if (argc < 2) {
        for (k = 0; builtins[k].name; k++)
            sayl(sh, io->out, builtins[k].name, "\n", NULL);
        return 0;
    }
    for (a = 1; a < argc; a++) {
        if (argv[a][0] == '-' && argv[a][1])
            continue;
        if (find_builtin(argv[a])) {
            sayl(sh, io->out, argv[a], ": ", argv[a], " [arguments]\n", NULL);
            found = 1;
        } else {
            sayl(sh, io->err, "vsh: help: no help topics match `", argv[a], "'.\n", NULL);
            st = 1;
        }
    }
    (void)found;
    return st;
}

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
    if (!sh->os.stat || (long)strlen(name) + 3 > max)
        return 0;
    if (strchr(name, ':') || strchr(name, '/') || sh_exists(sh, name, 0)) {
        if (!sh_exists(sh, name, 0))
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
        if (sh_exists(sh, out, 0))
            return 1;
    }
    strcpy(out, "C:");
    strcat(out, name);
    return sh_exists(sh, out, 0);
}

static long b_source(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    int ran_main = 0;
    sh_fh fh;
    int borrowed;
    char line[1024];
    char *text = 0;
    long len = 0, n, st = 0;
    int incomplete = 0;
    char found[512];
    const char *file;
    sh_list saved_args;
    int swap = argc > 2;
    if (argc < 2)
        return 2;
    file = argv[1];
    /* a name without a slash is looked for in $PATH first (bash: sourcepath), then here */
    if (shopt_get(sh, "sourcepath") && !strchr(file, '/') && !strchr(file, ':') && find_command_file(sh, file, found, sizeof(found)))
        file = found;
    borrowed = dev_fd(file) >= 0;
    fh = borrowed ? fd_get(sh, io, dev_fd(file)) : sh->os.open(sh->os.data, file, SH_OPEN_READ);
    if (!fh) {
        err2(sh, io, argv[1], "cannot open");
        posix_fatal(sh, 1);
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
    if (!borrowed)
        sh->os.close(sh->os.data, fh);
    if (swap) {
        int k;
        saved_args = sh->ctx.args;
        memset(&sh->ctx.args, 0, sizeof(sh->ctx.args));
        for (k = 2; k < argc; k++)
            sh_list_add(&sh->ctx.args, argv[k]);
    }
    if (text) {
        const char *outer_src = sh->cur_src;
        int is_main = sh->main_src && !sh->main_run && !strcmp(argv[1], sh->main_src);
        int pushed = !is_main && frame_push(sh, "source", file) == 0;
        sh->main_run = 1;
        sh->cur_src = file;
        st = sh_run_text(sh, text, &incomplete);
        ran_main = is_main;
        sh->cur_src = outer_src;
        if (pushed)
            frame_pop(sh);
    }
    free(text);
    if (swap) {
        sh_list_free(&sh->ctx.args);
        sh->ctx.args = saved_args;
    }
    if (!ran_main && sh->traps[TRAP_RETURN] && !sh->exiting && !sh->returning) {
        sh->ctx.status = st;
        SH_HIT(TRAP_RETURN);
        pseudo_trap(sh, TRAP_RETURN, io);
    }
    if (sh->returning) {   /* return in a sourced file ends the file */
        sh->returning = 0;
        st = sh->ctx.status;
    }
    return st;
}

/* ---- commands ----------------------------------------------------------------- */

static long exec_node(sh_shell *sh, const sh_node *n, const sh_io *io);

/* bash expands an alias only for a command word written literally and unquoted: a word that comes out of
 * a quote, a backslash, $ or a back quote is never one. The parser keeps the word as written, so the
 * test is on its text. */
static int alias_word_literal(const char *w)
{
    return w && *w && !strpbrk(w, "'\"\\$`");
}

/* Alias expansion of the first word: its value's words go in front. */
static void apply_alias(sh_shell *sh, sh_list *argv)
{
    int k, depth;
    if (!shopt_get(sh, "expand_aliases"))
        return;
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
    int i, mark = sh->n_locals, outer = sh->local_mark, pushed;
    char *sv_ret = 0, *sv_dbg = 0, *sv_err = 0;
    long st;
    sh->local_mark = mark;
    memset(&sh->ctx.args, 0, sizeof(sh->ctx.args));
    for (i = 1; i < argv->n; i++)
        sh_list_add(&sh->ctx.args, argv->v[i]);
    SH_HIT(FUNC_CALL);
    pushed = frame_push(sh, f->name, f->src) == 0;
    sh->func_depth++;
    f->busy++;
    /* without set -T / -E the function does not see the DEBUG, RETURN and ERR traps; what it sets
     * itself stays after it, else the outer ones come back */
    if (!(sh->opts & SO_FUNCTRACE)) {
        sv_ret = sh->traps[TRAP_RETURN];
        sv_dbg = sh->traps[TRAP_DEBUG];
        sh->traps[TRAP_RETURN] = sh->traps[TRAP_DEBUG] = 0;
    }
    if (!(sh->opts & SO_ERRTRACE)) {
        sv_err = sh->traps[TRAP_ERR];
        sh->traps[TRAP_ERR] = 0;
    }
    if ((sh->opts & SO_FUNCTRACE) && sh->traps[TRAP_DEBUG] && !sh->trap_busy) {
        SH_HIT(TRAP_DEBUG);
        pseudo_trap(sh, TRAP_DEBUG, io);  /* bash traces the entry too, with the call's BASH_COMMAND */
    }
    st = exec_node(sh, f->body.tree, io);
    f->busy--;
    if (sh->returning)
        st = sh->ctx.status;
    sh->returning = 0;
    if (sh->traps[TRAP_RETURN] && !sh->exiting) {
        sh->ctx.status = st;
        SH_HIT(TRAP_RETURN);
        pseudo_trap(sh, TRAP_RETURN, io);
    }
    if (!(sh->opts & SO_FUNCTRACE)) {
        if (sh->traps[TRAP_RETURN])
            free(sv_ret);
        else
            sh->traps[TRAP_RETURN] = sv_ret;
        if (sh->traps[TRAP_DEBUG])
            free(sv_dbg);
        else
            sh->traps[TRAP_DEBUG] = sv_dbg;
    }
    if (!(sh->opts & SO_ERRTRACE)) {
        if (sh->traps[TRAP_ERR])
            free(sv_err);
        else
            sh->traps[TRAP_ERR] = sv_err;
    }
    if (pushed)
        frame_pop(sh);
    sh->func_depth--;
    sh->local_mark = outer;
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
    size_t len = strcspn(word, "[+=");
    char *name = (char *)malloc(len + 1);
    if (name) {
        memcpy(name, word, len);
        name[len] = 0;
    }
    return name;
}

static void xt_assign(sh_shell *sh, const sh_io *io, const char *name, const char *value);

/* the subscript of an assignment word NAME[sub]..., or 0; *len is its length */
static const char *assign_sub(const char *w, size_t *len)
{
    const char *p = w + strcspn(w, "[+=");
    int d = 1;
    size_t k = 1;
    if (*p != '[')
        return 0;
    while (p[k] && d) {
        d += p[k] == '[' ? 1 : p[k] == ']' ? -1 : 0;
        k++;
    }
    *len = k - 2;
    return p + 1;
}

/* One assignment word as the statement NAME=value runs it (flag 0), as a prefix of a command
 * (1: also exported), or only for the trace of -x (2). The compound NAME=(...) and the
 * subscripted NAME[sub]=value go through the typed store. 0: ok. */
static int do_assign(sh_shell *sh, const char *text, const sh_io *io, int flag)
{
    char *name = assign_name(text), *sub = 0, *v;
    size_t sl = 0;
    const char *sp = assign_sub(text, &sl), *eq = strchr(sp ? sp + sl + 1 : text, '=');
    int app = eq > text && eq[-1] == '+', r = 0;
    if (!name || !eq) {
        free(name);
        return 0;
    }
    if (sp) {
        char *raw = (char *)malloc(sl + 1);
        if (raw) {
            memcpy(raw, sp, sl);
            raw[sl] = 0;
            sub = flag == 2 ? raw : expand_one(sh, raw, io);
            if (flag != 2)
                free(raw);
        }
    }
    if (eq[1] == '(' && eq[strlen(eq) - 1] == ')' && flag != 2) {
        char *body = sdup(eq + 2);
        if (body) {
            body[strlen(body) - 1] = 0;
            r = compound_assign(sh, name, app, body, io);
            free(body);
        }
        if (r)
            err2(sh, io, name, "readonly variable");
        r = r != 0;
    } else if ((v = expand_val(sh, eq + 1, io, SH_ASSIGN)) != 0) {
        if (flag == 2) {
            pbuf nb = { 0, 0, 0 };
            pb_str(&nb, name);
            if (sub) {
                pb_str(&nb, "[");
                pb_str(&nb, sub);
                pb_str(&nb, "]");
            }
            xt_assign(sh, io, nb.s ? nb.s : name, v);
            free(nb.s);
        } else if (sh_assign(&sh->ctx, name, sub, v, app)) {
            err2(sh, io, name, "readonly variable");
            r = 1;
        } else if (sub && !app && flag != 1 && !strcmp(name, "DIRSTACK")) {
            dirstack_write(sh, name, sub, v);
        } else if (flag == 1)
            sh_export(&sh->ctx, name);
        free(v);
    }
    free(sub);
    free(name);
    return r;
}

static int compound_assign(sh_shell *sh, const char *name, int append, const char *body, const sh_io *io)
{
    sh_parse p;
    const sh_word *w;
    const sh_node *t0;
    sh_list keys, vals;
    char *text = (char *)malloc(strlen(body) + 3);
    int assoc, r = 0, i, odd = 0;
    long next;
    name = sh_resolve(&sh->ctx, name);
    assoc = (sh_attr(&sh->ctx, name) & SH_ATTR_ASSOC) != 0;
    memset(&keys, 0, sizeof(keys));
    memset(&vals, 0, sizeof(vals));
    if (!text)
        return 1;
    /* the words of the list are the words of a command: parse "x <list>" and skip the x */
    strcpy(text, "x ");
    strcat(text, body);
    for (i = 0, odd = 0; text[i]; i++) {
        /* newlines separate elements; a comment runs to the end of its line; quotes hide both */
        if (text[i] == '\\' && text[i + 1])
            i++;
        else if (text[i] == '\'' || text[i] == '"') {
            char qc = text[i++];
            while (text[i] && text[i] != qc)
                i += text[i] == '\\' && qc == '"' && text[i + 1] ? 2 : 1;
            if (!text[i])
                break;
        } else if (text[i] == '#' && (i < 3 || text[i - 1] == ' ' || text[i - 1] == '\n' || text[i - 1] == '\t')) {
            while (text[i] && text[i] != '\n')
                text[i++] = ' ';
            if (text[i])
                text[i] = ' ';
        } else if (text[i] == '\n')
            text[i] = ' ';
    }
    sh_parse_text(&p, text);
    free(text);
    t0 = p.tree;
    while (t0 && t0->kind == SH_SEQ && !t0->b)
        t0 = t0->a;
    if (t0 && t0->kind == SH_CMD && t0->words && !p.error) {
        for (w = t0->words->next; w; w = w->next) {
            const char *t = w->text, *rb;
            if (t[0] == '[' && (rb = strstr(t, "]=")) != 0) {
                char *k = (char *)malloc((size_t)(rb - t));
                if (k) {
                    char *kx, *vx;
                    memcpy(k, t + 1, (size_t)(rb - t - 1));
                    k[rb - t - 1] = 0;
                    kx = expand_one(sh, k, io);
                    vx = expand_one(sh, rb + 2, io);
                    sh_list_add(&keys, kx ? kx : "0");
                    sh_list_add(&vals, vx ? vx : "");
                    free(kx);
                    free(vx);
                    free(k);
                }
            } else {
                sh_list f;
                int q;
                const char *er = 0;
                memset(&f, 0, sizeof(f));
                if (!sh_expand(&sh->ctx, t, 0, &f, &er))
                    for (q = 0; q < f.n; q++) {
                        if (assoc && !odd)
                            sh_list_add(&keys, f.v[q]);
                        else {
                            if (!assoc)
                                sh_list_add(&keys, "");
                            sh_list_add(&vals, f.v[q]);
                        }
                        if (assoc)
                            odd = !odd;
                    }
                sh_list_free(&f);
            }
        }
    }
    if (assoc && odd)
        sh_list_add(&vals, "");
    sh_parse_free(&p);
    if (append && !(sh_attr(&sh->ctx, name) & (SH_ATTR_ARRAY | SH_ATTR_ASSOC)))
        sh_attr_change(&sh->ctx, name, assoc ? SH_ATTR_ASSOC : SH_ATTR_ARRAY, 0);
    if (!append && sh_array_reset(&sh->ctx, name, assoc))
        r = 1;
    next = append ? sh_next_index(&sh->ctx, name) : 0;
    for (i = 0; !r && i < vals.n && i < keys.n; i++) {
        char d[24];
        const char *k = keys.v[i];
        if (!k[0]) {
            sh_ltoa(next, d);
            k = d;
        }
        r = sh_assign(&sh->ctx, name, k, vals.v[i], 0);
        if (!r && !assoc)
            dirstack_write(sh, name, k, vals.v[i]);
        if (!assoc) {
            const char *e = 0;
            next = sh_arith(&sh->ctx, k, &e) + 1;
        }
    }
    sh_list_free(&keys);
    sh_list_free(&vals);
    return r;
}

static void save_var(sh_shell *sh, const char *name, saved_var *s)
{
    s->name = sdup(name);
    s->var = sh_var_save(&sh->ctx, name);
}

static void restore_var(sh_shell *sh, saved_var *s)
{
    if (s->name)
        sh_var_restore(&sh->ctx, s->name, s->var);
    free(s->name);
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
    sh_parse_text_at(&p, t.s, (int)sh->lineno); /* its lines count from the line of the eval */
    free(t.s);
    if (p.error) {
        err2(sh, io, "eval", p.incomplete ? "unexpected end of input" : p.error);
        sh_parse_free(&p);
        posix_fatal(sh, 2);
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
    if ((b = find_bi(sh, argv[1])) != 0)
        st = b(sh, argc - 1, argv + 1, io);
    else {
        st = sh->os.run(sh->os.data, argv + 1, io, 1);
        if (st < 0) {
            err_not_found(sh, io, argv[1]);
            /* bash: a shell that is not interactive ends here with 127, posix mode or not
             * (shopt execfail keeps it running; so does an interactive shell) */
            if (!shopt_get(sh, "execfail") && !(sh->opts & SO_INTERACTIVE) && !sh->exiting) {
                SH_HIT(EXEC_MISSING_EXIT);
                sh->exiting = 1;
                sh->exit_status = 127;
            }
            return 127;
        }
    }
    if (!sh->exiting) {
        sh->exiting = 1;
        sh->exit_status = st;
    }
    return st;
}

/* The index of a trap's signal: 0 for EXIT, else its number (name with or without SIG, or a number); -1 if none */
static int trap_index(const char *name)
{
    int n;
    if (!strcmp(name, "EXIT") || !strcmp(name, "SIGEXIT"))
        return 0;
    if (!strcmp(name, "ERR"))
        return TRAP_ERR;
    if (!strcmp(name, "DEBUG"))
        return TRAP_DEBUG;
    if (!strcmp(name, "RETURN"))
        return TRAP_RETURN;
    n = sig_number(name);
    return n >= 0 && n < N_SIG ? n : -1;
}

static void trap_print(sh_shell *sh, const sh_io *io, int i)
{
    if (i >= SH_NSIG)
        sayl(sh, io->out, "trap -- '", sh->traps[i], "' ", i == TRAP_ERR ? "ERR" : i == TRAP_DEBUG ? "DEBUG" : "RETURN", "\n", NULL);
    else
        sayl(sh, io->out, "trap -- '", sh->traps[i], i ? "' SIG" : "' ", i ? sh_signames[i] : "EXIT", "\n", NULL);
}

/* trap, trap -p [sig ...], trap -l, trap action sig ..., trap - sig ..., trap '' sig ...:
 * no arguments lists the traps; "-" (or a first operand that is a number) resets, an
 * empty action ignores the signal. */
static long b_trap(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    const char *action;
    long st = 0;
    int i, first = 2;
    if (argc > 1 && !strcmp(argv[1], "-l")) {
        print_siglist(sh, io);
        return 0;
    }
    if (argc < 2 || !strcmp(argv[1], "-p")) {
        for (i = 2; i < argc; i++) {
            int k = trap_index(argv[i]);
            if (k < 0) {
                err2(sh, io, argv[i], "invalid signal specification");
                st = 1;
            } else if (sh->traps[k])
                trap_print(sh, io, k);
        }
        if (argc <= 2)
            for (i = 0; i < SH_NTRAP; i++)
                if (sh->traps[i])
                    trap_print(sh, io, i);
        return st;
    }
    action = argv[1];
    if (!strcmp(action, "--") && argc > 2) {
        action = argv[2];
        first = 3;
    }
    if (first == argc && trap_index(action) >= 0) {
        action = "-";    /* trap USR1: reset it */
        first--;
    } else if (first == 2 && argv[1][0] >= '0' && argv[1][0] <= '9' && !argv[1][strspn(argv[1], "0123456789")]) {
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
            err2(sh, io, argv[i], "invalid signal specification");
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
    int k = sig > 0 && sig < SH_NSIG ? sig : -1;
    if (k < 0 || !sh->traps[k] || sh->in_trap)
        return 0;   /* a trap that is running is broken like any command */
    if (sh->traps[k][0])
        run_trap_text(sh, sh->traps[k]);
    return 1;
}

/* ERR, DEBUG and RETURN: the action runs with $? as it was; none of the three fires inside a trap */
static void pseudo_trap(sh_shell *sh, int k, const sh_io *io)
{
    sh_parse p;
    long st = sh->ctx.status;
    char *text;
    if (!sh->traps[k] || !sh->traps[k][0] || sh->trap_busy)
        return;
    text = sdup(sh->traps[k]); /* the action may reset its own trap */
    sh->trap_busy = 1;
    sh_parse_text(&p, text);
    if (p.error)
        err2(sh, io, "trap", p.incomplete ? "unexpected end of input" : p.error);
    else
        exec_node(sh, p.tree, io);
    sh_parse_free(&p);
    free(text);
    sh->trap_busy = 0;
    sh->ctx.status = st;
}

/* the text DEBUG shows in BASH_COMMAND for a node (0: none) */
static char *debug_text(const sh_node *n)
{
    pbuf b = { 0, 0, 0 };
    const sh_word *w;
    if (n->kind == SH_CMD || n->kind == SH_ARITHCMD || n->kind == SH_DBRACK) {
        sh_node one = *n;
        char *t;
        one.redirs = n->kind == SH_CMD ? n->redirs : 0;
        t = sh_unparse(&one, 0);
        return t ? t : sdup("");
    } else if (n->kind == SH_FOR || n->kind == SH_SELECT) {
        pb_str(&b, n->kind == SH_SELECT ? "select " : "for ");
        pb_str(&b, n->name);
        if (n->has_in)
            pb_str(&b, " in");
        for (w = n->words; w; w = w->next) {
            pb_add(&b, " ", 1);
            pb_str(&b, w->text);
        }
    } else if (n->kind == SH_CASE) {
        pb_str(&b, "case ");
        pb_str(&b, n->words->text);
        pb_str(&b, " in ");
    }
    return b.s ? b.s : sdup("");
}

/* before a simple command, [[, ((, case, and each pass of a for: the DEBUG trap */
static void debug_trap(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    char *t;
    if (!sh->traps[TRAP_DEBUG] || sh->trap_busy)
        return;
    if (sh->debug_done) {
        sh->debug_done = 0;
        return;
    }
    t = debug_text(n);
    debug_trap_set(sh, t, io);
}

/* one part of for (( )): bash shows it as ((text)), leading blanks dropped, a blank expression as 1 */
static void debug_forarith(sh_shell *sh, const char *text, const sh_io *io)
{
    pbuf b = { 0, 0, 0 };
    if (!sh->traps[TRAP_DEBUG] || sh->trap_busy)
        return;
    while (*text == ' ' || *text == '\t' || *text == '\n')
        text++;
    pb_str(&b, "((");
    pb_str(&b, *text ? text : "1");
    pb_str(&b, "))");
    debug_trap_set(sh, b.s ? b.s : sdup(""), io);
}

static void debug_trap_set(sh_shell *sh, char *t, const sh_io *io)
{
    sh_set(&sh->ctx, "BASH_COMMAND", t);
    free(t);
    SH_HIT(TRAP_DEBUG);
    pseudo_trap(sh, TRAP_DEBUG, io);
}

void sh_exit_trap(sh_shell *sh)
{
    int exiting = sh->exiting;
    long status = sh->exit_status;
    if (sh->exit_trap_ran || !sh->traps[0])
        return;
    sh->exit_trap_ran = 1;
    SH_HIT(EXIT_TRAP);
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
        sayl(sh, io->out, out, "\n", NULL);
    }
    return 0;
}

/* set -x: PS4 expanded, its first character repeated per nesting level, then
 * the words quoted as bash does (sh_quote), on the command's error stream */
static void xt_begin(sh_shell *sh, const sh_io *io, pbuf *b)
{
    const char *ps4 = sh_get(&sh->ctx, "PS4");
    char *t;
    int k;
    unsigned long saved = sh->opts;
    sh->opts &= ~SO_XTRACE;
    t = expand_one(sh, ps4 ? ps4 : "+ ", io);
    sh->opts = saved;
    if (t && *t) {
        for (k = 0; k < sh->xlevel; k++)
            pb_add(b, t, 1);
        pb_str(b, t);
    }
    free(t);
}

static void xt_end(sh_shell *sh, const sh_io *io, pbuf *b)
{
    pb_str(b, "\n");
    if (b->s)
        put(sh, io->err, b->s, b->n);
    free(b->s);
}

static void xt_assign(sh_shell *sh, const sh_io *io, const char *name, const char *value)
{
    pbuf b = { 0, 0, 0 };
    char *q = sh_quote(value, SH_Q_SINGLE);
    xt_begin(sh, io, &b);
    pb_str(&b, name);
    pb_str(&b, "=");
    pb_str(&b, q ? q : "");
    free(q);
    xt_end(sh, io, &b);
}

static void xt_cmd(sh_shell *sh, const sh_io *io, const sh_list *argv)
{
    pbuf b = { 0, 0, 0 };
    int i;
    xt_begin(sh, io, &b);
    for (i = 0; i < argv->n; i++) {
        char *q = sh_quote(argv->v[i], SH_Q_SINGLE);
        if (i)
            pb_str(&b, " ");
        pb_str(&b, q ? q : "");
        free(q);
    }
    xt_end(sh, io, &b);
}

/* The simple command. *last gets the last word of the command as expanded (malloc'ed; 0 when the
 * expansion failed): the shell's $_ once the command has run. */
static long exec_cmd1(sh_shell *sh, const sh_node *n, const sh_io *parent, int wait, long *job, char **last)
{
    sh_list argv;
    sh_io io;
    const sh_word *a;
    builtin_fn b;
    sh_func *f;
    long st;
    saved_var *saved = 0;   /* on the heap: this frame is on every recursion level */
    int n_saved = 0, n_assigns = 0, nofunc = 0, cmd_mode = 0;
    memset(&argv, 0, sizeof(argv));
    if (job)
        *job = 0;
    sh->subst_ran = 0;
    if (expand_words(sh, n->words, &argv, parent)) {
        sh_list_free(&argv);
        return 1;
    }
    *last = sdup(argv.n ? argv.v[argv.n - 1] : "");
    if (!argv.n) {
        /* only assignments (and redirections): they set shell variables */
        for (a = n->assigns; a; a = a->next) {
            if (sh->opts & SO_XTRACE)
                do_assign(sh, a->text, parent, 2);
            if (do_assign(sh, a->text, parent, 0)) {
                expand_fatal(sh, 0);
                if (posix_fatal(sh, 127))
                    sh->exit_status = 127;
                sh_list_free(&argv);
                return 1;
            }
        }
        if (n->redirs && !redirect(sh, n->redirs, parent, &io))
            close_owned(sh, &io);
        sh_list_free(&argv);
        return sh->subst_ran ? sh->subst_status : 0;
    }
    if (alias_word_literal(n->words->text))
        apply_alias(sh, &argv);
    if (sh->opts & SO_XTRACE) {
        const sh_word *xa;
        for (xa = n->assigns; xa; xa = xa->next)
            do_assign(sh, xa->text, parent, 2);
        xt_cmd(sh, parent, &argv);
    }
    if (!strcmp(argv.v[0], "command")) {
        /* command NAME ...: the builtin or program NAME, never a function
         * of that name (the vshrc's telnet() runs the real telnet) */
        int ck = 1;
        while (ck < argv.n && argv.v[ck][0] == '-' && argv.v[ck][1]) {
            if (strchr(argv.v[ck], 'v'))
                cmd_mode = 2;
            else if (strchr(argv.v[ck], 'V'))
                cmd_mode = 3;
            ck++;
            if (!strcmp(argv.v[ck - 1], "--"))
                break;
        }
        while (ck--) {
            free(argv.v[0]);
            memmove(argv.v, argv.v + 1, (size_t)argv.n * sizeof(char *)); /* and the NULL */
            argv.n--;
        }
        if (!argv.n) {
            sh_list_free(&argv);
            return 0;
        }
        nofunc = 1;
    }
    if (redirect(sh, n->redirs, parent, &io)) {
        if (!nofunc && special_bi(argv.v[0]))
            posix_fatal(sh, 1);
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
        char *name = assign_name(a->text);
        if (name && saved)
            save_var(sh, name, &saved[n_saved++]);
        free(name);
        if (do_assign(sh, a->text, parent, 1) && posix_fatal(sh, !nofunc && special_bi(argv.v[0]) ? 127 : 1)) {
            close_owned(sh, &io);
            while (n_saved > 0)
                restore_var(sh, &saved[--n_saved]);
            free(saved);
            sh_list_free(&argv);
            return 1;
        }
    }
    if (cmd_mode) {
        int q;
        st = 0;
        for (q = 0; q < argv.n; q++)
            if (type_one(sh, &io, argv.v[q], cmd_mode, 0, 0))
                st = 1;
        close_owned(sh, &io);
    } else if (!nofunc && (f = find_func(sh, argv.v[0])) != 0) {
        st = run_function(sh, f, &argv, &io);
        close_owned(sh, &io);
    } else if ((b = find_bi(sh, argv.v[0])) != 0) {
        if (b == b_exec && argv.n == 1 && n->redirs &&
            (parent == &sh->io || (parent->in == sh->io.in && parent->out == sh->io.out && parent->err == sh->io.err))) {
            /* exec with redirections only: they stay for the shell itself */
            sh->io = io;
            sh->io.owned = 0;
            if (io.owned & SH_OWN_FDS)
                fd_commit(sh, io.fdmark);
            st = 0;
        } else {
            unsigned long errs0 = sh->bi_errs;
            sh->wfail = 0;
            st = b(sh, argv.n, argv.v, &io);
            if (sh->wfail && !st)
                st = 1; /* it wrote to a closed stream */
            sh->wfail = 0;
            close_owned(sh, &io);
            /* posix mode: a special builtin that reported an error ends a shell that is not interactive
             * (shift, break, continue only fail; eval, . and exec end it themselves, or run commands
             * whose own errors have been judged) */
            if (sh->bi_errs != errs0 && !nofunc && special_bi(argv.v[0]) && b != b_eval && b != b_source &&
                b != b_exec && b != b_shift && b != b_break && b != b_continue)
                posix_fatal(sh, st);
        }
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
        {
            char hp[512], *name0 = argv.v[0];
            if (hash_note_run(sh, name0, hp, sizeof(hp)))
                argv.v[0] = hp;
            st = sh->os.run(sh->os.data, argv.v, &io, wait);
            argv.v[0] = name0;
        }
        sh->os.suspendable = 0;
        if (!wait) {
            /* a redirection took the place of a pipe end the pipeline opened for this command: that
             * end is of no use to it and must not stay open here, or the next stage never sees EOF */
            if (io.in != parent->in && (parent->owned & SH_OWN_IN))
                sh->os.close(sh->os.data, parent->in);
            if (io.out != parent->out && (parent->owned & SH_OWN_OUT))
                sh->os.close(sh->os.data, parent->out);
            if (io.err != parent->err && (parent->owned & SH_OWN_ERR))
                sh->os.close(sh->os.data, parent->err);
        }
        /* the program only ever gets 0-2: the redirections of this command are undone in the
         * table they were made in; a handle the running background program still uses is
         * closed when its job is waited for */
        if (io.owned & SH_OWN_FDS)
            fd_unwind_job(sh, io.fdmark, !wait && st > 0 ? &io : 0, !wait && st > 0 ? st : 0);
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
    if ((sh->opts & SO_POSIX) && n_saved > 0 && !nofunc && special_bi(argv.v[0]) && !cmd_mode) {
        /* posix mode: the assignments before a special builtin outlast it */
        SH_HIT(POSIX_PERSIST);
        while (n_saved > 0) {
            sh_var_discard(saved[--n_saved].var);
            free(saved[n_saved].name);
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
    ext = !find_bi(sh, argv.v[0]) && !find_func(sh, argv.v[0]) && strcmp(argv.v[0], "command");
    sh_list_free(&argv);
    return ext;
}

/* Run n as a subshell (stage: a pipeline stage, which the caller waits for, keeps the streams it is
 * given; without wait and stage it is a background job): a process of its own when the OS layer can make
 * one (then the shell's variables, directory and functions are safe from
 * it), else here with the directory restored. io's owned streams go with
 * it. wait: its status; else *job (0: it ran here, or did not start). */
static long subshell_mode(sh_shell *sh, const sh_node *n, const sh_io *io, int wait, int stage, long *job)
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
    SH_HIT(SPAWN);
    r = sh->os.spawn(sh->os.data, c, t, io, stage ? SH_SPAWN_STAGE : wait);
    /* the child worked on a copy of the fd table: the redirections of this command are undone
     * here, in the table they were made in (a background child may still use their handles) */
    if (wait && (io->owned & SH_OWN_FDS))
        fd_unwind(sh, io->fdmark, io);
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

/* A command's text for jobs, as bash prints it (the command, then " &" is added by the listing) */
static char *node_text(const sh_node *n)
{
    char *t = sh_unparse(n, 0);
    return t ? t : sdup("");
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
    sh->job_foreign[i] = 0;
    sh->job_seq[i] = ++sh->job_seqno;
    free(sh->job_text[i]);
    sh->job_text[i] = text;
    if (!(sh->opts & SO_INTERACTIVE)) /* bash: only an interactive shell announces a job */
        return;
    num(nb, i + 1);
    sayl(sh, io->err, "[", nb, "] ", NULL);
    num(nb, job);
    sayl(sh, io->err, nb, "\n", NULL);
}

static long subshell(sh_shell *sh, const sh_node *n, const sh_io *io, int wait, long *job)
{
    return subshell_mode(sh, n, io, wait, 0, job);
}

static long exec_cmd(sh_shell *sh, const sh_node *n, const sh_io *parent, int wait, long *job)
{
    char *last = 0;
    long st = exec_cmd1(sh, n, parent, wait, job, &last);
    if (last) {
        sh_set(&sh->ctx, "_", last);
        free(last);
    }
    return st;
}

static long exec_pipeline(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    const sh_node *st[16];
    sh_io sio[16];
    long job[16];
    int started[16];
    int k = stages(n, st, 16), i;
    long status = 0, ps[16];
    sh_list hsnap;
    /* bash runs every stage in a subshell, so a command a stage finds on PATH is hashed there and not in
     * this shell; vsh runs external stages (and the last stage) here, so the table is put back after */
    memset(&hsnap, 0, sizeof(hsnap));
    if (!(sh->opts & SO_LASTPIPE))
        for (i = 0; i < sh->hashtab.n; i++)
            sh_list_add(&hsnap, sh->hashtab.v[i]);
    for (i = 0; i < k; i++) {
        sio[i] = *io;
        sio[i].owned = 0;
    }
    for (i = 0; i + 1 < k; i++) {
        sh_fh rd, wr;
        if (sh->os.pipe(sh->os.data, &rd, &wr)) {
            err2(sh, io, "pipe", "cannot create");
            sh_list_free(&hsnap);
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
    for (i = 0; i < k; i++) {   /* bash runs DEBUG in the shell, for each stage's simple command, before the stage starts */
        job[i] = 0;
        started[i] = 0;
        if (st[i]->kind == SH_CMD)
            debug_trap(sh, st[i], io);
        if (is_external(sh, st[i])) {
            exec_cmd(sh, st[i], &sio[i], 0, &job[i]);
        } else if (sh->os.spawn && (i + 1 < k || !(sh->opts & SO_LASTPIPE))) {
            subshell_mode(sh, st[i], &sio[i], 0, 1, &job[i]);
            started[i] = 1; /* or failed: its streams are gone either way */
        }
    }
    for (i = 0; i < k; i++) {
        ps[i] = 0;
        if (job[i] || started[i])
            continue;
        if (is_external(sh, st[i])) {
            /* it did not start; the OS layer took its streams all the same */
            char *name = expand_one(sh, st[i]->words->text, io);
            err_not_found(sh, io, name ? name : "?");
            free(name);
            ps[i] = 127;
            continue;
        }
        sh->debug_done = st[i]->kind == SH_CMD && sh->traps[TRAP_DEBUG] && !sh->trap_busy;
        ps[i] = exec_node(sh, st[i], &sio[i]);
        sh->debug_done = 0;
        close_owned(sh, &sio[i]);
    }
    for (i = 0; i < k; i++)
        if (job[i])
            ps[i] = job_wait(sh, job[i]);
    status = ps[k - 1];
    if (sh->opts & SO_PIPEFAIL) {
        for (i = k - 1; i >= 0; i--)
            if (ps[i]) {
                if (i != k - 1)
                    SH_HIT(PIPEFAIL);
                status = ps[i];
                break;
            }
    }
    sh_pstat(&sh->ctx, ps, k);
    if (!(sh->opts & SO_LASTPIPE)) {
        sh_list_free(&sh->hashtab);
        sh->hashtab = hsnap;
    }
    return status;
}

/* coproc [NAME] command: the command runs in the background with two pipes; NAME[0] is the descriptor the
 * shell reads its output from, NAME[1] the one it writes its input to, NAME_PID the job. */
static long exec_coproc(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    const char *name = n->name ? n->name : "COPROC";
    char vn[160], d[24];
    sh_fh r1, w1, r2, w2;
    sh_io cio;
    long job = 0;
    int s0, s1;
    if (!sh->os.spawn || strlen(name) > 140) {
        err2(sh, io, "coproc", "not supported here");
        return 1;
    }
    s0 = fd_alloc(sh);
    if (s0 >= 0) {
        sh->fdt[s0].fh = (sh_fh)-1; /* taken for the second search */
        s1 = fd_alloc(sh);
        sh->fdt[s0].fh = 0;
    } else
        s1 = -1;
    if (s0 < 0 || s1 < 0) {
        err2(sh, io, "coproc", "too many open files");
        return 1;
    }
    if (sh->os.pipe(sh->os.data, &r1, &w1) || sh->os.pipe(sh->os.data, &r2, &w2)) {
        err2(sh, io, "coproc", "cannot create a pipe");
        return 1;
    }
    SH_HIT(COPROC);
    cio = *io;
    cio.in = r1;
    cio.out = w2;
    cio.owned = SH_OWN_IN | SH_OWN_OUT;
    if (is_external(sh, n->a))
        exec_cmd(sh, n->a, &cio, 0, &job);
    else {
        sh->cp_close[0] = r2;
        sh->cp_close[1] = w1;
        subshell(sh, n->a, &cio, 0, &job);
        sh->cp_close[0] = sh->cp_close[1] = 0;
    }
    sh->fdt[s0].fh = r2;
    sh->fdt[s0].own = 1;
    sh->fdt[s1].fh = w1;
    sh->fdt[s1].own = 1;
    sh_array_reset(&sh->ctx, name, 0);
    sh_ltoa(3 + s0, d);
    sh_assign(&sh->ctx, name, "0", d, 0);
    sh_ltoa(3 + s1, d);
    sh_assign(&sh->ctx, name, "1", d, 0);
    strcpy(vn, name);
    strcat(vn, "_PID");
    sh_ltoa(job, d);
    sh_set(&sh->ctx, vn, d);
    if (job) {
        add_job(sh, job, node_text(n->a), io);
        sh->ctx.last_bg = job;
    }
    return 0;
}

static long exec_list_loop(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    long st = 0;
    int until = n->kind == SH_UNTIL;
    sh->loop_depth++;
    for (;;) {
        long c;
        sh->cond_depth++;
        c = exec_node(sh, n->a, io);
        sh->cond_depth--;
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

/* The value of arithmetic text: its $ expansions first, then sh_arith. 1: an error was reported. */
static int arith_text(sh_shell *sh, const char *text, const sh_io *io, sh_int *val)
{
    const char *err = 0;
    char *x = expand_val(sh, text, io, 0);
    if (!x)
        return 1;
    *val = sh_arith(&sh->ctx, x, &err);
    free(x);
    if (err) {
        err2(sh, io, "((", err);
        return 1;
    }
    return 0;
}

static long b_let(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    sh_int v = 0;
    int i;
    if (argc < 2) {
        err2(sh, io, "let", "expression expected");
        return 1;
    }
    for (i = 1; i < argc; i++)
        if (arith_text(sh, argv[i], io, &v))
            return 1;
    return v == 0;
}

/* (( expr )): status 0 when the value is not 0, 1 when it is, 1 on an error */
static long exec_arithcmd(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    sh_int v = 0;
    SH_HIT(ARITHCMD);
    if (arith_text(sh, n->words->text, io, &v))
        return 1;
    return v == 0;
}

static int blank_text(const char *t)
{
    while (*t == ' ' || *t == '\t' || *t == '\n')
        t++;
    return !*t;
}

/* for (( init; cond; step )): an empty cond is true; an error in a part ends the loop with status 1 */
static long exec_forarith(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    const sh_word *init = n->words, *cond = init->next, *step = cond->next;
    sh_int v = 0;
    long st = 0;
    SH_HIT(FORARITH);
    debug_forarith(sh, init->text, io);
    if (!blank_text(init->text) && arith_text(sh, init->text, io, &v))
        return 1;
    sh->loop_depth++;
    for (;;) {
        debug_forarith(sh, cond->text, io);
        if (!blank_text(cond->text)) {
            if (arith_text(sh, cond->text, io, &v)) {
                st = 1;
                break;
            }
            if (v == 0)
                break;
        }
        st = exec_node(sh, n->a, io);
        if (sh->breaking) {
            sh->breaking--;
            break;
        }
        sh->continuing = 0;
        if (sh->exiting || sh->returning || sh->intr)
            break;
        debug_forarith(sh, step->text, io);
        if (!blank_text(step->text) && arith_text(sh, step->text, io, &v)) {
            st = 1;
            break;
        }
    }
    sh->loop_depth--;
    return sh->intr ? intr_status(sh) : st;
}

/* ---- [[ ]] ----------------------------------------------------------------------- */

static char *db_word(sh_shell *sh, const char *text, const sh_io *io, int flags)
{
    return expand_val(sh, text, io, flags);
}

/* [[ a =~ re ]]: POSIX ERE through claude/regex.c; BASH_REMATCH is the whole match and the groups
 * (an empty array when there is no match). 1 match, 0 none; an invalid expression is status 2. */
static int db_regex(sh_shell *sh, const char *text, const char *pat, const sh_io *io, int *bad)
{
    char err[80];
    long *caps;
    int ng, i, r;
    cl_re *re = re_compile(pat, RE_POSIX, err, sizeof(err));
    if (!re) {
        err2(sh, io, "[[", err[0] ? err : "invalid regular expression");
        *bad = 2;
        return 0;
    }
    ng = re_ngroups(re);
    caps = (long *)malloc(sizeof(long) * 2 * (size_t)(ng + 1));
    r = caps ? re_search_groups(re, text, (long)strlen(text), 0, caps, ng + 1) : -1;
    re_free(re);
    sh_array_reset(&sh->ctx, "BASH_REMATCH", 0);
    if (r == 1) {
        SH_HIT(REGEX_CAPTURE);
        for (i = 0; i <= ng; i++) {
            char sub[24], *v;
            long a = caps[2 * i], e = caps[2 * i + 1];
            sh_ltoa(i, sub);
            v = (char *)malloc(a >= 0 ? (size_t)(e - a) + 1 : 1);
            if (!v)
                break;
            if (a >= 0)
                memcpy(v, text + a, (size_t)(e - a));
            v[a >= 0 ? e - a : 0] = 0;
            sh_assign(&sh->ctx, "BASH_REMATCH", sub, v, 0);
            free(v);
        }
    }
    free(caps);
    if (r < 0)
        *bad = 1;
    return r == 1;
}

/* 1 true, 0 false; *bad set when an error was reported (status 1 then, 2 for a syntax error) */
static int db_eval(sh_shell *sh, const sh_node *n, const sh_io *io, int *bad)
{
    const char *op = n->name ? n->name : "";
    char *a, *b = 0;
    int r = 0;
    tctx t;
    if (!strcmp(op, "||") || !strcmp(op, "&&")) {
        r = db_eval(sh, n->a, io, bad);
        if (*bad || (op[0] == '|') == (r != 0))
            return r;
        return db_eval(sh, n->b, io, bad);
    }
    if (!strcmp(op, "!"))
        r = !db_eval(sh, n->a, io, bad);
    else if (!strcmp(op, "("))
        r = db_eval(sh, n->a, io, bad);
    else {
        int pat = !strcmp(op, "==") || !strcmp(op, "=") || !strcmp(op, "!=");
        int rx = !strcmp(op, "=~");
        t.sh = sh;
        t.io = io;
        t.err = 0;
        a = db_word(sh, n->words->text, io, 0);
        if (n->words->next)
            b = db_word(sh, n->words->next->text, io, pat ? SH_PATTERN : rx ? SH_REGEX : 0);
        if (!a || (n->words->next && !b)) {
            *bad = 1;
            free(a);
            free(b);
            return 0;
        }
        if (!op[0])
            r = a[0] != 0;
        else if (!b)
            r = sh_test_unary(&t, op[1], a);
        else if (pat)
            r = sh_match(b, a, sh->ctx.nocasematch) == (op[0] != '!');
        else if (rx)
            r = db_regex(sh, a, b, io, bad);
        else if (op[0] == '<' || op[0] == '>')
            r = op[0] == '<' ? strcmp(a, b) < 0 : strcmp(a, b) > 0;
        else if (!strcmp(op, "-nt") || !strcmp(op, "-ot") || !strcmp(op, "-ef"))
            r = sh_test_binary(&t, a, op, b);
        else {
            const char *e1 = 0, *e2 = 0;
            sh_int x = sh_arith(&sh->ctx, a, &e1), y = sh_arith(&sh->ctx, b, &e2);
            if (e1 || e2) {
                err2(sh, io, "[[", e1 ? e1 : e2);
                *bad = 1;
            } else
                r = !strcmp(op, "-eq") ? x == y : !strcmp(op, "-ne") ? x != y : !strcmp(op, "-lt") ? x < y
                  : !strcmp(op, "-le") ? x <= y : !strcmp(op, "-gt") ? x > y : x >= y;
        }
        if (t.err)
            *bad = 2;
        free(a);
        free(b);
    }
    return r;
}

static long exec_dbrack(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    int bad = 0, r;
    SH_HIT(DBRACK);
    r = db_eval(sh, n, io, &bad);
    return bad ? bad : !r;
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
        debug_trap(sh, n, io);
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

/* bash's select: the numbered menu on stderr, PS3, a reply read into REPLY; an empty reply shows the menu again,
 * anything that is not a number in range sets the variable empty; the end of input ends the loop (status 1) */
static int digits_of(long v)
{
    char d[24];
    num(d, v);
    return (int)strlen(d);
}

static void select_menu(sh_shell *sh, const sh_io *io, const sh_list *items)
{
    const char *cv = sh_get(&sh->ctx, "COLUMNS");
    long cols = cv ? atol(cv) : 80, maxlen = 0, wide, ncols, nrows, row, i, n = items->n, idx, pos;
    char d[24];
    int ind = digits_of(n), firstw;
    if (cols <= 0)
        cols = 80;
    for (i = 0; i < n; i++)
        if ((long)strlen(items->v[i]) > maxlen)
            maxlen = (long)strlen(items->v[i]);
    wide = maxlen + ind + 4;
    ncols = cols / wide;
    if (ncols < 1)
        ncols = 1;
    nrows = n / ncols + (n % ncols ? 1 : 0);
    ncols = n / nrows + (n % nrows ? 1 : 0);
    if (nrows == 1) {
        nrows = ncols;
        ncols = 1;
    }
    firstw = digits_of(nrows);
    for (row = 0; row < nrows; row++) {
        pos = 0;
        for (idx = row;;) {
            int w = pos == 0 ? firstw : ind, k;
            long from;
            num(d, idx + 1);
            for (k = (int)strlen(d); k < w; k++)
                say(sh, io->err, " ");
            sayl(sh, io->err, d, ") ", items->v[idx], NULL);
            from = pos + (long)strlen(items->v[idx]) + w + 2;
            idx += nrows;
            if (idx >= n)
                break;
            pos += wide;
            while (from < pos) {
                if (pos / 8 > from / 8) {
                    say(sh, io->err, "\t");
                    from += 8 - from % 8;
                } else {
                    say(sh, io->err, " ");
                    from++;
                }
            }
        }
        say(sh, io->err, "\n");
    }
}

static long exec_select(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    sh_list items;
    long st = 0;
    int i, show = 1;
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
    if (!items.n) {
        sh_list_free(&items);
        return 0;
    }
    sh->loop_depth++;
    for (;;) {
        const char *ps3, *rep;
        char *rd[2];
        long r;
        char *end;
        if (show)
            select_menu(sh, io, &items);
        ps3 = sh_get(&sh->ctx, "PS3");
        say(sh, io->err, ps3 ? ps3 : "#? ");
        rd[0] = (char *)"read";
        rd[1] = (char *)"REPLY";
        if (b_read(sh, 2, rd, io) || sh->intr) {
            say(sh, io->out, "\n");
            st = 1;
            break;
        }
        rep = sh_get(&sh->ctx, "REPLY");
        if (!rep || !*rep) {
            show = 1;
            continue;
        }
        show = 0;
        r = strtol(rep, &end, 10);
        if (end == rep || *end || r < 1 || r > items.n)
            sh_set(&sh->ctx, n->name, "");
        else
            sh_set(&sh->ctx, n->name, items.v[r - 1]);
        debug_trap(sh, n, io);
        SH_HIT(SELECT_PASS);
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

/* ---- time: TIMEFORMAT's %R %U %S %P, a precision digit and l (minutes), through os.now and os.cpu ---- */

static void time_part(pbuf *o, long us, int prec, int lng)
{
    long sec = us / 1000000, frac = us % 1000000, div = 1000000;
    char d[24];
    int k;
    if (lng) {
        num(d, sec / 60);
        pb_str(o, d);
        pb_add(o, "m", 1);
        sec %= 60;
    }
    num(d, sec);
    pb_str(o, d);
    if (prec > 6)
        prec = 6;
    if (prec > 0) {
        pb_add(o, ".", 1);
        for (k = 0; k < prec; k++)
            div /= 10;
        num(d, frac / div);
        for (k = (int)strlen(d); k < prec; k++)
            pb_add(o, "0", 1);
        pb_str(o, d);
    }
    if (lng)
        pb_add(o, "s", 1);
}

static void time_report(sh_shell *sh, const sh_io *io, const char *fmt, long real, long user, long sys)
{
    pbuf o = { 0, 0, 0 };
    const char *f;
    for (f = fmt; *f; f++) {
        int prec = 3, lng = 0;
        const char *s = f + 1;
        if (*f != '%' || !*s) {
            pb_add(&o, f, 1);
            continue;
        }
        if (*s == '%') {
            pb_add(&o, "%", 1);
            f++;
            continue;
        }
        if (*s == 'P') {
            long p100 = real > 0 ? (user + sys) * 10000 / real : 0;
            char d[24];
            num(d, p100 / 100);
            pb_str(&o, d);
            pb_add(&o, ".", 1);
            num(d, p100 % 100);
            if (p100 % 100 < 10)
                pb_add(&o, "0", 1);
            pb_str(&o, d);
            f = s;
            continue;
        }
        if (*s >= '0' && *s <= '9') {
            prec = *s - '0';
            s++;
        }
        if (*s == 'l') {
            lng = 1;
            s++;
        }
        if (*s == 'R')
            time_part(&o, real, prec, lng);
        else if (*s == 'U')
            time_part(&o, user, prec, lng);
        else if (*s == 'S')
            time_part(&o, sys, prec, lng);
        else {
            char bad[2];
            bad[0] = *s;
            bad[1] = 0;
            sayl(sh, io->err, "vsh: TIMEFORMAT: `", bad, "': invalid format character\n", NULL);
            free(o.s);
            return;
        }
        f = s;
    }
    pb_add(&o, "\n", 1);
    if (o.s)
        put(sh, io->err, o.s, o.n);
    free(o.s);
}

static void time_now(sh_shell *sh, long *real, long *user, long *sys)
{
    long sec = 0, us = 0, t[4];
    memset(t, 0, sizeof(t));
    if (sh->os.now)
        sec = sh->os.now(sh->os.data, &us);
    *real = sec * 1000000 + us;
    if (sh->os.cpu)
        sh->os.cpu(sh->os.data, t);
    *user = t[0] + t[2];
    *sys = t[1] + t[3];
}

static long exec_time(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    long r0, u0, s0, r1, u1, s1, st = 0;
    const char *fmt;
    SH_HIT(TIME_RUN);
    time_now(sh, &r0, &u0, &s0);
    if (n->a)
        st = exec_node(sh, n->a, io);
    time_now(sh, &r1, &u1, &s1);
    fmt = sh_get(&sh->ctx, "TIMEFORMAT");
    if (!fmt)
        fmt = n->has_in ? "real %2R\nuser %2U\nsys %2S" : "\nreal\t%3lR\nuser\t%3lU\nsys\t%3lS";
    if (*fmt)
        time_report(sh, io, fmt, r1 - r0, u1 - u0, s1 - s0);
    return st;
}

static long exec_case(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    char *subject = expand_one(sh, n->words->text, io);
    const sh_case *c;
    const sh_word *p;
    long st = 0;
    int fall = 0;
    if (!subject)
        return 1;
    for (c = n->cases; c; c = c->next) {
        int hit = fall;
        for (p = c->patterns; !hit && p; p = p->next) {
            char *pat = expand_one(sh, p->text, io);
            hit = pat && sh_match(pat, subject, sh->ctx.nocasematch);
            free(pat);
        }
        if (hit) {
            st = exec_node(sh, c->body, io);
            if (c->term == 0 || sh->exiting || sh->intr)
                break;
            fall = c->term == 1; /* ;& runs the next body; ;;& tests the next patterns */
        }
    }
    free(subject);
    return st;
}

/* The status an unwinding line ends with: 130 after Ctrl-C, 2 after
 * "nested too deeply", 1 after an expansion error that bash does not exit on. */
static long intr_status(const sh_shell *sh)
{
    return sh->intr == 2 ? 2 : sh->intr == 3 ? 1 : 130;
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
    case SH_IF: {
        long c;
        sh->cond_depth++;
        c = exec_node(sh, n->a, io);
        sh->cond_depth--;
        if (!c)
            return exec_node(sh, n->b, io);
        return n->c ? exec_node(sh, n->c, io) : 0;
    }
    case SH_WHILE:
    case SH_UNTIL:
        return exec_list_loop(sh, n, io);
    case SH_FOR:
        return exec_for(sh, n, io);
    case SH_SELECT:
        return exec_select(sh, n, io);
    case SH_FORARITH:
        return exec_forarith(sh, n, io);
    case SH_ARITHCMD:
        return exec_arithcmd(sh, n, io);
    case SH_DBRACK:
        return exec_dbrack(sh, n, io);
    case SH_CASE:
        return exec_case(sh, n, io);
    default:
        return 0;
    }
}

static long exec_node1(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    long st = 0;
    sh_io rio;
    if (!n || sh->exiting)
        return sh->ctx.status;
    if ((sh->opts & SO_NOEXEC) && !(sh->opts & SO_INTERACTIVE))
        return 0;
    if (n->line)
        sh->lineno = n->line;
    if (poll_break(sh))
        return 130;
    if (sh->stack_limit && (unsigned long)&rio < sh->stack_limit) {
        /* the stack is nearly used up: stop here, as a clean error,
         * rather than run past it (on the Amiga that corrupts memory) */
        char d[16];
        num(d, sh->func_depth);
        sayl(sh, io->err, "vsh: nested too deeply (", d, " function levels)\n", NULL);
        sh->intr = 2; /* unwind like Ctrl-C, but as an error: status 2 */
        return 2;
    }
    if (sh->traps[TRAP_DEBUG] && (n->kind == SH_CMD || n->kind == SH_ARITHCMD || n->kind == SH_DBRACK || n->kind == SH_CASE))
        debug_trap(sh, n, io);
    switch (n->kind) {
    case SH_CMD:
        st = exec_cmd(sh, n, io, 1, 0);
        sh_pstat(&sh->ctx, &st, 1);
        break;
    case SH_SEQ:
        st = exec_node(sh, n->a, io);
        sh->ctx.status = st;
        if (!sh->exiting && !sh->breaking && !sh->continuing && !sh->returning && !sh->intr && n->b)
            st = exec_node(sh, n->b, io);
        break;
    case SH_BG: {
        long job = 0;
        if (n->a && n->a->kind == SH_CMD)
            debug_trap(sh, n->a, io); /* bash runs DEBUG in the shell, before the fork */
        if (n->a && is_external(sh, n->a))
            exec_cmd(sh, n->a, io, 0, &job);
        else if (n->a && sh->os.spawn) {
            sh_io bio = *io;
            bio.owned = 0;
            subshell(sh, n->a, &bio, 0, &job);
        } else {
            sh->debug_done = n->a && n->a->kind == SH_CMD && sh->traps[TRAP_DEBUG] && !sh->trap_busy;
            exec_node(sh, n->a, io); /* no subshell processes: it runs now */
            sh->debug_done = 0;
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
        sh->cond_depth++;
        st = exec_node(sh, n->a, io);
        sh->cond_depth--;
        if (!st && !sh->exiting)
            st = exec_node(sh, n->b, io);
        break;
    case SH_OR:
        sh->cond_depth++;
        st = exec_node(sh, n->a, io);
        sh->cond_depth--;
        if (st && !sh->exiting)
            st = exec_node(sh, n->b, io);
        break;
    case SH_TIME:
        st = exec_time(sh, n, io);
        break;
    case SH_COPROC:
        st = exec_coproc(sh, n, io);
        break;
    case SH_NOT:
        sh->cond_depth++;
        st = !exec_node(sh, n->a, io);
        sh->cond_depth--;
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
    case SH_SELECT:
    case SH_FORARITH:
    case SH_ARITHCMD:
    case SH_DBRACK:
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
        sh_func *f;
        if ((sh->opts & SO_POSIX) && special_bi(n->name)) {
            /* posix mode: a function cannot take the name of a special builtin */
            sayl(sh, io->err, "vsh: `", n->name, "': is a special builtin\n", NULL);
            posix_fatal(sh, 2);
            st = 2;
            break;
        }
        f = find_func(sh, n->name);
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
        free(f->src);
        f->src = sdup(sh->cur_src ? sh->cur_src : "");
        st = f->body.tree || !n->a ? 0 : 1;
        break;
    }
    }
    if (st && sh->traps[TRAP_ERR] && !sh->cond_depth && !sh->exiting && !sh->returning && !sh->intr &&
        (n->kind == SH_CMD || n->kind == SH_PIPE || n->kind == SH_SUBSHELL)) {
        SH_HIT(TRAP_ERR);
        sh->ctx.status = st;
        pseudo_trap(sh, TRAP_ERR, io);
    }
    if (st && (sh->opts & SO_ERREXIT) && !sh->cond_depth && !sh->exiting && !sh->returning && !sh->intr &&
        (n->kind == SH_CMD || n->kind == SH_PIPE || n->kind == SH_SUBSHELL)) {
        SH_HIT(ERREXIT);
        sh->exiting = 1;
        sh->exit_status = st;
    }
    sh->ctx.status = st;
    return st;
}

/* a node ran: the temp files its expansions and redirections made are done with */
static long exec_node(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    int mark = sh->ntmp;
    long st = exec_node1(sh, n, io);
    if (sh->ntmp > mark)
        tmp_sweep(sh, mark);
    return st;
}

long sh_exec(sh_shell *sh, const sh_node *n, const sh_io *io)
{
    return exec_node(sh, n, io);
}

/* The input is read the way bash reads it: one command at a time, as many lines as the command
 * needs, each run before the next is parsed (so set -v echoes exactly the lines read after it was
 * set, an alias made on one line works on the next, and a syntax error stops the script after the
 * commands before it). */
/* a non-interactive shell reads the file BASH_ENV names before its command (bash; the name is used as it
 * stands, not expanded a second time); a missing file is no error */
void sh_startup_env(sh_shell *sh)
{
    if (sh->opts & SO_POSIX) {
        /* posix mode: an interactive shell reads $ENV (expanded as a word), none reads BASH_ENV */
        const char *f = sh_get(&sh->ctx, "ENV");
        char *x, *q, *cmd;
        if (!(sh->opts & SO_INTERACTIVE) || !f || !*f)
            return;
        x = expand_one(sh, f, &sh->io);
        q = x ? sh_quote(x, SH_Q_SINGLE) : 0;
        cmd = q ? (char *)malloc(2 * strlen(q) + 40) : 0;
        if (cmd) {
            SH_HIT(POSIX_ENV);
            strcpy(cmd, "if [ -r ");
            strcat(cmd, q);
            strcat(cmd, " ]; then . ");
            strcat(cmd, q);
            strcat(cmd, "; fi");
            sh_run_text(sh, cmd, 0);
        }
        free(cmd);
        free(q);
        free(x);
        return;
    }
    {
        const char *f = sh_get(&sh->ctx, "BASH_ENV");
        if ((sh->opts & SO_INTERACTIVE) || !f || !*f)
            return;
        SH_HIT(BASH_ENV_RUN);
        sh_run_text(sh, "if [ -r \"$BASH_ENV\" ]; then . \"$BASH_ENV\"; fi", 0);
    }
}

/* A login shell (--login, -l): shopt login_shell is on, and it reads the system's profile, then the
 * user's (names after ENV:vsh/vshrc and $HOME/.vshrc). Not ~/.bash_profile: bash's files on an Amiga
 * would be someone else's. */
/* The script named on the command line ($0): run as the shell's own text. A name that is not there ends
 * the shell with status 127, as bash does (the file read by source would give 1). */
long sh_run_script(sh_shell *sh)
{
    const char *f = sh->ctx.arg0;
    sh_stat st;
    if (f && (!sh->os.stat || sh->os.stat(sh->os.data, f, &st, 0))) {
        sayl(sh, sh->io.err, "vsh: ", f, ": No such file or directory\n", NULL);
        sh->exiting = 1;
        sh->exit_status = 127;
        sh->ctx.status = 127;
        return 127;
    }
    return sh_run_text(sh, "source \"$0\"", 0);
}

void sh_startup_login(sh_shell *sh)
{
    int k = shopt_index("login_shell");
    if (k >= 0)
        sh->shopt_v[k] = 1;
    SH_HIT(LOGIN_PROFILE);
    sh_run_text(sh, "for f in ENV:vsh/profile \"$HOME/.vsh_profile\"; do if [ -r \"$f\" ]; then . \"$f\"; fi; done", 0);
}

static long b_logout(sh_shell *sh, int argc, char **argv, const sh_io *io)
{
    if (!shopt_get(sh, "login_shell")) {
        err2(sh, io, "logout", "not login shell: use `exit'");
        return 1;
    }
    return b_exit(sh, argc, argv, io);
}

long sh_run_text(sh_shell *sh, const char *text, int *incomplete)
{
    const char *s = text;
    long st = sh->ctx.status, line = 1;
    if (incomplete)
        *incomplete = 0;
    while (*s) {
        const char *e = s;
        char *chunk;
        sh_parse p;
        for (;;) {
            while (*e && *e != '\n')
                e++;
            if (*e)
                e++;
            chunk = (char *)malloc((size_t)(e - s) + 1);
            if (!chunk)
                return st;
            memcpy(chunk, s, (size_t)(e - s));
            chunk[e - s] = 0;
            sh_parse_text_at(&p, chunk, (int)line);
            if (p.incomplete && *e) {
                sh_parse_free(&p);
                free(chunk);
                continue;
            }
            break;
        }
        s = e;
        {
            const char *q;
            for (q = chunk; *q; q++)
                if (*q == '\n')
                    line++;
        }
        if ((sh->opts & SO_VERBOSE) && sh->io.err) {
            say(sh, sh->io.err, chunk);
            if (e[-1] != '\n')
                say(sh, sh->io.err, "\n");
        }
        free(chunk);
        if (incomplete)
            *incomplete = p.incomplete;
        if (p.error) {
            if (!p.incomplete)
                err2(sh, &sh->io, p.error, 0);
            sh_parse_free(&p);
            sh->ctx.status = p.incomplete ? sh->ctx.status : 2;
            return sh->ctx.status;
        }
        if (p.tree)
            st = exec_node(sh, p.tree, &sh->io);
        sh_parse_free(&p);
        if (sh->intr) {
            st = sh->ctx.status = intr_status(sh);
            sh->intr = 0;
        }
        if (sh->exiting || sh->returning || sh->breaking || sh->continuing)
            break;
    }
    return st;
}

/* $(cmd): cmd runs as a subshell writing into a pipe the shell reads
 * (without subshell processes: here, into a temporary file). */
static char *core_subst1(sh_ctx *c, const char *cmd)
{
    sh_shell *sh = (sh_shell *)c->user;
    pbuf out = { 0, 0, 0 };
    sh_parse p;
    char buf[512];
    long n;
    const sh_node *rn;
    SH_HIT(SUBST);
    sh_parse_text(&p, cmd);
    if (p.error) {
        err2(sh, &sh->io, p.error, 0);
        sh_parse_free(&p);
        sh->ctx.status = 2;
        return sdup("");
    }
    rn = p.tree;
    while (rn && rn->kind == SH_SEQ && !rn->b)
        rn = rn->a;
    if (rn && rn->kind == SH_CMD && !rn->words && !rn->assigns && rn->redirs && !rn->redirs->next &&
        rn->redirs->kind == SH_R_IN && rn->redirs->fd == 0) {
        /* $(< file): the file's contents, no command runs (bash) */
        char *path = expand_one(sh, rn->redirs->target, &sh->io);
        sh_fh fh = path ? sh->os.open(sh->os.data, path, SH_OPEN_READ) : SH_NOFH;
        if (path && !fh)
            err2(sh, &sh->io, path, "cannot open");
        if (fh) {
            if (sh->os.read) {
                while ((n = sh->os.read(sh->os.data, fh, buf, sizeof(buf))) > 0)
                    pb_add(&out, buf, n);
            } else {
                char line[512];
                while ((n = sh->os.read_line(sh->os.data, fh, line, sizeof(line))) >= 0)
                    pb_add(&out, line, n);
            }
            sh->os.close(sh->os.data, fh);
        }
        sh->ctx.status = fh ? 0 : 1;
        free(path);
        sh_parse_free(&p);
        return out.s ? out.s : sdup("");
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
            st = job_wait(sh, job);
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
        if (sh->os.remove)
            sh->os.remove(sh->os.data, path);
    }
    return out.s ? out.s : sdup("");
}

/* ---- temp files of here-documents and process substitution ------------------------ */

static void tmp_add(sh_shell *sh, const char *path, const char *cmd)
{
    if (sh->ntmp == sh->captmp) {
        int nc = sh->captmp ? sh->captmp * 2 : 8;
        sh_tmp *t = (sh_tmp *)realloc(sh->tmps, (size_t)nc * sizeof(sh_tmp));
        if (!t)
            return; /* the file stays: no memory to record it */
        sh->tmps = t;
        sh->captmp = nc;
    }
    sh->tmps[sh->ntmp].path = sdup(path);
    sh->tmps[sh->ntmp].cmd = cmd ? sdup(cmd) : 0;
    sh->ntmp++;
}

/* The command that used the temp files above entry mark has ended: each >( ) command runs on its file
 * (unless the shell is leaving), then the file is removed. $? is not changed. */
static void tmp_sweep(sh_shell *sh, int mark)
{
    long status = sh->ctx.status;
    while (sh->ntmp > mark) {
        sh_tmp t = sh->tmps[mark];
        memmove(sh->tmps + mark, sh->tmps + mark + 1, (size_t)(sh->ntmp - mark - 1) * sizeof(sh_tmp));
        sh->ntmp--;
        if (t.cmd && !sh->intr && !sh->exiting) {
            sh_parse p;
            sh_parse_text(&p, t.cmd);
            if (!p.error) {
                sh_io io = sh->io;
                sh_fh fh = sh->os.open(sh->os.data, t.path, SH_OPEN_READ);
                if (fh) {
                    io.in = fh;
                    io.owned = SH_OWN_IN;
                    subshell(sh, p.tree, &io, 1, 0);
                }
            }
            sh_parse_free(&p);
        }
        if (sh->os.remove && t.path)
            sh->os.remove(sh->os.data, t.path);
        free(t.path);
        free(t.cmd);
    }
    sh->ctx.status = status;
}

/* <(cmd): cmd runs to completion now with its output in a temp file, whose name is the word;
 * >(cmd): the word names an empty temp file, cmd runs on it when the command using the word ends. */
static char *core_procsub(sh_ctx *c, const char *cmd, int out)
{
    sh_shell *sh = (sh_shell *)c->user;
    char path[200], nb[24];
    sh_fh fh;
    SH_HIT(PROCSUB);
    strcpy(path, sh->os.tmpdir ? sh->os.tmpdir(sh->os.data) : "T:");
    strcat(path, "vsh-ps-");
    num(nb, sh->ctx.pid);
    strcat(path, nb);
    strcat(path, "-");
    num(nb, ++sh->heredocs);
    strcat(path, nb);
    fh = sh->os.open(sh->os.data, path, SH_OPEN_WRITE);
    if (!fh) {
        err2(sh, &sh->io, path, "cannot create");
        return 0;
    }
    tmp_add(sh, path, out ? cmd : 0);
    if (out) {
        sh->os.close(sh->os.data, fh);
    } else {
        sh_parse p;
        long status = sh->ctx.status;
        sh_io io = sh->io;
        sh_parse_text(&p, cmd);
        if (p.error) {
            err2(sh, &sh->io, p.error, 0);
            sh->os.close(sh->os.data, fh);
        } else {
            io.out = fh;
            io.owned = SH_OWN_OUT;
            subshell(sh, p.tree, &io, 1, 0);
        }
        sh_parse_free(&p);
        sh->ctx.status = status;
    }
    return sdup(path);
}

/* the glob's question about a path (sh_ctx.pathkind): a directory? a symbolic link? */
static int core_pathkind(sh_ctx *c, const char *path)
{
    sh_shell *sh = (sh_shell *)c->user;
    sh_stat st;
    int k = 0;
    if (!sh->os.stat)
        return 0;
    if (!sh->os.stat(sh->os.data, path, &st, 0) && st.type == SH_ST_DIR)
        k |= 1;
    if (!sh->os.stat(sh->os.data, path, &st, 1)) {
        k |= 4;
        if (st.link)
            k |= 2;
    }
    return k;
}

/* $( ) does not inherit set -e (bash), and its trace lines gain a PS4 character */
static char *core_subst(sh_ctx *c, const char *cmd)
{
    sh_shell *sh = (sh_shell *)c->user;
    unsigned long saved = sh->opts;
    char *r;
    if (sh->opts & SO_POSIX)
        SH_HIT(POSIX_SUBST);
    if (!shopt_get(sh, "inherit_errexit") && !(sh->opts & SO_POSIX)) /* posix mode: the substitution inherits set -e */
        sh->opts &= ~SO_ERREXIT;
    sh->xlevel++;
    r = core_subst1(c, cmd);
    sh->xlevel--;
    sh->opts = (sh->opts & ~SO_ERREXIT) | (saved & SO_ERREXIT);
    return r;
}

/* ---- the command line ---------------------------------------------------------- */

static void inv_msg(sh_shell *sh, const char *a, const char *b)
{
    sayl(sh, sh->io.err, "vsh: ", a, b, "\n", NULL);
}

/* the variables bash has when it starts: its version (5.2: the owner's decision), the system, the
 * shell's nesting depth, and the ids; the last few read-only */
static void start_vars(sh_shell *sh)
{
    static const char *const fixed[][2] = {
        { "BASH_VERSION", "5.2.0(1)-release" }, { "BASH", "bash" }, { "OSTYPE", "amigaos" },
        { "MACHTYPE", "m68k-commodore-amigaos" }, { "HOSTTYPE", "m68k" }
    };
    static const char *const info[] = { "5", "2", "0", "1", "release", "m68k-commodore-amigaos" };
    static const char *const ids[] = { "PPID", "UID", "EUID" };
    const char *old = sh_get(&sh->ctx, "SHLVL");
    char d[24];
    int i;
    for (i = 0; i < 5; i++)
        sh_set(&sh->ctx, fixed[i][0], fixed[i][1]);
    sh_ltoa((old ? atol(old) : 0) + 1, d);
    sh_set(&sh->ctx, "SHLVL", d);
    sh_attr_change(&sh->ctx, "SHLVL", SH_ATTR_EXPORT, 0);
    for (i = 0; i < 6; i++) {
        sh_ltoa(i, d);
        sh_assign(&sh->ctx, "BASH_VERSINFO", d, info[i], 0);
    }
    sh_attr_change(&sh->ctx, "BASH_VERSINFO", SH_ATTR_READONLY, 0);
    for (i = 0; i < 3; i++) {
        sh_ltoa(sh->os.sysid ? sh->os.sysid(sh->os.data, i) : 0, d);
        sh_set(&sh->ctx, ids[i], d);
        sh_attr_change(&sh->ctx, ids[i], SH_ATTR_READONLY, 0);
    }
}

void sh_invoke(sh_shell *sh, int argc, char **argv, int tty, sh_invoke_info *info)
{
    int i = 1, want_stdin = 0, interactive = 0, have_c = 0;
    unsigned long inv = 0;
    info->command = info->script = 0;
    if (sh_get(&sh->ctx, "POSIXLY_CORRECT")) {
        sh->opts |= SO_POSIX;  /* in the environment: posix mode from the start, the variable as it came */
        opts_apply(sh);
    }
    info->norc = info->login = info->exit_now = 0;
    info->status = 0;
    for (; i < argc; i++) {
        const char *a = argv[i], *p;
        int on;
        if (!strcmp(a, "--")) {
            i++;
            break;
        }
        if (!strcmp(a, "-")) {
            sh->opts &= ~(SO_XTRACE | SO_VERBOSE);
            i++;
            break;
        }
        if (a[0] == '-' && a[1] == '-') {
            if (!strcmp(a, "--login")) info->login = 1;
            else if (!strcmp(a, "--norc") || !strcmp(a, "--noprofile")) info->norc = 1;
            else if (!strcmp(a, "--posix")) opt_name(sh, "posix", 1);
            else if (!strcmp(a, "--noediting") || !strcmp(a, "--restricted")) ;
            else if (!strcmp(a, "--version")) {
                say(sh, sh->io.out, "vsh, a bash-compatible shell for AmigaDOS\n");
                info->exit_now = 1;
                return;
            } else if (!strcmp(a, "--help")) {
                say(sh, sh->io.out, "usage: vsh [-ceuxvfCanhils] [-o option] [+o option] [-c command [name]] [file] [argument ...]\n");
                info->exit_now = 1;
                return;
            } else if (!strcmp(a, "--rcfile") || !strcmp(a, "--init-file")) i++;
            else {
                inv_msg(sh, a, ": invalid option");
                info->exit_now = 1;
                info->status = 2;
                return;
            }
            continue;
        }
        if (a[0] != '-' && a[0] != '+')
            break;
        on = a[0] == '-';
        if (a[1] && a[2])
            SH_HIT(INVOKE_CLUSTER);
        for (p = a + 1; *p; p++) {
            if (*p == 'c' && on)
                have_c = 1;
            else if (*p == 's' && on)
                want_stdin = 1;
            else if (*p == 'i' && on)
                interactive = 1;
            else if (*p == 'l' && on)
                info->login = 1;
            else if (*p == 'o') {
                if (i + 1 >= argc) {
                    list_opts(sh, &sh->io, !on);
                } else if (!opt_name(sh, argv[++i], on)) {
                    inv_msg(sh, argv[i], ": invalid option name");
                    info->exit_now = 1;
                    info->status = 2;
                    return;
                }
            } else if (*p == 'r' || *p == 'D') {
                ;
            } else if (!opt_letter(sh, *p, on)) {
                char o[3];
                o[0] = a[0];
                o[1] = *p;
                o[2] = 0;
                inv_msg(sh, o, ": invalid option");
                info->exit_now = 1;
                info->status = 2;
                return;
            }
        }
    }
    if (have_c) {
        if (i >= argc) {
            inv_msg(sh, "-c", ": option requires an argument");
            info->exit_now = 1;
            info->status = 2;
            return;
        }
        info->command = argv[i++];
        inv |= SO_COMMAND;
        if (i < argc)
            sh->ctx.arg0 = argv[i++];
    } else if (i < argc && !want_stdin) {
        info->script = argv[i];
        sh->ctx.arg0 = argv[i++];
    } else {
        inv |= SO_STDIN;
        if (tty || interactive)
            interactive = 1;
    }
    if (interactive && !have_c && !info->script)
        inv |= SO_INTERACTIVE;
    if (interactive && (have_c || info->script) && (argc > 0))
        inv |= SO_INTERACTIVE;
    if (!(inv & SO_STDIN))
        inv &= ~SO_STDIN;
    sh->opts |= inv;
    opts_apply(sh);
    for (; i < argc; i++)
        sh_list_add(&sh->ctx.args, argv[i]);
    if (info->script)
        sh->main_src = info->script;
    sh->cur_src = sh->ctx.arg0;
    {
        long us;
        sh->secs0 = sh_now(sh, &us);
    }
    start_vars(sh);
    if (sh->os.cwd && !sh_get(&sh->ctx, "PWD")) {
        char *d = sh->os.cwd(sh->os.data);
        if (d && *d) {
            sh_set(&sh->ctx, "PWD", d);
            sh_attr_change(&sh->ctx, "PWD", SH_ATTR_EXPORT, 0);
        }
        free(d);
    }
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

static char *core_unescape(sh_ctx *c, const char *s)
{
    pbuf o = { 0, 0, 0 };
    const char *p;
    (void)c;
    for (p = s; *p; p++) {
        if (*p == '\\')
            pf_escape(&o, &p, 2);
        else
            pb_add(&o, p, 1);
    }
    return o.s ? o.s : sdup("");
}

/* zsh: also zsh's %-escapes (vsh's own prompts); ${x@P} is bash's: backslash escapes only */
static char *prompt_text(sh_shell *sh, const char *ps, int zsh)
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
        else if (zsh && *ps == '%' && ps[1])
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

char *sh_prompt(sh_shell *sh, const char *ps)
{
    return prompt_text(sh, ps, 1);
}

static char *core_prompt(sh_ctx *c, const char *ps)
{
    return prompt_text((sh_shell *)c->user, ps, 0);
}
