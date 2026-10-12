/* amiga-pi's provider-neutral LLM layer (llm/, plan 2026-10-07-amiga-pi
 * phase B) on the host: pi-ai's message model and its round trip (B1), the
 * neutral events into an assistant message (B2). */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "../llm/llm.h"
#include "../llm/registry.h"
#include "../llm/estimate.h"
#include "../llm/constrain.h"
#include "../llm/event.h"
#include "../llm/accum.h"
#include "../llm/request.h"
#include "../llm/handoff.h"
#include "claude_load.h"
#include "pi_stub.h"

static llm_ts ts_of(long sec, int ms)
{
    llm_ts t;
    t.sec = sec;
    t.ms = ms;
    return t;
}

/* canon(json) == json, and canon(canon(json)) == canon(json) */
static void round_trip(const char *what, const char *json, int role)
{
    jw a, b;
    jw_init(&a);
    jw_init(&b);
    CHECK_INT(llm_msg_canon(json, (long)strlen(json), &a), role);
    CHECK_STR(a.p ? a.p : "", json);
    CHECK_INT(llm_msg_canon(a.p ? a.p : "", a.n, &b), role);
    CHECK_STR(b.p ? b.p : "", a.p ? a.p : "");
    if (strcmp(a.p ? a.p : "", json))
        printf("  (%s)\n", what);
    jw_free(&a);
    jw_free(&b);
}

/* ---- B1: the message model ---- */

static void test_messages(void)
{
    jw c, m, x;
    llm_meta mt;
    llm_info inf;
    static const char sig[] = "EqQBCkgIBhABGAIiQL+sig/part1==+part2";
    llm_ts ts = ts_of(1760100000L, 42);

    /* every block kind in one assistant message */
    jw_init(&c);
    jw_raw(&c, "[", 1);
    llm_b_thinking(&c, "Let me think.", 13, sig, (long)strlen(sig), 0);
    jw_raw(&c, ",", 1);
    llm_b_thinking(&c, "", 0, "opaque-redacted-data", 20, 1);
    jw_raw(&c, ",", 1);
    {
        static const char g[] = "Gr\xc3\xbc\xc3\x9f" "e \"q\"\n\xf0\x9f\x98\x80";
        llm_b_text(&c, g, (long)strlen(g), "txs_1");
    }
    jw_raw(&c, ",", 1);
    llm_b_toolcall(&c, "toolu_01Read", "read", "{\"path\":\"S:Startup-Sequence\",\"limit\":20}", 40, 0);
    jw_raw(&c, ",", 1);
    llm_b_toolcall(&c, "call_2", "bash", "{\"command\":\"ls\"}", 16, "gemini-sig");
    jw_raw(&c, ",", 1);
    llm_b_raw(&c, "anthropic-messages", "{\"type\":\"server_tool_use\",\"id\":\"srvtoolu_1\",\"name\":\"web_search\","
              "\"input\":{\"query\":\"amiga\"}}", 90);
    jw_raw(&c, ",", 1);
    llm_b_image(&c, "iVBORw0KGgo=", "image/png");
    jw_raw(&c, "]", 1);
    memset(&mt, 0, sizeof(mt));
    mt.api = "anthropic-messages";
    mt.provider = "anthropic";
    mt.model = "claude-opus-5-5";
    mt.usage.input = 855;
    mt.usage.output = 150;
    mt.usage.cache_read = 2700;
    mt.usage.cache_write = 1200;
    mt.usage.cost[0] = 3420;
    mt.usage.cost[1] = 3000;
    mt.usage.cost[2] = 540;
    mt.usage.cost[3] = 6000;
    mt.stop = LS_TOOLUSE;
    jw_init(&m);
    llm_m_assistant(&m, c.p, c.n, &mt, ts);
    CHECK(strstr(m.p, "\"usage\":{\"input\":855,\"output\":150,\"cacheRead\":2700,\"cacheWrite\":1200,"
                      "\"totalTokens\":4905,\"cost\":{\"input\":0.00342,\"output\":0.003,\"cacheRead\":0.00054,"
                      "\"cacheWrite\":0.006,\"total\":0.01296}}") != 0);
    CHECK(strstr(m.p, "\"stopReason\":\"toolUse\",\"timestamp\":1760100000042}") != 0);
    CHECK(strstr(m.p, "{\"type\":\"thinking\",\"thinking\":\"\",\"thinkingSignature\":\"opaque-redacted-data\","
                      "\"redacted\":true}") != 0);
    CHECK(strstr(m.p, "\"thoughtSignature\":\"gemini-sig\"") != 0);
    round_trip("assistant", m.p, LR_ASSISTANT);
    CHECK_INT(llm_msg_info(m.p, m.n, &inf), LR_ASSISTANT);
    CHECK_INT(inf.stop, LS_TOOLUSE);
    CHECK_INT(inf.ntools, 2);
    /* the streaming message (JSON and RPC modes' message_start): the serializer writes pi-ai's "pending"; a stored
     * message with it is not one the reader takes */
    {
        jw pm, pc;
        llm_meta pend = mt;
        pend.stop = LS_PENDING;
        jw_init(&pm);
        llm_m_assistant(&pm, "[]", 2, &pend, ts);
        CHECK(strstr(pm.p, "\"stopReason\":\"pending\",\"timestamp\":1760100000042}") != 0);
        jw_init(&pc);
        CHECK_INT(llm_msg_canon(pm.p, pm.n, &pc), LR_BAD);
        jw_free(&pc);
        jw_free(&pm);
    }
    CHECK_INT(inf.ts.sec, 1760100000L);
    CHECK_INT(inf.ts.ms, 42);

    /* an error answer keeps its message */
    jw_reset(&m);
    mt.stop = LS_ERROR;
    mt.error = "overloaded";
    llm_m_assistant(&m, "[]", 2, &mt, ts);
    /* pi-ai's order: errorMessage last, after the timestamp */
    CHECK(strstr(m.p, "\"stopReason\":\"error\",\"timestamp\":") != 0);
    CHECK(strstr(m.p, ",\"errorMessage\":\"overloaded\"}") != 0);
    round_trip("assistant error", m.p, LR_ASSISTANT);

    /* user: a string, and blocks */
    jw_reset(&m);
    llm_m_user(&m, "\"hello\"", 7, ts);
    CHECK_STR(m.p, "{\"role\":\"user\",\"content\":\"hello\",\"timestamp\":1760100000042}");
    round_trip("user string", m.p, LR_USER);
    jw_reset(&c);
    jw_raw(&c, "[", 1);
    llm_b_text(&c, "look", 4, 0);
    jw_raw(&c, ",", 1);
    llm_b_image(&c, "AAAA", "image/jpeg");
    jw_raw(&c, "]", 1);
    jw_reset(&m);
    llm_m_user(&m, c.p, c.n, ts_of(0, 5));
    CHECK_STR(m.p, "{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"look\"},{\"type\":\"image\","
                   "\"data\":\"AAAA\",\"mimeType\":\"image/jpeg\"}],\"timestamp\":5}");
    round_trip("user blocks", m.p, LR_USER);

    /* toolResult, with and without details */
    jw_reset(&c);
    jw_raw(&c, "[", 1);
    llm_b_text(&c, "     1\tSetPatch QUIET\n", 22, 0);
    jw_raw(&c, "]", 1);
    jw_reset(&m);
    llm_m_toolresult(&m, "toolu_01Read", "read", c.p, c.n, "{\"truncated\":false}", 19, 0, ts);
    CHECK_STR(m.p, "{\"role\":\"toolResult\",\"toolCallId\":\"toolu_01Read\",\"toolName\":\"read\",\"content\":"
                   "[{\"type\":\"text\",\"text\":\"     1\\tSetPatch QUIET\\n\"}],\"details\":{\"truncated\":false},"
                   "\"isError\":false,\"timestamp\":1760100000042}");
    round_trip("toolResult", m.p, LR_TOOLRESULT);
    jw_reset(&m);
    llm_m_toolresult(&m, "call_2", "bash", c.p, c.n, 0, 0, 1, ts);
    round_trip("toolResult error", m.p, LR_TOOLRESULT);

    /* system */
    jw_reset(&m);
    llm_m_system(&m, "You are amiga-pi.", 17, ts);
    round_trip("system", m.p, LR_SYSTEM);
    /* pi-ai's whole SystemMessage: sections (a null removes one), tools added and removed, in pi-agent-core's key order */
    jw_reset(&m);
    {
        static const char secs[] = "{\"preamble\":\"You are.\",\"cwd\":null}";
        static const char add[] = "[{\"name\":\"read\",\"description\":\"Read\",\"parameters\":{}}]";
        static const char rem[] = "[{\"name\":\"bash\"}]";
        llm_m_system_ex(&m, "", 0, secs, (long)strlen(secs), add, (long)strlen(add), rem, (long)strlen(rem), ts);
    }
    CHECK_STR(m.p, "{\"role\":\"system\",\"content\":\"\",\"sections\":{\"preamble\":\"You are.\",\"cwd\":null},\"timestamp\":1760100000042,"
                   "\"toolsAdded\":[{\"name\":\"read\",\"description\":\"Read\",\"parameters\":{}}],\"toolsRemoved\":[{\"name\":\"bash\"}]}");
    round_trip("system with sections and tools", m.p, LR_SYSTEM);

    /* pi's own order and spacing come back in the canonical order */
    jw_init(&x);
    CHECK_INT(llm_msg_canon("{ \"timestamp\": 1760100000042, \"content\": [ {\"text\": \"hi\", \"type\": \"text\"} ],"
                            " \"role\": \"user\" }", 93, &x), LR_USER);
    CHECK_STR(x.p ? x.p : "", "{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"hi\"}],"
                              "\"timestamp\":1760100000042}");
    /* not a message: refused, nothing written */
    jw_reset(&x);
    CHECK_INT(llm_msg_canon("{\"role\":\"wizard\",\"content\":[]}", 30, &x), LR_BAD);
    CHECK_INT(llm_msg_canon("{\"role\":\"user\",\"content\":[{\"type\":\"hologram\"}]}", 47, &x), LR_BAD);
    CHECK_INT(llm_msg_canon("{\"role\":\"assistant\",\"content\":[],\"stopReason\":\"maybe\"}", 54, &x), LR_BAD);
    CHECK_INT(llm_msg_canon("{\"role\":\"user\"", 14, &x), LR_BAD);
    CHECK_INT(x.n, 0);

    /* cost: dollars as JS writes them, both ways */
    {
        static const char *const in[] = { "0.01296", "0", "1", "0.000001", "1e-6", "2.5e-7", "1.5E-5", "12.3456789", 0 };
        static const long want[] = { 12960, 0, 1000000, 1, 1, 0, 15, 12345679 };
        int i;
        for (i = 0; in[i]; i++) {
            jv v;
            v.p = in[i];
            v.n = (long)strlen(in[i]);
            CHECK_INT(llm_r_micro(v), want[i]);
        }
        jw_reset(&x);
        llm_w_micro(&x, 12960);
        jw_raw(&x, " ", 1);
        llm_w_micro(&x, 1000000);
        jw_raw(&x, " ", 1);
        llm_w_micro(&x, 1);
        CHECK_STR(x.p, "0.01296 1 0.000001");
    }
    jw_free(&x);
    jw_free(&c);
    jw_free(&m);
}

/* ---- B2: events into the assistant message ---- */

static void ev(llm_acc *a, int type, long index, const char *p)
{
    llm_event e;
    llm_event_init(&e, type);
    e.index = index;
    e.p = p;
    e.n = p ? (long)strlen(p) : 0;
    llm_acc_event(a, &e);
}

static void start(llm_acc *a, long index, int kind, const char *id, const char *name)
{
    llm_event e;
    llm_event_init(&e, LE_BLOCK_START);
    e.index = index;
    e.kind = kind;
    e.id = id;
    e.name = name;
    llm_acc_event(a, &e);
}

static void test_accum(void)
{
    llm_acc a;
    llm_event e;
    jw m;
    llm_info inf;
    const llm_ablock *t;

    /* text, thinking and two tool calls, their events interleaved */
    llm_acc_init(&a, "openai-completions", "ollama");
    llm_event_init(&e, LE_START);
    e.model = "qwen2.5-coder:7b";
    e.p = "chatcmpl-1";
    llm_acc_event(&a, &e);
    start(&a, 1, LB_THINKING, 0, 0);
    start(&a, 0, LB_TEXT, 0, 0);
    ev(&a, LE_DELTA, 1, "Hmm, ");
    ev(&a, LE_DELTA, 0, "I will ");
    ev(&a, LE_SIG_DELTA, 1, "sig-part-one/");
    ev(&a, LE_DELTA, 1, "two files.");
    start(&a, 3, LB_TOOLCALL, "call_b", "ls");
    start(&a, 2, LB_TOOLCALL, "call_a", "read");
    ev(&a, LE_DELTA, 2, "{\"pa");
    ev(&a, LE_DELTA, 3, "{\"path\"");
    ev(&a, LE_SIG_DELTA, 1, "+part-two==");
    ev(&a, LE_DELTA, 0, "look.");
    ev(&a, LE_DELTA, 2, "th\":\"S:\"}");
    ev(&a, LE_DELTA, 3, ":\"C:\"}");
    ev(&a, LE_BLOCK_END, 1, 0);
    ev(&a, LE_BLOCK_END, 0, 0);
    ev(&a, LE_BLOCK_END, 3, 0);
    ev(&a, LE_BLOCK_END, 2, 0);
    llm_event_init(&e, LE_USAGE);
    e.in = 120;
    e.out = 33;
    llm_acc_event(&a, &e);
    llm_event_init(&e, LE_STOP);
    e.stop = LS_TOOLUSE;
    e.stop_raw = "tool_calls";
    llm_acc_event(&a, &e);
    ev(&a, LE_DONE, -1, 0);
    jw_init(&m);
    CHECK_INT(llm_acc_message(&a, ts_of(1760100000L, 0), &m), 0);
    {
        char want[700];
        strcpy(want, "{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"I will look.\"},"
                     "{\"type\":\"thinking\",\"thinking\":\"Hmm, two files.\",\"thinkingSignature\":"
                     "\"sig-part-one/+part-two==\"},{\"type\":\"toolCall\",\"id\":\"call_a\",\"name\":\"read\",");
        strcat(want, "\"arguments\":{\"path\":\"S:\"}},{\"type\":\"toolCall\",\"id\":\"call_b\",\"name\":\"ls\","
                     "\"arguments\":{\"path\":\"C:\"}}],\"api\":\"openai-completions\",\"provider\":\"ollama\",");
        strcat(want, "\"model\":\"qwen2.5-coder:7b\",\"usage\":{\"input\":120,\"output\":33,\"cacheRead\":0,"
                   "\"cacheWrite\":0,\"totalTokens\":153,\"cost\":{\"input\":0,\"output\":0,\"cacheRead\":0,"
                   "\"cacheWrite\":0,\"total\":0}},\"stopReason\":\"toolUse\",\"timestamp\":1760100000000,"
                   "\"responseId\":\"chatcmpl-1\",\"rawStopReason\":\"tool_calls\"}");     /* pi-ai 1.0.4 keeps both */
        CHECK_STR(m.p, want);
    }
    CHECK_INT(llm_acc_tools(&a), 2);
    t = llm_acc_tool(&a, 0);
    CHECK(t && !strcmp(t->id, "call_a"));
    t = llm_acc_tool(&a, 1);
    CHECK(t && !strcmp(t->id, "call_b"));
    CHECK(llm_acc_tool(&a, 2) == 0);
    CHECK_INT(llm_msg_info(m.p, m.n, &inf), LR_ASSISTANT);
    CHECK_INT(inf.ntools, 2);
    round_trip("accumulated", m.p, LR_ASSISTANT);
    llm_acc_free(&a);

    /* a malformed tool call: an error message, not a crash, and no call */
    llm_acc_init(&a, "openai-completions", "ollama");
    ev(&a, LE_START, -1, 0);
    start(&a, 0, LB_TOOLCALL, "call_x", "edit");
    ev(&a, LE_DELTA, 0, "{\"path\":\"S:x\",\"old");
    ev(&a, LE_BLOCK_END, 0, 0);
    ev(&a, LE_DONE, -1, 0);
    jw_reset(&m);
    CHECK_INT(llm_acc_message(&a, ts_of(1, 0), &m), 0);
    CHECK(strstr(m.p, "\"content\":[]") != 0);
    CHECK(strstr(m.p, "\"stopReason\":\"error\",\"timestamp\":") != 0);
    CHECK(strstr(m.p, ",\"errorMessage\":\"the arguments of tool call edit are not valid JSON\"}") != 0);
    CHECK_INT(llm_acc_tools(&a), 0);
    llm_acc_free(&a);

    /* a delta for a block never opened: an error, not a crash */
    llm_acc_init(&a, "openai-completions", "ollama");
    ev(&a, LE_DELTA, 7, "stray");
    jw_reset(&m);
    llm_acc_message(&a, ts_of(1, 0), &m);
    CHECK(strstr(m.p, "\"stopReason\":\"error\"") != 0);
    llm_acc_free(&a);

    /* cut off mid-answer: aborted, the text so far kept */
    llm_acc_init(&a, "anthropic-messages", "anthropic");
    ev(&a, LE_START, -1, 0);
    start(&a, 0, LB_TEXT, 0, 0);
    ev(&a, LE_DELTA, 0, "Half an ans");
    jw_reset(&m);
    llm_acc_message(&a, ts_of(1, 0), &m);
    CHECK(strstr(m.p, "\"text\":\"Half an ans\"") != 0 && strstr(m.p, "\"stopReason\":\"aborted\"") != 0);
    /* an abort event says so, with its words */
    llm_event_init(&e, LE_ERROR);
    e.stop = LS_ABORTED;
    e.p = "Stopped by Ctrl+C";
    e.n = 17;
    llm_acc_event(&a, &e);
    jw_reset(&m);
    llm_acc_message(&a, ts_of(1, 0), &m);
    CHECK(strstr(m.p, "\"stopReason\":\"aborted\",\"timestamp\":") != 0);
    CHECK(strstr(m.p, ",\"errorMessage\":\"Stopped by Ctrl+C\"}") != 0);
    llm_acc_free(&a);

    /* a raw block whose input came in deltas (Anthropic's server tool) */
    llm_acc_init(&a, "anthropic-messages", "anthropic");
    llm_event_init(&e, LE_BLOCK_START);
    e.index = 0;
    e.kind = LB_RAW;
    e.raw_type = "server_tool_use";
    e.raw = "{\"type\":\"server_tool_use\",\"id\":\"srvtoolu_1\",\"name\":\"web_search\",\"input\":{}}";
    e.rn = (long)strlen(e.raw);
    llm_acc_event(&a, &e);
    ev(&a, LE_DELTA, 0, "{\"query\":");
    ev(&a, LE_DELTA, 0, "\"amiga\"}");
    ev(&a, LE_BLOCK_END, 0, 0);
    ev(&a, LE_DONE, -1, 0);
    jw_reset(&m);
    llm_acc_message(&a, ts_of(1, 0), &m);
    CHECK(strstr(m.p, "{\"type\":\"raw\",\"api\":\"anthropic-messages\",\"json\":{\"type\":\"server_tool_use\","
                      "\"id\":\"srvtoolu_1\",\"name\":\"web_search\",\"input\":{\"query\":\"amiga\"}}}") != 0);
    llm_acc_free(&a);
    jw_free(&m);
}

/* one conversation every request test sends: a user turn, an answer with
 * thinking and two calls, their results */
static char conv_asst[900];
static const char *conv_msgs[] = {
    "{\"role\":\"user\",\"content\":\"show me S/Startup-Sequence\",\"timestamp\":1760100000000}",
    conv_asst,
    "{\"role\":\"toolResult\",\"toolCallId\":\"call_1|item_9\",\"toolName\":\"read\",\"content\":[{\"type\":\"text\","
    "\"text\":\"SetPatch QUIET\\n\"}],\"isError\":false,\"timestamp\":1760100000002}",
    "{\"role\":\"toolResult\",\"toolCallId\":\"call_2\",\"toolName\":\"ls\",\"content\":[{\"type\":\"text\","
    "\"text\":\"Startup-Sequence\\n\"}],\"isError\":true,\"timestamp\":1760100000003}"
};
static long conv_n[4];
static llm_view conv_view;
static const char tools_json[] = "[{\"name\":\"read\",\"description\":\"Read a file\",\"parameters\":{\"type\":"
                                 "\"object\",\"properties\":{\"path\":{\"type\":\"string\"}}}}]";

static llm_ctx ctx_of(const llm_model *m, int nmsg)
{
    llm_ctx c;
    int i;
    strcpy(conv_asst, "{\"role\":\"assistant\",\"content\":[{\"type\":\"thinking\",\"thinking\":\"Read it.\","
                      "\"thinkingSignature\":\"sig==\"},{\"type\":\"text\",\"text\":\"Let me look.\"},");
    strcat(conv_asst, "{\"type\":\"toolCall\",\"id\":\"call_1|item_9\",\"name\":\"read\",\"arguments\":"
                      "{\"path\":\"S:Startup-Sequence\"}},{\"type\":\"toolCall\",\"id\":\"call_2\",\"name\":\"ls\","
                      "\"arguments\":{\"path\":\"S:\"}}],\"api\":\"anthropic-messages\",\"provider\":\"anthropic\",");
    strcat(conv_asst, "\"model\":\"claude-opus-5-5\",\"usage\":{\"input\":1,\"output\":1,\"cacheRead\":0,"
                      "\"cacheWrite\":0,\"totalTokens\":2,\"cost\":{\"input\":0,\"output\":0,\"cacheRead\":0,"
                      "\"cacheWrite\":0,\"total\":0}},\"stopReason\":\"toolUse\",\"timestamp\":1760100000001}");
    for (i = 0; i < 4; i++)
        conv_n[i] = (long)strlen(conv_msgs[i]);
    memset(&c, 0, sizeof(c));
    c.model = m;
    c.system = "You are amiga-pi.";
    /* the messages as the agent sends them: through the handoff for this model (ids, foreign thinking) */
    llm_view_free(&conv_view);
    if (llm_prepare(m, (const char *const *)conv_msgs, conv_n, nmsg, ts_of(1760100000L, 0), &conv_view))
        CHECK(0);
    c.msgs = conv_view.m;
    c.msg_n = conv_view.n;
    c.nmsg = conv_view.count;
    c.tools = tools_json;
    c.tools_n = (long)strlen(tools_json);
    return c;
}

static llm_req req_of(const llm_model *m, const llm_ctx *c, const llm_sink *k, const char *key)
{
    llm_req q;
    memset(&q, 0, sizeof(q));
    q.pool = &ps_pool;
    q.model = m;
    q.be = llm_backend_of(m->api);
    q.ctx = c;
    q.key = key;
    q.user_agent = "UP-Term-amiga-pi/0.1 (AmigaOS)";
    q.h.poll = ps_poll;
    q.h.ms = ps_ms;
    q.h.sleep = ps_sleep;
    q.h.notice = ps_notice;
    q.h.resolve = ps_resolve;
    q.h.log = ps_log;
    q.sink = k;
    return q;
}

/* the blocks of a message other than thinking: "text toolCall ..." */
static void kinds(const char *msg, char *out, long cap)
{
    jv m, c, b, t;
    jit it;
    out[0] = 0;
    if (json_parse(msg, (long)strlen(msg), &m) || !json_get(m, "content", &c))
        return;
    json_iter(c, &it);
    while (json_next(&it, 0, &b))
        if (json_get(b, "type", &t) && !json_streq(t, "thinking")) {
            char k[32];
            json_str(t, k, sizeof(k));
            if (out[0])
                strncat(out, " ", (size_t)(cap - (long)strlen(out) - 1));
            strncat(out, k, (size_t)(cap - (long)strlen(out) - 1));
        }
}

/* FA1: an anthropic-messages baseUrl means what pi means: no /v1; the request goes to baseUrl + "/v1/messages" */
static void test_anthropic_baseurl(void)
{
    static const char cfg[] = "{\"providers\":{\"proxy\":{\"baseUrl\":\"https://proxy.example\","
                              "\"api\":\"anthropic-messages\",\"apiKey\":\"k\",\"models\":[{\"id\":\"claude-x\"}]}}}";
    llm_reg r;
    const llm_model *mp;
    llm_ctx c;
    llm_req q;
    llm_acc a;
    llm_sink k;

    llm_reg_init(&r);
    CHECK_INT(llm_reg_builtin(&r), 0);
    CHECK_INT(llm_reg_overlay(&r, cfg, (long)sizeof(cfg) - 1), 0);
    mp = llm_reg_find(&r, "proxy", "claude-x");
    CHECK(mp != 0);
    if (mp) {
        ps_reset();
        ps_pool_up(LLM_POOL);
        ps_sse("tool_use.sse", 0);
        c = ctx_of(mp, 1);
        llm_acc_init(&a, mp->api, mp->provider);
        k = llm_acc_sink(&a);
        q = req_of(mp, &c, &k, "k");
        CHECK_INT(llm_request(&q), 0);
        CHECK(ps.nreq && strstr(ps.head[0], "POST /v1/messages HTTP/1.1\r\nHost: proxy.example\r\n") == ps.head[0]);
        llm_acc_free(&a);
    }
    /* the catalog's own anthropic models carry pi's baseUrl, no /v1 */
    mp = llm_reg_find(&r, "anthropic", "claude-opus-5-5");
    CHECK(mp && !strcmp(mp->base_url, "https://api.anthropic.com"));
    llm_reg_free(&r);
}

/* ---- FA4: openai-responses against pi's own output (tests/pi/responses, made by tools/gen_pi_responses.mjs) ---- */

/* JSON text without the whitespace between tokens (the indented cases.json -> the wire form), malloc'd */
static char *compact_dup(const char *p, long n, long *out_n)
{
    char *o = (char *)malloc((size_t)n + 1);
    long k, m = 0, in_str = 0;
    for (k = 0; k < n; k++) {
        char ch = p[k];
        if (in_str) {
            if (ch == '\\')
                o[m++] = p[k++];
            else if (ch == '"')
                in_str = 0;
        } else if (ch == '"')
            in_str = 1;
        else if (ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r')
            continue;
        o[m++] = p[k];
    }
    o[m] = 0;
    *out_n = m;
    return o;
}


/* what pi's digest line says of a message of ours: stop, error, usage, blocks */
static void resp_digest(const char *msg, jw *o)
{
    jv m, v, c, b, t, x;
    jit it;
    char buf[400];
    if (json_parse(msg, (long)strlen(msg), &m))
        return;
    jw_rawz(o, "stop=");
    if (json_get(m, "stopReason", &v))
        jw_raw(o, v.p + 1, v.n - 2);
    jw_rawz(o, "\nerror=");
    if (json_get(m, "errorMessage", &v)) {
        json_str(v, buf, sizeof(buf));
        jw_rawz(o, buf);
    } else
        jw_rawz(o, "-");
    jw_rawz(o, "\nusage=");
    if (json_get(m, "usage", &v)) {
        static const char *const k[4] = { "input", "output", "cacheRead", "cacheWrite" };
        int i;
        for (i = 0; i < 4; i++) {
            jw_rawz(o, i ? "," : "");
            jw_long(o, json_get(v, k[i], &x) ? json_long(x, -1) : -1);
        }
    }
    jw_rawz(o, "\n");
    if (!json_get(m, "content", &c))
        return;
    json_iter(c, &it);
    while (json_next(&it, 0, &b)) {
        if (!json_get(b, "type", &t))
            continue;
        if (json_streq(t, "thinking") || json_streq(t, "text")) {
            int th = json_streq(t, "thinking");
            jw_rawz(o, th ? "thinking=" : "text=");
            if (json_get(b, th ? "thinking" : "text", &v))
                jw_raw(o, v.p, v.n);
            jw_rawz(o, " sig=");
            if (json_get(b, th ? "thinkingSignature" : "textSignature", &v))
                jw_raw(o, v.p, v.n);
            else
                jw_rawz(o, "-");
            jw_rawz(o, "\n");
        } else if (json_streq(t, "toolCall")) {
            jw_rawz(o, "tool=");
            if (json_get(b, "id", &v)) {
                json_str(v, buf, sizeof(buf));
                jw_rawz(o, buf);
            }
            jw_rawz(o, " ");
            if (json_get(b, "name", &v)) {
                json_str(v, buf, sizeof(buf));
                jw_rawz(o, buf);
            }
            jw_rawz(o, " ");
            if (json_get(b, "arguments", &v))
                jw_raw(o, v.p, v.n);
            jw_rawz(o, "\n");
        }
    }
}

#define RC_MAX 40
/* the transcript pi's buildBaseOptions estimates: normalizeContext's initial system message (the prompt and the
 * tools it declares) followed by the messages as they stand before any provider transform */
static long est_of(const char *sys, const char *tools, long tools_n, char *const *msgs, const long *mn, int nm)
{
    jw sm;
    const char *all[20];
    long an[20];
    int i, k = 0;
    long est;
    jw_init(&sm);
    if ((sys && *sys) || tools_n > 2) {
        jw_rawz(&sm, "{\"role\":\"system\",\"content\":");
        jw_strz(&sm, sys ? sys : "");
        if (tools_n > 2) {
            jw_rawz(&sm, ",\"toolsAdded\":");
            jw_raw(&sm, tools, tools_n);
        }
        jw_rawz(&sm, ",\"timestamp\":0}");
        all[k] = sm.p;
        an[k++] = sm.n;
    }
    for (i = 0; i < nm; i++) {
        all[k] = msgs[i];
        an[k++] = mn[i];
    }
    est = llm_estimate_context(all, an, k);
    jw_free(&sm);
    return est;
}

static char *rc_reply[RC_MAX];
static char rc_name[RC_MAX][32];

static void test_responses(void)
{
    char *cases_txt, name[64];
    long cn, nreq0, feeds0 = llm_feeds_responses;
    jv root, cs, c, v, ms, mv;
    jit it;
    llm_reg r;
    int ncase = 0, i;
    cases_txt = claude_load("../pi/responses/cases.json", &cn);
    CHECK(cases_txt != 0);
    if (!cases_txt)
        return;
    CHECK_INT(json_parse(cases_txt, cn, &root), 0);
    CHECK(json_get(root, "cases", &cs) != 0);
    llm_reg_init(&r);
    CHECK_INT(llm_reg_builtin(&r), 0);
    json_iter(cs, &it);
    while (json_next(&it, 0, &c)) {
        char mname[96], file[96], sse[64], lvl[16], sys[200], sid[80];
        const llm_model *md;
        char *msgs[16], *want_body, *want_dig, *want_hdr;
        long mn[16], bn, dn, hn;
        int nm = 0, lv, own[16];
        jit mit;
        llm_ctx cx;
        llm_req q;
        llm_acc a;
        llm_sink k;
        llm_view vw;
        llm_model wmod;
        jw dig, msg;
        char *tools = 0;
        long tools_n = 0;
        memset(own, 0, sizeof(own));
        json_get(c, "name", &v);
        json_str(v, name, sizeof(name));
        json_get(c, "model", &v);
        json_str(v, mname, sizeof(mname));
        {
            char *sl = strchr(mname, '/');
            *sl = 0;
            md = llm_reg_find(&r, mname, sl + 1);
        }
        CHECK(md != 0);
        if (!md)
            continue;
        json_get(c, "level", &v);
        json_str(v, lvl, sizeof(lvl));
        lv = llm_think_of(lvl);
        json_get(c, "system", &v);
        json_str(v, sys, sizeof(sys));
        json_get(c, "sse", &v);
        json_str(v, sse, sizeof(sse));
        sid[0] = 0;
        if (json_get(c, "sessionId", &v))
            json_str(v, sid, sizeof(sid));
        if (json_get(c, "tools", &v)) {
            tools = compact_dup(v.p, v.n, &tools_n);
        }
        json_get(c, "messages", &ms);
        json_iter(ms, &mit);
        while (json_next(&mit, 0, &mv)) {
            jv rep;
            if (json_type(mv) == J_OBJ && json_get(mv, "$reply", &rep)) {
                char rn[32];
                int j;
                json_str(rep, rn, sizeof(rn));
                msgs[nm] = 0;
                for (j = 0; j < RC_MAX; j++)
                    if (rc_reply[j] && !strcmp(rc_name[j], rn))
                        msgs[nm] = rc_reply[j];
                CHECK(msgs[nm] != 0);
                if (!msgs[nm])
                    msgs[nm] = (char *)"{\"role\":\"user\",\"content\":\"\",\"timestamp\":0}";
                mn[nm] = (long)strlen(msgs[nm]);
            } else {
                /* cases.json is indented; a session's messages are compact */
                msgs[nm] = compact_dup(mv.p, mv.n, &mn[nm]);
                own[nm] = 1;
            }
            nm++;
        }
        if (json_get(c, "window", &v)) {
            /* a smaller context window (the generator's "window"): the output cap clamps on a short input */
            wmod = *md;
            wmod.context_window = json_long(v, 0);
            md = &wmod;
        }
        CHECK_INT(llm_prepare(md, (const char *const *)msgs, mn, nm, ts_of(1760100000L, 0), &vw), 0);
        memset(&cx, 0, sizeof(cx));
        cx.model = md;
        cx.est_tokens = est_of(sys, tools, tools_n, msgs, mn, nm);
        cx.system = sys;
        cx.msgs = vw.m;
        cx.msg_n = vw.n;
        cx.nmsg = vw.count;
        cx.tools = tools;
        cx.tools_n = tools_n;
        cx.thinking = lv;
        cx.session_id = sid[0] ? sid : 0;
        ps_reset();
        ps_pool_up(LLM_POOL);
        strcpy(file, "../pi/responses/");
        strcat(file, sse);
        strcat(file, ".sse");
        ps_sse(file, 0);
        llm_acc_init(&a, md->api, md->provider);
        llm_acc_model(&a, md->id, md->cost);
        k = llm_acc_sink(&a);
        q = req_of(md, &cx, &k, "sk-test-not-real");
        nreq0 = ps.nreq;
        llm_request(&q);
        /* the request on the wire: pi's body, byte for byte */
        strcpy(file, "../pi/responses/");
        strcat(file, name);
        strcat(file, ".body");
        want_body = claude_load(file, &bn);
        CHECK(want_body != 0);
        CHECK(ps.nreq == nreq0 + 1 && ps.body[0] != 0);
        if (want_body && ps.body[0] && strcmp(want_body, ps.body[0])) {
            printf("    case %s body\n      got  %s\n      want %s\n", name, ps.body[0], want_body);
            CHECK(0);
        }
        CHECK(ps.nreq && strstr(ps.head[0], "POST /v1/responses HTTP/1.1\r\nHost: api.openai.com\r\n") == ps.head[0]);
        CHECK(ps.nreq && strstr(ps.head[0], "Authorization: Bearer sk-test-not-real\r\n") != 0);
        file[strlen(file) - 5] = 0;
        strcat(file, ".headers");
        want_hdr = claude_load(file, &hn);
        if (want_hdr) {
            char *line = want_hdr;
            while (*line) {
                char *e = strchr(line, '\n'), save;
                if (!e)
                    break;
                save = *e;
                *e = 0;
                CHECK(strstr(ps.head[0], line) != 0);       /* the golden has \n, the wire \r\n */
                *e = save;
                line = e + 1;
            }
            free(want_hdr);
        } else
            CHECK(strstr(ps.head[0], "x-client-request-id") == 0);
        /* what the stream made: pi's digest */
        jw_init(&msg);
        jw_init(&dig);
        llm_acc_message(&a, ts_of(1760100000L, 0), &msg);
        resp_digest(msg.p, &dig);
        file[strlen(file) - 8] = 0;
        strcat(file, ".digest");
        want_dig = claude_load(file, &dn);
        CHECK(want_dig != 0);
        if (want_dig && strcmp(want_dig, dig.p)) {
            printf("    case %s digest\n      got  %s\n      want %s\n", name, dig.p, want_dig);
            CHECK(0);
        }
        for (i = 0; i < RC_MAX; i++)
            if (!rc_reply[i]) {
                rc_reply[i] = (char *)malloc((size_t)msg.n + 1);
                memcpy(rc_reply[i], msg.p, (size_t)msg.n + 1);
                strcpy(rc_name[i], name);
                break;
            }
        free(want_body);
        free(want_dig);
        jw_free(&dig);
        jw_free(&msg);
        llm_acc_free(&a);
        llm_view_free(&vw);
        free(tools);
        for (i = 0; i < nm; i++)
            if (own[i])
                free(msgs[i]);
        ncase++;
    }
    CHECK(ncase >= 27);
    CHECK(llm_feeds_responses > feeds0);          /* the new backend ran */
    for (i = 0; i < RC_MAX; i++) {
        free(rc_reply[i]);
        rc_reply[i] = 0;
    }
    llm_reg_free(&r);
    free(cases_txt);
}

/* ---- FB1/FB2: anthropic-messages against pi's own output (tests/pi/anthropic, made by tools/gen_pi_anthropic.mjs):
 * the body byte for byte, the anthropic-beta header, and the level each answer records ---- */

static char *ra_reply[RC_MAX];
static char ra_name[RC_MAX][32];

static void test_anthropic_pi(void)
{
    char *cases_txt, name[64];
    long cn, nreq0, feeds0 = llm_feeds_anthropic;
    jv root, cs, c, v, ms, mv;
    jit it;
    llm_reg r;
    int ncase = 0, i, mid_cases = 0;
    cases_txt = claude_load("../pi/anthropic/cases.json", &cn);
    CHECK(cases_txt != 0);
    if (!cases_txt)
        return;
    CHECK_INT(json_parse(cases_txt, cn, &root), 0);
    CHECK(json_get(root, "cases", &cs) != 0);
    llm_reg_init(&r);
    CHECK_INT(llm_reg_builtin(&r), 0);
    json_iter(cs, &it);
    while (json_next(&it, 0, &c)) {
        char mname[96], file[96], sse[64], lvl[16], sys[200];
        const llm_model *md;
        llm_model cmod, wmod;
        char *msgs[16], *want_body, *want_hdr, *want_dig;
        long mn[16], bn, hn, dn;
        int nm = 0, lv, own[16];
        jit mit;
        llm_ctx cx;
        llm_req q;
        llm_acc a;
        llm_sink k;
        llm_view vw;
        jw msg, dig;
        char *tools = 0;
        long tools_n = 0;
        memset(own, 0, sizeof(own));
        json_get(c, "name", &v);
        json_str(v, name, sizeof(name));
        json_get(c, "model", &v);
        json_str(v, mname, sizeof(mname));
        {
            char *sl = strchr(mname, '/');
            *sl = 0;
            md = llm_reg_find(&r, mname, sl + 1);
        }
        CHECK(md != 0);
        if (!md)
            continue;
        if (json_get(c, "compat", &v)) {
            /* a models.json model with only forceAdaptiveThinking: the managed-effort flags are unset */
            cmod = *md;
            cmod.compat.mid_effort = cmod.compat.mid_system = cmod.compat.mid_tools = cmod.compat.strict_tools = LC_UNSET;
            md = &cmod;
        }
        json_get(c, "level", &v);
        json_str(v, lvl, sizeof(lvl));
        lv = llm_think_of(lvl);
        json_get(c, "system", &v);
        json_str(v, sys, sizeof(sys));
        json_get(c, "sse", &v);
        json_str(v, sse, sizeof(sse));
        if (json_get(c, "tools", &v))
            tools = compact_dup(v.p, v.n, &tools_n);
        json_get(c, "messages", &ms);
        json_iter(ms, &mit);
        while (json_next(&mit, 0, &mv)) {
            jv rep;
            if (json_type(mv) == J_OBJ && json_get(mv, "$reply", &rep)) {
                char rn[32];
                int j;
                json_str(rep, rn, sizeof(rn));
                msgs[nm] = 0;
                for (j = 0; j < RC_MAX; j++)
                    if (ra_reply[j] && !strcmp(ra_name[j], rn))
                        msgs[nm] = ra_reply[j];
                CHECK(msgs[nm] != 0);
                if (!msgs[nm])
                    msgs[nm] = (char *)"{\"role\":\"user\",\"content\":\"\",\"timestamp\":0}";
                mn[nm] = (long)strlen(msgs[nm]);
            } else {
                msgs[nm] = compact_dup(mv.p, mv.n, &mn[nm]);
                own[nm] = 1;
            }
            nm++;
        }
        if (json_get(c, "window", &v)) {
            /* a smaller context window (the generator's "window"): the output cap clamps on a short input */
            wmod = *md;
            wmod.context_window = json_long(v, 0);
            md = &wmod;
        }
        CHECK_INT(llm_prepare(md, (const char *const *)msgs, mn, nm, ts_of(1760100000L, 0), &vw), 0);
        memset(&cx, 0, sizeof(cx));
        cx.model = md;
        cx.est_tokens = est_of(sys, tools, tools_n, msgs, mn, nm);
        cx.system = sys;
        cx.msgs = vw.m;
        cx.msg_n = vw.n;
        cx.nmsg = vw.count;
        cx.tools = tools;
        cx.tools_n = tools_n;
        cx.thinking = lv;
        ps_reset();
        ps_pool_up(LLM_POOL);
        strcpy(file, "../pi/anthropic/");
        strcat(file, sse);
        strcat(file, ".sse");
        ps_sse(file, 0);
        llm_acc_init(&a, md->api, md->provider);
        llm_acc_model(&a, md->id, md->cost);
        llm_acc_level(&a, llm_ant_effort(md, lv));
        k = llm_acc_sink(&a);
        q = req_of(md, &cx, &k, "sk-test-not-real");
        nreq0 = ps.nreq;
        llm_request(&q);
        /* the request on the wire: pi's body, byte for byte */
        strcpy(file, "../pi/anthropic/");
        strcat(file, name);
        strcat(file, ".body");
        want_body = claude_load(file, &bn);
        CHECK(want_body != 0);
        CHECK(ps.nreq == nreq0 + 1 && ps.body[0] != 0);
        if (want_body && ps.body[0] && strcmp(want_body, ps.body[0])) {
            printf("    case %s body\n      got  %s\n      want %s\n", name, ps.body[0], want_body);
            CHECK(0);
        }
        CHECK(ps.nreq && strstr(ps.head[0], "POST /v1/messages HTTP/1.1\r\nHost: api.anthropic.com\r\n") == ps.head[0]);
        file[strlen(file) - 5] = 0;
        strcat(file, ".headers");
        want_hdr = claude_load(file, &hn);
        if (want_hdr) {
            char *line = want_hdr, *e = strchr(line, '\n');
            if (e)
                *e = 0;
            CHECK(strstr(ps.head[0], line) != 0);          /* the golden has \n, the wire \r\n */
            if (e)
                *e = '\n';
            if (strstr(want_hdr, "mid-conversation-output-config"))
                mid_cases++;
            free(want_hdr);
        } else
            CHECK(strstr(ps.head[0], "anthropic-beta") == 0);
        /* what the stream made: pi's digest; and the level pi keeps on the answer */
        jw_init(&msg);
        jw_init(&dig);
        llm_acc_message(&a, ts_of(1760100000L, 0), &msg);
        resp_digest(msg.p, &dig);
        file[strlen(file) - 8] = 0;
        strcat(file, ".digest");
        want_dig = claude_load(file, &dn);
        CHECK(want_dig != 0);
        if (want_dig && strcmp(want_dig, dig.p)) {
            printf("    case %s digest\n      got  %s\n      want %s\n", name, dig.p, want_dig);
            CHECK(0);
        }
        free(want_dig);
        jw_free(&dig);
        for (i = 0; i < RC_MAX; i++)
            if (!ra_reply[i]) {
                ra_reply[i] = (char *)malloc((size_t)msg.n + 1);
                memcpy(ra_reply[i], msg.p, (size_t)msg.n + 1);
                strcpy(ra_name[i], name);
                break;
            }
        free(want_body);
        jw_free(&msg);
        llm_acc_free(&a);
        llm_view_free(&vw);
        free(tools);
        for (i = 0; i < nm; i++)
            if (own[i])
                free(msgs[i]);
        ncase++;
    }
    CHECK(ncase >= 24);
    CHECK(mid_cases >= 8);                          /* the managed-effort model's cases ran */
    CHECK(llm_feeds_anthropic > feeds0);            /* the backend ran */
    for (i = 0; i < RC_MAX; i++) {
        free(ra_reply[i]);
        ra_reply[i] = 0;
    }
    llm_reg_free(&r);
    free(cases_txt);
}

/* B11: one top-level request per backend, the neutral message of the same shape */
static void test_reach(void)
{
    llm_model mo = ps_model_openai("http://192.168.1.10:11434/v1"), ma = ps_model_anthropic();
    llm_ctx c;
    llm_req q;
    llm_acc a;
    llm_sink k;
    jw m;
    char ks[200];
    long f0 = llm_feeds_openai, a0 = llm_feeds_anthropic;
    llm_info inf;

    ps_reset();
    ps_pool_up(LLM_POOL);
    ps_sse("../pi/openai_tools.sse", 0);
    c = ctx_of(&mo, 1);
    llm_acc_init(&a, mo.api, mo.provider);
    k = llm_acc_sink(&a);
    q = req_of(&mo, &c, &k, 0);
    CHECK_INT(llm_request(&q), 0);
    CHECK(llm_feeds_openai > f0);
    CHECK_INT(ps.nreq, 1);
    CHECK(ps.nreq && strstr(ps.head[0], "POST /v1/chat/completions HTTP/1.1\r\nHost: 192.168.1.10:11434\r\n") == ps.head[0]);
    jw_init(&m);
    llm_acc_message(&a, ts_of(1760100000L, 0), &m);
    kinds(m.p, ks, sizeof(ks));
    CHECK_STR(ks, "text toolCall toolCall");
    CHECK(strstr(m.p, "{\"type\":\"toolCall\",\"id\":\"call_ReadStartup\",\"name\":\"read\",\"arguments\":"
                      "{\"path\":\"S:Startup-Sequence\",\"limit\":20}}") != 0);
    CHECK(strstr(m.p, "\"arguments\":{\"path\":\"S:\",\"pattern\":\"*\"}") != 0);
    CHECK_INT(llm_msg_info(m.p, m.n, &inf), LR_ASSISTANT);
    CHECK_INT(inf.stop, LS_TOOLUSE);
    CHECK_INT(inf.ntools, 2);
    llm_acc_free(&a);

    ps_reset();
    ps_pool_up(LLM_POOL);
    ps_sse("tool_use.sse", 0);
    c = ctx_of(&ma, 1);
    llm_acc_init(&a, ma.api, ma.provider);
    k = llm_acc_sink(&a);
    q = req_of(&ma, &c, &k, "sk-ant-test-not-real");
    CHECK_INT(llm_request(&q), 0);
    CHECK(llm_feeds_anthropic > a0);
    CHECK(ps.nreq && strstr(ps.head[0], "POST /v1/messages HTTP/1.1\r\nHost: api.anthropic.com\r\n") == ps.head[0]);
    CHECK(ps.nreq && strstr(ps.head[0], "x-api-key: sk-ant-test-not-real\r\n") != 0);
    jw_reset(&m);
    llm_acc_message(&a, ts_of(1760100000L, 0), &m);
    kinds(m.p, ks, sizeof(ks));
    CHECK_STR(ks, "text toolCall toolCall");
    CHECK_INT(llm_msg_info(m.p, m.n, &inf), LR_ASSISTANT);
    CHECK_INT(inf.stop, LS_TOOLUSE);
    CHECK_INT(inf.ntools, 2);
    llm_acc_free(&a);
    jw_free(&m);
}

/* B6: the openai-completions body, one golden per combination of the four compat flags */
static void test_openai_bodies(void)
{
    int bits;
    for (bits = 0; bits < 16; bits++) {
        llm_model mo = ps_model_openai("https://api.openai.com/v1");
        llm_ctx c;
        jw w;
        jv v;
        char name[64];
        mo.id = "gpt-5";
        mo.provider = "openai";
        mo.reasoning = 1;
        mo.compat.max_tokens_field = bits & 1;
        mo.compat.usage_in_streaming = (bits >> 1) & 1;
        mo.compat.developer_role = (bits >> 2) & 1;
        mo.compat.tool_result_name = (bits >> 3) & 1;
        c = ctx_of(&mo, 4);
        jw_init(&w);
        CHECK_INT(llm_be_openai_chat.body(&c, &w), 0);
        CHECK(json_parse(w.p, w.n, &v) == 0);
        strcpy(name, "../pi/body_openai_0000.json");
        name[18] = (char)('0' + (bits & 1));
        name[19] = (char)('0' + ((bits >> 1) & 1));
        name[20] = (char)('0' + ((bits >> 2) & 1));
        name[21] = (char)('0' + ((bits >> 3) & 1));
        jw_raw(&w, "\n", 1);
        claude_golden(name, w.p);
        jw_free(&w);
    }
    /* the defaults of a LAN endpoint: system role (not a reasoning model),
     * include_usage, max_completion_tokens, no tool name; ids made safe */
    {
        llm_model mo = ps_model_openai("http://192.168.1.10:11434/v1");
        llm_ctx c = ctx_of(&mo, 4);
        jw w;
        jw_init(&w);
        CHECK_INT(llm_be_openai_chat.body(&c, &w), 0);
        CHECK(strstr(w.p, "{\"role\":\"system\",\"content\":\"You are amiga-pi.\"}") != 0);
        CHECK(strstr(w.p, "\"stream_options\":{\"include_usage\":true}") != 0);
        CHECK(strstr(w.p, "\"max_completion_tokens\":4096") != 0);
        CHECK(strstr(w.p, "\"tool_call_id\":\"call_1_item_9\"") != 0 && strstr(w.p, "call_1|item_9") == 0);
        CHECK(strstr(w.p, "\"name\":\"read\"}") == 0);
        CHECK(strstr(w.p, "\"content\":\"Read it.Let me look.\"") != 0);   /* pi: another model's thinking is plain text */
        jw_free(&w);
    }
    /* the Anthropic body from the same messages: thinking with its
     * signature (the same model: its ids stay as written), the calls, both results in one user turn */
    {
        llm_model ma = ps_model_anthropic();
        llm_ctx c = ctx_of(&ma, 4);
        jw w;
        jw_init(&w);
        CHECK_INT(llm_be_anthropic.body(&c, &w), 0);
        CHECK(strstr(w.p, "{\"type\":\"thinking\",\"thinking\":\"Read it.\",\"signature\":\"sig==\"}") != 0);
        CHECK(strstr(w.p, "{\"type\":\"tool_use\",\"id\":\"call_1|item_9\",\"name\":\"read\",\"input\":"
                          "{\"path\":\"S:Startup-Sequence\"}}") != 0);
        /* pi: a result's texts joined into one string, is_error always present */
        CHECK(strstr(w.p, "{\"role\":\"user\",\"content\":[{\"type\":\"tool_result\",\"tool_use_id\":"
                          "\"call_1|item_9\",\"content\":\"SetPatch QUIET\\n\",\"is_error\":false},"
                          "{\"type\":\"tool_result\",\"tool_use_id\":\"call_2\",\"content\":"
                          "\"Startup-Sequence\\n\",\"is_error\":true,\"cache_control\":{\"type\":\"ephemeral\"}}]}") != 0);
        CHECK(strstr(w.p, "\"input_schema\":{\"type\":\"object\"") != 0);
        jw_free(&w);
    }
}

/* PX1: the strict schema pi-ai's makeStrictJsonSchema makes (tests/pi/strict/cases.json, made by tools/gen_pi_strict.mjs):
 * a tool schema outside the strict subset is refused (the tool then goes out plain), the rest comes out as pi's */
static void test_strict_schema(void)
{
    char *txt;
    long cn;
    jv root, cs, c, v;
    jit it;
    int n = 0, refused = 0;
    txt = claude_load("../pi/strict/cases.json", &cn);
    CHECK(txt != 0);
    if (!txt)
        return;
    CHECK_INT(json_parse(txt, cn, &root), 0);
    CHECK(json_get(root, "cases", &cs) != 0);
    json_iter(cs, &it);
    while (json_next(&it, 0, &c)) {
        char name[64], kw[16];
        char *schema, *want = 0;
        long sn, wn = 0;
        jv sv, st;
        jw out;
        int rc, ant;
        json_get(c, "name", &v);
        json_str(v, name, sizeof(name));
        json_get(c, "kw", &v);
        json_str(v, kw, sizeof(kw));
        ant = !strcmp(kw, "anthropic");
        json_get(c, "schema", &v);
        schema = compact_dup(v.p, v.n, &sn);
        json_get(c, "strict", &st);
        if (json_type(st) != J_NULL)
            want = compact_dup(st.p, st.n, &wn);
        CHECK_INT(json_parse(schema, sn, &sv), 0);
        jw_init(&out);
        rc = llm_strict_schema(sv, &out, ant ? LCS_ANTHROPIC : LCS_NONE);
        if (!want) {
            if (rc != -1)
                printf("    case %s: pi refuses it, we made %s\n", name, out.p ? out.p : "");
            CHECK_INT(rc, -1);
            refused++;
        } else {
            CHECK_INT(rc, 0);
            if (rc == 0 && strcmp(out.p, want)) {
                printf("    case %s\n      got  %s\n      want %s\n", name, out.p, want);
                CHECK(0);
            }
        }
        jw_free(&out);
        free(schema);
        free(want);
        n++;
    }
    CHECK(n >= 30);
    CHECK(refused >= 10);
    free(txt);
}

/* PX2: llm_estimate_context is pi-ai's estimateContextTokens (tests/pi/estimate/cases.json, tools/gen_pi_estimate.mjs):
 * 3.5 characters per token in UTF-16 units, images 4800 characters, the last good answer's usage plus what follows it */
static void test_estimate(void)
{
    char *txt;
    long cn;
    jv root, cs, c, v, ms, mv;
    jit it, mit;
    int n = 0;
    txt = claude_load("../pi/estimate/cases.json", &cn);
    CHECK(txt != 0);
    if (!txt)
        return;
    CHECK_INT(json_parse(txt, cn, &root), 0);
    CHECK(json_get(root, "cases", &cs) != 0);
    json_iter(cs, &it);
    while (json_next(&it, 0, &c)) {
        char name[64];
        char *msgs[16];
        long mn[16], got, want;
        int nm = 0, i;
        json_get(c, "name", &v);
        json_str(v, name, sizeof(name));
        json_get(c, "tokens", &v);
        want = json_long(v, -1);
        json_get(c, "messages", &ms);
        json_iter(ms, &mit);
        while (json_next(&mit, 0, &mv)) {
            msgs[nm] = compact_dup(mv.p, mv.n, &mn[nm]);
            nm++;
        }
        got = llm_estimate_context((const char *const *)msgs, mn, nm);
        if (got != want)
            printf("    case %s: got %ld, pi says %ld\n", name, got, want);
        CHECK_INT(got, want);
        for (i = 0; i < nm; i++)
            free(msgs[i]);
        n++;
    }
    CHECK(n >= 12);
    /* clampMaxTokensToContext: the window less the estimate less 4096, at least 1, never above the cap */
    {
        llm_model m;
        llm_model_init(&m);
        m.context_window = 8192;
        CHECK_INT(llm_clamp_max_tokens(&m, 1420, 8192), 2676);
        CHECK_INT(llm_clamp_max_tokens(&m, 1420, 2000), 2000);
        CHECK_INT(llm_clamp_max_tokens(&m, 9000, 8192), 1);
        m.context_window = 0;
        CHECK_INT(llm_clamp_max_tokens(&m, 9000, 8192), 8192);
        CHECK_INT(llm_clamp_max_tokens(&m, 9000, 0), 1);
    }
    free(txt);
}

/* PX1, PX2 for openai-completions: the body pi-ai's openai-completions makes for each case of
 * tests/pi/completions/cases.json (strict tools of a model that supports them, the fallback of a schema outside the
 * strict subset, a model without strict mode, the output cap clamped to the context window) */
static void test_completions(void)
{
    char *cases_txt;
    long cn;
    jv root, cs, c, v, mv, ms, mc;
    jit it;
    int ncase = 0, strict_cases = 0;
    cases_txt = claude_load("../pi/completions/cases.json", &cn);
    CHECK(cases_txt != 0);
    if (!cases_txt)
        return;
    CHECK_INT(json_parse(cases_txt, cn, &root), 0);
    CHECK(json_get(root, "cases", &cs) != 0);
    json_iter(cs, &it);
    while (json_next(&it, 0, &c)) {
        char name[64], idb[64], sys[200], file[96], pvb[64], ubb[128];
        llm_model md = ps_model_openai("http://192.168.1.10:11434/v1");
        char *msgs[4], *tools = 0, *want;
        long mn[4], tools_n = 0, wn;
        int nm = 0;
        jit mit;
        llm_view vw;
        llm_ctx cx;
        jw w;
        json_get(c, "name", &v);
        json_str(v, name, sizeof(name));
        json_get(c, "model", &mv);
        json_get(mv, "id", &v);
        json_str(v, idb, sizeof(idb));
        md.id = idb;
        md.provider = "lan";
        if (json_get(mv, "provider", &v)) {
            json_str(v, pvb, sizeof(pvb));
            md.provider = pvb;
        }
        if (json_get(mv, "baseUrl", &v)) {
            json_str(v, ubb, sizeof(ubb));
            md.base_url = ubb;
        }
        md.reasoning = 0;
        json_get(mv, "window", &v);
        md.context_window = json_long(v, 0);
        json_get(mv, "maxTokens", &v);
        md.max_tokens = json_long(v, 0);
        if (json_get(mv, "compat", &mc) && json_get(mc, "supportsStrictMode", &v))
            md.compat.strict_tools = json_type(v) == J_TRUE;
        if (json_get(mv, "compat", &mc) && json_get(mc, "supportsStore", &v))
            md.compat.store = json_type(v) == J_TRUE;
        json_get(c, "system", &v);
        json_str(v, sys, sizeof(sys));
        if (json_get(c, "tools", &v))
            tools = compact_dup(v.p, v.n, &tools_n);
        json_get(c, "messages", &ms);
        json_iter(ms, &mit);
        while (json_next(&mit, 0, &v)) {
            msgs[nm] = compact_dup(v.p, v.n, &mn[nm]);
            nm++;
        }
        CHECK_INT(llm_prepare(&md, (const char *const *)msgs, mn, nm, ts_of(1760100000L, 0), &vw), 0);
        memset(&cx, 0, sizeof(cx));
        cx.model = &md;
        cx.system = sys;
        cx.msgs = vw.m;
        cx.msg_n = vw.n;
        cx.nmsg = vw.count;
        cx.tools = tools;
        cx.tools_n = tools_n;
        cx.est_tokens = est_of(sys, tools, tools_n, msgs, mn, nm);
        jw_init(&w);
        CHECK_INT(llm_be_openai_chat.body(&cx, &w), 0);
        strcpy(file, "../pi/completions/");
        strcat(file, name);
        strcat(file, ".body");
        want = claude_load(file, &wn);
        CHECK(want != 0);
        if (want && strcmp(want, w.p)) {
            printf("    case %s body\n      got  %s\n      want %s\n", name, w.p, want);
            CHECK(0);
        }
        if (strstr(w.p, "\"strict\":true"))
            strict_cases++;
        free(want);
        jw_free(&w);
        llm_view_free(&vw);
        free(tools);
        while (nm > 0)
            free(msgs[--nm]);
        ncase++;
    }
    CHECK(ncase >= 8);
    CHECK(strict_cases >= 2);
    free(cases_txt);
}

/* B3: done, a cut stream, a 429 with Retry-After 2, Ctrl+C mid-stream, the idle limit; usage */
static void test_request(void)
{
    llm_model mo = ps_model_openai("http://192.168.1.10:11434/v1");
    llm_ctx c = ctx_of(&mo, 1);
    llm_req q;
    llm_acc a;
    llm_sink k;
    jw m;
    jw_init(&m);

    /* [DONE]: complete; the include_usage chunk's counts (cached tokens apart) */
    ps_reset();
    ps_pool_up(LLM_POOL);
    ps_sse("../pi/openai_text.sse", 0);
    llm_acc_init(&a, mo.api, mo.provider);
    k = llm_acc_sink(&a);
    q = req_of(&mo, &c, &k, 0);
    CHECK_INT(llm_request(&q), 0);
    llm_acc_message(&a, ts_of(1, 0), &m);
    CHECK(strstr(m.p, "\"text\":\"Hello from the Amiga! Gr\xc3\xbc\xc3\x9f" "e \xe2\x80\x94 ok\"") != 0);
    CHECK(strstr(m.p, "\"usage\":{\"input\":100,\"output\":9,\"cacheRead\":20,\"cacheWrite\":0,\"totalTokens\":129") != 0);
    CHECK(strstr(m.p, "\"stopReason\":\"stop\"") != 0);
    CHECK_INT(q.attempts, 1);
    llm_acc_free(&a);

    /* a 429 with Retry-After 2: one retry after 2 s, then the answer */
    ps_reset();
    ps_pool_up(LLM_POOL);
    ps_raw("../pi/openai_429.http");
    ps_sse("../pi/openai_text.sse", 0);
    llm_acc_init(&a, mo.api, mo.provider);
    k = llm_acc_sink(&a);
    q = req_of(&mo, &c, &k, 0);
    CHECK_INT(llm_request(&q), 0);
    CHECK_INT(q.attempts, 2);
    CHECK_INT(ps.sleeps, 1);
    CHECK_INT(ps.slept, 2000);
    CHECK_INT(ps.nreq, 2);
    llm_acc_free(&a);

    /* retries -1: the caller retries (amiga-pi's agent loop, as pi's agent session): the 429 is the answer, no wait */
    ps_reset();
    ps_pool_up(LLM_POOL);
    ps_raw("../pi/openai_429.http");
    ps_sse("../pi/openai_text.sse", 0);
    llm_acc_init(&a, mo.api, mo.provider);
    k = llm_acc_sink(&a);
    q = req_of(&mo, &c, &k, 0);
    q.retries = -1;
    CHECK_INT(llm_request(&q), -1);
    CHECK_INT(q.attempts, 1);
    CHECK_INT(ps.sleeps, 0);
    CHECK_INT(ps.nreq, 1);
    jw_reset(&m);
    llm_acc_message(&a, ts_of(1, 0), &m);
    CHECK(strstr(m.p, "\"stopReason\":\"error\"") != 0);
    llm_acc_free(&a);

    /* cut off after part of the answer: an error, not tried again (the text kept) */
    ps_reset();
    ps_pool_up(LLM_POOL);
    ps_sse("../pi/openai_cut.sse", 1);
    llm_acc_init(&a, mo.api, mo.provider);
    k = llm_acc_sink(&a);
    q = req_of(&mo, &c, &k, 0);
    CHECK_INT(llm_request(&q), -1);
    CHECK_INT(q.attempts, 1);
    jw_reset(&m);
    llm_acc_message(&a, ts_of(1, 0), &m);
    CHECK(strstr(m.p, "\"text\":\"This answer is cut off in the mid\"") != 0);
    CHECK(strstr(m.p, "\"stopReason\":\"error\",\"timestamp\":") != 0);
    CHECK(strstr(m.p, ",\"errorMessage\":\"the connection closed early\"}") != 0);
    llm_acc_free(&a);

    /* Ctrl+C mid-stream: aborted, the text so far kept */
    ps_reset();
    ps_pool_up(LLM_POOL);
    ps_sse("../pi/openai_text.sse", 0);
    ps.brk_at = 600;
    llm_acc_init(&a, mo.api, mo.provider);
    k = llm_acc_sink(&a);
    q = req_of(&mo, &c, &k, 0);
    CHECK_INT(llm_request(&q), -1);
    jw_reset(&m);
    llm_acc_message(&a, ts_of(1, 0), &m);
    CHECK(strstr(m.p, "\"text\":\"Hello from \"") != 0);
    /* pi-ai's text for a request stopped by the user (provider-retry.js createAbortError) */
    CHECK(strstr(m.p, "\"stopReason\":\"aborted\",\"timestamp\":") != 0);
    CHECK(strstr(m.p, ",\"errorMessage\":\"Request aborted\"}") != 0);
    CHECK_INT(ps.nreq, 1);
    llm_acc_free(&a);

    /* the server says nothing: the idle limit, three retries (2, 4, 8 s), then an error */
    ps_reset();
    ps_pool_up(LLM_POOL);
    ps.silent = 1;
    llm_acc_init(&a, mo.api, mo.provider);
    k = llm_acc_sink(&a);
    q = req_of(&mo, &c, &k, 0);
    q.idle_ms = 1000;
    CHECK_INT(llm_request(&q), -1);
    CHECK_INT(q.attempts, 4);
    CHECK_INT(ps.slept, 14000);
    jw_reset(&m);
    llm_acc_message(&a, ts_of(1, 0), &m);
    CHECK(strstr(m.p, "\"errorMessage\":\"no answer within the idle limit\"") != 0);
    llm_acc_free(&a);
    jw_free(&m);
}

/* B7: one connection per host, the least recently used closed first */
static void test_pool(void)
{
    int i, reused, rc;
    llm_conn *c;
    ps_reset();
    ps_pool_up(LLM_POOL);
    for (i = 0; i < 10; i++) {
        c = llm_pool_get(&ps_pool, i & 1 ? "b.lan" : "a.lan", 80, 0, &reused, &rc);
        CHECK(c != 0);
        CHECK_INT(reused, i >= 2);
    }
    CHECK_INT(ps_pool.opens, 2);
    CHECK_INT(llm_pool_open(&ps_pool), 2);
    llm_pool_get(&ps_pool, "c.lan", 80, 0, &reused, &rc);
    llm_pool_get(&ps_pool, "d.lan", 80, 0, &reused, &rc);
    llm_pool_get(&ps_pool, "a.lan", 80, 0, &reused, &rc);     /* a used last: b is the oldest */
    CHECK_INT(llm_pool_open(&ps_pool), 4);
    CHECK_INT(ps.closes, 0);
    llm_pool_get(&ps_pool, "e.lan", 80, 0, &reused, &rc);     /* a fifth: b closed */
    CHECK_INT(ps.closes, 1);
    CHECK_INT(llm_pool_open(&ps_pool), 4);
    llm_pool_get(&ps_pool, "a.lan", 80, 0, &reused, &rc);
    CHECK_INT(reused, 1);
    llm_pool_get(&ps_pool, "b.lan", 80, 0, &reused, &rc);
    CHECK_INT(reused, 0);
    CHECK_INT(ps_pool.opens, 6);
    /* the same host on another port or with TLS is another connection */
    llm_pool_get(&ps_pool, "a.lan", 443, 1, &reused, &rc);
    CHECK_INT(reused, 0);
    llm_pool_close(&ps_pool);
    CHECK_INT(llm_pool_open(&ps_pool), 0);
}

/* MCP over HTTP (amiga-pi K3) on llm_post: every response header reaches the header hook (the session id),
 * a 2xx application/json body reaches the body hook whole (not cut at errbody's 16 KB), an event stream
 * still goes to the SSE events, and without the hooks a JSON answer stays in errbody as before */
static jw hk_hdr, hk_body, hk_ev;
static void hk_header(void *u, const char *name, const char *value)
{
    (void)u;
    jw_rawz(&hk_hdr, name);
    jw_rawz(&hk_hdr, "=");
    jw_rawz(&hk_hdr, value);
    jw_rawz(&hk_hdr, ";");
}
static void hk_bodyfn(void *u, const char *s, long n)
{
    (void)u;
    jw_raw(&hk_body, s, n);
}
static void hk_event(void *u, const char *event, const char *data, long n)
{
    (void)u;
    (void)event;
    jw_raw(&hk_ev, data, n);
    jw_rawz(&hk_ev, "|");
}
static int hk_head(void *u, const char **head, long *hn)
{
    static const char h[] = "POST /mcp HTTP/1.1\r\nHost: nas.lan\r\nContent-Length: 2\r\n\r\n";
    (void)u;
    *head = h;
    *hn = (long)sizeof(h) - 1;
    return 0;
}
static void test_post_hooks(void)
{
    static char buf[4096];
    http_resp resp;
    sse ss;
    jw eb;
    llm_call c;
    jw big;
    long i;
    jw_init(&hk_hdr);
    jw_init(&hk_body);
    jw_init(&hk_ev);
    jw_init(&eb);
    jw_init(&big);
    jw_rawz(&big, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"t\":\"");
    for (i = 0; i < 30000; i++)
        jw_raw(&big, "x", 1);
    jw_rawz(&big, "\"}}");
    ps_reset();
    ps_pool_up(LLM_POOL);
    {
        jw a;
        jw_init(&a);
        jw_rawz(&a, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nMcp-Session-Id: abc-123\r\n");
        jw_rawz(&a, "Content-Length: ");
        jw_long(&a, big.n);
        jw_rawz(&a, "\r\n\r\n");
        jw_raw(&a, big.p, big.n);
        ps_http_text(a.p, a.n);
        jw_free(&a);
    }
    memset(&c, 0, sizeof(c));
    c.pool = &ps_pool;
    c.host = "nas.lan";
    c.port = 80;
    c.body = "{}";
    c.bn = 2;
    c.h.head = hk_head;
    c.h.header = hk_header;
    c.h.body = hk_bodyfn;
    c.h.event = hk_event;
    c.idle_ms = 5000;
    c.buf = buf;
    c.bufcap = sizeof(buf);
    c.resp = &resp;
    c.sse = &ss;
    c.errbody = &eb;
    CHECK_INT(llm_post(&c), LP_OK);
    CHECK_INT(resp.status, 200);
    CHECK(hk_hdr.p && strstr(hk_hdr.p, "mcp-session-id=abc-123;") != 0);
    CHECK(hk_hdr.p && strstr(hk_hdr.p, "content-type=application/json;") != 0);
    CHECK_INT(hk_body.n, big.n);
    CHECK(hk_body.p && !memcmp(hk_body.p, big.p, (size_t)big.n));
    CHECK_INT(eb.n, 0);
    /* an event stream on the same call: its events, nothing in the body hook */
    jw_free(&hk_body);
    jw_init(&hk_body);
    {
        static const char ev[] = "event: message\ndata: {\"id\":2}\n\n";
        ps_sse_text(ev, (long)sizeof(ev) - 1, 0);
    }
    CHECK_INT(llm_post(&c), LP_OK);
    CHECK(hk_ev.p && strstr(hk_ev.p, "{\"id\":2}|") != 0);
    CHECK_INT(hk_body.n, 0);
    /* without the hooks: a JSON answer kept in errbody, cut at 16 KB, as before */
    c.h.header = 0;
    c.h.body = 0;
    {
        static const char an[] = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 7\r\n\r\n{\"a\":1}";
        ps_http_text(an, (long)sizeof(an) - 1);
    }
    CHECK_INT(llm_post(&c), LP_OK);
    CHECK_INT(eb.n, 7);
    /* repeated refusals on one errbody (mcp_http keeps it across requests): the buffer is reused, not leaked
     * (init per call dropped the pointer: a new allocation each answer) */
    {
        static const char r503[] = "HTTP/1.1 503 Unavailable\r\nContent-Length: 5\r\n\r\nbusy!";
        char *kept = eb.p;
        for (i = 0; i < 3; i++) {
            ps_http_text(r503, (long)sizeof(r503) - 1);
            CHECK_INT(llm_post(&c), LP_OK);
            CHECK_INT(resp.status, 503);
            CHECK_INT(eb.n, 5);
            CHECK(eb.p == kept);
        }
    }
    llm_pool_close(&ps_pool);
    jw_free(&hk_hdr);
    jw_free(&hk_body);
    jw_free(&hk_ev);
    jw_free(&eb);
    jw_free(&big);
}

/* B8, B9: plain http only on the LAN, no key over it, a notice once; the log never holds the key */
static void test_lan(void)
{
    unsigned char ip[4];
    llm_model mo;
    llm_ctx c;
    llm_req q;
    llm_acc a;
    llm_sink k;
    jw m;
    static const char key[] = "sk-test-key-not-real";
    jw_init(&m);

    CHECK(llm_ipv4("192.168.1.10", ip) && ip[0] == 192 && ip[3] == 10);
    CHECK(!llm_ipv4("192.168.1", ip) && !llm_ipv4("1.2.3.256", ip) && !llm_ipv4("nas.lan", ip));
    CHECK(llm_lan_ok("x", (const unsigned char *)"\x0a\x00\x00\x01"));
    CHECK(llm_lan_ok("x", (const unsigned char *)"\xac\x10\x00\x01"));
    CHECK(llm_lan_ok("x", (const unsigned char *)"\xac\x1f\xff\x01"));
    CHECK(!llm_lan_ok("x", (const unsigned char *)"\xac\x20\x00\x01"));
    CHECK(llm_lan_ok("x", (const unsigned char *)"\x7f\x00\x00\x01"));
    CHECK(!llm_lan_ok("x", (const unsigned char *)"\xa9\xfe\x00\x01"));   /* link-local: not in the list */
    CHECK(llm_lan_ok("Ollama.LOCAL", 0));
    CHECK(!llm_lan_ok("nas.lan", 0));

    /* http to the LAN with a key configured: no credential header, one notice */
    ps_reset();
    ps_pool_up(LLM_POOL);
    ps_sse("../pi/openai_text.sse", 0);
    ps_sse("../pi/openai_text.sse", 0);
    mo = ps_model_openai("http://192.168.1.10:11434/v1");
    mo.provider = "lan-ollama-b8";
    c = ctx_of(&mo, 1);
    llm_acc_init(&a, mo.api, mo.provider);
    k = llm_acc_sink(&a);
    q = req_of(&mo, &c, &k, key);
    CHECK_INT(llm_request(&q), 0);
    CHECK_INT(q.key_sent, 0);
    CHECK(ps.nreq == 1 && !strstr(ps.head[0], "Authorization") && !strstr(ps.head[0], "x-api-key") &&
          !strstr(ps.head[0], "api-key") && !strstr(ps.head[0], key));
    CHECK_INT(ps.notices, 1);
    CHECK_STR(ps.notice, "key for lan-ollama-b8 not sent: plain http");
    llm_request(&q);
    CHECK_INT(ps.notices, 1);                               /* once per provider per run */
    llm_acc_free(&a);

    /* a name resolving to the LAN, and a .local name: allowed */
    ps_reset();
    ps_pool_up(LLM_POOL);
    ps_sse("../pi/openai_text.sse", 0);
    ps_sse("../pi/openai_text.sse", 0);
    mo = ps_model_openai("http://nas.lan:8080/v1");
    q = req_of(&mo, &c, &k, 0);
    llm_acc_init(&a, mo.api, mo.provider);
    CHECK_INT(llm_request(&q), 0);
    mo = ps_model_openai("http://studio.local:1234/v1");
    q = req_of(&mo, &c, &k, 0);
    CHECK_INT(llm_request(&q), 0);
    CHECK_INT(ps.nreq, 2);
    llm_acc_free(&a);

    /* http to the internet: refused before any connection, said in the stream */
    ps_reset();
    ps_pool_up(LLM_POOL);
    mo = ps_model_openai("http://8.8.8.8/v1");
    llm_acc_init(&a, mo.api, mo.provider);
    k = llm_acc_sink(&a);
    q = req_of(&mo, &c, &k, key);
    CHECK_INT(llm_request(&q), -1);
    CHECK_INT(ps.opens, 0);
    jw_reset(&m);
    llm_acc_message(&a, ts_of(1, 0), &m);
    CHECK(strstr(m.p, "plain http is only for the LAN") != 0 && strstr(m.p, "8.8.8.8 is not one: use https") != 0);
    llm_acc_free(&a);
    mo = ps_model_openai("http://public.example/v1");
    llm_acc_init(&a, mo.api, mo.provider);
    k = llm_acc_sink(&a);
    q = req_of(&mo, &c, &k, key);
    CHECK_INT(llm_request(&q), -1);
    mo = ps_model_openai("http://unresolvable.example/v1");
    q = req_of(&mo, &c, &k, key);
    CHECK_INT(llm_request(&q), -1);
    CHECK_INT(ps.opens, 0);
    llm_acc_free(&a);

    /* https: the key goes; the debug log has it blanked, for both backends */
    ps_reset();
    ps_pool_up(LLM_POOL);
    ps_sse("../pi/openai_text.sse", 0);
    ps_sse("tool_use.sse", 0);
    mo = ps_model_openai("https://api.openai.com/v1");
    mo.provider = "openai";
    llm_acc_init(&a, mo.api, mo.provider);
    k = llm_acc_sink(&a);
    q = req_of(&mo, &c, &k, key);
    CHECK_INT(llm_request(&q), 0);
    CHECK_INT(q.key_sent, 1);
    CHECK(ps.nreq == 1 && strstr(ps.head[0], "Authorization: Bearer sk-test-key-not-real\r\n") != 0);
    CHECK(ps.log.p && strstr(ps.log.p, "Authorization: [redacted]\r\n") != 0);
    CHECK(ps.log.p && strstr(ps.log.p, "chat.completion.chunk") != 0);  /* the events are logged too */
    llm_acc_free(&a);
    {
        llm_model ma = ps_model_anthropic();
        llm_ctx ca = ctx_of(&ma, 1);
        llm_acc_init(&a, ma.api, ma.provider);
        k = llm_acc_sink(&a);
        q = req_of(&ma, &ca, &k, key);
        CHECK_INT(llm_request(&q), 0);
        CHECK(ps.nreq == 2 && strstr(ps.head[1], "x-api-key: sk-test-key-not-real\r\n") != 0);
        CHECK(ps.log.p && strstr(ps.log.p, "x-api-key: [redacted]\r\n") != 0);
        llm_acc_free(&a);
    }
    CHECK(ps.log.p && strstr(ps.log.p, key) == 0);
    jw_free(&m);
}

/* PB3/PB2 (pi f10993bc anthropic-messages.ts:1613-1638 mapStopReason,
 * openai-completions.ts:1562-1586): a stop reason pi-ai calls an error
 * makes the message an error with pi-ai's words */
static void stop_case(const llm_backend *be, const char *const *ev, int n, const char *want_stop,
                      const char *want_err)
{
    llm_acc a;
    llm_sink k;
    void *d = be->dec_new();
    jw m;
    int i;
    llm_acc_init(&a, be->api, "p");
    k = llm_acc_sink(&a);
    for (i = 0; i < n; i += 2)
        be->feed(d, ev[i], ev[i + 1], (long)strlen(ev[i + 1]), &k);
    be->dec_free(d);
    jw_init(&m);
    llm_acc_message(&a, ts_of(1760100000L, 0), &m);
    CHECK(m.p && strstr(m.p, want_stop) != 0);
    if (want_err)
        CHECK(m.p && strstr(m.p, want_err) != 0);
    else
        CHECK(m.p && strstr(m.p, "errorMessage") == 0);
    if (m.p && (!strstr(m.p, want_stop) || (want_err && !strstr(m.p, want_err))))
        printf("    message %s\n", m.p);
    jw_free(&m);
    llm_acc_free(&a);
}

#define A_START "{\"type\":\"message_start\",\"message\":{\"id\":\"msg_1\",\"model\":\"m\",\"usage\":{\"input_tokens\":1}}}"
#define A_TEXT "{\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}"
#define A_END "{\"type\":\"content_block_stop\",\"index\":0}"
#define A_STOP "{\"type\":\"message_stop\"}"
#define O_CH(fr) "{\"id\":\"c\",\"object\":\"chat.completion.chunk\",\"created\":1,\"model\":\"m\",\"choices\":[{\"index\":0,\"delta\":{\"content\":\"x\"},\"finish_reason\":" fr "}]}"

static void test_stops(void)
{
    const char *refusal[] = {
        "message_start", A_START, "content_block_start", A_TEXT, "content_block_stop", A_END,
        "message_delta", ("{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"refusal\",\"stop_details\":"
                         "{\"type\":\"refusal\",\"category\":\"cyber\",\"explanation\":null}}}"),
        "message_stop", A_STOP };
    const char *explained[] = {
        "message_start", A_START,
        "message_delta", ("{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"refusal\",\"stop_details\":"
                         "{\"type\":\"refusal\",\"explanation\":\"Not this.\"}}}"),
        "message_stop", A_STOP };
    const char *sensitive[] = {
        "message_start", A_START,
        "message_delta", "{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"sensitive\"}}",
        "message_stop", A_STOP };
    const char *unknown[] = {
        "message_start", A_START,
        "message_delta", "{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"brand_new\"}}",
        "message_stop", A_STOP };
    const char *pause[] = {
        "message_start", A_START,
        "message_delta", "{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"pause_turn\"}}",
        "message_stop", A_STOP };
    const char *o_filter[] = { "", O_CH("\"content_filter\""), "", "[DONE]" };
    const char *o_end[] = { "", O_CH("\"end\""), "", "[DONE]" };
    const char *o_new[] = { "", O_CH("\"brand_new\""), "", "[DONE]" };
    const char *o_len[] = { "", O_CH("\"length\""), "", "[DONE]" };

    stop_case(&llm_be_anthropic, refusal, 10, "\"stopReason\":\"error\"",
              "\"errorMessage\":\"The model refused to complete the request\"");
    stop_case(&llm_be_anthropic, explained, 6, "\"stopReason\":\"error\"", "\"errorMessage\":\"Not this.\"");
    stop_case(&llm_be_anthropic, sensitive, 6, "\"stopReason\":\"error\"",
              "\"errorMessage\":\"Provider stopped with: sensitive\"");
    stop_case(&llm_be_anthropic, unknown, 6, "\"stopReason\":\"error\"",
              "\"errorMessage\":\"Unhandled stop reason: brand_new\"");
    stop_case(&llm_be_anthropic, pause, 6, "\"stopReason\":\"stop\"", 0);
    stop_case(&llm_be_openai_chat, o_filter, 4, "\"stopReason\":\"error\"",
              "\"errorMessage\":\"Provider finish_reason: content_filter\"");
    stop_case(&llm_be_openai_chat, o_end, 4, "\"stopReason\":\"stop\"", 0);
    stop_case(&llm_be_openai_chat, o_new, 4, "\"stopReason\":\"error\"",
              "\"errorMessage\":\"Provider finish_reason: brand_new\"");
    stop_case(&llm_be_openai_chat, o_len, 4, "\"stopReason\":\"length\"", 0);
}

/* PH-S: pi-ai 1.0.4's message fields -- usage.cacheWrite1h, responseId, rawStopReason (the provider's own words) and the
 * agent loop's thinkingLevel -- each written as the stream sets them (pi f10993bc/1.0.4 anthropic-messages.ts:666-684,
 * 819, 846; openai-completions.ts:564, 582; openai-responses-shared.ts:558-602, 589, 749) and kept by the round trip */
#define PHS_CREATED "{\"type\":\"response.created\",\"response\":{\"id\":\"resp_9\",\"status\":\"in_progress\"," \
                    "\"model\":\"m\",\"output\":[]}}"

static void phs_feed(const llm_backend *be, const char *const *ev, int n, const char *level, jw *m)
{
    llm_acc a;
    llm_sink k;
    void *d = be->dec_new();
    int i;
    llm_acc_init(&a, be->api, "p");
    if (level)
        llm_acc_thinking(&a, level);
    k = llm_acc_sink(&a);
    for (i = 0; i < n; i += 2)
        be->feed(d, ev[i], ev[i + 1], (long)strlen(ev[i + 1]), &k);
    be->dec_free(d);
    jw_init(m);
    llm_acc_message(&a, ts_of(1760100000L, 0), m);
    llm_acc_free(&a);
}

static void test_phs(void)
{
    jw m, c;
    const char *ant[] = {
        "message_start", ("{\"type\":\"message_start\",\"message\":{\"id\":\"msg_77\",\"model\":\"m\",\"usage\":{"
                         "\"input_tokens\":5,\"cache_creation_input_tokens\":300,\"cache_creation\":{"
                         "\"ephemeral_5m_input_tokens\":100,\"ephemeral_1h_input_tokens\":200}}}}"),
        "content_block_start", A_TEXT, "content_block_stop", A_END,
        "message_delta", ("{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{"
                         "\"output_tokens\":9}}"),
        "message_stop", A_STOP };
    const char *ant_plain[] = {
        "message_start", A_START,
        "message_delta", ("{\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"max_tokens\"},\"usage\":{"
                         "\"cache_creation\":{\"ephemeral_1h_input_tokens\":40}}}"),
        "message_stop", A_STOP };
    const char *chat[] = { "", O_CH("null"), "", O_CH("\"length\""), "", "[DONE]" };
    const char *rdone[] = {
        "response.created", PHS_CREATED,
        "response.completed", ("{\"type\":\"response.completed\",\"response\":{\"id\":\"resp_9\",\"status\":"
                              "\"incomplete\",\"incomplete_details\":{\"reason\":\"max_output_tokens\"},"
                              "\"model\":\"m\",\"output\":[]}}") };
    const char *rfail[] = {
        "response.created", PHS_CREATED,
        "response.failed", ("{\"type\":\"response.failed\",\"response\":{\"id\":\"resp_9\",\"status\":\"failed\","
                           "\"error\":{\"code\":\"server_error\",\"message\":\"overloaded\"}}}") };

    /* anthropic: the id and the 1 hour cache write from message_start, the stop reason as the wire says it, the level
     * last; no level on a message that was not made by the agent loop */
    phs_feed(&llm_be_anthropic, ant, 10, "high", &m);
    CHECK(m.p && strstr(m.p, "\"total\":0},\"cacheWrite1h\":200},\"stopReason\":\"stop\",\"timestamp\":1760100000000,"
                             "\"responseId\":\"msg_77\",\"rawStopReason\":\"end_turn\","
                             "\"thinkingLevel\":\"high\"}") != 0);
    jw_init(&c);
    CHECK_INT(llm_msg_canon(m.p, m.n, &c), LR_ASSISTANT);      /* a stored message keeps all four */
    CHECK(c.p && strstr(c.p, "\"cacheWrite1h\":200}") && strstr(c.p, "\"responseId\":\"msg_77\"") &&
          strstr(c.p, "\"rawStopReason\":\"end_turn\"") && strstr(c.p, "\"thinkingLevel\":\"high\""));
    jw_free(&c);
    jw_free(&m);
    {
        /* a sum of two usages (compaction of a split turn): pi's combineUsage puts cacheWrite1h before totalTokens */
        llm_usage u;
        jw w;
        memset(&u, 0, sizeof(u));
        u.input = 2;
        u.has_1h = u.combined = 1;
        u.cache_write_1h = 7;
        jw_init(&w);
        llm_w_usage(&w, &u);
        CHECK(w.p && strstr(w.p, "\"cacheWrite\":0,\"cacheWrite1h\":7,\"totalTokens\":2,\"cost\":{") != 0 &&
              strstr(w.p, "\"total\":0}}") != 0);
        jw_free(&w);
    }
    phs_feed(&llm_be_anthropic, ant, 10, 0, &m);
    CHECK(m.p && strstr(m.p, "thinkingLevel") == 0);
    jw_free(&m);
    /* message_start without a cache_creation object: pi-ai still writes 0; a delta's own number replaces it */
    phs_feed(&llm_be_anthropic, ant_plain, 6, "off", &m);
    CHECK(m.p && strstr(m.p, "\"cacheWrite1h\":40}") && strstr(m.p, "\"rawStopReason\":\"max_tokens\",\"thinkingLevel\":\"off\"}"));
    jw_free(&m);
    /* openai chat: the chunk's id, finish_reason as sent, no cacheWrite1h in the usage object */
    phs_feed(&llm_be_openai_chat, chat, 6, "off", &m);
    CHECK(m.p && strstr(m.p, "\"responseId\":\"c\"") && strstr(m.p, "\"rawStopReason\":\"length\",\"thinkingLevel\":\"off\"}") &&
          strstr(m.p, "cacheWrite1h") == 0);
    jw_free(&m);
    /* openai responses: the response id, status.reason; a failed response keeps its status */
    phs_feed(&llm_be_openai_responses, rdone, 4, 0, &m);
    CHECK(m.p && strstr(m.p, "\"responseId\":\"resp_9\"") && strstr(m.p, "\"rawStopReason\":\"incomplete.max_output_tokens\"}"));
    jw_free(&m);
    phs_feed(&llm_be_openai_responses, rfail, 4, 0, &m);
    CHECK(m.p && strstr(m.p, "\"responseId\":\"resp_9\"") && strstr(m.p, "\"rawStopReason\":\"failed\",\"errorMessage\":"));
    if (m.p && !strstr(m.p, "\"rawStopReason\":\"failed\",\"errorMessage\":"))
        printf("    message %s\n", m.p);
    jw_free(&m);
}

/* PB2: pi-ai's compat detection by provider and base URL (pi f10993bc
 * openai-completions.ts:1593-1690 detectCompat): which token-cap field and
 * system role a reasoning model's body gets */
static void compat_case(const char *provider, const char *base, const char *id, const char *cap, const char *role)
{
    llm_model m = ps_model_openai(base);
    llm_ctx c;
    jw w;
    m.provider = provider;
    m.id = id;
    m.reasoning = 1;
    c = ctx_of(&m, 1);
    jw_init(&w);
    CHECK_INT(llm_be_openai_chat.body(&c, &w), 0);
    CHECK(w.p && strstr(w.p, cap) != 0);
    CHECK(w.p && strstr(w.p, role) != 0);
    if (w.p && (!strstr(w.p, cap) || !strstr(w.p, role)))
        printf("    %s %s: %.160s\n", provider, base, w.p);
    jw_free(&w);
}

static void test_compat(void)
{
    const char *mct = "\"max_completion_tokens\":", *mt = "\"max_tokens\":";
    const char *dev = "{\"role\":\"developer\"", *sys = "{\"role\":\"system\"";
    compat_case("ollama", "http://192.168.1.10:11434/v1", "q", mct, dev);
    compat_case("openai", "https://api.openai.com/v1", "o3", mct, dev);
    compat_case("deepseek", "https://api.deepseek.com/v1", "r", mt, sys);
    compat_case("x", "https://API.DeepSeek.com/v1", "r", mt, sys);
    compat_case("x", "https://api.together.xyz/v1", "r", mt, sys);
    compat_case("moonshotai", "https://example/v1", "k", mt, sys);
    compat_case("x", "https://api.z.ai/api/paas/v4", "g", mt, sys);
    compat_case("x", "https://integrate.api.nvidia.com/v1", "n", mt, sys);
    compat_case("x", "https://llm.chutes.ai/v1", "c", mt, sys);
    compat_case("x", "https://gateway.ai.cloudflare.com/v1/a/b/compat", "c", mt, sys);
    compat_case("x", "https://api.ant-ling.com/v1", "l", mt, sys);
    compat_case("xai", "https://example/v1", "grok", mct, sys);
    compat_case("cerebras", "https://example/v1", "c", mct, sys);
    compat_case("opencode", "https://example/v1", "c", mct, sys);
    compat_case("cloudflare-workers-ai", "https://example/v1", "c", mct, sys);
    compat_case("openrouter", "https://openrouter.ai/api/v1", "qwen/q", mct, sys);
    compat_case("openrouter", "https://openrouter.ai/api/v1", "anthropic/claude", mct, dev);
    compat_case("x", "https://openrouter.ai/api/v1", "openai/o3", mct, dev);
}

/* an answer cut at its length keeps its last tool call with the arguments
 * completed as far as they go (pi-ai's salvage); any other stop with bad
 * arguments is an error (B2) */
static void salvage_case(const char *args_json_str, const char *fr, const char *want)
{
    const char *ev[6];
    char c1[400], c2[200];
    llm_acc a;
    llm_sink k;
    void *d = llm_be_openai_chat.dec_new();
    jw m;
    int i;
    strcpy(c1, "{\"id\":\"c\",\"object\":\"chat.completion.chunk\",\"created\":1,\"model\":\"m\",\"choices\":[{\"index\":0,"
               "\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_1\",\"type\":\"function\",\"function\":"
               "{\"name\":\"read\",\"arguments\":");
    strcat(c1, args_json_str);
    strcat(c1, "}}]},\"finish_reason\":null}]}");
    strcpy(c2, "{\"id\":\"c\",\"object\":\"chat.completion.chunk\",\"created\":1,\"model\":\"m\",\"choices\":[{\"index\":0,"
               "\"delta\":{},\"finish_reason\":");
    strcat(c2, fr);
    strcat(c2, "}]}");
    ev[0] = "";
    ev[1] = c1;
    ev[2] = "";
    ev[3] = c2;
    ev[4] = "";
    ev[5] = "[DONE]";
    llm_acc_init(&a, "openai-completions", "p");
    k = llm_acc_sink(&a);
    for (i = 0; i < 6; i += 2)
        llm_be_openai_chat.feed(d, ev[i], ev[i + 1], (long)strlen(ev[i + 1]), &k);
    llm_be_openai_chat.dec_free(d);
    jw_init(&m);
    llm_acc_message(&a, ts_of(1760100000L, 0), &m);
    CHECK(m.p && strstr(m.p, want) != 0);
    if (m.p && !strstr(m.p, want))
        printf("    message %s\n", m.p);
    jw_free(&m);
    llm_acc_free(&a);
}

static void test_salvage(void)
{
    salvage_case("\"{\\\"path\\\":\\\"S:\"", "\"length\"", "\"arguments\":{\"path\":\"S:\"}");
    salvage_case("\"{\\\"a\\\":1,\\\"b\\\":\"", "\"length\"", "\"arguments\":{\"a\":1}");
    salvage_case("\"{\\\"a\\\":[1,2\"", "\"length\"", "\"arguments\":{\"a\":[1,2]}");
    salvage_case("\"{\\\"a\\\"\"", "\"length\"", "\"arguments\":{}");
    salvage_case("\"{\\\"path\\\":\\\"S:\"", "\"length\"", "\"stopReason\":\"length\"");
    salvage_case("\"{\\\"path\\\":\\\"S:\"", "\"tool_calls\"", "\"errorMessage\":\"the arguments of tool call read are not valid JSON\"");
}

void suite_pi_llm(void)
{
    test_messages();
    test_accum();
    test_reach();
    test_anthropic_baseurl();
    test_responses();
    test_anthropic_pi();
    test_openai_bodies();
    test_completions();
    test_strict_schema();
    test_estimate();
    test_request();
    test_pool();
    test_post_hooks();
    test_lan();
    test_stops();
    test_phs();
    test_compat();
    test_salvage();
    ps_reset();
}
