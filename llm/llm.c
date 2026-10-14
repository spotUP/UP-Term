/* llm -- see llm.h. */
#include <stdlib.h>
#include <string.h>
#include "llm.h"

static const char *const stops[] = { "", "stop", "length", "toolUse", "error", "aborted", "pending" };

const char *llm_stop_name(int stop)
{
    return stop >= LS_STOP && stop <= LS_PENDING ? stops[stop] : "";
}

int llm_stop_of(jv v)
{
    int i;
    for (i = LS_STOP; i <= LS_ABORTED; i++)
        if (json_streq(v, stops[i]))
            return i;
    return LS_NONE;
}

void llm_w_micro(jw *w, long micro)
{
    char d[16];
    long whole, frac;
    int k;
    if (micro < 0) {
        jw_raw(w, "-", 1);
        micro = -micro;
    }
    whole = micro / 1000000L;
    frac = micro % 1000000L;
    jw_long(w, whole);
    if (!frac)
        return;
    for (k = 5; k >= 0; k--) {
        d[k] = (char)('0' + frac % 10);
        frac /= 10;
    }
    k = 6;
    while (k > 0 && d[k - 1] == '0')
        k--;
    jw_raw(w, ".", 1);
    jw_raw(w, d, k);
}

long llm_r_micro(jv v)
{
    /* the number as digits d and a power of ten: value = d * 10^e10;
     * micro-dollars keep e10 + 6 of it, rounded half up */
    char dg[40];
    const char *p = v.p, *e = v.p + v.n;
    int nd = 0, neg = 0, eneg = 0, i;
    long e10 = 0, ex = 0, keep, m = 0;
    if (json_type(v) != J_NUM)
        return 0;
    if (p < e && *p == '-') {
        neg = 1;
        p++;
    }
    for (; p < e && *p >= '0' && *p <= '9'; p++) {
        if (nd == 0 && *p == '0')
            continue;
        if (nd < (int)sizeof(dg))
            dg[nd++] = *p;
        else
            e10++;
    }
    if (p < e && *p == '.')
        for (p++; p < e && *p >= '0' && *p <= '9'; p++) {
            if (nd == 0 && *p == '0') {
                e10--;
                continue;
            }
            if (nd < (int)sizeof(dg)) {
                dg[nd++] = *p;
                e10--;
            }
        }
    if (p < e && (*p == 'e' || *p == 'E')) {
        p++;
        if (p < e && (*p == '-' || *p == '+'))
            eneg = *p++ == '-';
        for (; p < e && *p >= '0' && *p <= '9'; p++)
            if (ex < 1000)
                ex = ex * 10 + (*p - '0');
        e10 += eneg ? -ex : ex;
    }
    keep = nd + e10 + 6;
    if (keep <= 0)
        m = keep == 0 && nd && dg[0] >= '5';
    else {
        if (keep > 10)
            keep = 10;                  /* past 2^31 micro-dollars: the most a long holds */
        for (i = 0; i < keep; i++)
            m = m * 10 + (i < nd ? dg[i] - '0' : 0);
        if (keep < nd && dg[keep] >= '5')
            m++;
    }
    return neg ? -m : m;
}

void llm_w_ts(jw *w, llm_ts ts)
{
    char d[3];
    int ms = ts.ms < 0 ? 0 : ts.ms > 999 ? 999 : ts.ms;
    if (ts.sec <= 0) {
        jw_long(w, ms);
        return;
    }
    jw_long(w, ts.sec);
    d[0] = (char)('0' + ms / 100);
    d[1] = (char)('0' + ms / 10 % 10);
    d[2] = (char)('0' + ms % 10);
    jw_raw(w, d, 3);
}

llm_ts llm_r_ts(jv v)
{
    llm_ts t;
    long i, nd = 0;
    t.sec = 0;
    t.ms = 0;
    if (json_type(v) != J_NUM)
        return t;
    while (nd < v.n && v.p[nd] >= '0' && v.p[nd] <= '9')
        nd++;
    for (i = 0; i < nd; i++) {
        if (i < nd - 3)
            t.sec = t.sec * 10 + (v.p[i] - '0');
        else
            t.ms = t.ms * 10 + (v.p[i] - '0');
    }
    return t;
}

/* ---- blocks ---- */

static void key(jw *w, const char *k)
{
    jw_raw(w, ",", 1);
    jw_strz(w, k);
    jw_raw(w, ":", 1);
}

void llm_b_text(jw *w, const char *s, long n, const char *sig)
{
    jw_rawz(w, "{\"type\":\"text\",\"text\":");
    jw_str(w, s ? s : "", s ? n : 0);
    if (sig && *sig) {
        key(w, "textSignature");
        jw_strz(w, sig);
    }
    jw_raw(w, "}", 1);
}

void llm_b_thinking(jw *w, const char *s, long n, const char *sig, long sn, int redacted)
{
    jw_rawz(w, "{\"type\":\"thinking\",\"thinking\":");
    jw_str(w, s ? s : "", s ? n : 0);
    if (sig && sn > 0) {
        key(w, "thinkingSignature");
        jw_str(w, sig, sn);
    }
    if (redacted)
        jw_rawz(w, ",\"redacted\":true");
    jw_raw(w, "}", 1);
}

void llm_b_image(jw *w, const char *data, const char *mime)
{
    jw_rawz(w, "{\"type\":\"image\",\"data\":");
    jw_strz(w, data ? data : "");
    key(w, "mimeType");
    jw_strz(w, mime ? mime : "");
    jw_raw(w, "}", 1);
}

void llm_b_toolcall(jw *w, const char *id, const char *name, const char *args, long an, const char *tsig)
{
    jw_rawz(w, "{\"type\":\"toolCall\",\"id\":");
    jw_strz(w, id ? id : "");
    key(w, "name");
    jw_strz(w, name ? name : "");
    key(w, "arguments");
    if (args && an > 0)
        jw_raw(w, args, an);
    else
        jw_rawz(w, "{}");
    if (tsig && *tsig) {
        key(w, "thoughtSignature");
        jw_strz(w, tsig);
    }
    jw_raw(w, "}", 1);
}

void llm_b_raw(jw *w, const char *api, const char *json, long n)
{
    jw_rawz(w, "{\"type\":\"raw\",\"api\":");
    jw_strz(w, api ? api : "");
    key(w, "json");
    if (json && n > 0)
        jw_raw(w, json, n);
    else
        jw_rawz(w, "null");
    jw_raw(w, "}", 1);
}

/* ---- messages ---- */

static void content(jw *w, const char *c, long cn)
{
    if (c && cn > 0)
        jw_raw(w, c, cn);
    else
        jw_rawz(w, "[]");
}

void llm_m_user(jw *w, const char *c, long cn, llm_ts ts)
{
    jw_rawz(w, "{\"role\":\"user\",\"content\":");
    content(w, c, cn);
    key(w, "timestamp");
    llm_w_ts(w, ts);
    jw_raw(w, "}", 1);
}

void llm_w_usage(jw *w, const llm_usage *u)
{
    static const char *const ck[4] = { "input", "output", "cacheRead", "cacheWrite" };
    int i;
    jw_rawz(w, "{\"input\":");
    jw_long(w, u->input);
    jw_rawz(w, ",\"output\":");
    jw_long(w, u->output);
    jw_rawz(w, ",\"cacheRead\":");
    jw_long(w, u->cache_read);
    jw_rawz(w, ",\"cacheWrite\":");
    jw_long(w, u->cache_write);
    if (u->has_1h && u->combined) {
        jw_rawz(w, ",\"cacheWrite1h\":");
        jw_long(w, u->cache_write_1h);
    }
    if (u->has_reasoning && (u->combined || !u->has_1h)) {      /* openai: before totalTokens; anthropic's follows the cost */
        jw_rawz(w, ",\"reasoning\":");
        jw_long(w, u->reasoning);
    }
    jw_rawz(w, ",\"totalTokens\":");
    jw_long(w, u->has_total ? u->total : u->input + u->output + u->cache_read + u->cache_write);
    jw_rawz(w, ",\"cost\":{");
    for (i = 0; i < 4; i++) {
        if (i)
            jw_raw(w, ",", 1);
        jw_strz(w, ck[i]);
        jw_raw(w, ":", 1);
        llm_w_micro(w, u->cost[i]);
    }
    jw_rawz(w, ",\"total\":");
    llm_w_micro(w, u->cost[0] + u->cost[1] + u->cost[2] + u->cost[3]);
    jw_rawz(w, "}");
    if (u->has_1h && !u->combined) {    /* pi-ai adds it to the usage object after the cost, from the anthropic stream */
        jw_rawz(w, ",\"cacheWrite1h\":");
        jw_long(w, u->cache_write_1h);
    }
    if (u->has_reasoning && u->has_1h && !u->combined) {
        jw_rawz(w, ",\"reasoning\":");
        jw_long(w, u->reasoning);
    }
    jw_rawz(w, "}");
}

/* api/pi-messages.ts: the answer is a partial message whose keys are role, content, api, provider, model, usage, stopReason, timestamp;
 * the terminal event assigns stopReason and usage (in place), then errorMessage (error), responseId, providerThinkingLevel, and
 * appends diagnostics. A failure made anew (createErrorEvent) has errorMessage between stopReason and timestamp. */
static void m_pi_messages(jw *w, const char *c, long cn, const llm_meta *m, llm_ts ts)
{
    jw_rawz(w, "{\"role\":\"assistant\",\"content\":");
    content(w, c, cn);
    key(w, "api");
    jw_strz(w, m->api ? m->api : "");
    key(w, "provider");
    jw_strz(w, m->provider ? m->provider : "");
    key(w, "model");
    jw_strz(w, m->model ? m->model : "");
    key(w, "usage");
    if (m->usage_json && m->usage_json_n > 0)
        jw_raw(w, m->usage_json, m->usage_json_n);
    else
        llm_w_usage(w, &m->usage);
    key(w, "stopReason");
    jw_strz(w, m->stop ? llm_stop_name(m->stop) : "stop");
    if (m->error && m->pi_order == 1) {
        key(w, "errorMessage");
        jw_strz(w, m->error);
    }
    key(w, "timestamp");
    llm_w_ts(w, ts);
    if (m->error && m->pi_order == 2) {
        key(w, "errorMessage");
        jw_strz(w, m->error);
    }
    if (m->response_id && *m->response_id) {
        key(w, "responseId");
        jw_strz(w, m->response_id);
    }
    if (m->level && *m->level) {
        key(w, "providerThinkingLevel");
        jw_strz(w, m->level);
    }
    if (m->rewrite && m->rewrite_n > 0) {
        key(w, "diagnostics");
        jw_rawz(w, "[{\"type\":\"pi_messages_rewrite\",\"timestamp\":");
        llm_w_ts(w, m->rewrite_ts);
        jw_rawz(w, ",\"details\":");
        jw_raw(w, m->rewrite, m->rewrite_n);
        jw_rawz(w, "}]");
    }
    if (m->has_duration) {
        key(w, "durationMs");
        jw_long(w, m->duration_ms);
    }
    if (m->thinking_level && *m->thinking_level) {
        key(w, "thinkingLevel");
        jw_strz(w, m->thinking_level);
    }
    jw_raw(w, "}", 1);
}

void llm_m_assistant(jw *w, const char *c, long cn, const llm_meta *m, llm_ts ts)
{
    if (m->pi_order) {
        m_pi_messages(w, c, cn, m, ts);
        return;
    }
    jw_rawz(w, "{\"role\":\"assistant\",\"content\":");
    content(w, c, cn);
    key(w, "api");
    jw_strz(w, m->api ? m->api : "");
    key(w, "provider");
    jw_strz(w, m->provider ? m->provider : "");
    key(w, "model");
    jw_strz(w, m->model ? m->model : "");
    if (m->level && *m->level) {
        key(w, "providerThinkingLevel");
        jw_strz(w, m->level);
    }
    key(w, "usage");
    llm_w_usage(w, &m->usage);
    key(w, "stopReason");
    jw_strz(w, m->stop ? llm_stop_name(m->stop) : "stop");
    key(w, "timestamp");
    llm_w_ts(w, ts);
    /* the keys in the order pi-ai assigns them: the id and the server's model at the start of the answer, the provider's
     * stop reason at its end, an error in the stream's catch, the session's level last (the agent loop) */
    if (m->response_id && *m->response_id) {
        key(w, "responseId");
        jw_strz(w, m->response_id);
    }
    if (m->response_model && *m->response_model) {
        key(w, "responseModel");
        jw_strz(w, m->response_model);
    }
    if (m->raw_stop && *m->raw_stop) {
        key(w, "rawStopReason");
        jw_strz(w, m->raw_stop);
    }
    if (m->error) {
        key(w, "errorMessage");
        jw_strz(w, m->error);
    }
    if (m->has_duration) {          /* pi-ai 1.1.0: set when the stream's done/error event is pushed, before the agent loop's level */
        key(w, "durationMs");
        jw_long(w, m->duration_ms);
    }
    if (m->thinking_level && *m->thinking_level) {
        key(w, "thinkingLevel");
        jw_strz(w, m->thinking_level);
    }
    jw_raw(w, "}", 1);
}

void llm_m_toolresult(jw *w, const char *id, const char *name, const char *c, long cn,
                      const char *details, long dn, int is_error, long duration_ms, llm_ts ts)
{
    jw_rawz(w, "{\"role\":\"toolResult\",\"toolCallId\":");
    jw_strz(w, id ? id : "");
    key(w, "toolName");
    jw_strz(w, name ? name : "");
    key(w, "content");
    content(w, c, cn);
    if (details && dn > 0) {
        key(w, "details");
        jw_raw(w, details, dn);
    }
    jw_rawz(w, is_error ? ",\"isError\":true" : ",\"isError\":false");
    if (duration_ms >= 0) {         /* pi-agent-core 1.1.0 createToolResultMessage: after isError, before timestamp */
        key(w, "durationMs");
        jw_long(w, duration_ms);
    }
    key(w, "timestamp");
    llm_w_ts(w, ts);
    jw_raw(w, "}", 1);
}

void llm_m_system_ex(jw *w, const char *text, long n, const char *sections, long sn, const char *added, long an,
                     const char *removed, long rn, llm_ts ts)
{
    jw_rawz(w, "{\"role\":\"system\",\"content\":");
    jw_str(w, text ? text : "", text ? n : 0);
    if (sections && sn) {
        key(w, "sections");
        jw_raw(w, sections, sn);
    }
    key(w, "timestamp");
    llm_w_ts(w, ts);
    if (added && an) {
        key(w, "toolsAdded");
        jw_raw(w, added, an);
    }
    if (removed && rn) {
        key(w, "toolsRemoved");
        jw_raw(w, removed, rn);
    }
    jw_raw(w, "}", 1);
}

void llm_m_system(jw *w, const char *text, long n, llm_ts ts)
{
    llm_m_system_ex(w, text, n, 0, 0, 0, 0, 0, 0, ts);
}

/* ---- reading back ---- */

/* a string member, decoded (malloc'ed, 0 when missing or not a string) */
static char *sget(jv o, const char *k, long *n)
{
    jv v;
    *n = 0;
    return json_get(o, k, &v) && json_type(v) == J_STR ? json_strdup(v, n) : 0;
}

static int block_canon(jv b, jw *out)
{
    jv t, v;
    char *a = 0, *s = 0, *x = 0;
    long an, sn, xn;
    int rc = 0;
    if (json_type(b) != J_OBJ || !json_get(b, "type", &t))
        return -1;
    if (json_streq(t, "text")) {
        a = sget(b, "text", &an);
        s = sget(b, "textSignature", &sn);
        if (!a)
            rc = -1;
        else
            llm_b_text(out, a, an, s);
    } else if (json_streq(t, "thinking")) {
        a = sget(b, "thinking", &an);
        s = sget(b, "thinkingSignature", &sn);
        if (!a)
            rc = -1;
        else
            llm_b_thinking(out, a, an, s, sn, json_get(b, "redacted", &v) && json_type(v) == J_TRUE);
    } else if (json_streq(t, "image")) {
        a = sget(b, "data", &an);
        s = sget(b, "mimeType", &sn);
        if (!a || !s)
            rc = -1;
        else
            llm_b_image(out, a, s);
    } else if (json_streq(t, "toolCall")) {
        a = sget(b, "id", &an);
        s = sget(b, "name", &sn);
        x = sget(b, "thoughtSignature", &xn);
        if (!a || !s || !json_get(b, "arguments", &v) || json_type(v) != J_OBJ)
            rc = -1;
        else
            llm_b_toolcall(out, a, s, v.p, v.n, x);
    } else if (json_streq(t, "raw")) {
        a = sget(b, "api", &an);
        if (!a || !json_get(b, "json", &v))
            rc = -1;
        else
            llm_b_raw(out, a, v.p, v.n);
    } else
        rc = -1;
    free(a);
    free(s);
    free(x);
    return rc;
}

/* content: a block array, or (string_ok) a string */
static int content_canon(jv c, int string_ok, jw *out)
{
    jit it;
    jv b;
    int first = 1;
    if (json_type(c) == J_STR && string_ok) {
        jw_raw(out, c.p, c.n);
        return 0;
    }
    if (json_type(c) != J_ARR)
        return -1;
    jw_raw(out, "[", 1);
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        if (!first)
            jw_raw(out, ",", 1);
        first = 0;
        if (block_canon(b, out))
            return -1;
    }
    jw_raw(out, "]", 1);
    return 0;
}

static int role_of(jv m)
{
    jv r;
    if (json_type(m) != J_OBJ || !json_get(m, "role", &r))
        return LR_BAD;
    if (json_streq(r, "user"))
        return LR_USER;
    if (json_streq(r, "assistant"))
        return LR_ASSISTANT;
    if (json_streq(r, "toolResult"))
        return LR_TOOLRESULT;
    if (json_streq(r, "system"))
        return LR_SYSTEM;
    return LR_BAD;
}

void llm_r_usage(jv u, llm_usage *o)
{
    static const char *const ck[4] = { "input", "output", "cacheRead", "cacheWrite" };
    jv v, c;
    int i;
    memset(o, 0, sizeof(*o));
    if (json_get(u, "input", &v))
        o->input = json_long(v, 0);
    if (json_get(u, "output", &v))
        o->output = json_long(v, 0);
    if (json_get(u, "cacheRead", &v))
        o->cache_read = json_long(v, 0);
    if (json_get(u, "cacheWrite", &v))
        o->cache_write = json_long(v, 0);
    if (json_get(u, "cacheWrite1h", &v)) {
        o->cache_write_1h = json_long(v, 0);
        o->has_1h = 1;
    }
    if (json_get(u, "reasoning", &v)) {
        o->reasoning = json_long(v, 0);
        o->has_reasoning = 1;
    }
    if (json_get(u, "totalTokens", &v)) {
        long t = json_long(v, 0);
        if (t != o->input + o->output + o->cache_read + o->cache_write) {
            o->total = t;           /* the API's own count: kept as read */
            o->has_total = 1;
        }
    }
    if (json_get(u, "cost", &c))
        for (i = 0; i < 4; i++)
            if (json_get(c, ck[i], &v))
                o->cost[i] = llm_r_micro(v);
    if (o->has_1h) {            /* where cacheWrite1h stood: before totalTokens is a combined usage (pi's combineUsage) */
        jit it;
        jv k;
        json_iter(u, &it);
        while (json_next(&it, &k, &v)) {
            if (json_streq(k, "totalTokens"))
                break;
            if (json_streq(k, "cacheWrite1h")) {
                o->combined = 1;
                break;
            }
        }
    }
}

int llm_msg_canon(const char *json, long n, jw *out)
{
    jv m, c, v, d;
    jw body;
    int role, rc = 0;
    llm_ts ts;
    if (json_parse(json, n, &m))
        return LR_BAD;
    role = role_of(m);
    if (role == LR_BAD || !json_get(m, "content", &c))
        return LR_BAD;
    ts.sec = 0;
    ts.ms = 0;
    if (json_get(m, "timestamp", &v))
        ts = llm_r_ts(v);
    jw_init(&body);
    if (role == LR_SYSTEM) {
        char *s;
        long sn;
        jv se, ad, re;
        s = sget(m, "content", &sn);
        se.p = ad.p = re.p = 0;
        se.n = ad.n = re.n = 0;
        if (json_get(m, "sections", &se) && json_type(se) != J_OBJ)
            se.n = 0;
        if (json_get(m, "toolsAdded", &ad) && json_type(ad) != J_ARR)
            ad.n = 0;
        if (json_get(m, "toolsRemoved", &re) && json_type(re) != J_ARR)
            re.n = 0;
        if (!s)
            rc = -1;
        else
            llm_m_system_ex(&body, s, sn, se.p, se.n, ad.p, ad.n, re.p, re.n, ts);
        free(s);
    } else {
        jw cc;
        jw_init(&cc);
        rc = content_canon(c, role != LR_ASSISTANT, &cc);
        if (!rc && role == LR_USER)
            llm_m_user(&body, cc.p, cc.n, ts);
        else if (!rc && role == LR_ASSISTANT) {
            llm_meta mt;
            long ln;
            char *api = sget(m, "api", &ln), *prov = sget(m, "provider", &ln), *model = sget(m, "model", &ln),
                 *err = sget(m, "errorMessage", &ln), *rmod = sget(m, "responseModel", &ln),
                 *plv = sget(m, "providerThinkingLevel", &ln), *rid = sget(m, "responseId", &ln),
                 *rsr = sget(m, "rawStopReason", &ln), *tlv = sget(m, "thinkingLevel", &ln);
            memset(&mt, 0, sizeof(mt));
            mt.api = api;
            mt.provider = prov;
            mt.model = model;
            mt.error = err;
            mt.response_model = rmod;
            mt.level = plv;
            mt.response_id = rid;
            mt.raw_stop = rsr;
            mt.thinking_level = tlv;
            if (json_get(m, "durationMs", &v) && json_type(v) == J_NUM) {
                mt.duration_ms = json_long(v, 0);
                mt.has_duration = 1;
            }
            if (json_get(m, "usage", &v))
                llm_r_usage(v, &mt.usage);
            mt.stop = json_get(m, "stopReason", &v) ? llm_stop_of(v) : LS_NONE;
            if (!mt.stop)
                rc = -1;
            else
                llm_m_assistant(&body, cc.p, cc.n, &mt, ts);
            free(api);
            free(prov);
            free(model);
            free(err);
            free(rmod);
            free(plv);
            free(rid);
            free(rsr);
            free(tlv);
        } else if (!rc) {
            long ln;
            char *id = sget(m, "toolCallId", &ln), *name = sget(m, "toolName", &ln);
            int has_d = json_get(m, "details", &d);
            jv d2;
            if (!id || !name)
                rc = -1;
            else
                llm_m_toolresult(&body, id, name, cc.p, cc.n, has_d ? d.p : 0, has_d ? d.n : 0,
                                 json_get(m, "isError", &v) && json_type(v) == J_TRUE,
                                 json_get(m, "durationMs", &d2) && json_type(d2) == J_NUM ? json_long(d2, 0) : -1, ts);
            free(id);
            free(name);
        }
        jw_free(&cc);
    }
    if (rc || body.oom) {
        jw_free(&body);
        return LR_BAD;
    }
    jw_raw(out, body.p, body.n);
    jw_free(&body);
    return role;
}

int llm_msg_info(const char *json, long n, llm_info *i)
{
    jv m, v, c, b, t;
    jit it;
    memset(i, 0, sizeof(*i));
    if (json_parse(json, n, &m) || (i->role = role_of(m)) == LR_BAD) {
        i->role = LR_BAD;
        return LR_BAD;
    }
    if (json_get(m, "timestamp", &v))
        i->ts = llm_r_ts(v);
    if (i->role == LR_ASSISTANT) {
        if (json_get(m, "stopReason", &v))
            i->stop = llm_stop_of(v);
        if (json_get(m, "content", &c) && json_type(c) == J_ARR) {
            json_iter(c, &it);
            while (json_next(&it, 0, &b))
                if (json_get(b, "type", &t) && json_streq(t, "toolCall"))
                    i->ntools++;
        }
    }
    return i->role;
}
