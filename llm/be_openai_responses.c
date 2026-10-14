/* be_openai_responses -- the openai-responses backend (Phase F audit FA4):
 * POST {baseUrl}/responses with a Bearer key, the body from pi-ai messages
 * (pi-ai 1.0.4 api/openai-responses.js buildParams and
 * api/openai-responses-shared.js convertResponsesMessages /
 * convertResponsesTools), and the decoder from the Responses event stream
 * (processResponsesStream) to the neutral events.
 *
 * What is ported: the system prompt as a developer/system input item, user,
 * assistant (reasoning items replayed from the thinking signature, text as
 * message items with the id the text signature names, function calls with
 * their fc_ item id) and tool-result items; function tools (strict:false when
 * compat supportsStrictMode); reasoning effort through the model's
 * thinkingLevelMap, summary "auto", include encrypted reasoning, store false,
 * prompt_cache_key and the session headers from the session id.
 * Later system messages and tool additions: with supportsMidConvoSystemMessages they stay in place (the update text as an
 * input item) and, with supportsAdditionalTools, the tools they add go out as additional_tools items (llm/transcript.c,
 * pi-ai utils/transcript.js); else the leading system message and the tool list are the collapsed ones.
 * Not ported (COMPATIBILITY.md): grammar/custom tools, tool search, service tiers, sampling params, PI_CACHE_RETENTION long, the
 * Sign in with ChatGPT token rules, context-window clamping of max_output_tokens.
 * Portable C89, host-tested (tests/test_pi_llm.c, goldens from pi). */
#include <stdlib.h>
#include <string.h>
#include "backend.h"
#include "httperr.h"
#include "registry.h"
#include "constrain.h"
#include "estimate.h"
#include "handoff.h"
#include "transcript.h"
#include "../claude/http.h"
#include "../claude/util.h"

long llm_feeds_responses;

static long r_head(const char *host, int port, int tls, const char *path, const char *key,
                   const char *user_agent, long body_len, char *out, long cap)
{
    http_hdr h[4];
    char bearer[4200];
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

/* createClient: with a session id pi sends session_id and x-client-request-id (the "openai" affinity format) */
static long r_xhead(const llm_ctx *c, char *out, long cap)
{
    long n;
    if (!c->session_id || !*c->session_id)
        return 0;
    out[0] = 0;
    cl_copy(out, "session_id: ", cap);
    cl_cat(out, c->session_id, cap);
    cl_cat(out, "\r\nx-client-request-id: ", cap);
    cl_cat(out, c->session_id, cap);
    cl_cat(out, "\r\n", cap);
    n = (long)strlen(out);
    return n < cap - 1 ? n : 0;
}

/* ---- the body ---- */

static int is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* pi's parseTextSignature: the item id (and phase) a text block's signature names.
 * 1 with id/phase set (phase "" none), 0 no signature */
static int text_sig(jv b, char *id, long idcap, char *phase, long pcap)
{
    jv s, o, v;
    char *d;
    long l;
    id[0] = phase[0] = 0;
    if (!json_get(b, "textSignature", &s) || json_type(s) != J_STR)
        return 0;
    d = json_strdup(s, &l);
    if (!d)
        return 0;
    if (!l) {
        free(d);
        return 0;
    }
    if (d[0] == '{' && json_parse(d, l, &o) == 0 && json_type(o) == J_OBJ && json_get(o, "v", &v) &&
        json_type(v) == J_NUM && json_long(v, 0) == 1 && json_get(o, "id", &v) && json_type(v) == J_STR) {
        json_str(v, id, idcap);
        if (json_get(o, "phase", &v) && (json_streq(v, "commentary") || json_streq(v, "final_answer")))
            json_str(v, phase, pcap);
    } else
        cl_copy(id, d, idcap);
    free(d);
    return 1;
}

/* the text blocks of a content array joined with "\n" (convertToolResultOutput's textResult) */
static void join_text(jv c, jw *s, int *any)
{
    jit it;
    jv b, t, v;
    int k = 0;
    *any = 0;
    if (json_type(c) == J_STR) {
        char *x;
        long l;
        x = json_strdup(c, &l);
        if (x) {
            jw_raw(s, x, l);
            free(x);
        }
        *any = 1;
        return;
    }
    json_iter(c, &it);
    while (json_next(&it, 0, &b))
        if (json_get(b, "type", &t) && json_streq(t, "text") && json_get(b, "text", &v)) {
            char *x;
            long l;
            x = json_strdup(v, &l);
            if (k++)
                jw_rawz(s, "\n");
            if (x) {
                jw_raw(s, x, l);
                free(x);
            }
            *any = 1;
        }
}

static int has_img(jv c)
{
    jit it;
    jv b, t;
    if (json_type(c) != J_ARR)
        return 0;
    json_iter(c, &it);
    while (json_next(&it, 0, &b))
        if (json_get(b, "type", &t) && json_streq(t, "image"))
            return 1;
    return 0;
}

/* data:<mime>;base64,<data> as a JSON string */
static int w_image(jw *w, jv b)
{
    jv d, m;
    char *dd, *mm;
    long dl, ml;
    jw u;
    if (!json_get(b, "data", &d) || !json_get(b, "mimeType", &m))
        return -1;
    dd = json_strdup(d, &dl);
    mm = json_strdup(m, &ml);
    jw_init(&u);
    jw_rawz(&u, "data:");
    jw_raw(&u, mm ? mm : "", mm ? ml : 0);
    jw_rawz(&u, ";base64,");
    jw_raw(&u, dd ? dd : "", dd ? dl : 0);
    jw_rawz(w, "{\"type\":\"input_image\",\"detail\":\"auto\",\"image_url\":");
    jw_str(w, u.p, u.n);
    jw_raw(w, "}", 1);
    jw_free(&u);
    free(dd);
    free(mm);
    return 0;
}

/* the part of a tool-call id before "|" as a JSON string */
static void w_call_id(jw *w, jv id)
{
    char *d;
    long l, i;
    d = json_strdup(id, &l);
    if (!d) {
        jw_rawz(w, "\"\"");
        return;
    }
    for (i = 0; i < l && d[i] != '|'; i++)
        ;
    jw_str(w, d, i);
    free(d);
}

static int w_user(jw *w, jv c)
{
    jit it;
    jv b, t, v;
    int k = 0;
    if (json_type(c) == J_STR) {
        jw_rawz(w, "{\"role\":\"user\",\"content\":[{\"type\":\"input_text\",\"text\":");
        jw_raw(w, c.p, c.n);
        jw_rawz(w, "}]}");
        return 1;
    }
    if (json_type(c) != J_ARR || json_count(c) == 0)
        return 0;               /* pi: nothing for an empty content */
    jw_rawz(w, "{\"role\":\"user\",\"content\":[");
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        jw_rawz(w, k++ ? "," : "");
        if (json_get(b, "type", &t) && json_streq(t, "text") && json_get(b, "text", &v)) {
            jw_rawz(w, "{\"type\":\"input_text\",\"text\":");
            jw_raw(w, v.p, v.n);
            jw_raw(w, "}", 1);
        } else if (w_image(w, b))
            return -1;
    }
    jw_rawz(w, "]}");
    return 1;
}

/* one assistant message's items, comma separated (k counts them); msgidx is pi's msgIndex */
static int w_assistant(jw *w, const llm_model *md, jv m, long msgidx, int *k)
{
    jv c, b, t, v, id, nm, args, pv, av, mv;
    jit it;
    int tb = 0, same_pa = 0, diff = 0;
    if (!json_get(m, "content", &c) || json_type(c) != J_ARR)
        return 0;
    if (json_get(m, "provider", &pv) && json_get(m, "api", &av) && json_get(m, "model", &mv)) {
        same_pa = json_streq(pv, md->provider) && json_streq(av, md->api);
        diff = same_pa && !json_streq(mv, md->id);
    }
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        if (!json_get(b, "type", &t))
            continue;
        if (json_streq(t, "thinking")) {
            char *sig;
            long sl;
            jv o;
            if (!json_get(b, "thinkingSignature", &v) || json_type(v) != J_STR)
                continue;
            sig = json_strdup(v, &sl);
            if (!sig || !sl) {
                free(sig);
                continue;
            }
            if (json_parse(sig, sl, &o)) {
                free(sig);
                return -1;
            }
            jw_rawz(w, (*k)++ ? "," : "");
            jw_raw(w, sig, sl);
            free(sig);
        } else if (json_streq(t, "text")) {
            char sid[300], ph[24], fb[48], num[24];
            const char *useid;
            int has = text_sig(b, sid, sizeof(sid), ph, sizeof(ph));
            if (!json_get(b, "text", &v))
                continue;
            /* fallback id msg_pi_<msgIndex>[_<textBlockIndex>] */
            cl_copy(fb, "msg_pi_", sizeof(fb));
            cl_ltoa(msgidx, num);
            cl_cat(fb, num, sizeof(fb));
            if (tb) {
                cl_cat(fb, "_", sizeof(fb));
                cl_ltoa((long)tb, num);
                cl_cat(fb, num, sizeof(fb));
            }
            tb++;
            useid = has && sid[0] ? sid : fb;
            jw_rawz(w, (*k)++ ? "," : "");
            jw_rawz(w, "{\"type\":\"message\",\"role\":\"assistant\",\"content\":[{\"type\":\"output_text\",\"text\":");
            jw_raw(w, v.p, v.n);
            jw_rawz(w, ",\"annotations\":[]}],\"status\":\"completed\",\"id\":");
            if (strlen(useid) > 64) {
                char h[24], mid[40];
                llm_short_hash(useid, h, sizeof(h));
                cl_copy(mid, "msg_", sizeof(mid));
                cl_cat(mid, h, sizeof(mid));
                jw_strz(w, mid);
            } else
                jw_strz(w, useid);
            if (has && ph[0]) {
                jw_rawz(w, ",\"phase\":");
                jw_strz(w, ph);
            }
            jw_raw(w, "}", 1);
        } else if (json_streq(t, "toolCall")) {
            char *d;
            long l, bar;
            if (!json_get(b, "id", &id) || !json_get(b, "name", &nm))
                continue;
            d = json_strdup(id, &l);
            if (!d)
                return -1;
            for (bar = 0; bar < l && d[bar] != '|'; bar++)
                ;
            jw_rawz(w, (*k)++ ? "," : "");
            jw_rawz(w, "{\"type\":\"function_call\"");
            /* the item id only when it is an fc_ id and the model did not change under the same api */
            if (bar < l) {
                long e = bar + 1;
                while (e < l && d[e] != '|')
                    e++;
                if (e - bar - 1 >= 3 && !memcmp(d + bar + 1, "fc_", 3) && !diff) {
                    jw_rawz(w, ",\"id\":");
                    jw_str(w, d + bar + 1, e - bar - 1);
                }
            }
            jw_rawz(w, ",\"call_id\":");
            jw_str(w, d, bar);
            jw_rawz(w, ",\"name\":");
            jw_raw(w, nm.p, nm.n);
            jw_rawz(w, ",\"arguments\":");
            if (json_get(b, "arguments", &args))
                jw_str(w, args.p, args.n);      /* JSON.stringify(arguments): the object's text */
            else
                jw_rawz(w, "\"{}\"");
            jw_raw(w, "}", 1);
            free(d);
        }
    }
    return 0;
}

static int w_result(jw *w, const llm_model *md, jv m)
{
    jv id, c;
    jw s;
    int any;
    if (!json_get(m, "toolCallId", &id) || !json_get(m, "content", &c))
        return -1;
    jw_rawz(w, "{\"type\":\"function_call_output\",\"call_id\":");
    w_call_id(w, id);
    jw_rawz(w, ",\"output\":");
    jw_init(&s);
    join_text(c, &s, &any);
    if (!has_img(c) || !md->image) {
        if (!has_img(c) && !s.n)
            jw_strz(w, "(no tool output)");
        else if (!s.n)
            jw_strz(w, "(see attached image)");
        else
            jw_str(w, s.p, s.n);
    } else {
        jit it;
        jv b, t;
        int k = 0;
        jw_raw(w, "[", 1);
        if (s.n) {
            jw_rawz(w, "{\"type\":\"input_text\",\"text\":");
            jw_str(w, s.p, s.n);
            jw_raw(w, "}", 1);
            k++;
        }
        json_iter(c, &it);
        while (json_next(&it, 0, &b))
            if (json_get(b, "type", &t) && json_streq(t, "image")) {
                jw_rawz(w, k++ ? "," : "");
                if (w_image(w, b)) {
                    jw_free(&s);
                    return -1;
                }
            }
        jw_raw(w, "]", 1);
    }
    jw_free(&s);
    jw_raw(w, "}", 1);
    return 0;
}

static int g_codex;             /* body_ex is writing an openai-codex-responses request: the default strict is null */

/* convertResponsesTools: the tools as function declarations (strict when the model supports it and the tool's
 * constrainedSampling allows it), a JSON array appended. 0, -1 */
static int w_tools(jw *w, const jv *tl, int n, int strict)
{
    int i, st;
    jw sch;
    jv v;
    jw_init(&sch);
    jw_raw(w, "[", 1);
    for (i = 0; i < n; i++) {
        jv t = tl[i];
        if (!json_get(t, "name", &v)) {
            jw_free(&sch);
            return -1;
        }
        jw_rawz(w, i ? "," : "");
        jw_rawz(w, "{\"type\":\"function\",\"name\":");
        jw_raw(w, v.p, v.n);
        if (json_get(t, "description", &v)) {
            jw_rawz(w, ",\"description\":");
            jw_raw(w, v.p, v.n);
        }
        st = llm_tool_strict(t, strict, LCS_NONE, &sch, 0, 0);
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
        if (strict)
            jw_rawz(w, st == 1 ? ",\"strict\":true" : g_codex ? ",\"strict\":null" : ",\"strict\":false");
        jw_raw(w, "}", 1);
    }
    jw_free(&sch);
    jw_raw(w, "]", 1);
    return 0;
}

/* a system message's toolsAdded as an additional_tools item (pi: only the ones the transcript anchors); 1 written,
 * 0 nothing to add, -1 */
static int w_added_tools(jw *w, jv ta, int strict)
{
    jit it;
    jv t, *tls;
    int nt = 0, rc;
    if (json_type(ta) != J_ARR)
        return 0;
    json_iter(ta, &it);
    while (json_next(&it, 0, &t))
        nt++;
    if (!nt)
        return 0;
    tls = (jv *)malloc((size_t)nt * sizeof(jv));
    if (!tls)
        return -1;
    nt = 0;
    json_iter(ta, &it);
    while (json_next(&it, 0, &t))
        tls[nt++] = t;
    jw_rawz(w, "{\"type\":\"additional_tools\",\"role\":\"developer\",\"tools\":");
    rc = w_tools(w, tls, nt, strict);
    jw_raw(w, "}", 1);
    free(tls);
    return rc ? -1 : 1;
}

/* an instruction item for a system message: pi's getSystemMessageText of the leading one, renderSystemMessageUpdate of
 * a later one; nothing written (0) when the text is empty, else 1 */
static int w_sys_item(jw *w, const llm_model *md, jv m, int update)
{
    long at = w->n, ts;
    jw_rawz(w, md->reasoning && md->compat.developer_role != 0 ? "{\"role\":\"developer\",\"content\":"
                                                              : "{\"role\":\"system\",\"content\":");
    ts = w->n;
    if (update)
        llm_tx_sys_update(w, m);
    else
        llm_tx_sys_text(w, m);
    if (w->n == ts || w->n == ts + 2) {
        w->n = at;                  /* no text: no item */
        if (w->p)
            w->p[w->n] = 0;
        return 0;
    }
    jw_raw(w, "}", 1);
    return 1;
}

/* the Responses request for the model named `name` on the wire (the model's id; azure: the deployment's name);
 * strict: tools go out with their strict flag (openai: supportsStrictMode on, azure: on unless the model says off) */
static int body_in(const llm_ctx *x, jw *w, const char *name, int strict, int codex);

static int body_ex(const llm_ctx *x, jw *w, const char *name, int strict, int codex)
{
    int rc;
    g_codex = codex;
    rc = body_in(x, w, name, strict, codex);
    g_codex = 0;
    return rc;
}

static int body_in(const llm_ctx *x, jw *w, const char *name, int strict, int codex)
{
    const llm_model *md = x->model;
    int i, k = 0, lv, rc = -1;
    long msgidx = 0, maxt = llm_ctx_max_tokens(x);
    int mid = md->compat.mid_system == 1;
    int add = md->compat.add_tools == 1;
    llm_tx tx;
    const jv *reqt;
    int nreq, anchors;
    /* pi: resolveTranscript (later system messages stay in place or collapse into the leading one) and
     * resolveTranscriptTools (a model that takes tool additions keeps the initial tools at the top and loads later ones
     * where they appear, unless a tool was removed or declared twice) */
    if (llm_tx_plan(x, mid, &tx)) {
        llm_tx_free(&tx);
        return -1;
    }
    anchors = add && tx.additive;
    reqt = anchors ? tx.itl : tx.tl;
    nreq = anchors ? tx.nitl : tx.ntl;
    jw_rawz(w, "{\"model\":");
    jw_strz(w, name);
    if (codex) {
        /* openai-codex-responses buildRequestBody: store, stream, then the instructions (the leading system message's
         * text, else the prompt, else pi's default) and the input without a system item */
        jw ins;
        jw_init(&ins);
        if (tx.has_init)
            llm_tx_sys_text(&ins, tx.init);
        else if (x->system && *x->system)
            jw_strz(&ins, x->system);
        jw_rawz(w, ",\"store\":false,\"stream\":true,\"instructions\":");
        if (ins.n > 2)
            jw_raw(w, ins.p, ins.n);
        else
            jw_rawz(w, "\"You are a helpful assistant.\"");
        jw_free(&ins);
    }
    jw_rawz(w, ",\"input\":[");
    if (codex) {
        ;
    } else if (tx.has_init) {
        k += w_sys_item(w, md, tx.init, 0);
    } else if (x->system && *x->system) {
        jw_rawz(w, md->reasoning && md->compat.developer_role != 0 ? "{\"role\":\"developer\",\"content\":"
                                                                    : "{\"role\":\"system\",\"content\":");
        jw_strz(w, x->system);
        jw_raw(w, "}", 1);
        k++;
    }
    for (i = tx.has_init ? 1 : 0; i < x->nmsg; i++) {
        jv m, r, c;
        long mark = w->n;
        int wrote = 0;
        if (json_parse(x->msgs[i], x->msg_n[i], &m) || !json_get(m, "role", &r))
            goto out;
        jw_rawz(w, k ? "," : "");
        if (json_streq(r, "user")) {
            if (!json_get(m, "content", &c))
                goto out;
            wrote = w_user(w, c);
            if (wrote < 0)
                goto out;
        } else if (json_streq(r, "assistant")) {
            int items = 0;
            if (w_assistant(w, md, m, msgidx, &items))
                goto out;
            wrote = items > 0;
        } else if (json_streq(r, "toolResult")) {
            if (w_result(w, md, m))
                goto out;
            wrote = 1;
        } else if (mid && json_streq(r, "system")) {
            /* a later system message: the tools it adds (additional_tools, when anchored), then its update text */
            int n1 = 0;
            jv ta;
            if (anchors && json_get(m, "toolsAdded", &ta)) {
                n1 = w_added_tools(w, ta, strict);
                if (n1 < 0)
                    goto out;
                k += n1;
            }
            if (n1) {
                long at = w->n;
                jw_rawz(w, ",");
                if (w_sys_item(w, md, m, 1))
                    k++;
                else {
                    w->n = at;
                    if (w->p)
                        w->p[w->n] = 0;
                }
            } else if (w_sys_item(w, md, m, 1)) {
                n1 = 1;
                k++;
            }
            if (!n1) {
                w->n = mark;
                if (w->p)
                    w->p[w->n] = 0;
            }
            msgidx++;
            continue;
        } else {
            msgidx++;           /* a system message the model does not take: not sent, but pi counts it */
            w->n = mark;
            if (w->p)
                w->p[w->n] = 0;
            continue;
        }
        if (!wrote) {
            w->n = mark;        /* pi: `continue` before msgIndex++ */
            if (w->p)
                w->p[w->n] = 0;
            continue;
        }
        k++;
        msgidx++;
    }
    if (codex) {
        /* text, include, prompt_cache_key, tool_choice, parallel_tool_calls, tools, reasoning (pi's order) */
        jw_rawz(w, "],\"text\":{\"verbosity\":\"low\"},\"include\":[\"reasoning.encrypted_content\"]");
        if (x->session_id && *x->session_id) {
            const char *s = x->session_id;
            long n = 0, chars = 0;
            while (s[n] && chars < 64) {
                n++;
                while (s[n] && (s[n] & 0xC0) == 0x80)
                    n++;
                chars++;
            }
            jw_rawz(w, ",\"prompt_cache_key\":");
            jw_str(w, s, n);
        }
        jw_rawz(w, ",\"tool_choice\":\"auto\",\"parallel_tool_calls\":true");
        if (nreq > 0) {
            jw_rawz(w, ",\"tools\":");
            if (w_tools(w, reqt, nreq, strict)) {
                rc = -1;
                goto out;
            }
        }
        lv = llm_think_clamp(md, x->thinking);
        if (md->reasoning) {
            const llm_tmap *t = &md->thinking_map[lv];
            if (lv != LT_OFF) {
                jw_rawz(w, ",\"reasoning\":{\"effort\":");
                jw_strz(w, t->kind == LTM_STR ? t->val : llm_think_name(lv));
                jw_rawz(w, ",\"summary\":\"auto\"}");
            } else if (t->kind != LTM_NULL) {
                jw_rawz(w, ",\"reasoning\":{\"effort\":");
                jw_strz(w, t->kind == LTM_STR ? t->val : "none");
                jw_raw(w, "}", 1);
            }
        }
        jw_raw(w, "}", 1);
        rc = w->oom ? -1 : 0;
        goto out;
    }
    jw_rawz(w, "],\"stream\":true");
    if (x->session_id && *x->session_id) {
        /* clampOpenAIPromptCacheKey: at most 64 characters (code points) */
        const char *s = x->session_id;
        long n = 0, chars = 0;
        while (s[n] && chars < 64) {
            n++;
            while (s[n] && (s[n] & 0xC0) == 0x80)
                n++;
            chars++;
        }
        jw_rawz(w, ",\"prompt_cache_key\":");
        jw_str(w, s, n);
    }
    jw_rawz(w, ",\"store\":false");
    if (maxt > 0) {
        jw_rawz(w, ",\"max_output_tokens\":");
        jw_long(w, maxt < 16 ? 16 : maxt);
    }
    if (nreq > 0) {
        jw_rawz(w, ",\"tools\":");
        if (w_tools(w, reqt, nreq, strict)) {
            rc = -1;
            goto out;
        }
    }
    lv = llm_think_clamp(md, x->thinking);
    if (md->reasoning) {
        const llm_tmap *t = &md->thinking_map[lv];
        if (lv != LT_OFF) {
            /* streamSimple clamps the level; buildParams maps it through thinkingLevelMap */
            jw_rawz(w, ",\"reasoning\":{\"effort\":");
            jw_strz(w, t->kind == LTM_STR ? t->val : llm_think_name(lv));
            jw_rawz(w, ",\"summary\":\"auto\"},\"include\":[\"reasoning.encrypted_content\"]");
        } else if (t->kind != LTM_NULL) {
            jw_rawz(w, ",\"reasoning\":{\"effort\":");
            jw_strz(w, t->kind == LTM_STR ? t->val : "none");
            jw_raw(w, "}", 1);
        }
    }
    jw_raw(w, "}", 1);
    rc = w->oom ? -1 : 0;
out:
    llm_tx_free(&tx);
    return rc;
}

int llm_responses_body(const llm_ctx *x, jw *w, const char *name, int strict)
{
    return body_ex(x, w, name, strict, 0);
}

/* openai-codex-responses (api/openai-codex-responses.ts buildRequestBody): the same input and tools, pi's own field order
 * and set; the tools' strict flag is null unless a tool's own constraint says true (supportsStrictMode defaults on there,
 * strict: null) */
int llm_codex_body(const llm_ctx *x, jw *w)
{
    return body_ex(x, w, x->model->id, x->model->compat.strict_tools != 0, 1);
}

static int r_body(const llm_ctx *x, jw *w)
{
    return llm_responses_body(x, w, x->model->id, x->model->compat.strict_tools == 1);
}

/* ---- the decoder ---- */

#define OR_SLOTS 32

typedef struct or_slot {
    int kind;                   /* 0 free, else 1 + LB_TEXT / LB_THINKING / LB_TOOLCALL (LB_TEXT is 0) */
    long index;                 /* contentIndex */
    int got;                    /* a delta was sent */
    int pend;                   /* thinking: a paragraph break owed before the next summary text */
    jw args;                    /* a tool call: the arguments so far */
    char name[64], id[160];
} or_slot;

typedef struct or_dec {
    int started, done, bad;
    long next;
    int ntools;
    char rid[96];
    char raw_fail[32];       /* response.failed: the status pi-ai keeps as rawStopReason, "" none */
    or_slot s[OR_SLOTS];
} or_dec;

void *llm_responses_new(void)
{
    or_dec *d = (or_dec *)malloc(sizeof(or_dec));
    int i;
    if (!d)
        return 0;
    memset(d, 0, sizeof(*d));
    for (i = 0; i < OR_SLOTS; i++)
        jw_init(&d->s[i].args);
    return d;
}

void llm_responses_free(void *dv)
{
    or_dec *d = (or_dec *)dv;
    int i;
    if (!d)
        return;
    for (i = 0; i < OR_SLOTS; i++)
        jw_free(&d->s[i].args);
    free(d);
}

static void emit(const llm_sink *k, const llm_event *e)
{
    if (k && k->ev)
        k->ev(k->u, e);
}

static void fail(or_dec *d, const llm_sink *k, const char *type, const char *msg)
{
    llm_event e;
    llm_event_init(&e, LE_ERROR);
    e.stop = LS_ERROR;
    e.err_type = type;
    e.p = msg;
    e.n = (long)strlen(msg);
    e.stop_raw = d->raw_fail[0] ? d->raw_fail : 0;
    emit(k, &e);
    d->done = 1;
}

static void start(or_dec *d, const llm_sink *k)
{
    llm_event e;
    if (d->started)
        return;
    d->started = 1;
    llm_event_init(&e, LE_START);
    e.p = d->rid;
    e.n = (long)strlen(d->rid);
    emit(k, &e);
}

static void delta(const llm_sink *k, or_slot *s, const char *p, long n)
{
    llm_event e;
    if (n <= 0)
        return;
    llm_event_init(&e, LE_DELTA);
    e.index = s->index;
    e.p = p;
    e.n = n;
    emit(k, &e);
    s->got = 1;
}

static void sig_delta(const llm_sink *k, or_slot *s, const char *p, long n)
{
    llm_event e;
    llm_event_init(&e, LE_SIG_DELTA);
    e.index = s->index;
    e.p = p;
    e.n = n;
    emit(k, &e);
}

static void end_slot(const llm_sink *k, or_slot *s)
{
    llm_event e;
    if (!s->kind)
        return;
    llm_event_init(&e, LE_BLOCK_END);
    e.index = s->index;
    emit(k, &e);
    jw_reset(&s->args);
    s->kind = 0;
}

/* createSlot: the block an output item opens; 0 for an item type this client has no block for */
static or_slot *open_slot(or_dec *d, const llm_sink *k, long oi, jv item)
{
    jv v, t;
    llm_event e;
    or_slot *s;
    if (oi < 0 || oi >= OR_SLOTS || !json_get(item, "type", &t))
        return 0;
    s = &d->s[oi];
    if (s->kind)
        return s;
    start(d, k);
    llm_event_init(&e, LE_BLOCK_START);
    if (json_streq(t, "reasoning")) {
        e.kind = LB_THINKING;
        s->kind = 1 + LB_THINKING;
    } else if (json_streq(t, "message")) {
        e.kind = LB_TEXT;
        s->kind = 1 + LB_TEXT;
    } else if (json_streq(t, "function_call")) {
        char call[100], iid[100];
        call[0] = iid[0] = 0;
        if (json_get(item, "call_id", &v))
            json_str(v, call, sizeof(call));
        if (json_get(item, "id", &v))
            json_str(v, iid, sizeof(iid));
        cl_copy(s->id, call, sizeof(s->id));
        cl_cat(s->id, "|", sizeof(s->id));
        cl_cat(s->id, iid, sizeof(s->id));
        s->name[0] = 0;
        if (json_get(item, "name", &v))
            json_str(v, s->name, sizeof(s->name));
        e.kind = LB_TOOLCALL;
        s->kind = 1 + LB_TOOLCALL;
        e.id = s->id;
        e.name = s->name;
        d->ntools++;
    } else
        return 0;
    s->index = d->next++;
    s->got = s->pend = 0;
    jw_reset(&s->args);
    e.index = s->index;
    emit(k, &e);
    if (s->kind == 1 + LB_TOOLCALL && json_get(item, "arguments", &v) && json_type(v) == J_STR) {
        long l;
        char *a = json_strdup(v, &l);
        if (a && l) {
            jw_raw(&s->args, a, l);
            delta(k, s, a, l);
        }
        free(a);
    }
    return s;
}

static or_slot *slot_of(or_dec *d, long oi, int kind)
{
    if (oi < 0 || oi >= OR_SLOTS || d->s[oi].kind != 1 + kind)
        return 0;
    return &d->s[oi];
}

/* a string member into freshly allocated memory (decoded) */
static char *str_of(jv o, const char *key, long *l)
{
    jv v;
    if (!json_get(o, key, &v) || json_type(v) != J_STR)
        return 0;
    return json_strdup(v, l);
}

/* the texts of {text}/{refusal} members of an array joined with sep */
static void join_field(jv arr, const char *key, const char *sep, jw *out)
{
    jit it;
    jv b, v;
    int k = 0;
    if (json_type(arr) != J_ARR)
        return;
    json_iter(arr, &it);
    while (json_next(&it, 0, &b))
        if (json_get(b, key, &v) && json_type(v) == J_STR) {
            char *x;
            long l;
            x = json_strdup(v, &l);
            if (k++)
                jw_rawz(out, sep);
            if (x) {
                jw_raw(out, x, l);
                free(x);
            }
        }
}

/* the stop reason of a finished response (pi's mapStopReason) */
static int map_stop(const char *status, const char *reason, char *msg, long cap)
{
    msg[0] = 0;
    if (!status[0] || !strcmp(status, "completed") || !strcmp(status, "in_progress") || !strcmp(status, "queued"))
        return LS_STOP;
    if (!strcmp(status, "incomplete")) {
        if (!strcmp(reason, "max_output_tokens"))
            return LS_LENGTH;
        if (reason[0]) {
            cl_copy(msg, "Response incomplete: ", cap);
            cl_cat(msg, reason, cap);
        } else
            cl_copy(msg, "Response incomplete without a provider reason", cap);
        return LS_ERROR;
    }
    if (!strcmp(status, "failed") || !strcmp(status, "cancelled"))
        return LS_ERROR;
    cl_copy(msg, "Unhandled stop reason: ", cap);
    cl_cat(msg, status, cap);
    return LS_ERROR;
}

static void finish(or_dec *d, const llm_sink *k, jv resp)
{
    jv v, u, det, x;
    llm_event e;
    char status[32], reason[48], raw[96], msg[200];
    int stop, i;
    status[0] = reason[0] = 0;
    if (json_get(resp, "id", &v) && json_type(v) == J_STR)
        json_str(v, d->rid, sizeof(d->rid));
    start(d, k);
    if (json_get(resp, "usage", &u) && json_type(u) == J_OBJ) {
        long in = 0, cr = 0, cw = 0;
        llm_event_init(&e, LE_USAGE);
        if (json_get(u, "input_tokens", &v))
            in = json_long(v, 0);
        if (json_get(u, "input_tokens_details", &det) && json_type(det) == J_OBJ) {
            if (json_get(det, "cached_tokens", &x))
                cr = json_long(x, 0);
            if (json_get(det, "cache_write_tokens", &x))
                cw = json_long(x, 0);
        }
        e.in = in - cr - cw > 0 ? in - cr - cw : 0;
        e.cache_r = cr;
        e.cache_w = cw;
        e.out = json_get(u, "output_tokens", &v) ? json_long(v, 0) : 0;
        e.has_reasoning = 1;    /* finalizeResponse: output_tokens_details?.reasoning_tokens || 0 */
        if (json_get(u, "output_tokens_details", &det) && json_type(det) == J_OBJ && json_get(det, "reasoning_tokens", &x))
            e.reasoning = json_long(x, 0);
        e.has_total = 1;        /* totalTokens: response.usage.total_tokens || 0 (openai-responses-shared.ts:573) */
        e.total = json_get(u, "total_tokens", &x) ? json_long(x, 0) : 0;
        emit(k, &e);
    }
    if (json_get(resp, "status", &v) && json_type(v) == J_STR)
        json_str(v, status, sizeof(status));
    if (json_get(resp, "incomplete_details", &det) && json_type(det) == J_OBJ && json_get(det, "reason", &v) &&
        json_type(v) == J_STR)
        json_str(v, reason, sizeof(reason));
    cl_copy(raw, status, sizeof(raw));
    if (reason[0]) {
        cl_cat(raw, ".", sizeof(raw));
        cl_cat(raw, reason, sizeof(raw));
    }
    stop = map_stop(status, reason, msg, sizeof(msg));
    if (d->ntools && stop == LS_STOP)
        stop = LS_TOOLUSE;
    if (stop == LS_TOOLUSE)
        for (i = 0; i < OR_SLOTS; i++)
            if (d->s[i].kind == 1 + LB_TOOLCALL) {
                cl_copy(msg, "OpenAI Responses stream completed with an unfinished tool call: ", sizeof(msg));
                cl_cat(msg, d->s[i].name, sizeof(msg));
                cl_cat(msg, " (", sizeof(msg));
                cl_cat(msg, d->s[i].id, sizeof(msg));
                cl_cat(msg, ")", sizeof(msg));
                fail(d, k, "error", msg);
                return;
            }
    for (i = 0; i < OR_SLOTS; i++)
        end_slot(k, &d->s[i]);
    llm_event_init(&e, LE_STOP);
    e.stop = stop;
    e.stop_raw = raw;
    if (stop == LS_ERROR) {
        if (!msg[0])
            cl_copy(msg, "An unknown error occurred", sizeof(msg));
        e.p = msg;
        e.n = (long)strlen(msg);
    }
    emit(k, &e);
    llm_event_init(&e, LE_DONE);
    emit(k, &e);
    d->done = 1;
}

static int item_done(or_dec *d, const llm_sink *k, long oi, jv item)
{
    jv t, v;
    or_slot *s = open_slot(d, k, oi, item);
    if (!s || !json_get(item, "type", &t))
        return 0;
    if (json_streq(t, "reasoning") && s->kind == 1 + LB_THINKING) {
        jw sum;
        jv a;
        jw_init(&sum);
        if (json_get(item, "summary", &a))
            join_field(a, "text", "\n\n", &sum);
        if (!sum.n && json_get(item, "content", &a))
            join_field(a, "text", "\n\n", &sum);
        if (!s->got && sum.n)
            delta(k, s, sum.p, sum.n);
        jw_free(&sum);
        sig_delta(k, s, item.p, item.n);
        end_slot(k, s);
    } else if (json_streq(t, "message") && s->kind == 1 + LB_TEXT) {
        jw txt, sg;
        jv a;
        char id[160], ph[32];
        jw_init(&txt);
        jw_init(&sg);
        if (json_get(item, "content", &a) && json_type(a) == J_ARR) {
            jit it;
            jv b, bt;
            json_iter(a, &it);
            while (json_next(&it, 0, &b))
                if (json_get(b, "type", &bt)) {
                    const char *f = json_streq(bt, "output_text") ? "text" : "refusal";
                    if (json_get(b, f, &v) && json_type(v) == J_STR) {
                        long l;
                        char *x = json_strdup(v, &l);
                        if (x) {
                            jw_raw(&txt, x, l);
                            free(x);
                        }
                    }
                }
        }
        if (!s->got && txt.n)
            delta(k, s, txt.p, txt.n);
        id[0] = ph[0] = 0;
        if (json_get(item, "id", &v))
            json_str(v, id, sizeof(id));
        if (json_get(item, "phase", &v) && json_type(v) == J_STR)
            json_str(v, ph, sizeof(ph));
        /* encodeTextSignatureV1 */
        jw_rawz(&sg, "{\"v\":1,\"id\":");
        jw_strz(&sg, id);
        if (ph[0]) {
            jw_rawz(&sg, ",\"phase\":");
            jw_strz(&sg, ph);
        }
        jw_raw(&sg, "}", 1);
        sig_delta(k, s, sg.p, sg.n);
        jw_free(&txt);
        jw_free(&sg);
        end_slot(k, s);
    } else if (json_streq(t, "function_call") && s->kind == 1 + LB_TOOLCALL) {
        long l;
        char *a = str_of(item, "arguments", &l);
        if (a && l) {
            if (l >= s->args.n && !memcmp(a, s->args.p ? s->args.p : "", (size_t)s->args.n)) {
                delta(k, s, a + s->args.n, l - s->args.n);
            } else if (l != s->args.n || memcmp(a, s->args.p, (size_t)l)) {
                free(a);
                fail(d, k, "error", "the stream changed the arguments of a tool call it had begun");
                return 0;
            }
        }
        free(a);
        end_slot(k, s);
    }
    return 0;
}

/* the openai SDK (core/streaming.js) turns an `event: error` or a payload with an `error` member into an APIError
 * whose message is APIError.makeMessage(undefined, error): error.message when it is a non-empty string, else the
 * error's JSON text; pi shows that message as it is (no status) */
static void sdk_error(or_dec *d, const llm_sink *k, jv err)
{
    char msg[400];
    jv m;
    msg[0] = 0;
    if (json_type(err) == J_OBJ && json_get(err, "message", &m) && json_type(m) == J_STR && m.n > 2)
        json_str(m, msg, sizeof(msg));
    else if (json_type(err) == J_OBJ && json_get(err, "message", &m) && json_type(m) != J_STR &&
             json_type(m) != J_NULL && json_type(m) != J_FALSE) {
        long l = m.n < (long)sizeof(msg) - 1 ? m.n : (long)sizeof(msg) - 1;
        memcpy(msg, m.p, (size_t)l);
        msg[l] = 0;
    } else {
        long l = err.n < (long)sizeof(msg) - 1 ? err.n : (long)sizeof(msg) - 1;
        memcpy(msg, err.p, (size_t)l);
        msg[l] = 0;
    }
    start(d, k);
    fail(d, k, "error", msg);
}

int llm_responses_feed(void *dv, const char *event, const char *data, long n, const llm_sink *k)
{
    or_dec *d = (or_dec *)dv;
    jv ev, v, item;
    char type[64];
    long oi = 0;
    or_slot *s;
    if (d->bad)
        return -1;
    if (d->done)
        return 0;
    while (n > 0 && is_ws(*data))
        data++, n--;
    if (n == 6 && !memcmp(data, "[DONE]", 6))
        return 0;
    if (json_parse(data, n, &ev) || json_type(ev) != J_OBJ || !json_get(ev, "type", &v) || json_type(v) != J_STR) {
        d->bad = 1;
        return -1;
    }
    json_str(v, type, sizeof(type));
    if (event && !strcmp(event, "error")) {
        if (json_get(ev, "error", &v) && json_type(v) != J_NULL && json_type(v) != J_FALSE)
            sdk_error(d, k, v);
        else
            sdk_error(d, k, ev);
        return 0;
    }
    if (json_get(ev, "error", &v) && json_type(v) != J_NULL && json_type(v) != J_FALSE &&
        !(json_type(v) == J_STR && v.n <= 2)) {
        sdk_error(d, k, v);
        return 0;
    }
    if (json_get(ev, "output_index", &v))
        oi = json_long(v, 0);
    if (!strcmp(type, "response.created")) {
        if (json_get(ev, "response", &v) && json_get(v, "id", &item) && json_type(item) == J_STR)
            json_str(item, d->rid, sizeof(d->rid));
        start(d, k);
    } else if (!strcmp(type, "response.output_item.added")) {
        if (json_get(ev, "item", &item) && json_type(item) == J_OBJ)
            open_slot(d, k, oi, item);
    } else if (!strcmp(type, "response.reasoning_summary_text.delta") || !strcmp(type, "response.reasoning_text.delta")) {
        char *x;
        long l;
        s = slot_of(d, oi, LB_THINKING);
        if (s && (x = str_of(ev, "delta", &l)) != 0) {
            if (s->pend) {
                delta(k, s, "\n\n", 2);
                s->pend = 0;
            }
            delta(k, s, x, l);
            free(x);
        }
    } else if (!strcmp(type, "response.reasoning_summary_part.done")) {
        s = slot_of(d, oi, LB_THINKING);
        if (s && s->got)
            s->pend = 1;
    } else if (!strcmp(type, "response.output_text.delta") || !strcmp(type, "response.refusal.delta")) {
        char *x;
        long l;
        s = slot_of(d, oi, LB_TEXT);
        if (s && (x = str_of(ev, "delta", &l)) != 0) {
            delta(k, s, x, l);
            free(x);
        }
    } else if (!strcmp(type, "response.function_call_arguments.delta")) {
        char *x;
        long l;
        s = slot_of(d, oi, LB_TOOLCALL);
        if (s && (x = str_of(ev, "delta", &l)) != 0) {
            jw_raw(&s->args, x, l);
            delta(k, s, x, l);
            free(x);
        }
    } else if (!strcmp(type, "response.function_call_arguments.done")) {
        char *x;
        long l;
        s = slot_of(d, oi, LB_TOOLCALL);
        if (s && (x = str_of(ev, "arguments", &l)) != 0) {
            if (l >= s->args.n && !memcmp(x, s->args.p ? s->args.p : "", (size_t)s->args.n)) {
                long had = s->args.n;
                jw_raw(&s->args, x + had, l - had);
                delta(k, s, x + had, l - had);
            }
            free(x);
        }
    } else if (!strcmp(type, "response.output_item.done")) {
        if (json_get(ev, "item", &item) && json_type(item) == J_OBJ)
            item_done(d, k, oi, item);
    } else if (!strcmp(type, "response.completed") || !strcmp(type, "response.incomplete")) {
        if (json_get(ev, "response", &v) && json_type(v) == J_OBJ)
            finish(d, k, v);
        else
            finish(d, k, ev);
    } else if (!strcmp(type, "error")) {
        char msg[300], code[48], text[200];
        jv c, m;
        cl_copy(code, "undefined", sizeof(code));
        cl_copy(text, "undefined", sizeof(text));
        if (json_get(ev, "code", &c)) {
            if (json_type(c) == J_NULL)
                cl_copy(code, "null", sizeof(code));
            else
                json_str(c, code, sizeof(code));
        }
        if (json_get(ev, "message", &m)) {
            if (json_type(m) == J_NULL)
                cl_copy(text, "null", sizeof(text));
            else
                json_str(m, text, sizeof(text));
        }
        cl_copy(msg, "Error Code ", sizeof(msg));
        cl_cat(msg, code, sizeof(msg));
        cl_cat(msg, ": ", sizeof(msg));
        cl_cat(msg, text, sizeof(msg));
        start(d, k);
        fail(d, k, "error", msg);
    } else if (!strcmp(type, "response.failed")) {
        char msg[300], a[64], b[200];
        jv r, er, c, m, det, rs;
        msg[0] = 0;
        if (json_get(ev, "response", &r) && json_type(r) == J_OBJ) {
            if (json_get(r, "error", &er) && json_type(er) == J_OBJ) {
                cl_copy(a, "unknown", sizeof(a));
                cl_copy(b, "no message", sizeof(b));
                if (json_get(er, "code", &c) && json_type(c) == J_STR && c.n > 2)
                    json_str(c, a, sizeof(a));
                if (json_get(er, "message", &m) && json_type(m) == J_STR && m.n > 2)
                    json_str(m, b, sizeof(b));
                cl_copy(msg, a, sizeof(msg));
                cl_cat(msg, ": ", sizeof(msg));
                cl_cat(msg, b, sizeof(msg));
            } else if (json_get(r, "incomplete_details", &det) && json_type(det) == J_OBJ &&
                       json_get(det, "reason", &rs) && json_type(rs) == J_STR && rs.n > 2) {
                json_str(rs, b, sizeof(b));
                cl_copy(msg, "incomplete: ", sizeof(msg));
                cl_cat(msg, b, sizeof(msg));
            }
        }
        if (!msg[0])
            cl_copy(msg, "Unknown error (no error details in response)", sizeof(msg));
        if (json_get(ev, "response", &r) && json_get(r, "status", &m) && json_type(m) == J_STR)
            json_str(m, d->raw_fail, sizeof(d->raw_fail));
        start(d, k);
        fail(d, k, "error", msg);
    }
    return 0;
}

/* a stream the API's own words end in an error (openai-codex-responses: "Codex error: ..."): the start, then the error */
void llm_responses_fail(void *dv, const llm_sink *k, const char *msg)
{
    or_dec *d = (or_dec *)dv;
    start(d, k);
    fail(d, k, "error", msg);
}

static int r_feed(void *dv, const char *event, const char *data, long n, const llm_sink *k)
{
    llm_feeds_responses++;
    return llm_responses_feed(dv, event, data, n, k);
}

/* openai-responses.ts:222-226: "<OpenAI|provider> API error" */
static void r_http_err(const llm_http_fail *f, char *out, long cap)
{
    char prefix[100];
    const char *pv = f->model && f->model->provider ? f->model->provider : "";
    if (!strcmp(pv, "openai"))
        cl_copy(prefix, "OpenAI", sizeof(prefix));
    else
        cl_copy(prefix, pv, sizeof(prefix));
    cl_cat(prefix, " API error", sizeof(prefix));
    llm_httperr_openai(f, prefix, 0, out, cap);
}

const llm_backend llm_be_openai_responses = {
    "openai-responses", "Authorization", "/responses", r_head, r_body, r_xhead, llm_responses_new, r_feed,
    llm_responses_free,
    "OpenAI Responses stream ended before a terminal response event", 0, 0, 0, r_http_err, 0, 0
};
