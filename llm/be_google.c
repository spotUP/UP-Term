/* be_google -- the google-generative-ai backend (Phase M1): POST {baseUrl}/models/<id>:streamGenerateContent?alt=sse
 * with the key in x-goog-api-key, the body from pi-ai messages, and the decoder from the stream of whole
 * GenerateContentResponse objects (one per SSE data line) to the neutral events.
 *
 * The request is what pi 1.1.0 sends through the @google/genai SDK (2.21.0): pi's buildParams
 * (packages/ai/src/api/google-generative-ai.ts:370-430) and convertMessages / convertTools
 * (api/google-shared.ts:196-340, 342-380) give the parameters, the SDK's generateContentParametersToMldev
 * (dist/node/index.mjs:10850) puts them on the wire: contents, systemInstruction, tools, toolConfig, generationConfig,
 * and inside a part the members in the SDK's order (functionCall, functionResponse, inlineData, text, thought,
 * thoughtSignature). Goldens: tests/pi/google, made by tools/gen_pi_google.mjs.
 *
 * Decoder (google-generative-ai.ts:93-262): a part with text opens or extends a text / thinking block (thought: true),
 * a function call is a whole tool call (its id the one the API gave, else <name>_<ms>_<counter> as pi makes it, also
 * when the id repeats), thoughtSignature is kept (the last non-empty one of a block, retainThoughtSignature), usage
 * is replaced by every chunk that carries usageMetadata, the finish reason maps STOP -> stop (toolUse when the answer
 * has a call), MAX_TOKENS -> length, every other one is an error ("Provider stopped with: X"). The stream has no
 * terminal event: llm_backend.fin closes it when the body ends.
 * Not ported (COMPATIBILITY.md): tool_choice, temperature, custom thinking budgets, the SDK's x-goog-api-client header.
 * Portable C89, host-tested (tests/test_pi_llm.c, goldens from pi). */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "backend.h"
#include "registry.h"
#include "constrain.h"
#include "estimate.h"
#include "transcript.h"
#include "../claude/http.h"
#include "../claude/util.h"

long llm_feeds_google;

/* ---- the clock and the counter of generated tool-call ids ---- */

static llm_ts (*wall_now)(void *u);
static void *wall_u;
static long id_counter;

void llm_set_wall(llm_ts (*now)(void *u), void *u)
{
    wall_now = now;
    wall_u = u;
}

void llm_google_counter(long n)
{
    id_counter = n;
}

static llm_ts wall(void)
{
    llm_ts t;
    if (wall_now)
        return wall_now(wall_u);
    t.sec = (long)time(0);
    t.ms = 0;
    return t;
}

/* ---- head and route ---- */

static long g_head(const char *host, int port, int tls, const char *path, const char *key,
                   const char *user_agent, long body_len, char *out, long cap)
{
    http_hdr h[3];
    h[0].name = "User-Agent";
    h[0].value = user_agent;
    h[1].name = "x-goog-api-key";
    h[1].value = key ? key : "";
    h[2].name = "Content-Type";
    h[2].value = "application/json";
    return http_post_head(host, port, tls, path, h, 3, body_len, out, cap);
}

/* @google/genai: models/<id>:streamGenerateContent?alt=sse below the base URL (pi sets apiVersion "": the base URL
 * already ends in /v1beta) */
static long g_route(const llm_ctx *c, char *out, long cap)
{
    cl_copy(out, "/models/", cap);
    cl_cat(out, c->model->id, cap);
    cl_cat(out, ":streamGenerateContent?alt=sse", cap);
    return (long)strlen(out) < cap - 1 ? (long)strlen(out) : -1;
}

/* ---- model rules (google-shared.ts) ---- */

/* the major version of gemini[-live]-<n>, -1 when the id is not one (getGeminiMajorVersion, on the lowercased id) */
static int gemini_major(const char *id)
{
    char low[96];
    const char *s;
    long i, n = 0;
    for (i = 0; id[i] && i < (long)sizeof(low) - 1; i++)
        low[i] = id[i] >= 'A' && id[i] <= 'Z' ? (char)(id[i] + 32) : id[i];
    low[i] = 0;
    if (strncmp(low, "gemini-", 7))
        return -1;
    s = low + 7;
    if (!strncmp(s, "live-", 5))
        s += 5;
    if (*s < '0' || *s > '9')
        return -1;
    while (*s >= '0' && *s <= '9' && n < 1000000)
        n = n * 10 + (*s++ - '0');
    return (int)n;
}

/* requiresToolCallId */
static int needs_call_id(const char *id)
{
    int major = gemini_major(id);
    return !strncmp(id, "claude-", 7) || !strncmp(id, "gpt-oss-", 8) || major >= 3;
}

int llm_google_call_ids(const char *id)
{
    return needs_call_id(id);
}

/* supportsMultimodalFunctionResponse */
static int multimodal_fr(const char *id)
{
    int major = gemini_major(id);
    return major < 0 ? 1 : major >= 3;
}

/* usesGoogleThinkingLevel: gemini-3[.n]-pro|flash, gemini-flash[-lite]-latest, gemma-4 / gemma4 */
static int uses_level(const char *id)
{
    char low[96];
    long i;
    const char *p;
    for (i = 0; id[i] && i < (long)sizeof(low) - 1; i++)
        low[i] = id[i] >= 'A' && id[i] <= 'Z' ? (char)(id[i] + 32) : id[i];
    low[i] = 0;
    if (!strcmp(low, "gemini-flash-latest") || !strcmp(low, "gemini-flash-lite-latest"))
        return 1;
    if (strstr(low, "gemma-4") || strstr(low, "gemma4"))
        return 1;
    for (p = low; (p = strstr(p, "gemini-3")) != 0; p++) {
        const char *q = p + 8;
        if (*q == '.') {
            q++;
            if (*q < '0' || *q > '9')
                continue;
            while (*q >= '0' && *q <= '9')
                q++;
        }
        if (!strncmp(q, "-pro", 4) || !strncmp(q, "-flash", 6))
            return 1;
    }
    return 0;
}

/* the level word of a resolved level (resolveGoogleThinkingLevel + toGoogleThinkingLevel); 0 when the mapping is none
 * of minimal / low / medium / high (pi throws) */
static const char *level_word(const llm_model *m, int lv)
{
    static const char *const names[] = { "off", "minimal", "low", "medium", "high", "xhigh", "max" };
    const llm_tmap *t = &m->thinking_map[lv];
    const char *s = t->kind == LTM_STR ? t->val : names[lv];
    if (cl_strieq(s, "minimal"))
        return "MINIMAL";
    if (cl_strieq(s, "low"))
        return "LOW";
    if (cl_strieq(s, "medium"))
        return "MEDIUM";
    if (cl_strieq(s, "high"))
        return "HIGH";
    return 0;
}

/* getGoogleBudget (google-generative-ai.ts:440-470); lv LT_MINIMAL..LT_HIGH */
static long budget(const char *id, int lv)
{
    static const long pro[5] = { 0, 128, 2048, 8192, 32768 };
    static const long lite[5] = { 0, 512, 2048, 8192, 24576 };
    static const long flash[5] = { 0, 128, 2048, 8192, 24576 };
    if (lv < LT_MINIMAL || lv > LT_HIGH)
        return -1;
    if (strstr(id, "2.5-pro"))
        return pro[lv];
    if (strstr(id, "2.5-flash-lite"))
        return lite[lv];
    if (strstr(id, "2.5-flash"))
        return flash[lv];
    return -1;
}

/* ---- the body: contents ---- */

/* a thought signature is kept when it is valid base64 (TYPE_BYTES) and the message came from this provider and model */
static int sig_ok(jv s)
{
    char *d;
    long l, i, pad = 0;
    int ok = 1;
    if (json_type(s) != J_STR)
        return 0;
    d = json_strdup(s, &l);
    if (!d)
        return 0;
    if (l == 0 || l % 4)
        ok = 0;
    for (i = 0; ok && i < l; i++) {
        char ch = d[i];
        if (ch == '=') {
            pad++;
            if (pad > 2)
                ok = 0;
        } else if (pad || !((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
                            ch == '+' || ch == '/'))
            ok = 0;
    }
    if (ok && pad == l)
        ok = 0;
    free(d);
    return ok;
}

/* JS String.prototype.trim() is empty */
static int blank_text(jv s)
{
    char *d;
    long l, i;
    int b = 1;
    d = json_strdup(s, &l);
    if (!d)
        return 1;
    for (i = 0; i < l; i++)
        if (!(d[i] == ' ' || d[i] == '\t' || d[i] == '\n' || d[i] == '\r' || d[i] == '\v' || d[i] == '\f')) {
            b = 0;
            break;
        }
    free(d);
    return b;
}

/* the sink for the contents array being written */
typedef struct gc {
    jw *w;
    int n;                      /* contents so far */
    int last_fr;                /* the last content is a user turn whose parts hold a functionResponse */
    long tail;                  /* where its "],\"role\":\"user\"}" starts */
} gc;

static void part_sep(jw *w, int *k)
{
    jw_rawz(w, (*k)++ ? "," : "");
}

static void w_inline(jw *w, jv b, int sdk)
{
    jv d, m;
    json_get(b, "data", &d);
    json_get(b, "mimeType", &m);
    jw_rawz(w, "{\"inlineData\":{");
    if (sdk) {
        jw_rawz(w, "\"data\":");
        jw_raw(w, d.p, d.n);
        jw_rawz(w, ",\"mimeType\":");
        jw_raw(w, m.p, m.n);
    } else {
        jw_rawz(w, "\"mimeType\":");
        jw_raw(w, m.p, m.n);
        jw_rawz(w, ",\"data\":");
        jw_raw(w, d.p, d.n);
    }
    jw_rawz(w, "}}");
}

static int w_user(gc *g, jv c)
{
    jw *w = g->w;
    jit it;
    jv b, t, v;
    int k = 0;
    long mark = w->n;
    jw_rawz(w, g->n ? "," : "");
    jw_rawz(w, "{\"parts\":[");
    if (json_type(c) == J_STR) {
        jw_rawz(w, "{\"text\":");
        jw_raw(w, c.p, c.n);
        jw_rawz(w, "}");
        k = 1;
    } else if (json_type(c) == J_ARR) {
        json_iter(c, &it);
        while (json_next(&it, 0, &b)) {
            part_sep(w, &k);
            if (json_get(b, "type", &t) && json_streq(t, "text") && json_get(b, "text", &v)) {
                jw_rawz(w, "{\"text\":");
                jw_raw(w, v.p, v.n);
                jw_rawz(w, "}");
            } else if (json_get(b, "type", &t) && json_streq(t, "image") && json_get(b, "data", &v) &&
                       json_get(b, "mimeType", &v)) {
                w_inline(w, b, 1);
            } else
                return -1;
        }
    }
    if (!k) {
        w->n = mark;            /* pi: `if (parts.length === 0) continue` */
        if (w->p)
            w->p[w->n] = 0;
        return 0;
    }
    jw_rawz(w, "],\"role\":\"user\"}");
    g->n++;
    g->last_fr = 0;
    return 0;
}

static int w_assistant(gc *g, const llm_model *md, jv m)
{
    jw *w = g->w;
    jv c, b, t, v, pv, mv;
    jit it;
    int k = 0, same = 0;
    long mark = w->n;
    if (!json_get(m, "content", &c) || json_type(c) != J_ARR)
        return 0;
    if (json_get(m, "provider", &pv) && json_get(m, "model", &mv))
        same = json_streq(pv, md->provider) && json_streq(mv, md->id);
    jw_rawz(w, g->n ? "," : "");
    jw_rawz(w, "{\"parts\":[");
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        jv sg;
        int has_sig;
        if (!json_get(b, "type", &t))
            continue;
        if (json_streq(t, "text")) {
            if (!json_get(b, "text", &v))
                continue;
            has_sig = same && json_get(b, "textSignature", &sg) && sig_ok(sg);
            if ((v.n == 2 || blank_text(v)) && !has_sig)
                continue;
            part_sep(w, &k);
            jw_rawz(w, "{\"text\":");
            jw_raw(w, v.p, v.n);
            if (has_sig) {
                jw_rawz(w, ",\"thoughtSignature\":");
                jw_raw(w, sg.p, sg.n);
            }
            jw_rawz(w, "}");
        } else if (json_streq(t, "thinking")) {
            if (!json_get(b, "thinking", &v))
                continue;
            if (same) {
                has_sig = json_get(b, "thinkingSignature", &sg) && sig_ok(sg);
                if ((v.n == 2 || blank_text(v)) && !has_sig)
                    continue;
                part_sep(w, &k);
                jw_rawz(w, "{\"text\":");
                jw_raw(w, v.p, v.n);
                jw_rawz(w, ",\"thought\":true");
                if (has_sig) {
                    jw_rawz(w, ",\"thoughtSignature\":");
                    jw_raw(w, sg.p, sg.n);
                }
                jw_rawz(w, "}");
            } else {
                if (v.n == 2 || blank_text(v))
                    continue;
                part_sep(w, &k);
                jw_rawz(w, "{\"text\":");
                jw_raw(w, v.p, v.n);
                jw_rawz(w, "}");
            }
        } else if (json_streq(t, "toolCall")) {
            jv id, nm, args;
            if (!json_get(b, "name", &nm))
                continue;
            has_sig = same && json_get(b, "thoughtSignature", &sg) && sig_ok(sg);
            part_sep(w, &k);
            jw_rawz(w, "{\"functionCall\":{\"args\":");
            if (json_get(b, "arguments", &args) && json_type(args) != J_NULL)
                jw_raw(w, args.p, args.n);
            else
                jw_rawz(w, "{}");
            if (needs_call_id(md->id) && json_get(b, "id", &id)) {
                jw_rawz(w, ",\"id\":");
                jw_raw(w, id.p, id.n);
            }
            jw_rawz(w, ",\"name\":");
            jw_raw(w, nm.p, nm.n);
            jw_rawz(w, "}");
            if (has_sig) {
                jw_rawz(w, ",\"thoughtSignature\":");
                jw_raw(w, sg.p, sg.n);
            }
            jw_rawz(w, "}");
        }
    }
    if (!k) {
        w->n = mark;
        if (w->p)
            w->p[w->n] = 0;
        return 0;
    }
    jw_rawz(w, "],\"role\":\"model\"}");
    g->n++;
    g->last_fr = 0;
    return 0;
}

static int w_result(gc *g, const llm_model *md, jv m)
{
    jw *w = g->w;
    jv c, b, t, v, id, nm;
    jit it;
    jw text;
    int ntext = 0, nimg = 0, err = 0;
    jv imgs[16];
    int multi = multimodal_fr(md->id), i;
    if (!json_get(m, "content", &c) || json_type(c) != J_ARR)
        return -1;
    jw_init(&text);
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        if (!json_get(b, "type", &t))
            continue;
        if (json_streq(t, "text") && json_get(b, "text", &v)) {
            char *x;
            long l;
            x = json_strdup(v, &l);
            if (ntext++)
                jw_rawz(&text, "\n");
            if (x) {
                jw_raw(&text, x, l);
                free(x);
            }
        } else if (json_streq(t, "image") && md->image && nimg < 16)
            imgs[nimg++] = b;
    }
    if (json_get(m, "isError", &v))
        err = json_type(v) == J_TRUE;
    {
        jw part;
        long at;
        jw_init(&part);
        jw_rawz(&part, "{\"functionResponse\":{\"name\":");
        if (json_get(m, "toolName", &nm))
            jw_raw(&part, nm.p, nm.n);
        else
            jw_rawz(&part, "\"\"");
        jw_rawz(&part, ",\"response\":{");
        jw_rawz(&part, err ? "\"error\":" : "\"output\":");
        if (text.n > 0)
            jw_str(&part, text.p, text.n);
        else if (nimg > 0)
            jw_strz(&part, "(see attached image)");
        else
            jw_rawz(&part, "\"\"");
        jw_rawz(&part, "}");
        if (nimg > 0 && multi) {
            jw_rawz(&part, ",\"parts\":[");
            for (i = 0; i < nimg; i++) {
                jw_rawz(&part, i ? "," : "");
                w_inline(&part, imgs[i], 0);
            }
            jw_rawz(&part, "]");
        }
        if (needs_call_id(md->id) && json_get(m, "toolCallId", &id)) {
            jw_rawz(&part, ",\"id\":");
            jw_raw(&part, id.p, id.n);
        }
        jw_rawz(&part, "}}");
        if (g->last_fr) {
            w->n = g->tail;     /* a user turn of function responses: this one joins it */
            if (w->p)
                w->p[w->n] = 0;
            jw_rawz(w, ",");
        } else {
            jw_rawz(w, g->n ? "," : "");
            jw_rawz(w, "{\"parts\":[");
            g->n++;
        }
        jw_raw(w, part.p, part.n);
        at = w->n;
        jw_rawz(w, "],\"role\":\"user\"}");
        g->tail = at;
        g->last_fr = 1;
        jw_free(&part);
    }
    if (nimg > 0 && !multi) {
        jw_rawz(w, ",{\"parts\":[{\"text\":\"Tool result image:\"}");
        for (i = 0; i < nimg; i++) {
            jw_rawz(w, ",");
            w_inline(w, imgs[i], 1);
        }
        jw_rawz(w, "],\"role\":\"user\"}");
        g->n++;
        g->last_fr = 0;
    }
    jw_free(&text);
    return w->oom ? -1 : 0;
}

/* ---- the body ---- */

static int g_body(const llm_ctx *x, jw *w)
{
    const llm_model *md = x->model;
    int i, rc = -1, strict_any = 0, enabled = 0, lv;
    long maxt = llm_ctx_max_tokens(x);
    int strict_ok = gemini_major(md->id) >= 3;
    llm_tx tx;
    gc g;
    if (llm_tx_plan(x, 0, &tx)) {
        llm_tx_free(&tx);
        return -1;
    }
    g.w = w;
    g.n = 0;
    g.last_fr = 0;
    g.tail = 0;
    jw_rawz(w, "{\"contents\":[");
    for (i = 0; i < x->nmsg; i++) {
        jv m, r, c;
        if (json_parse(x->msgs[i], x->msg_n[i], &m) || !json_get(m, "role", &r))
            goto out;
        if (json_streq(r, "user")) {
            if (!json_get(m, "content", &c) || w_user(&g, c))
                goto out;
        } else if (json_streq(r, "assistant")) {
            if (w_assistant(&g, md, m))
                goto out;
        } else if (json_streq(r, "toolResult")) {
            if (w_result(&g, md, m))
                goto out;
        }                       /* a system message: collapsed into x->system / x->tools by the caller */
    }
    jw_rawz(w, "]");
    if (x->system && *x->system) {
        jw_rawz(w, ",\"systemInstruction\":{\"parts\":[{\"text\":");
        jw_strz(w, x->system);
        jw_rawz(w, "}],\"role\":\"user\"}");
    }
    if (tx.ntl > 0) {
        jw sch;
        jv v;
        jw_init(&sch);
        jw_rawz(w, ",\"tools\":[{\"functionDeclarations\":[");
        for (i = 0; i < tx.ntl; i++) {
            jv t = tx.tl[i];
            int st;
            if (!json_get(t, "name", &v)) {
                jw_free(&sch);
                goto out;
            }
            jw_rawz(w, i ? "," : "");
            jw_rawz(w, "{\"name\":");
            jw_raw(w, v.p, v.n);
            if (json_get(t, "description", &v)) {
                jw_rawz(w, ",\"description\":");
                jw_raw(w, v.p, v.n);
            }
            st = llm_tool_strict(t, strict_ok, LCS_NONE, &sch, 0, 0);
            if (st < 0) {
                jw_free(&sch);
                goto out;
            }
            strict_any |= st == 1;
            jw_rawz(w, ",\"parametersJsonSchema\":");
            if (st == 1)
                jw_raw(w, sch.p, sch.n);
            else if (json_get(t, "parameters", &v))
                jw_raw(w, v.p, v.n);
            else
                jw_rawz(w, "{\"type\":\"object\"}");
            jw_rawz(w, "}");
        }
        jw_rawz(w, "]}]");
        jw_free(&sch);
        if (strict_any)
            jw_rawz(w, ",\"toolConfig\":{\"functionCallingConfig\":{\"mode\":\"VALIDATED\"}}");
    }
    jw_rawz(w, ",\"generationConfig\":{");
    if (maxt > 0) {
        jw_rawz(w, "\"maxOutputTokens\":");
        jw_long(w, maxt);
    }
    /* streamSimple: no level, or one that clamps to off, disables thinking; else the level or budget of the model */
    lv = llm_think_clamp(md, x->thinking);
    enabled = x->thinking != LT_OFF && lv != LT_OFF;
    if (enabled && md->reasoning) {
        jw_rawz(w, maxt > 0 ? ",\"thinkingConfig\":{\"includeThoughts\":true," : "\"thinkingConfig\":{\"includeThoughts\":true,");
        if (uses_level(md->id)) {
            const char *wd = level_word(md, lv);
            if (!wd)
                goto out;
            jw_rawz(w, "\"thinkingLevel\":");
            jw_strz(w, wd);
        } else {
            jw_rawz(w, "\"thinkingBudget\":");
            jw_long(w, budget(md->id, lv));
        }
        jw_rawz(w, "}");
    } else if (!enabled && md->reasoning) {
        /* getDisabledGoogleThinkingConfig */
        jw_rawz(w, maxt > 0 ? ",\"thinkingConfig\":{" : "\"thinkingConfig\":{");
        if (!uses_level(md->id)) {
            jw_rawz(w, "\"thinkingBudget\":0");
        } else {
            int fb = llm_think_clamp(md, LT_OFF);
            if (fb == LT_OFF)
                jw_rawz(w, "\"thinkingBudget\":0");
            else {
                const char *wd = level_word(md, fb);
                if (!wd)
                    goto out;
                jw_rawz(w, "\"thinkingLevel\":");
                jw_strz(w, wd);
            }
        }
        jw_rawz(w, "}");
    }
    jw_rawz(w, "}}");
    rc = w->oom ? -1 : 0;
out:
    llm_tx_free(&tx);
    return rc;
}

/* ---- the decoder ---- */

#define GD_IDS 64

typedef struct gd_dec {
    int started, done, have_id;
    int cur;                    /* the open block: 0 none, 1 text, 2 thinking */
    long cur_index, next;
    int stop;                   /* LS_PENDING until a finish reason */
    char raw[48];
    int ntools;
    char ids[GD_IDS][100];
} gd_dec;

static void *g_new(void)
{
    gd_dec *d = (gd_dec *)malloc(sizeof(gd_dec));
    if (d) {
        memset(d, 0, sizeof(*d));
        d->stop = LS_PENDING;
    }
    return d;
}

static void g_free(void *d)
{
    free(d);
}

static void emit(const llm_sink *k, const llm_event *e)
{
    if (k && k->ev)
        k->ev(k->u, e);
}

static void gfail(gd_dec *d, const llm_sink *k, const char *type, const char *msg, const char *raw)
{
    llm_event e;
    llm_event_init(&e, LE_ERROR);
    e.stop = LS_ERROR;
    e.err_type = type;
    e.p = msg;
    e.n = (long)strlen(msg);
    e.stop_raw = raw && *raw ? raw : 0;
    emit(k, &e);
    d->done = 1;
}

static void start(gd_dec *d, const llm_sink *k, const char *id)
{
    llm_event e;
    llm_event_init(&e, LE_START);
    e.p = id;
    e.n = (long)strlen(id);
    emit(k, &e);
    d->started = 1;
}

static void end_cur(gd_dec *d, const llm_sink *k)
{
    llm_event e;
    if (!d->cur)
        return;
    llm_event_init(&e, LE_BLOCK_END);
    e.index = d->cur_index;
    emit(k, &e);
    d->cur = 0;
}

/* JSON.stringify of a parsed value: no whitespace, strings re-escaped the way JSON.stringify writes them */
static void compact(jw *w, jv v)
{
    jit it;
    jv k, x;
    int n = 0;
    switch (json_type(v)) {
    case J_OBJ:
        jw_rawz(w, "{");
        json_iter(v, &it);
        while (json_next(&it, &k, &x)) {
            char *d;
            long l;
            jw_rawz(w, n++ ? "," : "");
            d = json_strdup(k, &l);
            jw_str(w, d ? d : "", d ? l : 0);
            free(d);
            jw_rawz(w, ":");
            compact(w, x);
        }
        jw_rawz(w, "}");
        break;
    case J_ARR:
        jw_rawz(w, "[");
        json_iter(v, &it);
        while (json_next(&it, 0, &x)) {
            jw_rawz(w, n++ ? "," : "");
            compact(w, x);
        }
        jw_rawz(w, "]");
        break;
    case J_STR:
        {
            char *d;
            long l;
            d = json_strdup(v, &l);
            jw_str(w, d ? d : "", d ? l : 0);
            free(d);
        }
        break;
    default:
        jw_raw(w, v.p, v.n);
        break;
    }
}

/* retainThoughtSignature: a non-empty signature replaces the block's */
static void sig_part(gd_dec *d, const llm_sink *k, jv part)
{
    jv s;
    llm_event e;
    char *x;
    long l;
    if (!json_get(part, "thoughtSignature", &s) || json_type(s) != J_STR)
        return;
    x = json_strdup(s, &l);
    if (x && l > 0) {
        llm_event_init(&e, LE_SIG_DELTA);
        e.index = d->cur_index;
        e.p = x;
        e.n = l;
        e.replace = 1;
        emit(k, &e);
    }
    free(x);
}

static void text_part(gd_dec *d, const llm_sink *k, jv part, jv text)
{
    jv th;
    int thinking = json_get(part, "thought", &th) && json_type(th) == J_TRUE;
    llm_event e;
    char *x;
    long l;
    if (!d->cur || (thinking && d->cur != 2) || (!thinking && d->cur != 1)) {
        end_cur(d, k);
        llm_event_init(&e, LE_BLOCK_START);
        e.kind = thinking ? LB_THINKING : LB_TEXT;
        e.index = d->cur_index = d->next++;
        emit(k, &e);
        d->cur = thinking ? 2 : 1;
    }
    x = json_strdup(text, &l);
    if (x && l > 0) {
        llm_event_init(&e, LE_DELTA);
        e.index = d->cur_index;
        e.p = x;
        e.n = l;
        emit(k, &e);
    }
    free(x);
    sig_part(d, k, part);
}

static void call_part(gd_dec *d, const llm_sink *k, jv fc, jv part)
{
    jv v, s;
    char id[100], name[64];
    llm_event e;
    jw args, nid;
    int i, dup = 0, have = 0;
    end_cur(d, k);
    id[0] = name[0] = 0;
    if (json_get(fc, "name", &v) && json_type(v) == J_STR)
        json_str(v, name, sizeof(name));
    if (json_get(fc, "id", &v) && json_type(v) == J_STR) {
        json_str(v, id, sizeof(id));
        have = id[0] != 0;
    }
    for (i = 0; have && i < d->ntools; i++)
        if (!strcmp(d->ids[i], id))
            dup = 1;
    if (!have || dup) {
        llm_ts t = wall();
        char num[24];
        jw_init(&nid);
        jw_rawz(&nid, name[0] ? name : "undefined");
        jw_rawz(&nid, "_");
        cl_ltoa(t.sec, num);
        jw_rawz(&nid, num);
        num[0] = (char)('0' + t.ms / 100 % 10);
        num[1] = (char)('0' + t.ms / 10 % 10);
        num[2] = (char)('0' + t.ms % 10);
        num[3] = 0;
        jw_rawz(&nid, num);
        jw_rawz(&nid, "_");
        cl_ltoa(++id_counter, num);
        jw_rawz(&nid, num);
        cl_copy(id, nid.p ? nid.p : "", sizeof(id));
        jw_free(&nid);
    }
    if (d->ntools < GD_IDS)
        cl_copy(d->ids[d->ntools], id, sizeof(d->ids[0]));
    d->ntools++;
    jw_init(&args);
    if (json_get(fc, "args", &v) && json_type(v) != J_NULL)
        compact(&args, v);
    else
        jw_rawz(&args, "{}");
    llm_event_init(&e, LE_BLOCK_START);
    e.kind = LB_TOOLCALL;
    e.index = d->next++;
    e.id = id;
    e.name = name;
    if (json_get(part, "thoughtSignature", &s) && json_type(s) == J_STR && s.n > 2) {
        e.sig = s.p + 1;        /* a signature is base64: nothing to unescape */
        e.sn = s.n - 2;
    }
    emit(k, &e);
    {
        long idx = e.index;
        llm_event_init(&e, LE_DELTA);
        e.index = idx;
        e.p = args.p;
        e.n = args.n;
        emit(k, &e);
        llm_event_init(&e, LE_BLOCK_END);
        e.index = idx;
        emit(k, &e);
    }
    jw_free(&args);
}

/* mapStopReason: LS_* for a finish reason; -1 for a reason pi has no case for (it throws) */
static int map_finish(const char *r)
{
    static const char *const bad[] = { "BLOCKLIST", "PROHIBITED_CONTENT", "SPII", "SAFETY", "IMAGE_SAFETY",
        "IMAGE_PROHIBITED_CONTENT", "IMAGE_RECITATION", "IMAGE_OTHER", "RECITATION", "FINISH_REASON_UNSPECIFIED",
        "OTHER", "LANGUAGE", "MALFORMED_FUNCTION_CALL", "UNEXPECTED_TOOL_CALL", "TOO_MANY_TOOL_CALLS", "NO_IMAGE", 0 };
    int i;
    if (!strcmp(r, "STOP"))
        return LS_STOP;
    if (!strcmp(r, "MAX_TOKENS"))
        return LS_LENGTH;
    for (i = 0; bad[i]; i++)
        if (!strcmp(r, bad[i]))
            return LS_ERROR;
    return -1;
}

static int g_feed(void *dv, const char *event, const char *data, long n, const llm_sink *k)
{
    gd_dec *d = (gd_dec *)dv;
    jv root, cands, cand, content, parts, part, v, um;
    jit it;
    (void)event;
    llm_feeds_google++;
    if (!d || d->done)
        return 0;
    if (json_parse(data, n, &root) || json_type(root) != J_OBJ)
        return -1;
    if (!d->started)
        start(d, k, "");
    if (!d->have_id && json_get(root, "responseId", &v) && json_type(v) == J_STR && v.n > 2) {
        char id[96];
        json_str(v, id, sizeof(id));
        d->have_id = 1;
        start(d, k, id);        /* the id is the first non-empty one of the stream (output.responseId ||=) */
    }
    if (json_get(root, "candidates", &cands) && json_type(cands) == J_ARR && json_count(cands) > 0) {
        json_iter(cands, &it);
        json_next(&it, 0, &cand);
        if (json_get(cand, "content", &content) && json_type(content) == J_OBJ &&
            json_get(content, "parts", &parts) && json_type(parts) == J_ARR) {
            json_iter(parts, &it);
            while (json_next(&it, 0, &part)) {
                jv fc;
                if (json_type(part) != J_OBJ)
                    continue;
                if (json_get(part, "text", &v) && json_type(v) == J_STR)
                    text_part(d, k, part, v);
                if (json_get(part, "functionCall", &fc) && json_type(fc) == J_OBJ)
                    call_part(d, k, fc, part);
            }
        }
        if (json_get(cand, "finishReason", &v) && json_type(v) == J_STR && v.n > 2) {
            int st;
            json_str(v, d->raw, sizeof(d->raw));
            st = map_finish(d->raw);
            if (st < 0) {
                char m[96];
                end_cur(d, k);
                cl_copy(m, "Unhandled stop reason: ", sizeof(m));
                cl_cat(m, d->raw, sizeof(m));
                gfail(d, k, "error", m, d->raw);
                return 0;
            }
            d->stop = st;
        }
    }
    if (json_get(root, "usageMetadata", &um) && json_type(um) == J_OBJ) {
        long pt = 0, ct = 0, th = 0, cc = 0;
        llm_event e;
        if (json_get(um, "promptTokenCount", &v))
            pt = json_long(v, 0);
        if (json_get(um, "candidatesTokenCount", &v))
            ct = json_long(v, 0);
        if (json_get(um, "thoughtsTokenCount", &v))
            th = json_long(v, 0);
        if (json_get(um, "cachedContentTokenCount", &v))
            cc = json_long(v, 0);
        llm_event_init(&e, LE_USAGE);
        e.in = pt - cc;         /* google-generative-ai.ts:233 */
        e.out = ct + th;
        e.cache_r = cc;
        e.cache_w = 0;
        e.reasoning = th;
        e.has_reasoning = 1;
        emit(k, &e);
    }
    return 0;
}

/* the body ended (google-generative-ai.ts:253-296): close the open block, then the answer is an error without a finish
 * reason or with a failing one, else done (a call in the answer makes a plain stop a toolUse) */
static void g_fin(void *dv, const llm_sink *k)
{
    gd_dec *d = (gd_dec *)dv;
    llm_event e;
    if (!d || d->done)
        return;
    if (!d->started)
        start(d, k, "");
    end_cur(d, k);
    if (d->stop == LS_PENDING) {
        gfail(d, k, "error", "Google stream ended without a finish reason", 0);
        return;
    }
    if (d->stop == LS_ERROR) {
        char m[96];
        if (d->raw[0]) {
            cl_copy(m, "Provider stopped with: ", sizeof(m));
            cl_cat(m, d->raw, sizeof(m));
        } else
            cl_copy(m, "An unknown error occurred", sizeof(m));
        gfail(d, k, "error", m, d->raw);
        return;
    }
    if (d->stop == LS_STOP && d->ntools > 0)
        d->stop = LS_TOOLUSE;
    llm_event_init(&e, LE_STOP);
    e.stop = d->stop;
    e.stop_raw = d->raw[0] ? d->raw : 0;
    emit(k, &e);
    llm_event_init(&e, LE_DONE);
    e.stop = d->stop;
    emit(k, &e);
    d->done = 1;
}

const llm_backend llm_be_google = {
    "google-generative-ai", "x-goog-api-key", "/models", g_head, g_body, 0, g_new, g_feed, g_free,
    "Google stream ended without a finish reason", g_route, g_fin
};
