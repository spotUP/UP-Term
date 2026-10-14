/* be_pi_messages -- pi-messages (amiga-pi L3): pi's own message protocol, the wire of the Radius gateway (pi 1.1.0
 * ai/src/api/pi-messages.ts). One POST of {model, context, options} to <baseUrl>/messages; the answer is an SSE stream of
 * serialized assistant-message events and a terminal done / error event.
 *   - the body: the transcript verbatim (no transformMessages: llm_prepare passes it through), the initial system message made
 *     of the prompt and the tools when the transcript does not start with one (utils/transcript.ts normalizeContext), then
 *     options {temperature, maxTokens, reasoning, cacheRetention, sessionId, toolChoice} in pi's order, unset ones left out;
 *   - the head: authorization, accept, content-type (the models.json headers follow);
 *   - the stream: the gateway's events are pi's own, so the decoder only maps them to the neutral ones. The *_end events carry the
 *     block as the gateway finished it (text, thinking and signature, the tool call) and replace what the deltas built; done
 *     and error carry the usage as it is (its own cost, extra keys), the response id, the provider's thinking level and the
 *     rewrite impact (LE_META);
 *   - a failed HTTP answer: "<status> <reason>: <error.message or the body> (<error.code>)" (formatPiMessagesResponseError).
 * Not ported (COMPATIBILITY.md): the pi_messages_response_failure diagnostic (it carries a JS stack), the `debug` option.
 * Portable C89, host-tested (amiga-pi tests/test_pi_radius.c). */
#include <stdlib.h>
#include <string.h>
#include "backend.h"
#include "registry.h"
#include "../claude/http.h"
#include "../claude/util.h"

long llm_feeds_pi_messages;

static char g_provider[64];     /* the provider of the request being built: the text of a stream that ends without a terminal event */

static long pm_head(const char *host, int port, int tls, const char *path, const char *key, const char *user_agent,
                    long body_len, char *out, long cap)
{
    http_hdr h[4];
    static char bearer[4200];
    bearer[0] = 0;
    if (key && *key) {
        cl_copy(bearer, "Bearer ", sizeof(bearer));
        cl_cat(bearer, key, sizeof(bearer));
    }
    h[0].name = "User-Agent";
    h[0].value = user_agent;
    h[1].name = "authorization";
    h[1].value = bearer;
    h[2].name = "accept";
    h[2].value = "text/event-stream";
    h[3].name = "content-type";
    h[3].value = "application/json";
    return http_post_head(host, port, tls, path, h, 4, body_len, out, cap);
}

/* the transcript starts with a system message */
static int starts_system(const llm_ctx *x)
{
    jv m, r;
    return x->nmsg > 0 && !json_parse(x->msgs[0], x->msg_n[0], &m) && json_get(m, "role", &r) && json_streq(r, "system");
}

/* createInitialSystemMessage: {role, content, toolsAdded?, timestamp: 0}; nothing when the prompt and the tools are both empty */
static int initial_system(const llm_ctx *x, jw *w)
{
    int has_sys = x->system && *x->system, has_tools = x->tools && x->tools_n > 2;
    if (!has_sys && !has_tools)
        return 0;
    jw_rawz(w, "{\"role\":\"system\",\"content\":");
    jw_strz(w, has_sys ? x->system : "");
    if (has_tools) {
        jw_rawz(w, ",\"toolsAdded\":");
        jw_raw(w, x->tools, x->tools_n);
    }
    jw_rawz(w, ",\"timestamp\":0}");
    return 1;
}

static void opt_key(jw *w, int *n, const char *k)
{
    jw_rawz(w, *n ? ",\"" : "\"");
    jw_rawz(w, k);
    jw_rawz(w, "\":");
    (*n)++;
}

static int pm_body(const llm_ctx *x, jw *out)
{
    const llm_model *m = x->model;
    int i, n = 0, lv;
    const char *ret;
    cl_copy(g_provider, m->provider ? m->provider : "", sizeof(g_provider));
    jw_rawz(out, "{\"model\":");
    jw_strz(out, m->id);
    jw_rawz(out, ",\"context\":{\"messages\":[");
    if (!starts_system(x) && initial_system(x, out) && x->nmsg > 0)
        jw_rawz(out, ",");
    for (i = 0; i < x->nmsg; i++) {
        if (i)
            jw_rawz(out, ",");
        jw_raw(out, x->msgs[i], x->msg_n[i]);
    }
    jw_rawz(out, "]},\"options\":{");
    if (x->has_temperature) {
        opt_key(out, &n, "temperature");
        llm_temp_text(out, x->temperature_micro);
    }
    if (x->max_tokens > 0) {
        opt_key(out, &n, "maxTokens");
        jw_long(out, x->max_tokens);
    }
    lv = x->thinking == LT_OFF ? LT_OFF : llm_think_clamp(m, x->thinking);
    if (lv != LT_OFF) {
        opt_key(out, &n, "reasoning");
        jw_strz(out, llm_think_name(lv));
    }
    /* resolveCacheRetention: the option, else PI_CACHE_RETENTION=long (llm_cfg_endpoint reads it), else the backend's default */
    ret = x->cache_retention && *x->cache_retention ? x->cache_retention : m->cache_long ? "long" : 0;
    if (ret) {
        opt_key(out, &n, "cacheRetention");
        jw_strz(out, ret);
    }
    if (x->session_id && *x->session_id) {
        opt_key(out, &n, "sessionId");
        jw_strz(out, x->session_id);
    }
    if (x->tool_choice && *x->tool_choice) {
        opt_key(out, &n, "toolChoice");
        if (!strncmp(x->tool_choice, "tool:", 5)) {
            jw_rawz(out, "{\"type\":\"function\",\"function\":{\"name\":");
            jw_strz(out, x->tool_choice + 5);
            jw_rawz(out, "}}");
        } else
            jw_strz(out, !strcmp(x->tool_choice, "any") ? "required" : x->tool_choice);
    }
    jw_rawz(out, "}}");
    return out->oom ? -1 : 0;
}

/* ---- the stream ---- */

#define PM_BLOCKS 64

typedef struct pm_blk {
    long index;
    int used;
    jw text;                    /* the text, thinking or argument JSON the deltas built so far */
} pm_blk;

typedef struct pm_dec {
    pm_blk b[PM_BLOCKS];
    int term;                   /* a terminal event came */
} pm_dec;

static void *pm_new(void)
{
    pm_dec *d = (pm_dec *)calloc(1, sizeof(*d));
    return d;
}

static void pm_free(void *dv)
{
    pm_dec *d = (pm_dec *)dv;
    int i;
    if (!d)
        return;
    for (i = 0; i < PM_BLOCKS; i++)
        if (d->b[i].used)
            jw_free(&d->b[i].text);
    free(d);
}

static pm_blk *find_blk(pm_dec *d, long index)
{
    int i;
    for (i = 0; i < PM_BLOCKS; i++)
        if (d->b[i].used && d->b[i].index == index)
            return &d->b[i];
    return 0;
}

static pm_blk *open_blk(pm_dec *d, long index)
{
    int i;
    pm_blk *b = find_blk(d, index);
    if (b) {
        jw_reset(&b->text);
        return b;
    }
    for (i = 0; i < PM_BLOCKS; i++)
        if (!d->b[i].used) {
            d->b[i].used = 1;
            d->b[i].index = index;
            jw_init(&d->b[i].text);
            return &d->b[i];
        }
    return 0;
}

static void emit(const llm_sink *k, const llm_event *e)
{
    if (k && k->ev)
        k->ev(k->u, e);
}

/* a string member as malloc'ed bytes (the unescaped text), 0 when absent or not a string */
static char *str_of(jv o, const char *key, long *len)
{
    jv v;
    if (!json_get(o, key, &v) || json_type(v) != J_STR)
        return 0;
    return json_strdup(v, len);
}

static int reason_stop(jv v)
{
    if (json_streq(v, "length"))
        return LS_LENGTH;
    if (json_streq(v, "toolUse"))
        return LS_TOOLUSE;
    return LS_STOP;
}

/* usage -> LE_USAGE, with the gateway's own cost */
static void send_usage(jv o, const llm_sink *k)
{
    jv u, v, c;
    llm_event e;
    static const char *const ck[4] = { "input", "output", "cacheRead", "cacheWrite" };
    int i;
    if (!json_get(o, "usage", &u) || json_type(u) != J_OBJ)
        return;
    llm_event_init(&e, LE_USAGE);
    if (json_get(u, "input", &v))
        e.in = json_long(v, 0);
    if (json_get(u, "output", &v))
        e.out = json_long(v, 0);
    if (json_get(u, "cacheRead", &v))
        e.cache_r = json_long(v, 0);
    if (json_get(u, "cacheWrite", &v))
        e.cache_w = json_long(v, 0);
    if (json_get(u, "totalTokens", &v) && json_type(v) == J_NUM) {
        e.total = json_long(v, 0);
        e.has_total = 1;
    }
    if (json_get(u, "reasoning", &v) && json_type(v) == J_NUM) {
        e.reasoning = json_long(v, 0);
        e.has_reasoning = 1;
    }
    if (json_get(u, "cacheWrite1h", &v) && json_type(v) == J_NUM) {
        e.cache_w1h = json_long(v, 0);
        e.has_w1h = 1;
    }
    if (json_get(u, "cost", &c) && json_type(c) == J_OBJ) {
        e.has_cost = 1;
        for (i = 0; i < 4; i++)
            e.cost[i] = json_get(c, ck[i], &v) ? llm_r_micro(v) : 0;
    }
    emit(k, &e);
}

/* done / error: the server-owned parts of the message, then the usage numbers */
static void send_meta(jv o, const llm_sink *k)
{
    jv v, u;
    llm_event e;
    char *id, *lvl;
    long idn = 0, ln = 0;
    send_usage(o, k);
    llm_event_init(&e, LE_META);
    id = str_of(o, "responseId", &idn);
    lvl = str_of(o, "providerThinkingLevel", &ln);
    e.p = id;
    e.n = idn;
    e.level = lvl;
    if (json_get(o, "usage", &u) && json_type(u) == J_OBJ) {
        e.raw = u.p;
        e.rn = u.n;
    }
    if (json_get(o, "rewrite", &v) && json_type(v) == J_OBJ) {
        e.rewrite = v.p;
        e.rewrite_n = v.n;
    }
    emit(k, &e);
    free(id);
    free(lvl);
}

/* a replace of the block's text by what the *_end event says, when the deltas built something else */
static void settle_text(pm_blk *b, const char *text, long n, const llm_sink *k)
{
    llm_event e;
    if (b->text.n == n && (n == 0 || !memcmp(b->text.p, text, (size_t)n)))
        return;
    llm_event_init(&e, LE_DELTA);
    e.index = b->index;
    e.p = text;
    e.n = n;
    e.replace = 1;
    emit(k, &e);
    jw_reset(&b->text);
    jw_raw(&b->text, text, n);
}

static void send_sig(long index, const char *sig, long n, int redacted, const llm_sink *k)
{
    llm_event e;
    llm_event_init(&e, LE_SIG_DELTA);
    e.index = index;
    e.p = sig;
    e.n = n;
    e.replace = 1;
    e.redacted = redacted;
    emit(k, &e);
}

static int pm_feed(void *dv, const char *event, const char *data, long n, const llm_sink *k)
{
    pm_dec *d = (pm_dec *)dv;
    jv ev, v, idx;
    char type[24];
    llm_event e;
    long index;
    pm_blk *b;
    (void)event;
    llm_feeds_pi_messages++;
    if (d->term)
        return 0;
    /* parsePiMessagesEvent: [DONE] is no event */
    if (n >= 6 && !memcmp(data, "[DONE]", 6))
        return 0;
    if (json_parse(data, n, &ev) || json_type(ev) != J_OBJ || !json_get(ev, "type", &v) || json_str(v, type, sizeof(type)) < 0)
        return -1;
    index = json_get(ev, "contentIndex", &idx) ? json_long(idx, -1) : -1;
    if (!strcmp(type, "start")) {
        llm_event_init(&e, LE_START);
        emit(k, &e);
    } else if (!strcmp(type, "text_start") || !strcmp(type, "thinking_start")) {
        if (index < 0 || !(b = open_blk(d, index)))
            return -1;
        llm_event_init(&e, LE_BLOCK_START);
        e.index = index;
        e.kind = type[0] == 't' && type[1] == 'e' ? LB_TEXT : LB_THINKING;
        emit(k, &e);
    } else if (!strcmp(type, "text_delta") || !strcmp(type, "thinking_delta") || !strcmp(type, "toolcall_delta")) {
        long dn = 0;
        char *s = str_of(ev, "delta", &dn);
        if (!s || !(b = find_blk(d, index))) {
            free(s);
            return -1;
        }
        jw_raw(&b->text, s, dn);
        llm_event_init(&e, LE_DELTA);
        e.index = index;
        e.p = s;
        e.n = dn;
        emit(k, &e);
        free(s);
    } else if (!strcmp(type, "text_end") || !strcmp(type, "thinking_end")) {
        long cn = 0, sn = 0;
        char *content = str_of(ev, "content", &cn), *sig = str_of(ev, "contentSignature", &sn);
        int redacted = json_get(ev, "redacted", &v) && json_type(v) == J_TRUE;
        if (!(b = find_blk(d, index))) {
            free(content);
            free(sig);
            return -1;
        }
        if (content)
            settle_text(b, content, cn, k);
        if (sig || redacted)
            send_sig(index, sig ? sig : "", sig ? sn : 0, redacted, k);
        llm_event_init(&e, LE_BLOCK_END);
        e.index = index;
        emit(k, &e);
        free(content);
        free(sig);
    } else if (!strcmp(type, "toolcall_start")) {
        char *id, *name;
        long idn = 0, nn = 0;
        if (index < 0 || !(b = open_blk(d, index)))
            return -1;
        id = str_of(ev, "id", &idn);
        name = str_of(ev, "toolName", &nn);
        llm_event_init(&e, LE_BLOCK_START);
        e.index = index;
        e.kind = LB_TOOLCALL;
        e.id = id ? id : "";
        e.name = name ? name : "";
        emit(k, &e);
        free(id);
        free(name);
    } else if (!strcmp(type, "toolcall_end")) {
        jv tc, args;
        char *id = 0, *name = 0, *tsig = 0;
        long tn = 0;
        if (!(b = find_blk(d, index)) || !json_get(ev, "toolCall", &tc) || json_type(tc) != J_OBJ)
            return -1;
        id = str_of(tc, "id", &tn);
        name = str_of(tc, "name", &tn);
        tsig = str_of(tc, "thoughtSignature", &tn);
        if (json_get(tc, "arguments", &args))
            settle_text(b, args.p, args.n, k);
        if (tsig)
            send_sig(index, tsig, tn, 0, k);
        llm_event_init(&e, LE_BLOCK_END);
        e.index = index;
        e.id = id;
        e.name = name;
        emit(k, &e);
        free(id);
        free(name);
        free(tsig);
    } else if (!strcmp(type, "done")) {
        d->term = 1;
        send_meta(ev, k);
        llm_event_init(&e, LE_STOP);
        e.stop = json_get(ev, "reason", &v) ? reason_stop(v) : LS_STOP;
        emit(k, &e);
        llm_event_init(&e, LE_DONE);
        e.stop = LS_NONE;
        emit(k, &e);
    } else if (!strcmp(type, "error")) {
        long mn = 0;
        char *msg = str_of(ev, "errorMessage", &mn);
        d->term = 1;
        send_meta(ev, k);
        llm_event_init(&e, LE_ERROR);
        e.stop = json_get(ev, "reason", &v) && json_streq(v, "aborted") ? LS_ABORTED : LS_ERROR;
        e.err_type = e.stop == LS_ABORTED ? "aborted" : "error";
        e.p = msg ? msg : "error";
        e.n = msg ? mn : 5;
        emit(k, &e);
        free(msg);
    }
    return 0;
}

/* the body ended and no terminal event came (pi-messages.ts: `${model.provider} stream ended without a terminal event`) */
static void pm_fin(void *dv, const llm_sink *k)
{
    pm_dec *d = (pm_dec *)dv;
    llm_event e;
    char msg[160];
    if (d->term)
        return;
    cl_copy(msg, g_provider, sizeof(msg));
    cl_cat(msg, " stream ended without a terminal event", sizeof(msg));
    llm_event_init(&e, LE_ERROR);
    e.stop = LS_ERROR;
    e.err_type = "error";
    e.p = msg;
    e.n = (long)strlen(msg);
    emit(k, &e);
}

/* formatPiMessagesResponseError: "<status> <statusText>: <error.message, else the body>[ (<error.code>)]" */
static void pm_http_err(const llm_http_fail *f, char *out, long cap)
{
    jv root, er, v;
    static char msg[LLM_ERR_MAX];
    char num[16], code[100];
    int have_msg = 0;
    code[0] = 0;
    if (f->body && f->bn > 0 && !json_parse(f->body, f->bn, &root) && json_type(root) == J_OBJ && json_get(root, "error", &er) &&
        json_type(er) == J_OBJ) {
        if (json_get(er, "message", &v) && json_type(v) == J_STR) {
            json_str(v, msg, sizeof(msg));
            have_msg = 1;
        }
        if (json_get(er, "code", &v) && json_type(v) == J_STR)
            json_str(v, code, sizeof(code));
    }
    if (!have_msg) {
        long n = f->bn < (long)sizeof(msg) - 1 ? f->bn : (long)sizeof(msg) - 1;
        if (n > 0)
            memcpy(msg, f->body, (size_t)n);
        else
            n = 0;
        msg[n] = 0;
    }
    cl_ltoa(f->status, num);
    cl_copy(out, num, cap);
    cl_cat(out, " ", cap);
    cl_cat(out, f->reason ? f->reason : "", cap);
    cl_cat(out, ": ", cap);
    cl_cat(out, msg, cap);
    if (code[0]) {
        cl_cat(out, " (", cap);
        cl_cat(out, code, cap);
        cl_cat(out, ")", cap);
    }
}

const llm_backend llm_be_pi_messages = {
    "pi-messages", "authorization", "/messages", pm_head, pm_body, 0, pm_new, pm_feed, pm_free, 0, 0, pm_fin, 0, pm_http_err, 0
};
