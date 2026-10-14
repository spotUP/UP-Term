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
    jw_init(&a->usage_raw);
    jw_init(&a->rewrite);
}

void llm_acc_model(llm_acc *a, const char *id, const long price[4])
{
    int i;
    cl_copy(a->cfg_model, id, sizeof(a->cfg_model));
    for (i = 0; i < 4; i++)
        a->price[i] = price ? price[i] : 0;
}

void llm_acc_fallbacks(llm_acc *a, const llm_fallback *fb, int n)
{
    a->nfb = n > LLM_FALLBACKS ? LLM_FALLBACKS : n < 0 ? 0 : n;
    if (a->nfb)
        memcpy(a->fb, fb, (size_t)a->nfb * sizeof(*fb));
}

void llm_acc_level(llm_acc *a, const char *level)
{
    cl_copy(a->level, level ? level : "", sizeof(a->level));
}

void llm_acc_thinking(llm_acc *a, const char *level)
{
    cl_copy(a->tlevel, level ? level : "", sizeof(a->tlevel));
}

void llm_acc_duration(llm_acc *a, long ms)
{
    a->duration_ms = ms < 0 ? 0 : ms;
    a->has_duration = 1;
}

/* tokens * price / 1e6 in micro-dollars without leaving 32 bits */
static long cost_of(long tokens, long price)
{
    unsigned long t = tokens > 0 ? (unsigned long)tokens : 0, p = price > 0 ? (unsigned long)price : 0;
    unsigned long q = t / 1000, r = t % 1000;
    return (long)(q * (p / 1000) + q * (p % 1000) / 1000 + r * (p / 1000) / 1000);
}

void llm_acc_meta(const llm_acc *a, llm_meta *m)
{
    memset(m, 0, sizeof(*m));
    m->api = a->api;
    m->provider = a->provider;
    m->model = a->cfg_model[0] ? a->cfg_model : a->model;
    m->level = a->level[0] ? a->level : 0;
    m->thinking_level = a->tlevel[0] ? a->tlevel : 0;
    m->duration_ms = a->duration_ms;
    m->has_duration = a->has_duration;
    m->response_id = a->id[0] ? a->id : 0;
    if (a->level_srv[0] && !m->level)
        m->level = a->level_srv;
    if (a->usage_raw.n) {
        m->usage_json = a->usage_raw.p;
        m->usage_json_n = a->usage_raw.n;
    }
    if (a->rewrite.n) {
        m->rewrite = a->rewrite.p;
        m->rewrite_n = a->rewrite.n;
        m->rewrite_ts = a->rewrite_ts;
    }
    m->pi_order = !strcmp(a->api ? a->api : "", "pi-messages") ? (a->meta_seen ? 2 : 1) : 0;
    m->raw_stop = a->stop_raw[0] ? a->stop_raw : 0;
    m->response_model = a->cfg_model[0] && a->model[0] && strcmp(a->cfg_model, a->model) ? a->model : 0;
    m->usage = a->usage;
    if (a->cfg_model[0] && !a->have_cost) {
        const long *price = a->price;
        int i;
        if (m->response_model)          /* the server's model, when the list allows it: its own price */
            for (i = 0; i < a->nfb; i++)
                if (a->fb[i].model && !strcmp(a->fb[i].model, a->model) && a->fb[i].provider && a->provider &&
                    !strcmp(a->fb[i].provider, a->provider)) {
                    price = a->fb[i].cost;
                    break;
                }
        m->usage.cost[0] = cost_of(m->usage.input, price[0]);
        m->usage.cost[1] = cost_of(m->usage.output, price[1]);
        m->usage.cost[2] = cost_of(m->usage.cache_read, price[2]);
        m->usage.cost[3] = cost_of(m->usage.cache_write, price[3]);
    }
}

void llm_acc_free(llm_acc *a)
{
    int i;
    jw_free(&a->usage_raw);
    jw_free(&a->rewrite);
    for (i = 0; i < a->nb; i++) {
        jw_free(&a->b[i].a);
        jw_free(&a->b[i].sig);
        jw_free(&a->b[i].raw);
    }
    free(a->b);
    a->b = 0;
    a->nb = a->cap = 0;
    free(a->err);
    a->err = 0;
}

static void fail(llm_acc *a, int stop, const char *msg)
{
    if (a->failed)
        return;                 /* the first reason is the one told */
    a->failed = 1;
    a->stop = stop;
    free(a->err);
    a->err = (char *)malloc(LLM_ERR_MAX);
    if (a->err)
        cl_copy(a->err, msg, LLM_ERR_MAX);
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

/* ---- parseStreamingJson: the arguments of a tool call that did not arrive whole ----
 * A port of the `partial-json` package pi-ai 1.1.0 uses (parse with Allow.ALL, node_modules/partial-json/dist/index.js:
 * parseAny, parseStr, parseObj, parseArr, parseNum) as utils/json-parse.js:parseStreamingJson calls it. The same rules:
 * an object that ends early is closed after the last member that parsed; a half-written key, string escape or number
 * drops that member (a string keeps what it has: "S:Star"), a half-written true / false / null is completed, and an
 * exception inside a member only costs that member. The value is written as the token's own text (pi writes
 * JSON.stringify of the value, so a number like 1e2 or a \u escape reads differently, equal as JSON); a repeated key is
 * kept twice (pi keeps the last value in the first position). Anything that does not begin with '{' is {}. */
typedef struct pjs {
    const char *s;
    long n, i;
    jw *w;
} pjs;

#define PJ_THROW (-1)

/* JavaScript's String.prototype.substring: arguments clamped to 0..n and swapped when the start is after the end */
static void js_sub(const pjs *p, long a, long b, long *from, long *len)
{
    long t;
    if (a < 0)
        a = 0;
    if (b < 0)
        b = 0;
    if (a > p->n)
        a = p->n;
    if (b > p->n)
        b = p->n;
    if (a > b) {
        t = a;
        a = b;
        b = t;
    }
    *from = a;
    *len = b - a;
}

static long last_index_of(const pjs *p, char c)
{
    long i;
    for (i = p->n - 1; i >= 0; i--)
        if (p->s[i] == c)
            return i;
    return -1;
}

/* JSON.parse of a string literal: quotes included, no raw control character, only JSON's escapes */
static int json_str_ok(const char *t, long n)
{
    long i;
    if (n < 2 || t[0] != '"' || t[n - 1] != '"')
        return 0;
    for (i = 1; i < n - 1; i++) {
        unsigned char c = (unsigned char)t[i];
        if (c < 0x20 || c == '"')
            return 0;
        if (c == '\\') {
            char e;
            if (++i >= n - 1)
                return 0;
            e = t[i];
            if (e == 'u') {
                int k;
                for (k = 1; k <= 4; k++) {
                    char h = i + k < n - 1 ? t[i + k] : 0;
                    if (!((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') || (h >= 'A' && h <= 'F')))
                        return 0;
                }
                i += 4;
            } else if (!strchr("\"\\/bfnrt", e))
                return 0;
        }
    }
    return 1;
}

/* JSON.parse of a number (JSON whitespace around it is allowed): the trimmed span in *from / *len */
static int json_num_ok(const char *t, long n, long *from, long *len)
{
    long i = 0, e = n;
    while (i < e && strchr(" \n\r\t", t[i]))
        i++;
    while (e > i && strchr(" \n\r\t", t[e - 1]))
        e--;
    *from = i;
    *len = e - i;
    if (i < e && t[i] == '-')
        i++;
    if (i >= e)
        return 0;
    if (t[i] == '0')
        i++;
    else if (t[i] >= '1' && t[i] <= '9')
        while (i < e && t[i] >= '0' && t[i] <= '9')
            i++;
    else
        return 0;
    if (i < e && t[i] == '.') {
        long d = ++i;
        while (i < e && t[i] >= '0' && t[i] <= '9')
            i++;
        if (i == d)
            return 0;
    }
    if (i < e && (t[i] == 'e' || t[i] == 'E')) {
        long d;
        i++;
        if (i < e && (t[i] == '+' || t[i] == '-'))
            i++;
        d = i;
        while (i < e && t[i] >= '0' && t[i] <= '9')
            i++;
        if (i == d)
            return 0;
    }
    return i == e;
}

static void pj_blank(pjs *p)
{
    while (p->i < p->n && strchr(" \n\r\t", p->s[p->i]))
        p->i++;
}

static int pj_any(pjs *p);

/* parseStr: the string at p->i is written (quotes included) */
static int pj_str(pjs *p)
{
    long start = p->i, from, len, bs;
    int esc = 0;
    jw t;
    p->i++;
    while (p->i < p->n && (p->s[p->i] != '"' || (esc && p->s[p->i - 1] == '\\'))) {
        esc = p->s[p->i] == '\\' ? !esc : 0;
        p->i++;
    }
    if (p->i < p->n && p->s[p->i] == '"') {
        p->i++;
        js_sub(p, start, p->i - esc, &from, &len);
        if (!json_str_ok(p->s + from, len))
            return PJ_THROW;
        jw_raw(p->w, p->s + from, len);
        return 0;
    }
    /* unterminated (Allow.STR): what there is, closed; if that is no string (a half escape), up to the last backslash */
    jw_init(&t);
    js_sub(p, start, p->i - esc, &from, &len);
    jw_raw(&t, p->s + from, len);
    jw_raw(&t, "\"", 1);
    if (!t.oom && json_str_ok(t.p, t.n)) {
        jw_raw(p->w, t.p, t.n);
        jw_free(&t);
        return 0;
    }
    jw_reset(&t);
    bs = last_index_of(p, '\\');
    js_sub(p, start, bs, &from, &len);
    jw_raw(&t, p->s + from, len);
    jw_raw(&t, "\"", 1);
    if (!t.oom && json_str_ok(t.p, t.n)) {
        jw_raw(p->w, t.p, t.n);
        jw_free(&t);
        return 0;
    }
    jw_free(&t);
    return PJ_THROW;
}

/* parseNum (a number inside an object or array) */
static int pj_num(pjs *p)
{
    long start = p->i, from, len, f2, l2, e;
    if (p->i < p->n && p->s[p->i] == '-')
        p->i++;
    while (p->i < p->n && !strchr(",]}", p->s[p->i]))
        p->i++;
    if (json_num_ok(p->s + start, p->i - start, &f2, &l2)) {
        jw_raw(p->w, p->s + start + f2, l2);
        return 0;
    }
    /* JSON.parse(jsonString.substring(start, jsonString.lastIndexOf("e"))): the last 'e' of the whole text */
    e = last_index_of(p, 'e');
    js_sub(p, start, e, &from, &len);
    if (json_num_ok(p->s + from, len, &f2, &l2)) {
        jw_raw(p->w, p->s + from + f2, l2);
        return 0;
    }
    return PJ_THROW;
}

/* a literal, or its start when the text ends inside it (Allow.NULL / BOOL / INFINITY / NAN) */
static int pj_lit(pjs *p, const char *lit, const char *out, long minleft)
{
    long ll = (long)strlen(lit), left = p->n - p->i;
    if ((left >= ll && !strncmp(p->s + p->i, lit, (size_t)ll)) ||
        (left < ll && left > minleft && !strncmp(p->s + p->i, lit, (size_t)left))) {
        p->i += ll;
        jw_rawz(p->w, out);
        return 1;
    }
    return 0;
}

static int pj_obj(pjs *p)
{
    int first = 1;
    jw_rawz(p->w, "{");
    p->i++;
    pj_blank(p);
    while (p->i >= p->n || p->s[p->i] != '}') {
        long mark = p->w->n;
        pj_blank(p);
        if (p->i >= p->n)
            break;
        if (!first)
            jw_rawz(p->w, ",");
        if (pj_str(p)) {
            p->w->n = mark;
            break;
        }
        jw_rawz(p->w, ":");
        pj_blank(p);
        p->i++;                         /* the colon, whatever it is */
        if (pj_any(p)) {
            p->w->n = mark;
            break;
        }
        first = 0;
        pj_blank(p);
        if (p->i < p->n && p->s[p->i] == ',')
            p->i++;
    }
    if (p->w->p)
        p->w->p[p->w->n] = 0;
    jw_rawz(p->w, "}");
    if (p->i < p->n && p->s[p->i] == '}')
        p->i++;
    return 0;
}

static int pj_arr(pjs *p)
{
    int first = 1;
    jw_rawz(p->w, "[");
    p->i++;
    while (p->i >= p->n || p->s[p->i] != ']') {
        long mark = p->w->n;
        if (!first)
            jw_rawz(p->w, ",");
        if (pj_any(p)) {
            p->w->n = mark;
            break;
        }
        first = 0;
        pj_blank(p);
        if (p->i < p->n && p->s[p->i] == ',')
            p->i++;
    }
    if (p->w->p)
        p->w->p[p->w->n] = 0;
    jw_rawz(p->w, "]");
    if (p->i < p->n && p->s[p->i] == ']')
        p->i++;
    return 0;
}

static int pj_any(pjs *p)
{
    pj_blank(p);
    if (p->i >= p->n)
        return PJ_THROW;
    switch (p->s[p->i]) {
    case '"':
        return pj_str(p);
    case '{':
        return pj_obj(p);
    case '[':
        return pj_arr(p);
    default:
        break;
    }
    if (pj_lit(p, "null", "null", 0) || pj_lit(p, "true", "true", 0) || pj_lit(p, "false", "false", 0) ||
        pj_lit(p, "Infinity", "null", 0) || pj_lit(p, "-Infinity", "null", 1) || pj_lit(p, "NaN", "null", 0))
        return 0;
    return pj_num(p);
}

/* w holds the text so far of a tool call's arguments; afterwards the object parseStreamingJson makes of it */
static void salvage(jw *w)
{
    pjs p;
    jw o;
    long a = 0, e = w->n;
    while (a < e && strchr(" \n\r\t", w->p[a]))
        a++;
    while (e > a && strchr(" \n\r\t", w->p[e - 1]))
        e--;
    jw_init(&o);
    if (e > a && w->p[a] == '{') {
        p.s = w->p + a;
        p.n = e - a;
        p.i = 0;
        p.w = &o;
        pj_obj(&p);
    }
    jw_reset(w);
    if (!o.oom && o.n)
        jw_raw(w, o.p, o.n);
    else
        jw_rawz(w, "{}");
    jw_free(&o);
}

/* tool calls whose arguments did not parse: kept and completed when the
 * answer stopped at its length, else the message is an error */
static void settle(llm_acc *a)
{
    int i;
    for (i = 0; i < a->nb; i++) {
        llm_ablock *b = &a->b[i];
        if (!b->bad)
            continue;
        if (a->stop == LS_LENGTH && !a->failed) {
            salvage(&b->a);
            b->bad = 0;
        } else {
            char m[200];
            cl_copy(m, "the arguments of tool call ", sizeof(m));
            cl_cat(m, b->name, sizeof(m));
            cl_cat(m, " are not valid JSON", sizeof(m));
            if (!a->failed)
                a->args_failed = 1;
            fail(a, LS_ERROR, m);
            return;
        }
    }
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
        if (e->type == LE_SIG_DELTA && e->replace)
            jw_reset(&b->sig);
        if (e->type == LE_DELTA && e->replace)
            jw_reset(&b->a);        /* pi-messages: the *_end event's content is the block's text (or arguments) whatever the deltas said */
        if (e->type == LE_SIG_DELTA && e->redacted)
            b->redacted = 1;        /* bedrock: the first encrypted reasoning chunk makes the block redacted */
        jw_raw(e->type == LE_DELTA ? &b->a : &b->sig, e->p, e->n);
        break;
    case LE_BLOCK_END:
        b = find(a, e->index);
        if (!b) {
            fail(a, LS_ERROR, "the stream ended a block it had not opened");
            break;
        }
        b->done = 1;
        if (e->id)
            copyn(b->id, sizeof(b->id), e->id);         /* pi-messages' toolcall_end: the call as the gateway finished it */
        if (e->name)
            copyn(b->name, sizeof(b->name), e->name);
        if (b->kind == LB_TOOLCALL && !args_ok(b))
            b->bad = 1;         /* settled once the stop reason is known */
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
        if (e->has_w1h) {
            a->usage.cache_write_1h = e->cache_w1h;
            a->usage.has_1h = 1;
        }
        if (e->has_total) {
            a->usage.total = e->total;
            a->usage.has_total = 1;
        }
        if (e->has_reasoning) {
            a->usage.reasoning = e->reasoning;
            a->usage.has_reasoning = 1;
        }
        if (e->has_cost) {
            int k;
            for (k = 0; k < 4; k++)
                a->usage.cost[k] = e->cost[k];
            a->have_cost = 1;
        }
        break;
    case LE_META:
        a->meta_seen = 1;
        if (e->p && e->n > 0) {
            long k = e->n < (long)sizeof(a->id) - 1 ? e->n : (long)sizeof(a->id) - 1;
            memcpy(a->id, e->p, (size_t)k);
            a->id[k] = 0;
        }
        if (e->level)
            copyn(a->level_srv, sizeof(a->level_srv), e->level);
        if (e->raw && e->rn > 0) {
            jw_reset(&a->usage_raw);
            jw_raw(&a->usage_raw, e->raw, e->rn);
        }
        if (e->rewrite && e->rewrite_n > 0) {
            jw_reset(&a->rewrite);
            jw_raw(&a->rewrite, e->rewrite, e->rewrite_n);
            a->rewrite_ts = llm_wall();
        }
        break;
    case LE_STOP:
        if (e->stop == LS_ERROR) {
            static char m[LLM_ERR_MAX];
            fail(a, LS_ERROR, ev_text(e, m, sizeof(m)) ? m : "error");  /* pi-ai: an error stop ends as an error */
        } else if (!a->failed)
            a->stop = e->stop;
        copyn(a->stop_raw, sizeof(a->stop_raw), e->stop_raw);
        break;
    case LE_DONE:
        a->done = 1;
        if (!a->failed && !a->stop)
            a->stop = e->stop ? e->stop : LS_STOP;
        settle(a);
        break;
    case LE_ERROR:
        {
            static char m[LLM_ERR_MAX];
            fail(a, e->stop == LS_ABORTED ? LS_ABORTED : LS_ERROR,
                 ev_text(e, m, sizeof(m)) ? m : e->err_type ? e->err_type : "error");
        }
        if (e->stop_raw)
            copyn(a->stop_raw, sizeof(a->stop_raw), e->stop_raw);
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
    int i, first = 1, *ord, cut = (a->failed && !a->args_failed) || !a->done, fresh;
    /* an answer that ended in an error or was cut off keeps the tool calls it had begun, with the arguments so far
     * (pi: every provider's catch block deletes only its scratch fields); a call of a finished answer must be whole */
    settle(a);
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
    /* pi-messages: a failure that is not the server's own error event is a message made anew (api/pi-messages.ts createErrorEvent):
     * no content, no usage, no ids */
    fresh = !strcmp(a->api ? a->api : "", "pi-messages") && !a->meta_seen && (a->failed || !a->done);
    for (i = 0; i < a->nb && !fresh; i++) {
        const llm_ablock *b = &a->b[ord[i]];
        jw part;
        int partial = b->kind == LB_TOOLCALL && (!b->done || !args_ok(b));
        if (partial && !cut)
            continue;           /* half a call is no call */
        if (b->kind == LB_TEXT && !b->a.n)
            continue;
        if (!first)
            jw_raw(&c, ",", 1);
        first = 0;
        switch (b->kind) {
        case LB_TEXT:
            llm_b_text(&c, b->a.p, b->a.n, b->sig.n ? b->sig.p : 0);
            break;
        case LB_THINKING:
            llm_b_thinking(&c, b->a.p, b->a.n, b->sig.p, b->sig.n, b->redacted);
            break;
        case LB_TOOLCALL:
            if (partial) {
                jw_init(&part);
                jw_raw(&part, b->a.p, b->a.n);
                if (!args_ok(b))
                    salvage(&part);
                llm_b_toolcall(&c, b->id, b->name, part.p, part.n, b->sig.n ? b->sig.p : 0);
                jw_free(&part);
            } else
                llm_b_toolcall(&c, b->id, b->name, b->a.p, b->a.n, b->sig.n ? b->sig.p : 0);
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
    llm_acc_meta(a, &m);
    if (fresh) {
        memset(&m.usage, 0, sizeof(m.usage));
        m.usage_json = 0;
        m.usage_json_n = 0;
        m.response_id = 0;
        m.level = 0;
        m.rewrite = 0;
        m.rewrite_n = 0;
        m.raw_stop = 0;
        m.response_model = 0;
    }
    m.stop = a->failed ? a->stop : !a->done ? LS_ABORTED : a->stop ? a->stop : LS_STOP;
    m.error = a->failed ? (a->err ? a->err : "error") : 0;
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
