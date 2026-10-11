/* amiga-pi's provider-neutral LLM layer (llm/, plan 2026-10-07-amiga-pi
 * phase B) on the host: pi-ai's message model and its round trip (B1), the
 * neutral events into an assistant message (B2). */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "../llm/llm.h"
#include "../llm/event.h"
#include "../llm/accum.h"
#include "../llm/request.h"
#include "claude_load.h"

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

/* ---- a stub transport: canned HTTP answers, the requests kept ---- */

#define PS_MAX 12

typedef struct pstub {
    char *resp[PS_MAX];
    long rlen[PS_MAX];
    int nresp, next, cur;
    long pos;
    jw req;
    char *head[PS_MAX], *body[PS_MAX];
    int nreq, opens, closes;
    long brk_at;                /* the poll says stop once this many bytes went out */
    int brk;
    int silent;                 /* answer nothing at all (the idle limit) */
    int sleeps;
    long slept;
    int notices;
    char notice[160];
    jw log;
    unsigned long clock;
} pstub;

static pstub ps;

static void ps_reset(void)
{
    int i;
    for (i = 0; i < ps.nresp; i++)
        free(ps.resp[i]);
    for (i = 0; i < ps.nreq; i++) {
        free(ps.head[i]);
        free(ps.body[i]);
    }
    jw_free(&ps.req);
    jw_free(&ps.log);
    memset(&ps, 0, sizeof(ps));
    ps.cur = -1;
    jw_init(&ps.req);
    jw_init(&ps.log);
}

/* an SSE recording as a chunked 200 answer (cut: no terminating chunk, the connection closes) */
static void ps_sse(const char *name, int cut)
{
    long n, i;
    char *sse = claude_load(name, &n);
    jw w;
    static const char hex[] = "0123456789abcdef";
    if (!sse)
        return;
    jw_init(&w);
    jw_rawz(&w, "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nTransfer-Encoding: chunked\r\n\r\n");
    for (i = 0; i < n; i += 100) {
        long k = n - i < 100 ? n - i : 100;
        char h[8];
        int hl = 0;
        if (k >= 16)
            h[hl++] = hex[k >> 4];
        h[hl++] = hex[k & 15];
        jw_raw(&w, h, hl);
        jw_raw(&w, "\r\n", 2);
        jw_raw(&w, sse + i, k);
        jw_raw(&w, "\r\n", 2);
    }
    if (!cut)
        jw_rawz(&w, "0\r\n\r\n");
    free(sse);
    ps.resp[ps.nresp] = w.p;
    ps.rlen[ps.nresp++] = cut ? -w.n : w.n;     /* negative: close after it */
}

static void ps_raw(const char *name)
{
    long n;
    char *b = claude_load(name, &n);
    if (!b)
        return;
    ps.resp[ps.nresp] = b;
    ps.rlen[ps.nresp++] = n;
}

static int s_open(void *u, const char *host, int port, int tls)
{
    (void)u;
    (void)host;
    (void)port;
    (void)tls;
    ps.opens++;
    return 0;
}

static long s_send(void *u, const char *b, long n)
{
    char *e;
    (void)u;
    jw_raw(&ps.req, b, n);
    e = strstr(ps.req.p, "\r\n\r\n");
    if (e) {
        char *cl = strstr(ps.req.p, "Content-Length: ");
        long hl = (long)(e + 4 - ps.req.p), bl = cl ? atol(cl + 16) : 0;
        if (ps.req.n >= hl + bl && ps.nreq < PS_MAX) {
            ps.head[ps.nreq] = (char *)malloc((size_t)hl + 1);
            memcpy(ps.head[ps.nreq], ps.req.p, (size_t)hl);
            ps.head[ps.nreq][hl] = 0;
            ps.body[ps.nreq] = (char *)malloc((size_t)bl + 1);
            memcpy(ps.body[ps.nreq], ps.req.p + hl, (size_t)bl);
            ps.body[ps.nreq][bl] = 0;
            ps.nreq++;
            jw_reset(&ps.req);
            ps.cur = ps.next < ps.nresp ? ps.next++ : -1;
            ps.pos = 0;
        }
    }
    return n;
}

static long s_recv(void *u, char *b, long cap, int timeout_ms)
{
    long n, len;
    (void)u;
    (void)timeout_ms;
    ps.clock += 250;
    if (ps.silent || ps.cur < 0)
        return ps.silent ? NET_TIMEOUT : 0;
    len = ps.rlen[ps.cur] < 0 ? -ps.rlen[ps.cur] : ps.rlen[ps.cur];
    n = len - ps.pos;
    if (!n)
        return ps.rlen[ps.cur] < 0 ? 0 : NET_TIMEOUT;
    if (n > 37)
        n = 37;                 /* small reads: every split is tried */
    if (n > cap)
        n = cap;
    memcpy(b, ps.resp[ps.cur] + ps.pos, (size_t)n);
    ps.pos += n;
    if (ps.brk_at && ps.pos >= ps.brk_at)
        ps.brk = 1;
    return n;
}

static void s_close(void *u)
{
    (void)u;
    ps.closes++;
    ps.cur = -1;
}

static const char *s_err(void *u)
{
    (void)u;
    return "stub";
}

static int h_poll(void *u)
{
    (void)u;
    return ps.brk;
}

static unsigned long h_ms(void *u)
{
    (void)u;
    return ps.clock;
}

static int h_sleep(void *u, long ms)
{
    (void)u;
    ps.sleeps++;
    ps.slept += ms;
    return 0;
}

static void h_notice(void *u, const char *t)
{
    (void)u;
    ps.notices++;
    strncpy(ps.notice, t, sizeof(ps.notice) - 1);
}

static int h_resolve(void *u, const char *host, unsigned char ip[4])
{
    (void)u;
    if (!strcmp(host, "nas.lan")) {
        ip[0] = 192, ip[1] = 168, ip[2] = 1, ip[3] = 10;
        return 0;
    }
    if (!strcmp(host, "public.example")) {
        ip[0] = 8, ip[1] = 8, ip[2] = 8, ip[3] = 8;
        return 0;
    }
    return -1;
}

static void h_log(void *u, const char *s, long n)
{
    (void)u;
    jw_raw(&ps.log, s, n);
}

static cl_net nets[LLM_POOL];
static cl_net *netp[LLM_POOL];
static llm_pool pool;

static void pool_up(int n)
{
    int i;
    for (i = 0; i < LLM_POOL; i++) {
        memset(&nets[i], 0, sizeof(nets[i]));
        nets[i].open = s_open;
        nets[i].send = s_send;
        nets[i].recv = s_recv;
        nets[i].close = s_close;
        nets[i].err = s_err;
        netp[i] = &nets[i];
    }
    llm_pool_init(&pool, netp, n);
}

static llm_model model_openai(const char *base)
{
    llm_model m;
    memset(&m, 0, sizeof(m));
    m.id = "qwen2.5-coder:7b";
    m.api = "openai-completions";
    m.provider = "ollama";
    m.base_url = base;
    m.max_tokens = 4096;
    m.compat.max_tokens_field = m.compat.usage_in_streaming = m.compat.developer_role =
        m.compat.tool_result_name = LC_UNSET;
    return m;
}

static llm_model model_anthropic(void)
{
    llm_model m;
    memset(&m, 0, sizeof(m));
    m.id = "claude-opus-5-5";
    m.api = "anthropic-messages";
    m.provider = "anthropic";
    m.base_url = "https://api.anthropic.com/v1";
    m.max_tokens = 8192;
    m.compat.max_tokens_field = m.compat.usage_in_streaming = m.compat.developer_role =
        m.compat.tool_result_name = LC_UNSET;
    return m;
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
    c.msgs = conv_msgs;
    c.msg_n = conv_n;
    c.nmsg = nmsg;
    c.tools = tools_json;
    c.tools_n = (long)strlen(tools_json);
    return c;
}

static llm_req req_of(const llm_model *m, const llm_ctx *c, const llm_sink *k, const char *key)
{
    llm_req q;
    memset(&q, 0, sizeof(q));
    q.pool = &pool;
    q.model = m;
    q.be = llm_backend_of(m->api);
    q.ctx = c;
    q.key = key;
    q.user_agent = "UP-Term-amiga-pi/0.1 (AmigaOS)";
    q.h.poll = h_poll;
    q.h.ms = h_ms;
    q.h.sleep = h_sleep;
    q.h.notice = h_notice;
    q.h.resolve = h_resolve;
    q.h.log = h_log;
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

/* B11: one top-level request per backend, the neutral message of the same shape */
static void test_reach(void)
{
    llm_model mo = model_openai("http://192.168.1.10:11434/v1"), ma = model_anthropic();
    llm_ctx c;
    llm_req q;
    llm_acc a;
    llm_sink k;
    jw m;
    char ks[200];
    long f0 = llm_feeds_openai, a0 = llm_feeds_anthropic;
    llm_info inf;

    ps_reset();
    pool_up(LLM_POOL);
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
    pool_up(LLM_POOL);
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
        llm_model mo = model_openai("https://api.openai.com/v1");
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
        llm_model mo = model_openai("http://192.168.1.10:11434/v1");
        llm_ctx c = ctx_of(&mo, 4);
        jw w;
        jw_init(&w);
        CHECK_INT(llm_be_openai_chat.body(&c, &w), 0);
        CHECK(strstr(w.p, "{\"role\":\"system\",\"content\":\"You are amiga-pi.\"}") != 0);
        CHECK(strstr(w.p, "\"stream_options\":{\"include_usage\":true}") != 0);
        CHECK(strstr(w.p, "\"max_completion_tokens\":4096") != 0);
        CHECK(strstr(w.p, "\"tool_call_id\":\"call_1_item_9\"") != 0 && strstr(w.p, "call_1|item_9") == 0);
        CHECK(strstr(w.p, "\"name\":\"read\"}") == 0);
        CHECK(strstr(w.p, "Read it.") == 0);     /* another provider's thinking is not sent */
        jw_free(&w);
    }
    /* the Anthropic body from the same messages: thinking with its
     * signature (same api), the calls, both results in one user turn */
    {
        llm_model ma = model_anthropic();
        llm_ctx c = ctx_of(&ma, 4);
        jw w;
        jw_init(&w);
        CHECK_INT(llm_be_anthropic.body(&c, &w), 0);
        CHECK(strstr(w.p, "{\"type\":\"thinking\",\"thinking\":\"Read it.\",\"signature\":\"sig==\"}") != 0);
        CHECK(strstr(w.p, "{\"type\":\"tool_use\",\"id\":\"call_1_item_9\",\"name\":\"read\",\"input\":"
                          "{\"path\":\"S:Startup-Sequence\"}}") != 0);
        CHECK(strstr(w.p, "{\"role\":\"user\",\"content\":[{\"type\":\"tool_result\",\"tool_use_id\":"
                          "\"call_1_item_9\",\"content\":[{\"type\":\"text\",\"text\":\"SetPatch QUIET\\n\"}]},"
                          "{\"type\":\"tool_result\",\"tool_use_id\":\"call_2\",\"content\":[{\"type\":\"text\","
                          "\"text\":\"Startup-Sequence\\n\"}],\"is_error\":true}]}") != 0);
        CHECK(strstr(w.p, "\"input_schema\":{\"type\":\"object\"") != 0);
        jw_free(&w);
    }
}

/* B3: done, a cut stream, a 429 with Retry-After 2, Ctrl+C mid-stream, the idle limit; usage */
static void test_request(void)
{
    llm_model mo = model_openai("http://192.168.1.10:11434/v1");
    llm_ctx c = ctx_of(&mo, 1);
    llm_req q;
    llm_acc a;
    llm_sink k;
    jw m;
    jw_init(&m);

    /* [DONE]: complete; the include_usage chunk's counts (cached tokens apart) */
    ps_reset();
    pool_up(LLM_POOL);
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
    pool_up(LLM_POOL);
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

    /* cut off after part of the answer: an error, not tried again (the text kept) */
    ps_reset();
    pool_up(LLM_POOL);
    ps_sse("../pi/openai_cut.sse", 1);
    llm_acc_init(&a, mo.api, mo.provider);
    k = llm_acc_sink(&a);
    q = req_of(&mo, &c, &k, 0);
    CHECK_INT(llm_request(&q), -1);
    CHECK_INT(q.attempts, 1);
    jw_reset(&m);
    llm_acc_message(&a, ts_of(1, 0), &m);
    CHECK(strstr(m.p, "\"text\":\"This answer is cut off in the mid\"") != 0);
    CHECK(strstr(m.p, "\"stopReason\":\"error\",\"errorMessage\":\"the connection closed early\"") != 0);
    llm_acc_free(&a);

    /* Ctrl+C mid-stream: aborted, the text so far kept */
    ps_reset();
    pool_up(LLM_POOL);
    ps_sse("../pi/openai_text.sse", 0);
    ps.brk_at = 600;
    llm_acc_init(&a, mo.api, mo.provider);
    k = llm_acc_sink(&a);
    q = req_of(&mo, &c, &k, 0);
    CHECK_INT(llm_request(&q), -1);
    jw_reset(&m);
    llm_acc_message(&a, ts_of(1, 0), &m);
    CHECK(strstr(m.p, "\"text\":\"Hello from \"") != 0);
    CHECK(strstr(m.p, "\"stopReason\":\"aborted\",\"errorMessage\":\"Stopped.\"") != 0);
    CHECK_INT(ps.nreq, 1);
    llm_acc_free(&a);

    /* the server says nothing: the idle limit, three retries (2, 4, 8 s), then an error */
    ps_reset();
    pool_up(LLM_POOL);
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
    pool_up(LLM_POOL);
    for (i = 0; i < 10; i++) {
        c = llm_pool_get(&pool, i & 1 ? "b.lan" : "a.lan", 80, 0, &reused, &rc);
        CHECK(c != 0);
        CHECK_INT(reused, i >= 2);
    }
    CHECK_INT(pool.opens, 2);
    CHECK_INT(llm_pool_open(&pool), 2);
    llm_pool_get(&pool, "c.lan", 80, 0, &reused, &rc);
    llm_pool_get(&pool, "d.lan", 80, 0, &reused, &rc);
    llm_pool_get(&pool, "a.lan", 80, 0, &reused, &rc);     /* a used last: b is the oldest */
    CHECK_INT(llm_pool_open(&pool), 4);
    CHECK_INT(ps.closes, 0);
    llm_pool_get(&pool, "e.lan", 80, 0, &reused, &rc);     /* a fifth: b closed */
    CHECK_INT(ps.closes, 1);
    CHECK_INT(llm_pool_open(&pool), 4);
    llm_pool_get(&pool, "a.lan", 80, 0, &reused, &rc);
    CHECK_INT(reused, 1);
    llm_pool_get(&pool, "b.lan", 80, 0, &reused, &rc);
    CHECK_INT(reused, 0);
    CHECK_INT(pool.opens, 6);
    /* the same host on another port or with TLS is another connection */
    llm_pool_get(&pool, "a.lan", 443, 1, &reused, &rc);
    CHECK_INT(reused, 0);
    llm_pool_close(&pool);
    CHECK_INT(llm_pool_open(&pool), 0);
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
    pool_up(LLM_POOL);
    ps_sse("../pi/openai_text.sse", 0);
    ps_sse("../pi/openai_text.sse", 0);
    mo = model_openai("http://192.168.1.10:11434/v1");
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
    pool_up(LLM_POOL);
    ps_sse("../pi/openai_text.sse", 0);
    ps_sse("../pi/openai_text.sse", 0);
    mo = model_openai("http://nas.lan:8080/v1");
    q = req_of(&mo, &c, &k, 0);
    llm_acc_init(&a, mo.api, mo.provider);
    CHECK_INT(llm_request(&q), 0);
    mo = model_openai("http://studio.local:1234/v1");
    q = req_of(&mo, &c, &k, 0);
    CHECK_INT(llm_request(&q), 0);
    CHECK_INT(ps.nreq, 2);
    llm_acc_free(&a);

    /* http to the internet: refused before any connection, said in the stream */
    ps_reset();
    pool_up(LLM_POOL);
    mo = model_openai("http://8.8.8.8/v1");
    llm_acc_init(&a, mo.api, mo.provider);
    k = llm_acc_sink(&a);
    q = req_of(&mo, &c, &k, key);
    CHECK_INT(llm_request(&q), -1);
    CHECK_INT(ps.opens, 0);
    jw_reset(&m);
    llm_acc_message(&a, ts_of(1, 0), &m);
    CHECK(strstr(m.p, "plain http is only for the LAN") != 0 && strstr(m.p, "8.8.8.8 is not one: use https") != 0);
    llm_acc_free(&a);
    mo = model_openai("http://public.example/v1");
    llm_acc_init(&a, mo.api, mo.provider);
    k = llm_acc_sink(&a);
    q = req_of(&mo, &c, &k, key);
    CHECK_INT(llm_request(&q), -1);
    mo = model_openai("http://unresolvable.example/v1");
    q = req_of(&mo, &c, &k, key);
    CHECK_INT(llm_request(&q), -1);
    CHECK_INT(ps.opens, 0);
    llm_acc_free(&a);

    /* https: the key goes; the debug log has it blanked, for both backends */
    ps_reset();
    pool_up(LLM_POOL);
    ps_sse("../pi/openai_text.sse", 0);
    ps_sse("tool_use.sse", 0);
    mo = model_openai("https://api.openai.com/v1");
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
        llm_model ma = model_anthropic();
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

/* PB2: pi-ai's compat detection by provider and base URL (pi f10993bc
 * openai-completions.ts:1593-1690 detectCompat): which token-cap field and
 * system role a reasoning model's body gets */
static void compat_case(const char *provider, const char *base, const char *id, const char *cap, const char *role)
{
    llm_model m = model_openai(base);
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

void suite_pi_llm(void)
{
    test_messages();
    test_accum();
    test_reach();
    test_openai_bodies();
    test_request();
    test_pool();
    test_lan();
    test_stops();
    test_compat();
    ps_reset();
}
