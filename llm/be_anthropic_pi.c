/* be_anthropic_pi -- the anthropic-messages backend's amiga-pi half: the
 * head for a model and key, the body from pi-ai messages, and the backend
 * table (the head and decoder C:Claude shares are in be_anthropic.c). */
#include <stdlib.h>
#include <string.h>
#include "be_anthropic.h"
#include "../claude/http.h"

static long a_head(const char *host, int port, int tls, const char *path, const char *key,
                   const char *user_agent, long body_len, char *out, long cap)
{
    http_hdr h[5];
    h[0].name = "User-Agent";
    h[0].value = user_agent;
    h[1].name = "Accept";
    h[1].value = "text/event-stream";
    h[2].name = "Content-Type";
    h[2].value = "application/json";
    h[3].name = "anthropic-version";
    h[3].value = "2023-06-01";
    h[4].name = "x-api-key";
    h[4].value = key;
    return http_post_head(host, port, tls, path, h, 5, body_len, out, cap);
}

/* ---- the backend for amiga-pi: pi-ai messages to the Messages API ---- */

static int same_api(jv m)
{
    jv a;
    return json_get(m, "api", &a) && json_streq(a, "anthropic-messages");
}

/* Anthropic tool-use ids: [a-zA-Z0-9_-]+, at most 64 (another provider's
 * id cleaned the way pi-ai's adapters do) */
static void w_id(jw *w, jv v)
{
    char id[128];
    long i, n = json_str(v, id, sizeof(id));
    if (n < 0)
        n = 0;
    if (n > 64)
        n = 64;
    for (i = 0; i < n; i++) {
        char c = id[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            id[i] = '_';
    }
    jw_str(w, id, n);
}

/* a text or image block as the API takes it (user and tool results) */
static int w_user_block(jw *w, jv b)
{
    jv t, v, m;
    if (!json_get(b, "type", &t))
        return -1;
    if (json_streq(t, "text") && json_get(b, "text", &v)) {
        jw_rawz(w, "{\"type\":\"text\",\"text\":");
        jw_raw(w, v.p, v.n);
        jw_raw(w, "}", 1);
        return 0;
    }
    if (json_streq(t, "image") && json_get(b, "data", &v) && json_get(b, "mimeType", &m)) {
        jw_rawz(w, "{\"type\":\"image\",\"source\":{\"type\":\"base64\",\"media_type\":");
        jw_raw(w, m.p, m.n);
        jw_rawz(w, ",\"data\":");
        jw_raw(w, v.p, v.n);
        jw_rawz(w, "}}");
        return 0;
    }
    return -1;
}

static int w_content(jw *w, jv c)
{
    jit it;
    jv b;
    int first = 1;
    if (json_type(c) == J_STR) {
        jw_raw(w, c.p, c.n);
        return 0;
    }
    jw_raw(w, "[", 1);
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        if (!first)
            jw_raw(w, ",", 1);
        first = 0;
        if (w_user_block(w, b))
            return -1;
    }
    jw_raw(w, "]", 1);
    return 0;
}

/* an assistant message's blocks: 0 written none (the message is left out) */
static int w_assistant(jw *w, jv m)
{
    jv c, b, t, v, s;
    jit it;
    int same = same_api(m), k = 0;
    if (!json_get(m, "content", &c) || json_type(c) != J_ARR)
        return 0;
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        long mark = w->n;
        if (!json_get(b, "type", &t))
            continue;
        jw_rawz(w, k ? "," : "");
        if (json_streq(t, "text") && json_get(b, "text", &v)) {
            if (v.n <= 2) {
                w->n = mark;
                continue;               /* the API refuses empty text blocks */
            }
            jw_rawz(w, "{\"type\":\"text\",\"text\":");
            jw_raw(w, v.p, v.n);
            jw_raw(w, "}", 1);
        } else if (json_streq(t, "thinking") && same) {
            jv red;
            if (json_get(b, "redacted", &red) && json_type(red) == J_TRUE) {
                if (!json_get(b, "thinkingSignature", &s)) {
                    w->n = mark;
                    continue;
                }
                jw_rawz(w, "{\"type\":\"redacted_thinking\",\"data\":");
                jw_raw(w, s.p, s.n);
                jw_raw(w, "}", 1);
            } else {
                if (!json_get(b, "thinking", &v) || !json_get(b, "thinkingSignature", &s)) {
                    w->n = mark;
                    continue;           /* thinking without its signature cannot go back */
                }
                jw_rawz(w, "{\"type\":\"thinking\",\"thinking\":");
                jw_raw(w, v.p, v.n);
                jw_rawz(w, ",\"signature\":");
                jw_raw(w, s.p, s.n);
                jw_raw(w, "}", 1);
            }
        } else if (json_streq(t, "thinking")) {
            /* another provider's thinking: text in tags (pi-ai's handoff) */
            char *x;
            long xn;
            if (!json_get(b, "thinking", &v) || !(x = json_strdup(v, &xn)) || !xn) {
                w->n = mark;
                continue;
            }
            jw_rawz(w, "{\"type\":\"text\",\"text\":");
            {
                jw tg;
                jw_init(&tg);
                jw_rawz(&tg, "<thinking>\n");
                jw_raw(&tg, x, xn);
                jw_rawz(&tg, "\n</thinking>");
                jw_str(w, tg.p, tg.n);
                jw_free(&tg);
            }
            free(x);
            jw_raw(w, "}", 1);
        } else if (json_streq(t, "toolCall") && json_get(b, "id", &v) && json_get(b, "name", &s)) {
            jv args;
            jw_rawz(w, "{\"type\":\"tool_use\",\"id\":");
            w_id(w, v);
            jw_rawz(w, ",\"name\":");
            jw_raw(w, s.p, s.n);
            jw_rawz(w, ",\"input\":");
            if (json_get(b, "arguments", &args))
                jw_raw(w, args.p, args.n);
            else
                jw_rawz(w, "{}");
            jw_raw(w, "}", 1);
        } else if (json_streq(t, "raw") && same && json_get(b, "json", &v)) {
            jw_raw(w, v.p, v.n);
        } else {
            w->n = mark;
            continue;
        }
        k++;
    }
    if (w->p)
        w->p[w->n] = 0;
    return k;
}

static int a_body(const llm_ctx *c, jw *w)
{
    int i, first = 1, in_results = 0;
    jw_rawz(w, "{\"model\":");
    jw_strz(w, c->model->id);
    jw_rawz(w, ",\"max_tokens\":");
    jw_long(w, c->max_tokens > 0 ? c->max_tokens : c->model->max_tokens > 0 ? c->model->max_tokens : 8192);
    jw_rawz(w, ",\"stream\":true");
    if (c->system && *c->system) {
        jw_rawz(w, ",\"system\":[{\"type\":\"text\",\"text\":");
        jw_strz(w, c->system);
        jw_rawz(w, "}]");
    }
    jw_rawz(w, ",\"messages\":[");
    for (i = 0; i < c->nmsg; i++) {
        jv m, r, x;
        if (json_parse(c->msgs[i], c->msg_n[i], &m) || !json_get(m, "role", &r))
            return -1;
        if (json_streq(r, "toolResult")) {
            /* consecutive results: one user message of tool_result blocks */
            jv id, ct, er;
            if (!in_results) {
                jw_rawz(w, first ? "" : ",");
                jw_rawz(w, "{\"role\":\"user\",\"content\":[");
            } else
                jw_raw(w, ",", 1);
            first = 0;
            if (!json_get(m, "toolCallId", &id) || !json_get(m, "content", &ct))
                return -1;
            jw_rawz(w, "{\"type\":\"tool_result\",\"tool_use_id\":");
            w_id(w, id);
            jw_rawz(w, ",\"content\":");
            if (w_content(w, ct))
                return -1;
            if (json_get(m, "isError", &er) && json_type(er) == J_TRUE)
                jw_rawz(w, ",\"is_error\":true");
            jw_raw(w, "}", 1);
            in_results = 1;
            continue;
        }
        if (in_results) {
            jw_rawz(w, "]}");
            in_results = 0;
        }
        if (json_streq(r, "user")) {
            if (!json_get(m, "content", &x))
                return -1;
            jw_rawz(w, first ? "" : ",");
            first = 0;
            jw_rawz(w, "{\"role\":\"user\",\"content\":");
            if (w_content(w, x))
                return -1;
            jw_raw(w, "}", 1);
        } else if (json_streq(r, "assistant")) {
            long mark = w->n;
            jw_rawz(w, first ? "" : ",");
            jw_rawz(w, "{\"role\":\"assistant\",\"content\":[");
            if (!w_assistant(w, m)) {
                w->n = mark;            /* an empty answer (an error, an abort) is left out */
                if (w->p)
                    w->p[w->n] = 0;
                continue;
            }
            jw_rawz(w, "]}");
            first = 0;
        }
        /* system messages: the system prompt carries them */
    }
    if (in_results)
        jw_rawz(w, "]}");
    jw_raw(w, "]", 1);
    if (c->tools && c->tools_n > 2) {
        jv ts, t, v;
        jit it;
        int k = 0;
        if (json_parse(c->tools, c->tools_n, &ts) || json_type(ts) != J_ARR)
            return -1;
        jw_rawz(w, ",\"tools\":[");
        json_iter(ts, &it);
        while (json_next(&it, 0, &t)) {
            jw_rawz(w, k++ ? "," : "");
            jw_rawz(w, "{\"name\":");
            if (!json_get(t, "name", &v))
                return -1;
            jw_raw(w, v.p, v.n);
            if (json_get(t, "description", &v)) {
                jw_rawz(w, ",\"description\":");
                jw_raw(w, v.p, v.n);
            }
            jw_rawz(w, ",\"input_schema\":");
            if (json_get(t, "parameters", &v))
                jw_raw(w, v.p, v.n);
            else
                jw_rawz(w, "{\"type\":\"object\"}");
            jw_raw(w, "}", 1);
        }
        jw_raw(w, "]", 1);
    }
    jw_raw(w, "}", 1);
    return w->oom ? -1 : 0;
}

static void *a_new(void)
{
    llm_anth_dec *d = (llm_anth_dec *)malloc(sizeof(llm_anth_dec));
    if (d)
        llm_anth_init(d);
    return d;
}

static int a_feed(void *dec, const char *event, const char *data, long n, const llm_sink *k)
{
    (void)event;
    return llm_anth_feed((llm_anth_dec *)dec, data, n, k);
}

static void a_free(void *dec)
{
    if (dec)
        llm_anth_free((llm_anth_dec *)dec);
    free(dec);
}

const llm_backend llm_be_anthropic = {
    "anthropic-messages", "x-api-key", "/messages", a_head, a_body, a_new, a_feed, a_free
};
