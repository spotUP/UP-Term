/* WAIT_CHAR packets by task: see waitset.h. */
#include "waitset.h"

static void take_out(waitset *s, int i)
{
    for (; i + 1 < s->n; i++)
        s->w[i] = s->w[i + 1];
    s->n--;
}

/* a - b in microseconds, 0 when b is not before a */
static long until(ws_time a, ws_time b)
{
    if (a.s < b.s || (a.s == b.s && a.us <= b.us))
        return 0;
    if (a.s - b.s > 2000)   /* a long is 32 bits: cap at ~33 minutes */
        return 2000000000L;
    return (long)(a.s - b.s) * 1000000L + (long)a.us - (long)b.us;
}

void *ws_drop_task(waitset *s, void *task)
{
    int i;
    for (i = 0; i < s->n; i++)
        if (s->w[i].task == task) {
            void *pk = s->w[i].pk;
            take_out(s, i);
            return pk;
        }
    return 0;
}

void *ws_add(waitset *s, void *pk, void *task, ws_time now, unsigned long micros)
{
    void *out = 0;
    ws_waiter *w;
    if (s->n == WS_MAX) {
        out = s->w[0].pk;
        take_out(s, 0);
    }
    w = &s->w[s->n++];
    w->pk = pk;
    w->task = task;
    w->due.s = now.s + micros / 1000000UL;
    w->due.us = now.us + micros % 1000000UL;
    if (w->due.us >= 1000000UL) {
        w->due.us -= 1000000UL;
        w->due.s++;
    }
    return out;
}

void *ws_take(waitset *s)
{
    void *pk;
    if (!s->n)
        return 0;
    pk = s->w[0].pk;
    take_out(s, 0);
    return pk;
}

void *ws_expired(waitset *s, ws_time now)
{
    int i;
    for (i = 0; i < s->n; i++)
        if (!until(s->w[i].due, now)) {
            void *pk = s->w[i].pk;
            take_out(s, i);
            return pk;
        }
    return 0;
}

long ws_next(const waitset *s, ws_time now)
{
    long best = -1;
    int i;
    for (i = 0; i < s->n; i++) {
        long t = until(s->w[i].due, now);
        if (best < 0 || t < best)
            best = t;
    }
    return best;
}
