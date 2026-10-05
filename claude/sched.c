/* sched -- what happens in a session while nobody types (A4 gaps 2): the
 * background tasks' news (shells_poll: a command ended, a monitor printed
 * lines, a deadline passed), the async hooks' answers, the watched files'
 * changes (FileChanged), and the cron jobs coming due.
 *
 * News reaches Claude two ways, as Claude Code's task notifications do:
 * during a turn, after a tool round, beside the round's results
 * (sched_collect); between turns, as a turn of its own whose message says
 * no human wrote it (sched_wake). A due cron job's prompt is a turn of its
 * own too, between turns only: the screen's idle tick hands it over
 * (tui.h wake), the line mode looks before it waits for a line; print mode
 * fires none. The jobs live in the tools (tasks.c); here they are kept
 * with the session (<session>.cron beside its JSONL file, restored on a
 * resume) and the durable ones in <root>/.claude/scheduled_tasks.json.
 *
 * /loop (A4 gaps 3): a self-paced loop's wakeup is a job of the same table
 * (tasks.c ScheduleWakeup); it fires here as "Claude resuming /loop
 * wakeup". A bare /loop's sentinel prompts become the default prompt when
 * they fire (loop.md read again: edits count from the next iteration). At
 * each turn's end sched_loop_end gives an iteration that set no wakeup one
 * fallback, ends the loop after a second, and on the screen folds quiet
 * iterations in a row into one line. Esc on the idle screen cancels the
 * pending wakeup (sched_esc_idle).
 * Portable C89, host-tested through the REPL suite. */
#include <stdlib.h>
#include <string.h>
#include "repl_int.h"
#include "tasks.h"
#include "tools_int.h"
#include "path.h"
#include "util.h"

#define WAKE_AFTER_MS 1000UL        /* the screen waits this long after a turn before one of its own */
#define LOOP_MD_MAX 25000L          /* Claude Code: loop.md beyond this is cut */
#define LOOP_MD_READ 1048576L       /* read whole up to this (no partial reads here); a longer one is skipped */

static long now_s(cl_repl *r)
{
    return r->sys->now ? r->sys->now(r->sys->u) : -1;
}

int sched_collect(cl_repl *r, jw *w)
{
    int n = 0;
    if (r->tools.sh)
        n += shells_poll(&r->tools, w);
    n += pol_async_collect(r, w);
    n += pol_files_changed(r, w);
    return n;
}

/* the notice that frames news nobody typed */
static void framed(jw *out, const char *news, long n)
{
    jw_rawz(out, "<task-notification>\nNo human input has occurred: this message reports background events of "
                 "the session, not a request from the user.\n");
    jw_raw(out, news, n);
    jw_rawz(out, "\n</task-notification>");
}

int sched_news(cl_repl *r, jw *extra)
{
    jw w;
    int n;
    jw_init(&w);
    n = sched_collect(r, &w);
    if (n && w.n) {
        if (extra->n)
            jw_rawz(extra, "\n\n");
        framed(extra, w.p, w.n);
    }
    jw_free(&w);
    return n;
}

/* the cron file kept with the session: <dir>/<id>.cron */
static int cron_file(cl_repl *r, char *out, long cap)
{
    char name[64];
    cl_copy(name, r->sess.id, 9);
    cl_cat(name, ".cron", sizeof(name));
    return r->sess.dir[0] ? path_join(r->sess.dir, name, out, cap) : -1;
}

static int durable_file(cl_repl *r, char *out, long cap)
{
    return path_join(r->tools.root, ".claude/scheduled_tasks.json", out, cap);
}

void sched_save(cl_repl *r)
{
    char f[400];
    jw w;
    if (!r->tools.tasks || !tasks_changed(r->tools.tasks))
        return;
    jw_init(&w);
    tasks_crons_json(r->tools.tasks, &w, 0, 0);
    if (!w.oom && !r->sess.off && cron_file(r, f, sizeof(f)) == 0) {
        if (r->sys->mkdir)
            r->sys->mkdir(r->sys->u, r->sess.dir);
        r->sys->write(r->sys->u, f, w.p, w.n);
    }
    jw_reset(&w);
    tasks_crons_json(r->tools.tasks, &w, 1, 0);
    if (!w.oom && durable_file(r, f, sizeof(f)) == 0) {
        if (w.n > 2 || r->sys->kind(r->sys->u, f) == 1) {
            char d[400];
            if (path_join(r->tools.root, ".claude", d, sizeof(d)) == 0 && r->sys->mkdir)
                r->sys->mkdir(r->sys->u, d);
            r->sys->write(r->sys->u, f, w.p, w.n);
        }
    }
    jw_free(&w);
}

/* jobs from a file added (only_durable: the durable ones) */
static void load_from(cl_repl *r, const char *f, int only_durable)
{
    char *b = 0;
    long n = 0, now = now_s(r);
    if (now < 0 || r->sys->kind(r->sys->u, f) != 1 || r->sys->read(r->sys->u, f, 64L * 1024, &b, &n))
        return;
    tasks_crons_load(r->tools.tasks, b, n, now, only_durable);
    free(b);
}

void sched_load(cl_repl *r, int resumed)
{
    char f[400];
    if (!r->tools.tasks || r->tools.no_cron)
        return;
    if (resumed && cron_file(r, f, sizeof(f)) == 0)
        load_from(r, f, 0);         /* Claude Code: a resume restores the session's jobs */
    if (durable_file(r, f, sizeof(f)) == 0)
        load_from(r, f, 1);
    tasks_changed(r->tools.tasks);
}

/* ---- /loop ---- */

/* Claude Code's built-in maintenance prompt (its three steps; the pull
 * request step is what this machine has instead: no git here) */
static const char maintenance[] =
    "Work through the following, in order:\n"
    "- continue any unfinished work from this conversation;\n"
    "- check on what the session left running or waiting (background commands, monitors, scheduled tasks, "
    "builds and tests) and deal with what they report;\n"
    "- when nothing else is pending, run a cleanup pass: hunt for bugs in, or simplify, the code worked on.\n";
static const char maintenance2[] =
    "Do not start new initiatives outside that scope. Irreversible actions (deleting, overwriting, sending "
    "anything out) only proceed when they continue something this conversation already authorized. If there "
    "is nothing to do, say so in one line.";

int sched_loop_default(cl_repl *r, jw *out)
{
    char f[400];
    int src;
    for (src = 0; src < 2; src++) {
        char *b = 0;
        long n = 0;
        /* the project's .claude/loop.md first, then the user's (ENVARC:Claude = ~/.claude) */
        if (src == 0 ? path_join(r->tools.root, ".claude/loop.md", f, sizeof(f))
                     : path_join(r->home, "loop.md", f, sizeof(f)))
            continue;
        if (!r->home[0] && src == 1)
            continue;
        if (r->sys->kind(r->sys->u, f) != 1 || r->sys->read(r->sys->u, f, LOOP_MD_READ, &b, &n))
            continue;
        jw_raw(out, b, n > LOOP_MD_MAX ? LOOP_MD_MAX : n);
        free(b);
        return 1;
    }
    jw_rawz(out, maintenance);
    jw_rawz(out, maintenance2);
    return 0;
}

/* a sentinel's prompt at its fire: the default prompt, and for the
 * self-paced one how to go on */
static void loop_sentinel(cl_repl *r, jw *prompt, int dynamic)
{
    sched_loop_default(r, prompt);
    if (dynamic)
        jw_rawz(prompt, "\n\n(An iteration of a self-paced /loop with no prompt of its own. When it needs "
                        "another, call ScheduleWakeup with prompt " LOOP_DYNAMIC "; to end it, stop: true.)");
}

static const char *blanks(const char *a)
{
    while (*a == ' ' || *a == '\t')
        a++;
    return a;
}

/* "5m", "5 minutes": the end of the time at a, 0 when it is not one */
static const char *time_at(const char *a)
{
    static const char *const words[] = { "seconds", "second", "secs", "sec", "minutes", "minute", "mins", "min",
                                         "hours", "hour", "hrs", "hr", "days", "day", 0 };
    const char *p = a;
    int i;
    while (*p >= '0' && *p <= '9')
        p++;
    if (p == a)
        return 0;
    if ((*p == 's' || *p == 'm' || *p == 'h' || *p == 'd') && (!p[1] || p[1] == ' ' || p[1] == '\t'))
        return p + 1;
    p = blanks(p);
    for (i = 0; words[i]; i++) {
        long l = (long)strlen(words[i]);
        if (!strncmp(p, words[i], (size_t)l) && (!p[l] || p[l] == ' ' || p[l] == '\t'))
            return p + l;
    }
    return 0;
}

int sched_loop_has_prompt(const char *a)
{
    const char *p;
    a = blanks(a);
    /* rule 1, a leading interval token (^\d+[smhd]$): the rest is the prompt */
    p = a;
    while (*p >= '0' && *p <= '9')
        p++;
    if (p > a && (*p == 's' || *p == 'm' || *p == 'h' || *p == 'd') && (!p[1] || p[1] == ' ' || p[1] == '\t'))
        return *blanks(p + 1) != 0;
    /* rule 2, a trailing "every N unit": empty when it is all there is */
    if (!strncmp(a, "every", 5) && (a[5] == ' ' || a[5] == '\t') && (p = time_at(blanks(a + 5))) != 0 &&
        !*blanks(p))
        return 0;
    return *a != 0;
}

int sched_esc_idle(void *u)
{
    cl_repl *r = (cl_repl *)u;
    if (!r->tools.tasks || !tasks_wakeup_get(r->tools.tasks))
        return 0;
    tasks_wakeup_cancel(r->tools.tasks);
    r->loop_fallback = 0;
    r->fold_n = 0;
    ui_line(&r->ui, "Cancelled the pending /loop wakeup: the loop is stopped (/loop starts it again).");
    return 1;
}

void sched_loop_end(cl_repl *r)
{
    loop_turn lt;
    int tick = r->loop_tick;
    struct cl_tasks *k = r->tools.tasks;
    r->loop_tick = 0;
    if (!k)
        return;
    tasks_loop_take(k, &lt);
    if (lt.called)
        r->loop_fallback = 0;
    if (tick && !lt.called && tasks_loop_on(k) && !tasks_wakeup_get(k)) {
        if (r->turn_rc == TURN_CANCEL) {
            tasks_wakeup_cancel(k);
            ui_line(&r->ui, "The /loop is stopped: its iteration was interrupted.");
        } else if (!r->loop_fallback) {
            char err[200], *again = 0;
            long now = now_s(r);
            const char *lp = tasks_loop_prompt(k);
            again = (char *)malloc(strlen(lp) + 1);
            if (again) {
                strcpy(again, lp);
                if (now >= 0 && tasks_wakeup_set(k, LOOP_FALLBACK_S, again, now, 0, err, sizeof(err)) == 0) {
                    r->loop_fallback = 1;
                    ui_line(&r->ui, "This /loop iteration set no next wakeup: one more in 20 minutes, and the loop "
                                    "ends if that one sets none either (Esc cancels it).");
                }
                free(again);
            }
        } else {
            tasks_wakeup_cancel(k);
            r->loop_fallback = 0;
            ui_line(&r->ui, "The /loop has ended: its fallback iteration set no next wakeup.");
        }
    }
    if (!r->tui)
        return;
    if (tick && lt.called && !lt.stopped && lt.noop) {
        /* quiet iterations in a row (nothing drawn between them) fold into one line */
        if (r->fold_n > 0 && r->fold_end == r->loop_mark && tui_takeback(r->tui, r->fold_mark) == 0) {
            char m[400], num[16];
            r->fold_n++;
            cl_copy(m, "Claude resuming /loop wakeup (", sizeof(m));
            cl_ltoa(r->fold_n, num);
            cl_cat(m, num, sizeof(m));
            cl_cat(m, " quiet wake-ups, nothing to do)", sizeof(m));
            if (lt.reason[0]) {
                cl_cat(m, ": ", sizeof(m));
                cl_cat(m, lt.reason, sizeof(m));
            }
            ui_line(&r->ui, m);
        } else {
            r->fold_mark = r->loop_mark;
            r->fold_n = 1;
        }
        r->fold_end = r->tui->n_lines;
    } else if (tick)
        r->fold_n = 0;
}

int sched_wake(cl_repl *r, jw *prompt, jw *shown)
{
    char p[2000], id[12];
    long now = now_s(r);
    jw w;
    int n, due;
    if (r->no_person)
        return 0;                   /* print mode: no turns of its own */
    jw_init(&w);
    n = sched_collect(r, &w);
    if (n && w.n) {
        framed(prompt, w.p, w.n);
        jw_rawz(shown, "Background news for Claude:\n");
        jw_raw(shown, w.p, w.n);
        jw_free(&w);
        return 1;
    }
    jw_free(&w);
    if (!r->tools.no_cron && r->tools.tasks &&
        (due = tasks_cron_due(r->tools.tasks, now, p, sizeof(p), id, sizeof(id))) != 0) {
        int sentinel = !strcmp(p, LOOP_DYNAMIC) || !strcmp(p, LOOP_FIXED);
        r->n_cron_fired++;
        if (sentinel)
            loop_sentinel(r, prompt, !strcmp(p, LOOP_DYNAMIC));
        else
            jw_rawz(prompt, p);
        if (due == 2) {
            /* Claude Code's words for a wakeup */
            r->loop_tick = 1;
            r->n_loop_ticks++;
            jw_rawz(shown, "Claude resuming /loop wakeup");
        } else {
            jw_rawz(shown, "Scheduled task ");
            jw_rawz(shown, id);
            jw_rawz(shown, ": ");
            jw_rawz(shown, sentinel ? "/loop (the default loop prompt)" : p);
        }
        sched_save(r);
        return 1;
    }
    return 0;
}

/* tui.h wake: the screen waits for keys; a prompt to run now, malloc'ed */
char *sched_tui_wake(void *u)
{
    cl_repl *r = (cl_repl *)u;
    jw p, s;
    char *line = 0;
    if (r->in_turn)
        return 0;
    if (r->idle_from && r->io->ms && r->io->ms(r->io->u) - r->idle_from < WAKE_AFTER_MS)
        return 0;                   /* a moment after a turn: keys typed straight on come first */
    jw_init(&p);
    jw_init(&s);
    if (r->tui)
        r->loop_mark = r->tui->n_lines;     /* a /loop iteration's lines start here */
    if (sched_wake(r, &p, &s) && !p.oom && p.n) {
        line = (char *)malloc((size_t)p.n + 1);
        if (line) {
            memcpy(line, p.p, (size_t)p.n);
            line[p.n] = 0;
            r->woke = 1;
            ui_line(&r->ui, s.p ? s.p : "");
        }
    }
    jw_free(&p);
    jw_free(&s);
    return line;
}

int sched_line_mode(cl_repl *r)
{
    jw p, s;
    int ran = 0;
    jw_init(&p);
    jw_init(&s);
    while (sched_wake(r, &p, &s) && !p.oom && ran < 8) {
        ui_line(&r->ui, s.p ? s.p : "");
        r->woke = 1;
        repl_line(r, p.p);
        r->woke = 0;
        r->loop_tick = 0;
        jw_reset(&p);
        jw_reset(&s);
        ran++;
    }
    jw_free(&p);
    jw_free(&s);
    return ran;
}
