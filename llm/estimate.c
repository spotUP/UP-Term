/* estimate -- see estimate.h. Lengths are JavaScript string lengths (UTF-16 units), counted on the JSON text
 * without decoding it; JSON.stringify's length is counted on the parsed value (compact, strings escaped as
 * JSON.stringify does, a number as written: pi writes its numbers shortest-form, so they agree). */
#include <string.h>
#include "estimate.h"

#define IMAGE_CHARS 4800L
#define DEEP 48

/* ceil(chars / 3.5) */
static long tokens_of(long chars)
{
    return (2 * chars + 6) / 7;
}

/* the UTF-16 length of a JSON string token's value (quotes included in s) */
static long str_len(jv s)
{
    const unsigned char *p = (const unsigned char *)s.p;
    long i = 1, e = s.n - 1, n = 0;
    while (i < e) {
        unsigned char c = p[i];
        if (c == '\\') {
            i += (i + 1 < e && p[i + 1] == 'u') ? 6 : 2;
            n++;
        } else if (c >= 0xF0) {
            i++;
            n += 2;
        } else if (c >= 0xC0 || c < 0x80) {
            i++;
            n++;
        } else
            i++;                    /* a continuation byte */
    }
    return n;
}

/* the length of the string as JSON.stringify writes it, quotes included */
static long str_json_len(jv s)
{
    const unsigned char *p = (const unsigned char *)s.p;
    long i = 1, e = s.n - 1, n = 2;
    while (i < e) {
        unsigned char c = p[i];
        if (c == '\\') {
            unsigned char x = i + 1 < e ? p[i + 1] : 0;
            if (x == 'u') {
                long v = 0;
                int k;
                for (k = 2; k < 6; k++) {
                    unsigned char h = p[i + k];
                    v = v * 16 + (h <= '9' ? h - '0' : (h | 0x20) - 'a' + 10);
                }
                n += (v == 0x22 || v == 0x5C || v == 8 || v == 9 || v == 10 || v == 12 || v == 13) ? 2 : v < 0x20 ? 6 : 1;
                i += 6;
            } else {
                n += (x == '/') ? 1 : 2;
                i += 2;
            }
        } else if (c >= 0xF0) {
            i++;
            n += 2;
        } else if (c >= 0xC0 || c < 0x80) {
            i++;
            n++;
        } else
            i++;
    }
    return n;
}

/* JSON.stringify(value).length */
static long json_js_len(jv v, int depth)
{
    jit it;
    jv k, e;
    long n = 0;
    int first = 1;
    switch (json_type(v)) {
    case J_STR:
        return str_json_len(v);
    case J_OBJ:
    case J_ARR: {
        int obj = json_type(v) == J_OBJ;
        if (depth > DEEP)
            return 0;
        n = 2;
        json_iter(v, &it);
        while (json_next(&it, obj ? &k : 0, &e)) {
            if (!first)
                n++;
            first = 0;
            if (obj)
                n += str_json_len(k) + 1;
            n += json_js_len(e, depth + 1);
        }
        return n;
    }
    case J_NULL:
    case J_TRUE:
        return 4;
    case J_FALSE:
        return 5;
    default:
        return v.n;
    }
}

/* the characters of a user / toolResult content: a string, or text blocks and images */
static long content_chars(jv c)
{
    jit it;
    jv b, t, x;
    long n = 0;
    if (json_type(c) == J_STR)
        return str_len(c);
    if (json_type(c) != J_ARR)
        return 0;
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        if (json_get(b, "type", &t) && json_streq(t, "text") && json_get(b, "text", &x) && json_type(x) == J_STR)
            n += str_len(x);
        else
            n += IMAGE_CHARS;
    }
    return n;
}

/* contentText(content): a string, or the text blocks joined by newlines */
static long text_chars(jv c, int *parts)
{
    jit it;
    jv b, t, x;
    long n = 0;
    int k = 0;
    if (json_type(c) == J_STR) {
        n = str_len(c);
        *parts = n > 0;
        return n;
    }
    if (json_type(c) != J_ARR) {
        *parts = 0;
        return 0;
    }
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        if (json_get(b, "type", &t) && json_streq(t, "text") && json_get(b, "text", &x) && json_type(x) == J_STR) {
            n += str_len(x) + (k ? 1 : 0);
            k++;
        }
    }
    *parts = n > 0;
    return n;
}

/* estimateToolsTokens */
static long tools_tokens(jv m, const char *key)
{
    jv v;
    if (!json_get(m, key, &v) || json_type(v) != J_ARR || json_count(v) == 0)
        return 0;
    return tokens_of(json_js_len(v, 0));
}

/* estimateMessageTokens */
static long message_tokens(jv m)
{
    jv r, c, v;
    long chars = 0;
    if (!json_get(m, "role", &r))
        return 0;
    if (json_streq(r, "system")) {
        long n;
        int parts = 0, np = 0;
        jv k, s;
        jit it;
        n = json_get(m, "content", &c) ? text_chars(c, &parts) : 0;
        np = parts;
        chars = n;
        if (json_get(m, "sections", &v) && json_type(v) == J_OBJ) {
            json_iter(v, &it);
            while (json_next(&it, &k, &s)) {
                if (json_type(s) == J_STR && str_len(s) > 0) {
                    chars += str_len(s);
                    np++;
                }
            }
        }
        if (np > 1)
            chars += 2L * (np - 1);
        return tokens_of(chars) + tools_tokens(m, "toolsAdded") + tools_tokens(m, "toolsRemoved");
    }
    if (json_streq(r, "user") || json_streq(r, "toolResult"))
        return json_get(m, "content", &c) ? tokens_of(content_chars(c)) : 0;
    if (json_get(m, "content", &c) && json_type(c) == J_ARR) {
        jit it;
        jv b, t, x, a;
        json_iter(c, &it);
        while (json_next(&it, 0, &b)) {
            if (!json_get(b, "type", &t))
                continue;
            if (json_streq(t, "text")) {
                if (json_get(b, "text", &x) && json_type(x) == J_STR)
                    chars += str_len(x);
            } else if (json_streq(t, "thinking")) {
                if (json_get(b, "thinking", &x) && json_type(x) == J_STR)
                    chars += str_len(x);
            } else {
                if (json_get(b, "name", &x) && json_type(x) == J_STR)
                    chars += str_len(x);
                chars += json_get(b, "arguments", &a) ? json_js_len(a, 0) : 9;     /* "undefined" */
            }
        }
    }
    return tokens_of(chars);
}

static long usage_total(jv m)
{
    jv u, v;
    long t, sum;
    if (!json_get(m, "usage", &u) || json_type(u) != J_OBJ)
        return 0;
    t = json_get(u, "totalTokens", &v) ? json_long(v, 0) : 0;
    if (t)
        return t;
    sum = 0;
    if (json_get(u, "input", &v))
        sum += json_long(v, 0);
    if (json_get(u, "output", &v))
        sum += json_long(v, 0);
    if (json_get(u, "cacheRead", &v))
        sum += json_long(v, 0);
    if (json_get(u, "cacheWrite", &v))
        sum += json_long(v, 0);
    return sum;
}

static int ts_ge(llm_ts a, llm_ts b)
{
    return a.sec > b.sec || (a.sec == b.sec && a.ms >= b.ms);
}

long llm_estimate_context(const char *const *msgs, const long *n, int count)
{
    int i, last = -1;
    long usage = 0, tokens = 0;
    llm_ts latest;
    int have_latest = 0;
    /* getLastAssistantUsageInfo: the last answer whose usage describes the prefix before it */
    latest.sec = 0;
    latest.ms = 0;
    for (i = 0; i < count; i++) {
        jv m, v;
        llm_ts ts;
        if (json_parse(msgs[i], n[i], &m))
            continue;
        ts.sec = 0;
        ts.ms = 0;
        if (json_get(m, "timestamp", &v))
            ts = llm_r_ts(v);
        if (json_get(m, "role", &v) && json_streq(v, "assistant")) {
            jv sr;
            long u = usage_total(m);
            int bad = json_get(m, "stopReason", &sr) && (json_streq(sr, "aborted") || json_streq(sr, "error"));
            if ((!have_latest || ts_ge(ts, latest)) && !bad && u > 0) {
                last = i;
                usage = u;
            }
        }
        if (!have_latest || ts_ge(ts, latest)) {
            latest = ts;
            have_latest = 1;
        }
    }
    tokens = usage;
    for (i = last + 1; i < count; i++) {
        jv m;
        if (!json_parse(msgs[i], n[i], &m))
            tokens += message_tokens(m);
    }
    return tokens;
}

long llm_clamp_max_tokens(const llm_model *m, long est, long max_tokens)
{
    long avail;
    if (m->context_window <= 0)
        return max_tokens > 1 ? max_tokens : 1;
    avail = m->context_window - est - LLM_CONTEXT_SAFETY;
    if (avail < 1)
        avail = 1;
    return max_tokens < avail ? max_tokens : avail;
}

long llm_ctx_max_tokens(const llm_ctx *c)
{
    long base = c->max_tokens > 0 ? c->max_tokens : c->model->max_tokens;
    return base > 0 ? llm_clamp_max_tokens(c->model, c->est_tokens, base) : 0;
}

/* simple-options.ts DEFAULT_THINKING_BUDGETS: xhigh and max count as high */
long llm_think_budget_of(int level)
{
    return level <= LT_MINIMAL ? 1024 : level == LT_LOW ? 2048 : level == LT_MEDIUM ? 8192 : 16384;
}

long llm_ctx_custom_budget(const llm_ctx *x, int lv)
{
    if (lv > LT_HIGH)
        lv = LT_HIGH;
    return lv >= LT_MINIMAL && (x->think_budget_set & (1u << lv)) ? x->think_budget[lv] : -1;
}

void llm_think_budget(const llm_model *m, long est, long base, int lv, long custom, long *maxt, long *budget)
{
    long mm = m->max_tokens > 0 ? m->max_tokens : 8192, b = custom >= 0 ? custom : llm_think_budget_of(lv), m1, room;
    /* adjustMaxTokensForThinking, clampMaxTokensToContext again, then min(budget, maxTokens - 1024) */
    m1 = base + b < mm ? base + b : mm;
    if (m1 <= b)
        b = b < (m1 > 1024 ? m1 - 1024 : 0) ? b : (m1 > 1024 ? m1 - 1024 : 0);
    *maxt = llm_clamp_max_tokens(m, est, m1);
    room = *maxt > 1024 ? *maxt - 1024 : 0;
    *budget = b < room ? b : room;
}
