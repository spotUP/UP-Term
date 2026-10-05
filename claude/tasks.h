/* tasks -- the task list and the session's cron jobs (A4 gaps 2): what the
 * REPL needs of tasks.c besides the tools themselves (tools_int.h). Times
 * are seconds since 1978-01-01, local (sys.h now). */
#ifndef CL_TASKS_H
#define CL_TASKS_H

#include "json.h"

/* a 5-field cron expression, a bit per value */
typedef struct cron_spec {
    unsigned long min[2], hour[2], dom[2], mon[2], dow[2];
    int dom_star, dow_star;
} cron_spec;

typedef struct cron_tm {
    int year, mon, day, hour, min, dow;     /* mon 1..12, day 1..31, dow 0 Sunday */
} cron_tm;

typedef struct cron_job {
    char id[12];                /* 8 characters */
    char expr[64];
    char *prompt;
    int recurring, durable;
    int wakeup;                 /* ScheduleWakeup's pending wakeup (/loop's self-paced mode) */
    long created;               /* when it was made */
    long next;                  /* the schedule's next match */
    long fire;                  /* when it fires (the match and its offset), -1 never */
    cron_spec spec;
} cron_job;

struct cl_tasks;

/* 0, or -1 with the reason in err */
int cron_parse(const char *expr, cron_spec *c, char *err, long cap);
/* the first matching minute at or after from, -1 none within four years */
long cron_next(const cron_spec *c, long from);
void cron_civil(long t, cron_tm *tm);

/* a job added (CronCreate's work): 0 with its id (9 bytes), -1 with err */
int tasks_cron_add(struct cl_tasks *k, const char *expr, const char *prompt, int recurring, int durable, long now,
                   char *id, char *err, long cap);
int tasks_cron_delete(struct cl_tasks *k, const char *id);
/* A job due at now: 1 with its prompt (and id), 2 when it was the
 * self-paced /loop's wakeup; a one-shot job (or a recurring one seven
 * days old) is removed, a recurring one set to its next time after now
 * (no catch-up). 0 none due. */
int tasks_cron_due(struct cl_tasks *k, long now, char *prompt, long cap, char *id, long icap);
int tasks_cron_count(const struct cl_tasks *k);
const cron_job *tasks_cron_get(const struct cl_tasks *k, int i);
/* did the jobs change since the last call? (the REPL saves them then) */
int tasks_changed(struct cl_tasks *k);
/* The jobs as a JSON array: stop_shape 1 Stop's session_crons ({id,
 * schedule, recurring, prompt}), else the form kept with the session and in
 * .claude/scheduled_tasks.json ({id, cron, recurring, prompt, created,
 * durable}); durable_only: only the durable ones. */
void tasks_crons_json(const struct cl_tasks *k, jw *w, int durable_only, int stop_shape);
/* Jobs kept earlier added back (a resume, scheduled_tasks.json): those
 * not expired; only_durable takes only the durable ones. Their count. */
int tasks_crons_load(struct cl_tasks *k, const char *json, long n, long now, int only_durable);

/* /loop's self-paced mode (ScheduleWakeup). One wakeup is pending at a
 * time, a one-shot job of the cron table flagged wakeup: it fires at
 * now + delay to the second (no jitter), shows in CronList and Stop's
 * session_crons, is not kept with the session (a resume does not bring it
 * back). The loop is on from the first wakeup until stop, a cancel, the
 * fallback's end or seven days. */
#define LOOP_MIN_S 60L
#define LOOP_MAX_S 3600L
#define LOOP_FALLBACK_S 1200L   /* an iteration that neither rescheduled nor stopped */
#define LOOP_DYNAMIC "<<autonomous-loop-dynamic>>"  /* ScheduleWakeup's prompt for a bare /loop */
#define LOOP_FIXED "<<autonomous-loop>>"            /* CronCreate's, for /loop INTERVAL alone */
/* the wakeup set (replacing a pending one), delay clamped to 60..3600:
 * 0 with its id, -1 with err (the loop is seven days old, the table full) */
int tasks_wakeup_set(struct cl_tasks *k, long delay, const char *prompt, long now, char *id, char *err, long cap);
/* the pending wakeup cancelled and the loop ended: 1 there was one */
int tasks_wakeup_cancel(struct cl_tasks *k);
/* the pending wakeup, 0 none */
const cron_job *tasks_wakeup_get(const struct cl_tasks *k);
/* is a self-paced loop on? */
int tasks_loop_on(const struct cl_tasks *k);
/* the turn's ScheduleWakeup calls: what the REPL reads at the turn's end
 * (called: rescheduled or stopped; noop: the last call said nothing
 * changed), then reset */
typedef struct loop_turn {
    int called, stopped, noop;
    char reason[200];
} loop_turn;
void tasks_loop_take(struct cl_tasks *k, loop_turn *t);
/* "20 minutes", "1 minute 30 seconds" */
void tasks_span_text(long s, char *out, long cap);
/* the prompt of the loop's last wakeup (the fallback repeats it) */
const char *tasks_loop_prompt(const struct cl_tasks *k);

#endif
