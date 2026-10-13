/* be_openai_chat -- the openai-completions backend (amiga-pi plan B6):
 * POST {baseUrl}/chat/completions with a Bearer key, the body from pi-ai
 * messages with pi-ai's four compat flags, and the decoder from the
 * chat.completion.chunk stream (data: JSON ... data: [DONE]) to the
 * neutral events. Wire details: pi f10993bc
 * packages/ai/src/api/openai-completions.ts (amiga-pi addendum A3 1, 2).
 * Portable C89, host-tested (tests/test_pi_llm.c). */
#include <stdlib.h>
#include <string.h>
#include "backend.h"
#include "registry.h"
#include "constrain.h"
#include "estimate.h"
#include "../claude/http.h"
#include "../claude/util.h"

long llm_feeds_openai;

static long o_head(const char *host, int port, int tls, const char *path, const char *key,
                   const char *user_agent, long body_len, char *out, long cap)
{
    http_hdr h[4];
    char bearer[600];
    bearer[0] = 0;
    if (key && *key) {
        cl_copy(bearer, "Bearer ", sizeof(bearer));
        cl_cat(bearer, key, sizeof(bearer));
    }
    h[0].name = "User-Agent";
    h[0].value = user_agent;
    h[1].name = "Accept";
    h[1].value = "text/event-stream";
    h[2].name = "Content-Type";
    h[2].value = "application/json";
    h[3].name = "Authorization";
    h[3].value = bearer;
    return http_post_head(host, port, tls, path, h, 4, body_len, out, cap);
}

/* ---- compat: the endpoint's defaults, then the model's explicit values ---- */

static int has(const char *s, const char *w)
{
    return s && strstr(s, w) != 0;
}

static int is(const char *s, const char *w)
{
    return s && !strcmp(s, w);
}

/* w (lower case) anywhere in s, ignoring case */
static int has_nocase(const char *s, const char *w)
{
    long n = (long)strlen(w);
    for (; s && *s; s++)
        if (cl_strnieq(s, w, n))
            return 1;
    return 0;
}

/* pi-ai's detectCompat (pi f10993bc openai-completions.ts:1593-1690) for
 * the four flags this backend writes: the provider name or the base URL
 * names a host that needs max_tokens or takes no developer role */
static llm_compat detect(const llm_model *m)
{
    const char *p = m->provider, *u = m->base_url, *id = m->id ? m->id : "";
    llm_compat c;
    int zai = is(p, "zai") || is(p, "zai-coding-cn") || has(u, "api.z.ai") || has(u, "open.bigmodel.cn");
    int together = is(p, "together") || has(u, "api.together.ai") || has(u, "api.together.xyz");
    int moonshot = is(p, "moonshotai") || is(p, "moonshotai-cn") || has(u, "api.moonshot.");
    int openrouter = is(p, "openrouter") || has(u, "openrouter.ai");
    int cf_workers = is(p, "cloudflare-workers-ai") || has(u, "api.cloudflare.com");
    int cf_gateway = is(p, "cloudflare-ai-gateway") || has(u, "gateway.ai.cloudflare.com");
    int nvidia = is(p, "nvidia") || has(u, "integrate.api.nvidia.com");
    int antling = is(p, "ant-ling") || has(u, "api.ant-ling.com");
    int cerebras = is(p, "cerebras") || has(u, "cerebras.ai");
    int deepseek = is(p, "deepseek") || has_nocase(u, "deepseek.com");
    int nonstd = nvidia || cerebras || is(p, "xai") || has(u, "api.x.ai") || together || has(u, "chutes.ai") ||
                 deepseek || zai || moonshot || is(p, "opencode") || has(u, "opencode.ai") || cf_workers ||
                 cf_gateway || antling;
    int or_dev = openrouter && (!strncmp(id, "anthropic/", 10) || !strncmp(id, "openai/", 7));
    c.store = !nonstd;
    c.developer_role = or_dev || (!nonstd && !openrouter);
    c.usage_in_streaming = 1;
    c.max_tokens_field = has(u, "chutes.ai") || deepseek || moonshot || cf_gateway || together || nvidia ||
                         antling || zai;
    c.tool_result_name = 0;
    c.reasoning_effort = !(is(p, "xai") || has(u, "api.x.ai") || zai || moonshot || together || cf_gateway || nvidia || antling);
    c.thinking_as_text = 0;
    return c;
}

/* the detected values, then the model's explicit ones (getCompat :1696) */
static llm_compat compat_of(const llm_model *m)
{
    llm_compat c = detect(m);
    if (m->compat.developer_role != LC_UNSET)
        c.developer_role = m->compat.developer_role;
    if (m->compat.store != LC_UNSET)
        c.store = m->compat.store;
    if (m->compat.usage_in_streaming != LC_UNSET)
        c.usage_in_streaming = m->compat.usage_in_streaming;
    if (m->compat.max_tokens_field != LC_UNSET)
        c.max_tokens_field = m->compat.max_tokens_field;
    if (m->compat.tool_result_name != LC_UNSET)
        c.tool_result_name = m->compat.tool_result_name;
    if (m->compat.reasoning_effort != LC_UNSET)
        c.reasoning_effort = m->compat.reasoning_effort;
    if (m->compat.thinking_as_text != LC_UNSET)
        c.thinking_as_text = m->compat.thinking_as_text;
    return c;
}

/* tool-call ids: llm_prepare (handoff.c) already made them the form this API takes */
static void w_id(jw *w, jv v)
{
    jw_raw(w, v.p, v.n);
}

/* the text blocks of content joined (a tool result's or an answer's text) */
static void w_text(jw *w, jv c)
{
    jit it;
    jv b, t, v;
    jw s;
    if (json_type(c) == J_STR) {
        jw_raw(w, c.p, c.n);
        return;
    }
    jw_init(&s);
    json_iter(c, &it);
    while (json_next(&it, 0, &b))
        if (json_get(b, "type", &t) && json_streq(t, "text") && json_get(b, "text", &v)) {
            long l;
            char *x = json_strdup(v, &l);
            if (x) {
                jw_raw(&s, x, l);
                free(x);
            }
        }
    jw_str(w, s.p ? s.p : "", s.n);
    jw_free(&s);
}

static int w_user(jw *w, jv c)
{
    jit it;
    jv b, t, v, m;
    int k = 0;
    if (json_type(c) == J_STR) {
        jw_raw(w, c.p, c.n);
        return 0;
    }
    jw_raw(w, "[", 1);
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        if (!json_get(b, "type", &t))
            return -1;
        jw_rawz(w, k++ ? "," : "");
        if (json_streq(t, "text") && json_get(b, "text", &v)) {
            jw_rawz(w, "{\"type\":\"text\",\"text\":");
            jw_raw(w, v.p, v.n);
            jw_raw(w, "}", 1);
        } else if (json_streq(t, "image") && json_get(b, "data", &v) && json_get(b, "mimeType", &m)) {
            char *d, *mm;
            long dl, ml;
            jw u;
            d = json_strdup(v, &dl);
            mm = json_strdup(m, &ml);
            jw_init(&u);
            jw_rawz(&u, "data:");
            jw_raw(&u, mm ? mm : "", mm ? ml : 0);
            jw_rawz(&u, ";base64,");
            jw_raw(&u, d ? d : "", d ? dl : 0);
            jw_rawz(w, "{\"type\":\"image_url\",\"image_url\":{\"url\":");
            jw_str(w, u.p, u.n);
            jw_rawz(w, "}}");
            jw_free(&u);
            free(d);
            free(mm);
        } else
            return -1;
    }
    jw_raw(w, "]", 1);
    return 0;
}

/* openai-completions convertMessages for an assistant message: the text as one string; thinking back as the
 * field its signature names (reasoning, reasoning_content, reasoning_text) or, requiresThinkingAsText, as plain
 * text before the answer. 0 when it has nothing this API can take */
static int w_assistant(jw *w, jv m, const llm_compat *cp)
{
    jv c, b, t, v, id, nm, args;
    jit it;
    jw text, think;
    char sig[32];
    int calls = 0, nthink = 0;
    if (!json_get(m, "content", &c) || json_type(c) != J_ARR)
        return 0;
    jw_init(&text);
    jw_init(&think);
    sig[0] = 0;
    json_iter(c, &it);
    while (json_next(&it, 0, &b))
        if (json_get(b, "type", &t) && json_streq(t, "text") && json_get(b, "text", &v)) {
            long l;
            char *x = json_strdup(v, &l);
            if (x) {
                jw_raw(&text, x, l);
                free(x);
            }
        } else if (json_get(b, "type", &t) && json_streq(t, "thinking") && json_get(b, "thinking", &v)) {
            long l, i;
            char *x = json_strdup(v, &l);
            int empty = 1;
            for (i = 0; x && i < l; i++)
                if (!(x[i] == ' ' || x[i] == '\t' || x[i] == '\n' || x[i] == '\r'))
                    empty = 0;
            if (x && !empty) {
                if (nthink++)
                    jw_rawz(&think, cp->thinking_as_text == 1 ? "\n\n" : "\n");
                jw_raw(&think, x, l);
                if (nthink == 1 && json_get(b, "thinkingSignature", &t) && json_type(t) == J_STR)
                    json_str(t, sig, sizeof(sig));
            }
            free(x);
        }
    jw_rawz(w, "{\"role\":\"assistant\",\"content\":");
    if (nthink && cp->thinking_as_text == 1) {
        jw_rawz(w, "[{\"type\":\"text\",\"text\":");
        jw_str(w, think.p, think.n);
        jw_raw(w, "}", 1);
        if (text.n) {
            jw_rawz(w, ",{\"type\":\"text\",\"text\":");
            jw_str(w, text.p, text.n);
            jw_raw(w, "}", 1);
        }
        jw_raw(w, "]", 1);
    } else if (text.n)
        jw_str(w, text.p, text.n);
    else
        jw_rawz(w, "null");
    if (nthink && cp->thinking_as_text != 1 &&
        (!strcmp(sig, "reasoning") || !strcmp(sig, "reasoning_content") || !strcmp(sig, "reasoning_text"))) {
        jw_rawz(w, ",\"");
        jw_rawz(w, sig);
        jw_rawz(w, "\":");
        jw_str(w, think.p, think.n);
    }
    jw_free(&text);
    jw_free(&think);
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        if (!json_get(b, "type", &t) || !json_streq(t, "toolCall") || !json_get(b, "id", &id) ||
            !json_get(b, "name", &nm))
            continue;
        jw_rawz(w, calls++ ? "," : ",\"tool_calls\":[");
        jw_rawz(w, "{\"id\":");
        w_id(w, id);
        jw_rawz(w, ",\"type\":\"function\",\"function\":{\"name\":");
        jw_raw(w, nm.p, nm.n);
        jw_rawz(w, ",\"arguments\":");
        if (json_get(b, "arguments", &args))
            jw_str(w, args.p, args.n);  /* the arguments travel as a JSON string */
        else
            jw_rawz(w, "\"{}\"");
        jw_rawz(w, "}}");
    }
    if (calls)
        jw_raw(w, "]", 1);
    jw_raw(w, "}", 1);
    return 1;
}

static int o_body(const llm_ctx *x, jw *w)
{
    const llm_model *md = x->model;
    llm_compat cp = compat_of(md);
    int i, k = 0;
    long maxt = llm_ctx_max_tokens(x);
    jw_rawz(w, "{\"model\":");
    jw_strz(w, md->id);
    jw_rawz(w, ",\"messages\":[");
    if (x->system && *x->system) {
        jw_rawz(w, md->reasoning && cp.developer_role ? "{\"role\":\"developer\",\"content\":"
                                                       : "{\"role\":\"system\",\"content\":");
        jw_strz(w, x->system);
        jw_raw(w, "}", 1);
        k++;
    }
    for (i = 0; i < x->nmsg; i++) {
        jv m, r, c, v;
        long mark = w->n;
        if (json_parse(x->msgs[i], x->msg_n[i], &m) || !json_get(m, "role", &r))
            return -1;
        jw_rawz(w, k ? "," : "");
        if (json_streq(r, "user")) {
            if (!json_get(m, "content", &c))
                return -1;
            jw_rawz(w, "{\"role\":\"user\",\"content\":");
            if (w_user(w, c))
                return -1;
            jw_raw(w, "}", 1);
        } else if (json_streq(r, "assistant")) {
            if (!w_assistant(w, m, &cp)) {
                w->n = mark;
                if (w->p)
                    w->p[w->n] = 0;
                continue;
            }
        } else if (json_streq(r, "toolResult")) {
            if (!json_get(m, "toolCallId", &v) || !json_get(m, "content", &c))
                return -1;
            /* openai-completions.ts:1424-1431: role, content, tool_call_id, then name */
            jw_rawz(w, "{\"role\":\"tool\",\"content\":");
            w_text(w, c);
            jw_rawz(w, ",\"tool_call_id\":");
            w_id(w, v);
            if (cp.tool_result_name && json_get(m, "toolName", &v)) {
                jw_rawz(w, ",\"name\":");
                jw_raw(w, v.p, v.n);
            }
            jw_raw(w, "}", 1);
        } else {
            w->n = mark;            /* system messages ride in the system prompt */
            if (w->p)
                w->p[w->n] = 0;
            continue;
        }
        k++;
    }
    jw_rawz(w, "],\"stream\":true");
    if (cp.usage_in_streaming)
        jw_rawz(w, ",\"stream_options\":{\"include_usage\":true}");
    if (cp.store)
        jw_rawz(w, ",\"store\":false");
    if (maxt > 0) {
        jw_rawz(w, cp.max_tokens_field ? ",\"max_tokens\":" : ",\"max_completion_tokens\":");
        jw_long(w, maxt);
    }
    if (md->reasoning && cp.reasoning_effort) {
        /* openai-completions: reasoning_effort = thinkingLevelMap[level] ?? level; off sends only a mapped value */
        int lv = llm_think_clamp(md, x->thinking);
        const llm_tmap *t = &md->thinking_map[lv];
        if (lv != LT_OFF) {
            jw_rawz(w, ",\"reasoning_effort\":");
            jw_strz(w, t->kind == LTM_STR ? t->val : llm_think_name(lv));
        } else if (t->kind == LTM_STR) {
            jw_rawz(w, ",\"reasoning_effort\":");
            jw_strz(w, t->val);
        }
    }
    if (x->tools && x->tools_n > 2) {
        jv ts, t, v;
        jit it;
        int n = 0, st;
        jw sch;
        if (json_parse(x->tools, x->tools_n, &ts) || json_type(ts) != J_ARR)
            return -1;
        jw_init(&sch);
        jw_rawz(w, ",\"tools\":[");
        json_iter(ts, &it);
        while (json_next(&it, 0, &t)) {
            if (!json_get(t, "name", &v))
                return -1;
            jw_rawz(w, n++ ? "," : "");
            jw_rawz(w, "{\"type\":\"function\",\"function\":{\"name\":");
            jw_raw(w, v.p, v.n);
            if (json_get(t, "description", &v)) {
                jw_rawz(w, ",\"description\":");
                jw_raw(w, v.p, v.n);
            }
            /* resolveJsonSchemaStrictSampling, then getJsonSchemaToolParameters; "strict" only where the model supports it */
            st = llm_tool_strict(t, md->compat.strict_tools == 1, LCS_NONE, &sch, 0, 0);
            if (st < 0) {
                jw_free(&sch);
                return -1;
            }
            jw_rawz(w, ",\"parameters\":");
            if (st == 1)
                jw_raw(w, sch.p, sch.n);
            else if (json_get(t, "parameters", &v))
                jw_raw(w, v.p, v.n);
            else
                jw_rawz(w, "{\"type\":\"object\"}");
            if (md->compat.strict_tools == 1)
                jw_rawz(w, st == 1 ? ",\"strict\":true" : ",\"strict\":false");
            jw_rawz(w, "}}");
        }
        jw_free(&sch);
        jw_raw(w, "]", 1);
    }
    jw_raw(w, "}", 1);
    return w->oom ? -1 : 0;
}

/* ---- the decoder ---- */

#define OC_TOOLS 16

typedef struct oc_dec {
    int started, done, bad;
    long next;                  /* the next contentIndex */
    long text, think;           /* the open text / thinking block's index, -1 none */
    long tool[OC_TOOLS];        /* tool_calls[i] -> contentIndex, -1 none */
    int stop;
    char stop_raw[32];
} oc_dec;

static void *o_new(void)
{
    oc_dec *d = (oc_dec *)malloc(sizeof(oc_dec));
    int i;
    if (!d)
        return 0;
    memset(d, 0, sizeof(*d));
    d->text = d->think = -1;
    for (i = 0; i < OC_TOOLS; i++)
        d->tool[i] = -1;
    return d;
}

static void o_free(void *d)
{
    free(d);
}

static void emit(const llm_sink *k, const llm_event *e)
{
    if (k && k->ev)
        k->ev(k->u, e);
}

static void end_block(const llm_sink *k, long *index)
{
    llm_event e;
    if (*index < 0)
        return;
    llm_event_init(&e, LE_BLOCK_END);
    e.index = *index;
    emit(k, &e);
    *index = -1;
}

static void end_all(oc_dec *d, const llm_sink *k)
{
    int i;
    end_block(k, &d->think);
    end_block(k, &d->text);
    for (i = 0; i < OC_TOOLS; i++)
        end_block(k, &d->tool[i]);
}

/* a text or thinking delta: the block opened first (the other kind closed) */
static int piece(oc_dec *d, const llm_sink *k, int kind, jv v, const char *field)
{
    llm_event e;
    long l, *open = kind == LB_TEXT ? &d->text : &d->think;
    char *s;
    if (json_type(v) != J_STR)
        return 0;
    s = json_strdup(v, &l);
    if (!s)
        return -1;
    if (!l) {
        free(s);
        return 0;
    }
    if (*open < 0) {
        end_block(k, kind == LB_TEXT ? &d->think : &d->text);
        *open = d->next++;
        llm_event_init(&e, LE_BLOCK_START);
        e.index = *open;
        e.kind = kind;
        if (kind == LB_THINKING && field) {
            e.sig = field;          /* pi: the thinking block's signature names the field it came in */
            e.sn = (long)strlen(field);
        }
        emit(k, &e);
    }
    llm_event_init(&e, LE_DELTA);
    e.index = *open;
    e.p = s;
    e.n = l;
    emit(k, &e);
    free(s);
    return 0;
}

static int tool_delta(oc_dec *d, const llm_sink *k, jv tc)
{
    jv v, f, a;
    llm_event e;
    long i = 0;
    char id[96], name[64];
    if (json_get(tc, "index", &v))
        i = json_long(v, 0);
    if (i < 0 || i >= OC_TOOLS)
        return -1;
    f.p = 0;
    json_get(tc, "function", &f);
    if (d->tool[i] < 0) {
        id[0] = name[0] = 0;
        if (json_get(tc, "id", &v) && json_type(v) == J_STR)
            json_str(v, id, sizeof(id));
        if (f.p && json_get(f, "name", &v) && json_type(v) == J_STR)
            json_str(v, name, sizeof(name));
        end_block(k, &d->think);
        end_block(k, &d->text);
        d->tool[i] = d->next++;
        llm_event_init(&e, LE_BLOCK_START);
        e.index = d->tool[i];
        e.kind = LB_TOOLCALL;
        e.id = id;
        e.name = name;
        emit(k, &e);
    }
    if (f.p && json_get(f, "arguments", &a) && json_type(a) == J_STR) {
        long l;
        char *s = json_strdup(a, &l);
        if (!s)
            return -1;
        if (l) {
            llm_event_init(&e, LE_DELTA);
            e.index = d->tool[i];
            e.p = s;
            e.n = l;
            emit(k, &e);
        }
        free(s);
    }
    return 0;
}

/* pi-ai's mapStopReason (pi f10993bc openai-completions.ts:1562-1586) */
static int stop_of(jv v, char *raw, long cap)
{
    json_str(v, raw, cap);
    if (!strcmp(raw, "stop") || !strcmp(raw, "end"))
        return LS_STOP;
    if (!strcmp(raw, "length"))
        return LS_LENGTH;
    if (!strcmp(raw, "tool_calls") || !strcmp(raw, "function_call"))
        return LS_TOOLUSE;
    return LS_ERROR;            /* content_filter, network_error, anything new: "Provider finish_reason: <it>" */
}

static int o_feed(void *dv, const char *event, const char *data, long n, const llm_sink *k)
{
    oc_dec *d = (oc_dec *)dv;
    jv ch, v, cs, c0, dl, u, tcs, tc;
    jit it;
    llm_event e;
    int have_c0 = 0, have_u;
    (void)event;
    llm_feeds_openai++;
    if (d->bad)
        return -1;
    while (n > 0 && (*data == ' ' || *data == '\n'))
        data++, n--;
    if (n == 6 && !memcmp(data, "[DONE]", 6)) {
        end_all(d, k);
        if (d->stop) {
            char msg[64];
            llm_event_init(&e, LE_STOP);
            e.stop = d->stop;
            e.stop_raw = d->stop_raw;
            if (d->stop == LS_ERROR) {
                cl_copy(msg, "Provider finish_reason: ", sizeof(msg));
                cl_cat(msg, d->stop_raw, sizeof(msg));
                e.p = msg;
                e.n = (long)strlen(msg);
            }
            emit(k, &e);
        }
        llm_event_init(&e, LE_DONE);
        emit(k, &e);
        d->done = 1;
        return 0;
    }
    if (d->done || json_parse(data, n, &ch) || json_type(ch) != J_OBJ) {
        d->bad = 1;
        return -1;
    }
    if (json_get(ch, "error", &v)) {
        char m[256], t[48];
        jv x;
        m[0] = 0;
        cl_copy(t, "error", sizeof(t));
        if (json_type(v) == J_OBJ) {
            if (json_get(v, "message", &x))
                json_str(x, m, sizeof(m));
            if (json_get(v, "type", &x) && json_type(x) == J_STR)
                json_str(x, t, sizeof(t));
        } else
            json_str(v, m, sizeof(m));
        llm_event_init(&e, LE_ERROR);
        e.stop = LS_ERROR;
        e.err_type = t;
        e.p = m;
        e.n = (long)strlen(m);
        emit(k, &e);
        return 0;
    }
    if (!d->started) {
        char model[64], id[96];
        model[0] = id[0] = 0;
        if (json_get(ch, "model", &v))
            json_str(v, model, sizeof(model));
        if (json_get(ch, "id", &v))
            json_str(v, id, sizeof(id));
        llm_event_init(&e, LE_START);
        e.model = model;
        e.p = id;
        e.n = (long)strlen(id);
        emit(k, &e);
        d->started = 1;
    }
    if (json_get(ch, "choices", &cs) && json_type(cs) == J_ARR) {
        json_iter(cs, &it);
        if (json_next(&it, 0, &c0)) {
            have_c0 = 1;
            if (json_get(c0, "delta", &dl) && json_type(dl) == J_OBJ) {
                if (json_get(dl, "reasoning_content", &v) && piece(d, k, LB_THINKING, v, "reasoning_content"))
                    return -1;
                if (json_get(dl, "reasoning", &v) && piece(d, k, LB_THINKING, v, "reasoning"))
                    return -1;
                if (json_get(dl, "content", &v) && piece(d, k, LB_TEXT, v, 0))
                    return -1;
                if (json_get(dl, "tool_calls", &tcs) && json_type(tcs) == J_ARR) {
                    jit ti;
                    json_iter(tcs, &ti);
                    while (json_next(&ti, 0, &tc))
                        if (tool_delta(d, k, tc)) {
                            d->bad = 1;
                            return -1;
                        }
                }
            }
            if (json_get(c0, "finish_reason", &v) && json_type(v) == J_STR)
                d->stop = stop_of(v, d->stop_raw, sizeof(d->stop_raw));
        }
    }
    /* chunk.usage, else choices[0].usage (Moonshot; openai-completions.ts:575-579) */
    have_u = json_get(ch, "usage", &u) && json_type(u) == J_OBJ;
    if (!have_u && have_c0)
        have_u = json_get(c0, "usage", &u) && json_type(u) == J_OBJ;
    if (have_u) {
        jv pt, ct, det, cached, rt;
        long in = -1, cr = 0, cw = 0;
        llm_event_init(&e, LE_USAGE);
        if (json_get(u, "prompt_tokens", &pt))
            in = json_long(pt, -1);
        /* cache read: prompt_tokens_details.cached_tokens ?? prompt_cache_hit_tokens (DeepSeek) ?? cached_tokens (Kimi), openai-completions.ts:1531-1532 */
        if (json_get(u, "prompt_cache_hit_tokens", &cached) && json_type(cached) != J_NULL)
            cr = json_long(cached, 0);
        else if (json_get(u, "cached_tokens", &cached) && json_type(cached) != J_NULL)
            cr = json_long(cached, 0);
        if (json_get(u, "prompt_tokens_details", &det) && json_type(det) == J_OBJ) {
            if (json_get(det, "cached_tokens", &cached) && json_type(cached) != J_NULL)
                cr = json_long(cached, 0);
            if (json_get(det, "cache_write_tokens", &cached))    /* parseChunkUsage (openai-completions.ts:1533): a separate count */
                cw = json_long(cached, 0);
        }
        if (in >= 0) {
            e.in = in - cr - cw > 0 ? in - cr - cw : 0;        /* openai-completions.ts:1546 */
            e.cache_r = cr;
            e.cache_w = cw;
        }
        if (json_get(u, "completion_tokens", &ct))
            e.out = json_long(ct, -1);
        e.has_reasoning = 1;    /* parseChunkUsage: completion_tokens_details.reasoning_tokens || 0 */
        if (json_get(u, "completion_tokens_details", &det) && json_type(det) == J_OBJ && json_get(det, "reasoning_tokens", &rt))
            e.reasoning = json_long(rt, 0);
        emit(k, &e);
    }
    return 0;
}

const llm_backend llm_be_openai_chat = {
    "openai-completions", "Authorization", "/chat/completions", o_head, o_body, 0, o_new, o_feed, o_free, 0, 0, 0
};

const llm_backend *llm_backend_of(const char *api)
{
    if (api && !strcmp(api, llm_be_anthropic.api))
        return &llm_be_anthropic;
    if (api && !strcmp(api, llm_be_openai_chat.api))
        return &llm_be_openai_chat;
    if (api && !strcmp(api, llm_be_openai_responses.api))
        return &llm_be_openai_responses;
    if (api && !strcmp(api, llm_be_azure.api))
        return &llm_be_azure;
    if (api && !strcmp(api, llm_be_google.api))
        return &llm_be_google;
    return 0;
}
