/* amiga-pi's provider-neutral LLM layer (llm/, plan 2026-10-07-amiga-pi
 * phase B) on the host: pi-ai's message model and its round trip (B1), the
 * neutral events into an assistant message (B2). */
#include <stdlib.h>
#include <string.h>
#include "harness.h"
#include "../llm/llm.h"
#include "../llm/event.h"
#include "../llm/accum.h"

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
    CHECK_INT(inf.ts.sec, 1760100000L);
    CHECK_INT(inf.ts.ms, 42);

    /* an error answer keeps its message */
    jw_reset(&m);
    mt.stop = LS_ERROR;
    mt.error = "overloaded";
    llm_m_assistant(&m, "[]", 2, &mt, ts);
    CHECK(strstr(m.p, "\"stopReason\":\"error\",\"errorMessage\":\"overloaded\",") != 0);
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
                   "\"cacheWrite\":0,\"total\":0}},\"stopReason\":\"toolUse\",\"timestamp\":1760100000000}");
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
    CHECK(strstr(m.p, "\"stopReason\":\"error\",\"errorMessage\":\"the arguments of tool call edit are not valid "
                      "JSON\"") != 0);
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
    CHECK(strstr(m.p, "\"stopReason\":\"aborted\",\"errorMessage\":\"Stopped by Ctrl+C\"") != 0);
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

void suite_pi_llm(void)
{
    test_messages();
    test_accum();
}
