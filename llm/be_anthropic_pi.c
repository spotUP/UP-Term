/* be_anthropic_pi -- the anthropic-messages backend's amiga-pi half: the
 * head for a model and key, the body from pi-ai messages, and the backend
 * table (the head and decoder C:Claude shares are in be_anthropic.c). */
#include <stdlib.h>
#include <string.h>
#include "be_anthropic.h"
#include "../claude/http.h"
#include "registry.h"
#include "constrain.h"
#include "estimate.h"
#include "transcript.h"

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

/* ---- the backend for amiga-pi: pi-ai messages to the Messages API (pi-ai api/anthropic-messages.js) ----
 *
 * The request is pi's buildParams, member for member and in its order: model, messages, max_tokens, stream, system,
 * tools, thinking, output_config. The `betas` pi passes the SDK travel in the anthropic-beta header (a_xhead).
 * Compat flags of the model (llm_compat) decide the rest:
 *   supportsMidConvoSystemMessages: later system messages stay in the transcript (a text block of their update; with
 *     supportsMidConvoToolChanges also tool_removal / tool_addition blocks, the request's tool list then the initial
 *     tools plus a deferred placeholder); otherwise the caller's collapsed prompt (c->system, c->tools) is the request's.
 *   supportsMidConvoEffort: thinking is always adaptive with a block binding, output_config is "high", and the level
 *     travels as {"role":"system","content":[],"output_config":{"effort":L}} messages -- one before each assistant
 *     message that recorded a providerThinkingLevel, one at the end for this request.
 * The initial system message is msgs[0] when it is one; else c->system and c->tools stand for it (pi's
 * normalizeContext). */

#define AP_HELD 32
#define AP_CACHE ",\"cache_control\":{\"type\":\"ephemeral\"}"

enum { K_USER_STR, K_USER, K_RESULT, K_SYS, K_ASSIST };

typedef struct ap {
    const llm_ctx *c;
    int mid_sys, mid_effort, mid_tools, eager, cache_tools, empty_sig, strict;
    llm_tx tx;                  /* the initial system message and tools, the tools after the later ones */
    int native_tools;           /* pi's nativeToolChanges */
} ap;

static int flag(int v, int def)
{
    return v == LC_UNSET ? def : v;
}

/* trim().length === 0 on a JSON string's text (the escapes of the white space JS trims) */
static int blank(jv s)
{
    long i;
    if (json_type(s) != J_STR)
        return 1;
    for (i = 1; i < s.n - 1; i++) {
        char ch = s.p[i];
        if (ch == ' ')
            continue;
        if (ch == '\\' && i + 1 < s.n - 1) {
            char e = s.p[i + 1];
            if (e == 'n' || e == 't' || e == 'r' || e == 'f') {
                i++;
                continue;
            }
            if (e == 'u' && i + 5 < s.n) {
                static const char *const ws[] = { "0009", "000a", "000b", "000c", "000d", "0020", "00a0", "2028",
                                                  "2029", "feff", "000A", "00A0", "FEFF" };
                unsigned k;
                int hit = 0;
                for (k = 0; k < sizeof(ws) / sizeof(ws[0]); k++)
                    if (!memcmp(s.p + i + 2, ws[k], 4))
                        hit = 1;
                if (hit) {
                    i += 5;
                    continue;
                }
            }
        }
        return 0;
    }
    return 1;
}

/* pi's mapThinkingLevelToEffort on a clamped level; 0 when the level is off (no reasoning requested) */
static const char *effort_map(const llm_model *m, int lv)
{
    const llm_tmap *t;
    if (lv == LT_OFF)
        return 0;
    t = &m->thinking_map[lv];
    return t->kind == LTM_STR ? t->val : lv == LT_MEDIUM ? "medium" : lv <= LT_LOW ? "low" : "high";
}

const char *llm_ant_effort(const llm_model *m, int thinking)
{
    const char *e;
    if (!m || !m->api || strcmp(m->api, "anthropic-messages") || m->compat.mid_effort != 1)
        return 0;
    e = m->reasoning && m->adaptive ? effort_map(m, llm_think_clamp(m, thinking)) : 0;
    return e ? e : "high";
}

int llm_mid_system(const llm_model *m)
{
    return m && m->api && (!strcmp(m->api, "anthropic-messages") || !strcmp(m->api, "openai-responses") ||
                          !strcmp(m->api, "azure-openai-responses") || !strcmp(m->api, "openai-codex-responses")) &&
           m->compat.mid_system == 1;
}

static int is_effort(jv v)
{
    return json_streq(v, "low") || json_streq(v, "medium") || json_streq(v, "high") || json_streq(v, "xhigh") ||
           json_streq(v, "max");
}

static const char *effort_name(jv v)
{
    return json_streq(v, "low") ? "low" : json_streq(v, "medium") ? "medium" : json_streq(v, "high") ? "high"
         : json_streq(v, "xhigh") ? "xhigh" : "max";
}

/* one tool as the API takes it (pi's convertTools): a tool with constrainedSampling goes out strict when the model
 * supports it (resolveJsonSchemaStrictSampling), its input_schema then the strict one; otherwise the schema is
 * rebuilt from properties and required. 0, -1 */
static int w_tool(jw *w, jv t, int eager, int cache, int strict)
{
    jv v, pr;
    jw sch;
    int st;
    jw_init(&sch);
    st = llm_tool_strict(t, strict, LCS_ANTHROPIC, &sch, 0, 0);
    if (st < 0) {
        jw_free(&sch);
        return -1;
    }
    jw_rawz(w, "{\"name\":");
    if (!json_get(t, "name", &v)) {
        jw_free(&sch);
        return -1;
    }
    jw_raw(w, v.p, v.n);
    if (json_get(t, "description", &v)) {
        jw_rawz(w, ",\"description\":");
        jw_raw(w, v.p, v.n);
    }
    if (eager)
        jw_rawz(w, ",\"eager_input_streaming\":true");
    if (st == 1)
        jw_rawz(w, ",\"strict\":true");
    if (st == 1) {
        /* {...parameters, type, properties, required}: the strict schema, properties appended when it had none */
        jv sv;
        jw_rawz(w, ",\"input_schema\":");
        if (!json_parse(sch.p, sch.n, &sv) && !json_get(sv, "properties", &v)) {
            jw_raw(w, sch.p, sch.n - 1);
            jw_rawz(w, ",\"properties\":{}}");
        } else
            jw_raw(w, sch.p, sch.n);
    } else {
        jw_rawz(w, ",\"input_schema\":{\"type\":\"object\",\"properties\":");
        if (json_get(t, "parameters", &pr) && json_get(pr, "properties", &v) && json_type(v) != J_NULL)
            jw_raw(w, v.p, v.n);
        else
            jw_rawz(w, "{}");
        jw_rawz(w, ",\"required\":");
        if (json_get(t, "parameters", &pr) && json_get(pr, "required", &v) && json_type(v) != J_NULL)
            jw_raw(w, v.p, v.n);
        else
            jw_rawz(w, "[]");
        jw_raw(w, "}", 1);
    }
    jw_free(&sch);
    if (cache)
        jw_rawz(w, AP_CACHE);
    jw_raw(w, "}", 1);
    return 0;
}

static int w_tools(jw *w, const jv *tl, int n, int eager, int cache, int strict)
{
    int i;
    for (i = 0; i < n; i++) {
        jw_rawz(w, i ? "," : "");
        if (w_tool(w, tl[i], eager, cache && i == n - 1, strict))
            return -1;
    }
    return 0;
}

/* the flags of the model, and the transcript's system messages and tools (llm_tx_plan); llm_tx_free(&a->tx) after */
static int plan(const llm_ctx *c, ap *a)
{
    memset(a, 0, sizeof(*a));
    a->c = c;
    a->mid_sys = flag(c->model->compat.mid_system, 0);
    a->mid_effort = flag(c->model->compat.mid_effort, 0);
    a->mid_tools = flag(c->model->compat.mid_tools, 0);
    a->eager = flag(c->model->compat.eager_tools, 1);
    a->cache_tools = flag(c->model->compat.cache_tools, 1);
    a->empty_sig = flag(c->model->compat.empty_signature, 0);
    a->strict = flag(c->model->compat.strict_tools, 0);
    if (llm_tx_plan(c, a->mid_sys, &a->tx)) {
        llm_tx_free(&a->tx);
        return -1;
    }
    a->native_tools = a->mid_sys && a->mid_tools && a->tx.nitl > 0;
    return 0;
}

/* a text or image block as the API takes it */
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

/* a user message's content blocks (text left out when blank): the number written */
static int w_user_blocks(jw *w, jv c)
{
    jit it;
    jv b, t, v;
    int k = 0;
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        long mark = w->n;
        jw_rawz(w, k ? "," : "");
        if ((json_get(b, "type", &t) && json_streq(t, "text") && json_get(b, "text", &v) && blank(v)) ||
            w_user_block(w, b)) {
            w->n = mark;
            continue;
        }
        k++;
    }
    if (w->p)
        w->p[w->n] = 0;
    return k;
}

/* a tool result's content (convertContentBlocks): the texts joined by newlines as a string; with images, blocks
 * (and a placeholder text when there is none) */
static int w_result_content(jw *w, jv c)
{
    jit it;
    jv b, t;
    int has_img = 0, has_txt = 0, k = 0;
    if (json_type(c) == J_STR) {
        jw_raw(w, c.p, c.n);
        return 0;
    }
    if (json_type(c) != J_ARR)
        return -1;
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        if (json_get(b, "type", &t) && json_streq(t, "image"))
            has_img = 1;
        else if (json_get(b, "type", &t) && json_streq(t, "text"))
            has_txt = 1;
    }
    if (!has_img) {
        jw_rawz(w, "\"");
        llm_tx_content_text(w, c);
        jw_rawz(w, "\"");
        return 0;
    }
    jw_raw(w, "[", 1);
    if (!has_txt) {
        jw_rawz(w, "{\"type\":\"text\",\"text\":\"(see attached image)\"}");
        k++;
    }
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        jw_rawz(w, k ? "," : "");
        if (w_user_block(w, b))
            return -1;
        k++;
    }
    jw_raw(w, "]", 1);
    return 0;
}

/* an assistant message's blocks: 0 written none (the message is left out) */
static int w_assistant(jw *w, jv m, int empty_sig)
{
    jv c, b, t, v, s;
    jit it;
    int k = 0;
    if (!json_get(m, "content", &c) || json_type(c) != J_ARR)
        return 0;
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        long mark = w->n;
        if (!json_get(b, "type", &t))
            continue;
        jw_rawz(w, k ? "," : "");
        if (json_streq(t, "text") && json_get(b, "text", &v)) {
            if (blank(v)) {
                w->n = mark;
                continue;               /* the API refuses empty text blocks */
            }
            jw_rawz(w, "{\"type\":\"text\",\"text\":");
            jw_raw(w, v.p, v.n);
            jw_raw(w, "}", 1);
        } else if (json_streq(t, "thinking")) {
            /* redacted back as redacted_thinking; empty and unsigned dropped; unsigned as plain text (or an empty
             * signature, where the provider takes one); else thinking with its signature */
            jv red;
            int signed_ = json_get(b, "thinkingSignature", &s) && json_type(s) == J_STR && !blank(s);
            if (json_get(b, "redacted", &red) && json_type(red) == J_TRUE) {
                if (!json_get(b, "thinkingSignature", &s)) {
                    w->n = mark;
                    continue;
                }
                jw_rawz(w, "{\"type\":\"redacted_thinking\",\"data\":");
                jw_raw(w, s.p, s.n);
                jw_raw(w, "}", 1);
            } else {
                if (!json_get(b, "thinking", &v) || json_type(v) != J_STR || (blank(v) && !signed_)) {
                    w->n = mark;
                    continue;
                }
                if (!signed_ && empty_sig) {
                    jw_rawz(w, "{\"type\":\"thinking\",\"thinking\":");
                    jw_raw(w, v.p, v.n);
                    jw_rawz(w, ",\"signature\":\"\"}");
                } else if (!signed_) {
                    jw_rawz(w, "{\"type\":\"text\",\"text\":");
                    jw_raw(w, v.p, v.n);
                    jw_raw(w, "}", 1);
                } else {
                    jw_rawz(w, "{\"type\":\"thinking\",\"thinking\":");
                    jw_raw(w, v.p, v.n);
                    jw_rawz(w, ",\"signature\":");
                    jw_raw(w, s.p, s.n);
                    jw_raw(w, "}", 1);
                }
            }
        } else if (json_streq(t, "toolCall") && json_get(b, "id", &v) && json_get(b, "name", &s)) {
            jv args;
            /* tool-use ids: llm_prepare (handoff.c) already made them the form this API takes */
            jw_rawz(w, "{\"type\":\"tool_use\",\"id\":");
            jw_raw(w, v.p, v.n);
            jw_rawz(w, ",\"name\":");
            jw_raw(w, s.p, s.n);
            jw_rawz(w, ",\"input\":");
            if (json_get(b, "arguments", &args))
                jw_raw(w, args.p, args.n);
            else
                jw_rawz(w, "{}");
            jw_raw(w, "}", 1);
        } else if (json_streq(t, "raw") && json_get(b, "json", &v)) {
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

/* simple-options.ts: DEFAULT_THINKING_BUDGETS, xhigh and max count as high */
static long think_budget(int level)
{
    return level <= LT_MINIMAL ? 1024 : level == LT_LOW ? 2048 : level == LT_MEDIUM ? 8192 : 16384;
}

/* what the thinking level makes of the request (pi's anthropic streamSimple and buildParams):
 * 0 nothing, 1 budget (*maxt, *budget set), 2 adaptive, 3 disabled */
static int think_mode(const llm_ctx *c, long *maxt, long *budget)
{
    const llm_model *m = c->model;
    long mm = m->max_tokens > 0 ? m->max_tokens : 8192, base = c->max_tokens > 0 ? c->max_tokens : mm, b, m1;
    int lv;
    /* buildBaseOptions: the cap clamped to the context window */
    base = llm_clamp_max_tokens(m, c->est_tokens, base);
    *maxt = base;
    if (!m->reasoning)
        return 0;
    lv = llm_think_clamp(m, c->thinking);
    if (lv == LT_OFF)
        return m->thinking_map[LT_OFF].kind == LTM_NULL ? 0 : 3;
    if (m->adaptive)
        return 2;
    b = think_budget(lv);
    /* adjustMaxTokensForThinking, clampMaxTokensToContext again, then min(budget, maxTokens - 1024) */
    m1 = base + b < mm ? base + b : mm;
    if (m1 <= b)
        b = b < (m1 > 1024 ? m1 - 1024 : 0) ? b : (m1 > 1024 ? m1 - 1024 : 0);
    *maxt = llm_clamp_max_tokens(m, c->est_tokens, m1);
    {
        long room = *maxt > 1024 ? *maxt - 1024 : 0;
        *budget = b < room ? b : room;
    }
    return 1;
}

/* pi's getBetaFeatures, as the anthropic-beta header */
static long a_xhead(const llm_ctx *c, char *out, long cap)
{
    long mt, b, n;
    ap a;
    int k = 0;
    char line[320];
    if (plan(c, &a))
        return 0;
    strcpy(line, "anthropic-beta: ");
    n = (long)strlen(line);
#define BETA(s) do { if (k++) line[n++] = ','; strcpy(line + n, s); n += (long)strlen(s); } while (0)
    if (a.tx.ntl > 0 && !a.eager)
        BETA("fine-grained-tool-streaming-2025-05-14");
    if (think_mode(c, &mt, &b) == 1)
        BETA("interleaved-thinking-2025-05-14");
    if (c->model->compat.nfb > 0)
        BETA("server-side-fallback-2026-07-01");
    if (a.mid_effort) {
        BETA("mid-conversation-output-config-2026-07-01");
        BETA("thinking-binding-controls-2026-08-01");
    }
    if (a.native_tools)
        BETA("inline-tools-2026-09-15");
#undef BETA
    llm_tx_free(&a.tx);
    if (!k)
        return 0;
    strcpy(line + n, "\r\n");
    n += 2;
    if (cap <= n)
        return 0;
    memcpy(out, line, (size_t)n + 1);
    return n;
}

/* the converted conversation (pi's convertMessages): where each message starts, its kind, the effort it recorded */
typedef struct conv {
    jw cm;
    long *start;
    int *kind;
    const char **lvl;
    int n;
    jv last_str;                /* the last user message's string content (K_USER_STR) */
} conv;

static void begin(conv *cv, int kind)
{
    jw_rawz(&cv->cm, cv->n ? "," : "");
    cv->start[cv->n] = cv->cm.n;
    cv->kind[cv->n] = kind;
    cv->lvl[cv->n] = 0;
    cv->n++;
}

/* the message begun at mark (before its comma) taken back */
static void unbegin(conv *cv, long mark)
{
    cv->n--;
    cv->cm.n = mark;
    if (cv->cm.p)
        cv->cm.p[mark] = 0;
}

/* does a toolsAdded array hold a tool of that name */
static int tool_find_arr(jv arr, jv name)
{
    jit it;
    jv t, n2;
    json_iter(arr, &it);
    while (json_next(&it, 0, &t))
        if (json_get(t, "name", &n2) && n2.n == name.n && !memcmp(n2.p, name.p, (size_t)name.n))
            return 1;
    return 0;
}

/* a later system message as a held message: the text of its update, with native tool changes the tool blocks */
static int sys_message(const ap *a, jv m, jw *b)
{
    jw t;
    jv v, added, removed;
    jit it;
    int k = 0, has_add;
    jw_rawz(b, "{\"role\":\"system\",\"content\":[");
    jw_init(&t);
    if (llm_tx_sys_update(&t, m)) {
        jw_rawz(b, "{\"type\":\"text\",\"text\":");
        jw_raw(b, t.p, t.n);
        jw_raw(b, "}", 1);
        k++;
    }
    jw_free(&t);
    if (a->native_tools) {
        has_add = json_get(m, "toolsAdded", &added) && json_type(added) == J_ARR;
        if (json_get(m, "toolsRemoved", &removed) && json_type(removed) == J_ARR) {
            json_iter(removed, &it);
            while (json_next(&it, 0, &v)) {
                jv nm;
                if (!json_get(v, "name", &nm))
                    continue;
                /* a new definition under the same name replaces the old one: no removal */
                if (has_add && tool_find_arr(added, nm))
                    continue;
                jw_rawz(b, k ? "," : "");
                jw_rawz(b, "{\"type\":\"tool_removal\",\"tool\":{\"type\":\"tool_reference\",\"name\":");
                jw_raw(b, nm.p, nm.n);
                jw_rawz(b, "}}");
                k++;
            }
        }
        if (has_add) {
            json_iter(added, &it);
            while (json_next(&it, 0, &v)) {
                jw_rawz(b, k ? "," : "");
                jw_rawz(b, "{\"type\":\"tool_addition\",\"tool\":{\"type\":\"tool_definition\",\"definition\":");
                if (w_tool(b, v, a->eager, 0, a->strict))
                    return -1;
                jw_rawz(b, "}}");
                k++;
            }
        }
    }
    jw_rawz(b, "]}");
    return k ? 1 : 0;
}

static int convert(const ap *a, conv *cv)
{
    const llm_ctx *c = a->c;
    int i, nheld = 0, j, rc = -1;
    jw held[AP_HELD];
    for (i = a->tx.has_init ? 1 : 0; i < c->nmsg; i++) {
        jv m, r, x;
        long mark = cv->cm.n;
        if (json_parse(c->msgs[i], c->msg_n[i], &m) || !json_get(m, "role", &r))
            goto out;
        if (json_streq(r, "system")) {
            int k;
            if (!a->mid_sys)
                continue;               /* the caller's prompt carries them */
            if (nheld == AP_HELD)
                goto out;
            jw_init(&held[nheld]);
            k = sys_message(a, m, &held[nheld]);
            if (k < 0) {
                jw_free(&held[nheld]);
                goto out;
            }
            if (k)
                nheld++;
            else
                jw_free(&held[nheld]);
        } else if (json_streq(r, "user")) {
            if (!json_get(m, "content", &x))
                goto out;
            if (json_type(x) == J_STR) {
                if (blank(x))
                    continue;
                begin(cv, K_USER_STR);
                cv->last_str = x;
                jw_rawz(&cv->cm, "{\"role\":\"user\",\"content\":");
                jw_raw(&cv->cm, x.p, x.n);
                jw_raw(&cv->cm, "}", 1);
            } else {
                begin(cv, K_USER);
                jw_rawz(&cv->cm, "{\"role\":\"user\",\"content\":[");
                if (!w_user_blocks(&cv->cm, x)) {
                    unbegin(cv, mark);
                    continue;
                }
                jw_rawz(&cv->cm, "]}");
            }
        } else if (json_streq(r, "assistant")) {
            jv lv, pv, api;
            for (j = 0; j < nheld; j++) {
                begin(cv, K_SYS);
                jw_raw(&cv->cm, held[j].p, held[j].n);
                jw_free(&held[j]);
            }
            nheld = 0;
            mark = cv->cm.n;
            begin(cv, K_ASSIST);
            jw_rawz(&cv->cm, "{\"role\":\"assistant\",\"content\":[");
            if (!w_assistant(&cv->cm, m, a->empty_sig)) {
                unbegin(cv, mark);      /* an empty answer (an error, an abort) is left out */
                continue;
            }
            jw_rawz(&cv->cm, "]}");
            if (a->mid_effort && json_get(m, "api", &api) && json_streq(api, "anthropic-messages") &&
                json_get(m, "provider", &pv) && json_streq(pv, c->model->provider) &&
                json_get(m, "providerThinkingLevel", &lv) && is_effort(lv))
                cv->lvl[cv->n - 1] = effort_name(lv);
        } else if (json_streq(r, "toolResult")) {
            /* consecutive results: one user message of tool_result blocks */
            int k = 0;
            begin(cv, K_RESULT);
            jw_rawz(&cv->cm, "{\"role\":\"user\",\"content\":[");
            for (;;) {
                jv id, ct, er;
                jw_rawz(&cv->cm, k ? "," : "");
                if (!json_get(m, "toolCallId", &id) || !json_get(m, "content", &ct))
                    goto out;
                jw_rawz(&cv->cm, "{\"type\":\"tool_result\",\"tool_use_id\":");
                jw_raw(&cv->cm, id.p, id.n);
                jw_rawz(&cv->cm, ",\"content\":");
                if (w_result_content(&cv->cm, ct))
                    goto out;
                if (json_get(m, "isError", &er) && (json_type(er) == J_TRUE || json_type(er) == J_FALSE))
                    jw_rawz(&cv->cm, json_type(er) == J_TRUE ? ",\"is_error\":true" : ",\"is_error\":false");
                jw_raw(&cv->cm, "}", 1);
                k++;
                if (i + 1 >= c->nmsg || json_parse(c->msgs[i + 1], c->msg_n[i + 1], &m) ||
                    !json_get(m, "role", &r) || !json_streq(r, "toolResult"))
                    break;
                i++;
            }
            jw_rawz(&cv->cm, "]}");
        }
    }
    for (j = 0; j < nheld; j++) {
        begin(cv, K_SYS);
        jw_raw(&cv->cm, held[j].p, held[j].n);
        jw_free(&held[j]);
    }
    nheld = 0;
    /* cache_control on the last block of a final user or system message */
    if (cv->n > 0 && cv->kind[cv->n - 1] != K_ASSIST) {
        jw *cm = &cv->cm;
        if (cv->kind[cv->n - 1] == K_USER_STR) {
            cm->n = cv->start[cv->n - 1];
            jw_rawz(cm, "{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":");
            jw_raw(cm, cv->last_str.p, cv->last_str.n);
            jw_rawz(cm, AP_CACHE "}]}");
        } else if (cm->n >= 3) {
            cm->n -= 3;                 /* ...}]}: the last block's closing brace */
            jw_rawz(cm, AP_CACHE "}]}");
        }
    }
    rc = cv->cm.oom ? -1 : 0;
out:
    for (j = 0; j < nheld; j++)
        jw_free(&held[j]);
    return rc;
}

static void w_effort_msg(jw *w, const char *e)
{
    jw_rawz(w, "{\"role\":\"system\",\"content\":[],\"output_config\":{\"effort\":");
    jw_strz(w, e);
    jw_rawz(w, "}}");
}

static int a_body(const llm_ctx *c, jw *w)
{
    ap a;
    conv cv;
    long maxt, budget = 0, sp;
    int tm, i, first = 1, rc = -1;
    const char *eff;
    if (plan(c, &a))
        return -1;
    tm = think_mode(c, &maxt, &budget);
    memset(&cv, 0, sizeof(cv));
    jw_init(&cv.cm);
    cv.start = (long *)malloc((size_t)(c->nmsg + 1) * sizeof(long));
    cv.kind = (int *)malloc((size_t)(c->nmsg + 1) * sizeof(int));
    cv.lvl = (const char **)malloc((size_t)(c->nmsg + 1) * sizeof(char *));
    if (!cv.start || !cv.kind || !cv.lvl || convert(&a, &cv))
        goto out;
    jw_rawz(w, "{\"model\":");
    jw_strz(w, c->model->id);
    jw_rawz(w, ",\"messages\":[");
    for (i = 0; i < cv.n; i++) {
        long e = i + 1 < cv.n ? cv.start[i + 1] - 1 : cv.cm.n;
        if (cv.lvl[i]) {
            jw_rawz(w, first ? "" : ",");
            w_effort_msg(w, cv.lvl[i]);
            first = 0;
        }
        jw_rawz(w, first ? "" : ",");
        jw_raw(w, cv.cm.p + cv.start[i], e - cv.start[i]);
        first = 0;
    }
    eff = a.mid_effort ? llm_ant_effort(c->model, c->thinking) : 0;
    if (eff) {
        jw_rawz(w, first ? "" : ",");
        w_effort_msg(w, eff);
    }
    jw_rawz(w, "],\"max_tokens\":");
    jw_long(w, maxt);
    jw_rawz(w, ",\"stream\":true");
    sp = w->n;
    jw_rawz(w, ",\"system\":[{\"type\":\"text\",\"text\":");
    if (a.tx.has_init) {
        long at = w->n;
        llm_tx_sys_text(w, a.tx.init);
        if (w->n == at + 2)
            w->n = sp;                  /* nothing to say: no system member */
    } else if (c->system && *c->system)
        jw_strz(w, c->system);
    else
        w->n = sp;
    if (w->n != sp)
        jw_rawz(w, AP_CACHE "}]");
    if (w->p)
        w->p[w->n] = 0;
    if (a.native_tools) {
        jw_rawz(w, ",\"tools\":[");
        if (w_tools(w, a.tx.itl, a.tx.nitl, a.eager, a.cache_tools, a.strict))
            goto out;
        jw_rawz(w, ",{\"name\":\"__pi_deferred_placeholder__\",\"description\":\"Reserved placeholder. Never available. "
                   "Never call this.\",\"input_schema\":{\"type\":\"object\",\"properties\":{},\"required\":[]},"
                   "\"defer_loading\":true}]");
    } else if (a.tx.ntl > 0) {
        jw_rawz(w, ",\"tools\":[");
        if (w_tools(w, a.tx.tl, a.tx.ntl, a.eager, a.cache_tools, a.strict))
            goto out;
        jw_raw(w, "]", 1);
    }
    if (a.mid_effort)
        jw_rawz(w, ",\"thinking\":{\"type\":\"adaptive\",\"display\":\"summarized\",\"block_binding\":"
                   "{\"prefix_mismatch_behavior\":\"drop_block\"}},\"output_config\":{\"effort\":\"high\"}");
    else if (tm == 1) {
        jw_rawz(w, ",\"thinking\":{\"type\":\"enabled\",\"budget_tokens\":");
        jw_long(w, budget ? budget : 1024);
        jw_rawz(w, ",\"display\":\"summarized\"}");
    } else if (tm == 2) {
        jw_rawz(w, ",\"thinking\":{\"type\":\"adaptive\",\"display\":\"summarized\"},\"output_config\":{\"effort\":");
        jw_strz(w, effort_map(c->model, llm_think_clamp(c->model, c->thinking)));
        jw_raw(w, "}", 1);
    } else if (tm == 3)
        jw_rawz(w, ",\"thinking\":{\"type\":\"disabled\"}");
    if (c->model->compat.nfb > 0) {     /* last (buildParams): the models the server may fall back to */
        jw_rawz(w, ",\"fallbacks\":[");
        for (i = 0; i < c->model->compat.nfb; i++) {
            jw_rawz(w, i ? ",{\"model\":" : "{\"model\":");
            jw_strz(w, c->model->compat.fb[i].model);
            jw_raw(w, "}", 1);
        }
        jw_raw(w, "]", 1);
    }
    jw_raw(w, "}", 1);
    rc = w->oom ? -1 : 0;
out:
    llm_tx_free(&a.tx);
    jw_free(&cv.cm);
    free(cv.start);
    free(cv.kind);
    free((void *)cv.lvl);
    return rc;
}

static void *a_new(void)
{
    llm_anth_dec *d = (llm_anth_dec *)malloc(sizeof(llm_anth_dec));
    if (d)
        llm_anth_init(d);
    return d;
}

/* usage.cacheWrite1h, which C:Claude's decoder (be_anthropic.c, shared) does not read: pi-ai sets it from message_start's
 * usage.cache_creation.ephemeral_1h_input_tokens whatever the counts (0 without), and from a message_delta's only when
 * the delta carries it (anthropic-messages.ts:684, 846) */
static void w1h(const char *data, long n, const llm_sink *k)
{
    jv ev, t, u, cc, v;
    llm_event e;
    int start;
    if (json_parse(data, n, &ev) || !json_get(ev, "type", &t))
        return;
    start = json_streq(t, "message_start");
    if (start) {
        jv m;
        if (!json_get(ev, "message", &m) || !json_get(m, "usage", &u))
            return;
    } else if (!json_streq(t, "message_delta") || !json_get(ev, "usage", &u))
        return;
    llm_event_init(&e, LE_USAGE);
    if (!start && json_get(u, "output_tokens_details", &cc) && json_type(cc) == J_OBJ &&
        json_get(cc, "thinking_tokens", &v) && json_type(v) != J_NULL) {   /* Anthropic reports reasoning as a subset of output (anthropic-messages.ts:849) */
        e.reasoning = json_long(v, 0);
        e.has_reasoning = 1;
    }
    if (json_get(u, "cache_creation", &cc) && json_type(cc) == J_OBJ && json_get(cc, "ephemeral_1h_input_tokens", &v)) {
        e.cache_w1h = json_long(v, 0);
        e.has_w1h = 1;
    } else if (start)
        e.has_w1h = 1;
    if ((e.has_w1h || e.has_reasoning) && k && k->ev)
        k->ev(k->u, &e);
}

static int a_feed(void *dec, const char *event, const char *data, long n, const llm_sink *k)
{
    int rc;
    (void)event;
    rc = llm_anth_feed((llm_anth_dec *)dec, data, n, k);
    if (rc == 0)
        w1h(data, n, k);
    return rc;
}

static void a_free(void *dec)
{
    if (dec)
        llm_anth_free((llm_anth_dec *)dec);
    free(dec);
}

const llm_backend llm_be_anthropic = {
    "anthropic-messages", "x-api-key", "/v1/messages", a_head, a_body, a_xhead, a_new, a_feed, a_free,
    "Anthropic stream ended before message_stop", 0, 0
};
