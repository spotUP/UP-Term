/* proc_core -- see proc_core.h. */
#include <string.h>
#include "proc_core.h"

/* ---- text without printf: C89 has no snprintf, and the host build refuses the other ---- */

/* v in decimal, right-aligned in width columns (more digits widen it), appended to out */
static void num(char *out, long v, int width)
{
    char d[24];
    int n = 0, neg = v < 0;
    unsigned long u = neg ? (unsigned long)-v : (unsigned long)v;
    do {
        d[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    if (neg)
        d[n++] = '-';
    out += strlen(out);
    while (width-- > n)
        *out++ = ' ';
    while (n > 0)
        *out++ = d[--n];
    *out = 0;
}

/* s appended, padded with blanks to width (cut to max characters, 0: no cut) */
static void str(char *out, const char *s, int width, int max)
{
    int n = (int)strlen(s);
    if (max > 0 && n > max)
        n = max;
    out += strlen(out);
    memcpy(out, s, (size_t)n);
    out += n;
    while (width-- > n)
        *out++ = ' ';
    *out = 0;
}

long pc_account(pc_task *t, int n, const pc_ring *r, unsigned long from, long *idle, long *gone)
{
    unsigned long to = r->count, k;
    int i;
    if (to - from > PC_RING)
        from = to - PC_RING; /* the ring kept only the newest */
    for (k = from; k != to; k++) {
        unsigned long v = r->slot[k & (PC_RING - 1)];
        if (v & PC_IDLE) {
            (*idle)++;
            continue;
        }
        for (i = 0; i < n && t[i].id != v; i++)
            ;
        if (i < n)
            t[i].samples++;
        else
            (*gone)++;
    }
    return (long)(to - from);
}

int pc_tenths(long part, long total)
{
    if (total <= 0)
        return 0;
    return (int)((part * 1000L + total / 2) / total);
}

void pc_pct(char *out, int tenths)
{
    out[0] = 0;
    num(out, tenths / 10, 3);
    str(out, ".", 0, 0);
    num(out, tenths % 10, 1);
}

static int name_cmp(const pc_task *a, const pc_task *b)
{
    int c = strcmp(pc_shown(a), pc_shown(b));
    if (c)
        return c;
    return a->id < b->id ? -1 : a->id > b->id;
}

/* insertion sort: a few dozen tasks */
static void sort(pc_task *t, int n, int (*before)(const pc_task *, const pc_task *))
{
    int i, j;
    pc_task x;
    for (i = 1; i < n; i++) {
        x = t[i];
        for (j = i; j > 0 && before(&x, &t[j - 1]); j--)
            t[j] = t[j - 1];
        t[j] = x;
    }
}

static int cpu_before(const pc_task *a, const pc_task *b)
{
    if (a->samples != b->samples)
        return a->samples > b->samples;
    return name_cmp(a, b) < 0;
}

static int ps_before(const pc_task *a, const pc_task *b)
{
    /* CLI processes first, by number; then the rest by name */
    if ((a->cli > 0) != (b->cli > 0))
        return a->cli > 0;
    if (a->cli != b->cli)
        return a->cli < b->cli;
    return name_cmp(a, b) < 0;
}

void pc_sort_cpu(pc_task *t, int n)
{
    sort(t, n, cpu_before);
}

void pc_sort_ps(pc_task *t, int n)
{
    sort(t, n, ps_before);
}

const char *pc_state(int state)
{
    switch (state) {
    case PC_RUN: return "running";
    case PC_READY: return "ready";
    case PC_WAIT: return "waiting";
    }
    return "other";
}

const char *pc_shown(const pc_task *t)
{
    return t->cmd[0] ? t->cmd : t->name;
}

/* out cut to width columns (the lines are Latin-1: a byte is a column) */
static void cut(char *out, int width)
{
    if (width > 0 && (int)strlen(out) > width)
        out[width] = 0;
}

static void cli_col(char *out, int cli)
{
    out[0] = 0;
    if (cli > 0)
        num(out, cli, 4);
    else
        strcpy(out, "   -");
}

void pc_ps_header(char *out, int cap, int width)
{
    (void)cap;
    strcpy(out, " CLI  PRI  STATE    TYPE     NAME");
    cut(out, width);
}

void pc_ps_row(char *out, int cap, const pc_task *t, int width)
{
    if (cap < 40 + PC_NAME)
        return;
    cli_col(out, t->cli);
    num(out, t->pri, 5);
    str(out, "  ", 0, 0);
    str(out, pc_state(t->state), 8, 0);
    str(out, " ", 0, 0);
    str(out, t->process ? "process" : "task", 8, 0);
    str(out, " ", 0, 0);
    str(out, pc_shown(t), 0, PC_NAME - 1);
    cut(out, width);
}

/* bytes as "123.4 MB" or "512 KB" */
static void mem(char *out, unsigned long b)
{
    const unsigned long mb = 1024UL * 1024UL;
    out[0] = 0;
    if (b >= mb) {
        unsigned long tenths = (b / 1024UL * 10UL + 512UL) / 1024UL; /* rounded, no overflow in 32 bits */
        num(out, (long)(tenths / 10UL), 0);
        str(out, ".", 0, 0);
        num(out, (long)(tenths % 10UL), 0);
        str(out, " MB", 0, 0);
    } else {
        num(out, (long)(b / 1024UL), 0);
        str(out, " KB", 0, 0);
    }
}

void pc_top_summary(char *out, int cap, int ntasks, long busy, long total, unsigned long chip, unsigned long fast,
                    int width)
{
    char b[8], i[8], c[24], f[24];
    int bt = pc_tenths(busy, total);
    pc_pct(b, bt);
    pc_pct(i, total > 0 ? 1000 - bt : 0);
    mem(c, chip);
    mem(f, fast);
    if (cap < 128)
        return;
    out[0] = 0;
    num(out, ntasks, 0);
    str(out, " tasks, CPU busy ", 0, 0);
    str(out, b + strspn(b, " "), 0, 0);
    str(out, " %, idle ", 0, 0);
    str(out, i + strspn(i, " "), 0, 0);
    str(out, " %, ", 0, 0);
    num(out, total, 0);
    str(out, " samples; free chip ", 0, 0);
    str(out, c, 0, 0);
    str(out, ", fast ", 0, 0);
    str(out, f, 0, 0);
    cut(out, width);
}

void pc_top_header(char *out, int cap, int width)
{
    (void)cap;
    strcpy(out, " CPU %  CLI  PRI  STATE    NAME");
    cut(out, width);
}

void pc_top_row(char *out, int cap, const pc_task *t, long total, int width)
{
    char p[8], cli[16];
    if (cap < 32 + PC_NAME)
        return;
    pc_pct(p, pc_tenths(t->samples, total));
    cli_col(cli, t->cli);
    out[0] = 0;
    str(out, " ", 0, 0);
    str(out, p, 0, 0);
    str(out, " ", 0, 0);
    str(out, cli, 0, 0);
    num(out, t->pri, 5);
    str(out, "  ", 0, 0);
    str(out, pc_state(t->state), 8, 0);
    str(out, " ", 0, 0);
    str(out, pc_shown(t), 0, PC_NAME - 1);
    cut(out, width);
}
