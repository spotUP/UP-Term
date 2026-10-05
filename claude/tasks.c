/* tasks -- Claude Code's task list and the session's scheduled prompts
 * (A4 gaps 2).
 *
 * TaskCreate / TaskGet / TaskList / TaskUpdate keep a list of tasks with
 * ids "1", "2", ...: subject, description, activeForm, status (pending,
 * in_progress, completed; "deleted" removes one), what each blocks and is
 * blocked by, an owner and metadata. The screen's task list (Ctrl+T) and
 * /todos show it as they show TodoWrite's list (tasks_todos).
 *
 * CronCreate / CronDelete / CronList keep up to 50 jobs of the session: a
 * standard 5-field cron expression in local time (* , - / in every field;
 * day of week 0 or 7 Sunday; when both days are restricted either one
 * matches, as vixie-cron), the prompt, recurring or once, an 8-character
 * id. The REPL fires a due job between turns (tasks_cron_due); a recurring
 * job fires a little after its time (a deterministic offset from its id:
 * up to half its interval, at most 30 minutes) and ends after seven days;
 * a one-shot job at :00 or :30 may fire up to 90 seconds early. durable:
 * true keeps the job in <root>/.claude/scheduled_tasks.json as well.
 *
 * ScheduleWakeup (A4 gaps 3) is /loop's self-paced mode: the model picks
 * when the next iteration runs, 60 s to an hour out (clamped), and the
 * pending wakeup is a one-shot job of the same table flagged wakeup --
 * fired by the same tasks_cron_due between turns, to the second, no
 * jitter, never kept with the session. stop: true cancels it and ends the
 * loop. The REPL reads each turn's calls at its end (tasks_loop_take) for
 * the fallback wakeup and the screen's folding of quiet ticks.
 * Times are seconds since 1978-01-01 (sys.h now, AmigaDOS's epoch, a
 * Sunday).
 * Portable C89, host-tested (tests/test_claude_tools.c, the REPL suite). */
#include <stdlib.h>
#include <string.h>
#include "tools_int.h"
#include "tasks.h"
#include "util.h"

#define TASKS_MAX 100
#define CRON_MAX 50
#define WEEK (7L * 86400L)

typedef struct task {
    int id;                     /* 0: a free slot */
    char *subject, *desc, *active, *owner, *meta;
    char status[16];
    char blocks[64], blocked[64];   /* ids, comma-separated */
} task;

typedef struct cl_tasks {
    task t[TASKS_MAX];
    int next;
    cron_job c[CRON_MAX];
    int nc;
    unsigned long seq;
    int changed;                /* the jobs changed since the REPL last looked */
    /* /loop's self-paced mode */
    int loop_on;
    long loop_t0;               /* the loop's first wakeup was set (seven days from here) */
    char *loop_prompt;          /* its last wakeup's prompt */
    loop_turn lt;               /* this turn's ScheduleWakeup calls */
} cl_tasks;

cl_tasks *tasks_new(void)
{
    cl_tasks *k = (cl_tasks *)calloc(1, sizeof(cl_tasks));
    if (k)
        k->next = 1;
    return k;
}

static void task_clear(task *x)
{
    free(x->subject);
    free(x->desc);
    free(x->active);
    free(x->owner);
    free(x->meta);
    memset(x, 0, sizeof(*x));
}

void tasks_free(cl_tasks *k)
{
    int i;
    if (!k)
        return;
    for (i = 0; i < TASKS_MAX; i++)
        task_clear(&k->t[i]);
    for (i = 0; i < k->nc; i++)
        free(k->c[i].prompt);
    free(k->loop_prompt);
    free(k);
}

static char *dupz(const char *s)
{
    char *d = (char *)malloc(strlen(s) + 1);
    if (d)
        strcpy(d, s);
    return d;
}

static task *by_id(cl_tasks *k, const char *id)
{
    int i, n;
    if (*id == '#')
        id++;
    n = atoi(id);
    for (i = 0; i < TASKS_MAX && n > 0; i++)
        if (k->t[i].id == n)
            return &k->t[i];
    return 0;
}

/* an id appended to a comma list (once) */
static void list_add(char *list, long cap, const char *id)
{
    char num[16];
    const char *p = list;
    long l;
    if (*id == '#')
        id++;
    cl_copy(num, id, sizeof(num));
    l = (long)strlen(num);
    while (*p) {
        if (!strncmp(p, num, (size_t)l) && (p[l] == ',' || !p[l]))
            return;
        p = strchr(p, ',');
        if (!p)
            break;
        p++;
    }
    if (list[0])
        cl_cat(list, ",", cap);
    cl_cat(list, num, cap);
}


static void list_text(jw *w, const char *list)
{
    const char *p = list;
    while (*p) {
        const char *e = strchr(p, ',');
        long l = e ? (long)(e - p) : (long)strlen(p);
        jw_raw(w, "#", 1);
        jw_raw(w, p, l);
        p += l;
        if (*p) {
            jw_rawz(w, ", ");
            p++;
        }
    }
}

/* the string array of key added to the comma list */
static void add_ids(char *list, long cap, jv in, const char *key)
{
    jv a, e;
    jit it;
    if (!json_get(in, key, &a) || json_type(a) != J_ARR)
        return;
    json_iter(a, &it);
    while (json_next(&it, 0, &e)) {
        char id[16];
        if (json_type(e) == J_STR && json_str(e, id, sizeof(id)) > 0)
            list_add(list, cap, id);
    }
}

static void set_str(char **f, jv in, const char *key)
{
    long l;
    char *v = tl_prop(in, key, &l);
    if (!v)
        return;
    free(*f);
    *f = v;
}

static void task_create(cl_tools *t, cl_tasks *k, jw *out, const char *id, jv in)
{
    task *x = 0;
    int i;
    jw m;
    jv md;
    char num[16];
    for (i = 0; i < TASKS_MAX; i++)
        if (!k->t[i].id) {
            x = &k->t[i];
            break;
        }
    if (!x) {
        tl_error(t, out, id, "the task list is full (100 tasks); delete finished ones first", 0);
        return;
    }
    memset(x, 0, sizeof(*x));
    x->subject = tl_prop(in, "subject", 0);
    x->desc = tl_prop(in, "description", 0);
    x->active = tl_prop(in, "activeForm", 0);
    x->owner = dupz("");
    x->meta = json_get(in, "metadata", &md) && json_type(md) == J_OBJ ? (char *)malloc((size_t)md.n + 1) : 0;
    if (x->meta) {
        memcpy(x->meta, md.p, (size_t)md.n);
        x->meta[md.n] = 0;
    }
    if (!x->subject || !x->desc || !x->active || !x->owner) {
        task_clear(x);
        tl_error(t, out, id, "out of memory", 0);
        return;
    }
    cl_copy(x->status, "pending", sizeof(x->status));
    x->id = k->next++;
    jw_init(&m);
    jw_rawz(&m, "Task #");
    cl_ltoa(x->id, num);
    jw_rawz(&m, num);
    jw_rawz(&m, " created successfully: ");
    jw_rawz(&m, x->subject);
    tl_result(t, out, id, m.p, m.n, 0);
    jw_free(&m);
}

static void task_get(cl_tools *t, cl_tasks *k, jw *out, const char *id, jv in)
{
    char *tid = tl_prop(in, "taskId", 0);
    task *x = tid ? by_id(k, tid) : 0;
    char num[16];
    jw m;
    if (!x) {
        tl_error(t, out, id, "Task not found: ", tid ? tid : "?");
        free(tid);
        return;
    }
    free(tid);
    jw_init(&m);
    jw_rawz(&m, "Task #");
    cl_ltoa(x->id, num);
    jw_rawz(&m, num);
    jw_rawz(&m, ": ");
    jw_rawz(&m, x->subject);
    jw_rawz(&m, "\nStatus: ");
    jw_rawz(&m, x->status);
    if (x->owner[0]) {
        jw_rawz(&m, "\nOwner: ");
        jw_rawz(&m, x->owner);
    }
    jw_rawz(&m, "\nDescription: ");
    jw_rawz(&m, x->desc);
    if (x->blocks[0]) {
        jw_rawz(&m, "\nBlocks: ");
        list_text(&m, x->blocks);
    }
    if (x->blocked[0]) {
        jw_rawz(&m, "\nBlocked by: ");
        list_text(&m, x->blocked);
    }
    if (x->meta) {
        jw_rawz(&m, "\nMetadata: ");
        jw_rawz(&m, x->meta);
    }
    tl_result(t, out, id, m.p, m.n, 0);
    jw_free(&m);
}

/* an open task among the ids of a list (a blocker that is not done yet)? */
static int open_among(cl_tasks *k, const char *list)
{
    const char *p = list;
    while (*p) {
        task *x = by_id(k, p);
        if (x && strcmp(x->status, "completed"))
            return 1;
        p = strchr(p, ',');
        if (!p)
            break;
        p++;
    }
    return 0;
}

static void task_list(cl_tools *t, cl_tasks *k, jw *out, const char *id)
{
    int i, n = 0;
    char num[16];
    jw m;
    jw_init(&m);
    for (i = 0; i < TASKS_MAX; i++) {
        task *x = &k->t[i];
        if (!x->id)
            continue;
        if (m.n)
            jw_raw(&m, "\n", 1);
        jw_rawz(&m, "#");
        cl_ltoa(x->id, num);
        jw_rawz(&m, num);
        jw_rawz(&m, " [");
        jw_rawz(&m, x->status);
        jw_rawz(&m, "] ");
        jw_rawz(&m, x->subject);
        if (x->owner[0]) {
            jw_rawz(&m, " (owner: ");
            jw_rawz(&m, x->owner);
            jw_rawz(&m, ")");
        }
        if (x->blocked[0] && open_among(k, x->blocked)) {
            jw_rawz(&m, " [blocked by ");
            list_text(&m, x->blocked);
            jw_rawz(&m, "]");
        }
        n++;
    }
    if (!n)
        jw_rawz(&m, "No tasks found");
    tl_result(t, out, id, m.p ? m.p : "", m.n, 0);
    jw_free(&m);
}

static void task_update(cl_tools *t, cl_tasks *k, jw *out, const char *id, jv in)
{
    char *tid = tl_prop(in, "taskId", 0), st[16], num[16], from[16];
    task *x = tid ? by_id(k, tid) : 0;
    jv v;
    jw f, m;
    if (!x) {
        tl_error(t, out, id, "Task not found: ", tid ? tid : "?");
        free(tid);
        return;
    }
    free(tid);
    jw_init(&f);
    jw_init(&m);
    st[0] = 0;
    cl_copy(from, x->status, sizeof(from));
    if (json_get(in, "status", &v) && json_type(v) == J_STR)
        json_str(v, st, sizeof(st));
    cl_ltoa(x->id, num);
    if (!strcmp(st, "deleted")) {
        int i;
        /* gone, and out of the others' lists */
        for (i = 0; i < TASKS_MAX; i++)
            if (k->t[i].id && k->t[i].id != x->id) {
                /* rebuild the lists without num */
                char *lists[2];
                int j;
                lists[0] = k->t[i].blocks;
                lists[1] = k->t[i].blocked;
                for (j = 0; j < 2; j++) {
                    char keep[64], *p = lists[j];
                    keep[0] = 0;
                    while (*p) {
                        char one[16];
                        long l = (long)strcspn(p, ",");
                        if (l < (long)sizeof(one)) {
                            memcpy(one, p, (size_t)l);
                            one[l] = 0;
                            if (strcmp(one, num))
                                list_add(keep, sizeof(keep), one);
                        }
                        p += l;
                        if (*p)
                            p++;
                    }
                    cl_copy(lists[j], keep, 64);
                }
            }
        task_clear(x);
        jw_rawz(&m, "Updated task #");
        jw_rawz(&m, num);
        jw_rawz(&m, " deleted");
        tl_result(t, out, id, m.p, m.n, 0);
        jw_free(&m);
        jw_free(&f);
        return;
    }
#define FIELD(name)                                                                                                \
    do {                                                                                                           \
        if (f.n)                                                                                                   \
            jw_rawz(&f, ", ");                                                                                     \
        jw_rawz(&f, name);                                                                                         \
    } while (0)
    if (st[0]) {
        cl_copy(x->status, st, sizeof(x->status));
        FIELD("status");
    }
    if (json_get(in, "subject", &v)) {
        set_str(&x->subject, in, "subject");
        FIELD("subject");
    }
    if (json_get(in, "description", &v)) {
        set_str(&x->desc, in, "description");
        FIELD("description");
    }
    if (json_get(in, "activeForm", &v)) {
        set_str(&x->active, in, "activeForm");
        FIELD("activeForm");
    }
    if (json_get(in, "owner", &v)) {
        set_str(&x->owner, in, "owner");
        FIELD("owner");
    }
    if (json_get(in, "addBlocks", &v)) {
        jv e;
        jit it;
        add_ids(x->blocks, sizeof(x->blocks), in, "addBlocks");
        /* the other side knows it too */
        json_iter(v, &it);
        while (json_next(&it, 0, &e)) {
            char o[16];
            task *y;
            if (json_type(e) == J_STR && json_str(e, o, sizeof(o)) > 0 && (y = by_id(k, o)) != 0)
                list_add(y->blocked, sizeof(y->blocked), num);
        }
        FIELD("blocks");
    }
    if (json_get(in, "addBlockedBy", &v)) {
        jv e;
        jit it;
        add_ids(x->blocked, sizeof(x->blocked), in, "addBlockedBy");
        json_iter(v, &it);
        while (json_next(&it, 0, &e)) {
            char o[16];
            task *y;
            if (json_type(e) == J_STR && json_str(e, o, sizeof(o)) > 0 && (y = by_id(k, o)) != 0)
                list_add(y->blocks, sizeof(y->blocks), num);
        }
        FIELD("blockedBy");
    }
    if (json_get(in, "metadata", &v) && json_type(v) == J_OBJ) {
        free(x->meta);
        x->meta = (char *)malloc((size_t)v.n + 1);
        if (x->meta) {
            memcpy(x->meta, v.p, (size_t)v.n);
            x->meta[v.n] = 0;
        }
        FIELD("metadata");
    }
#undef FIELD
    jw_rawz(&m, "Updated task #");
    jw_rawz(&m, num);
    jw_rawz(&m, " ");
    jw_raw(&m, f.n ? f.p : "nothing", f.n ? f.n : 7);
    if (st[0] && strcmp(from, st)) {
        jw_rawz(&m, " (");
        jw_rawz(&m, from);
        jw_rawz(&m, " -> ");
        jw_rawz(&m, st);
        jw_rawz(&m, ")");
    }
    tl_result(t, out, id, m.p ? m.p : "", m.n, 0);
    jw_free(&m);
    jw_free(&f);
}

char *tasks_todos(const cl_tasks *k)
{
    jw w;
    int i, n = 0;
    if (!k)
        return 0;
    jw_init(&w);
    jw_rawz(&w, "{\"todos\":[");
    for (i = 0; i < TASKS_MAX; i++) {
        const task *x = &k->t[i];
        if (!x->id)
            continue;
        if (n++)
            jw_raw(&w, ",", 1);
        jw_rawz(&w, "{\"content\":");
        jw_strz(&w, x->subject);
        jw_rawz(&w, ",\"status\":");
        jw_strz(&w, x->status);
        jw_rawz(&w, ",\"activeForm\":");
        jw_strz(&w, x->active[0] ? x->active : x->subject);
        jw_raw(&w, "}", 1);
    }
    jw_rawz(&w, "]}");
    if (!n || w.oom) {
        jw_free(&w);
        return 0;
    }
    return w.p;
}

/* ---- cron expressions ---- */

/* one field into its bits (lo..hi): 0, -1 */
static int field(const char *s, long n, int lo, int hi, unsigned long *bits, int *star)
{
    long i = 0;
    *star = n == 1 && s[0] == '*';
    while (i < n) {
        long e = i, a, b, step = 1, k;
        while (e < n && s[e] != ',')
            e++;
        if (e == i)
            return -1;
        k = i;
        if (s[k] == '*') {
            a = lo;
            b = hi;
            k++;
        } else {
            if (s[k] < '0' || s[k] > '9')
                return -1;
            for (a = 0; k < e && s[k] >= '0' && s[k] <= '9'; k++)
                a = a * 10 + (s[k] - '0');
            b = a;
            if (k < e && s[k] == '-') {
                k++;
                if (k >= e || s[k] < '0' || s[k] > '9')
                    return -1;
                for (b = 0; k < e && s[k] >= '0' && s[k] <= '9'; k++)
                    b = b * 10 + (s[k] - '0');
            }
        }
        if (k < e && s[k] == '/') {
            k++;
            if (k >= e || s[k] < '0' || s[k] > '9')
                return -1;
            for (step = 0; k < e && s[k] >= '0' && s[k] <= '9'; k++)
                step = step * 10 + (s[k] - '0');
            if (step < 1)
                return -1;
            if (a == b && s[i] != '*')
                b = hi;             /* "5/15": from 5 on */
        }
        if (k != e || a < lo || b > hi || a > b)
            return -1;
        for (; a <= b; a += step)
            bits[(a) / 32] |= 1ul << (a % 32);
        i = e + 1;
        if (e < n && i >= n)
            return -1;              /* a trailing comma */
    }
    return 0;
}

static int bit(const unsigned long *b, int i)
{
    return (b[i / 32] >> (i % 32)) & 1;
}

int cron_parse(const char *expr, cron_spec *c, char *err, long cap)
{
    const char *p = expr;
    int f = 0;
    memset(c, 0, sizeof(*c));
    while (*p && f < 6) {
        const char *s;
        int star, rc;
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        s = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
        switch (f) {
        case 0:
            rc = field(s, (long)(p - s), 0, 59, c->min, &star);
            break;
        case 1:
            rc = field(s, (long)(p - s), 0, 23, c->hour, &star);
            break;
        case 2:
            rc = field(s, (long)(p - s), 1, 31, c->dom, &star);
            c->dom_star = star;
            break;
        case 3:
            rc = field(s, (long)(p - s), 1, 12, c->mon, &star);
            break;
        case 4:
            rc = field(s, (long)(p - s), 0, 7, c->dow, &star);
            c->dow_star = star;
            if (bit(c->dow, 7))
                c->dow[0] |= 1ul;   /* 7 is Sunday too */
            break;
        default:
            rc = -1;
        }
        if (rc) {
            static const char *const names[] = { "minute", "hour", "day of month", "month", "day of week", "?" };
            cl_copy(err, "the cron expression's ", cap);
            cl_cat(err, names[f < 5 ? f : 5], cap);
            cl_cat(err, f < 5 ? " field is not usable (numbers, *, ranges a-b, steps /n, lists a,b; no names)"
                              : " has more than five fields",
                   cap);
            return -1;
        }
        f++;
    }
    if (f != 5) {
        cl_copy(err, "a cron expression has five fields: minute hour day-of-month month day-of-week", cap);
        return -1;
    }
    return 0;
}

/* days from 1978-01-01 to y-m-d (m 1..12) */
static long days_of(int y, int m, int d)
{
    static const int cum[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    long days = 0;
    int k;
    for (k = 1978; k < y; k++)
        days += (k % 4 == 0 && (k % 100 != 0 || k % 400 == 0)) ? 366 : 365;
    days += cum[m - 1] + d - 1;
    if (m > 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))
        days++;
    return days;
}

void cron_civil(long t, cron_tm *tm)
{
    long days = t / 86400L, s = t % 86400L, y = 1978;
    int leap, m;
    static const int ml[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    tm->dow = (int)(days % 7);      /* 1978-01-01 was a Sunday */
    for (;;) {
        long yl;
        leap = y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
        yl = leap ? 366 : 365;
        if (days < yl)
            break;
        days -= yl;
        y++;
    }
    for (m = 0; m < 12; m++) {
        long l = ml[m] + (m == 1 && leap);
        if (days < l)
            break;
        days -= l;
    }
    tm->year = (int)y;
    tm->mon = m + 1;
    tm->day = (int)days + 1;
    tm->hour = (int)(s / 3600);
    tm->min = (int)(s % 3600 / 60);
}

static int day_ok(const cron_spec *c, const cron_tm *tm)
{
    int d = bit(c->dom, tm->day), w = bit(c->dow, tm->dow);
    if (c->dom_star && c->dow_star)
        return 1;
    if (c->dom_star)
        return w;
    if (c->dow_star)
        return d;
    return d || w;                  /* both restricted: either (vixie-cron) */
}

long cron_next(const cron_spec *c, long from)
{
    long t = (from + 59) / 60 * 60, limit = from + 4L * 366 * 86400L;
    cron_tm tm;
    while (t <= limit) {
        cron_civil(t, &tm);
        if (!bit(c->mon, tm.mon) || !day_ok(c, &tm)) {
            /* to the next day's midnight */
            t = days_of(tm.year, tm.mon, tm.day) * 86400L + 86400L;
            continue;
        }
        if (!bit(c->hour, tm.hour)) {
            t = t - tm.min * 60L + 3600L;
            continue;
        }
        if (!bit(c->min, tm.min)) {
            t += 60;
            continue;
        }
        return t;
    }
    return -1;
}

/* a recurring job's offset after its time: up to half its interval, at
 * most 30 minutes, the same for the same id */
static long jitter(const cron_job *j)
{
    unsigned long h = 5381;
    const char *p;
    long a = cron_next(&j->spec, j->next + 60), span;
    for (p = j->id; *p; p++)
        h = h * 33 + (unsigned char)*p;
    span = a > 0 ? (a - j->next) / 2 : 1800;
    if (span > 1800)
        span = 1800;
    if (span < 1)
        return 0;
    return (long)(h % (unsigned long)span);
}

/* the readable schedule of the shapes /loop makes; else the expression */
static void human(const char *expr, char *out, long cap)
{
    char f[5][24];
    int n = 0, i;
    const char *p = expr;
    while (*p && n < 5) {
        int k = 0;
        while (*p == ' ')
            p++;
        while (*p && *p != ' ' && k < 23)
            f[n][k++] = *p++;
        f[n][k] = 0;
        if (k)
            n++;
    }
    for (i = 2; i < 5 && n == 5; i++)
        if (strcmp(f[i], "*"))
            break;
    if (n == 5 && i == 5) {
        if (!strncmp(f[0], "*/", 2) && !strcmp(f[1], "*")) {
            cl_copy(out, "every ", cap);
            cl_cat(out, f[0] + 2, cap);
            cl_cat(out, !strcmp(f[0] + 2, "1") ? " minute" : " minutes", cap);
            return;
        }
        if (!strcmp(f[0], "*") && !strcmp(f[1], "*")) {
            cl_copy(out, "every minute", cap);
            return;
        }
        if (f[0][0] >= '0' && f[0][0] <= '9' && !strchr(f[0], ',') && !strncmp(f[1], "*/", 2)) {
            cl_copy(out, "every ", cap);
            cl_cat(out, f[1] + 2, cap);
            cl_cat(out, " hours at minute ", cap);
            cl_cat(out, f[0], cap);
            return;
        }
        if (f[0][0] >= '0' && f[0][0] <= '9' && !strchr(f[0], ',') && !strcmp(f[1], "*")) {
            cl_copy(out, "every hour at minute ", cap);
            cl_cat(out, f[0], cap);
            return;
        }
        if (f[0][0] >= '0' && f[0][0] <= '9' && f[1][0] >= '0' && f[1][0] <= '9' && !strpbrk(f[0], ",-/") &&
            !strpbrk(f[1], ",-/")) {
            cl_copy(out, "every day at ", cap);
            cl_cat(out, f[1], cap);
            cl_cat(out, ":", cap);
            if (strlen(f[0]) < 2)
                cl_cat(out, "0", cap);
            cl_cat(out, f[0], cap);
            return;
        }
    }
    cl_copy(out, "cron ", cap);
    cl_cat(out, expr, cap);
}

static void job_drop(cl_tasks *k, int i)
{
    free(k->c[i].prompt);
    memmove(&k->c[i], &k->c[i + 1], sizeof(cron_job) * (size_t)(k->nc - i - 1));
    k->nc--;
    k->changed = 1;
}

/* the job's next fire from now on (with its offset), -1 none */
static long schedule(cron_job *j, long now)
{
    long n = cron_next(&j->spec, now);
    j->next = n;
    if (n < 0)
        return -1;
    if (j->recurring)
        j->fire = n + jitter(j);
    else {
        cron_tm tm;
        cron_civil(n, &tm);
        j->fire = n;
        if (tm.min == 0 || tm.min == 30) {
            /* Claude Code: a one-shot at the top or bottom of the hour, up to 90 s early */
            unsigned long h = 5381;
            const char *p;
            for (p = j->id; *p; p++)
                h = h * 33 + (unsigned char)*p;
            j->fire = n - (long)(h % 91UL);
        }
    }
    return j->fire;
}

int tasks_cron_add(cl_tasks *k, const char *expr, const char *prompt, int recurring, int durable, long now,
                   char *id, char *err, long cap)
{
    cron_job *j;
    cron_spec spec;
    static const char hex[] = "0123456789abcdef";
    unsigned long h;
    const char *p;
    int i;
    if (k->nc >= CRON_MAX) {
        cl_copy(err, "a session holds at most 50 scheduled tasks; delete one first", cap);
        return -1;
    }
    if (cron_parse(expr, &spec, err, cap))
        return -1;
    if (cron_next(&spec, now) < 0) {
        cl_copy(err, "the cron expression never matches", cap);
        return -1;
    }
    j = &k->c[k->nc];
    memset(j, 0, sizeof(*j));
    j->spec = spec;
    cl_copy(j->expr, expr, sizeof(j->expr));
    j->prompt = dupz(prompt);
    if (!j->prompt) {
        cl_copy(err, "out of memory", cap);
        return -1;
    }
    j->recurring = recurring;
    j->durable = durable;
    j->created = now;
    /* an 8-character id */
    h = 5381 + (unsigned long)now + ++k->seq * 2654435761UL;
    for (p = prompt; *p; p++)
        h = h * 33 + (unsigned char)*p;
    for (p = expr; *p; p++)
        h = h * 31 + (unsigned char)*p;
    for (i = 0; i < 8; i++) {
        j->id[i] = hex[h & 15];
        h = (h >> 4) | ((h & 15) << 28);
        h ^= k->seq * 0x9e37UL;
    }
    j->id[8] = 0;
    k->nc++;
    schedule(j, now);
    k->changed = 1;
    if (id)
        cl_copy(id, j->id, 9);
    return 0;
}

int tasks_cron_delete(cl_tasks *k, const char *id)
{
    int i;
    for (i = 0; i < k->nc; i++)
        if (cl_strieq(k->c[i].id, id)) {
            if (k->c[i].wakeup)
                k->loop_on = 0;     /* CronDelete of the wakeup: the loop ends */
            job_drop(k, i);
            return 0;
        }
    return -1;
}

int tasks_cron_due(cl_tasks *k, long now, char *prompt, long cap, char *id, long icap)
{
    int i;
    if (!k || now < 0)
        return 0;
    for (i = 0; i < k->nc; i++) {
        cron_job *j = &k->c[i];
        int wake = j->wakeup;
        if (j->fire < 0 || now < j->fire)
            continue;
        cl_copy(prompt, j->prompt, cap);
        if (id)
            cl_copy(id, j->id, icap);
        if (!j->recurring || now - j->created >= WEEK) {
            job_drop(k, i);         /* once, or its last fire after seven days */
        } else {
            /* no catch-up: the next time after now */
            schedule(j, now + 1);
            k->changed = 1;
        }
        return wake ? 2 : 1;
    }
    return 0;
}

int tasks_cron_count(const cl_tasks *k)
{
    return k ? k->nc : 0;
}

const cron_job *tasks_cron_get(const cl_tasks *k, int i)
{
    return k && i >= 0 && i < k->nc ? &k->c[i] : 0;
}

int tasks_changed(cl_tasks *k)
{
    int c = k && k->changed;
    if (k)
        k->changed = 0;
    return c;
}

void tasks_crons_json(const cl_tasks *k, jw *w, int durable_only, int stop_shape)
{
    int i, first = 1;
    jw_raw(w, "[", 1);
    for (i = 0; k && i < k->nc; i++) {
        const cron_job *j = &k->c[i];
        if (durable_only && !j->durable)
            continue;
        if (j->wakeup && !stop_shape)
            continue;               /* Claude Code: a self-paced /loop is not restored on a resume */
        if (!first)
            jw_raw(w, ",", 1);
        first = 0;
        jw_rawz(w, "{\"id\":");
        jw_strz(w, j->id);
        jw_rawz(w, stop_shape ? ",\"schedule\":" : ",\"cron\":");
        jw_strz(w, j->expr);
        jw_rawz(w, ",\"recurring\":");
        jw_rawz(w, j->recurring ? "true" : "false");
        jw_rawz(w, ",\"prompt\":");
        if (stop_shape && (long)strlen(j->prompt) > 1000) {
            jw ptxt;
            char num[16];
            jw_init(&ptxt);
            jw_raw(&ptxt, j->prompt, 1000);
            jw_rawz(&ptxt, "... [+");
            cl_ltoa((long)strlen(j->prompt) - 1000, num);
            jw_rawz(&ptxt, num);
            jw_rawz(&ptxt, " chars]");
            jw_str(w, ptxt.p ? ptxt.p : "", ptxt.n);
            jw_free(&ptxt);
        } else
            jw_strz(w, j->prompt);
        if (!stop_shape) {
            jw_rawz(w, ",\"created\":");
            jw_long(w, j->created);
            jw_rawz(w, ",\"durable\":");
            jw_rawz(w, j->durable ? "true" : "false");
        }
        jw_raw(w, "}", 1);
    }
    jw_raw(w, "]", 1);
}

int tasks_crons_load(cl_tasks *k, const char *json, long n, long now, int only_durable)
{
    jv a, e, x;
    jit it;
    int got = 0;
    if (!k || json_parse(json, n, &a) || json_type(a) != J_ARR)
        return 0;
    json_iter(a, &it);
    while (json_next(&it, 0, &e)) {
        char expr[64], id[12];
        char *prompt;
        long l, created;
        int rec, dur, i, dup = 0;
        expr[0] = id[0] = 0;
        if (json_get(e, "cron", &x))
            json_str(x, expr, sizeof(expr));
        if (json_get(e, "id", &x))
            json_str(x, id, sizeof(id));
        rec = json_get(e, "recurring", &x) && json_type(x) == J_TRUE;
        dur = json_get(e, "durable", &x) && json_type(x) == J_TRUE;
        created = json_get(e, "created", &x) ? json_long(x, now) : now;
        if (only_durable && !dur)
            continue;
        for (i = 0; i < k->nc; i++)
            dup |= cl_strieq(k->c[i].id, id);
        if (dup || !expr[0] || !json_get(e, "prompt", &x) || !(prompt = json_strdup(x, &l)))
            continue;
        if (rec && now - created >= WEEK) {
            free(prompt);
            continue;               /* expired */
        }
        {
            char err[160], nid[12];
            if (tasks_cron_add(k, expr, prompt, rec, dur, now, nid, err, sizeof(err)) == 0) {
                cron_job *j = &k->c[k->nc - 1];
                if (id[0])
                    cl_copy(j->id, id, sizeof(j->id));
                j->created = created;
                schedule(j, now);
                if (!rec && j->next < 0) {
                    job_drop(k, k->nc - 1);
                    free(prompt);
                    continue;
                }
                got++;
            }
        }
        free(prompt);
    }
    return got;
}

/* ---- /loop's self-paced mode ---- */

int tasks_wakeup_cancel(cl_tasks *k)
{
    int i, had = 0;
    for (i = 0; k && i < k->nc; i++)
        if (k->c[i].wakeup) {
            job_drop(k, i);
            had = 1;
            break;
        }
    if (k)
        k->loop_on = 0;
    return had;
}

const cron_job *tasks_wakeup_get(const cl_tasks *k)
{
    int i;
    for (i = 0; k && i < k->nc; i++)
        if (k->c[i].wakeup)
            return &k->c[i];
    return 0;
}

int tasks_loop_on(const cl_tasks *k)
{
    return k && k->loop_on;
}

const char *tasks_loop_prompt(const cl_tasks *k)
{
    return k && k->loop_prompt ? k->loop_prompt : "";
}

void tasks_loop_take(cl_tasks *k, loop_turn *t)
{
    if (!k) {
        memset(t, 0, sizeof(*t));
        return;
    }
    *t = k->lt;
    memset(&k->lt, 0, sizeof(k->lt));
}

int tasks_wakeup_set(cl_tasks *k, long delay, const char *prompt, long now, char *id, char *err, long cap)
{
    cron_tm tm;
    char expr[64], num[16], *keep;
    int i;
    cron_job *j;
    if (delay < LOOP_MIN_S)
        delay = LOOP_MIN_S;
    if (delay > LOOP_MAX_S)
        delay = LOOP_MAX_S;
    if (k->loop_on && now + delay - k->loop_t0 >= WEEK) {
        tasks_wakeup_cancel(k);
        cl_copy(err, "the loop is seven days old and has ended (Claude Code's expiry); /loop starts it again", cap);
        return -1;
    }
    keep = dupz(prompt);
    if (!keep) {
        cl_copy(err, "out of memory", cap);
        return -1;
    }
    for (i = 0; i < k->nc; i++)
        if (k->c[i].wakeup) {
            job_drop(k, i);         /* one pending wakeup: the new one replaces it */
            break;
        }
    /* its expression pins the minute it fires in (CronList, session_crons) */
    cron_civil(now + delay, &tm);
    cl_ltoa(tm.min, expr);
    cl_cat(expr, " ", sizeof(expr));
    cl_ltoa(tm.hour, num);
    cl_cat(expr, num, sizeof(expr));
    cl_cat(expr, " ", sizeof(expr));
    cl_ltoa(tm.day, num);
    cl_cat(expr, num, sizeof(expr));
    cl_cat(expr, " ", sizeof(expr));
    cl_ltoa(tm.mon, num);
    cl_cat(expr, num, sizeof(expr));
    cl_cat(expr, " *", sizeof(expr));
    if (tasks_cron_add(k, expr, prompt, 0, 0, now, id, err, cap)) {
        free(keep);
        return -1;
    }
    j = &k->c[k->nc - 1];
    j->wakeup = 1;
    j->fire = now + delay;          /* to the second, no jitter */
    if (!k->loop_on)
        k->loop_t0 = now;
    k->loop_on = 1;
    free(k->loop_prompt);
    k->loop_prompt = keep;
    return 0;
}

/* ---- the tools ---- */

static void cron_create(cl_tools *t, cl_tasks *k, jw *out, const char *id, jv in)
{
    char *expr = tl_prop(in, "cron", 0), *prompt = tl_prop(in, "prompt", 0), err[200], jid[12], hs[96];
    jv v;
    int rec = !(json_get(in, "recurring", &v) && json_type(v) == J_FALSE);
    int dur = json_get(in, "durable", &v) && json_type(v) == J_TRUE;
    long now = t->sys->now ? t->sys->now(t->sys->u) : -1;
    jw m;
    if (!expr || !prompt) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    if (now < 0) {
        tl_error(t, out, id, "the machine's clock is not available: nothing can be scheduled", 0);
        goto done;
    }
    if (t->show)
        t->show(t->u, "CronCreate", expr);
    if (tasks_cron_add(k, expr, prompt, rec, dur, now, jid, err, sizeof(err))) {
        tl_error(t, out, id, err, 0);
        goto done;
    }
    human(expr, hs, sizeof(hs));
    jw_init(&m);
    jw_rawz(&m, "{\"id\":");
    jw_strz(&m, jid);
    jw_rawz(&m, ",\"humanSchedule\":");
    jw_strz(&m, hs);
    jw_rawz(&m, ",\"recurring\":");
    jw_rawz(&m, rec ? "true" : "false");
    jw_rawz(&m, ",\"durable\":");
    jw_rawz(&m, dur ? "true" : "false");
    jw_rawz(&m, "}\nScheduled ");
    jw_rawz(&m, rec ? "(recurring, ends after 7 days): " : "(once): ");
    jw_rawz(&m, hs);
    jw_rawz(&m, ", local time. It fires between turns while the session is open and idle; CronDelete ");
    jw_rawz(&m, jid);
    jw_rawz(&m, " cancels it.");
    tl_result(t, out, id, m.p ? m.p : "", m.n, 0);
    jw_free(&m);
done:
    free(expr);
    free(prompt);
}

static void cron_delete(cl_tools *t, cl_tasks *k, jw *out, const char *id, jv in)
{
    char *jid = tl_prop(in, "id", 0);
    jw m;
    if (!jid) {
        tl_error(t, out, id, "out of memory", 0);
        return;
    }
    if (t->show)
        t->show(t->u, "CronDelete", jid);
    if (tasks_cron_delete(k, jid)) {
        tl_error(t, out, id, "No scheduled task with id ", jid);
        free(jid);
        return;
    }
    jw_init(&m);
    jw_rawz(&m, "{\"id\":");
    jw_strz(&m, jid);
    jw_rawz(&m, "}\nCancelled.");
    tl_result(t, out, id, m.p ? m.p : "", m.n, 0);
    jw_free(&m);
    free(jid);
}

static void cron_list(cl_tools *t, cl_tasks *k, jw *out, const char *id)
{
    jw m;
    int i;
    jw_init(&m);
    jw_rawz(&m, "{\"jobs\":[");
    for (i = 0; i < k->nc; i++) {
        const cron_job *j = &k->c[i];
        char hs[96];
        human(j->expr, hs, sizeof(hs));
        if (i)
            jw_raw(&m, ",", 1);
        jw_rawz(&m, "{\"id\":");
        jw_strz(&m, j->id);
        jw_rawz(&m, ",\"cron\":");
        jw_strz(&m, j->expr);
        jw_rawz(&m, ",\"humanSchedule\":");
        jw_strz(&m, hs);
        jw_rawz(&m, ",\"prompt\":");
        jw_strz(&m, j->prompt);
        jw_rawz(&m, ",\"recurring\":");
        jw_rawz(&m, j->recurring ? "true" : "false");
        jw_rawz(&m, ",\"durable\":");
        jw_rawz(&m, j->durable ? "true" : "false");
        jw_raw(&m, "}", 1);
    }
    jw_rawz(&m, "]}");
    tl_result(t, out, id, m.p ? m.p : "", m.n, 0);
    jw_free(&m);
}

/* "20 minutes", "1 minute 30 seconds", "1 hour" */
void tasks_span_text(long s, char *out, long cap)
{
    char num[16];
    long h = s / 3600, m = (s % 3600) / 60, sec = s % 60;
    out[0] = 0;
    if (h) {
        cl_ltoa(h, num);
        cl_cat(out, num, cap);
        cl_cat(out, h == 1 ? " hour" : " hours", cap);
    }
    if (m) {
        if (out[0])
            cl_cat(out, " ", cap);
        cl_ltoa(m, num);
        cl_cat(out, num, cap);
        cl_cat(out, m == 1 ? " minute" : " minutes", cap);
    }
    if (sec || !out[0]) {
        if (out[0])
            cl_cat(out, " ", cap);
        cl_ltoa(sec, num);
        cl_cat(out, num, cap);
        cl_cat(out, sec == 1 ? " second" : " seconds", cap);
    }
}

/* ScheduleWakeup: the next iteration of a self-paced /loop, or stop */
static void schedule_wakeup(cl_tools *t, cl_tasks *k, jw *out, const char *id, jv in)
{
    char *prompt = tl_prop(in, "prompt", 0), *reason = tl_prop(in, "reason", 0), err[200], jid[12], span[64];
    long now = t->sys->now ? t->sys->now(t->sys->u) : -1, asked, delay;
    int stop = tl_bool(in, "stop"), noop = tl_bool(in, "noop");
    jv v;
    jw m;
    jw_init(&m);
    if (!prompt || !reason) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    if (t->show)
        t->show(t->u, "ScheduleWakeup", stop ? "stop" : reason);
    if (stop) {
        int had = tasks_wakeup_cancel(k);
        k->lt.called = k->lt.stopped = 1;
        k->lt.noop = 0;
        jw_rawz(&m, had ? "Loop stopped: the pending wakeup is cancelled." : "Loop stopped (no wakeup was pending).");
        jw_rawz(&m, " Tell the user the loop's outcome in your reply: no later iteration will.");
        tl_result(t, out, id, m.p ? m.p : "", m.n, 0);
        goto done;
    }
    if (!prompt[0]) {
        tl_error(t, out, id, "prompt is required: the /loop input to run at the wakeup (\"/loop <input>\"), or "
                             "stop: true to end the loop", 0);
        goto done;
    }
    if (!json_get(in, "delaySeconds", &v) || json_type(v) != J_NUM) {
        tl_error(t, out, id, "delaySeconds is required: seconds until the next iteration (60 to 3600)", 0);
        goto done;
    }
    if (now < 0) {
        tl_error(t, out, id, "the machine's clock is not available: nothing can be scheduled", 0);
        goto done;
    }
    asked = json_long(v, LOOP_MIN_S);
    delay = asked < LOOP_MIN_S ? LOOP_MIN_S : asked > LOOP_MAX_S ? LOOP_MAX_S : asked;
    if (tasks_wakeup_set(k, delay, prompt, now, jid, err, sizeof(err))) {
        k->lt.called = k->lt.stopped = 1;   /* nothing to fall back on: the loop is over */
        tl_error(t, out, id, err, 0);
        goto done;
    }
    k->lt.called = 1;
    k->lt.stopped = 0;
    k->lt.noop = noop;
    cl_copy(k->lt.reason, reason, sizeof(k->lt.reason));
    tasks_span_text(delay, span, sizeof(span));
    jw_rawz(&m, "Next /loop wakeup in ");
    jw_rawz(&m, span);
    if (delay != asked)
        jw_rawz(&m, asked < LOOP_MIN_S ? " (the shortest wait is 1 minute)" : " (the longest wait is 1 hour)");
    if (reason[0]) {
        jw_rawz(&m, ": ");
        jw_rawz(&m, reason);
    }
    jw_rawz(&m, "\n{\"id\":");
    jw_strz(&m, jid);
    jw_rawz(&m, ",\"delaySeconds\":");
    jw_long(&m, delay);
    jw_rawz(&m, ",\"noop\":");
    jw_rawz(&m, noop ? "true" : "false");
    jw_rawz(&m, "}\nIt fires between turns while the session is open and idle; Esc cancels it. End the turn with "
                "a short visible update: what this iteration did and when the next one runs.");
    tl_result(t, out, id, m.p ? m.p : "", m.n, 0);
done:
    jw_free(&m);
    free(prompt);
    free(reason);
}

void tasks_run(cl_tools *t, int tool, jw *out, const char *id, jv in)
{
    cl_tasks *k = t->tasks;
    if (!k) {
        tl_error(t, out, id, "out of memory", 0);
        return;
    }
    switch (tool) {
    case T_TASK_CREATE:
        task_create(t, k, out, id, in);
        break;
    case T_TASK_GET:
        task_get(t, k, out, id, in);
        break;
    case T_TASK_LIST:
        task_list(t, k, out, id);
        break;
    case T_TASK_UPDATE:
        task_update(t, k, out, id, in);
        break;
    case T_CRON_CREATE:
        cron_create(t, k, out, id, in);
        break;
    case T_CRON_DELETE:
        cron_delete(t, k, out, id, in);
        break;
    case T_CRON_LIST:
        cron_list(t, k, out, id);
        break;
    case T_SCHEDULE_WAKEUP:
        schedule_wakeup(t, k, out, id, in);
        break;
    }
}
