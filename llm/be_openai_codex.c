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

const llm_backend llm_be_openai_codex = {
    "openai-codex-responses", "Authorization", "/codex/responses", c_head, c_body, c_xhead, llm_responses_new, c_feed,
    llm_responses_free, "OpenAI Responses stream ended before a terminal response event", 0, 0, 0, 0
};
