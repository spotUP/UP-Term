/* shells -- the background tasks (ledger A4 WP2, A4 gaps 2): Bash's
 * run_in_background commands, a foreground command moved there at its
 * time limit or by the user's Ctrl+B, and Monitor's watches, each a job of
 * sys.h (bg_start); the foreground Bash command (and a ! line) itself runs
 * as a job polled to its end, so its output can be cut head and tail and
 * it can move -- fg_wait, fg_move, one way for both triggers. TaskStop stops a task
 * (KillShell, the older name, too); Claude reads a task's output file with
 * Read (BashOutput, the older way, still answers). Ids are "bash_N" and
 * "monitor_N". What happens to the tasks while Claude is not looking --
 * one ended, a monitor printed lines, a deadline passed -- is collected by
 * shells_poll as notices for Claude (the REPL delivers them). The tasks
 * still running when C:Claude ends are stopped (tools_free). */
#include <stdlib.h>
#include <string.h>
#include "tools_int.h"
#include "regex.h"
#include "util.h"

#define SHELLS_MAX 16
#define SHELL_READ (30L * 1024)
#define SAVED_MAX 8
#define MON_LINES 50                /* monitor lines handed over at one look */

enum { SH_BASH, SH_MONITOR };

typedef struct shell {
    int id;                     /* 0: a free slot */
    int kind;                   /* SH_* */
    long job;
    char cmd[200];
    char desc[120];
    long pos;                   /* output bytes BashOutput already returned */
    long mpos;                  /* a monitor's output bytes already handed over as lines */
    int ended, killed, told;    /* told: Claude was told it ended */
    int moved;                  /* moved to the background (its time limit, Ctrl+B) */
    int owner;                  /* the subagent run that started it (cl_tools.run_id), 0 the conversation */
    long rc;
    unsigned long t0;           /* when it entered the background (t->clock) */
    long limit_ms;              /* its deadline from t0, 0 none */
} shell;

typedef struct cl_shells {
    shell s[SHELLS_MAX];
    int next;
    char saved[SAVED_MAX][300];     /* outputs too long to give inline, kept for Read */
    int nsaved;
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
    for (i = 0; i < sh->nsaved && sys && sys->remove; i++)
        sys->remove(sys->u, sh->saved[i]);
    free(sh);
}

static unsigned long now_ms(cl_tools *t)
{
    return t->clock ? t->clock(t->u) : 0;
}

static void id_text(const shell *s, char *out)
{
    char num[16];
    cl_ltoa(s->id, num);
    strcpy(out, s->kind == SH_MONITOR ? "monitor_" : "bash_");
    strcat(out, num);
}

static shell *find(cl_shells *sh, const char *name)
{
    int i, id, kind;
    if (!strncmp(name, "bash_", 5)) {
        kind = SH_BASH;
        id = atoi(name + 5);
    } else if (!strncmp(name, "monitor_", 8)) {
        kind = SH_MONITOR;
        id = atoi(name + 8);
    } else
        return 0;
    for (i = 0; i < SHELLS_MAX; i++)
        if (sh->s[i].id && sh->s[i].id == id && sh->s[i].kind == kind)
            return &sh->s[i];
    return 0;
}

/* a free slot (or the oldest ended one, its files dropped), 0 none */
static shell *slot(cl_tools *t)
{
    cl_shells *sh = t->sh;
    int i;
    for (i = 0; i < SHELLS_MAX; i++)
        if (!sh->s[i].id)
            return &sh->s[i];
    for (i = 0; i < SHELLS_MAX; i++)
        if (sh->s[i].ended && sh->s[i].told) {
            if (t->sys->bg_drop)
                t->sys->bg_drop(t->sys->u, sh->s[i].job);
            return &sh->s[i];
        }
    return 0;
}

/* the job's state looked at again: ended (with its code) or not */
static void look(cl_tools *t, shell *s)
{
    char one[1];
    long got = 0, rc = 0;
    int running = 1;
    if (s->ended || !t->sys->bg_read)
        return;
    if (t->sys->bg_read(t->sys->u, s->job, 0, one, 0, &got, &running, &rc) == 0 && !running) {
        s->ended = 1;
        s->rc = rc;
    }
}

static const char *file_of(cl_tools *t, const shell *s)
{
    return t->sys->bg_file ? t->sys->bg_file(t->sys->u, s->job) : "";
}

/* a task made from a started job: its slot filled, its id into name */
static shell *adopt(cl_tools *t, int kind, long job, const char *cmd, const char *desc, long limit_ms, char *name)
{
    shell *s = slot(t);
    if (!s)
        return 0;
    memset(s, 0, sizeof(*s));
    s->kind = kind;
    s->job = job;
    s->id = t->sh->next++;
    cl_copy(s->cmd, cmd, sizeof(s->cmd));
    cl_copy(s->desc, desc && *desc ? desc : cmd, sizeof(s->desc));
    s->t0 = now_ms(t);
    s->limit_ms = limit_ms;
    s->owner = t->run_id;
    id_text(s, name);
    return s;
}

void shells_start(cl_tools *t, jw *out, const char *id, const char *cmd, const char *desc, long limit_ms)
{
    shell *s;
    long job;
    char name[24];
    jw m;
    if (!t->sh || !t->sys->bg_start || t->no_background) {
        tl_error(t, out, id, "background tasks are not available here; run the command in the foreground", 0);
        return;
    }
    if (!slot(t)) {
        tl_error(t, out, id, "16 background tasks are running already; stop one with TaskStop first", 0);
        return;
    }
    if (t->sys->bg_start(t->sys->u, cmd, &job)) {
        tl_error(t, out, id, "the command did not start: ", t->sys->err(t->sys->u));
        return;
    }
    s = adopt(t, SH_BASH, job, cmd, desc, limit_ms, name);
    jw_init(&m);
    jw_rawz(&m, "Command running in background with ID: ");
    jw_rawz(&m, name);
    if (*file_of(t, s)) {
        jw_rawz(&m, ". Output is being written to: ");
        jw_rawz(&m, file_of(t, s));
        jw_rawz(&m, " (read it with Read; you are told when it ends)");
    }
    tl_result(t, out, id, m.p, m.n, 0);
    jw_free(&m);
}

/* ---- the foreground command, run as a job ---- */

/* n bytes of the job's output from from, appended to w */
static void put_range(cl_tools *t, long job, long from, long n, jw *w)
{
    char *b;
    long got = 0, rc = 0;
    int running = 0;
    if (n <= 0)
        return;
    b = (char *)malloc((size_t)n + 1);
    if (!b)
        return;
    if (t->sys->bg_read(t->sys->u, job, from, b, n, &got, &running, &rc) == 0)
        jw_raw(w, b, got);
    free(b);
}

/* is the command a pause (sleep, AmigaDOS Wait)? It stops at its time limit
 * instead of moving to the background (Claude Code's "unless it starts with
 * sleep") */
static int is_sleep(const char *cmd)
{
    while (*cmd == ' ' || *cmd == '\t')
        cmd++;
    return (!strncmp(cmd, "sleep", 5) || cl_strnieq(cmd, "wait", 4)) &&
           (cmd[cmd[0] == 's' || cmd[0] == 'S' ? 5 : 4] == ' ' || !cmd[cmd[0] == 's' || cmd[0] == 'S' ? 5 : 4]);
}

/* does the line change directory somewhere (cd, pushd, popd, chdir as a word)? */
static int has_cd(const char *cmd)
{
    static const char *const w[] = { "cd", "pushd", "popd", "chdir", 0 };
    const char *p = cmd;
    int i;
    for (; *p; p++) {
        if (p != cmd && p[-1] != ' ' && p[-1] != '\t' && p[-1] != ';' && p[-1] != '&' && p[-1] != '|' &&
            p[-1] != '\n' && p[-1] != '(')
            continue;
        for (i = 0; w[i]; i++) {
            long l = (long)strlen(w[i]);
            if (cl_strnieq(p, w[i], l) && (p[l] == ' ' || p[l] == '\t' || !p[l] || p[l] == ';' || p[l] == '\n'))
                return 1;
        }
    }
    return 0;
}

int shells_can_run(cl_tools *t)
{
    return t->sh && t->sys->bg_start && t->sys->bg_read && t->sys->bg_size && t->sys->pause && t->sys->bg_kill &&
           t->sys->bg_drop;
}

/* fg_wait's answers besides SYS_BREAK */
enum { FG_END, FG_LIMIT, FG_MOVE };

/* The foreground command waited on, a look every 100 ms: FG_END it ended,
 * FG_LIMIT its time is up, SYS_BREAK the user stopped it, FG_MOVE the user
 * moved it to the background (the screen's Ctrl+B / Ctrl+Enter, a slot
 * free, background tasks on). With t->wait (the screen) the keys are read
 * meanwhile (Esc, Ctrl+C stop it); without, sys->pause sees Ctrl+C. */
static int fg_wait(cl_tools *t, long job, int secs, long *rc)
{
    unsigned long t0 = now_ms(t), limit = (unsigned long)secs * 1000UL, waited = 0;
    int running = 1;
    for (;;) {
        char one[1];
        long got;
        if (t->sys->bg_read(t->sys->u, job, 0, one, 0, &got, &running, rc) || !running)
            return FG_END;
        if (t->wait) {
            int w = t->wait(t->u, 100);
            if (w == TW_STOP)
                return SYS_BREAK;
            if (w == TW_BACKGROUND && !t->no_background && slot(t))
                return FG_MOVE;
        } else if (t->sys->pause(t->sys->u, 100))
            return SYS_BREAK;
        waited += 100;
        if (t->clock && now_ms(t) - t0 > waited)
            waited = now_ms(t) - t0;    /* the machine's clock, when it runs ahead */
        if (waited >= limit)
            return FG_LIMIT;
    }
}

/* a break, and ten seconds to end: 0 it ended (*rc its code), 1 it runs on */
static int fg_break(cl_tools *t, long job, long *rc)
{
    int running = 1, k;
    t->sys->bg_kill(t->sys->u, job);
    for (k = 0; k < 100; k++) {
        char one[1];
        long got;
        if (t->sys->bg_read(t->sys->u, job, 0, one, 0, &got, &running, rc))
            break;
        if (!running)
            return 0;
        t->sys->pause(t->sys->u, 100);
    }
    return 1;
}

/* the job moved to the background -- at its time limit or by the user's
 * Ctrl+B, one way for both: a task (bash_N) Claude is told about when it
 * ends; its id into name */
static shell *fg_move(cl_tools *t, long job, const char *shown, const char *desc, char *name)
{
    shell *s = adopt(t, SH_BASH, job, shown, desc, t->bg_limit_ms, name);
    if (s)
        s->moved = 1;
    return s;
}

int tools_run_fg(cl_tools *t, const char *cmd, int secs, char *out, long cap, long *outn, long *rc, char *id,
                 long idcap)
{
    long job;
    int fg, broke = 0, running = 0;
    char name[24];
    *outn = 0;
    *rc = -1;
    if (id && idcap)
        id[0] = 0;
    if (!t->wait || !shells_can_run(t))
        return t->sys->run(t->sys->u, cmd, secs, out, cap, outn, rc);
    if (t->sys->bg_start(t->sys->u, cmd, &job))
        return -1;
    fg = fg_wait(t, job, secs, rc);
    if (fg == FG_MOVE && fg_move(t, job, cmd, "", name)) {
        if (id && idcap)
            cl_copy(id, name, idcap);
        return SHELL_MOVED;
    }
    if (fg != FG_END) {
        broke = fg == SYS_BREAK ? SYS_BREAK : SYS_TIMEOUT;
        if (fg_break(t, job, rc)) {
            /* it does not end after the break: kept as a task (so C:Claude's
             * end stops it), its output so far */
            if (adopt(t, SH_BASH, job, cmd, "", 0, name) && id && idcap)
                cl_copy(id, name, idcap);
            t->sys->bg_read(t->sys->u, job, 0, out, cap, outn, &running, rc);
            return broke;
        }
    }
    t->sys->bg_read(t->sys->u, job, 0, out, cap, outn, &running, rc);
    t->sys->bg_drop(t->sys->u, job);
    return broke;
}

int shells_run_fg(cl_tools *t, const char *cmd, const char *shown, const char *desc, int secs, jw *res,
                  long *rc_out, int *is_err)
{
    long job, total, rc = 0, inl, fail_max;
    int fg, broke = 0, r = 0;
    char num[16], name[24];
    *rc_out = 0;
    *is_err = 0;
    if (t->sys->bg_start(t->sys->u, cmd, &job))
        return -1;
    fg = fg_wait(t, job, secs, &rc);
    if (fg == FG_MOVE || (fg == FG_LIMIT && !t->no_background && !is_sleep(shown) && slot(t))) {
        /* Claude Code: at its time limit a command moves to the background
         * (a pause excepted); Ctrl+B moves it before */
        shell *s = fg_move(t, job, shown, desc, name);
        if (fg == FG_MOVE) {
            jw_rawz(res, "Command was manually backgrounded by user with ID: ");
            jw_rawz(res, name);
        } else {
            jw_rawz(res, "Command did not complete within its ");
            cl_ltoa(secs, num);
            jw_rawz(res, num);
            jw_rawz(res, "s timeout and was moved to the background with ID: ");
            jw_rawz(res, name);
        }
        if (*file_of(t, s)) {
            jw_rawz(res, ". Output is being written to: ");
            jw_rawz(res, file_of(t, s));
        }
        jw_rawz(res, ". You are told when it ends; stop it with TaskStop.");
        if (has_cd(shown)) {
            jw_rawz(res, "\nSession cwd remains ");
            jw_rawz(res, t->cwd[0] ? t->cwd : t->root);
            jw_rawz(res, "; directory changes made by the backgrounded command do not apply to subsequent "
                         "commands.");
        }
        return SHELL_MOVED;
    }
    if (fg != FG_END) {
        /* stopped: by the user, or at its time limit (a pause, background
         * tasks off, no slot) */
        broke = fg == SYS_BREAK ? SYS_BREAK : SYS_TIMEOUT;
        if (fg_break(t, job, &rc)) {
            /* it does not end: left running, as a task */
            name[0] = 0;
            if (slot(t))
                adopt(t, SH_BASH, job, shown, desc, 0, name);
            jw_rawz(res, "The command did not end after a break; it was left running in the background");
            if (name[0]) {
                jw_rawz(res, " as ");
                jw_rawz(res, name);
            }
            jw_rawz(res, ".\n");
            *is_err = 1;
            return broke;
        }
    }
    r = broke;
    if (broke == SYS_TIMEOUT) {
        jw_rawz(res, "The command ran out of time (");
        cl_ltoa(secs, num);
        jw_rawz(res, num);
        jw_rawz(res, " s) and was sent a break (Ctrl+C).\n");
    } else if (broke == SYS_BREAK)
        jw_rawz(res, "The user stopped the command (Ctrl+C).\n");
    *rc_out = rc;
    *is_err = r != 0 || rc >= 10;
    jw_rawz(res, "Return code ");
    cl_ltoa(rc, num);
    jw_rawz(res, num);
    jw_rawz(res, ".\n");
    /* Claude Code's output limits: a valid result inline up to the
     * ceiling, past it a preview and the kept file; a failure up to 10000
     * characters, past it the head and the tail */
    total = t->sys->bg_size(t->sys->u, job);
    inl = t->out_inline > 0 ? t->out_inline : TL_OUT_MAX;
    fail_max = 10000L;
    if (fail_max > inl)
        fail_max = inl;
    if (*is_err ? total <= fail_max : total <= inl)
        put_range(t, job, 0, total, res);
    else if (*is_err) {
        long half = fail_max / 2;
        put_range(t, job, 0, half, res);
        jw_rawz(res, "\n\n... [");
        cl_ltoa(total - 2 * half, num);
        jw_rawz(res, num);
        jw_rawz(res, " characters cut from the middle] ...\n\n");
        put_range(t, job, total - half, half, res);
    } else {
        char keep[300];
        const char *f = t->sys->bg_file ? t->sys->bg_file(t->sys->u, job) : "";
        cl_shells *sh = t->sh;
        cl_copy(keep, f, sizeof(keep) - 8);
        cl_cat(keep, ".out", sizeof(keep));
        if (f[0] && t->sys->rename && sh->nsaved < SAVED_MAX && t->sys->rename(t->sys->u, f, keep) == 0) {
            cl_copy(sh->saved[sh->nsaved++], keep, sizeof(sh->saved[0]));
            jw_rawz(res, "Output too large (");
            cl_ltoa(total, num);
            jw_rawz(res, num);
            jw_rawz(res, " characters). Full output saved to: ");
            jw_rawz(res, keep);
            jw_rawz(res, "\nRead or Grep it for the rest. Preview (the first 2000 characters):\n");
            /* the file moved: the preview from its new name */
            {
                char *b = 0;
                long bn = 0;
                if (t->sys->read(t->sys->u, keep, 64L * 1024 * 1024, &b, &bn) == 0) {
                    jw_raw(res, b, bn < 2000 ? bn : 2000);
                    free(b);
                }
            }
        } else {
            put_range(t, job, 0, inl, res);
            jw_rawz(res, "\n(output cut at ");
            cl_ltoa(inl, num);
            jw_rawz(res, num);
            jw_rawz(res, " characters)");
        }
    }
    t->sys->bg_drop(t->sys->u, job);
    return r;
}

/* ---- BashOutput (the older tool), TaskStop / KillShell ---- */

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
        s->told = 1;            /* Claude has seen it end */
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

/* TaskStop {task_id | shell_id} and KillShell {shell_id}: the task gets a break */
void shells_kill(cl_tools *t, jw *out, const char *id, jv in)
{
    char *name = tl_prop(in, "task_id", 0), *alt = tl_prop(in, "shell_id", 0);
    shell *s;
    jw m;
    int stop = t->cur == T_TASK_STOP;
    if (!name || !alt) {
        free(name);
        free(alt);
        tl_error(t, out, id, "out of memory", 0);
        return;
    }
    if (!*name) {
        free(name);
        name = alt;             /* shell_id, TaskStop's deprecated key and KillShell's */
        alt = 0;
    }
    if (t->show)
        t->show(t->u, stop ? "TaskStop" : "KillShell", name);
    if (perm_refused(&t->perm, stop ? T_TASK_STOP : T_KILL_SHELL)) {
        tl_gate(t, out, id, stop ? T_TASK_STOP : T_KILL_SHELL, name, 0, 0);
        goto done;
    }
    s = t->sh ? find(t->sh, name) : 0;
    if (!s) {
        jw_init(&m);
        jw_rawz(&m, "No task found with ID: ");
        jw_rawz(&m, name);
        {
            char list[600];
            tools_shells(t, list, sizeof(list));
            if (list[0]) {
                jw_rawz(&m, ". The tasks:\n");
                jw_rawz(&m, list);
            }
        }
        tl_error(t, out, id, m.p ? m.p : "No task found", 0);
        jw_free(&m);
        goto done;
    }
    look(t, s);
    if (s->ended) {
        tl_error(t, out, id, "the task has already ended: ", name);
        goto done;
    }
    if (!t->sys->bg_kill || t->sys->bg_kill(t->sys->u, s->job)) {
        tl_error(t, out, id, "cannot stop the task: ", t->sys->err(t->sys->u));
        goto done;
    }
    s->killed = 1;
    s->told = 1;                /* Claude stopped it: no notice of its end */
    jw_init(&m);
    if (stop) {
        jw_rawz(&m, "{\"message\":");
        {
            jw x;
            jw_init(&x);
            jw_rawz(&x, "Successfully stopped task: ");
            jw_rawz(&x, name);
            jw_rawz(&x, " (");
            jw_rawz(&x, s->desc);
            jw_rawz(&x, ")");
            jw_str(&m, x.p ? x.p : "", x.n);
            jw_free(&x);
        }
        jw_rawz(&m, ",\"task_id\":");
        jw_strz(&m, name);
        jw_rawz(&m, ",\"task_type\":");
        jw_strz(&m, s->kind == SH_MONITOR ? "monitor" : "shell");
        jw_rawz(&m, ",\"command\":");
        jw_strz(&m, s->cmd);
        jw_raw(&m, "}", 1);
    } else {
        jw_rawz(&m, "Successfully killed shell: ");
        jw_rawz(&m, name);
        jw_rawz(&m, " (");
        jw_rawz(&m, s->cmd);
        jw_rawz(&m, ")");
    }
    tl_result(t, out, id, m.p, m.n, 0);
    jw_free(&m);
done:
    free(name);
    free(alt);
}

/* ---- Monitor ---- */

void monitor_start(cl_tools *t, jw *out, const char *id, jv in)
{
    char *cmd = tl_prop(in, "command", 0), *desc = tl_prop(in, "description", 0), name[24], num[16];
    long ms = tl_num(in, "timeout_ms", 300000L), max = t->bg_limit_ms ? 600000L : 1800000L, job;
    jv ws;
    shell *s;
    jw m;
    if (!cmd || !desc) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    if (json_get(in, "ws", &ws)) {
        tl_error(t, out, id, "a WebSocket watch is not available on the Amiga (no WebSocket client); watch a "
                             "command's output instead", 0);
        goto done;
    }
    if (!*cmd) {
        tl_error(t, out, id, "Monitor needs a command (one event per line it prints)", 0);
        goto done;
    }
    /* Claude Code: 5 minutes by default, at most 30 (10 in print mode) */
    if (ms <= 0)
        ms = 300000L;
    if (ms > max)
        ms = max;
    if (!t->sh || !t->sys->bg_start || t->no_background) {
        tl_error(t, out, id, "background tasks are not available here", 0);
        goto done;
    }
    if (!slot(t)) {
        tl_error(t, out, id, "16 background tasks are running already; stop one with TaskStop first", 0);
        goto done;
    }
    if (t->sys->bg_start(t->sys->u, cmd, &job)) {
        tl_error(t, out, id, "the command did not start: ", t->sys->err(t->sys->u));
        goto done;
    }
    s = adopt(t, SH_MONITOR, job, cmd, desc, ms, name);
    (void)s;
    jw_init(&m);
    jw_rawz(&m, "{\"taskId\":");
    jw_strz(&m, name);
    jw_rawz(&m, ",\"timeoutMs\":");
    cl_ltoa(ms, num);
    jw_rawz(&m, num);
    jw_rawz(&m, "}\nThe watch runs in the background: each line its command prints comes to you as an event "
                "between turns. Stop it early with TaskStop.");
    tl_result(t, out, id, m.p, m.n, 0);
    jw_free(&m);
done:
    free(cmd);
    free(desc);
}

/* ---- what Claude is told, the lists ---- */

/* the same file? (path is canonical; f as the job named it, T:...) */
static int same_file(cl_tools *t, const char *f, const char *path)
{
    char c[300];
    if (!*f)
        return 0;
    if (cl_strieq(f, path))
        return 1;
    return t->sys->canon(t->sys->u, f, c, sizeof(c)) == 0 && cl_strieq(c, path);
}

int shells_owns(cl_tools *t, const char *path)
{
    cl_shells *sh = t->sh;
    int i;
    if (!sh)
        return 0;
    for (i = 0; i < SHELLS_MAX; i++)
        if (sh->s[i].id && same_file(t, file_of(t, &sh->s[i]), path))
            return 1;
    for (i = 0; i < sh->nsaved; i++)
        if (same_file(t, sh->saved[i], path))
            return 1;
    return 0;
}

static void notice(jw *w, const char *a, const char *b, const char *c)
{
    if (w->n)
        jw_raw(w, "\n", 1);
    jw_rawz(w, a);
    if (b)
        jw_rawz(w, b);
    if (c)
        jw_rawz(w, c);
}

int shells_poll(cl_tools *t, jw *w)
{
    cl_shells *sh = t->sh;
    int i, got = 0;
    char name[24], num[16];
    unsigned long now = now_ms(t);
    if (!sh || !t->sys->bg_read)
        return 0;
    for (i = 0; i < SHELLS_MAX; i++) {
        shell *s = &sh->s[i];
        if (!s->id || s->told)
            continue;
        id_text(s, name);
        look(t, s);
        if (s->kind == SH_MONITOR) {
            /* the new whole lines, each an event */
            char *b = (char *)malloc(8192 + 1);
            long n = 0, rc = 0, k, at = 0;
            int running = 0, lines = 0;
            if (b && t->sys->bg_read(t->sys->u, s->job, s->mpos, b, 8192, &n, &running, &rc) == 0) {
                if (!s->ended || running)
                    while (n > 0 && b[n - 1] != '\n')
                        n--;    /* a line still being written waits */
                for (k = 0; k < n && lines < MON_LINES; k++)
                    if (b[k] == '\n') {
                        long e = k;
                        if (e > at && b[e - 1] == '\r')
                            e--;
                        if (e > at) {
                            jw line;
                            jw_init(&line);
                            jw_rawz(&line, "Monitor ");
                            jw_rawz(&line, name);
                            jw_rawz(&line, " (");
                            jw_rawz(&line, s->desc);
                            jw_rawz(&line, "): ");
                            jw_raw(&line, b + at, e - at);
                            if (!line.oom)
                                notice(w, line.p, 0, 0);
                            jw_free(&line);
                            got++;
                            lines++;
                        }
                        at = k + 1;
                    }
                if (s->ended && n > at && lines < MON_LINES) {
                    jw line;
                    jw_init(&line);
                    jw_rawz(&line, "Monitor ");
                    jw_rawz(&line, name);
                    jw_rawz(&line, ": ");
                    jw_raw(&line, b + at, n - at);
                    notice(w, line.p ? line.p : "", 0, 0);
                    jw_free(&line);
                    at = n;
                    got++;
                }
                s->mpos += at;
            }
            free(b);
            if (s->ended) {
                cl_ltoa(s->rc, num);
                notice(w, "Monitor ", name, " ended: its command finished");
                jw_rawz(w, " (exit code ");
                jw_rawz(w, num);
                jw_rawz(w, ").");
                s->told = 1;
                got++;
            } else if (s->limit_ms && now - s->t0 >= (unsigned long)s->limit_ms) {
                t->sys->bg_kill(t->sys->u, s->job);
                s->killed = 1;
                s->told = 1;
                notice(w, "Monitor ", name, " (");
                jw_rawz(w, s->desc);
                jw_rawz(w, ") reached its deadline and was stopped; start it again if it is still needed.");
                got++;
            }
            continue;
        }
        if (s->ended) {
            cl_ltoa(s->rc, num);
            notice(w, "Background command ", name, " (\"");
            jw_rawz(w, s->desc);
            jw_rawz(w, s->killed ? "\") was stopped" : "\") completed");
            jw_rawz(w, " with exit code ");
            jw_rawz(w, num);
            if (*file_of(t, s)) {
                jw_rawz(w, ". Its output: ");
                jw_rawz(w, file_of(t, s));
            }
            jw_rawz(w, ".");
            s->told = 1;
            got++;
        } else if (s->limit_ms && now - s->t0 >= (unsigned long)s->limit_ms) {
            /* an unattended run's background time limit */
            t->sys->bg_kill(t->sys->u, s->job);
            s->killed = 1;
            s->told = 1;
            notice(w, "Background command \"", s->desc, "\" was stopped after reaching its background time limit.");
            got++;
        }
    }
    return got;
}

void shells_end_owner(cl_tools *t, int owner)
{
    cl_shells *sh = t->sh;
    int i;
    if (!sh || !owner)
        return;
    for (i = 0; i < SHELLS_MAX; i++) {
        shell *s = &sh->s[i];
        if (!s->id || s->owner != owner)
            continue;
        look(t, s);
        if (!s->ended && t->sys->bg_kill) {
            t->sys->bg_kill(t->sys->u, s->job);
            s->killed = 1;
        }
        s->told = 1;            /* the subagent's own: its end is not news for the conversation */
    }
}

int shells_busy(cl_tools *t)
{
    cl_shells *sh = t->sh;
    int i, k = 0;
    if (!sh)
        return 0;
    for (i = 0; i < SHELLS_MAX; i++)
        if (sh->s[i].id && !sh->s[i].told) {
            look(t, &sh->s[i]);
            k += !sh->s[i].ended || sh->s[i].kind == SH_MONITOR;
        }
    return k;
}

void shells_json(cl_tools *t, jw *w)
{
    cl_shells *sh = t->sh;
    int i, first = 1;
    jw_raw(w, "[", 1);
    for (i = 0; sh && i < SHELLS_MAX; i++) {
        shell *s = &sh->s[i];
        char name[24];
        if (!s->id)
            continue;
        look(t, s);
        if (s->ended)
            continue;               /* in flight only */
        id_text(s, name);
        if (!first)
            jw_raw(w, ",", 1);
        first = 0;
        jw_rawz(w, "{\"id\":");
        jw_strz(w, name);
        jw_rawz(w, ",\"type\":");
        jw_strz(w, s->kind == SH_MONITOR ? "monitor" : "shell");
        jw_rawz(w, ",\"status\":\"running\",\"description\":");
        jw_strz(w, s->desc);
        if (s->kind == SH_BASH) {
            jw_rawz(w, ",\"command\":");
            jw_strz(w, s->cmd);
        }
        jw_raw(w, "}", 1);
    }
    jw_raw(w, "]", 1);
}

/* /tasks: every task of this session, oldest first, with its state as it
 * is now */
void tools_shells(cl_tools *t, char *out, long cap)
{
    cl_shells *sh = t->sh;
    int i, k, order[SHELLS_MAX], n = 0;
    if (cap <= 0)
        return;
    out[0] = 0;
    if (!sh)
        return;
    for (i = 0; i < SHELLS_MAX; i++)
        if (sh->s[i].id) {
            for (k = n; k > 0 && sh->s[order[k - 1]].id > sh->s[i].id; k--)
                order[k] = order[k - 1];
            order[k] = i;
            n++;
        }
    for (k = 0; k < n; k++) {
        shell *s = &sh->s[order[k]];
        char name[24], num[16];
        look(t, s);
        id_text(s, name);
        if (out[0])
            cl_cat(out, "\n", cap);
        cl_cat(out, "  ", cap);
        cl_cat(out, name, cap);
        if (!s->ended)
            cl_cat(out, "  running  ", cap);
        else if (s->killed)
            cl_cat(out, "  stopped  ", cap);
        else {
            cl_cat(out, "  completed (exit ", cap);
            cl_ltoa(s->rc, num);
            cl_cat(out, num, cap);
            cl_cat(out, ")  ", cap);
        }
        cl_cat(out, s->kind == SH_MONITOR ? s->desc : s->cmd, cap);
    }
}
