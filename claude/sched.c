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
 * Portable C89, host-tested through the REPL suite. */
#include <stdlib.h>
#include <string.h>
#include "repl_int.h"
#include "tasks.h"
#include "tools_int.h"
#include "path.h"
#include "util.h"

#define WAKE_AFTER_MS 1000UL        /* the screen waits this long after a turn before one of its own */

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

int sched_wake(cl_repl *r, jw *prompt, jw *shown)
{
    char p[2000], id[12];
    long now = now_s(r);
    jw w;
    int n;
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
    if (!r->tools.no_cron && r->tools.tasks && tasks_cron_due(r->tools.tasks, now, p, sizeof(p), id, sizeof(id))) {
        r->n_cron_fired++;
        jw_rawz(prompt, p);
        jw_rawz(shown, "Scheduled task ");
        jw_rawz(shown, id);
        jw_rawz(shown, ": ");
        jw_rawz(shown, p);
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
        jw_reset(&p);
        jw_reset(&s);
        ran++;
    }
    jw_free(&p);
    jw_free(&s);
    return ran;
}
