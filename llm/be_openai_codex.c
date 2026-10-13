/* be_openai_codex -- openai-codex-responses (phase O): the ChatGPT subscription's Responses endpoint, POST
 * {baseUrl}/codex/responses with the OAuth access token (pi 1.1.0 ai/src/api/openai-codex-responses.ts, SSE transport
 * only: pi tries a WebSocket first, "auto", and falls back to SSE; a WebSocket is not ported, COMPATIBILITY.md).
 *   - the body: llm_codex_body (store, stream, instructions, input, text, include, prompt_cache_key, tool_choice,
 *     parallel_tool_calls, tools, reasoning), buildRequestBody;
 *   - the head: originator, User-Agent, Authorization, chatgpt-account-id (the claim of the access token, extractAccountId),
 *     OpenAI-Beta, accept, content-type (buildSSEHeaders); session-id and x-client-request-id with a session (xhead);
 *   - the stream: the Responses events; response.done is response.completed, and an error event or response.failed ends
 *     the answer in Codex's own words (mapCodexEvents).
 * Portable C89, host-tested (amiga-pi tests/test_pi_login.c). */
#include <stdlib.h>
#include <string.h>
#include "backend.h"
#include "creds.h"
#include "../claude/http.h"
#include "../claude/util.h"

long llm_feeds_codex;

static long c_head(const char *host, int port, int tls, const char *path, const char *key,
                   const char *user_agent, long body_len, char *out, long cap)
{
    http_hdr h[9];
    char acct[200], bearer[4200];
    acct[0] = bearer[0] = 0;
    if (key && *key) {
        if (llm_jwt_account_id(key, acct, sizeof(acct)))
            return -1;          /* pi: "Failed to extract accountId from token" */
        cl_copy(bearer, "Bearer ", sizeof(bearer));
        cl_cat(bearer, key, sizeof(bearer));
    }
    h[0].name = "originator";
    h[0].value = "pi";
    h[1].name = "User-Agent";
    h[1].value = user_agent;
    h[2].name = "Authorization";
    h[2].value = bearer;
    h[3].name = "chatgpt-account-id";
    h[3].value = acct;
    h[4].name = "OpenAI-Beta";
    h[4].value = "responses=experimental";
    h[5].name = "accept";
    h[5].value = "text/event-stream";
    h[6].name = "content-type";
    h[6].value = "application/json";
    return http_post_head(host, port, tls, path, h, 7, body_len, out, cap);
}

static long c_xhead(const llm_ctx *c, char *out, long cap)
{
    long n;
    if (!c->session_id || !*c->session_id)
        return 0;
    cl_copy(out, "session-id: ", cap);
    cl_cat(out, c->session_id, cap);
    cl_cat(out, "\r\nx-client-request-id: ", cap);
    cl_cat(out, c->session_id, cap);
    cl_cat(out, "\r\n", cap);
    n = (long)strlen(out);
    return n < cap - 1 ? n : 0;
}

static int c_body(const llm_ctx *x, jw *w)
{
    return llm_codex_body(x, w);
}

/* the event's text with its "type" renamed: response.done -> response.completed (mapCodexEvents) */
static int is_type(jv ev, const char *t)
{
    jv v;
    return json_get(ev, "type", &v) && json_streq(v, t);
}

static int c_feed(void *dv, const char *event, const char *data, long n, const llm_sink *k)
{
    jv ev, v, r;
    char msg[300], code[96], text[200];
    llm_feeds_codex++;
    if (json_parse(data, n, &ev) || json_type(ev) != J_OBJ)
        return llm_responses_feed(dv, event, data, n, k);
    if (is_type(ev, "error")) {
        jv nested;
        code[0] = text[0] = 0;
        if (json_get(ev, "code", &v) && json_type(v) == J_STR)
            json_str(v, code, sizeof(code));
        else if (json_get(ev, "error", &nested) && json_type(nested) == J_OBJ && json_get(nested, "code", &v) &&
                 json_type(v) == J_STR)
            json_str(v, code, sizeof(code));
        if (json_get(ev, "message", &v) && json_type(v) == J_STR)
            json_str(v, text, sizeof(text));
        else if (json_get(ev, "error", &nested) && json_type(nested) == J_OBJ && json_get(nested, "message", &v) &&
                 json_type(v) == J_STR)
            json_str(v, text, sizeof(text));
        cl_copy(msg, "Codex error: ", sizeof(msg));
        cl_cat(msg, text[0] ? text : code[0] ? code : data, sizeof(msg));
        llm_responses_fail(dv, k, msg);
        return 0;
    }
    if (is_type(ev, "response.failed")) {
        code[0] = text[0] = 0;
        if (json_get(ev, "response", &r) && json_type(r) == J_OBJ && json_get(r, "error", &v) && json_type(v) == J_OBJ) {
            jv m;
            if (json_get(v, "message", &m) && json_type(m) == J_STR)
                json_str(m, text, sizeof(text));
        }
        llm_responses_fail(dv, k, text[0] ? text : "Codex response failed");
        return 0;
    }
    if (is_type(ev, "response.done")) {
        /* the same event under the name the Responses decoder knows: "response.done" is 13 bytes, the new one 18 */
        char *copy = (char *)malloc((size_t)n + 16);
        const char *at;
        long i, off = -1;
        int rc;
        if (!copy)
            return -1;
        at = data;
        for (i = 0; i + 13 <= n; i++)
            if (!memcmp(at + i, "response.done", 13)) {
                off = i;
                break;
            }
        if (off < 0) {
            free(copy);
            return llm_responses_feed(dv, event, data, n, k);
        }
        memcpy(copy, data, (size_t)off);
        memcpy(copy + off, "response.completed", 18);
        memcpy(copy + off + 18, data + off + 13, (size_t)(n - off - 13));
        rc = llm_responses_feed(dv, event, copy, n + 5, k);
        free(copy);
        return rc;
    }
    return llm_responses_feed(dv, event, data, n, k);
}

/* openai-codex-responses.ts:1596-1620 parseErrorResponse, thrown as `friendlyMessage || message` (:436-441): a plain Error,
 * so pi-ai's formatProviderError leaves the message alone (no status or body on it). message is the body, else the
 * status line's reason phrase, else "Request failed"; the JSON body's error.message replaces it. A 429, or an error code
 * of usage_limit_reached / usage_not_included / rate_limit_exceeded, says "You have hit your ChatGPT usage limit" with
 * the plan and the minutes until error.resets_at. A throw inside the parse (a plan_type that is no string) leaves the
 * raw body. */
static int code_is_limit(const char *c)
{
    static const char *const k[] = { "usage_limit_reached", "usage_not_included", "rate_limit_exceeded" };
    int i;
    for (i = 0; i < 3; i++) {
        long n = (long)strlen(k[i]), j, m = (long)strlen(c);
        for (j = 0; j + n <= m; j++) {
            long t;
            for (t = 0; t < n; t++) {
                char x = c[j + t];
                if (x >= 'A' && x <= 'Z')
                    x = (char)(x + 32);
                if (x != k[i][t])
                    break;
            }
            if (t == n)
                return 1;
        }
    }
    return 0;
}

static void c_http_err(const llm_http_fail *f, char *out, long cap)
{
    char friendly[300], msg[LLM_ERR_MAX], code[120], plan[80], tmp[40];
    jv root, err, v;
    int have_friendly = 0;
    cl_copy(msg, f->body && f->bn > 0 ? "" : f->reason && *f->reason ? f->reason : "Request failed", sizeof(msg));
    if (f->body && f->bn > 0) {
        long n = f->bn < (long)sizeof(msg) - 1 ? f->bn : (long)sizeof(msg) - 1;
        memcpy(msg, f->body, (size_t)n);
        msg[n] = 0;
    }
    friendly[0] = 0;
    if (f->body && f->bn > 0 && json_parse(f->body, f->bn, &root) == 0 && json_type(root) == J_OBJ && json_get(root, "error", &err) &&
        json_type(err) != J_NULL && json_type(err) != J_FALSE && !(json_type(err) == J_STR && json_streq(err, "")) &&
        !(json_type(err) == J_NUM && json_long(err, 1) == 0)) {
        int obj = json_type(err) == J_OBJ, bail = 0;
        code[0] = 0;
        if (obj) {
            if (json_get(err, "code", &v) && json_type(v) == J_STR && !json_streq(v, ""))
                json_str(v, code, sizeof(code));
            else if (json_get(err, "type", &v) && json_type(v) == J_STR)
                json_str(v, code, sizeof(code));
        }
        if (code_is_limit(code) || f->status == 429) {
            long mins = -1;
            plan[0] = 0;
            if (obj && json_get(err, "plan_type", &v) && json_type(v) != J_NULL && json_type(v) != J_FALSE &&
                !(json_type(v) == J_STR && json_streq(v, ""))) {
                long i;
                if (json_type(v) != J_STR)
                    bail = 1;           /* plan_type.toLowerCase() throws */
                else {
                    plan[0] = ' ';
                    plan[1] = '(';
                    json_str(v, plan + 2, (long)sizeof(plan) - 4);
                    for (i = 2; plan[i]; i++)
                        if (plan[i] >= 'A' && plan[i] <= 'Z')
                            plan[i] = (char)(plan[i] + 32);
                    cl_cat(plan, " plan)", (long)sizeof(plan));
                }
            }
            if (!bail && obj && json_get(err, "resets_at", &v) && json_type(v) == J_NUM && json_long(v, 0) != 0) {
                long d = json_long(v, 0) - llm_wall().sec;
                mins = d <= 0 ? 0 : (d + 30) / 60;      /* Math.max(0, Math.round(ms / 60000)) */
            }
            if (!bail) {
                cl_copy(friendly, "You have hit your ChatGPT usage limit", sizeof(friendly));
                cl_cat(friendly, plan, sizeof(friendly));
                cl_cat(friendly, ".", sizeof(friendly));
                if (mins >= 0) {
                    cl_cat(friendly, " Try again in ~", sizeof(friendly));
                    cl_ltoa(mins, tmp);
                    cl_cat(friendly, tmp, sizeof(friendly));
                    cl_cat(friendly, " min.", sizeof(friendly));
                }
                have_friendly = 1;
            }
        }
        if (!bail && obj && json_get(err, "message", &v) && json_type(v) != J_NULL && json_type(v) != J_FALSE &&
            !(json_type(v) == J_STR && json_streq(v, ""))) {
            if (json_type(v) == J_STR)
                json_str(v, msg, sizeof(msg));
            else if (json_type(v) == J_NUM || json_type(v) == J_TRUE) {
                long n = v.n < (long)sizeof(msg) - 1 ? v.n : (long)sizeof(msg) - 1;
                memcpy(msg, v.p, (size_t)n);
                msg[n] = 0;
            } else
                cl_copy(msg, "[object Object]", sizeof(msg));
        } else if (!bail && have_friendly)
            cl_copy(msg, friendly, sizeof(msg));
    }
    cl_copy(out, have_friendly ? friendly : msg, cap);
}

const llm_backend llm_be_openai_codex = {
    "openai-codex-responses", "Authorization", "/codex/responses", c_head, c_body, c_xhead, llm_responses_new, c_feed,
    llm_responses_free, "OpenAI Responses stream ended before a terminal response event", 0, 0, 0, c_http_err, 0
};
