/* accum -- see accum.h. */
#include <stdlib.h>
#include <string.h>
#include "accum.h"
#include "../claude/util.h"

void llm_acc_init(llm_acc *a, const char *api, const char *provider)
{
    memset(a, 0, sizeof(*a));
    a->api = api;
    a->provider = provider;
}

void llm_acc_free(llm_acc *a)
{
    int i;
    for (i = 0; i < a->nb; i++) {
        jw_free(&a->b[i].a);
        jw_free(&a->b[i].sig);
        jw_free(&a->b[i].raw);
    }
    free(a->b);
    a->b = 0;
    a->nb = a->cap = 0;
}

static void fail(llm_acc *a, int stop, const char *msg)
{
    if (a->failed)
        return;                 /* the first reason is the one told */
    a->failed = 1;
    a->stop = stop;
    cl_copy(a->err, msg, sizeof(a->err));
}

/* the event's bytes as a string in m: their length */
static long ev_text(const llm_event *e, char *m, long cap)
{
    long n = e->n < cap - 1 ? e->n : cap - 1;
    if (!e->p || n < 0)
        n = 0;
    if (n)
        memcpy(m, e->p, (size_t)n);
    m[n] = 0;
    return n;
}

static llm_ablock *find(llm_acc *a, long index)
{
    int i;
    for (i = a->nb - 1; i >= 0; i--)
        if (a->b[i].index == index)
            return &a->b[i];
    return 0;
}

static void copyn(char *dst, long cap, const char *s)
{
    cl_copy(dst, s ? s : "", cap);
}

static void block_start(llm_acc *a, const llm_event *e)
{
    llm_ablock *b;
    if (a->nb == a->cap) {
        int c = a->cap ? a->cap * 2 : 8;
        llm_ablock *g = (llm_ablock *)realloc(a->b, (size_t)c * sizeof(llm_ablock));
        if (!g) {
            a->oom = 1;
            fail(a, LS_ERROR, "out of memory");
            return;
        }
        a->b = g;
        a->cap = c;
    }
    b = &a->b[a->nb++];
    memset(b, 0, sizeof(*b));
    jw_init(&b->a);
    jw_init(&b->sig);
    jw_init(&b->raw);
    b->index = e->index;
    b->kind = e->kind;
    b->redacted = e->redacted;
    if (e->p && e->n)
        jw_raw(&b->a, e->p, e->n);
    if (e->sig && e->sn)
        jw_raw(&b->sig, e->sig, e->sn);
    if (e->raw && e->rn)
        jw_raw(&b->raw, e->raw, e->rn);
    copyn(b->id, sizeof(b->id), e->id);
    copyn(b->name, sizeof(b->name), e->name);
    copyn(b->raw_type, sizeof(b->raw_type), e->raw_type);
}

static int args_ok(const llm_ablock *b)
{
    jv v;
    return !b->a.n || (json_parse(b->a.p, b->a.n, &v) == 0 && json_type(v) == J_OBJ);
}

void llm_acc_event(void *u, const llm_event *e)
{
    llm_acc *a = (llm_acc *)u;
    llm_ablock *b;
    a->events++;
    switch (e->type) {
    case LE_START:
        a->started = 1;
        copyn(a->model, sizeof(a->model), e->model);
        if (e->p)
            cl_copy(a->id, e->p, sizeof(a->id));
        break;
    case LE_BLOCK_START:
        block_start(a, e);
        break;
    case LE_DELTA:
    case LE_SIG_DELTA:
        b = find(a, e->index);
        if (!b || b->done) {
            fail(a, LS_ERROR, "the stream sent a delta for a block it had not opened");
            break;
        }
        jw_raw(e->type == LE_DELTA ? &b->a : &b->sig, e->p, e->n);
        break;
    case LE_BLOCK_END:
        b = find(a, e->index);
        if (!b) {
            fail(a, LS_ERROR, "the stream ended a block it had not opened");
            break;
        }
        b->done = 1;
        if (b->kind == LB_TOOLCALL && !args_ok(b)) {
            char m[200];
            cl_copy(m, "the arguments of tool call ", sizeof(m));
            cl_cat(m, b->name, sizeof(m));
            cl_cat(m, " are not valid JSON", sizeof(m));
            fail(a, LS_ERROR, m);
        }
        break;
    case LE_USAGE:
        if (e->in >= 0)
            a->usage.input = e->in;
        if (e->out >= 0)
            a->usage.output = e->out;
        if (e->cache_r >= 0)
            a->usage.cache_read = e->cache_r;
        if (e->cache_w >= 0)
            a->usage.cache_write = e->cache_w;
        break;
    case LE_STOP:
        if (e->stop == LS_ERROR) {
            char m[256];
            fail(a, LS_ERROR, ev_text(e, m, sizeof(m)) ? m : "error");  /* pi-ai: an error stop ends as an error */
        } else if (!a->failed)
            a->stop = e->stop;
        copyn(a->stop_raw, sizeof(a->stop_raw), e->stop_raw);
        break;
    case LE_DONE:
        a->done = 1;
        if (!a->failed && !a->stop)
            a->stop = e->stop ? e->stop : LS_STOP;
        break;
    case LE_ERROR:
        {
            char m[256];
            fail(a, e->stop == LS_ABORTED ? LS_ABORTED : LS_ERROR,
                 ev_text(e, m, sizeof(m)) ? m : e->err_type ? e->err_type : "error");
        }
        a->done = 1;
        break;
    }
}

llm_sink llm_acc_sink(llm_acc *a)
{
    llm_sink k;
    k.u = a;
    k.ev = llm_acc_event;
    return k;
}

/* a raw block with input that came in deltas: the provider's object with
 * "input" set to it */
static void raw_json(jw *w, const llm_ablock *b)
{
    jv o, k, v;
    jit it;
    int first = 1;
    if (!b->a.n || json_parse(b->raw.p ? b->raw.p : "", b->raw.n, &o) || json_type(o) != J_OBJ) {
        jw_raw(w, b->raw.p ? b->raw.p : "null", b->raw.n ? b->raw.n : 4);
        return;
    }
    jw_raw(w, "{", 1);
    json_iter(o, &it);
    while (json_next(&it, &k, &v)) {
        if (json_streq(k, "input"))
            continue;
        if (!first)
            jw_raw(w, ",", 1);
        first = 0;
        jw_raw(w, k.p, k.n);
        jw_raw(w, ":", 1);
        jw_raw(w, v.p, v.n);
    }
    jw_rawz(w, first ? "\"input\":" : ",\"input\":");
    jw_raw(w, b->a.p, b->a.n);
    jw_raw(w, "}", 1);
}

int llm_acc_message(llm_acc *a, llm_ts ts, jw *out)
{
    jw c;
    llm_meta m;
    int i, first = 1, *ord;
    ord = (int *)malloc((size_t)(a->nb ? a->nb : 1) * sizeof(int));
    if (!ord)
        return -1;
    for (i = 0; i < a->nb; i++)
        ord[i] = i;
    /* contentIndex order, stable (insertion sort: a few blocks) */
    for (i = 1; i < a->nb; i++) {
        int j = i, t = ord[i];
        while (j > 0 && a->b[ord[j - 1]].index > a->b[t].index) {
            ord[j] = ord[j - 1];
            j--;
        }
        ord[j] = t;
    }
    jw_init(&c);
    jw_raw(&c, "[", 1);
    for (i = 0; i < a->nb; i++) {
        const llm_ablock *b = &a->b[ord[i]];
        if (b->kind == LB_TOOLCALL && (!b->done || !args_ok(b)))
            continue;           /* half a call is no call */
        if (b->kind == LB_TEXT && !b->a.n)
            continue;
        if (!first)
            jw_raw(&c, ",", 1);
        first = 0;
        switch (b->kind) {
        case LB_TEXT:
            llm_b_text(&c, b->a.p, b->a.n, 0);
            break;
        case LB_THINKING:
            llm_b_thinking(&c, b->a.p, b->a.n, b->sig.p, b->sig.n, b->redacted);
            break;
        case LB_TOOLCALL:
            llm_b_toolcall(&c, b->id, b->name, b->a.p, b->a.n, 0);
            break;
        default:
            {
                jw r;
                jw_init(&r);
                raw_json(&r, b);
                llm_b_raw(&c, a->api, r.p, r.n);
                jw_free(&r);
            }
            break;
        }
    }
    jw_raw(&c, "]", 1);
    free(ord);
    memset(&m, 0, sizeof(m));
    m.api = a->api;
    m.provider = a->provider;
    m.model = a->model;
    m.usage = a->usage;
    m.stop = a->failed ? a->stop : !a->done ? LS_ABORTED : a->stop ? a->stop : LS_STOP;
    m.error = a->failed ? a->err : 0;
    llm_m_assistant(out, c.p, c.n, &m, ts);
    i = c.oom || out->oom ? -1 : 0;
    jw_free(&c);
    return i;
}

int llm_acc_tools(const llm_acc *a)
{
    int i, n = 0;
    for (i = 0; i < a->nb; i++)
        if (a->b[i].kind == LB_TOOLCALL && a->b[i].done && args_ok(&a->b[i]))
            n++;
    return n;
}

const llm_ablock *llm_acc_tool(const llm_acc *a, int k)
{
    int i, best = -1;
    long last = -1;
    /* in contentIndex order */
    for (;;) {
        best = -1;
        for (i = 0; i < a->nb; i++) {
            const llm_ablock *b = &a->b[i];
            if (b->kind == LB_TOOLCALL && b->done && args_ok(b) && b->index > last &&
                (best < 0 || b->index < a->b[best].index))
                best = i;
        }
        if (best < 0)
            return 0;
        if (!k--)
            return &a->b[best];
        last = a->b[best].index;
    }
}
