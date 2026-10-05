/* shells -- Bash's background shells (ledger A4 WP2): run_in_background
 * starts a command through sys.h's bg_start, BashOutput reads what it
 * wrote since the last look (and whether it still runs), KillShell sends
 * it a break. Ids are Claude Code's ("bash_1", ...). The shells still
 * running when C:Claude ends are stopped (tools_free). */
#include <stdlib.h>
#include <string.h>
#include "tools_int.h"
#include "regex.h"
#include "util.h"

#define SHELLS_MAX 16
#define SHELL_READ (30L * 1024)

typedef struct shell {
    int id;                     /* 0: a free slot */
    long job;
    char cmd[200];
    long pos;                   /* output bytes already returned */
    int ended, killed;
    long rc;
} shell;

typedef struct cl_shells {
    shell s[SHELLS_MAX];
    int next;
} cl_shells;

cl_shells *shells_new(void)
{
    cl_shells *sh = (cl_shells *)calloc(1, sizeof(cl_shells));
    if (sh)
        sh->next = 1;
    return sh;
}

void shells_free(cl_sys *sys, cl_shells *sh)
{
    int i;
    if (!sh)
        return;
    for (i = 0; i < SHELLS_MAX; i++)
        if (sh->s[i].id && sys) {
            if (!sh->s[i].ended && sys->bg_kill)
                sys->bg_kill(sys->u, sh->s[i].job);
            if (sys->bg_drop)
                sys->bg_drop(sys->u, sh->s[i].job);
        }
    free(sh);
}

static shell *find(cl_shells *sh, const char *name)
{
    int i, id;
    if (strncmp(name, "bash_", 5))
        return 0;
    id = atoi(name + 5);
    for (i = 0; i < SHELLS_MAX; i++)
        if (sh->s[i].id && sh->s[i].id == id)
            return &sh->s[i];
    return 0;
}

static void id_text(const shell *s, char *out)
{
    char num[16];
    cl_ltoa(s->id, num);
    strcpy(out, "bash_");
    strcat(out, num);
}

/* a free slot (an ended shell's when none is free), 0 none */
static shell *grab(cl_tools *t)
{
    cl_shells *sh = t->sh;
    int i;
    for (i = 0; i < SHELLS_MAX; i++)
        if (!sh->s[i].id)
            return &sh->s[i];
    /* reuse the oldest ended one */
    for (i = 0; i < SHELLS_MAX; i++)
        if (sh->s[i].ended) {
            if (t->sys->bg_drop)
                t->sys->bg_drop(t->sys->u, sh->s[i].job);
            return &sh->s[i];
        }
    return 0;
}

void shells_start(cl_tools *t, jw *out, const char *id, const char *cmd)
{
    cl_shells *sh = t->sh;
    shell *s;
    char name[24];
    jw m;
    if (!sh || !t->sys->bg_start) {
        tl_error(t, out, id, "background shells are not available here; run the command in the foreground", 0);
        return;
    }
    s = grab(t);
    if (!s) {
        tl_error(t, out, id, "16 background shells are running already; stop one with KillShell first", 0);
        return;
    }
    memset(s, 0, sizeof(*s));
    if (t->sys->bg_start(t->sys->u, cmd, &s->job)) {
        tl_error(t, out, id, "the command did not start: ", t->sys->err(t->sys->u));
        return;
    }
    s->id = sh->next++;
    cl_copy(s->cmd, cmd, sizeof(s->cmd));
    id_text(s, name);
    jw_init(&m);
    jw_rawz(&m, "Command running in background with ID: ");
    jw_rawz(&m, name);
    tl_result(t, out, id, m.p, m.n, 0);
    jw_free(&m);
}

int tools_run_fg(cl_tools *t, const char *cmd, int secs, char *out, long cap, long *outn, long *rc, char *id,
                 long idcap)
{
    cl_sys *sys = t->sys;
    shell *s;
    long waited = 0, limit = (long)secs * 1000L, n = 0;
    int running = 1, broke = 0;
    *outn = 0;
    *rc = -1;
    if (id && idcap)
        id[0] = 0;
    if (!t->wait || !t->sh || !sys->bg_start || !sys->bg_read || !sys->bg_kill || !(s = grab(t)))
        return sys->run(sys->u, cmd, secs, out, cap, outn, rc);
    memset(s, 0, sizeof(*s));
    if (sys->bg_start(sys->u, cmd, &s->job))
        return -1;
    s->id = -1;                     /* taken; no id until it moves to the background */
    cl_copy(s->cmd, cmd, sizeof(s->cmd));
    for (;;) {
        int w;
        if (sys->bg_read(sys->u, s->job, 0, out, 0, &n, &running, rc) || !running)
            break;
        if (broke) {
            if (waited >= limit)
                break;              /* it does not end after a break: left running */
        } else if (waited >= limit) {
            sys->bg_kill(sys->u, s->job);
            broke = SYS_TIMEOUT;
            limit = waited + 10000;
        }
        w = t->wait(t->u, 100);
        waited += 100;
        if (w == TW_STOP && !broke) {
            sys->bg_kill(sys->u, s->job);
            broke = SYS_BREAK;
            limit = waited + 10000;
        } else if (w == TW_BACKGROUND && !broke) {
            /* Ctrl+B: it runs on as a background shell */
            s->id = t->sh->next++;
            if (id && idcap > 16)
                id_text(s, id);
            return SHELL_MOVED;
        }
    }
    if (running) {
        /* it does not end after the break: kept as a background shell (so
         * C:Claude's end stops it), its output so far */
        s->id = t->sh->next++;
        if (id && idcap > 16)
            id_text(s, id);
        sys->bg_read(sys->u, s->job, 0, out, cap, outn, &running, rc);
        return broke;
    }
    sys->bg_read(sys->u, s->job, 0, out, cap, outn, &running, rc);
    if (sys->bg_drop)
        sys->bg_drop(sys->u, s->job);
    memset(s, 0, sizeof(*s));
    return broke;
}

/* the lines of s[0..n) that match re, appended to w */
static void filtered(jw *w, const char *s, long n, const cl_re *re)
{
    long i = 0;
    while (i < n) {
        long e = i, ms, me;
        while (e < n && s[e] != '\n')
            e++;
        if (re_search(re, s + i, e - i, 0, &ms, &me) == 1) {
            jw_raw(w, s + i, e - i);
            jw_raw(w, "\n", 1);
        }
        i = e + 1;
    }
}

void shells_output(cl_tools *t, jw *out, const char *id, jv in)
{
    char *name = tl_prop(in, "bash_id", 0), *flt = tl_prop(in, "filter", 0), *buf = 0, err[200], num[16];
    shell *s;
    long n = 0, rc = 0;
    int running = 0;
    cl_re *re = 0;
    jw m;
    if (!name || !flt) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    if (t->show)
        t->show(t->u, "BashOutput", name);
    s = t->sh ? find(t->sh, name) : 0;
    if (!s || !t->sys->bg_read) {
        tl_error(t, out, id, "No shell found with ID: ", name);
        goto done;
    }
    if (*flt && !(re = re_compile(flt, 0, err, sizeof(err)))) {
        tl_error(t, out, id, "the filter is not a usable regular expression: ", err);
        goto done;
    }
    buf = (char *)malloc(SHELL_READ + 1);
    if (!buf || t->sys->bg_read(t->sys->u, s->job, s->pos, buf, SHELL_READ, &n, &running, &rc)) {
        tl_error(t, out, id, buf ? "cannot read the shell's output: " : "out of memory",
                 buf ? t->sys->err(t->sys->u) : 0);
        goto done;
    }
    if (n > 0 && running) {
        /* while it runs, hand out whole lines only */
        long k = n;
        while (k > 0 && buf[k - 1] != '\n')
            k--;
        if (k > 0)
            n = k;
    }
    s->pos += n;
    if (!running) {
        s->ended = 1;
        s->rc = rc;
    }
    jw_init(&m);
    jw_rawz(&m, "<status>");
    jw_rawz(&m, running ? "running" : s->killed ? "killed" : "completed");
    jw_rawz(&m, "</status>\n\n");
    if (!running) {
        jw_rawz(&m, "<exit_code>");
        cl_ltoa(rc, num);
        jw_rawz(&m, num);
        jw_rawz(&m, "</exit_code>\n\n");
    }
    if (n) {
        jw_rawz(&m, "<stdout>\n");
        if (re)
            filtered(&m, buf, n, re);
        else
            jw_raw(&m, buf, n);
        if (m.n && m.p[m.n - 1] != '\n')
            jw_raw(&m, "\n", 1);
        jw_rawz(&m, "</stdout>\n");
        if (n >= SHELL_READ)
            jw_rawz(&m, "\n(more output waits: call BashOutput again)\n");
    } else
        jw_rawz(&m, "(no new output)\n");
    tl_result(t, out, id, m.p, m.n, 0);
    jw_free(&m);
done:
    re_free(re);
    free(buf);
    free(name);
    free(flt);
}

void shells_kill(cl_tools *t, jw *out, const char *id, jv in)
{
    char *name = tl_prop(in, "shell_id", 0);
    shell *s;
    jw m;
    if (!name) {
        tl_error(t, out, id, "out of memory", 0);
        return;
    }
    if (t->show)
        t->show(t->u, "KillShell", name);
    if (perm_refused(&t->perm, T_KILL_SHELL)) {
        tl_gate(t, out, id, T_KILL_SHELL, name, 0, 0);
        free(name);
        return;
    }
    s = t->sh ? find(t->sh, name) : 0;
    if (!s) {
        tl_error(t, out, id, "No shell found with ID: ", name);
        free(name);
        return;
    }
    if (s->ended) {
        tl_error(t, out, id, "the shell has already ended: ", name);
        free(name);
        return;
    }
    if (!t->sys->bg_kill || t->sys->bg_kill(t->sys->u, s->job)) {
        tl_error(t, out, id, "cannot stop the shell: ", t->sys->err(t->sys->u));
        free(name);
        return;
    }
    s->killed = 1;
    jw_init(&m);
    jw_rawz(&m, "Successfully killed shell: ");
    jw_rawz(&m, name);
    jw_rawz(&m, " (");
    jw_rawz(&m, s->cmd);
    jw_rawz(&m, ")");
    tl_result(t, out, id, m.p, m.n, 0);
    jw_free(&m);
    free(name);
}
