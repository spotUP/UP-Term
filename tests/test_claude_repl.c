/* The Claude client's reachability test (ledger A2): the REPL core
 * (claude/repl.c, the code C:Claude runs) driven end to end -- typed
 * lines, HTTP over a stub transport that answers with the recorded streams
 * in tests/claude/, the tool round on a temporary tree, the permission
 * question answered from the script -- with a call-count sentinel on the
 * renderer proving the streamed text went through it. Then the paths that
 * must leave the history as it was: Ctrl+C, a refusal, a cut tool call,
 * and the retries (an overloaded stream, a 529 with retry-after). */
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <utime.h>
#include <time.h>
#include "harness.h"
#include "claude_load.h"
#include "../claude/setup.h"
#include "../claude/cli.h"
#include "../claude/repl.h"
#include "../claude/repl_int.h"
#include "../claude/tasks.h"
#include "../claude/print.h"
#include "../claude/sys_posix.h"
#include "../claude/util.h"
#include "../claude/tui.h"
#include "../claude/show.h"
#include "../claude/session.h"
#include "claude_screen.h"

/* ---- the stub transport: one canned HTTP response per request ---- */

#define SB_MAX 16               /* responses and requests a test may have */

typedef struct stub {
    char *resp[SB_MAX];
    long rlen[SB_MAX];
    int nresp, next, cur;
    long pos;
    jw req;                     /* the request being received */
    char *head[SB_MAX], *body[SB_MAX];
    int nreq;
    int opens;
    long chunk;                 /* bytes per recv */
    long brk_at;                /* Ctrl+C once this many bytes of a response went out (0: never) */
    int brk;
} stub;

static stub sb;

static int open_fail;                   /* the setup wizard's connect test fails */
static const char *open_host;
static int open_port;

static int s_open(void *u, const char *host, int port, int tls)
{
    (void)u;
    (void)tls;
    sb.opens++;
    open_host = host;
    open_port = port;
    return open_fail ? NET_ERROR : 0;
}

static long s_send(void *u, const char *b, long n)
{
    char *e;
    (void)u;
    jw_raw(&sb.req, b, n);
    e = strstr(sb.req.p, "\r\n\r\n");
    if (e) {
        char *cl = strstr(sb.req.p, "Content-Length: ");
        long hl = (long)(e + 4 - sb.req.p), bl = cl ? atol(cl + 16) : 0;
        if (sb.req.n >= hl + bl && sb.nreq < SB_MAX) {
            sb.head[sb.nreq] = (char *)malloc((size_t)hl + 1);
            memcpy(sb.head[sb.nreq], sb.req.p, (size_t)hl);
            sb.head[sb.nreq][hl] = 0;
            sb.body[sb.nreq] = (char *)malloc((size_t)bl + 1);
            memcpy(sb.body[sb.nreq], sb.req.p + hl, (size_t)bl);
            sb.body[sb.nreq][bl] = 0;
            sb.nreq++;
            jw_reset(&sb.req);
            sb.cur = sb.next < sb.nresp ? sb.next++ : -1;
            sb.pos = 0;
        }
    }
    return n;
}

static long s_recv(void *u, char *b, long cap, int timeout_ms)
{
    long n;
    (void)u;
    (void)timeout_ms;
    if (sb.cur < 0)
        return 0;                   /* nothing more to say: the far end closes */
    if (sb.brk_at && sb.pos >= sb.brk_at) {
        sb.brk_at = 0;
        sb.brk = 1;
        return NET_TIMEOUT;         /* the program notices the break between reads */
    }
    n = sb.rlen[sb.cur] - sb.pos;
    if (!n)
        return NET_TIMEOUT;         /* a kept connection, quiet */
    if (n > sb.chunk)
        n = sb.chunk;
    if (n > cap)
        n = cap;
    if (sb.brk_at && sb.pos + n > sb.brk_at)
        n = sb.brk_at - sb.pos;
    memcpy(b, sb.resp[sb.cur] + sb.pos, (size_t)n);
    sb.pos += n;
    return n;
}

static void s_close(void *u)
{
    (void)u;
    sb.cur = -1;
}

static const char *s_err(void *u)
{
    (void)u;
    return open_fail ? "cannot connect to host: connection refused (ECONNREFUSED): nothing listens on that port" : "stub";
}

static void add_sse(const char *sse, long n);

/* a recording as the API sends it: chunked, 100 bytes a chunk */
static void add_stream(const char *name)
{
    long n;
    char *sse = claude_load(name, &n);
    if (!sse)
        return;
    add_sse(sse, n);
    free(sse);
}

/* An answer made here (A4 gaps tests): a tool call, the input JSON as it
 * is; or (name 0) a text answer. Same events as the recordings. */
static void add_answer(const char *id, const char *name, const char *input, const char *text)
{
    jw s;
    jw_init(&s);
    jw_rawz(&s, "event: message_start\ndata: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_gap\",\"type\":"
                "\"message\",\"role\":\"assistant\",\"model\":\"claude-opus-5-5\",\"content\":[],\"stop_reason\":"
                "null,\"stop_sequence\":null,\"usage\":{\"input_tokens\":100,\"cache_creation_input_tokens\":0,"
                "\"cache_read_input_tokens\":0,\"output_tokens\":1}}}\n\n");
    if (name) {
        jw_rawz(&s, "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":0,"
                    "\"content_block\":{\"type\":\"tool_use\",\"id\":\"");
        jw_rawz(&s, id);
        jw_rawz(&s, "\",\"name\":\"");
        jw_rawz(&s, name);
        jw_rawz(&s, "\",\"input\":{}}}\n\nevent: content_block_delta\ndata: {\"type\":\"content_block_delta\","
                    "\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":");
        jw_strz(&s, input);
        jw_rawz(&s, "}}\n\n");
    } else {
        jw_rawz(&s, "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":0,"
                    "\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\nevent: content_block_delta\ndata: "
                    "{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":");
        jw_strz(&s, text);
        jw_rawz(&s, "}}\n\n");
    }
    jw_rawz(&s, "event: content_block_stop\ndata: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
                "event: message_delta\ndata: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"");
    jw_rawz(&s, name ? "tool_use" : "end_turn");
    jw_rawz(&s, "\",\"stop_sequence\":null},\"usage\":{\"output_tokens\":20}}\n\n"
                "event: message_stop\ndata: {\"type\":\"message_stop\"}\n\n");
    add_sse(s.p, s.n);
    jw_free(&s);
}

static void add_sse(const char *sse, long n)
{
    long i;
    jw w;
    static const char hex[] = "0123456789abcdef";
    jw_init(&w);
    jw_rawz(&w, "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream; charset=utf-8\r\n"
                "Transfer-Encoding: chunked\r\nrequest-id: req_test\r\n\r\n");
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
    jw_rawz(&w, "0\r\n\r\n");
    sb.resp[sb.nresp] = w.p;
    sb.rlen[sb.nresp++] = w.n;
}

static void add_raw(const char *http)
{
    long n = (long)strlen(http);
    sb.resp[sb.nresp] = (char *)malloc((size_t)n + 1);
    strcpy(sb.resp[sb.nresp], http);
    sb.rlen[sb.nresp++] = n;
}

static void stub_reset(void)
{
    int i;
    for (i = 0; i < sb.nresp; i++)
        free(sb.resp[i]);
    for (i = 0; i < sb.nreq; i++) {
        free(sb.head[i]);
        free(sb.body[i]);
    }
    jw_free(&sb.req);
    memset(&sb, 0, sizeof(sb));
    jw_init(&sb.req);
    sb.cur = -1;
    sb.chunk = 37;
}

/* ---- the console: a script of typed lines, the screen captured ---- */

typedef struct con {
    const char **lines;
    int next;
    jw screen;
    long slept[8];
    int nsleep;
    unsigned long clock;
} con;

static con cn;

static long c_read(void *u, char *buf, long cap)
{
    (void)u;
    if (!cn.lines[cn.next])
        return -1;
    cl_copy(buf, cn.lines[cn.next++], cap);
    return (long)strlen(buf);
}

static void c_write(void *u, const char *s, long n)
{
    (void)u;
    jw_raw(&cn.screen, s, n);
}

static int c_brk(void *u)
{
    int b = sb.brk;
    (void)u;
    sb.brk = 0;
    return b;
}

static int c_sleep(void *u, long ms)
{
    (void)u;
    if (cn.nsleep < 8)
        cn.slept[cn.nsleep++] = ms;
    return 0;
}

static unsigned long c_ms(void *u)
{
    (void)u;
    return cn.clock += 10;
}

/* ---- the sentinel on the renderer ---- */

typedef struct sentinel {
    cl_render inner;
    int text_calls, end_calls;
    long *full;                 /* the screen's layout count at each text call (0: no screen) */
    long fulls[16];
    jw text;
} sentinel;

static sentinel snt;

static void sn_text(void *u, const char *s, long n)
{
    (void)u;
    if (snt.full && snt.text_calls < 16)
        snt.fulls[snt.text_calls] = *snt.full;
    snt.text_calls++;
    jw_raw(&snt.text, s, n);
    snt.inner.text(snt.inner.u, s, n);
}

static void sn_end(void *u)
{
    (void)u;
    snt.end_calls++;
    snt.inner.end(snt.inner.u);
}

/* ---- the tree ---- */

static char dir[512];
static char top[512];           /* the temporary directory dir lies in (rm_tree removes it) */

static void mk_tree(void)
{
    const char *base = getenv("TMPDIR");
    char p[600];
    FILE *f;
    strcpy(top, base && *base ? base : "/tmp");
    if (top[strlen(top) - 1] == '/')
        top[strlen(top) - 1] = 0;
    strcat(top, "/claude_repl_XXXXXX");
    if (!mkdtemp(top))
        return;
    /* the start directory deep down, so that every test runs with a long
     * root: paths of 130+ characters overflowed fixed buffers (a settings
     * file built in a char[900], -r's transcript path cut at 127) only when
     * TMPDIR happened to be long */
    strcpy(dir, top);
    strcat(dir, "/a-start-directory-deeper-than-the-usual-temporary-one-eighty-characters-long-x");
    if (mkdir(dir, 0700))
        return;
    /* the user's directory (ENVARC:Claude) and T: inside the tree */
    strcpy(p, dir);
    strcat(p, "/home");
    mkdir(p, 0700);
    setenv("CLAUDE_CONFIG_DIR", p, 1);
    {
        /* the tree is a trusted workspace (A4 gaps 2): the trust question is
         * test_gaps2_hooks's, on a tree of its own */
        char real[600];
        strcat(p, "/claude.json");
        f = fopen(p, "wb");
        if (f && realpath(dir, real)) {
            fprintf(f, "{\"projects\":{\"%s\":{\"hasTrustDialogAccepted\":true}}}\n", real);
        }
        if (f)
            fclose(f);
    }
    strcpy(p, dir);
    strcat(p, "/t");
    mkdir(p, 0700);
    setenv("CLAUDE_CODE_TMPDIR", p, 1);
    /* the screen's background requests (A4 gaps 3: the prompt suggestion,
     * the away recap) would take the scripted answers: off, but in the
     * tests that drive them */
    setenv("CLAUDE_CODE_ENABLE_PROMPT_SUGGESTION", "false", 1);
    setenv("CLAUDE_CODE_ENABLE_AWAY_SUMMARY", "0", 1);
    strcpy(p, dir);
    strcat(p, "/S");
    mkdir(p, 0700);
    strcat(p, "/Startup-Sequence");
    f = fopen(p, "wb");
    if (f) {
        fputs("SetPatch QUIET\n", f);
        fclose(f);
    }
}

static void rm_tree(void)
{
    char cmd[600];
    strcpy(cmd, "rm -rf ");
    strcat(cmd, top);
    if (system(cmd))
        printf("  [ERROR] could not remove %s\n", top);
}

static cl_io io;
static cl_net net;
static sys_posix sp;
static cl_sys sys;

static const char *setup_key = "test-key-not-real";

static void setup_in(cl_repl *r, const char **script, const char *root)
{
    stub_reset();
    jw_free(&cn.screen);
    memset(&cn, 0, sizeof(cn));
    jw_init(&cn.screen);
    cn.lines = script;
    io.u = 0;
    io.read_line = c_read;
    io.write = c_write;
    io.brk = c_brk;
    io.sleep = c_sleep;
    io.ms = c_ms;
    io.log = 0;
    net.u = 0;
    net.open = s_open;
    net.send = s_send;
    net.recv = s_recv;
    net.close = s_close;
    net.err = s_err;
    sys_posix_init(&sp, &sys);
    CHECK_INT(repl_init(r, &io, &net, &sys, CL_DEFAULT_URL, setup_key, root), 0);
    jw_free(&snt.text);
    memset(&snt, 0, sizeof(snt));
    jw_init(&snt.text);
    snt.inner = r->render;
    r->render.u = 0;
    r->render.text = sn_text;
    r->render.end = sn_end;
}

static void setup(cl_repl *r, const char **script)
{
    setup_in(r, script, dir);
}

/* the "messages" array of a request body */
static int messages_of(const char *body, jv *m)
{
    jv b;
    return json_parse(body, (long)strlen(body), &b) == 0 && json_get(b, "messages", m) ? 0 : -1;
}

static const char text_content[] =
    "[{\"type\":\"thinking\",\"thinking\":\"\",\"signature\":\"EqQBCkgIBhABGAIiQL+sig/part1==+part2\"},"
    "{\"type\":\"text\",\"text\":\"Hello from the Amiga!\\n\\n```c\\nint x = \\\"q\\\";\\n```\\n"
    "Gr\xc3\xbc\xc3\x9f" "e \xe2\x80\x94 ok \xf0\x9f\x98\x80\"}]";

static void test_reach(void)
{
    static const char *script[] = { "hello", "show me S/Startup-Sequence", "/cost", "/exit", 0 };
    static cl_repl r;
    jv m, e, c, x, b;
    jit it;
    char s[200];
    setup(&r, script);
    add_stream("text.sse");
    add_stream("tool_use.sse");
    add_stream("tool_final.sse");
    repl_run(&r);

    /* the sentinel: the streamed text went through the renderer hook */
    CHECK_INT(snt.text_calls, 6);
    CHECK(snt.end_calls >= 3);
    CHECK_STR(snt.text.p ? snt.text.p : "",
              "Hello from the Amiga!\n\n```c\nint x = \"q\";\n```\nGr\xc3\xbc\xc3\x9f" "e \xe2\x80\x94 ok \xf0\x9f\x98\x80"
              "Let me look.Your Startup-Sequence runs SetPatch first.");
    /* three requests on one kept connection */
    CHECK_INT(sb.nreq, 3);
    CHECK_INT(sb.opens, 1);
    if (sb.nreq < 3)
        return;
    CHECK(strstr(sb.head[0], "POST /v1/messages HTTP/1.1\r\nHost: api.anthropic.com\r\n") == sb.head[0]);
    CHECK(strstr(sb.head[0], "x-api-key: test-key-not-real\r\n") != 0);
    CHECK(strstr(sb.head[0], "anthropic-beta: server-side-fallback-2026-07-01\r\n") != 0);

    /* request 1: the body's settings, the tools, the one message */
    CHECK_INT(json_parse(sb.body[0], (long)strlen(sb.body[0]), &b), 0);
    CHECK(json_get(b, "model", &x) && json_streq(x, "claude-opus-5-5"));
    CHECK(json_get(b, "max_tokens", &x) && json_long(x, 0) == 64000);
    CHECK(json_get(b, "stream", &x) && json_type(x) == J_TRUE);
    CHECK(json_get(b, "output_config", &x) && json_get(x, "effort", &e) && json_streq(e, "medium"));
    CHECK(json_get(b, "fallbacks", &x) && json_streq(x, "default"));
    /* the tools by Claude Code's names: no WebFetch without its connection, no
     * SlashCommand without a command (Skill: the bundled ones are always there);
     * the web_search server tool declared */
    CHECK(json_get(b, "tools", &x) && json_count(x) == 19);
    CHECK(strstr(sb.body[0], "- simplify: Review this session's changed code") != 0);
    CHECK(strstr(sb.body[0], "{\"name\":\"Read\",") != 0);
    CHECK(strstr(sb.body[0], "{\"name\":\"Task\",") != 0);
    CHECK(strstr(sb.body[0], "{\"name\":\"WebSearch\",") != 0 && strstr(sb.body[0], "web_search_2026") == 0);
    CHECK(strstr(sb.body[0], "\"name\":\"WebFetch\"") == 0);
    CHECK(json_get(b, "tool_choice", &x) && json_get(x, "type", &e) && json_streq(e, "auto"));
    CHECK(json_get(b, "system", &x));
    CHECK(strstr(sb.body[0], dir) != 0 || strstr(sb.body[0], "claude_repl_") != 0);
    CHECK_INT(messages_of(sb.body[0], &m), 0);
    CHECK_INT(json_count(m), 1);

    /* request 2: the first answer replayed exactly, thinking signature included */
    CHECK_INT(messages_of(sb.body[1], &m), 0);
    CHECK_INT(json_count(m), 3);
    json_iter(m, &it);
    json_next(&it, 0, &e);
    json_next(&it, 0, &e);
    CHECK(json_get(e, "role", &x) && json_streq(x, "assistant"));
    CHECK(json_get(e, "content", &c) && c.n == (long)strlen(text_content) &&
          !memcmp(c.p, text_content, (size_t)c.n));

    /* append-only: every request's history is a prefix of the next one's */
    {
        jv m1, m2, m3;
        messages_of(sb.body[0], &m1);
        messages_of(sb.body[1], &m2);
        messages_of(sb.body[2], &m3);
        CHECK(!memcmp(m1.p, m2.p, (size_t)m1.n - 1));
        CHECK(!memcmp(m2.p, m3.p, (size_t)m2.n - 1));
    }

    /* request 3: both tool calls answered in ONE user message, in order */
    CHECK_INT(messages_of(sb.body[2], &m), 0);
    CHECK_INT(json_count(m), 5);
    json_iter(m, &it);
    while (json_next(&it, 0, &e))
        ;
    CHECK(json_get(e, "role", &x) && json_streq(x, "user"));
    CHECK(json_get(e, "content", &c) && json_count(c) == 2);
    json_iter(c, &it);
    CHECK(json_next(&it, 0, &x));
    CHECK(json_get(x, "tool_use_id", &e) && json_streq(e, "toolu_01ReadStartup"));
    CHECK(json_get(x, "content", &e) && json_streq(e, "     1\tSetPatch QUIET\n"));
    CHECK(!json_get(x, "is_error", &e));
    CHECK(json_next(&it, 0, &x));
    CHECK(json_get(x, "tool_use_id", &e) && json_streq(e, "toolu_01ListS"));
    CHECK(json_get(x, "content", &e) && json_str(e, s, sizeof(s)) > 0 && strstr(s, "/S/Startup-Sequence\n") != 0);

    /* the screen: the tool calls shown, no question (reads inside the start
     * directory run without one, as in Claude Code), the cost */
    CHECK(strstr(cn.screen.p, "Tool \033[0mRead") != 0);
    CHECK(strstr(cn.screen.p, "Tool \033[0mGlob") != 0);
    CHECK(strstr(cn.screen.p, "Allow ") == 0);
    CHECK(strstr(cn.screen.p, "Requests 3. Tokens: input 855, output 150, cache write 1200, cache read 2700. Cost $") != 0);
    CHECK(strstr(cn.screen.p, "test-key-not-real") == 0);
    CHECK_INT(r.conv.n, 6);
    CHECK_INT(cn.next, 4);
    /* the cost: 855*4 + 150*20 + 1200*5 + 2700*0.2 = 3420 + 3000 + 6000 + 540 micro-dollars */
    CHECK_INT((long)r.conv.cost_micro, 12960L);
    conv_dollars(r.conv.cost_micro, s, sizeof(s));
    CHECK(strstr(cn.screen.p, s) != 0);
    repl_free(&r);
}

static void test_unfinished(void)
{
    static const char *script[] = { "one", "two", "three", "four", 0 };
    static cl_repl r;
    setup(&r, script);
    /* one: Ctrl+C in the middle of the answer */
    add_stream("text.sse");
    sb.brk_at = 900;
    repl_line(&r, cn.lines[cn.next++]);
    CHECK(strstr(cn.screen.p, "Stopped. The unfinished answer is not kept.") != 0);
    CHECK_INT(r.conv.n, 0);
    /* two: a refusal is shown, never kept as text */
    add_stream("refusal.sse");
    repl_line(&r, cn.lines[cn.next++]);
    CHECK(strstr(cn.screen.p, "Claude declined this request (cyber). Nothing was added") != 0);
    CHECK_INT(r.conv.n, 0);
    /* three: a tool call cut at max_tokens is never run */
    add_stream("max_tokens.sse");
    repl_line(&r, cn.lines[cn.next++]);
    CHECK(strstr(cn.screen.p, "nothing was run or kept") != 0);
    CHECK_INT(r.conv.n, 0);
    CHECK(strstr(cn.screen.p, "Tool ") == 0);
    /* four: overloaded before any answer, then a 529 with retry-after, then the answer */
    add_stream("overloaded.sse");
    add_raw("HTTP/1.1 529 Overloaded\r\nretry-after: 3\r\nContent-Type: application/json\r\nContent-Length: 75\r\n\r\n"
            "{\"type\":\"error\",\"error\":{\"type\":\"overloaded_error\",\"message\":\"Overloaded\"}}");
    add_stream("text.sse");
    repl_line(&r, cn.lines[cn.next++]);
    CHECK_INT(cn.nsleep, 2);
    CHECK_INT(cn.slept[0], 2000);
    CHECK_INT(cn.slept[1], 3000);
    CHECK(strstr(cn.screen.p, "The API refused the request (HTTP 529, overloaded_error): Overloaded") != 0);
    CHECK_INT(r.conv.n, 2);
    /* only "four" and its answer: the unfinished turns left nothing */
    CHECK(strstr(r.conv.m[0].json, "four") != 0);
    repl_free(&r);
}

static int has(const char *rel, const char *what);

/* the model /model saved in the user's settings taken out again (the home
 * is shared by the tests) */
static void unset_home_model(void)
{
    char p[600];
    strcpy(p, dir);
    strcat(p, "/home/settings.json");
    cfg_write_key(&sys, p, "model", 0);
}

static void test_commands(void)
{
    static const char *script[] = { 0 };
    static cl_repl r;
    setup(&r, script);
    CHECK_INT(repl_line(&r, "/model claude-sonnet-5-5"), 0);
    CHECK_STR(r.model, "claude-sonnet-5-5");
    CHECK(has("home/settings.json", "\"model\": \"claude-sonnet-5-5\""));   /* kept for new sessions */
    unset_home_model();
    CHECK_INT(repl_line(&r, "/effort huge"), 0);
    CHECK_STR(r.effort, "medium");
    CHECK_INT(repl_line(&r, "/effort high"), 0);
    CHECK_STR(r.effort, "high");
    CHECK_INT(repl_line(&r, "/frobnicate"), 0);
    CHECK(strstr(cn.screen.p, "Unknown command") != 0);
    CHECK_INT(repl_line(&r, "/exit"), 1);
    CHECK_INT(repl_line(&r, "   "), 0);
    CHECK_INT(sb.nreq, 0);
    repl_free(&r);
    /* no key for an https URL (A4 WP4): the start goes on, nothing is sent without one */
    {
        static cl_repl r2;
        CHECK_INT(repl_init(&r2, &io, &net, &sys, CL_DEFAULT_URL, "", dir), 0);
        CHECK_INT(repl_need_key(&r2), 1);
        repl_line(&r2, "hello");
        CHECK_INT(sb.nreq, 0);
        CHECK(strstr(cn.screen.p, "Not logged in: no API key") != 0);
        repl_free(&r2);
        CHECK_INT(repl_init(&r2, &io, &net, &sys, "http://10.0.2.2:8080/v1/messages", 0, dir), 0);
        repl_free(&r2);
    }
    /* a plain http URL never carries the key, even when one is set */
    {
        static const char *one[] = { "hi", 0 };
        static cl_repl r3;
        setup(&r3, one);
        repl_free(&r3);
        CHECK_INT(repl_init(&r3, &io, &net, &sys, "http://10.0.2.2:8080/v1/messages", "test-key-not-real", dir), 0);
        add_stream("text.sse");
        repl_line(&r3, "hi");
        CHECK_INT(sb.nreq, 1);
        CHECK(sb.nreq < 1 || strstr(sb.head[0], "x-api-key") == 0);
        CHECK(sb.nreq < 1 || strstr(sb.head[0], "Host: 10.0.2.2:8080\r\n") != 0);
        repl_free(&r3);
    }
    {
        char k1[] = "  sk-ant-test-0123456789\n";
        char k2[] = "sk ant";
        char k3[] = "\n";
        CHECK_INT(cl_key_clean(k1), 0);
        CHECK_STR(k1, "sk-ant-test-0123456789");
        CHECK_INT(cl_key_clean(k2), -1);
        CHECK_INT(cl_key_clean(k3), -1);
    }
}

/* ---- A3: the same program on its screen of its own ----
 *
 * The reachability test of ledger A3: repl_screen + repl_run, the code
 * C:Claude runs in an UP-Term window, driven by typed keys through the
 * engine (the window) and the recorded streams (the network): an answer in
 * Markdown, a read and a list with the permission menu answered "2", a
 * todo list and an edit with its diff asked with Enter, /cost, /exit. The
 * sentinel counts the renderer's calls; the screen module's own counters
 * say the transcript went through it and the footer was not repainted per
 * token. */

#define SB "\342\217\272"   /* the bullet */
#define SC "\342\216\277"   /* the result corner */

/* the edit's rows, looked at before /cost (whose /usage lines scroll them away) */
static int scr_update, scr_summary, scr_minus, scr_plus;

static void screen_before_cost(void)
{
    if (cs.next != 4)
        return;
    scr_update = cs_find("\342\217\272 Update(claude-test.txt)");
    scr_summary = cs_find(SC "  Updated claude-test.txt with 1 addition and 1 removal");
    scr_minus = cs_find("1 - hello");
    scr_plus = cs_find("1 + hello from the Amiga");
}

static void test_screen(void)
{
    static const char *keys[] = {
        "hello\r", "show me S/Startup-Sequence\r", "please edit the greeting\r", "\r", "/cost\r", "/exit\r", 0
    };
    static cl_repl r;
    char p[600];
    FILE *f;
    char *after = 0;
    long an = 0, full0;
    int row;
    stub_reset();
    add_stream("text.sse");
    add_stream("tool_use.sse");
    add_stream("tool_final.sse");
    add_stream("tool_edit.sse");
    add_stream("tool_final.sse");
    strcpy(p, dir);
    strcat(p, "/claude-test.txt");
    f = fopen(p, "wb");
    if (f) {
        fputs("hello\n", f);
        fclose(f);
    }
    cs_open(80, 24, keys);
    cs.before_read = screen_before_cost;
    scr_update = scr_summary = scr_minus = scr_plus = -1;
    cs_io(&io);
    io.log = 0;
    net.u = 0;
    net.open = s_open;
    net.send = s_send;
    net.recv = s_recv;
    net.close = s_close;
    net.err = s_err;
    sys_posix_init(&sp, &sys);
    CHECK_INT(repl_init(&r, &io, &net, &sys, CL_DEFAULT_URL, "test-key-not-real", dir), 0);
    CHECK_INT(repl_screen(&r), 0);
    CHECK_INT(cs.raw_on, 1);
    jw_free(&snt.text);
    memset(&snt, 0, sizeof(snt));
    jw_init(&snt.text);
    snt.inner = r.render;
    r.render.u = 0;
    r.render.text = sn_text;
    r.render.end = sn_end;
    full0 = r.tui->n_full;
    snt.full = &r.tui->n_full;
    repl_run(&r);
    if (getenv("CL_DUMP")) {
        int k;
        for (k = 0; k < cs.rows; k++)
            printf("%2d|%s\n", k, cs_row(k));
    }

    /* the sentinel: every streamed text went through the renderer, and the
     * screen's renderer drew it (blocks with the bullet) */
    CHECK_INT(snt.text_calls, 8);
    CHECK(r.show->n_text == 8);
    CHECK(r.show->n_blocks >= 4);
    CHECK_INT(r.show->n_tools, 4);          /* Read, Glob, Read, Edit (TodoWrite has no header call) */
    CHECK_INT(r.show->n_diffs, 1);
    CHECK(r.tui->n_lines > 20);
    /* five requests, every key used, the edit made */
    CHECK_INT(sb.nreq, 5);
    CHECK_INT(cs.next, 6);
    CHECK_INT(sys.read(sys.u, p, 1000, &after, &an), 0);
    CHECK_STR(after ? after : "", "hello from the Amiga\n");
    free(after);
    /* the reads ran without a menu (Claude Code's default inside the start directory) */
    CHECK(sb.nreq < 3 || strstr(sb.body[2], "/S/Startup-Sequence\\n") != 0);
    CHECK(!(r.tools.perm.session & (1ul << T_GLOB)));
    /* the todo list and the edit's result reached the history and the screen */
    CHECK(sb.nreq < 5 || strstr(sb.body[4], "Todos have been modified") != 0);
    CHECK(sb.nreq < 5 || strstr(sb.body[4], "claude-test.txt has been updated.") != 0);
    /* the todo list, in progress (it has scrolled into the scrollback by now) */
    CHECK(strstr(cs.sent.p, "\342\226\240\033[0m \033[1mChange the greeting") != 0);
    cs.before_read = 0;
    row = scr_update;
    CHECK(row >= 0);
    CHECK(scr_summary > row);
    CHECK(scr_minus > row);
    CHECK(scr_plus > row);
    CHECK(cs_find("> please edit the greeting") >= 0 || vt_scrollback_lines(cs.vt) > 0);
    CHECK(cs_find("Requests 5. Tokens:") >= 0);
    /* the footer is still the box and the status line */
    CHECK(strstr(cs_row(20), "\342\225\255") != 0);
    CHECK(strstr(cs_row(23), "ctx: 100% left") != 0);
    /* economy: the footer was laid out again only when its shape changed
     * (busy on/off, a menu open/closed), never per token; no screen clear */
    CHECK(r.tui->n_full - full0 <= 20);
    /* the first answer streamed in four pieces: the footer kept its shape */
    CHECK(snt.fulls[0] > full0);
    CHECK_INT(snt.fulls[3], snt.fulls[0]);
    /* one clear only: at the start (W37), none while answers stream */
    CHECK(strstr(cs.sent.p, "\033[2J") != 0 && strstr(strstr(cs.sent.p, "\033[2J") + 4, "\033[2J") == 0);
    CHECK(strstr(cs.sent.p, "test-key-not-real") == 0);
    repl_free(&r);
    /* raw mode off, the scroll region and the modes reset */
    CHECK_INT(cs.raw_on, 0);
    CHECK(strstr(cs.sent.p + cs.sent.n - 80, "\033[r") != 0);
    CHECK(strstr(cs.sent.p + cs.sent.n - 80, "\033[?2004l") != 0);
    cs_close();
}

/* ---- A4 WP3: the project's own settings, memory, commands and hooks ----
 *
 * The reachability test of WP3: the REPL core with a project that has
 * CLAUDE.md (importing notes.md) and the user's CLAUDE.md, a settings
 * file that allows Read and one Edit and hooks list_dir away (exit 2), and
 * a custom command. Typed: the command, a read-and-list prompt, an edit
 * prompt, /todos, /rewind of the edit, /exit -- with no permission answer
 * in the script at all. Then a new REPL continues the saved session. */

static int has(const char *rel, const char *what);

static void wput(const char *rel, const char *text)
{
    char p[700];
    FILE *f;
    strcpy(p, dir);
    strcat(p, "/wp3/");
    strcat(p, rel);
    f = fopen(p, "wb");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static void test_wp3(void)
{
    static const char *script[] = { "/greet Amiga", "show me S/Startup-Sequence", "please edit the greeting",
                                    "/todos", "/rewind 1 code", "/exit", 0 };
    static cl_repl r, r2;
    char root[600], p[700], hook[1400], user_md[700];
    char *after = 0;
    long an = 0;
    jv b, x, m, e, c;
    jit it;
    int i, found = 0;
    FILE *f;
    strcpy(root, dir);
    strcat(root, "/wp3");
    mkdir(root, 0700);
    strcpy(p, root);
    strcat(p, "/S");
    mkdir(p, 0700);
    strcpy(p, root);
    strcat(p, "/.claude");
    mkdir(p, 0700);
    strcat(p, "/commands");
    mkdir(p, 0700);
    wput("S/Startup-Sequence", "SetPatch QUIET\n");
    wput("claude-test.txt", "hello\n");
    wput("CLAUDE.md", "PROJECT-MEMORY-SENTINEL: build with smake. See @notes.md\n");
    wput("notes.md", "IMPORTED-SENTINEL\n");
    wput(".claude/commands/greet.md", "---\ndescription: Greet someone\n---\nSay hello to $ARGUMENTS.\n");
    wput("nolist.sh", "cat >/dev/null\necho 'listing is not allowed here'\nexit 2\n");
    strcpy(hook, "{\"permissions\":{\"allow\":[\"Read\",\"Edit(claude-test.txt)\"]},"
                 "\"hooks\":{\"PreToolUse\":[{\"matcher\":\"Glob\",\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    strcat(hook, root);
    strcat(hook, "/nolist.sh\"}]}]}}\n");
    wput(".claude/settings.json", hook);
    strcpy(user_md, dir);
    strcat(user_md, "/home/CLAUDE.md");
    f = fopen(user_md, "wb");
    if (f) {
        fputs("USER-MEMORY-SENTINEL\n", f);
        fclose(f);
    }

    setup_in(&r, script, root);
    add_stream("text.sse");
    add_stream("tool_use.sse");
    add_stream("tool_final.sse");
    add_stream("tool_edit.sse");
    add_stream("tool_final.sse");
    repl_run(&r);
    CHECK_INT(sb.nreq, 5);
    CHECK_INT(cn.next, 6);
    if (sb.nreq < 5)
        return;

    /* CLAUDE.md (user, project, its import) reached the request body */
    CHECK_INT(json_parse(sb.body[0], (long)strlen(sb.body[0]), &b), 0);
    CHECK(json_get(b, "system", &x));
    CHECK(strstr(sb.body[0], "USER-MEMORY-SENTINEL") != 0);
    CHECK(strstr(sb.body[0], "PROJECT-MEMORY-SENTINEL") != 0);
    CHECK(strstr(sb.body[0], "IMPORTED-SENTINEL") != 0);
    CHECK(strstr(sb.body[0], "USER-MEMORY-SENTINEL") < strstr(sb.body[0], "PROJECT-MEMORY-SENTINEL"));
    /* ... in the system prompt, ahead of its cache breakpoint, the same every request */
    CHECK(strstr(sb.body[0], "\"cache_control\":{\"type\":\"ephemeral\"}}],\"tools\"") != 0);
    {
        const char *s0 = strstr(sb.body[0], "\"system\":"), *s4 = strstr(sb.body[4], "\"system\":");
        const char *e0 = s0 ? strstr(s0, "\"tools\"") : 0;
        CHECK(s0 && s4 && e0 && !strncmp(s0, s4, (size_t)(e0 - s0)));
    }

    /* the custom command expanded into the first prompt */
    CHECK_INT(r.n_cmds_run, 1);
    CHECK_INT(messages_of(sb.body[0], &m), 0);
    CHECK(strstr(sb.body[0], "Say hello to Amiga.") != 0);

    /* the permission rules answered: no question was asked at all */
    CHECK(strstr(cn.screen.p, "Always this session") == 0);
    CHECK(strstr(cn.screen.p, "Allow?") == 0);
    CHECK_INT(r.n_rule_allow, 1);               /* Edit(claude-test.txt); the reads never ask */

    /* the hook blocked Glob: its reason went to Claude as the result */
    CHECK_INT(r.hooks.n_run, 1);
    CHECK_INT(messages_of(sb.body[2], &m), 0);
    json_iter(m, &it);
    while (json_next(&it, 0, &e))
        ;
    CHECK(json_get(e, "content", &c));
    json_iter(c, &it);
    while (json_next(&it, 0, &x)) {
        jv id, ct;
        if (json_get(x, "tool_use_id", &id) && json_streq(id, "toolu_01ListS")) {
            found++;
            CHECK(json_get(x, "is_error", &ct) && json_type(ct) == J_TRUE);
            CHECK(json_get(x, "content", &ct) &&
                  json_streq(ct, "PreToolUse hook blocked this call: listing is not allowed here"));
        }
        if (json_get(x, "tool_use_id", &id) && json_streq(id, "toolu_01ReadStartup")) {
            found++;
            CHECK(json_get(x, "content", &ct) && json_streq(ct, "     1\tSetPatch QUIET\n")); /* Read numbers lines (A4 WP2) */
        }
    }
    CHECK_INT(found, 2);

    /* the edit ran (its result reached Claude), /todos showed the list,
     * /rewind put the file back from its checkpoint */
    CHECK(strstr(sb.body[4], " has been updated.") != 0); /* Claude Code's Edit result (A4 WP2) */
    CHECK(strstr(cn.screen.p, "[>] Change the greeting") != 0);
    CHECK(strstr(cn.screen.p, "[x] Read the file") != 0);
    CHECK(strstr(cn.screen.p, "Files put back: 1.") != 0);
    CHECK_INT(r.cp.n_snaps, 1);
    strcpy(p, root);
    strcat(p, "/claude-test.txt");
    CHECK_INT(sys.read(sys.u, p, 1000, &after, &an), 0);
    CHECK_STR(after ? after : "", "hello\n");
    free(after);

    /* the session: one append per completed turn, never per key */
    CHECK_INT(r.sess.n_appends, 3);
    CHECK_INT(sys.kind(sys.u, r.sess.file), 1);
    CHECK_INT(r.conv.n, 10);
    {
        /* the history as it stood, to compare with the resumed one */
        jw was;
        jw_init(&was);
        conv_messages(&r.conv, &was);
        repl_free(&r);

        /* a new start continues it, every message byte for byte */
        setup_in(&r2, script, root);
        CHECK_INT(repl_continue(&r2), 0);
        CHECK_INT(r2.conv.n, 10);
        {
            jw now;
            jw_init(&now);
            conv_messages(&r2.conv, &now);
            CHECK(was.p && now.p && was.n == now.n && !memcmp(was.p, now.p, (size_t)was.n));
            jw_free(&now);
        }
        CHECK(strstr(cn.screen.p, "Resumed a conversation of 10 messages.") != 0);
        for (i = 0; i < r2.conv.n; i++)
            CHECK(r2.conv.m[i].n > 2);
        jw_free(&was);
    }
    repl_free(&r2);

    /* the screen's side (WP1's Esc Esc menu, /theme, # memory) through the
     * cl_ui fields WP3 fills: a rewind point that can restore the code */
    {
        static const char *one[] = { 0 };
        static cl_repl r3;
        cl_memfile mf[6];
        char lab[100];
        int k;
        setup_in(&r3, one, root);
        add_stream("tool_edit.sse");
        add_stream("tool_final.sse");
        repl_line(&r3, "please edit the greeting");
        CHECK(r3.ctx_used > 0);
        k = r3.ui.rw.count(r3.ui.rw.u);
        CHECK_INT(k, 1);
        CHECK_INT(r3.ui.rw.can(r3.ui.rw.u, 0), RW_CONV | RW_CODE);
        CHECK_INT(r3.ui.rw.label(r3.ui.rw.u, 0, lab, sizeof(lab)), 0);
        CHECK_STR(lab, "please edit the greeting");
        CHECK_INT(r3.ui.rw.restore(r3.ui.rw.u, 0, RW_CODE | RW_CONV), 0);
        CHECK_INT(r3.conv.n, 0);
        CHECK_INT(r3.ctx_used, 0);      /* the context in use goes back with the conversation */
        strcpy(p, root);
        strcat(p, "/claude-test.txt");
        after = 0;
        CHECK_INT(sys.read(sys.u, p, 1000, &after, &an), 0);
        CHECK_STR(after ? after : "", "hello\n");
        free(after);
        r3.ui.set_setting(r3.ui.su, "theme", "light");
        CHECK_STR(r3.ui.setting(r3.ui.su, "theme"), "light");
        CHECK(has("home/settings.json", "\"theme\": \"light\""));
        CHECK_INT(r3.ui.memory_files(r3.ui.mu, mf, 6), 3);
        CHECK(strstr(mf[0].path, "/wp3/CLAUDE.md") != 0);
        wput("CLAUDE.md", "CHANGED-MEMORY\n");
        r3.ui.memory_changed(r3.ui.mu, mf[0].path);
        CHECK(strstr(r3.system, "CHANGED-MEMORY") != 0);
        repl_free(&r3);
    }
    remove(user_md);
}

/* the commands of WP3, each through repl_line, in the line mode (no
 * picker: they list instead), and auto-compact, a prompt hook, the
 * fallback model */
static int has(const char *rel, const char *what)
{
    char p[700], *b = 0;
    long n = 0;
    int ok;
    strcpy(p, dir);
    strcat(p, "/");
    strcat(p, rel);
    if (sys.read(sys.u, p, 1L << 20, &b, &n))
        return 0;
    ok = strstr(b, what) != 0;
    free(b);
    return ok;
}

static int edits;

/* the editor: appends a line */
static int ed_stub(void *u, const char *path)
{
    FILE *f = fopen(path, "ab");
    (void)u;
    edits++;
    if (!f)
        return -1;
    fputs("EDITED-IN-EDITOR\n", f);
    fclose(f);
    return 0;
}

static void test_wp3_commands(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    char root[600], p[700], id0[16], hook[1200];
    FILE *f;
    int i;
    strcpy(root, dir);
    strcat(root, "/wp3b");
    mkdir(root, 0700);
    strcpy(p, root);
    strcat(p, "/lib");
    mkdir(p, 0700);
    strcpy(p, root);
    strcat(p, "/.claude");
    mkdir(p, 0700);
    strcpy(p, root);
    strcat(p, "/ctx.sh");
    f = fopen(p, "wb");
    if (f) {
        fputs("cat >/dev/null\necho HOOK-CONTEXT\n", f);
        fclose(f);
    }
    strcpy(p, root);
    strcat(p, "/status.sh");
    f = fopen(p, "wb");
    if (f) {
        fputs("grep -q '\"current_dir\"' && echo STATUS-LINE-OK\n", f);
        fclose(f);
    }
    strcpy(hook, "{\"hooks\":{\"UserPromptSubmit\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    strcat(hook, root);
    strcat(hook, "/ctx.sh\"}]}]}}");
    strcpy(p, root);
    strcat(p, "/.claude/settings.json");
    f = fopen(p, "wb");
    if (f) {
        fputs(hook, f);
        fclose(f);
    }
    setup_in(&r, none, root);

    /* 3.3 a rule written to the local settings, listed */
    repl_line(&r, "/permissions allow Bash(make *)");
    CHECK(has("wp3b/.claude/settings.local.json", "\"allow\": [\"Bash(make *)\"]"));
    CHECK(r.cfg.nrules == 1 && r.cfg.rules[0].kind == RULE_ALLOW);
    repl_line(&r, "/permissions");
    CHECK(strstr(cn.screen.p, "allow  Bash(make *)  (local)") != 0);
    repl_line(&r, "/permissions remove Bash(make *)");
    CHECK_INT(r.cfg.nrules, 0);
    CHECK(!has("wp3b/.claude/settings.local.json", "make"));
    /* 3.5 an output style: into the system prompt, kept for the project */
    repl_line(&r, "/output-style Explanatory");
    CHECK(strstr(r.system, "# Output style: Explanatory") != 0);
    CHECK(has("wp3b/.claude/settings.local.json", "\"outputStyle\": \"Explanatory\""));
    repl_line(&r, "/output-style");
    CHECK(strstr(cn.screen.p, "* Explanatory") != 0);
    /* 3.10 /config, /autocompact */
    repl_line(&r, "/config effortLevel high");
    CHECK_STR(r.effort, "high");
    CHECK(has("home/settings.json", "\"effortLevel\": \"high\""));
    repl_line(&r, "/config model haiku project");
    CHECK_STR(r.model, "claude-haiku-4-5-20251001");
    CHECK(has("wp3b/.claude/settings.json", "\"model\": \"haiku\""));
    CHECK(has("wp3b/.claude/settings.json", "UserPromptSubmit"));      /* the rest kept */
    repl_line(&r, "/model opus");
    CHECK_STR(r.model, "claude-opus-5-5");
    CHECK(has("home/settings.json", "\"model\": \"opus\""));
    unset_home_model();
    repl_line(&r, "/autocompact off");
    CHECK_INT(r.auto_compact, 0);
    repl_line(&r, "/autocompact on");
    CHECK_INT(r.auto_compact, 1);
    /* 3.7 a name */
    repl_line(&r, "/rename Amiga work");
    CHECK_STR(r.sess.title, "Amiga work");
    cl_copy(id0, r.sess.id, sizeof(id0));

    /* 3.6 + 3.8: the prompt hook's context goes with the prompt; the answer
     * fills the window past the threshold: compacted by itself */
    add_stream("full.sse");
    add_stream("text.sse");
    repl_line(&r, "hello");
    CHECK_INT(sb.nreq, 2);
    CHECK(sb.nreq >= 1 && strstr(sb.body[0], "HOOK-CONTEXT") != 0);
    CHECK(sb.nreq >= 2 && strstr(sb.body[1], "Summarise this conversation") != 0);
    CHECK(sb.nreq >= 2 && strstr(sb.body[1], "\"tool_choice\":{\"type\":\"none\"}") != 0);
    CHECK(strstr(cn.screen.p, "The context window is nearly full: compacting") != 0);
    CHECK(strstr(cn.screen.p, "Compacted.") != 0);
    CHECK_INT(r.conv.n, 1);
    CHECK(strstr(r.conv.m[0].json, "compacted") != 0);

    /* 3.10 /export to a file and to the clipboard */
    repl_line(&r, "/export out.txt");
    CHECK(has("wp3b/out.txt", "> This session continues an earlier conversation"));
    repl_line(&r, "/export");
    CHECK(strstr(sp.clip, "This session continues") != 0);
    /* 3.7 /branch, then /resume by name goes back to the first */
    repl_line(&r, "/branch");
    CHECK(strcmp(r.sess.id, id0) != 0);
    CHECK_STR(r.sess.title, "Amiga work (branch)");
    CHECK_INT(sys.kind(sys.u, r.sess.file), 1);
    repl_line(&r, "/resume Amiga work");
    CHECK_STR(r.sess.id, id0);
    CHECK_INT(r.conv.n, 1);
    repl_line(&r, "/resume");
    CHECK(strstr(cn.screen.p, "Amiga work (branch)  [") != 0);
    /* 3.10 the rest */
    repl_line(&r, "/status");
    CHECK(strstr(cn.screen.p, "Output style: Explanatory") != 0);
    CHECK(strstr(cn.screen.p, "Session: ") != 0);
    repl_line(&r, "/login sk-ant-test-12345678");
    CHECK(has("home/key", "sk-ant-test-12345678"));
    CHECK(r.key && !strcmp(r.key, "sk-ant-test-12345678"));
    repl_line(&r, "/logout");
    CHECK(!has("home/key", "sk"));
    CHECK(r.key == 0);
    repl_line(&r, "/login");
    CHECK_INT(r.await_key, 1);
    repl_line(&r, "sk-ant-typed-87654321");
    CHECK(r.key && !strcmp(r.key, "sk-ant-typed-87654321"));
    CHECK_INT(sb.nreq, 2);                     /* the key line was never sent */
    repl_line(&r, "/add-dir lib");
    CHECK_INT(r.cfg.ndirs, 1);
    repl_line(&r, "/hooks");
    CHECK(strstr(cn.screen.p, "UserPromptSubmit  sh ") != 0);
    repl_line(&r, "/commands");
    CHECK(strstr(cn.screen.p, "No custom commands") != 0);
    repl_line(&r, "/agents");
    repl_line(&r, "/skills");
    CHECK(strstr(cn.screen.p, "  simplify (built-in)  Review this session's changed code") != 0);
    repl_line(&r, "/doctor");
    CHECK(strstr(cn.screen.p, "[OK]    System: a POSIX host") != 0);
    CHECK(strstr(cn.screen.p, "[OK]    Settings") != 0);
    repl_line(&r, "/terminal-setup");
    CHECK(strstr(cn.screen.p, "Raw keys") != 0);
    repl_line(&r, "/usage");
    CHECK(strstr(cn.screen.p, "Context ") != 0);
    repl_line(&r, "/tasks");
    CHECK(strstr(cn.screen.p, "No commands running in the background.") != 0);
    repl_line(&r, "/todos");
    CHECK(strstr(cn.screen.p, "No todo list") != 0);
    strcpy(p, "/statusline command sh ");
    strcat(p, root);
    strcat(p, "/status.sh");
    repl_line(&r, p);
    CHECK_STR(r.status_text, "STATUS-LINE-OK");
    repl_line(&r, "/memory");
    CHECK(strstr(cn.screen.p, "/memory user, /memory project or /memory local") != 0);
    /* /memory local: made, edited in the editor (WP1's io->edit), read again */
    io.edit = ed_stub;
    repl_line(&r, "/memory local");
    io.edit = 0;
    CHECK_INT(edits, 1);
    CHECK(has("wp3b/CLAUDE.local.md", "EDITED-IN-EDITOR"));
    CHECK(strstr(r.system, "EDITED-IN-EDITOR") != 0);
    repl_line(&r, "/help");
    for (i = 0; i < slash_nbuiltin; i++)
        CHECK(strstr(cn.screen.p, slash_builtin[i].name) != 0);
    /* a name as long as the column keeps a gap before its help (the rig showed
     * "/run-skill-generatorWrite a project skill ...") */
    CHECK(strstr(cn.screen.p, "/run-skill-generatorWrite") == 0);
    CHECK(strstr(cn.screen.p, "/run-skill-generator  Write") != 0);
    repl_line(&r, "/cd lib");
    CHECK(strstr(r.tools.root, "/wp3b/lib") != 0);
    CHECK(strstr(r.system, "/wp3b/lib") != 0);
    repl_free(&r);

    /* 3.11 the fallback model: overloaded five times, the turn goes on with it */
    setup_in(&r, none, root);
    cl_copy(r.fallback, cfg_model("sonnet"), sizeof(r.fallback));
    for (i = 0; i < 5; i++)
        add_raw("HTTP/1.1 529 Overloaded\r\nContent-Type: application/json\r\nContent-Length: 75\r\n\r\n"
                "{\"type\":\"error\",\"error\":{\"type\":\"overloaded_error\",\"message\":\"Overloaded\"}}");
    add_stream("text.sse");
    repl_line(&r, "hi");
    CHECK_INT(sb.nreq, 6);
    CHECK_STR(r.model, "claude-sonnet-5-5");
    CHECK(sb.nreq == 6 && strstr(sb.body[5], "\"model\":\"claude-sonnet-5-5\"") != 0);
    CHECK(strstr(cn.screen.p, "going on with the fallback model claude-sonnet-5-5") != 0);
    CHECK_INT(r.conv.n, 2);
    repl_free(&r);
}

/* ---- A4 WP1: the prompt's prefixes and keys through the whole program ----
 *
 * repl_screen + repl_run, keys typed into the engine: "! echo hi" runs the
 * command and Claude gets its output; "# remember the milk" goes into the
 * project's CLAUDE.md through the "where" menu; "@S/Startup-Sequence"
 * attaches the file; a prompt typed while a tool round runs goes into the
 * same turn with the tool results; Ctrl+O opens the transcript viewer and
 * q closes it. The history file keeps the prompts. Sentinels: the request
 * bodies, the files on disk, the viewer's open count. */
static void test_wp1(void)
{
    static const char *keys[] = {
        "!!echo hi\r",                                  /* ! bash mode */
        "#remember the milk\r", "\r",                   /* # memory, the project's file */
        "explain @S/Startup-Sequence\r",                /* @ mention */
        "show me S/Startup-Sequence\r", "!also say hi\r",        /* typed ahead during the tool round */
        "\017", "q",                                    /* Ctrl+O, q */
        "/exit\r", 0
    };
    static cl_repl r;
    char p[600], hp[600];
    char *b = 0;
    long n = 0;
    stub_reset();
    add_stream("text.sse");
    add_stream("text.sse");
    add_stream("tool_use.sse");
    add_stream("tool_final.sse");
    strcpy(hp, dir);
    strcat(hp, "/history");
    cs_open(80, 24, keys);
    cs_io(&io);
    io.log = 0;
    net.u = 0;
    net.open = s_open;
    net.send = s_send;
    net.recv = s_recv;
    net.close = s_close;
    net.err = s_err;
    sys_posix_init(&sp, &sys);
    CHECK_INT(repl_init(&r, &io, &net, &sys, CL_DEFAULT_URL, "test-key-not-real", dir), 0);
    cl_copy(r.ui.histfile, hp, sizeof(r.ui.histfile));
    CHECK_INT(repl_screen(&r), 0);
    repl_run(&r);
    if (getenv("CL_DUMP")) {
        int k;
        for (k = 0; k < cs.rows; k++)
            printf("%2d|%s\n", k, cs_row(k));
    }
    CHECK_INT(cs.next, 9);                  /* every key used */
    CHECK_INT(sb.nreq, 4);
    /* !: the command ran, Claude got its output as Claude Code sends it */
    CHECK(sb.nreq < 1 || strstr(sb.body[0], "<bash-input>echo hi</bash-input>\\n<bash-stdout>hi\\n</bash-stdout>") != 0);
    CHECK(cs_find("! echo hi") >= 0 || vt_scrollback_lines(cs.vt) > 0);
    /* #: appended to the project's memory file */
    strcpy(p, dir);
    strcat(p, "/CLAUDE.md");
    CHECK_INT(sys.read(sys.u, p, 1000, &b, &n), 0);
    CHECK_STR(b ? b : "", "- remember the milk\n");
    free(b);
    b = 0;
    /* @: the file's text went with the prompt */
    CHECK(sb.nreq < 2 ||
          strstr(sb.body[1], "explain @S/Startup-Sequence\\n\\n<file path=\\\"S/Startup-Sequence\\\">\\nSetPatch QUIET\\n</file>") != 0);
    /* type-ahead: in the same turn, after the tool results */
    CHECK(sb.nreq < 4 || strstr(sb.body[3], "{\"type\":\"text\",\"text\":\"also say hi\"}]}]") != 0);
    CHECK(sb.nreq < 3 || strstr(sb.body[2], "also say hi") == 0);
    /* Ctrl+O: the viewer opened once and closed */
    CHECK_INT(r.tui->n_views, 1);
    CHECK(strstr(cs.sent.p, "\033[?1049h") != 0 && strstr(cs.sent.p, "\033[?1049l") != 0);
    /* the history file has the prompts, as typed */
    CHECK_INT(sys.read(sys.u, hp, 10000, &b, &n), 0);
    CHECK(b && strstr(b, "{\"display\":\"!echo hi\",") != 0);
    CHECK(b && strstr(b, "{\"display\":\"#remember the milk\",") != 0);
    CHECK(b && strstr(b, "{\"display\":\"also say hi\",") != 0);
    free(b);
    repl_free(&r);
    cs_close();
}

/* ---- A4 input rest: the keys WP1 left out, through the REPL core ----
 *
 * The reachability test: repl_run on the screen. Claude's Bash runs in
 * the foreground as a shells.c job the tool waits on through the screen
 * (tools.wait -> ui_wait -> tui_wait); Ctrl+B typed while it runs leaves
 * it running as bash_1 and Claude gets Claude Code's answer for that.
 * A ! command goes the same way (ui.tools): bash_2. Alt+P opens the model
 * picker without the line echoed as typed. The sentinel: the result texts
 * in the requests, which only the moved-to-background path writes. */

static cl_repl *ir_r;
static const char **ir_keys;
static const char ir_ctrl_b[] = "(ctrl+b once a command runs)";

/* Ctrl+B "typed" only once a command waits in the foreground */
static int ir_flips;
static void ir_look(void)
{
    /* the first while Claude's Bash runs (one request out), the second
     * while the ! command does (after the turn's second request) */
    if (ir_keys[cs.next] == ir_ctrl_b && ir_r->tui && ir_r->tui->bgable && sb.nreq == (ir_flips ? 2 : 1)) {
        ir_keys[cs.next] = "!\002";
        ir_flips++;
    }
}

static void test_input_rest(void)
{
    static const char *keys[] = {
        "run the slow one\r", "1", ir_ctrl_b,   /* Bash asks, runs; Ctrl+B */
        "!!sleep 20\r", ir_ctrl_b,              /* a ! command; Ctrl+B */
        "\033p", "\033",                        /* Alt+P, the picker left */
        "/exit\r", 0
    };
    static cl_repl r;
    stub_reset();
    add_stream("tool_fg.sse");
    add_stream("tool_final.sse");
    add_stream("text.sse");
    cs_open(80, 24, keys);
    cs_io(&io);
    io.log = 0;
    net.u = 0;
    net.open = s_open;
    net.send = s_send;
    net.recv = s_recv;
    net.close = s_close;
    net.err = s_err;
    sys_posix_init(&sp, &sys);
    CHECK_INT(repl_init(&r, &io, &net, &sys, CL_DEFAULT_URL, "test-key-not-real", dir), 0);
    CHECK_INT(repl_screen(&r), 0);
    ir_r = &r;
    ir_keys = keys;
    ir_flips = 0;
    cs.before_read = ir_look;
    repl_run(&r);
    cs.before_read = 0;
    if (getenv("CL_DUMP")) {
        int k;
        for (k = 0; k < cs.rows; k++)
            printf("%2d|%s\n", k, cs_row(k));
    }
    CHECK_INT(cs.next, 8);                  /* every key used */
    CHECK_INT(sb.nreq, 3);
    /* Ctrl+B: the Bash call answered at once, the command left running */
    CHECK(sb.nreq < 2 ||
          strstr(sb.body[1], "\"tool_use_id\":\"toolu_01Foreground\",\"content\":\"Command was manually "
                             "backgrounded by user with ID: bash_1.") != 0);
    /* ... the same move as at the time limit (shells.c fg_move): a task Claude hears of when it ends */
    CHECK(sb.nreq < 2 || strstr(sb.body[1], "You are told when it ends; stop it with TaskStop.") != 0);
    /* the ! command: moved the same way, Claude told its id */
    CHECK(sb.nreq < 3 || strstr(sb.body[2], "<bash-input>sleep 20</bash-input>\\n<bash-stdout>The command was "
                                            "moved to the background with ID: bash_2") != 0);
    CHECK(strstr(cs.sent.p, "Moved to the background as bash_2.") != 0);
    /* Alt+P: the picker came, no "/model" line in the transcript */
    CHECK(strstr(cs.sent.p, "Select a model") != 0);
    CHECK(strstr(cs.sent.p, "/model") == 0);
    repl_free(&r);                          /* the two shells stopped */
    cs_close();
}

/* ---- A4 WP2: the new tools through the REPL core ----
 *
 * The reachability test of the tools at Claude Code parity: repl_line,
 * the code C:Claude runs, over the recorded streams: a web search (the
 * server tool's blocks shown and kept in the history), a Task subagent
 * (its two requests nested inside the turn, through repl.c's api_send:
 * the turn's own stream kept aside, the subagent's Grep asked and run,
 * its report the Task's result) and a WebFetch (the page from a stub web,
 * the small model's answer the result). The sentinel counts the renderer:
 * the subagent's and the small model's text never reach the screen. */

static const char wp2_page[] =
    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 47\r\n\r\n"
    "<html><body><h1>Hi</h1><p>hello</p></body></html>";
static long wp2_pos;
static int wp2_open_n;
static int wp2_tls;             /* the last open's TLS flag (WebFetch's http -> https) */
static char wp2_req[600];

static int wp_open(void *u, const char *host, int port, int tls)
{
    (void)u;
    (void)host;
    (void)port;
    wp2_tls = tls;
    wp2_open_n++;
    wp2_pos = 0;
    return 0;
}

static long wp_send(void *u, const char *b, long n)
{
    (void)u;
    if ((long)strlen(wp2_req) + n < (long)sizeof(wp2_req))
        strncat(wp2_req, b, (size_t)n);
    return n;
}

static long wp_recv(void *u, char *b, long cap, int timeout_ms)
{
    long n = (long)sizeof(wp2_page) - 1 - wp2_pos;
    (void)u;
    (void)timeout_ms;
    if (n > cap)
        n = cap;
    memcpy(b, wp2_page + wp2_pos, (size_t)n);
    wp2_pos += n;
    return n;
}

static void wp_close(void *u)
{
    (void)u;
}

static void test_wp2(void)
{
    static const char *script[] = { "search the web for accelerators", "send an agent", "a", "fetch the page", "y",
                                    "/exit", 0 };
    static cl_repl r;
    cl_net web;
    jv m, e, x, c;
    jit it;
    int i;
    setup(&r, script);
    web.u = 0;
    web.open = wp_open;
    web.send = wp_send;
    web.recv = wp_recv;
    web.close = wp_close;
    web.err = s_err;
    r.tools.web = &web;
    free(r.tools.json);         /* WebFetch is declared now that it has its connection */
    r.tools.json = 0;
    add_stream("websearch.sse");
    add_stream("tool_task.sse");
    add_stream("agent_tool.sse");
    add_stream("agent_final.sse");
    add_stream("tool_final.sse");
    add_stream("tool_fetch.sse");
    add_stream("fetch_answer.sse");
    add_stream("tool_final.sse");
    repl_run(&r);
    CHECK_INT(sb.nreq, 8);
    CHECK_INT(cn.next, 6);
    if (sb.nreq < 8) {
        repl_free(&r);
        return;
    }
    /* the web search: declared, shown, and its blocks kept for the next request */
    CHECK(strstr(sb.body[0], "{\"name\":\"WebSearch\",") != 0);
    CHECK(strstr(sb.body[0], "{\"name\":\"WebFetch\",") != 0);
    CHECK(strstr(cn.screen.p, "Web Search(\"Amiga 1200 accelerator cards\")") != 0);
    CHECK(strstr(cn.screen.p, "Did 1 search: 2 results") != 0);
    CHECK(strstr(sb.body[1], "{\"type\":\"server_tool_use\",\"id\":\"srvtoolu_01Search\",\"name\":\"web_search\","
                             "\"input\":{\"query\":\"Amiga 1200 accelerator cards\"}}") != 0);
    CHECK(strstr(sb.body[1], "{\"type\":\"web_search_tool_result\",\"tool_use_id\":\"srvtoolu_01Search\"") != 0);
    /* the Task: two nested requests with the subagent's own prompt and tools */
    CHECK(strstr(sb.body[2], "file search specialist") != 0);
    CHECK(strstr(sb.body[2], "{\"name\":\"Edit\",") == 0);
    CHECK(strstr(sb.body[2], "Find the Startup-Sequence in S") != 0);
    CHECK(strstr(sb.body[3], "/S/Startup-Sequence:1:SetPatch QUIET") != 0);
    /* the parent's next request: its history untouched, the Task's result added */
    CHECK_INT(messages_of(sb.body[4], &m), 0);
    CHECK_INT(json_count(m), 5);
    {
        jv m1;
        messages_of(sb.body[1], &m1);
        CHECK(!memcmp(m1.p, m.p, (size_t)m1.n - 1));
    }
    json_iter(m, &it);
    for (i = 0; i < 5; i++)
        json_next(&it, 0, &e);
    CHECK(json_get(e, "content", &c));
    json_iter(c, &it);
    CHECK(json_next(&it, 0, &x));
    CHECK(json_get(x, "tool_use_id", &e) && json_streq(e, "toolu_01Task"));
    CHECK(json_get(x, "content", &e) && json_streq(e, "S/Startup-Sequence runs SetPatch first (line 1: SetPatch "
                                                      "QUIET).\n\n(Agent Explore: 1 tool use.)"));
    /* the WebFetch: the page fetched, the small model asked, its answer the result */
    CHECK_INT(wp2_open_n, 1);
    CHECK(strstr(wp2_req, "GET /page HTTP/1.1\r\nHost: 127.0.0.1:8080\r\n") != 0);
    CHECK(strstr(sb.body[6], "\"model\":\"claude-haiku-4-5") != 0);
    CHECK(strstr(sb.body[6], "# Hi\\n\\nhello") != 0);
    CHECK(strstr(sb.body[6], "\"tools\"") == 0);
    CHECK(strstr(sb.body[7], "\"tool_use_id\":\"toolu_01Fetch\",\"content\":\"The page is the UP-Term test page.") != 0);
    /* the screen: the calls, the questions; the nested answers not drawn */
    CHECK(strstr(cn.screen.p, "Tool \033[0mTask") != 0);
    CHECK(strstr(cn.screen.p, "Tool \033[0mGrep") != 0);
    CHECK(strstr(cn.screen.p, "Tool \033[0mWebFetch") != 0);
    CHECK(strstr(snt.text.p ? snt.text.p : "", "SetPatch first (line 1") == 0);
    CHECK(strstr(snt.text.p ? snt.text.p : "", "UP-Term test page") == 0);
    CHECK(strstr(snt.text.p ? snt.text.p : "", "Blizzard 1230") != 0);
    /* every request counted for /cost, the nested ones too */
    CHECK_INT(r.conv.requests, 8);
    repl_free(&r);
}

/* ---- A4 WP4: the command line and print mode, through the REPL core ----
 *
 * main_amiga.c's print path, step by step on the host: the words parsed
 * (cli_parse_line), applied (cli_apply), the run (print_run) against the
 * recorded streams; the output captured and compared with golden files
 * (tests/claude/print_*), the run's own values (uuids, the session id,
 * durations, the temporary directory) blanked first. */

typedef struct pcap {
    jw out, err;
    const char *in;
    long pos;
} pcap;

static pcap pc;

static void pc_out(void *u, const char *s, long n)
{
    (void)u;
    jw_raw(&pc.out, s, n);
}

static void pc_err(void *u, const char *s, long n)
{
    (void)u;
    jw_raw(&pc.err, s, n);
}

/* what is piped in, 7 bytes a read (lines cross reads) */
static long pc_in(void *u, char *b, long cap)
{
    long n = (long)strlen(pc.in) - pc.pos;
    (void)u;
    if (n > 7)
        n = 7;
    if (n > cap)
        n = cap;
    memcpy(b, pc.in + pc.pos, (size_t)n);
    pc.pos += n;
    return n;
}

/* main's print path; the exit code */
static int run_print(cl_repl *r, const char *args, const char *in)
{
    cl_cli c;
    cl_pout po;
    int rc;
    jw_reset(&pc.out);
    jw_reset(&pc.err);
    pc.in = in;
    pc.pos = 0;
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, args), 0);
    CHECK_INT(cli_apply(&c, r), 0);
    po.u = 0;
    po.out = pc_out;
    po.err = pc_err;
    po.in = in ? pc_in : 0;
    rc = print_run(r, &c, &po);
    cli_free(&c);
    return rc;
}

static const char *outp(void)
{
    return pc.out.p ? pc.out.p : "";
}

/* the run's own values blanked: uuids, the session id, durations, the start directory */
static void norm(const char *s, const char *root, jw *o)
{
    static const char *const strs[] = { "\"uuid\":\"", "\"session_id\":\"", 0 };
    static const char *const nums[] = { "\"duration_ms\":", "\"duration_api_ms\":", 0 };
    long rl = (long)strlen(root);
    jw_reset(o);
    while (*s) {
        int i, done = 0;
        if (rl && !strncmp(s, root, (size_t)rl)) {
            jw_rawz(o, "<ROOT>");
            s += rl;
            continue;
        }
        for (i = 0; strs[i] && !done; i++)
            if (!strncmp(s, strs[i], strlen(strs[i]))) {
                jw_rawz(o, strs[i]);
                s += strlen(strs[i]);
                while (*s && *s != '"')
                    s++;
                jw_rawz(o, "*");
                done = 1;
            }
        for (i = 0; nums[i] && !done; i++)
            if (!strncmp(s, nums[i], strlen(nums[i]))) {
                jw_rawz(o, nums[i]);
                s += strlen(nums[i]);
                while (*s >= '0' && *s <= '9')
                    s++;
                jw_rawz(o, "0");
                done = 1;
            }
        if (!done)
            jw_raw(o, s++, 1);
    }
}

/* got against tests/claude/<name> (CL_UPDATE_GOLDEN=1 writes it instead) */
static void golden(const char *name, const char *got)
{
    long n;
    char *want;
    if (getenv("CL_UPDATE_GOLDEN")) {
        char p[256];
        FILE *f;
        strcpy(p, "tests/claude/");
        strcat(p, name);
        f = fopen(p, "wb");
        if (f) {
            fputs(got, f);
            fclose(f);
        }
    }
    want = claude_load(name, &n);
    CHECK(want != 0);
    if (want)
        CHECK_STR(got, want);
    free(want);
}

/* amiga-pi B4: the bytes C:Claude sends (head + body) for six recorded turns,
 * captured BEFORE the Anthropic head and decoder move behind amiga-pi's
 * backend interface (plan 2026-10-07-amiga-pi B4/B5): B5 must leave every one
 * of these equal. The start directory is blanked (norm) and the head's
 * Content-Length checked against the real body, then blanked, since the
 * directory's length moves it. tests/claude/golden_req_<recording>.txt. */
static void req_norm(const char *s, jw *o)
{
    char real[600], slug[2][64];
    const char *roots[4];
    jw t;
    int k;
    if (!realpath(dir, real))
        strcpy(real, dir);
    sess_slug(real, slug[0], sizeof(slug[0]));
    sess_slug(dir, slug[1], sizeof(slug[1]));
    roots[0] = real;            /* the resolved root first: /private/var/.. holds /var/.. */
    roots[1] = dir;
    roots[2] = slug[0];         /* the memory directory's name, from the random root */
    roots[3] = slug[1];
    jw_init(&t);
    jw_rawz(&t, s);
    for (k = 0; k < 4; k++) {
        norm(t.p ? t.p : "", roots[k], o);
        jw_reset(&t);
        jw_rawz(&t, o->p ? o->p : "");
    }
    jw_free(&t);
}

static void req_golden(int i, const char *name)
{
    jw h, b, all;
    const char *cl;
    if (i >= sb.nreq) {
        CHECK(i < sb.nreq);
        return;
    }
    cl = strstr(sb.head[i], "Content-Length: ");
    CHECK(cl != 0 && atol(cl + 16) == (long)strlen(sb.body[i]));
    jw_init(&h);
    jw_init(&b);
    jw_init(&all);
    req_norm(sb.head[i], &h);
    req_norm(sb.body[i], &b);
    if (h.p && cl) {
        char *c = strstr(h.p, "Content-Length: ") + 16;
        char *e = c;
        while (*e >= '0' && *e <= '9')
            e++;
        jw_raw(&all, h.p, (long)(c - h.p));
        jw_rawz(&all, "*");
        jw_rawz(&all, e);
    }
    jw_rawz(&all, b.p ? b.p : "");
    golden(name, all.p ? all.p : "");
    jw_free(&h);
    jw_free(&b);
    jw_free(&all);
}

static void test_req_golden(void)
{
    static const char *script[] = { "hello", "show me S/Startup-Sequence", "two", "three", "four", 0 };
    static cl_repl r;
    setup(&r, script);
    add_stream("text.sse");
    add_stream("tool_use.sse");
    add_stream("tool_final.sse");
    add_stream("refusal.sse");
    add_stream("max_tokens.sse");
    add_stream("overloaded.sse");
    add_stream("text.sse");
    while (cn.lines[cn.next])
        repl_line(&r, cn.lines[cn.next++]);
    CHECK_INT(sb.nreq, 7);
    req_golden(0, "golden_req_text.txt");
    req_golden(1, "golden_req_tool_use.txt");
    req_golden(2, "golden_req_tool_final.txt");
    req_golden(3, "golden_req_refusal.txt");
    req_golden(4, "golden_req_max_tokens.txt");
    req_golden(5, "golden_req_overloaded.txt");
    /* the retry after the overloaded stream sends the same bytes again */
    if (sb.nreq == 7) {
        CHECK_STR(sb.head[6], sb.head[5]);
        CHECK_STR(sb.body[6], sb.body[5]);
    }
    repl_free(&r);
}

/* every line of the output a JSON object; the count; types: their "type"s, joined by ' ' */
static int json_lines(const char *s, char *types, long cap)
{
    int k = 0;
    types[0] = 0;
    while (*s) {
        const char *e = strchr(s, '\n');
        jv v, t;
        char ty[40];
        if (!e)
            e = s + strlen(s);
        CHECK(json_parse(s, (long)(e - s), &v) == 0 && json_type(v) == J_OBJ);
        if (json_parse(s, (long)(e - s), &v) == 0 && json_get(v, "type", &t) && json_str(t, ty, sizeof(ty)) >= 0) {
            if (k)
                cl_cat(types, " ", cap);
            cl_cat(types, ty, cap);
        }
        k++;
        s = *e ? e + 1 : e;
    }
    return k;
}

/* the result object of an output (its last line) */
static int result_of(jv *v)
{
    const char *s = outp(), *last = s, *p;
    for (p = s; *p; p++)
        if (*p == '\n' && p[1])
            last = p + 1;
    return json_parse(last, (long)strlen(last), v) == 0 && json_type(*v) == J_OBJ ? 0 : -1;
}

static int has_rule(const cl_repl *r, const char *text, int kind, int src)
{
    int i;
    for (i = 0; i < r->cfg.nrules; i++)
        if (!strcmp(r->cfg.rules[i].text, text) && r->cfg.rules[i].kind == kind && r->cfg.rules[i].src == src)
            return 1;
    return 0;
}

static void test_wp4_text(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    jv m, e, x;
    jit it;
    /* text: the answer and nothing else */
    setup(&r, none);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p hello", 0), 0);
    golden("print_text.txt", outp());
    CHECK_INT(sb.nreq, 1);
    CHECK_INT(pc.err.n, 0);
    CHECK_INT(snt.text_calls, 4);           /* the sentinel: the stream went through the renderer */
    CHECK(strstr(cn.screen.p ? cn.screen.p : "", "Hello") == 0);    /* not on the console */
    repl_free(&r);

    /* text piped in goes after the prompt, a newline between */
    setup(&r, none);
    add_stream("tool_final.sse");
    CHECK_INT(run_print(&r, "PRINT explain", "line one\nline two\n"), 0);
    CHECK_STR(outp(), "Your Startup-Sequence runs SetPatch first.\n");
    CHECK_INT(sb.nreq, 1);
    if (sb.nreq == 1 && messages_of(sb.body[0], &m) == 0) {
        json_iter(m, &it);
        CHECK(json_next(&it, 0, &e) && json_get(e, "content", &x));
        CHECK(strstr(x.p, "\"text\":\"explain\\nline one\\nline two\\n\"") != 0);
    }
    repl_free(&r);

    /* only what is piped in */
    setup(&r, none);
    add_stream("tool_final.sse");
    CHECK_INT(run_print(&r, "-p", "what is this"), 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "\"text\":\"what is this\"") != 0);
    repl_free(&r);

    /* nothing at all: Claude Code's error, nothing sent */
    setup(&r, none);
    CHECK_INT(run_print(&r, "-p", 0), 20);
    CHECK(strstr(pc.err.p ? pc.err.p : "", "Input must be provided either through stdin or as a prompt argument") != 0);
    CHECK_INT(sb.nreq, 0);
    CHECK_INT(pc.out.n, 0);
    repl_free(&r);

    /* --verbose: the transcript on the error stream, the answer alone on the output */
    setup(&r, none);
    add_stream("tool_use.sse");
    add_stream("tool_final.sse");
    CHECK_INT(run_print(&r, "-p --verbose show me S/Startup-Sequence", 0), 0);
    CHECK_STR(outp(), "Your Startup-Sequence runs SetPatch first.\n");
    CHECK(strstr(pc.err.p ? pc.err.p : "", "Read") != 0);
    CHECK(strstr(pc.err.p ? pc.err.p : "", "Let me look.") != 0);
    repl_free(&r);
}

static void test_wp4_json(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    jw n;
    jv v, x, y;
    char types[400];
    jw_init(&n);
    /* json: one result; the reads in the start directory ran without a question */
    setup(&r, none);
    add_stream("tool_use.sse");
    add_stream("tool_final.sse");
    CHECK_INT(run_print(&r, "-p --output-format json show me S/Startup-Sequence", 0), 0);
    CHECK_INT(sb.nreq, 2);
    CHECK_INT(json_lines(outp(), types, sizeof(types)), 1);
    CHECK_INT(result_of(&v), 0);
    CHECK(json_get(v, "session_id", &x) && json_streq(x, r.sess.id));
    CHECK(json_get(v, "num_turns", &x) && json_long(x, 0) == 2);
    CHECK(json_get(v, "result", &x) && json_streq(x, "Your Startup-Sequence runs SetPatch first."));
    CHECK(json_get(v, "permission_denials", &x) && json_count(x) == 0);
    CHECK(json_get(v, "modelUsage", &x) && json_get(x, "claude-opus-5-5", &y));
    norm(outp(), r.tools.root, &n);
    golden("print_json.json", n.p ? n.p : "");
    CHECK(sb.nreq < 2 || strstr(sb.body[1], "SetPatch QUIET") != 0);
    repl_free(&r);

    /* stream-json: init, the answers, the tool results, the result */
    setup(&r, none);
    add_stream("tool_use.sse");
    add_stream("tool_final.sse");
    CHECK_INT(run_print(&r, "-p --output-format stream-json --verbose show me S/Startup-Sequence", 0), 0);
    CHECK_INT(json_lines(outp(), types, sizeof(types)), 5);
    CHECK_STR(types, "system assistant user assistant result");
    norm(outp(), r.tools.root, &n);
    golden("print_stream.jsonl", n.p ? n.p : "");
    repl_free(&r);

    /* --include-partial-messages: every event of the stream (not the pings) as it came */
    setup(&r, none);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p --output-format stream-json --verbose --include-partial-messages hello", 0), 0);
    CHECK_INT(json_lines(outp(), types, sizeof(types)), 16);
    CHECK_STR(types, "system stream_event stream_event stream_event stream_event stream_event stream_event "
                     "stream_event stream_event stream_event stream_event stream_event stream_event stream_event "
                     "assistant result");
    CHECK(strstr(outp(), "{\"type\":\"stream_event\",\"event\":{\"type\":\"message_start\",") != 0);
    CHECK(strstr(outp(), "\"type\":\"ping\"") == 0);
    repl_free(&r);
    jw_free(&n);
}

/* --input-format stream-json: a user message a line, each answered with its result */
static void test_wp4_input(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    char types[400];
    jv m;
    setup(&r, none);
    add_stream("text.sse");
    add_stream("tool_final.sse");
    CHECK_INT(run_print(&r, "-p --input-format stream-json --output-format stream-json --verbose",
                        "{\"type\":\"user\",\"message\":{\"role\":\"user\",\"content\":\"hello\"}}\n"
                        "{\"type\":\"control_request\"}\n"
                        "{\"type\":\"user\",\"message\":{\"role\":\"user\",\"content\":[{\"type\":\"text\","
                        "\"text\":\"and then?\"}]}}"),
              0);
    CHECK_INT(sb.nreq, 2);
    CHECK_INT(json_lines(outp(), types, sizeof(types)), 5);
    CHECK_STR(types, "system assistant result assistant result");
    CHECK(sb.nreq > 1 && messages_of(sb.body[1], &m) == 0 && json_count(m) == 3);
    CHECK(sb.nreq > 1 && strstr(sb.body[1], "\"text\":\"and then?\"") != 0);
    repl_free(&r);
}

/* --max-turns and --max-budget-usd: the turn stops after a tool round, the history valid */
static void test_wp4_limits(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    jv v, x;
    char e[80];
    setup(&r, none);
    add_stream("tool_use.sse");
    add_stream("tool_final.sse");
    CHECK_INT(run_print(&r, "-p --max-turns 1 --output-format json show me S/Startup-Sequence", 0), 10);
    CHECK_INT(sb.nreq, 1);
    CHECK_INT(r.conv.n, 3);                 /* the prompt, the tool calls, their results */
    CHECK_INT(result_of(&v), 0);
    CHECK(json_get(v, "subtype", &x) && json_streq(x, "error_max_turns"));
    CHECK(json_get(v, "is_error", &x) && json_type(x) == J_TRUE);
    CHECK(json_get(v, "result", &x) == 0);
    CHECK(json_get(v, "errors", &x) && json_count(x) == 1);
    e[0] = 0;
    if (json_get(v, "errors", &x)) {
        jit it;
        jv one;
        json_iter(x, &it);
        if (json_next(&it, 0, &one))
            json_str(one, e, sizeof(e));
    }
    CHECK_STR(e, "Reached max turns (1)");
    repl_free(&r);

    setup(&r, none);
    add_stream("tool_use.sse");
    CHECK_INT(run_print(&r, "MAX-TURNS=1 PRINT show me S/Startup-Sequence", 0), 10);
    CHECK_STR(outp(), "Error: Reached max turns (1)\n");
    repl_free(&r);

    /* the first answer costs $0.0034: a budget of $0.001 stops before the second request */
    setup(&r, none);
    add_stream("tool_use.sse");
    add_stream("tool_final.sse");
    CHECK_INT(run_print(&r, "-p --max-budget-usd 0.001 --output-format json show me S/Startup-Sequence", 0), 10);
    CHECK_INT(sb.nreq, 1);
    CHECK_INT(result_of(&v), 0);
    CHECK(json_get(v, "subtype", &x) && json_streq(x, "error_max_budget_usd"));
    CHECK(json_get(v, "total_cost_usd", &x) && x.n == 6 && !memcmp(x.p, "0.0034", 6));
    repl_free(&r);

    /* a budget not reached, and the limits outside print mode do nothing */
    setup(&r, none);
    add_stream("tool_use.sse");
    add_stream("tool_final.sse");
    CHECK_INT(run_print(&r, "-p --max-budget-usd 5 --max-turns 2 show me S/Startup-Sequence", 0), 0);
    CHECK_INT(sb.nreq, 2);
    repl_free(&r);
}

/* permissions in print mode: nobody to ask, so a write is denied and listed, unless allowed */
static void test_wp4_perms(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    char p[600], *b = 0;
    long n = 0;
    jv v, x, y;
    FILE *f;
    strcpy(p, dir);
    strcat(p, "/claude-test.txt");
    f = fopen(p, "wb");
    if (f) {
        fputs("hello\n", f);
        fclose(f);
    }
    setup(&r, none);
    add_stream("tool_edit.sse");
    add_stream("tool_final.sse");
    CHECK_INT(run_print(&r, "-p --output-format json please edit", 0), 0);
    CHECK_INT(result_of(&v), 0);
    CHECK(json_get(v, "permission_denials", &x) && json_count(x) == 1);
    if (json_get(v, "permission_denials", &x) && json_count(x) == 1) {
        jit it;
        jv d;
        json_iter(x, &it);
        json_next(&it, 0, &d);
        CHECK(json_get(d, "tool_name", &y) && json_streq(y, "Edit"));
        CHECK(json_get(d, "tool_use_id", &y) && json_streq(y, "toolu_01Edit"));
        CHECK(json_get(d, "tool_input", &y) && json_get(y, "file_path", &x) && json_streq(x, "claude-test.txt"));
    }
    CHECK(sys.read(sys.u, p, 1000, &b, &n) == 0 && b && !strcmp(b, "hello\n"));
    free(b);
    b = 0;
    repl_free(&r);

    /* --allowedTools Edit: it runs */
    setup(&r, none);
    add_stream("tool_edit.sse");
    add_stream("tool_final.sse");
    CHECK_INT(run_print(&r, "-p --output-format json --allowedTools Edit -- please edit", 0), 0);
    CHECK_INT(result_of(&v), 0);
    CHECK(json_get(v, "permission_denials", &x) && json_count(x) == 0);
    CHECK(sys.read(sys.u, p, 1000, &b, &n) == 0 && b && !strcmp(b, "hello from the Amiga\n"));
    free(b);
    b = 0;
    repl_free(&r);

    /* --dangerously-skip-permissions (bypassPermissions): every question answered yes */
    f = fopen(p, "wb");
    if (f) {
        fputs("hello\n", f);
        fclose(f);
    }
    setup(&r, none);
    add_stream("tool_edit.sse");
    add_stream("tool_final.sse");
    CHECK_INT(run_print(&r, "-p --output-format json --dangerously-skip-permissions please edit", 0), 0);
    CHECK_INT(r.tools.perm.mode, PERM_BYPASS);
    CHECK_INT(r.tools.perm.can_bypass, 1);
    CHECK_STR(perm_name(r.tools.perm.mode), "bypassPermissions");
    CHECK_INT(result_of(&v), 0);
    CHECK(json_get(v, "permission_denials", &x) && json_count(x) == 0);
    CHECK(sys.read(sys.u, p, 1000, &b, &n) == 0 && b && !strcmp(b, "hello from the Amiga\n"));
    free(b);
    repl_free(&r);
    remove(p);

    /* gaps 3: --allow-dangerously-skip-permissions: the session starts in
     * default, bypass is only reachable (Shift+Tab); without the flag not */
    setup(&r, none);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p --allow-dangerously-skip-permissions hello", 0), 0);
    CHECK_INT(r.tools.perm.mode, PERM_DEFAULT);
    CHECK_INT(r.tools.perm.can_bypass, 1);
    repl_free(&r);
    setup(&r, none);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p hello", 0), 0);
    CHECK_INT(r.tools.perm.can_bypass, 0);
    repl_free(&r);

    /* --tools and --disallowedTools: what the request declares */
    setup(&r, none);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p --tools Read,Glob hello", 0), 0);
    CHECK(sb.nreq == 1 && json_parse(sb.body[0], (long)strlen(sb.body[0]), &v) == 0 && json_get(v, "tools", &x) &&
          json_count(x) == 2);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "web_search") == 0);
    repl_load(&r);                          /* a reload (/cd, /permissions) keeps it so */
    CHECK_INT(r.tools.web_search, 0);
    repl_free(&r);
    setup(&r, none);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p --disallowedTools Bash WebSearch \"Edit(S/*)\" --tools \"\" -- hello", 0), 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "\"tools\"") == 0);    /* none at all */
    CHECK(has_rule(&r, "Edit(S/*)", RULE_DENY, CFG_SESSION));
    repl_free(&r);
    setup(&r, none);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p --disallowedTools Bash WebSearch -- hello", 0), 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "{\"name\":\"Bash\"") == 0 && strstr(sb.body[0], "web_search") == 0 &&
          strstr(sb.body[0], "{\"name\":\"Read\"") != 0);
    repl_free(&r);
}

/* the session flags: --continue restores, --resume NAME, --fork-session, --no-session-persistence */
static void test_wp4_session(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    char first[16];
    jv v, x, m;
    setup(&r, none);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p -n my-work hello", 0), 0);
    cl_copy(first, r.sess.id, sizeof(first));
    CHECK(r.sess.n_appends >= 1);
    repl_free(&r);

    /* --continue: the conversation comes back, the same session goes on */
    setup(&r, none);
    add_stream("tool_final.sse");
    CHECK_INT(run_print(&r, "-p -c --output-format json go on", 0), 0);
    CHECK_INT(sb.nreq, 1);
    CHECK(sb.nreq == 1 && messages_of(sb.body[0], &m) == 0 && json_count(m) == 3);
    CHECK(sb.nreq && strstr(sb.body[0], "\"text\":\"hello\"") != 0);
    CHECK(sb.nreq && strstr(sb.body[0], text_content) != 0);       /* the answer replayed byte for byte */
    CHECK_INT(result_of(&v), 0);
    CHECK(json_get(v, "session_id", &x) && json_streq(x, first));
    CHECK_STR(r.sess.id, first);
    repl_free(&r);

    /* --resume by name, --fork-session: a new session with the whole conversation */
    setup(&r, none);
    add_stream("tool_final.sse");
    CHECK_INT(run_print(&r, "PRINT RESUME=my-work FORK-SESSION once more", 0), 0);
    CHECK_INT(sb.nreq, 1);
    CHECK(sb.nreq == 1 && messages_of(sb.body[0], &m) == 0 && json_count(m) == 5);
    CHECK(strcmp(r.sess.id, first) != 0);
    repl_free(&r);

    /* nothing to resume: Claude Code's error as the result */
    setup(&r, none);
    CHECK_INT(run_print(&r, "-p -r nothing-here hi", 0), 10);
    CHECK_STR(outp(), "No conversation found with session ID: nothing-here\n");
    CHECK_INT(sb.nreq, 0);
    repl_free(&r);

    /* --no-session-persistence: nothing written */
    setup(&r, none);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p --no-session-persistence hello", 0), 0);
    CHECK_INT(r.sess.n_appends, 0);
    repl_free(&r);
}

/* the other flags, applied: the settings layer wins over the files and survives a reload */
static void test_wp4_apply(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    cl_cli c;
    char p[600];
    long l;
    FILE *f;
    jv v, x;
    setup(&r, none);
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, "--system-prompt \"Be brief.\" --append-system-prompt \"Answer in German.\" "
                                 "--model sonnet --effort high --tools Read,Grep --add-dir S --allowedTools "
                                 "\"Bash(make *)\" --permission-mode plan --settings "
                                 "\"{*\"model*\":*\"opus*\",*\"outputStyle*\":*\"Explanatory*\"}\""),
              0);
    CHECK_INT(cli_apply(&c, &r), 0);
    CHECK_STR(r.model, "claude-sonnet-5-5");    /* the flag wins over --settings */
    CHECK_STR(r.style, "Explanatory");          /* --settings' own key */
    CHECK_STR(r.effort, "high");
    CHECK_INT(r.tools.perm.mode, PERM_PLAN);
    CHECK(r.cfg.ndirs == 1 && !strcmp(r.cfg.dirs[0], "S"));
    CHECK(has_rule(&r, "Bash(make *)", RULE_ALLOW, CFG_SESSION));
    CHECK(has_rule(&r, "WebSearch", RULE_DENY, CFG_SESSION));     /* --tools without it */
    CHECK(!strncmp(r.system, "Be brief.", 9));
    l = (long)strlen(r.system);
    CHECK(l > 17 && !strcmp(r.system + l - 17, "Answer in German."));
    CHECK(strstr(r.system, "Output style: Explanatory") != 0);
    CHECK(strstr(r.system, "native client on an Amiga") == 0);
    add_stream("text.sse");
    repl_line(&r, "hello");
    CHECK(sb.nreq == 1 && json_parse(sb.body[0], (long)strlen(sb.body[0]), &v) == 0 && json_get(v, "tools", &x) &&
          json_count(x) == 2);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "\"model\":\"claude-sonnet-5-5\"") != 0);
    /* /cd and every reload keep the command line's layer */
    repl_load(&r);
    CHECK_STR(r.model, "claude-sonnet-5-5");
    CHECK_INT(r.tools.perm.mode, PERM_PLAN);
    cli_free(&c);
    repl_free(&r);

    /* --agent: its prompt and its tools; --settings from a file; dontAsk */
    strcpy(p, dir);
    strcat(p, "/s.json");
    f = fopen(p, "wb");
    if (f) {
        fputs("{\"model\":\"haiku\",\"permissions\":{\"defaultMode\":\"dontAsk\"}}", f);
        fclose(f);
    }
    setup(&r, none);
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, "AGENT=Explore SETTINGS=s.json"), 0);
    CHECK_INT(cli_apply(&c, &r), 0);
    CHECK(!strncmp(r.system, "You are a file search specialist", 32));
    CHECK_STR(r.model, cfg_model("haiku"));
    CHECK_INT(r.ask_policy, ASKP_DENY);
    CHECK_INT((long)r.tools.allowed,
              (long)((1ul << T_GLOB) | (1ul << T_GREP) | (1ul << T_READ) | (1ul << T_BASH)));
    CHECK_INT(r.tools.web_search, 0);
    cli_free(&c);
    repl_free(&r);
    remove(p);

    /* what cannot be applied */
    setup(&r, none);
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, "--add-dir nothere"), 0);
    CHECK_INT(cli_apply(&c, &r), -1);
    CHECK_STR(c.err, "Error: --add-dir: not a directory: nothere");
    cli_free(&c);
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, "--agent nobody"), 0);
    CHECK_INT(cli_apply(&c, &r), -1);
    cli_free(&c);
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, "--system-prompt-file missing.txt"), 0);
    CHECK_INT(cli_apply(&c, &r), -1);
    CHECK(strstr(c.err, "--system-prompt-file file not found") != 0);
    cli_free(&c);
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, "--settings \"{not json\""), 0);
    CHECK_INT(cli_apply(&c, &r), -1);
    cli_free(&c);
    repl_free(&r);
}


/* ---- the setup wizard (CLAUDE-SETUP-WIZARD) ---- */

static void setup_done_mark(int on)
{
    char p[700];
    strcpy(p, dir);
    strcat(p, "/home/setup-done");
    if (on) {
        FILE *f = fopen(p, "wb");
        if (f) {
            fputs("x\n", f);
            fclose(f);
        }
    } else
        remove(p);
}

static void unlink_home(const char *name)
{
    char p[700];
    strcpy(p, dir);
    strcat(p, "/home/");
    strcat(p, name);
    remove(p);
}

static int home_has(const char *name)
{
    char p[700];
    strcpy(p, dir);
    strcat(p, "/home/");
    strcat(p, name);
    return sys.kind(sys.u, p) == 1;
}

/* the whole wizard through the REPL's own start (repl_run): the entry point */
static void test_setup_reach(void)
{
    static const char *remote_script[] = { "1", "192.0.2.10 2323", "n", "/exit", 0 };
    static cl_repl r;
    char *t = 0;
    long n = 0, port = 0;
    char host[100];
    char p[700];

    /* first start: no key, no remote, no marker: the wizard comes up in place of /login */
    setup_done_mark(0);
    unlink_home("remote");
    setup_key = "";
    setup(&r, remote_script);
    setup_key = "test-key-not-real";
    CHECK_INT(setup_due(&r), 1);
    repl_run(&r);
    CHECK(strstr(cn.screen.p, "Setup, step 1 of 4") != 0);
    CHECK(strstr(cn.screen.p, "uptelnetd") != 0);                   /* explained */
    CHECK(strstr(cn.screen.p, "[OK] Connect to the computer: 192.0.2.10 port 2323") != 0);
    CHECK(strstr(cn.screen.p, "Paste the API key") == 0);           /* not /login's page */
    CHECK_INT(sb.opens, 1);                                         /* the sentinel: the connect test ran */
    CHECK_STR(open_host, "192.0.2.10");
    CHECK_INT(open_port, 2323);
    CHECK(strstr(cn.screen.p, "github.com/thomas-luebker") == 0);     /* n: the optional page skipped */
    CHECK(strstr(cn.screen.p, "Setup, step 4 of 4: done.") != 0);
    CHECK(home_has("remote") && home_has("setup-done"));
    strcpy(p, dir);
    strcat(p, "/home/remote");
    CHECK_INT(sys.read(sys.u, p, 511, &t, &n), 0);
    CHECK_INT(cli_remote_parse(t, host, sizeof(host), &port), 1);   /* the format main_amiga.c reads */
    CHECK_STR(host, "192.0.2.10");
    CHECK_INT(port, 2323);
    CHECK(strncmp(t, "; Claude Code on another computer", 33) == 0);
    free(t);
    CHECK_INT(setup_due(&r), 0);                                    /* the marker and the file are there */
    repl_free(&r);
}

static void test_setup_steps(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    char text[512];

    /* the writer: two comment lines, then host port; refuses what the reader would */
    CHECK_INT(setup_remote_text("nas", 2399, text, sizeof(text)), 0);
    CHECK_STR(text, "; Claude Code on another computer: host and port. With no API key,\n"
                    "; plain Claude connects there (uptelnet). Delete this file to stop.\nnas 2399\n");
    CHECK_INT(setup_remote_text("nas", 2399, text, 20), -1);

    /* remote, the computer does not answer: named error, and the file waits for the answer */
    setup_done_mark(0);
    unlink_home("remote");
    setup(&r, none);
    open_fail = 1;
    setup_begin(&r);
    CHECK_INT(r.wiz.step, SETUP_MODE);
    repl_line(&r, "4");
    CHECK_INT(r.wiz.step, SETUP_MODE);                              /* not a choice */
    repl_line(&r, "1");
    CHECK_INT(r.wiz.step, SETUP_HOST);
    repl_line(&r, "nas.local 70000");
    CHECK_INT(r.wiz.step, SETUP_HOST);
    repl_line(&r, "nas.local 2399");
    CHECK_INT(r.wiz.step, SETUP_KEEP);
    CHECK(strstr(cn.screen.p, "[FAIL] Connect to the computer: cannot connect to host: connection refused (ECONNREFUSED)") != 0);
    CHECK_INT(home_has("remote"), 0);
    repl_line(&r, "n");                                             /* asked for the address again */
    CHECK_INT(r.wiz.step, SETUP_HOST);
    repl_line(&r, "nas.local 2399");
    repl_line(&r, "y");                                             /* kept anyway */
    CHECK_INT(r.wiz.step, SETUP_INFO);
    CHECK_INT(home_has("remote"), 1);
    CHECK_INT(home_has("setup-done"), 0);
    repl_line(&r, "y");
    CHECK(strstr(cn.screen.p, "github.com/thomas-luebker/amimcp") != 0);
    CHECK(strstr(cn.screen.p, "never") != 0 && strstr(cn.screen.p, "forward the port") != 0);
    CHECK_INT(r.wiz.step, SETUP_OFF);
    CHECK_INT(home_has("setup-done"), 1);

    /* the existing file is the default: an empty line keeps it (CLAUDE-REMOTE-KEPT) */
    open_fail = 0;
    sb.opens = 0;
    repl_line(&r, "/setup");
    repl_line(&r, "1");
    CHECK(strstr(cn.screen.p, "Now: nas.local 2399 (an empty line keeps it)") != 0);
    repl_line(&r, "");
    CHECK_INT(sb.opens, 1);
    CHECK_STR(open_host, "nas.local");
    repl_line(&r, "n");
    CHECK_INT(r.wiz.step, SETUP_OFF);
    CHECK_INT(setup_write_remote(&sys, r.home, "10.0.0.2", 2323), 0);   /* the one writer replaces the line */
    {
        char *t = 0, host[64];
        long n = 0, port = 0;
        char p[700];
        strcpy(p, dir);
        strcat(p, "/home/remote");
        CHECK_INT(sys.read(sys.u, p, 511, &t, &n), 0);
        CHECK_INT(cli_remote_parse(t, host, sizeof(host), &port), 1);
        CHECK_STR(host, "10.0.0.2");
        free(t);
    }
    CHECK_INT(setup_write_remote(&sys, r.home, "", 2323), -1);
    CHECK_INT(setup_write_remote(&sys, r.home, "nas", 0), -1);
    repl_free(&r);
    unlink_home("remote");

    /* a slash command leaves the wizard and runs as usual; later stores nothing */
    setup_done_mark(0);
    setup(&r, none);
    setup_begin(&r);
    repl_line(&r, "/cost");
    CHECK_INT(r.wiz.step, SETUP_OFF);
    CHECK_INT(home_has("setup-done"), 0);
    repl_line(&r, "/setup");
    repl_line(&r, "3");
    CHECK(strstr(cn.screen.p, "Nothing was stored") != 0);
    CHECK_INT(home_has("setup-done"), 1);
    CHECK_INT(home_has("remote"), 0);
    repl_free(&r);
    setup_done_mark(0);
}

/* the wizard's menus in the screen: Down and Enter choose, Esc goes back (line mode: the tests above) */
static void test_setup_menu(void)
{
    static const char *keys[] = {
        "\033",                            /* 1: Esc at the first question leaves */
        "\033[B", "\033[B", "\r",          /* 2: Down, Down, Enter: Later */
        "\r",                              /* 3: Enter: Claude Code on another computer */
        "\033[B", "\r",                    /* 3: the connect failed: Down, Enter: enter the address again */
        "\033",                            /* 3: failed again: Esc is Back to the first question */
        "\033",                            /* 3: Esc at the first question leaves */
        "\r", "\r",                        /* 4: the computer, failed: Enter stores anyway */
        "\033[B", "\r",                    /* 4: the page question: Down, Enter skips it */
        "\r", "\r",                        /* 5: the computer answers; the page question: Enter shows it */
        "\033[B", "\r", 0                  /* 6: the key mode (Down, Enter) */
    };
    static cl_repl r;
    stub_reset();
    cs_open(80, 24, keys);
    cs_io(&io);
    io.log = 0;
    net.u = 0;
    net.open = s_open;
    net.send = s_send;
    net.recv = s_recv;
    net.close = s_close;
    net.err = s_err;
    sys_posix_init(&sp, &sys);
    setup_done_mark(0);
    unlink_home("remote");
    setup_key = "";
    CHECK_INT(repl_init(&r, &io, &net, &sys, CL_DEFAULT_URL, setup_key, dir), 0);
    setup_key = "test-key-not-real";
    CHECK_INT(repl_screen(&r), 0);
    CHECK(r.ui.tui != 0);
    open_fail = 0;

    setup_begin(&r);
    CHECK_INT(r.wiz.step, SETUP_OFF);                       /* Esc */
    CHECK_INT(cs.next, 1);
    CHECK_INT(home_has("setup-done"), 0);

    setup_begin(&r);
    CHECK_INT(r.wiz.step, SETUP_OFF);                       /* Later */
    CHECK_INT(cs.next, 4);
    CHECK_INT(home_has("setup-done"), 1);
    CHECK_INT(home_has("remote"), 0);
    setup_done_mark(0);

    open_fail = 1;
    setup_begin(&r);
    CHECK_INT(r.wiz.step, SETUP_HOST);                      /* Enter on the first option */
    CHECK_INT(cs.next, 5);
    repl_line(&r, "nas.local 2399");
    CHECK_INT(r.wiz.step, SETUP_HOST);                      /* Enter the address again */
    CHECK_INT(cs.next, 7);
    CHECK_INT(sb.opens, 1);
    repl_line(&r, "nas.local 2399");
    CHECK_INT(r.wiz.step, SETUP_OFF);                       /* Esc: Back, then Esc leaves */
    CHECK_INT(cs.next, 9);
    CHECK_INT(sb.opens, 2);
    CHECK_INT(home_has("remote"), 0);

    setup_begin(&r);
    repl_line(&r, "nas.local 2399");
    CHECK_INT(r.wiz.step, SETUP_OFF);                       /* stored anyway, page skipped, done */
    CHECK_INT(cs.next, 13);
    CHECK_INT(home_has("remote"), 1);
    CHECK_INT(home_has("setup-done"), 1);
    CHECK(strstr(cn.screen.p, "github.com/thomas-luebker") == 0);
    setup_done_mark(0);

    open_fail = 0;
    setup_begin(&r);
    repl_line(&r, "nas.local 2399");
    CHECK_INT(r.wiz.step, SETUP_OFF);                       /* answers; Show */
    CHECK_INT(cs.next, 15);
    CHECK(strstr(cn.screen.p, "github.com/thomas-luebker") != 0 || cs_find("amimcp") >= 0);
    setup_done_mark(0);

    setup_begin(&r);
    CHECK_INT(cs.next, 17);
    CHECK_INT(r.wiz.step, SETUP_KEY);                       /* Down, Enter: the API key here */
    CHECK_INT(r.await_key, 1);
    repl_free(&r);
    setup_done_mark(0);
    unlink_home("remote");
}

/* the key: through /login's path, masked, tested with one request, never in the screen or the log */
static void test_setup_key(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    setup_done_mark(0);
    unlink_home("key");
    setup_key = "";
    setup(&r, none);
    setup_key = "test-key-not-real";
    setup_begin(&r);
    repl_line(&r, "2");
    CHECK_INT(r.wiz.step, SETUP_KEY);
    CHECK_INT(r.await_key, 1);                                      /* the line is read as a key: not echoed */
    repl_line(&r, "short");
    CHECK_INT(r.wiz.step, SETUP_KEY);                               /* refused, asked again */
    CHECK_INT(home_has("key"), 0);
    add_stream("text.sse");
    repl_line(&r, "sk-ant-wizard-0123456789");
    CHECK(has("home/key", "sk-ant-wizard-0123456789"));             /* /login's file */
    CHECK(r.key && !strcmp(r.key, "sk-ant-wizard-0123456789"));
    CHECK_INT(sb.nreq, 1);                                          /* the test request ran */
    CHECK(sb.nreq == 1 && strstr(sb.head[0], "x-api-key: sk-ant-wizard-0123456789\r\n") != 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "\"max_tokens\":1") != 0);
    CHECK(strstr(cn.screen.p, "sk-ant-wizard") == 0);               /* never shown */
    CHECK_INT(r.wiz.step, SETUP_INFO);
    repl_line(&r, "n");
    CHECK(strstr(cn.screen.p, "Setup, step 4 of 4: done.") != 0);
    CHECK_INT(home_has("setup-done"), 1);
    CHECK(strstr(cn.screen.p, "sk-ant-wizard") == 0);
    /* with a key the wizard is not due; Claude SETUP asks for it anyway */
    CHECK_INT(setup_due(&r), 0);
    repl_free(&r);
    unlink_home("key");
    setup_done_mark(0);
}

/* `Claude "prompt"` starts the session with it; without a key, /login comes first */
static void test_wp4_start(void)
{
    static const char *bye[] = { "/exit", 0 };
    static const char *login[] = { "sk-ant-api-test-0123456789", "/exit", 0 };
    static cl_repl r;
    cl_cli c;
    jv v, x;
    setup(&r, bye);
    add_stream("text.sse");
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, "\"hello there\""), 0);
    CHECK_INT(cli_apply(&c, &r), 0);
    r.first = c.prompt;
    repl_run(&r);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "\"text\":\"hello there\"") != 0);
    CHECK_INT(cn.next, 1);                  /* then the prompt, where /exit was typed */
    cli_free(&c);
    repl_free(&r);

    setup_key = "";
    setup(&r, login);
    setup_key = "test-key-not-real";
    add_stream("text.sse");
    setup_done_mark(1);                     /* the wizard has run: /login comes first */
    CHECK_INT(repl_need_key(&r), 1);
    r.first = "hello";
    repl_run(&r);
    CHECK(strstr(cn.screen.p, "Paste the API key") != 0);
    CHECK(strstr(cn.screen.p, "sk-ant-api-test") == 0);     /* never shown */
    CHECK_INT(sb.nreq, 1);
    CHECK(sb.nreq == 1 && strstr(sb.head[0], "x-api-key: sk-ant-api-test-0123456789\r\n") != 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "\"text\":\"hello\"") != 0);
    repl_free(&r);
    setup_done_mark(0);
    unlink_home("key");

    /* print mode without a key: the error as the result, nothing sent */
    setup_key = "";
    setup(&r, bye);
    setup_key = "test-key-not-real";
    CHECK_INT(run_print(&r, "-p --output-format json hi", 0), 10);
    CHECK_INT(sb.nreq, 0);
    CHECK_INT(result_of(&v), 0);
    CHECK(json_get(v, "is_error", &x) && json_type(x) == J_TRUE);
    CHECK(json_get(v, "result", &x) && !strncmp(x.p, "\"Not logged in", 14));
    repl_free(&r);
}

static void test_wp4(void)
{
    jw_init(&pc.out);
    jw_init(&pc.err);
    test_wp4_text();
    test_wp4_json();
    test_wp4_input();
    test_wp4_limits();
    test_wp4_perms();
    test_wp4_session();
    test_wp4_apply();
    test_setup_reach();
    test_setup_steps();
    test_setup_menu();
    test_setup_key();
    test_wp4_start();
    jw_free(&pc.out);
    jw_free(&pc.err);
}

/* ---- A4 wiring: the three packages' seams joined ----
 *
 * The reachability test of the wiring, on the screen: a project with a
 * subagent (.claude/agents), two skills (one only the user may run), a
 * custom command with allowed-tools, a settings file with a status line,
 * a deny rule for WebSearch and one for a file; and a user subagent in
 * the user's directory. Typed: a prompt Claude answers with a Task for the
 * project's agent (whose reads run without a question, one of them denied
 * by the rule inside the agent), one answered with the Skill tool, one
 * with SlashCommand (whose allowed-tools let the Bash call that follows
 * run without a question), /tasks, /exit. */

/* text into root/rel, the directories on the way made */
static void xput(const char *root, const char *rel, const char *text)
{
    char p[800];
    char *s;
    FILE *f;
    strcpy(p, root);
    strcat(p, "/");
    strcat(p, rel);
    for (s = p + strlen(root) + 1; *s; s++)
        if (*s == '/') {
            *s = 0;
            mkdir(p, 0700);
            *s = '/';
        }
    f = fopen(p, "wb");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static void test_wiring(void)
{
    static const char *keys[] = { "review the startup\r", "check it\r", "\r", "greet\r", "\r", "/tasks\r",
                                  "/exit\r", 0 };
    static cl_repl r;
    char root[600], home[600], p[700];
    int row, box;
    const char *t;
    strcpy(root, dir);
    strcat(root, "/wire");
    mkdir(root, 0700);
    strcpy(home, dir);
    strcat(home, "/home");
    xput(root, "S/Startup-Sequence", "SetPatch QUIET\n");
    xput(root, "secret.txt", "SECRET-CONTENT\n");
    xput(root, ".claude/agents/amiga-reviewer.md",
         "---\nname: amiga-reviewer\ndescription: Reviews Amiga startup files\ntools: Read\nmodel: haiku\n---\n"
         "AMIGA-REVIEWER-PROMPT: you review AmigaDOS scripts.\n");
    xput(home, "agents/user-helper.md", "---\nname: user-helper\ndescription: The user's own helper\n---\nHelp.\n");
    xput(root, ".claude/skills/startup-check/SKILL.md",
         "---\nname: startup-check\ndescription: Checks a Startup-Sequence\n---\n"
         "SKILL-BODY-SENTINEL: read S/Startup-Sequence first.\n");
    xput(root, ".claude/skills/only-me/SKILL.md",
         "---\nname: only-me\ndescription: ONLY-THE-USER\ndisable-model-invocation: true\n---\nNo.\n");
    xput(root, ".claude/commands/greet.md",
         "---\ndescription: Greet someone\nallowed-tools: Bash(Wait:*)\n---\nSay hello to $ARGUMENTS.\n");
    xput(root, ".claude/settings.json",
         "{\"statusLine\":{\"type\":\"command\",\"command\":\"echo WIRED-STATUS\"},"
         "\"permissions\":{\"deny\":[\"WebSearch\",\"Read(secret.txt)\"]}}\n");

    stub_reset();
    add_stream("wire_task.sse");
    add_stream("wire_agent_reads.sse");
    add_stream("agent_final.sse");
    add_stream("tool_final.sse");
    add_stream("wire_skill.sse");
    add_stream("tool_final.sse");
    add_stream("wire_slash.sse");
    add_stream("tool_bg.sse");
    add_stream("tool_final.sse");

    cs_open(80, 24, keys);
    cs_io(&io);
    io.log = 0;
    net.u = 0;
    net.open = s_open;
    net.send = s_send;
    net.recv = s_recv;
    net.close = s_close;
    net.err = s_err;
    sys_posix_init(&sp, &sys);
    sp.bg_hold = 1;                 /* "Wait 2" ends in real seconds, the screen runs on a fake clock: held */
    CHECK_INT(repl_init(&r, &io, &net, &sys, CL_DEFAULT_URL, "test-key-not-real", root), 0);
    CHECK_INT(repl_screen(&r), 0);
    repl_run(&r);
    if (getenv("CL_DUMP")) {
        int k;
        for (k = 0; k < cs.rows; k++)
            printf("%2d|%s\n", k, cs_row(k));
    }
    /* every key used: no question the script did not expect (the reads asked nothing) */
    sp.bg_hold = 0;
    CHECK_INT(cs.next, 7);
    CHECK_INT(sb.nreq, 9);          /* keys typed straight on: no background turn in between */
    if (sb.nreq < 9) {
        repl_free(&r);
        cs_close();
        return;
    }

    /* the first request: Task offers the project's and the user's agents beside the
     * built-ins; Skill lists the model's skills only; SlashCommand the command; no
     * web_search (the deny rule) */
    t = sb.body[0];
    CHECK(strstr(t, "- amiga-reviewer: Reviews Amiga startup files (Tools: Read)") != 0);
    CHECK(strstr(t, "- user-helper: The user's own helper") != 0);
    CHECK(strstr(t, "- general-purpose: ") != 0);
    CHECK(strstr(t, "{\"name\":\"Skill\",") != 0);
    CHECK(strstr(t, "- startup-check: Checks a Startup-Sequence") != 0);
    CHECK(strstr(t, "ONLY-THE-USER") == 0);
    CHECK(strstr(t, "{\"name\":\"SlashCommand\",") != 0);
    CHECK(strstr(t, "- /greet: Greet someone") != 0);
    CHECK(strstr(t, "web_search") == 0);

    /* the project agent ran with its prompt, its model and its one tool */
    t = sb.body[1];
    CHECK(strstr(t, "AMIGA-REVIEWER-PROMPT") != 0);
    CHECK(strstr(t, "\"model\":\"claude-haiku-4-5") != 0);
    CHECK(strstr(t, "{\"name\":\"Read\",") != 0);
    CHECK(strstr(t, "{\"name\":\"Grep\",") == 0);
    CHECK(strstr(t, "{\"name\":\"Task\",") == 0);
    /* its calls went through the policy: the read ran, the rule denied secret.txt */
    t = sb.body[2];
    CHECK(strstr(t, "\"tool_use_id\":\"toolu_01WireRead\",\"content\":\"     1\\tSetPatch QUIET\\n\"") != 0);
    CHECK(strstr(t, "Permission to use Read has been denied by the rule Read(secret.txt) (project settings).") !=
          0);
    CHECK(strstr(t, "SECRET-CONTENT") == 0);
    CHECK_INT(r.n_rule_deny, 1);
    /* ... and the screen showed the agent's Read as a Read, not as its Task */
    CHECK(strstr(cs.sent.p, "Read\033[0m(S/Startup-Sequence)") != 0 || strstr(cs.sent.p, "Read(S/Startup-Sequence)") != 0);
    CHECK(strstr(sb.body[3], "(Agent amiga-reviewer: 2 tool uses.)") != 0);

    /* the skill: its body (no frontmatter) and the arguments came back */
    t = sb.body[5];
    CHECK(strstr(t, "Launching skill: startup-check") != 0);
    CHECK(strstr(t, "SKILL-BODY-SENTINEL: read S/Startup-Sequence first.") != 0);
    CHECK(strstr(t, "ARGUMENTS: S:") != 0);
    CHECK(strstr(t, "description: Checks") == 0);

    /* the custom command through SlashCommand, expanded */
    t = sb.body[7];
    CHECK(strstr(t, "Launching command /greet. Carry out these instructions:\\n\\nSay hello to Amiga.") != 0);
    CHECK_INT(r.n_cmds_run, 1);
    /* its allowed-tools let the Bash call in the same turn run without a question */
    CHECK(strstr(sb.body[8], "Command running in background with ID: bash_1") != 0);
    CHECK_INT(r.n_rule_allow, 1);
    CHECK(r.turn_tools == 0);       /* only for that turn */

    /* /tasks: the background shell in the one list */
    CHECK(strstr(cs.sent.p, "bash_1") != 0);
    CHECK(strstr(cs.sent.p, "Background shells") != 0);

    /* the status line: the command ran on its events, its row under the box */
    CHECK_STR(r.status_text, "WIRED-STATUS");
    CHECK(r.n_status_runs >= 2);
    row = cs_find("WIRED-STATUS");
    box = cs_find("\342\225\260");
    CHECK(row >= 0 && box >= 0 && row == box + 1);
    CHECK(row >= 0 && strstr(cs_row(row + 1), "ctx:") != 0);
    /* its schedule: nothing changed, no run; a mode change (Shift+Tab) runs it;
     * an event within 300 ms waits for the next tick; refreshInterval */
    {
        long n0 = r.n_status_runs;
        cs.clock += 1000;
        pol_status_tick(&r);
        CHECK_INT(r.n_status_runs, n0);
        r.tools.perm.mode = PERM_ACCEPT;
        pol_status_tick(&r);
        CHECK_INT(r.n_status_runs, n0 + 1);
        pol_status_event(&r);
        CHECK_INT(r.n_status_runs, n0 + 1);
        CHECK_INT(r.status_due, 1);
        cs.clock += 400;
        pol_status_tick(&r);
        CHECK_INT(r.n_status_runs, n0 + 2);
        r.cfg.status_refresh_s = 2;
        cs.clock += 1000;
        pol_status_tick(&r);
        CHECK_INT(r.n_status_runs, n0 + 2);
        cs.clock += 1500;
        pol_status_tick(&r);
        CHECK_INT(r.n_status_runs, n0 + 3);
    }
    repl_free(&r);
    cs_close();
    strcpy(p, home);
    strcat(p, "/agents/user-helper.md");
    remove(p);
}

/* WebFetch on the screen: under "Fetch(url)" Claude Code's line "Received
 * N bytes (200 OK)" -- the small model's answer goes to Claude only (it
 * used to be drawn there, cut at the window's edge: rig run 2026-10-05) */
static void test_fetch_screen(void)
{
    static const char *keys[] = { "fetch the page\r", "\r", "/exit\r", 0 };
    static cl_repl r;
    cl_net web;
    int row;
    stub_reset();
    add_stream("tool_fetch.sse");
    add_stream("fetch_answer.sse");
    add_stream("tool_final.sse");
    wp2_open_n = 0;
    cs_open(80, 24, keys);
    cs_io(&io);
    io.log = 0;
    net.u = 0;
    net.open = s_open;
    net.send = s_send;
    net.recv = s_recv;
    net.close = s_close;
    net.err = s_err;
    web = net;
    web.open = wp_open;
    web.send = wp_send;
    web.recv = wp_recv;
    web.close = wp_close;
    sys_posix_init(&sp, &sys);
    CHECK_INT(repl_init(&r, &io, &net, &sys, CL_DEFAULT_URL, "test-key-not-real", dir), 0);
    r.tools.web = &web;
    free(r.tools.json);
    r.tools.json = 0;
    CHECK_INT(repl_screen(&r), 0);
    repl_run(&r);
    if (getenv("CL_DUMP")) {
        int k;
        for (k = 0; k < cs.rows; k++)
            printf("%2d|%s\n", k, cs_row(k));
    }
    CHECK_INT(cs.next, 3);
    CHECK_INT(sb.nreq, 3);
    CHECK_INT(wp2_open_n, 1);
    /* Claude got the answer; the screen got the summary */
    CHECK(sb.nreq < 3 || strstr(sb.body[2], "The page is the UP-Term test page.") != 0);
    row = cs_find(SB " Fetch(http://127.0.0.1:8080/page)");
    CHECK(row >= 0);
    CHECK_STR(row >= 0 ? cs_row(row + 1) : "", "  " SC "  Received 47 bytes (200 OK)");
    CHECK(cs_find("The page is the UP-Term test page.") < 0);
    repl_free(&r);
    cs_close();
}

/* WebSearch on the screen (rig run 2026-10-05, claude_rig2): it asks first,
 * as Claude Code does, and a prompt typed while the question is open
 * answers it (its Enter is the "Yes": the rig's lost "fetch the page").
 * Answered, the search is one "Web Search("query")" line with the tool's
 * result under it: the search's own request (api_send) draws nothing (its
 * server_tool_use and result were drawn a second time, as a header and a
 * result of their own). The prompt after the turn goes out. */
static int ws_head, ws_plain, ws_server_result;
static char ws_under[200];

static void ws_look(void)
{
    if (cs.next != 2)
        return;                     /* the turn is over, the next prompt not typed yet */
    ws_head = cs_find(SB " Web Search(\"Amiga 1200 accelerator cards\")");
    ws_plain = cs_find("Web Search(Amiga");
    ws_server_result = cs_find("Did 1 search: 2 results");
    cl_copy(ws_under, ws_head >= 0 ? cs_row(ws_head + 1) : "", sizeof(ws_under));
}

static void test_websearch_screen(void)
{
    static const char *keys[] = { "search the web\r", "\r", "the next prompt\r", "/exit\r", 0 };
    static cl_repl r;
    stub_reset();
    add_stream("tool_websearch.sse");
    add_stream("websearch.sse");
    add_stream("tool_final.sse");
    add_stream("text.sse");
    cs_open(80, 24, keys);
    cs.before_read = ws_look;
    ws_head = ws_plain = ws_server_result = -2;
    ws_under[0] = 0;
    cs_io(&io);
    io.log = 0;
    net.u = 0;
    net.open = s_open;
    net.send = s_send;
    net.recv = s_recv;
    net.close = s_close;
    net.err = s_err;
    sys_posix_init(&sp, &sys);
    CHECK_INT(repl_init(&r, &io, &net, &sys, CL_DEFAULT_URL, "test-key-not-real", dir), 0);
    CHECK_INT(repl_screen(&r), 0);
    repl_run(&r);
    cs.before_read = 0;
    if (getenv("CL_DUMP")) {
        int k;
        for (k = 0; k < cs.rows; k++)
            printf("%2d|%s\n", k, cs_row(k));
    }
    CHECK_INT(cs.next, 4);
    /* the question came; its Enter let the search run */
    CHECK(strstr(cs.sent.p, "Do you want to allow this?") != 0);
    CHECK_INT(sb.nreq, 4);
    CHECK(sb.nreq < 2 || strstr(sb.body[1], "You are an assistant for performing a web search tool use.") != 0);
    /* one header, quoted, with the tool's result under it; nothing of the
     * search's own request drawn */
    CHECK(ws_head >= 0);
    CHECK_STR(ws_under, "  " SC "  Did 1 search");
    CHECK_INT(ws_plain, -1);
    CHECK_INT(ws_server_result, -1);
    /* the prompt after the turn went out */
    CHECK(sb.nreq < 4 || strstr(sb.body[3], "{\"type\":\"text\",\"text\":\"the next prompt\"}") != 0);
    repl_free(&r);
    cs_close();
}

/* ---- A4 gaps (thoughts/shared/plans/2026-10-05-a4-gaps-progress.md) ---- */

static int count_of(const char *s, const char *what)
{
    int k = 0;
    long l = (long)strlen(what);
    while (s && (s = strstr(s, what)) != 0) {
        k++;
        s += l;
    }
    return k;
}

static int exists(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0;
}

/* -c on the screen: the start header comes first, the resumed
 * conversation and its "Resumed ..." line under it (the owner saw the old
 * session drawn above the header, 2026-10-05) */
static void test_continue_header_first(void)
{
    static const char *first[] = { "hello\r", "/exit\r", 0 };
    static const char *second[] = { "/exit\r", 0 };
    static cl_repl r;
    const char *head, *resumed, *old;
    stub_reset();
    add_stream("text.sse");
    cs_open(80, 24, first);
    cs_io(&io);
    io.log = 0;
    net.u = 0;
    net.open = s_open;
    net.send = s_send;
    net.recv = s_recv;
    net.close = s_close;
    net.err = s_err;
    sys_posix_init(&sp, &sys);
    CHECK_INT(repl_init(&r, &io, &net, &sys, CL_DEFAULT_URL, "test-key-not-real", dir), 0);
    CHECK_INT(repl_screen(&r), 0);
    repl_run(&r);
    repl_free(&r);
    cs_close();

    stub_reset();
    cs_open(80, 24, second);
    cs_io(&io);
    io.log = 0;
    CHECK_INT(repl_init(&r, &io, &net, &sys, CL_DEFAULT_URL, "test-key-not-real", dir), 0);
    CHECK_INT(repl_screen(&r), 0);
    CHECK_INT(repl_continue(&r), 0);
    repl_run(&r);
    head = strstr(cs.sent.p, "for the Amiga");
    resumed = strstr(cs.sent.p, "Resumed a conversation of");
    old = strstr(cs.sent.p, "Hello from the Amiga!");
    CHECK(head != 0 && resumed != 0 && old != 0);
    CHECK(head && old && head < old);
    CHECK(head && resumed && head < resumed);
    CHECK_INT(count_of(cs.sent.p, "for the Amiga"), 1);  /* drawn once */
    /* W37: the window is cleared before the header, so a Shell listing above it is gone */
    {
        const char *clr = strstr(cs.sent.p, "\033[H\033[2J");
        CHECK(clr != 0 && head != 0 && clr < head);
    }
    repl_free(&r);
    cs_close();
}


/* Phase 1: the command line and print mode (G1-G17), through print_run */
static void test_gaps_print(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    static const char uuid[] = "550e8400-e29b-41d4-a716-446655440000";
    char file[400], args[700], types[400], root[600], home[600], p[700];
    jv v, m, x;
    cl_cli c;

    /* G1 --resume FILE.jsonl: the transcript's path in place of an id */
    setup(&r, none);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p hello", 0), 0);
    cl_copy(file, r.sess.file, sizeof(file));
    repl_free(&r);
    CHECK(exists(file));
    setup(&r, none);
    add_stream("tool_final.sse");
    strcpy(args, "-p -r ");
    strcat(args, file);
    strcat(args, " and again");
    CHECK_INT(run_print(&r, args, 0), 0);
    CHECK(sb.nreq == 1 && messages_of(sb.body[0], &m) == 0 && json_count(m) == 3);
    CHECK_STR(r.sess.file, file);
    repl_free(&r);

    /* G2 --session-id: a UUID as the id, the file named by its first group */
    setup(&r, none);
    add_stream("text.sse");
    strcpy(args, "-p --output-format json --session-id ");
    strcat(args, uuid);
    strcat(args, " hello");
    CHECK_INT(run_print(&r, args, 0), 0);
    CHECK_STR(r.sess.id, uuid);
    CHECK(strstr(r.sess.file, "/550e8400.jsonl") != 0 && exists(r.sess.file));
    CHECK(result_of(&v) == 0 && json_get(v, "session_id", &x) && json_streq(x, uuid));
    repl_free(&r);
    setup(&r, none);                    /* in use now */
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, args), 0);
    CHECK_INT(cli_apply(&c, &r), -1);
    CHECK(strstr(c.err, "is already in use") != 0);
    cli_free(&c);
    repl_free(&r);
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, "-p --session-id 1234 hi"), -1);
    CHECK_STR(c.err, "Error: Invalid session ID. Must be a valid UUID.");
    cli_free(&c);
    setup(&r, none);                    /* resumed by the whole UUID */
    add_stream("tool_final.sse");
    strcpy(args, "-p -r ");
    strcat(args, uuid);
    strcat(args, " more");
    CHECK_INT(run_print(&r, args, 0), 0);
    CHECK(sb.nreq == 1 && messages_of(sb.body[0], &m) == 0 && json_count(m) == 3);
    repl_free(&r);

    /* G3 --json-schema: the StructuredOutput tool with the schema as its
     * input; a wrong answer is sent back, the right one is the result's
     * structured_output */
    xput(dir, "schema.json",
         "{\"type\":\"object\",\"properties\":{\"functions\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}},"
         "\"required\":[\"functions\"],\"additionalProperties\":false}");
    setup(&r, none);
    add_answer("toolu_S1", "StructuredOutput", "{\"functions\":3}", 0);
    add_answer("toolu_S2", "StructuredOutput", "{\"functions\":[\"main\",\"loop\"]}", 0);
    add_answer(0, 0, 0, "Done.");
    CHECK_INT(run_print(&r, "-p --output-format json --json-schema schema.json list them", 0), 0);
    CHECK_INT(sb.nreq, 3);
    CHECK(sb.nreq > 0 && strstr(sb.body[0], "{\"name\":\"StructuredOutput\",") != 0 &&
          strstr(sb.body[0], "\"input_schema\":{\"type\":\"object\",\"properties\":{\"functions\"") != 0);
    CHECK(sb.nreq > 1 && strstr(sb.body[1], "does not match the required schema") != 0);
    CHECK(sb.nreq > 2 && strstr(sb.body[2], "Structured output provided successfully") != 0);
    CHECK(result_of(&v) == 0 && json_get(v, "subtype", &x) && json_streq(x, "success"));
    CHECK(json_get(v, "structured_output", &x) && json_get(x, "functions", &m) && json_count(m) == 2);
    repl_free(&r);
    /* never called: reminded three times, then Claude Code's error subtype */
    setup(&r, none);
    add_answer(0, 0, 0, "No tool.");
    add_answer(0, 0, 0, "Still no tool.");
    add_answer(0, 0, 0, "No.");
    add_answer(0, 0, 0, "Never.");
    CHECK_INT(run_print(&r, "-p --output-format json --json-schema schema.json list them", 0), 10);
    CHECK_INT(sb.nreq, 4);
    CHECK(sb.nreq > 1 && strstr(sb.body[1], "You have not called the StructuredOutput tool") != 0);
    CHECK(result_of(&v) == 0 && json_get(v, "subtype", &x) && json_streq(x, "error_max_structured_output_retries"));
    repl_free(&r);
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, "--json-schema schema.json hi"), -1);  /* print mode only */
    cli_free(&c);

    /* G4 --replay-user-messages and G5 an image block: echoed, and sent
     * to the API as it came (base64 untouched) */
    setup(&r, none);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p --input-format stream-json --output-format stream-json --verbose --replay-user-messages",
                        "{\"type\":\"user\",\"message\":{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":"
                        "\"what is this\"},{\"type\":\"image\",\"source\":{\"type\":\"base64\",\"media_type\":"
                        "\"image/png\",\"data\":\"iVBORw0KGgo=\"}}]}}\n"),
              0);
    CHECK_INT(json_lines(outp(), types, sizeof(types)), 4);
    CHECK_STR(types, "system user assistant result");
    CHECK(strstr(outp(), "\"isReplay\":true") != 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "{\"type\":\"image\",\"source\":{\"type\":\"base64\",\"media_type\":"
                                             "\"image/png\",\"data\":\"iVBORw0KGgo=\"}}") != 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "\"text\":\"what is this\"") != 0);
    repl_free(&r);
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, "-p --replay-user-messages hi"), -1);
    CHECK(strstr(c.err, "--replay-user-messages requires") != 0);
    cli_free(&c);

    /* G6 a subagent's messages, parent_tool_use_id its Task call: its
     * prompt, its tool_use and tool_result; its text only with
     * --forward-subagent-text */
    setup(&r, none);
    add_answer("toolu_T1", "Task",
               "{\"description\":\"Look\",\"prompt\":\"Find the startup\",\"subagent_type\":\"general-purpose\"}", 0);
    add_answer("toolu_R1", "Read", "{\"file_path\":\"S/Startup-Sequence\"}", 0);
    add_answer(0, 0, 0, "SUB-REPORT");
    add_answer(0, 0, 0, "All done.");
    CHECK_INT(run_print(&r, "-p --output-format stream-json --verbose look", 0), 0);
    CHECK_INT(sb.nreq, 4);
    json_lines(outp(), types, sizeof(types));
    CHECK_STR(types, "system assistant user assistant user user assistant result");
    CHECK_INT(count_of(outp(), "\"parent_tool_use_id\":\"toolu_T1\""), 3);
    CHECK(strstr(outp(), "\"text\":\"Find the startup\"") != 0);
    CHECK(strstr(outp(), "\"text\":\"SUB-REPORT\"") == 0);
    repl_free(&r);
    setup(&r, none);
    add_answer("toolu_T1", "Task",
               "{\"description\":\"Look\",\"prompt\":\"Find the startup\",\"subagent_type\":\"general-purpose\"}", 0);
    add_answer("toolu_R1", "Read", "{\"file_path\":\"S/Startup-Sequence\"}", 0);
    add_answer(0, 0, 0, "SUB-REPORT");
    add_answer(0, 0, 0, "All done.");
    CHECK_INT(run_print(&r, "-p --output-format stream-json --verbose --forward-subagent-text look", 0), 0);
    json_lines(outp(), types, sizeof(types));
    CHECK_STR(types, "system assistant user assistant user assistant user assistant result");
    CHECK_INT(count_of(outp(), "\"parent_tool_use_id\":\"toolu_T1\""), 4);
    CHECK(strstr(outp(), "\"text\":\"SUB-REPORT\"") != 0);
    repl_free(&r);

    /* G7 --bare / G8 --safe-mode: no CLAUDE.md, no hooks, no commands; bare
     * also only Bash, read and edit tools. SessionStart runs at the first
     * line, once the flags are known. */
    strcpy(root, dir);
    strcat(root, "/gaps");
    mkdir(root, 0700);
    strcpy(home, dir);
    strcat(home, "/home");
    xput(root, "S/Startup-Sequence", "SetPatch QUIET\n");
    xput(root, "CLAUDE.md", "GAPS-MEMORY-MARK\n");
    strcpy(p, "{\"hooks\":{\"SessionStart\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"touch ");
    strcat(p, root);
    strcat(p, "/hook-ran\"}]}]}}");
    xput(root, ".claude/settings.json", p);
    xput(root, ".claude/commands/hi.md", "Say HI-CMD.\n");
    xput(root, ".claude/skills/sk/SKILL.md", "---\ndescription: A skill\n---\nSKILL-TYPED $0 in ${CLAUDE_SKILL_DIR}\n");
    strcpy(p, root);
    strcat(p, "/hook-ran");
    setup_in(&r, none, root);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p --bare hello", 0), 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "GAPS-MEMORY-MARK") == 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "{\"name\":\"Task\",") == 0 &&
          strstr(sb.body[0], "{\"name\":\"WebFetch\",") == 0 && strstr(sb.body[0], "web_search") == 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "{\"name\":\"Bash\",") != 0 &&
          strstr(sb.body[0], "{\"name\":\"Edit\",") != 0);
    CHECK_INT(defs_count(&r.defs, DEF_COMMAND), 0);
    CHECK(!exists(p));
    repl_free(&r);
    setup_in(&r, none, root);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p hello", 0), 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "GAPS-MEMORY-MARK") != 0);
    CHECK(exists(p));                   /* SessionStart ran, after the flags */
    remove(p);
    repl_free(&r);
    setup_in(&r, none, root);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p --safe-mode hello", 0), 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "GAPS-MEMORY-MARK") == 0 && strstr(sb.body[0], "{\"name\":\"Task\",") != 0);
    CHECK(!exists(p));
    repl_free(&r);
    remove(p);

    /* S19 a skill typed as /name, expanded (X2: $0, ${CLAUDE_SKILL_DIR}); G11
     * --disable-slash-commands: neither commands nor skills */
    setup_in(&r, none, root);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p /sk ARG1", 0), 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "SKILL-TYPED ARG1 in ") != 0 && strstr(sb.body[0], "/skills/sk") != 0);
    CHECK_INT((int)r.n_skills_run, 1);
    repl_free(&r);
    setup_in(&r, none, root);
    CHECK_INT(run_print(&r, "-p --disable-slash-commands /hi", 0), 0);
    CHECK_INT(sb.nreq, 0);
    CHECK(strstr(outp(), "Unknown command") != 0);
    repl_free(&r);
    remove(p);

    /* G9 --agents (a file) with X4 keys, G10 --append-subagent-system-prompt,
     * X5 CLAUDE.md for the subagent; maxTurns 1 stops it after one round */
    xput(root, "agents.json",
         "{\"rev\":{\"description\":\"Reviews\",\"prompt\":\"REV-PROMPT\",\"tools\":[\"Read\",\"Bash\"],"
         "\"disallowedTools\":[\"Bash\"],\"maxTurns\":1}}");
    setup_in(&r, none, root);
    add_answer("toolu_T2", "Task", "{\"description\":\"Review\",\"prompt\":\"Review it\",\"subagent_type\":\"rev\"}",
               0);
    add_answer("toolu_R2", "Read", "{\"file_path\":\"S/Startup-Sequence\"}", 0);
    add_answer(0, 0, 0, "Parent done.");
    CHECK_INT(run_print(&r, "-p --agents agents.json --append-subagent-system-prompt SUB-APPEND go", 0), 0);
    CHECK_INT(sb.nreq, 3);
    CHECK(sb.nreq > 1 && strstr(sb.body[1], "REV-PROMPT") != 0 && strstr(sb.body[1], "SUB-APPEND") != 0 &&
          strstr(sb.body[1], "GAPS-MEMORY-MARK") != 0);
    CHECK(sb.nreq > 1 && strstr(sb.body[1], "{\"name\":\"Read\",") != 0 && strstr(sb.body[1], "{\"name\":\"Bash\",") == 0);
    CHECK(sb.nreq > 2 && strstr(sb.body[2], "did not finish within 1 rounds") != 0);
    repl_free(&r);
    setup_in(&r, none, root);           /* --agent with an --agents agent: the conversation as it */
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p --agents agents.json --agent rev hi", 0), 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "\"system\":[{\"type\":\"text\",\"text\":\"REV-PROMPT") != 0);
    repl_free(&r);
    remove(p);

    /* G12 --setting-sources: the user's file left out */
    xput(home, "settings.json", "{\"model\":\"haiku\"}");
    setup_in(&r, none, root);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p --setting-sources project,local hello", 0), 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "\"model\":\"claude-opus-5-5\"") != 0);
    repl_free(&r);
    setup_in(&r, none, root);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p hello", 0), 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "\"model\":\"claude-haiku-4-5") != 0);
    repl_free(&r);
    strcpy(p, home);
    strcat(p, "/settings.json");
    remove(p);

    /* G13 --betas in the anthropic-beta header; G14 --autocompact */
    setup(&r, none);
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p --betas x-test-1 x-test-2 --autocompact 500k hello", 0), 0);
    CHECK(sb.nreq == 1 && strstr(sb.head[0], "anthropic-beta: ") != 0 && strstr(sb.head[0], "x-test-1,x-test-2") != 0);
    CHECK_INT((int)(repl_compact_at(&r) / 1000), 500);
    repl_line(&r, "/autocompact 300k");     /* saved for later sessions */
    CHECK_INT((int)(repl_compact_at(&r) / 1000), 300);
    CHECK(has("home/settings.json", "\"autoCompactWindow\": 300000"));
    repl_line(&r, "/autocompact auto");
    CHECK_INT((int)(repl_compact_at(&r) / 1000), 920);
    repl_free(&r);
    remove(p);
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, "--autocompact 50 hi"), -1);
    cli_free(&c);

    /* G16 --permission-prompts none: the denial tells Claude not to retry */
    setup(&r, none);
    add_answer("toolu_B1", "Bash", "{\"command\":\"makedir NEWDIR\"}", 0);
    add_answer(0, 0, 0, "ok");
    CHECK_INT(run_print(&r, "-p --permission-prompts none run it", 0), 0);
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "Do not retry it") != 0);
    repl_free(&r);
    setup(&r, none);
    add_answer("toolu_B1", "Bash", "{\"command\":\"makedir NEWDIR\"}", 0);
    add_answer(0, 0, 0, "ok");
    CHECK_INT(run_print(&r, "-p run it", 0), 0);
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "no one can approve it") != 0);
    repl_free(&r);

    /* G17 system/api_retry before the second attempt */
    setup(&r, none);
    add_raw("HTTP/1.1 529 Overloaded\r\nContent-Type: application/json\r\nContent-Length: 75\r\n\r\n"
            "{\"type\":\"error\",\"error\":{\"type\":\"overloaded_error\",\"message\":\"Overloaded\"}}");
    add_stream("text.sse");
    CHECK_INT(run_print(&r, "-p --output-format stream-json --verbose hello", 0), 0);
    json_lines(outp(), types, sizeof(types));
    CHECK_STR(types, "system system assistant result");
    CHECK(strstr(outp(), "{\"type\":\"system\",\"subtype\":\"api_retry\",\"attempt\":1,\"max_retries\":4,"
                         "\"retry_delay_ms\":2000,\"error_status\":529,\"error\":\"overloaded\",") != 0);
    repl_free(&r);
}

/* G18 --verbose at the screen: a result unfolded in place (else three
 * lines and "+N lines (ctrl+o to expand)") */
static void gaps_verbose_run(int verbose)
{
    static const char *keys[] = { "show lines\r", "\r", "/exit\r", 0 };
    static cl_repl r;
    stub_reset();
    add_answer("toolu_V1", "Bash", "{\"command\":\"printf 'L1\\\\nL2\\\\nL3\\\\nL4\\\\nL5\\\\nLAST-LINE\\\\n'\"}", 0);
    add_answer(0, 0, 0, "Six lines.");
    cs_open(80, 30, keys);
    cs_io(&io);
    io.log = 0;
    net.u = 0;
    net.open = s_open;
    net.send = s_send;
    net.recv = s_recv;
    net.close = s_close;
    net.err = s_err;
    sys_posix_init(&sp, &sys);
    CHECK_INT(repl_init(&r, &io, &net, &sys, CL_DEFAULT_URL, "test-key-not-real", dir), 0);
    r.verbose = verbose;
    CHECK_INT(repl_screen(&r), 0);
    repl_run(&r);
    if (getenv("CL_DUMP")) {
        int k;
        for (k = 0; k < cs.rows; k++)
            printf("%2d|%s\n", k, cs_row(k));
    }
    CHECK_INT(sb.nreq, 2);
    if (verbose) {
        CHECK(cs_find("     LAST-LINE") >= 0);
        CHECK(cs_find("ctrl+o to expand") < 0);
    } else {
        CHECK(cs_find("     L4") < 0);
        CHECK(cs_find("+3 lines (ctrl+o to expand)") >= 0);
    }
    repl_free(&r);
    cs_close();
}

static void test_gaps_verbose(void)
{
    cl_cli c;
    gaps_verbose_run(0);
    gaps_verbose_run(2);
    /* the flag sets it, both syntaxes; --debug-file implies debug */
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, "--verbose --debug-file RAM:x.log"), 0);
    CHECK(c.verbose && c.debug);
    CHECK_STR(c.debug_file, "RAM:x.log");
    cli_free(&c);
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, "VERBOSE DEBUG-FILE=RAM:y.log BARE SAFE-MODE SETTING-SOURCES=user AUTOCOMPACT=1M"), 0);
    CHECK(c.verbose && c.debug && c.bare && c.safe && c.has_sources && c.sources == 1u);
    CHECK_INT((int)(c.autocompact / 1000), 1000);
    cli_free(&c);
}

/* Phase 2: the slash commands (S1-S19), typed through repl_line */
static void test_gaps_commands(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    char p[700], root[600];
    const char *sc;
    jv m, e, x;
    jit it;
    int n0;

    setup(&r, none);
    /* S1 Claude Code's commands that cannot be here say why; /help lists them */
    repl_line(&r, "/mcp");
    repl_line(&r, "/bug");
    repl_line(&r, "/install-github-app");
    repl_line(&r, "/help");
    sc = cn.screen.p ? cn.screen.p : "";
    CHECK(strstr(sc, "/mcp is not available on the Amiga: it MCP servers run as Node or Python processes") != 0);
    CHECK(strstr(sc, "/bug is not available on the Amiga: it reports go to Anthropic's feedback service") != 0);
    CHECK(strstr(sc, "/install-github-app is not available on the Amiga") != 0);
    CHECK(strstr(sc, "Not on the Amiga (type one to see why): /mcp /plugin") != 0);
    CHECK(strstr(sc, "Unknown command") == 0);
    CHECK_INT(sb.nreq, 0);

    /* a conversation first */
    add_stream("text.sse");
    repl_line(&r, "hello");
    CHECK_INT(r.conv.n, 2);
    /* S2 /btw: asked with the conversation, answered, not kept */
    add_answer(0, 0, 0, "SIDE-ANSWER");
    repl_line(&r, "/btw what was that?");
    CHECK_INT(sb.nreq, 2);
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "\"text\":\"what was that?\"") != 0 &&
          strstr(sb.body[1], "Hello from the Amiga!") != 0 && strstr(sb.body[1], "\"tool_choice\"") != 0);
    CHECK(strstr(cn.screen.p, "SIDE-ANSWER") != 0);
    CHECK_INT(r.conv.n, 2);
    /* S6 /recap */
    add_answer(0, 0, 0, "We said hello.");
    repl_line(&r, "/recap");
    CHECK(strstr(cn.screen.p, "Recap: We said hello.") != 0);
    CHECK_INT(r.conv.n, 2);
    /* S15 /rename without a name: Claude names it */
    add_answer(0, 0, 0, "Amiga Greeting Chat\n");
    repl_line(&r, "/rename");
    CHECK_STR(r.sess.title, "Amiga Greeting Chat");
    /* S3 /copy: the last answer to the clipboard (sys_posix keeps it) */
    repl_line(&r, "/copy");
    CHECK(strstr(sp.clip, "Hello from the Amiga!") == sp.clip);
    CHECK_INT((int)r.n_copies, 1);
    repl_line(&r, "/copy 2");
    CHECK(strstr(cn.screen.p, "No answer of Claude's to copy yet.") != 0);
    /* S9 /usage, /cost and /stats: the totals, per model, the time */
    repl_line(&r, "/stats");
    CHECK(strstr(cn.screen.p, "  claude-opus-5-5: ") != 0 && strstr(cn.screen.p, "Time: ") != 0);
    /* S12 /effort auto: no effort sent (the model's own); status */
    repl_line(&r, "/effort auto");
    repl_line(&r, "/effort status");
    CHECK(strstr(cn.screen.p, "Effort: auto") != 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "again");
    CHECK(sb.nreq == 5 && strstr(sb.body[4], "\"effort\"") == 0 && strstr(sb.body[0], "\"effort\"") != 0);
    repl_line(&r, "/effort medium");
    /* S16 /rewind N summarize: the last prompt on becomes a summary */
    n0 = r.conv.n;
    CHECK_INT(n0, 4);
    add_answer(0, 0, 0, "SUMMARY-OF-THE-REST");
    repl_line(&r, "/rewind 1 summarize");
    CHECK_INT(r.conv.n, 3);
    CHECK(r.conv.n == 3 && strstr(r.conv.m[2].json, "SUMMARY-OF-THE-REST") != 0 && r.conv.m[2].user);
    /* ... and "up to here": what came before the prompt becomes one */
    add_answer(0, 0, 0, "ok2");
    repl_line(&r, "and then");
    add_answer(0, 0, 0, "SUMMARY-BEFORE");
    repl_line(&r, "/rewind 1 summarize-up");
    CHECK(r.conv.n == 2 && strstr(r.conv.m[0].json, "SUMMARY-BEFORE") != 0 && strstr(r.conv.m[0].json, "and then") != 0);
    /* S10 /clear NAME: the old conversation named, the totals back to nothing */
    repl_line(&r, "/clear old talk");
    CHECK_INT(r.conv.n, 0);
    CHECK_INT((int)r.conv.requests, 0);
    {
        cl_sess_info l[8];
        int k, n = sess_list(&r.sess, l, 8), hit = 0;
        for (k = 0; k < n; k++)
            hit |= !strcmp(l[k].title, "old talk");
        CHECK(hit);
    }
    /* S5 /plan, S18 /debug, S7 /release-notes */
    repl_line(&r, "/plan");
    CHECK_INT(r.tools.perm.mode, PERM_PLAN);
    r.tools.perm.mode = PERM_DEFAULT;
    repl_line(&r, "/debug");
    CHECK_INT(r.debug, 1);
    r.debug = 0;
    repl_line(&r, "/release-notes");
    CHECK(strstr(cn.screen.p, "A4 gaps: --json-schema") != 0);
    /* S11 /config key=value */
    repl_line(&r, "/config verbose=true effortLevel=low");
    CHECK_INT(r.verbose, 1);
    CHECK_STR(r.effort, "low");
    CHECK(has("home/settings.json", "\"verbose\": true"));
    repl_line(&r, "/config verbose=false effortLevel=medium");
    /* S17 /statusline: the statusline-setup agent asked to do it; clear */
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "/statusline the model and the directory");
    CHECK(strstr(sb.body[sb.nreq - 1], "subagent_type statusline-setup") != 0 &&
          strstr(sb.body[sb.nreq - 1], "What I want: the model and the directory") != 0);
    repl_line(&r, "/statusline command echo X");
    CHECK_STR(r.cfg.status_cmd, "echo X");
    repl_line(&r, "/statusline clear");
    CHECK_STR(r.cfg.status_cmd, "");
    CHECK(!has("home/settings.json", "statusLine"));
    repl_free(&r);

    /* S4 /diff: the files Claude changed, from the checkpoints */
    strcpy(root, dir);
    strcat(root, "/gapsdiff");
    mkdir(root, 0700);
    xput(root, "a.txt", "one\ntwo\nthree\n");
    setup_in(&r, none, root);
    repl_line(&r, "/diff");
    CHECK(strstr(cn.screen.p, "No file changed by Claude in this session") != 0);
    strcpy(p, root);
    strcat(p, "/a.txt");
    cp_turn(&r.cp, 0);
    CHECK_INT(cp_before_write(&r.cp, p), 0);
    xput(root, "a.txt", "one\nTWO\nthree\n");
    repl_line(&r, "/diff");
    CHECK(strstr(cn.screen.p, "Changed: ") != 0 && strstr(cn.screen.p, "- two\n+ TWO\n") != 0);
    /* S8 /reload-skills: a skill added on disk is there */
    xput(root, ".claude/skills/new-one/SKILL.md", "---\ndescription: New\nargument-hint: [file]\n---\nX\n");
    repl_line(&r, "/reload-skills");
    CHECK(strstr(cn.screen.p, "Skills: 10 (+1)") != 0);
    /* S19 the skill is in the menu, with its argument hint */
    repl_line(&r, "/help");
    CHECK(strstr(cn.screen.p, "/new-one") != 0 && strstr(cn.screen.p, "New  [file]") != 0);
    repl_free(&r);
    (void)m;
    (void)e;
    (void)x;
    (void)it;
}

/* S14 /permissions as menus at the screen: allow Read, kept locally */
static void test_gaps_perm_menu(void)
{
    static const char *keys[] = { "/permissions\r", "1", "2", "1", "/exit\r", 0 };
    static cl_repl r;
    char root[600];
    strcpy(root, dir);
    strcat(root, "/gapsperm");
    mkdir(root, 0700);
    stub_reset();
    cs_open(80, 30, keys);
    cs_io(&io);
    io.log = 0;
    net.u = 0;
    net.open = s_open;
    net.send = s_send;
    net.recv = s_recv;
    net.close = s_close;
    net.err = s_err;
    sys_posix_init(&sp, &sys);
    CHECK_INT(repl_init(&r, &io, &net, &sys, CL_DEFAULT_URL, "test-key-not-real", root), 0);
    CHECK_INT(repl_screen(&r), 0);
    repl_run(&r);
    if (getenv("CL_DUMP")) {
        int k;
        for (k = 0; k < cs.rows; k++)
            printf("%2d|%s\n", k, cs_row(k));
    }
    CHECK_INT(cs.next, 5);
    CHECK(has("gapsperm/.claude/settings.local.json", "\"Read\""));
    CHECK(cs_find("Allow: Read") >= 0 && cs_find("saved in") >= 0);
    repl_free(&r);
    cs_close();
}

/* An ask rule's question (policy.c, through repl_ask) takes Tab's comment
 * as the tools' own question does: Yes with one -> the comment follows the
 * call's result; No with one -> Claude is told it and the turn goes on. */
static void rule_note_run(const char *const *keys, int nkeys, const char *want)
{
    static cl_repl r;
    char root[600];
    strcpy(root, dir);
    strcat(root, "/rulenote");
    mkdir(root, 0700);
    xput(root, "S/Startup-Sequence", "SetPatch QUIET\n");
    xput(root, ".claude/settings.json", "{\"permissions\":{\"ask\":[\"Read\"]}}");
    stub_reset();
    add_stream("tool_use.sse");
    add_stream("text.sse");
    cs_open(80, 30, (const char **)keys);
    cs_io(&io);
    io.log = 0;
    net.u = 0;
    net.open = s_open;
    net.send = s_send;
    net.recv = s_recv;
    net.close = s_close;
    net.err = s_err;
    sys_posix_init(&sp, &sys);
    CHECK_INT(repl_init(&r, &io, &net, &sys, CL_DEFAULT_URL, "test-key-not-real", root), 0);
    CHECK_INT(repl_screen(&r), 0);
    repl_run(&r);
    if (getenv("CL_DUMP")) {
        int k;
        for (k = 0; k < cs.rows; k++)
            printf("%2d|%s\n", k, cs_row(k));
    }
    CHECK_INT(cs.next, nkeys);
    CHECK_INT(sb.nreq, 2);
    CHECK(sb.nreq < 2 || strstr(sb.body[1], want) != 0);
    repl_free(&r);
    cs_close();
}

static void test_rule_ask_comment(void)
{
    static const char *yes[] = { "show me the startup\r", "\t", "keep it short", "\r", "/exit\r", 0 };
    static const char *no[] = { "show me the startup\r", "\033[B", "\033[B", "\t", "not that file", "\r",
                                "/exit\r", 0 };
    rule_note_run(yes, 5, "SetPatch QUIET\\n\\n\\nThe user allowed this call with a comment: keep it short");
    rule_note_run(no, 7, "\"content\":\"the user declined this tool call and said: not that file\",\"is_error\":true");
}

/* Phase 3: skills, agents, styles, the status line (X1 X3 X4 X6 X8 X9) */
static void test_gaps_ext(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    char root[600], p[700], *b = 0;
    long bn = 0;
    strcpy(root, dir);
    strcat(root, "/gapsext");
    mkdir(root, 0700);
    xput(root, "S/Startup-Sequence", "SetPatch QUIET\n");
    xput(root, ".claude/skills/echoer/SKILL.md",
         "---\ndescription: Echoes things\nwhen_to_use: WHEN-ECHO-NEEDED\nallowed-tools: Bash(printf *)\n---\n"
         "ECHO-SKILL for $0\n");
    xput(root, ".claude/skills/forked/SKILL.md",
         "---\ndescription: Looks in a subagent\ncontext: fork\nagent: Explore\n---\nFORKED-SKILL-TEXT $ARGUMENTS\n");
    xput(root, ".claude/skills/pre/SKILL.md", "---\ndescription: Preloaded\n---\nPRELOADED-SKILL-BODY\n");

    /* X1: the Skill tool expands the skill (arguments put in) and its
     * allowed-tools let Bash(echo *) run in print mode, where nobody can
     * say yes; X3: when_to_use in the Skill tool's list */
    setup_in(&r, none, root);
    add_answer("toolu_K1", "Skill", "{\"skill\":\"echoer\",\"args\":\"ARGX\"}", 0);
    add_answer("toolu_B2", "Bash", "{\"command\":\"printf SKILL-RAN\"}", 0);
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p --allowedTools Skill -- use the skill", 0), 0);
    CHECK_INT(sb.nreq, 3);
    CHECK(sb.nreq > 0 && strstr(sb.body[0], "- echoer: Echoes things WHEN-ECHO-NEEDED") != 0);
    CHECK(sb.nreq > 1 && strstr(sb.body[1], "ECHO-SKILL for ARGX") != 0);
    CHECK(sb.nreq > 2 && strstr(sb.body[2], "SKILL-RAN") != 0);
    CHECK_INT((int)r.n_skills_run, 1);
    repl_free(&r);
    setup_in(&r, none, root);               /* without the skill: denied */
    add_answer("toolu_B2", "Bash", "{\"command\":\"printf SKILL-RAN\"}", 0);
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p just run it", 0), 0);
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "no one can approve it") != 0);
    repl_free(&r);

    /* X1 context: fork: the skill runs in a subagent (its agent's prompt),
     * the report is the result */
    setup_in(&r, none, root);
    add_answer("toolu_K2", "Skill", "{\"skill\":\"forked\",\"args\":\"S:\"}", 0);
    add_answer(0, 0, 0, "FORK-REPORT");
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p --allowedTools Skill -- fork it", 0), 0);
    CHECK_INT(sb.nreq, 3);
    CHECK(sb.nreq > 1 && strstr(sb.body[1], "file search specialist") != 0 &&
          strstr(sb.body[1], "FORKED-SKILL-TEXT S:") != 0);
    CHECK(sb.nreq > 2 && strstr(sb.body[2], "FORK-REPORT") != 0);
    repl_free(&r);

    /* X4 an agent's effort, preloaded skills, permissionMode plan (its Edit refused) */
    xput(root, "ag.json", "{\"pl\":{\"description\":\"Plans\",\"prompt\":\"PL-PROMPT\",\"effort\":\"low\","
                          "\"skills\":[\"pre\"],\"permissionMode\":\"plan\"}}");
    xput(root, "f.txt", "x\n");
    setup_in(&r, none, root);
    add_answer("toolu_T3", "Task", "{\"description\":\"P\",\"prompt\":\"Plan it\",\"subagent_type\":\"pl\"}", 0);
    add_answer("toolu_W3", "Write", "{\"file_path\":\"new.txt\",\"content\":\"y\"}", 0);
    add_answer(0, 0, 0, "planned");
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p --agents ag.json plan", 0), 0);
    CHECK_INT(sb.nreq, 4);
    CHECK(sb.nreq > 1 && strstr(sb.body[1], "\"effort\":\"low\"") != 0 &&
          strstr(sb.body[1], "# Skill: pre") != 0 && strstr(sb.body[1], "PRELOADED-SKILL-BODY") != 0);
    CHECK(sb.nreq > 2 && strstr(sb.body[2], "plan mode") != 0);
    strcpy(p, root);
    strcat(p, "/new.txt");
    CHECK(!exists(p));
    CHECK_INT(r.tools.perm.mode, PERM_DEFAULT);     /* the parent's own mode untouched */
    repl_free(&r);

    /* X6 a deny rule naming Agent covers the Task tool */
    xput(root, "deny.json", "{\"permissions\":{\"deny\":[\"Agent(pl)\"]}}");
    setup_in(&r, none, root);
    add_answer("toolu_T4", "Task", "{\"description\":\"P\",\"prompt\":\"Plan it\",\"subagent_type\":\"pl\"}", 0);
    add_answer(0, 0, 0, "denied then");
    CHECK_INT(run_print(&r, "-p --agents ag.json --settings deny.json plan", 0), 0);
    CHECK_INT(sb.nreq, 2);
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "\"is_error\":true") != 0);
    repl_free(&r);

    /* X8 the built-in styles Proactive and Concise; a custom style without
     * keep-coding-instructions drops the way-of-working part; the setting
     * is case-sensitive */
    xput(root, ".claude/output-styles/plain.md", "---\nname: Plain\ndescription: x\n---\nPLAIN-STYLE\n");
    xput(root, ".claude/output-styles/keep.md",
         "---\nname: Keep\ndescription: x\nkeep-coding-instructions: true\n---\nKEEP-STYLE\n");
    setup_in(&r, none, root);
    repl_line(&r, "/output-style Concise");
    CHECK(strstr(r.system, "Lead every response with the result") != 0);
    repl_line(&r, "/output-style Proactive");
    CHECK(strstr(r.system, "Start on a task as soon as it is given") != 0);
    repl_line(&r, "/output-style Plain");
    CHECK(strstr(r.system, "PLAIN-STYLE") != 0 && strstr(r.system, "task list with the task tools") == 0);
    repl_line(&r, "/output-style Keep");
    CHECK(strstr(r.system, "KEEP-STYLE") != 0 && strstr(r.system, "task list with the task tools") != 0);
    repl_free(&r);
    xput(root, ".claude/settings.local.json", "{\"outputStyle\":\"concise\"}");
    setup_in(&r, none, root);
    CHECK_STR(r.style, "");                 /* "concise" is no style's exact name: Default */
    repl_free(&r);
    xput(root, ".claude/settings.local.json", "{\"outputStyle\":\"Concise\"}");
    setup_in(&r, none, root);
    CHECK_STR(r.style, "Concise");
    repl_free(&r);
    strcpy(p, root);
    strcat(p, "/.claude/settings.local.json");
    remove(p);

    /* X9 the status line's JSON: Claude Code's fields */
    strcpy(p, "cat > ");
    strcat(p, root);
    strcat(p, "/status-in.json\necho OK\n");
    xput(root, "st.sh", p);
    strcpy(p, "{\"statusLine\":{\"type\":\"command\",\"command\":\"sh ");
    strcat(p, root);
    strcat(p, "/st.sh\"}}");
    xput(root, ".claude/settings.json", p);
    setup_in(&r, none, root);
    sess_rename(&r.sess, "My Session");
    pol_statusline(&r);
    CHECK_STR(r.status_text, "OK");
    strcpy(p, root);
    strcat(p, "/status-in.json");
    CHECK_INT(sys.read(sys.u, p, 100000, &b, &bn), 0);
    if (b) {
        CHECK(strstr(b, "\"model\":{\"id\":\"claude-opus-5-5\",\"display_name\":\"Opus 5.5\"}") != 0);
        CHECK(strstr(b, "\"total_lines_added\":0") != 0 && strstr(b, "\"total_api_duration_ms\":") != 0);
        CHECK(strstr(b, "\"context_window\":{\"context_window_size\":1000000,") != 0);
        CHECK(strstr(b, "\"used_percentage\":0") != 0 && strstr(b, "\"remaining_percentage\":100") != 0);
        CHECK(strstr(b, "\"effort\":{\"level\":\"medium\"}") != 0 && strstr(b, "\"thinking\":{\"enabled\":true}") != 0);
        CHECK(strstr(b, "\"session_name\":\"My Session\"") != 0 && strstr(b, "\"version\":") != 0);
        CHECK(strstr(b, "\"project_dir\":") != 0);
    }
    free(b);
    repl_free(&r);
    strcpy(p, root);
    strcat(p, "/.claude/settings.json");
    remove(p);
}

/* a hook's settings line: event, matcher, an "if" rule (0 none), the script */
static void hook_json(jw *w, const char *event, const char *matcher, const char *cond, const char *root,
                      const char *script)
{
    if (w->n && w->p[w->n - 1] != '{')
        jw_raw(w, ",", 1);
    jw_strz(w, event);
    jw_rawz(w, ":[{\"matcher\":");
    jw_strz(w, matcher);
    jw_rawz(w, ",\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    jw_rawz(w, root);
    jw_raw(w, "/", 1);
    jw_rawz(w, script);
    jw_raw(w, "\"", 1);
    if (cond) {
        jw_rawz(w, ",\"if\":");
        jw_strz(w, cond);
    }
    jw_rawz(w, "}]}]");
}

static int marker(const char *root, const char *name)
{
    char p[700];
    strcpy(p, root);
    strcat(p, "/");
    strcat(p, name);
    return exists(p);
}

/* Phase 4: hooks (H1-H4), Bash (H6 H7), Read media (H8), memory (M1-M4) */
static void test_gaps_hooks(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    char root[600], p[900], m[700];
    jw w;
    strcpy(root, dir);
    strcat(root, "/gapshk");
    mkdir(root, 0700);
    xput(root, "S/Startup-Sequence", "SetPatch QUIET\n");
    /* the scripts: each leaves a marker; some answer with JSON or exit 2 */
#define SCRIPT(name, body)                                                                                         \
    do {                                                                                                           \
        strcpy(m, "cd ");                                                                                          \
        strcat(m, root);                                                                                           \
        strcat(m, "\n");                                                                                           \
        strcat(m, body);                                                                                           \
        xput(root, name, m);                                                                                       \
    } while (0)
    SCRIPT("fail.sh", "cat > m-failure.json\n");
    SCRIPT("perm.sh", "cat > m-perm.json\necho '{\"hookSpecificOutput\":{\"hookEventName\":\"PermissionRequest\","
                      "\"decision\":{\"behavior\":\"allow\"}}}'\n");
    SCRIPT("sub.sh", "cat > m-substart.json\necho '{\"hookSpecificOutput\":{\"hookEventName\":\"SubagentStart\","
                     "\"additionalContext\":\"SUBSTART-CONTEXT\"}}'\n");
    SCRIPT("cwd.sh", "cat > m-cwd.json\n");
    SCRIPT("dir.sh", "cat > m-dir.json\n");
    SCRIPT("exp.sh", "cat > m-exp.json\necho no expansion today\nexit 2\n");
    SCRIPT("ifblock.sh", "cat > m-if.json\necho blocked by if\nexit 2\n");
    SCRIPT("pre.sh", "echo '{\"systemMessage\":\"SYSTEM-MESSAGE-SHOWN\",\"hookSpecificOutput\":{\"hookEventName\":"
                     "\"PreToolUse\",\"additionalContext\":\"PRE-CONTEXT\",\"updatedInput\":{\"file_path\":"
                     "\"S/Startup-Sequence\"}}}'\n");
    SCRIPT("pd.sh", "echo \"$CLAUDE_PROJECT_DIR\" > m-pd.txt\n");
    SCRIPT("end.sh", "cat > m-end.json\n");
    SCRIPT("stopf.sh", "cat > m-stopfail.json\n");
#undef SCRIPT
    jw_init(&w);
    jw_rawz(&w, "{\"hooks\":{");
    hook_json(&w, "PostToolUseFailure", "Read", 0, root, "fail.sh");
    hook_json(&w, "PermissionRequest", "Bash", 0, root, "perm.sh");
    hook_json(&w, "SubagentStart", "general-purpose", 0, root, "sub.sh");
    hook_json(&w, "CwdChanged", "", 0, root, "cwd.sh");
    hook_json(&w, "DirectoryAdded", "slash_command", 0, root, "dir.sh");
    hook_json(&w, "UserPromptExpansion", "hi", 0, root, "exp.sh");
    hook_json(&w, "SessionEnd", "prompt_input_exit", 0, root, "end.sh");
    hook_json(&w, "StopFailure", "invalid_request", 0, root, "stopf.sh");
    jw_rawz(&w, ",\"PreToolUse\":[{\"matcher\":\"Bash\",\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    jw_rawz(&w, root);
    jw_rawz(&w, "/ifblock.sh\",\"if\":\"Bash(makedir *)\"}]},{\"matcher\":\"Read\",\"hooks\":[{\"type\":\"command\","
                "\"command\":\"sh ");
    jw_rawz(&w, root);
    jw_rawz(&w, "/pre.sh\"},{\"type\":\"command\",\"command\":\"sh ${CLAUDE_PROJECT_DIR}/pd.sh\"}]}]");
    jw_rawz(&w, "}}");
    xput(root, ".claude/settings.json", w.p);
    jw_free(&w);
    xput(root, ".claude/commands/hi.md", "Say HI.\n");

    /* H1 PostToolUseFailure on a failed Read (not PostToolUse); H3 the
     * PreToolUse answer: updatedInput (the Read reads another file),
     * additionalContext for Claude, systemMessage for the user; H4
     * CLAUDE_PROJECT_DIR set and substituted */
    setup_in(&r, none, root);
    add_answer("toolu_R9", "Read", "{\"file_path\":\"nothere.txt\"}", 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "read it");
    CHECK_INT(sb.nreq, 2);
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "SetPatch QUIET") != 0);       /* the updated input's file */
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "PRE-CONTEXT") != 0);
    CHECK(strstr(cn.screen.p, "SYSTEM-MESSAGE-SHOWN") != 0);
    CHECK(!marker(root, "m-failure.json"));                              /* it did not fail after all */
    strcpy(p, root);
    strcat(p, "/m-pd.txt");
    {
        char *b = 0;
        long bn = 0;
        CHECK_INT(sys.read(sys.u, p, 1000, &b, &bn), 0);
        CHECK(b && strstr(b, "/gapshk") != 0);     /* the project's directory (canonical) */
        free(b);
    }
    repl_free(&r);
    /* ... a Read the hook does not rewrite (Glob is no Read): PostToolUseFailure on the failure */
    xput(root, ".claude/settings.local.json", "{\"hooks\":{\"PostToolUseFailure\":[{\"matcher\":\"Grep\",\"hooks\":"
                                               "[{\"type\":\"command\",\"command\":\"echo FAILURE-SEEN\"}]}]}}");
    setup_in(&r, none, root);
    add_answer("toolu_G9", "Grep", "{\"pattern\":\"(\",\"path\":\"nowhere\"}", 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "grep it");
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "\"is_error\":true") != 0);
    CHECK_INT((int)r.hooks.n_run, 1);
    repl_free(&r);
    strcpy(p, root);
    strcat(p, "/.claude/settings.local.json");
    remove(p);

    /* H2 "if": the PreToolUse hook runs for Bash(makedir *) only, and blocks it;
     * H1 PermissionRequest: its allow answers the question in print mode */
    setup_in(&r, none, root);
    add_answer("toolu_B5", "Bash", "{\"command\":\"makedir NEWDIR\"}", 0);
    add_answer(0, 0, 0, "ok");
    CHECK_INT(run_print(&r, "-p make it", 0), 0);
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "blocked by if") != 0);
    CHECK(marker(root, "m-if.json"));
    repl_free(&r);
    setup_in(&r, none, root);
    strcpy(p, "{\"command\":\"touch ");
    strcat(p, root);
    strcat(p, "/NEWFILE\"}");
    add_answer("toolu_B6", "Bash", p, 0);
    add_answer(0, 0, 0, "ok");
    CHECK_INT(run_print(&r, "-p touch it", 0), 0);
    CHECK(marker(root, "m-perm.json"));         /* asked the hook, not nobody */
    CHECK(marker(root, "NEWFILE"));             /* and it ran */
    repl_free(&r);

    /* H1 SubagentStart: its additionalContext in the agent's system prompt */
    setup_in(&r, none, root);
    add_answer("toolu_T9", "Task", "{\"description\":\"x\",\"prompt\":\"Look\",\"subagent_type\":\"general-purpose\"}",
               0);
    add_answer(0, 0, 0, "agent done");
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p go", 0), 0);
    CHECK(sb.nreq > 1 && strstr(sb.body[1], "SUBSTART-CONTEXT") != 0);
    CHECK(marker(root, "m-substart.json"));
    repl_free(&r);

    /* H1 CwdChanged (/cd), DirectoryAdded (/add-dir), UserPromptExpansion
     * (exit 2 blocks /hi), StopFailure (an API error), SessionEnd's reason */
    setup_in(&r, none, root);
    repl_line(&r, "/add-dir S");
    CHECK(marker(root, "m-dir.json"));
    repl_line(&r, "/hi");
    CHECK_INT(sb.nreq, 0);
    CHECK(strstr(cn.screen.p, "A UserPromptExpansion hook blocked /hi") != 0);
    add_raw("HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\nContent-Length: 82\r\n\r\n"
            "{\"type\":\"error\",\"error\":{\"type\":\"invalid_request_error\",\"message\":\"bad request!\"}}");
    repl_line(&r, "fail please");
    CHECK(marker(root, "m-stopfail.json"));
    repl_line(&r, "/exit");
    repl_free(&r);
    CHECK(marker(root, "m-end.json"));
    setup_in(&r, none, root);
    repl_line(&r, "/cd S");
    CHECK(marker(root, "m-cwd.json"));
    repl_free(&r);

    /* H8 Read of an image and a PDF: base64 blocks for Claude */
    xput(root, "pic.png", "\211PNG\r\n\032\nIHDRxxxx");
    xput(root, "doc.pdf", "%PDF-1.4 x");
    strcpy(p, root);
    strcat(p, "/.claude/settings.json");
    remove(p);
    setup_in(&r, none, root);
    add_answer("toolu_P1", "Read", "{\"file_path\":\"pic.png\"}", 0);
    add_answer("toolu_P2", "Read", "{\"file_path\":\"doc.pdf\"}", 0);
    add_answer(0, 0, 0, "a picture and a document");
    CHECK_INT(run_print(&r, "-p look", 0), 0);
    CHECK(sb.nreq == 3 && strstr(sb.body[1], "\"content\":[{\"type\":\"image\",\"source\":{\"type\":\"base64\","
                                             "\"media_type\":\"image/png\",\"data\":\"iVBORw0KGgpJSERSeHh4eA==\"}}]") != 0);
    CHECK(sb.nreq == 3 && strstr(sb.body[2], "{\"type\":\"document\",\"source\":{\"type\":\"base64\",\"media_type\":"
                                             "\"application/pdf\",\"data\":\"JVBERi0xLjQgeA==\"}") != 0);
    repl_free(&r);

    /* H6 Bash's default time, from BASH_DEFAULT_TIMEOUT_MS */
    setup_in(&r, none, root);
    CHECK_INT(r.tools.timeout_s, 120);
    repl_free(&r);
    setenv("BASH_DEFAULT_TIMEOUT_MS", "30000", 1);
    setenv("BASH_MAX_TIMEOUT_MS", "90000", 1);
    setup_in(&r, none, root);
    CHECK_INT(r.tools.timeout_s, 30);
    CHECK_INT((int)r.tools.max_timeout_ms, 90000);
    repl_free(&r);
    unsetenv("BASH_DEFAULT_TIMEOUT_MS");
    unsetenv("BASH_MAX_TIMEOUT_MS");

    /* M1-M4: HTML comments out, rules (always / by paths:), claudeMdExcludes, auto memory */
    xput(root, "CLAUDE.md", "VISIBLE-MEMORY\n<!-- HIDDEN-COMMENT\nstill hidden -->\nAFTER-COMMENT\n");
    xput(root, "CLAUDE.local.md", "LOCAL-EXCLUDED\n");
    xput(root, ".claude/rules/always.md", "RULE-ALWAYS\n");
    xput(root, ".claude/rules/sub/scoped.md", "---\npaths:\n  - \"S/*\"\n---\nRULE-FOR-S\n");
    strcpy(p, "{\"claudeMdExcludes\":[\"**/CLAUDE.local.md\"]}");
    xput(root, ".claude/settings.json", p);
    setup_in(&r, none, root);
    CHECK(strstr(r.system, "VISIBLE-MEMORY") != 0 && strstr(r.system, "AFTER-COMMENT") != 0);
    CHECK(strstr(r.system, "HIDDEN-COMMENT") == 0 && strstr(r.system, "still hidden") == 0);
    CHECK(strstr(r.system, "RULE-ALWAYS") != 0 && strstr(r.system, "RULE-FOR-S") == 0);
    CHECK(strstr(r.system, "LOCAL-EXCLUDED") == 0);
    CHECK(strstr(r.system, "# Auto memory") != 0);
    add_answer("toolu_R7", "Read", "{\"file_path\":\"S/Startup-Sequence\"}", 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "read the startup");
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "RULE-FOR-S") != 0);
    /* auto memory: MEMORY.md read at the start; Claude writes in its directory unasked */
    cl_copy(m, r.mem.auto_dir, sizeof(m));
    repl_free(&r);
    {
        char mp[800];
        strcpy(mp, m);
        strcat(mp, "/MEMORY.md");
        CHECK(m[0] != 0);
        mkdir(m, 0700);
        {
            FILE *f = fopen(mp, "wb");
            if (f) {
                fputs("- AUTO-MEMORY-FACT\n", f);
                fclose(f);
            }
        }
        setup_in(&r, none, root);
        CHECK(strstr(r.system, "AUTO-MEMORY-FACT") != 0);
        strcpy(p, "{\"file_path\":\"/");    /* AmigaDOS: a leading / is the start directory's parent: <dir> */
        strcat(p, m + strlen(dir) + 1);
        strcat(p, "/topic.md\",\"content\":\"x\"}");
        add_answer("toolu_W7", "Write", p, 0);
        add_answer(0, 0, 0, "saved");
        CHECK_INT(run_print(&r, "-p remember", 0), 0);
        strcpy(mp, m);
        strcat(mp, "/topic.md");
        CHECK(exists(mp));
        repl_free(&r);
    }
    xput(root, ".claude/settings.json", "{\"autoMemoryEnabled\":false}");
    setup_in(&r, none, root);
    CHECK(strstr(r.system, "# Auto memory") == 0 && strstr(r.system, "AUTO-MEMORY-FACT") == 0);
    repl_free(&r);

    /* H4 PreCompact can block; H3 continue:false on PostToolUse ends the turn after the round */
    xput(root, "noc.sh", "echo not now\nexit 2\n");
    xput(root, "halt.sh", "echo '{\"continue\":false,\"stopReason\":\"HALTED-BY-HOOK\"}'\n");
    strcpy(p, "{\"hooks\":{\"PreCompact\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    strcat(p, root);
    strcat(p, "/noc.sh\"}]}],\"PostToolUse\":[{\"matcher\":\"Read\",\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    strcat(p, root);
    strcat(p, "/halt.sh\"}]}]}}");
    xput(root, ".claude/settings.json", p);
    setup_in(&r, none, root);
    add_answer("toolu_R8", "Read", "{\"file_path\":\"S/Startup-Sequence\"}", 0);
    add_answer(0, 0, 0, "never asked for");
    repl_line(&r, "read and halt");
    CHECK_INT(sb.nreq, 1);                  /* no request after the round */
    CHECK(strstr(cn.screen.p, "HALTED-BY-HOOK") != 0);
    repl_line(&r, "/compact");
    CHECK_INT(sb.nreq, 1);
    CHECK(strstr(cn.screen.p, "A PreCompact hook blocked the compaction: not now") != 0);
    repl_line(&r, "/memory auto off");
    CHECK(has("home/settings.json", "\"autoMemoryEnabled\": false"));
    repl_free(&r);
    strcpy(p, dir);
    strcat(p, "/home/settings.json");
    cfg_write_key(&sys, p, "autoMemoryEnabled", 0);
    strcpy(p, root);
    strcat(p, "/.claude/settings.json");
    remove(p);
}

/* a file's time set seconds before now (cleanupPeriodDays, ConfigChange) */
static void age_file(const char *p, long secs)
{
    struct utimbuf u;
    struct stat st;
    if (stat(p, &st))
        return;
    u.actime = st.st_atime - secs;
    u.modtime = st.st_mtime - secs;
    utime(p, &u);
}

/* w holds s alone (the tests' paths and settings, of any start directory's length) */
static void pset(jw *w, const char *s)
{
    jw_reset(w);
    jw_rawz(w, s);
}

/* Phase 5: the rest the Amiga can do (P1-P8) */
static void test_gaps_more(void)
{
    static const char *none[] = { 0 };
    static const char *yes[] = { "y", 0 };
    static const char *proj[] = { "p", 0 };
    static cl_repl r;
    char root[600], m[700], home0[600];
    const char *env;
    cl_cli c;
    cl_net web;
    jw pw;                      /* settings with the root in them five times: no fixed size holds them */
    jw_init(&pw);
    strcpy(root, dir);
    strcat(root, "/gapsmore");
    mkdir(root, 0700);
    xput(root, "S/Startup-Sequence", "SetPatch QUIET\n");
#define SCRIPT(name, body)                                                                                         \
    do {                                                                                                           \
        strcpy(m, "cd ");                                                                                          \
        strcat(m, root);                                                                                           \
        strcat(m, "\n");                                                                                           \
        strcat(m, body);                                                                                           \
        xput(root, name, m);                                                                                       \
    } while (0)
#define HOOKS(json)                                                                                                \
    do {                                                                                                           \
        xput(root, ".claude/settings.json", json);                                                                 \
    } while (0)
    SCRIPT("mark.sh", "cat > m-$1.json\n");
    SCRIPT("envf.sh", "echo 'export GAPS_ENV_VAR=from-hook' >> \"$CLAUDE_ENV_FILE\"\n");
    SCRIPT("batch.sh", "echo '{\"decision\":\"block\",\"reason\":\"BATCH-STOP\"}'\n");
    SCRIPT("nohaiku.sh", "echo no haiku here\nexit 2\n");

    /* P2 a prompt hook on Stop: the small model says not yet, Claude goes on with its reason */
    pset(&pw, "{\"hooks\":{\"Stop\":[{\"hooks\":[{\"type\":\"prompt\",\"prompt\":\"Done? $ARGUMENTS\"}]}]}}");
    HOOKS(pw.p);
    setup_in(&r, none, root);
    add_answer(0, 0, 0, "first answer");
    add_answer(0, 0, 0, "{\"ok\": false, \"reason\": \"KEEP-GOING\"}");
    add_answer(0, 0, 0, "second answer");
    add_answer(0, 0, 0, "{\"ok\": true}");
    repl_line(&r, "work");
    CHECK_INT(sb.nreq, 4);
    CHECK(sb.nreq > 1 && strstr(sb.body[1], "claude-haiku-4-5") != 0 && strstr(sb.body[1], "Done? {") != 0 &&
          strstr(sb.body[1], "hook_event_name") != 0);
    CHECK(sb.nreq > 2 && strstr(sb.body[2], "KEEP-GOING") != 0);
    repl_free(&r);

    /* P2 once, matchers (a plain list, a regular expression), CLAUDE_ENV_FILE,
     * InstructionsLoaded, Notification's types, PostToolBatch */
    xput(root, "CLAUDE.md", "MEM\n");
    pset(&pw, "{\"hooks\":{\"PostToolUse\":[{\"matcher\":\"Grep, Read\",\"hooks\":[{\"type\":\"command\",\"once\":true,"
              "\"statusMessage\":\"Checking\",\"command\":\"sh ");
    jw_rawz(&pw, root);
    jw_rawz(&pw, "/mark.sh once\"}]}],\"SessionStart\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    jw_rawz(&pw, root);
    jw_rawz(&pw, "/envf.sh\"}]}],\"InstructionsLoaded\":[{\"matcher\":\"session_start\",\"hooks\":[{\"type\":\"command\","
              "\"command\":\"sh ");
    jw_rawz(&pw, root);
    jw_rawz(&pw, "/mark.sh instr\"}]}],\"Notification\":[{\"matcher\":\"^idle_\",\"hooks\":[{\"type\":\"command\","
              "\"command\":\"sh ");
    jw_rawz(&pw, root);
    jw_rawz(&pw, "/mark.sh idle\"}]}],\"PostToolBatch\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    jw_rawz(&pw, root);
    jw_rawz(&pw, "/batch.sh\"}]}]}}");
    HOOKS(pw.p);
    setup_in(&r, none, root);
    add_answer("toolu_A1", "Read", "{\"file_path\":\"S/Startup-Sequence\"}", 0);
    add_answer(0, 0, 0, "never");
    repl_line(&r, "read twice");
    CHECK_INT(sb.nreq, 1);                          /* PostToolBatch blocked the next request */
    CHECK(marker(root, "m-once.json") && marker(root, "m-instr.json"));
    env = getenv("GAPS_ENV_VAR");
    CHECK(env && !strcmp(env, "from-hook"));
    add_answer("toolu_A2", "Read", "{\"file_path\":\"S/Startup-Sequence\"}", 0);
    add_answer(0, 0, 0, "x");
    pset(&pw, root);
    jw_rawz(&pw, "/m-once.json");
    remove(pw.p);
    repl_line(&r, "again");
    CHECK(sb.nreq >= 2 && strstr(sb.body[1], "BATCH-STOP") != 0);     /* Claude was told why */
    CHECK(!marker(root, "m-once.json"));            /* once: not again */
    r.idle_from = 1;
    cn.clock += 70000;
    pol_status_tick(&r);
    CHECK(marker(root, "m-idle.json"));
    repl_free(&r);
    unsetenv("GAPS_ENV_VAR");

    /* P2 Pre/PostModelSwitch; ConfigChange reads a changed file again */
    pset(&pw, "{\"hooks\":{\"PreModelSwitch\":[{\"matcher\":\".*haiku.*\",\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    jw_rawz(&pw, root);
    jw_rawz(&pw, "/nohaiku.sh\"}]}],\"PostModelSwitch\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    jw_rawz(&pw, root);
    jw_rawz(&pw, "/mark.sh postswitch\"}]}]}}");
    HOOKS(pw.p);
    setup_in(&r, none, root);
    repl_line(&r, "/model haiku");
    CHECK_STR(r.model, "claude-opus-5-5");
    CHECK(strstr(cn.screen.p, "A PreModelSwitch hook blocked the switch: no haiku here") != 0);
    repl_line(&r, "/model sonnet");
    CHECK_STR(r.model, "claude-sonnet-5-5");
    CHECK(marker(root, "m-postswitch.json"));
    unset_home_model();
    pset(&pw, root);
    jw_rawz(&pw, "/.claude/settings.json");
    age_file(pw.p, 100);
    r.cfg_mtime[CFG_PROJECT] = 0;               /* as if read before the file changed */
    repl_line(&r, "/status");
    CHECK(strstr(cn.screen.p, "Settings changed on disk, read again:") != 0);
    repl_free(&r);
    remove(pw.p);

    /* P2 Setup (--init-only) and --include-hook-events; P6 --prompt-suggestions,
     * --exclude-dynamic-system-prompt-sections */
    pset(&pw, "{\"hooks\":{\"Setup\":[{\"matcher\":\"init\",\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    jw_rawz(&pw, root);
    jw_rawz(&pw, "/mark.sh setup\"}]}],\"SessionStart\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"echo STARTED\"}]}]}}");
    HOOKS(pw.p);
    setup_in(&r, none, root);
    CHECK_INT(run_print(&r, "-p --init-only", 0), 0);
    CHECK(marker(root, "m-setup.json"));
    CHECK_INT(sb.nreq, 0);
    repl_free(&r);
    setup_in(&r, none, root);
    add_answer(0, 0, 0, "hello there");
    add_answer(0, 0, 0, "what next?");
    CHECK_INT(run_print(&r, "-p --output-format stream-json --verbose --include-hook-events --prompt-suggestions "
                            "--exclude-dynamic-system-prompt-sections hi",
                        0),
              0);
    CHECK(strstr(outp(), "{\"type\":\"system\",\"subtype\":\"hook_started\",\"hook_name\":\"echo STARTED\"") != 0);
    CHECK(strstr(outp(), "\"subtype\":\"hook_response\"") != 0 && strstr(outp(), "STARTED") != 0);
    CHECK(strstr(outp(), "{\"type\":\"prompt_suggestion\",\"suggestion\":\"what next?\"") != 0);
    CHECK(sb.nreq >= 1 && strstr(sb.body[0], "\"system\":") != 0);
    if (sb.nreq >= 1) {
        const char *msgs = strstr(sb.body[0], "\"messages\":");
        CHECK(msgs && strstr(msgs, "# Auto memory") != 0);       /* with the first prompt ... */
        CHECK(strstr(r.system, "# Auto memory") == 0);            /* ... not in the system prompt */
    }
    repl_free(&r);
    pset(&pw, root);
    jw_rawz(&pw, "/.claude/settings.json");
    remove(pw.p);

    /* P3 "don't ask again in this project" kept as a rule; ~/ in rules; apiKeyHelper,
     * availableModels, bashOutputMaxChars, cleanupPeriodDays */
    setup_in(&r, proj, root);
    add_answer("toolu_B7", "Bash", "{\"command\":\"makedir NEWDIR\"}", 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "make a dir");
    CHECK(has("gapsmore/.claude/settings.local.json", "\"Bash(makedir *)\""));
    repl_free(&r);
    pset(&pw, root);
    jw_rawz(&pw, "/.claude/settings.local.json");
    remove(pw.p);
    pset(&pw, "{\"apiKeyHelper\":\"echo sk-from-helper-1234\",\"availableModels\":[\"sonnet\"],"
              "\"bashOutputMaxChars\":100,\"cleanupPeriodDays\":5,\"permissions\":{\"deny\":[\"Read(~/secret.txt)\"]}}");
    HOOKS(pw.p);
    xput(root, "secret.txt", "SECRET\n");
    {
        char real[700];
        const char *h0 = getenv("HOME");
        cl_copy(home0, h0 ? h0 : "", sizeof(home0));
        if (realpath(root, real))
            setenv("HOME", real, 1);    /* the start directory as the tools see it (canonical) */
    }
    setup_in(&r, none, root);
    CHECK(r.key && !strcmp(r.key, "sk-from-helper-1234"));
    {
        /* an old session file of this project: gone at the start (the first line) */
        char old[700];
        strcpy(old, r.sess.dir);
        strcat(old, "/0000beef.jsonl");
        mkdir(r.sess.dir, 0700);
        xput(r.sess.dir, "0000beef.jsonl", "{}\n");
        age_file(old, 10L * 86400);
        repl_line(&r, "/model opus");
        CHECK_STR(r.model, "claude-opus-5-5");
        CHECK(strstr(cn.screen.p, "Not in availableModels") != 0);
        add_answer("toolu_B8", "Bash", "{\"command\":\"printf '%0200d' 0\"}", 0);
        add_answer("toolu_R8", "Read", "{\"file_path\":\"secret.txt\"}", 0);
        add_answer(0, 0, 0, "ok");
        CHECK_INT(run_print(&r, "-p --allowedTools Bash -- go", 0), 0);
        CHECK(!exists(old));
        CHECK(sb.nreq >= 2 && strstr(sb.body[1], "Output too large (200 characters). Full output saved to: ") != 0);
        CHECK(sb.nreq >= 3 && strstr(sb.body[2], "has been denied by the rule Read(~/secret.txt)") != 0);
    }
    repl_free(&r);
    if (home0[0])
        setenv("HOME", home0, 1);
    else
        unsetenv("HOME");
    remove(pw.p);
    pset(&pw, root);
    jw_rawz(&pw, "/.claude/settings.json");
    remove(pw.p);

    /* P4 AGENTS.md only where no CLAUDE.md is; CLAUDE.local.md in a subdirectory */
    xput(root, "AGENTS.md", "ROOT-AGENTS\n");
    xput(root, "sub/AGENTS.md", "SUB-AGENTS\n");
    xput(root, "sub/CLAUDE.local.md", "SUB-LOCAL\n");
    xput(root, "sub/f.txt", "x\n");
    setup_in(&r, none, root);
    CHECK(strstr(r.system, "ROOT-AGENTS") == 0);    /* CLAUDE.md is there */
    add_answer("toolu_R9", "Read", "{\"file_path\":\"sub/f.txt\"}", 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "read sub");
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "SUB-AGENTS") != 0 && strstr(sb.body[1], "SUB-LOCAL") != 0);
    /* P5 the built-in agents */
    CHECK(tools_agent(&r.tools, "claude-code-guide") != 0 && tools_agent(&r.tools, "claude") != 0);
    /* P1 a bundled skill typed; P7 /skills with a filter, /skill-doctor, /context's categories */
    add_answer(0, 0, 0, "simplified");
    repl_line(&r, "/simplify");
    CHECK(strstr(sb.body[sb.nreq - 1], "Review the code changed in this session for cleanup") != 0);
    repl_line(&r, "/skills run");
    CHECK(strstr(cn.screen.p, "  run (built-in)") != 0 && strstr(cn.screen.p, "  insights (built-in)") == 0);
    repl_line(&r, "/skill-doctor");
    CHECK(strstr(cn.screen.p, "No skills of yours (the bundled ones are not counted)") != 0);
    repl_line(&r, "/context");
    CHECK(strstr(cn.screen.p, "By category (estimated):") != 0 && strstr(cn.screen.p, "  Memory files") != 0);
    /* P7 /goal: a small model judges after the turn; not met: one more turn */
    add_answer(0, 0, 0, "half done");
    add_answer(0, 0, 0, "NOT MET: the second half is missing");
    add_answer(0, 0, 0, "all done");
    add_answer(0, 0, 0, "MET");
    {
        int n0 = sb.nreq;
        repl_line(&r, "/goal both halves are done");
        CHECK_INT(sb.nreq - n0, 4);
        CHECK(strstr(sb.body[n0 + 2], "Keep working toward the goal: both halves are done") != 0);
        CHECK_STR(r.goal, "");
    }
    repl_free(&r);

    /* P8 Bash: a cd persists */
    setup_in(&r, none, root);
    add_answer("toolu_C1", "Bash", "{\"command\":\"cd S\"}", 0);
    add_answer("toolu_C2", "Bash", "{\"command\":\"pwd\"}", 0);
    add_answer(0, 0, 0, "ok");
    CHECK_INT(run_print(&r, "-p go", 0), 0);
    CHECK(sb.nreq == 3 && strstr(sb.body[2], "gapsmore/S\\n") != 0);
    repl_free(&r);

    /* P8 WebFetch: the same page within 15 minutes is not fetched again */
    setup_in(&r, none, root);
    web.u = 0;
    web.open = wp_open;
    web.send = wp_send;
    web.recv = wp_recv;
    web.close = wp_close;
    web.err = s_err;
    r.tools.web = &web;
    free(r.tools.json);
    r.tools.json = 0;
    wp2_open_n = 0;
    add_answer("toolu_F1", "WebFetch", "{\"url\":\"http://127.0.0.1:8080/page\",\"prompt\":\"what is it\"}", 0);
    add_answer(0, 0, 0, "a page");
    add_answer("toolu_F2", "WebFetch", "{\"url\":\"http://127.0.0.1:8080/page\",\"prompt\":\"and again\"}", 0);
    add_answer(0, 0, 0, "the same page");
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p --allowedTools WebFetch -- fetch twice", 0), 0);
    CHECK_INT(wp2_open_n, 1);
    CHECK_INT((int)r.tools.n_fetch_cached, 1);
    repl_free(&r);

    /* P6 the subcommands: doctor, auth status, purge */
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, "doctor"), 0);
    CHECK_INT(c.sub, SUB_DOCTOR);
    cli_free(&c);
    setup_in(&r, none, root);
    cli_init(&c);
    cli_parse_line(&c, "auth status");
    {
        cl_pout po;
        jw_reset(&pc.out);
        po.u = 0;
        po.out = pc_out;
        po.err = 0;
        po.in = 0;
        CHECK_INT(print_subcommand(&r, &c, &po), 0);
        CHECK(strstr(outp(), "{\"loggedIn\":true,\"authMethod\":\"api_key\"") != 0);
        cli_free(&c);
        cli_init(&c);
        cli_parse_line(&c, "doctor");
        jw_reset(&pc.out);
        print_subcommand(&r, &c, &po);
        CHECK(strstr(outp(), "[OK]    System") != 0);
    }
    cli_free(&c);
    repl_free(&r);
    setup_in(&r, yes, root);
    cli_init(&c);
    cli_parse_line(&c, "purge");
    {
        cl_pout po;
        po.u = 0;
        po.out = pc_out;
        po.err = 0;
        po.in = 0;
        mkdir(r.sess.dir, 0700);
        xput(r.sess.dir, "1234abcd.jsonl", "{}\n");
        jw_reset(&pc.out);
        CHECK_INT(print_subcommand(&r, &c, &po), 0);
        CHECK(strstr(outp(), "Removed ") != 0);
        pset(&pw, r.sess.dir);
        jw_rawz(&pw, "/1234abcd.jsonl");
        CHECK(!exists(pw.p));
    }
    cli_free(&c);
    repl_free(&r);
    /* P5 initialPrompt: an --agents agent starts the session with it */
    xput(root, "ip.json", "{\"ip\":{\"description\":\"x\",\"prompt\":\"IP\",\"initialPrompt\":\"START-WITH-THIS\"}}");
    setup_in(&r, none, root);
    cli_init(&c);
    CHECK_INT(cli_parse_line(&c, "--agents ip.json --agent ip"), 0);
    CHECK_INT(cli_apply(&c, &r), 0);
    CHECK(c.prompt && !strcmp(c.prompt, "START-WITH-THIS"));
    cli_free(&c);
    repl_free(&r);

    /* SessionStart's own answers: sessionTitle, initialUserMessage (print
     * mode's first turn), reloadSkills; terminalSequence (only the allowed
     * escapes reach the console); PostToolUse's updatedToolOutput */
    {
        char s1[600];
        strcpy(s1, "echo '{\"hookSpecificOutput\":{\"hookEventName\":\"SessionStart\",\"sessionTitle\":\"HOOK-TITLE\","
                   "\"initialUserMessage\":\"FIRST-FROM-HOOK\",\"reloadSkills\":true},"
                   "\"terminalSequence\":\"\\\\u001b]0;HOOK-SEQ\\\\u0007\"}'\n");
        xput(root, "ss.sh", s1);
        xput(root, "out.sh", "echo '{\"hookSpecificOutput\":{\"hookEventName\":\"PostToolUse\","
                             "\"updatedToolOutput\":\"REPLACED-OUTPUT\"}}'\n");
        xput(root, "bad.sh", "echo '{\"terminalSequence\":\"\\\\u001b[2J\"}'\n");
    }
    pset(&pw, "{\"hooks\":{\"SessionStart\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    jw_rawz(&pw, root);
    jw_rawz(&pw, "/ss.sh\"}]}],\"PostToolUse\":[{\"matcher\":\"Read\",\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    jw_rawz(&pw, root);
    jw_rawz(&pw, "/out.sh\"},{\"type\":\"command\",\"command\":\"sh ");
    jw_rawz(&pw, root);
    jw_rawz(&pw, "/bad.sh\"}]}]}}");
    xput(root, ".claude/settings.json", pw.p);
    setup_in(&r, none, root);
    add_answer(0, 0, 0, "first done");
    add_answer("toolu_R5", "Read", "{\"file_path\":\"S/Startup-Sequence\"}", 0);
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p and then", 0), 0);
    CHECK_INT(sb.nreq, 3);
    CHECK(sb.nreq > 0 && strstr(sb.body[0], "FIRST-FROM-HOOK") != 0 && strstr(sb.body[0], "and then") == 0);
    CHECK(sb.nreq > 1 && strstr(sb.body[1], "\"text\":\"and then\"") != 0);
    CHECK(sb.nreq > 2 && strstr(sb.body[2], "REPLACED-OUTPUT") != 0 && strstr(sb.body[2], "SetPatch QUIET") == 0);
    CHECK_STR(r.sess.title, "HOOK-TITLE");
    repl_free(&r);
    setup_in(&r, none, root);
    add_answer(0, 0, 0, "hello");
    repl_line(&r, "hi");
    CHECK(strstr(cn.screen.p, "\033]0;HOOK-SEQ\007") != 0);
    CHECK(strstr(cn.screen.p, "\033[2J") == 0);      /* not on the allowlist: ignored */
    repl_free(&r);
    pset(&pw, root);
    jw_rawz(&pw, "/.claude/settings.json");
    remove(pw.p);

    /* skills that wait (paths:, a nested .claude/skills) until a matching file is
     * worked on; a typed skill's effort; a command in a subdirectory is dir:name */
    xput(root, ".claude/skills/pathsk/SKILL.md", "---\ndescription: For S files\npaths: \"S/*\"\n---\nPATHSK\n");
    xput(root, "sub2/.claude/skills/nest/SKILL.md", "---\ndescription: Nested one\n---\nNEST\n");
    xput(root, "sub2/x.txt", "x\n");
    xput(root, ".claude/skills/eff/SKILL.md", "---\ndescription: Low effort\neffort: low\n---\nEFF-SKILL\n");
    xput(root, ".claude/commands/grp/cmd.md", "GROUPED-COMMAND\n");
    setup_in(&r, none, root);
    add_answer("toolu_S7", "Read", "{\"file_path\":\"S/Startup-Sequence\"}", 0);
    add_answer("toolu_S8", "Read", "{\"file_path\":\"sub2/x.txt\"}", 0);
    add_answer(0, 0, 0, "done");
    repl_line(&r, "look around");
    CHECK_INT(sb.nreq, 3);
    CHECK(sb.nreq > 0 && strstr(sb.body[0], "- pathsk:") == 0 && strstr(sb.body[0], "- nest:") == 0);
    CHECK(sb.nreq > 1 && strstr(sb.body[1], "- pathsk: For S files") != 0 && strstr(sb.body[1], "- nest:") == 0);
    CHECK(sb.nreq > 2 && strstr(sb.body[2], "- nest: Nested one") != 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "/eff");
    CHECK(strstr(sb.body[sb.nreq - 1], "EFF-SKILL") != 0 && strstr(sb.body[sb.nreq - 1], "\"effort\":\"low\"") != 0);
    CHECK_STR(r.effort, "medium");
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "/grp:cmd");
    CHECK(strstr(sb.body[sb.nreq - 1], "GROUPED-COMMAND") != 0);
    repl_free(&r);

    /* P9 checkpoints kept across a restart: a file Claude wrote in one run
     * is taken back by /rewind after a resume in the next */
    {
        char id[40], f[700];
        strcpy(f, root);
        strcat(f, "/made-by-claude.txt");
        setup_in(&r, none, root);
        add_answer("toolu_W9", "Write", "{\"file_path\":\"made-by-claude.txt\",\"content\":\"new\\n\"}", 0);
        add_answer(0, 0, 0, "written");
        CHECK_INT(run_print(&r, "-p --allowedTools Write -- write it", 0), 0);
        CHECK(exists(f));
        cl_copy(id, r.sess.id, sizeof(id));
        repl_free(&r);
        setup_in(&r, none, root);
        CHECK_INT(repl_resume_session(&r, id), 0);
        CHECK_INT(r.cp.n, 1);
        repl_line(&r, "/rewind 1 code");
        CHECK(!exists(f));
        repl_free(&r);
    }
    jw_free(&pw);
#undef SCRIPT
#undef HOOKS
}

/* ---- A4 gaps 2 (thoughts/shared/plans/2026-10-05-a4-gaps2-progress.md) ---- */

/* WebSearch's own request answered: one search, one result, a summary */
static void add_search_answer(void)
{
    static const char *const s[] = {
        "event: message_start\ndata: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_srch\",\"type\":\"message\","
        "\"role\":\"assistant\",\"model\":\"claude-opus-5-5\",\"content\":[],\"stop_reason\":null,\"stop_sequence\":null,"
        "\"usage\":{\"input_tokens\":50,\"output_tokens\":1}}}\n\n",
        "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":"
        "\"server_tool_use\",\"id\":\"srvtoolu_G2\",\"name\":\"web_search\",\"input\":{}}}\n\n",
        "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":"
        "\"input_json_delta\",\"partial_json\":\"{\\\"query\\\":\\\"amiga\\\"}\"}}\n\n"
        "event: content_block_stop\ndata: {\"type\":\"content_block_stop\",\"index\":0}\n\n",
        "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":1,\"content_block\":{\"type\":"
        "\"web_search_tool_result\",\"tool_use_id\":\"srvtoolu_G2\",\"content\":[{\"type\":\"web_search_result\","
        "\"title\":\"Aminet\",\"url\":\"https://aminet.net/\",\"encrypted_content\":\"x\"}]}}\n\n",
        "event: content_block_stop\ndata: {\"type\":\"content_block_stop\",\"index\":1}\n\n"
        "event: content_block_start\ndata: {\"type\":\"content_block_start\",\"index\":2,\"content_block\":{\"type\":"
        "\"text\",\"text\":\"\"}}\n\n",
        "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":2,\"delta\":{\"type\":"
        "\"text_delta\",\"text\":\"Aminet is the archive.\"}}\n\n"
        "event: content_block_stop\ndata: {\"type\":\"content_block_stop\",\"index\":2}\n\n",
        "event: message_delta\ndata: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\","
        "\"stop_sequence\":null},\"usage\":{\"output_tokens\":20}}\n\n"
        "event: message_stop\ndata: {\"type\":\"message_stop\"}\n\n",
        0
    };
    jw w;
    int i;
    jw_init(&w);
    for (i = 0; s[i]; i++)
        jw_rawz(&w, s[i]);
    add_sse(w.p, w.n);
    jw_free(&w);
}

static void remove_rel(const char *root, const char *rel)
{
    char p[800];
    strcpy(p, root);
    strcat(p, "/");
    strcat(p, rel);
    remove(p);
}

/* the whole text of a file in the tree ("" none) */
static void slurp(const char *root, const char *rel, char *out, long cap)
{
    char p[800];
    FILE *f;
    long n = 0;
    strcpy(p, root);
    strcat(p, "/");
    strcat(p, rel);
    out[0] = 0;
    f = fopen(p, "rb");
    if (!f)
        return;
    n = (long)fread(out, 1, (size_t)cap - 1, f);
    out[n] = 0;
    fclose(f);
}

/* Tools: T1 nested subagents, T2 T3 Bash's background move and limits, T4
 * Edit's relaxed check, T5 WebFetch, T6 WebSearch, T7 the task tools and
 * TaskStop, T8 Monitor, T9 cron, T10 Edit and Read rules, H7 Stop's fields */
static void test_gaps2_tools(void)
{
    static const char *none[] = { 0 };
    static const char *planit[] = { "plan it", 0 };
    static cl_repl r;
    char root[600], p[900], txt[4096];
    cl_net web;
    strcpy(root, dir);
    strcat(root, "/gaps2");
    mkdir(root, 0700);
    xput(root, "S/Startup-Sequence", "SetPatch QUIET\n");

    /* T1 nested subagents: the subagent may launch one of its own (three
     * layers below the conversation by default) */
    setup_in(&r, none, root);
    add_answer("toolu_N1", "Task", "{\"description\":\"outer\",\"prompt\":\"look deeper\",\"subagent_type\":"
                                   "\"general-purpose\"}", 0);
    add_answer("toolu_N2", "Task", "{\"description\":\"inner\",\"prompt\":\"find it\",\"subagent_type\":\"Explore\"}", 0);
    add_answer(0, 0, 0, "INNER-REPORT");
    add_answer(0, 0, 0, "OUTER-REPORT");
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p go", 0), 0);
    CHECK_INT(sb.nreq, 5);
    CHECK(sb.nreq == 5 && strstr(sb.body[1], "{\"name\":\"Task\",") != 0);    /* layer 1 may delegate */
    CHECK(sb.nreq == 5 && strstr(sb.body[2], "file search specialist") != 0 && strstr(sb.body[2], "find it") != 0);
    CHECK(sb.nreq == 5 && strstr(sb.body[3], "INNER-REPORT") != 0);
    CHECK(sb.nreq == 5 && strstr(sb.body[4], "OUTER-REPORT") != 0);
    repl_free(&r);
    setenv("CLAUDE_CODE_MAX_SUBAGENT_SPAWN_DEPTH", "1", 1);     /* nesting off */
    setup_in(&r, none, root);
    add_answer("toolu_N3", "Task", "{\"description\":\"outer\",\"prompt\":\"p\",\"subagent_type\":\"general-purpose\"}", 0);
    add_answer(0, 0, 0, "R");
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p go", 0), 0);
    CHECK(sb.nreq == 3 && strstr(sb.body[1], "{\"name\":\"Task\",") == 0 && strstr(sb.body[0], "{\"name\":\"Task\",") != 0);
    repl_free(&r);
    unsetenv("CLAUDE_CODE_MAX_SUBAGENT_SPAWN_DEPTH");

    /* T2 a command still running at its time limit moves to the background
     * (cd noted), its end is news beside a later round's results; a sleep
     * stops instead; T3 a failure's long output: its head and its tail */
    setenv("BASH_DEFAULT_TIMEOUT_MS", "1000", 1);
    setup_in(&r, none, root);
    add_answer("toolu_B1", "Bash", "{\"command\":\"cd S; echo start; sleep 2; echo late\"}", 0);
    add_answer("toolu_B2", "Bash", "{\"command\":\"sleep 2\",\"timeout\":5000}", 0);
    add_answer("toolu_B3", "Bash", "{\"command\":\"sleep 3\",\"timeout\":1000}", 0);
    add_answer("toolu_B4", "Bash", "{\"command\":\"yes a | head -n 6000; echo TAILEND; exit 10\"}", 0);
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p --allowedTools Bash -- go", 0), 0);
    CHECK_INT(sb.nreq, 5);
    CHECK(sb.nreq == 5 && strstr(sb.body[1], "Command did not complete within its 1s timeout and was moved to the "
                                             "background with ID: bash_1") != 0);
    CHECK(sb.nreq == 5 && strstr(sb.body[1], "Session cwd remains ") != 0);
    CHECK(sb.nreq == 5 && strstr(sb.body[2], "No human input has occurred") != 0 &&
          strstr(sb.body[2], "Background command bash_1 (\\\"cd S; echo start; sleep 2; echo late\\\") completed with "
                             "exit code 0") != 0);
    CHECK(sb.nreq == 5 && strstr(sb.body[3], "\"tool_use_id\":\"toolu_B3\",\"content\":\"The command ran out of time (1 s)") != 0);
    CHECK(sb.nreq == 5 && strstr(sb.body[4], "characters cut from the middle") != 0 &&
          strstr(sb.body[4], "TAILEND") != 0 && strstr(sb.body[4], "Return code 10.") != 0);
    repl_free(&r);
    setenv("CLAUDE_CODE_DISABLE_BACKGROUND_TASKS", "1", 1);
    setup_in(&r, none, root);
    add_answer("toolu_B5", "Bash", "{\"command\":\"echo s; sleep 3\"}", 0);
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p --allowedTools Bash -- go", 0), 0);
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "ran out of time (1 s)") != 0 &&
          strstr(sb.body[0], "{\"name\":\"Monitor\",") == 0);
    repl_free(&r);
    unsetenv("CLAUDE_CODE_DISABLE_BACKGROUND_TASKS");
    unsetenv("BASH_DEFAULT_TIMEOUT_MS");

    /* T4 Edit: an unread file edited by a newer model; a file changed since
     * its read edited when old_string still matches, with a note; Claude
     * Haiku 4.5 reads first -- a cat of the file counts as the read */
    xput(root, "un.txt", "alpha\n");
    xput(root, "c.txt", "one\ntwo\n");
    xput(root, "h.txt", "h\n");
    xput(root, "h2.txt", "h2\n");
    setup_in(&r, none, root);
    r.tools.perm.mode = PERM_ACCEPT;
    add_answer("toolu_E1", "Edit", "{\"file_path\":\"un.txt\",\"old_string\":\"alpha\",\"new_string\":\"beta\"}", 0);
    add_answer("toolu_R1", "Read", "{\"file_path\":\"c.txt\"}", 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "edit it");
    slurp(root, "un.txt", txt, sizeof(txt));
    CHECK_STR(txt, "beta\n");
    xput(root, "c.txt", "one\ntwo\nthree\n");
    strcpy(p, root);
    strcat(p, "/c.txt");
    age_file(p, 100);
    add_answer("toolu_E2", "Edit", "{\"file_path\":\"c.txt\",\"old_string\":\"two\",\"new_string\":\"TWO\"}", 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "edit c");
    slurp(root, "c.txt", txt, sizeof(txt));
    CHECK_STR(txt, "one\nTWO\nthree\n");
    CHECK(sb.nreq == 5 && strstr(sb.body[4], "Note: the file had changed on disk since you last read it") != 0);
    cl_copy(r.model, "claude-haiku-4-5", sizeof(r.model));
    add_answer("toolu_E3", "Edit", "{\"file_path\":\"h.txt\",\"old_string\":\"h\",\"new_string\":\"H\"}", 0);
    add_answer("toolu_B6", "Bash", "{\"command\":\"cat h2.txt\"}", 0);
    add_answer("toolu_E4", "Edit", "{\"file_path\":\"h2.txt\",\"old_string\":\"h2\",\"new_string\":\"H2\"}", 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "edit h");
    CHECK(sb.nreq == 9 && strstr(sb.body[6], "File has not been read yet") != 0);
    slurp(root, "h2.txt", txt, sizeof(txt));
    CHECK_STR(txt, "H2\n");
    repl_free(&r);

    /* T10 a Read deny rule blocks Write too; an Edit allow rule grants Read
     * (a read outside the working directories, in print mode: no one asks) */
    {
        char outside[600], home[600];
        const char *hv = getenv("HOME");
        strcpy(outside, dir);
        strcat(outside, "/outside");
        mkdir(outside, 0700);
        xput(dir, "outside/o.txt", "OUTSIDE-TEXT\n");
        cl_copy(home, hv ? hv : "", sizeof(home));
        if (!realpath(dir, p))
            strcpy(p, dir);
        setenv("HOME", p, 1);       /* canonical, as the start directory is */
        xput(root, ".claude/settings.json", "{\"permissions\":{\"deny\":[\"Read(secret.txt)\"],\"allow\":"
                                            "[\"Edit(~/outside/**)\"]}}");
        setup_in(&r, none, root);
        strcpy(p, "{\"file_path\":\"/outside/o.txt\"}");    /* AmigaOS: / is the parent */
        add_answer("toolu_W1", "Write", "{\"file_path\":\"secret.txt\",\"content\":\"x\"}", 0);
        add_answer("toolu_R2", "Read", p, 0);
        add_answer(0, 0, 0, "done");
        CHECK_INT(run_print(&r, "-p --allowedTools Write -- go", 0), 0);
        CHECK(sb.nreq == 3 && strstr(sb.body[1], "has been denied by the rule Read(secret.txt)") != 0);
        CHECK(sb.nreq == 3 && strstr(sb.body[2], "OUTSIDE-TEXT") != 0);
        repl_free(&r);
        if (hv)
            setenv("HOME", home, 1);
        else
            unsetenv("HOME");
        strcpy(p, root);
        strcat(p, "/.claude/settings.json");
        remove(p);
    }

    /* T5 WebFetch: localhost refused before any request; http upgraded to
     * https; a preapproved documentation host fetched with no one to ask,
     * another host denied */
    setup_in(&r, none, root);
    web.u = 0;
    web.open = wp_open;
    web.send = wp_send;
    web.recv = wp_recv;
    web.close = wp_close;
    web.err = s_err;
    r.tools.web = &web;
    free(r.tools.json);
    r.tools.json = 0;
    wp2_open_n = 0;
    wp2_tls = 0;
    wp2_req[0] = 0;
    add_answer("toolu_F1", "WebFetch", "{\"url\":\"http://localhost:3000/x\",\"prompt\":\"p\"}", 0);
    add_answer("toolu_F2", "WebFetch", "{\"url\":\"http://docs.python.org/3/\",\"prompt\":\"p\"}", 0);
    add_answer(0, 0, 0, "a python page");
    add_answer("toolu_F3", "WebFetch", "{\"url\":\"https://example.com/\",\"prompt\":\"p\"}", 0);
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p go", 0), 0);
    CHECK_INT(wp2_open_n, 1);
    CHECK_INT(wp2_tls, 1);
    CHECK(strstr(wp2_req, "Host: docs.python.org") != 0);
    CHECK(sb.nreq >= 2 && strstr(sb.body[1], "WebFetch cannot fetch localhost or other hostnames without a dot") != 0);
    CHECK(sb.nreq >= 5 && strstr(sb.body[3], "a python page") != 0);
    CHECK(sb.nreq >= 5 && strstr(sb.body[4], "permission to use this tool was denied") != 0);
    repl_free(&r);

    /* T6 WebSearch: Claude Code's client tool, its search the server tool in
     * a request of its own with the domains; the session's cap */
    setenv("CLAUDE_CODE_MAX_WEB_SEARCHES_PER_SESSION", "1", 1);
    setup_in(&r, none, root);
    add_answer("toolu_S1", "WebSearch", "{\"query\":\"amiga\",\"allowed_domains\":[\"aminet.net\"]}", 0);
    add_search_answer();
    add_answer("toolu_S2", "WebSearch", "{\"query\":\"again\"}", 0);
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p --allowedTools WebSearch -- search", 0), 0);
    CHECK_INT(sb.nreq, 4);
    CHECK(sb.nreq == 4 && strstr(sb.body[1], "\"allowed_domains\":[\"aminet.net\"]") != 0 &&
          strstr(sb.body[1], "Perform a web search for the query: amiga") != 0 &&
          strstr(sb.body[1], "\"web_search_2026") != 0);
    CHECK(sb.nreq == 4 && strstr(sb.body[2], "Links: [{\\\"title\\\":\\\"Aminet\\\",\\\"url\\\":\\\"https://aminet.net/\\\"}]") != 0 &&
          strstr(sb.body[2], "Aminet is the archive.") != 0);
    CHECK(sb.nreq == 4 && strstr(sb.body[3], "The web search limit of this session is reached") != 0);
    repl_free(&r);
    unsetenv("CLAUDE_CODE_MAX_WEB_SEARCHES_PER_SESSION");

    /* T7 the task tools on a model that has them (Claude Haiku 4.5), the list
     * the screen's task list shows; TodoWrite with CLAUDE_CODE_ENABLE_TASKS=0;
     * none on a newer model unless asked for; TaskStop */
    setup_in(&r, none, root);
    cl_copy(r.model, "claude-haiku-4-5", sizeof(r.model));
    add_answer("toolu_T1", "TaskCreate", "{\"subject\":\"Run the tests\",\"description\":\"all suites\","
                                         "\"activeForm\":\"Running the tests\"}", 0);
    add_answer("toolu_T2", "TaskUpdate", "{\"taskId\":\"1\",\"status\":\"in_progress\"}", 0);
    add_answer("toolu_T3", "TaskList", "{}", 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "track it");
    CHECK(sb.nreq == 4 && strstr(sb.body[0], "{\"name\":\"TaskCreate\",") != 0 &&
          strstr(sb.body[0], "{\"name\":\"TodoWrite\",") == 0);
    CHECK(sb.nreq == 4 && strstr(sb.body[1], "Task #1 created successfully: Run the tests") != 0);
    CHECK(sb.nreq == 4 && strstr(sb.body[3], "#1 [in_progress] Run the tests") != 0);
    CHECK(r.todos && strstr(r.todos, "\"status\":\"in_progress\"") != 0 && strstr(r.todos, "Running the tests") != 0);
    repl_free(&r);
    setenv("CLAUDE_CODE_ENABLE_TASKS", "0", 1);
    setup_in(&r, none, root);
    cl_copy(r.model, "claude-haiku-4-5", sizeof(r.model));
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "hi");
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "{\"name\":\"TodoWrite\",") != 0 &&
          strstr(sb.body[0], "{\"name\":\"TaskCreate\",") == 0);
    repl_free(&r);
    unsetenv("CLAUDE_CODE_ENABLE_TASKS");
    setup_in(&r, none, root);
    add_answer("toolu_K1", "Bash", "{\"command\":\"sleep 30\",\"run_in_background\":true}", 0);
    add_answer("toolu_K2", "TaskStop", "{\"task_id\":\"bash_1\"}", 0);
    add_answer(0, 0, 0, "ok");
    CHECK_INT(run_print(&r, "-p --allowedTools Bash,TaskCreate -- go", 0), 0);
    CHECK(sb.nreq == 3 && strstr(sb.body[0], "{\"name\":\"TaskCreate\",") != 0);     /* asked for on Opus */
    CHECK(sb.nreq == 3 && strstr(sb.body[2], "Successfully stopped task: bash_1") != 0);
    repl_free(&r);

    /* T8 Monitor: each line its command prints comes to Claude as news */
    setup_in(&r, none, root);
    add_answer("toolu_M1", "Monitor", "{\"description\":\"watch\",\"command\":\"echo EV-ONE; echo EV-TWO\","
                                      "\"timeout_ms\":60000}", 0);
    add_answer("toolu_M2", "Bash", "{\"command\":\"sleep 1\"}", 0);
    add_answer(0, 0, 0, "ok");
    CHECK_INT(run_print(&r, "-p --allowedTools Bash -- watch", 0), 0);
    CHECK(sb.nreq == 3 && strstr(sb.body[1], "\\\"taskId\\\":\\\"monitor_1\\\"") != 0);
    CHECK(sb.nreq == 3 && strstr(sb.body[2], "Monitor monitor_1 (watch): EV-ONE") != 0 &&
          strstr(sb.body[2], "Monitor monitor_1 (watch): EV-TWO") != 0 && strstr(sb.body[2], "ended") != 0);
    repl_free(&r);

    /* T9 cron: a one-shot job fires between turns (the line mode looks before
     * it waits for a line); a durable one is kept in the project; the
     * session's are restored on a resume; the screen's idle tick hands a due
     * one over; H7 Stop's background_tasks and session_crons */
    {
        cron_spec cs;
        char err[100], id[40];
        long due;
        char *w;
        CHECK_INT(cron_parse("7 10 * * *", &cs, err, sizeof(err)), 0);
        due = cron_next(&cs, 1500000000L);
        setup_in(&r, planit, root);
        sp.fake_now = due;
        add_answer("toolu_C1", "CronCreate", "{\"cron\":\"7 10 * * *\",\"prompt\":\"CRON-PROMPT\",\"recurring\":false}", 0);
        add_answer("toolu_C2", "CronCreate", "{\"cron\":\"*/5 * * * *\",\"prompt\":\"KEEP-PROMPT\",\"durable\":true}", 0);
        add_answer(0, 0, 0, "scheduled");
        add_answer(0, 0, 0, "cron ran");
        repl_run(&r);
        CHECK_INT((int)r.n_cron_fired, 1);
        CHECK(sb.nreq == 4 && strstr(sb.body[3], "CRON-PROMPT") != 0);
        CHECK(strstr(cn.screen.p, "Scheduled task ") != 0);
        slurp(root, ".claude/scheduled_tasks.json", txt, sizeof(txt));
        CHECK(strstr(txt, "KEEP-PROMPT") != 0 && strstr(txt, "CRON-PROMPT") == 0);
        CHECK_INT(tasks_cron_count(r.tools.tasks), 1);
        cl_copy(id, r.sess.id, sizeof(id));
        repl_free(&r);
        remove_rel(root, ".claude/scheduled_tasks.json");
        setup_in(&r, none, root);
        sp.fake_now = due;
        CHECK_INT(repl_resume_session(&r, id), 0);
        CHECK_INT(tasks_cron_count(r.tools.tasks), 1);  /* the recurring one, back */
        /* the screen: a due job handed over while it waits for keys */
        CHECK_INT(tasks_cron_add(r.tools.tasks, "* * * * *", "TUI-PROMPT", 0, 0, due, 0, err, sizeof(err)), 0);
        sp.fake_now = due + 120;
        w = sched_tui_wake(&r);
        CHECK(w && !strcmp(w, "TUI-PROMPT"));
        CHECK_INT(r.woke, 1);
        free(w);
        r.woke = 0;
        repl_free(&r);
    }
    {
        /* H7: Stop's last_assistant_message, background_tasks, session_crons */
        strcpy(p, "{\"hooks\":{\"Stop\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"cat > ");
        strcat(p, root);
        strcat(p, "/stop.json\"}]}]}}");
        xput(root, ".claude/settings.json", p);
        setup_in(&r, none, root);
        sp.fake_now = 1500000000L;
        add_answer("toolu_H1", "Bash", "{\"command\":\"sleep 30\",\"run_in_background\":true,\"description\":\"wait\"}", 0);
        add_answer("toolu_H2", "CronCreate", "{\"cron\":\"*/5 * * * *\",\"prompt\":\"check the build\"}", 0);
        add_answer(0, 0, 0, "FINAL-TEXT");
        CHECK_INT(run_print(&r, "-p --allowedTools Bash -- go", 0), 0);
        slurp(root, "stop.json", txt, sizeof(txt));
        CHECK(strstr(txt, "\"last_assistant_message\":\"FINAL-TEXT\"") != 0);
        CHECK(strstr(txt, "\"background_tasks\":[{\"id\":\"bash_1\",\"type\":\"shell\",\"status\":\"running\","
                          "\"description\":\"wait\",\"command\":\"sleep 30\"}]") != 0);
        CHECK(strstr(txt, "\"session_crons\":[{\"id\":\"") != 0 &&
              strstr(txt, "\"schedule\":\"*/5 * * * *\",\"recurring\":true,\"prompt\":\"check the build\"}]") != 0);
        repl_free(&r);
        remove_rel(root, ".claude/settings.json");
        remove_rel(root, ".claude/scheduled_tasks.json");
    }
}

/* ---- A4 gaps 3: /loop and ScheduleWakeup
 * (thoughts/shared/plans/2026-10-05-a4-gaps3-loop-progress.md) ---- */

static const char *last_body(void)
{
    return sb.nreq ? sb.body[sb.nreq - 1] : "";
}

/* request i's last user message (the conversation before it left out) */
static const char *user_tail(int i)
{
    const char *b = i >= 0 && i < sb.nreq ? sb.body[i] : "", *q, *at = b;
    for (q = b; (q = strstr(q, "\"role\":\"user\"")) != 0; q++)
        at = q;
    return at;
}

/* the user's loop.md (CLAUDE_CONFIG_DIR = ~/.claude) */
static void user_loop_md(cl_repl *r, const char *text)
{
    char p[600];
    FILE *f;
    strcpy(p, r->home);
    strcat(p, "/loop.md");
    if (!text) {
        remove(p);
        return;
    }
    f = fopen(p, "wb");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

/* LS the screen: the clock moves on a minute at each idle tick ("" in the script) */
static void loop_tick_clock(void)
{
    if (cs.script && cs.script[cs.next] && !cs.script[cs.next][0]) {
        sp.fake_now += 61;
        cs.clock += 2000;
    }
}

/* LS (the reachability test): /loop typed on the screen (repl_screen +
 * repl_run); its wakeups fire while the screen waits for keys, as
 * "Claude resuming /loop wakeup", their lines not echoed; three quiet
 * iterations in a row fold into one line; Esc on the idle box cancels
 * the pending wakeup */
static void test_loop_screen(const char *root)
{
    static const char *keys[] = { "/loop check the deploy\r", "", "", "", "\033", "/exit\r", 0 };
    static cl_repl r;
    int i, echoes = 0;
    const char *q;
    stub_reset();
    add_answer("toolu_LS1", "ScheduleWakeup", "{\"delaySeconds\":60,\"reason\":\"a first look\",\"prompt\":"
                                             "\"/loop check the deploy\",\"noop\":false}", 0);
    add_answer(0, 0, 0, "FIRST-LOOK done.");
    for (i = 0; i < 3; i++) {
        add_answer("toolu_LSq", "ScheduleWakeup", "{\"delaySeconds\":60,\"reason\":\"still quiet\",\"prompt\":"
                                                 "\"/loop check the deploy\",\"noop\":true}", 0);
        add_answer(0, 0, 0, "QUIET-TICK nothing new.");
    }
    cs_open(80, 24, keys);
    cs.before_read = loop_tick_clock;
    cs_io(&io);
    io.log = 0;
    net.u = 0;
    net.open = s_open;
    net.send = s_send;
    net.recv = s_recv;
    net.close = s_close;
    net.err = s_err;
    sys_posix_init(&sp, &sys);
    sp.fake_now = 1500000000L;
    CHECK_INT(repl_init(&r, &io, &net, &sys, CL_DEFAULT_URL, "test-key-not-real", root), 0);
    CHECK_INT(repl_screen(&r), 0);
    repl_run(&r);
    if (getenv("CL_DUMP")) {
        int k;
        for (k = 0; k < cs.rows; k++)
            printf("%2d|%s\n", k, cs_row(k));
    }
    CHECK_INT(cs.next, 6);
    CHECK_INT(sb.nreq, 8);
    CHECK_INT((int)r.n_loop_ticks, 3);                  /* the sentinel: three wakeups fired */
    CHECK(sb.nreq == 8 && strstr(sb.body[2], "## Input\\n\\ncheck the deploy") != 0);  /* /loop again */
    /* typed once; the wakeups' lines are not echoed */
    for (q = r.tui->log.p; q && (q = strstr(q, "/loop check the deploy")) != 0; q++)
        echoes++;
    CHECK_INT(echoes, 1);
    CHECK(strstr(r.tui->log.p, "Claude resuming /loop wakeup") != 0);
    /* the three quiet iterations: one line on the screen */
    CHECK(cs_find("Claude resuming /loop wakeup (3 quiet wake-ups, nothing to do): still") >= 0);
    CHECK(cs_find("QUIET-TICK") < 0);
    CHECK(cs_find("FIRST-LOOK done.") >= 0);
    /* Esc on the idle box: the pending wakeup cancelled */
    CHECK(cs_find("Cancelled the pending /loop wakeup") >= 0);
    CHECK(tasks_wakeup_get(r.tools.tasks) == 0 && !tasks_loop_on(r.tools.tasks));
    repl_free(&r);
    cs_close();
}

static void test_gaps3_loop(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    char root[600], txt[40000], err[200], id[12];
    const cron_job *j;
    long t0 = 1500000000L;
    jw w;
    strcpy(root, dir);
    strcat(root, "/gaps3");
    mkdir(root, 0700);

    /* L1 the input's prompt beside its interval (rule 1 a leading token,
     * rule 2 a trailing "every" clause) */
    CHECK(sched_loop_has_prompt("check the deploy"));
    CHECK(sched_loop_has_prompt("5m check the deploy"));
    CHECK(sched_loop_has_prompt("check every PR"));
    CHECK(sched_loop_has_prompt("run tests every 5 minutes"));
    CHECK(sched_loop_has_prompt("every PR"));
    CHECK(!sched_loop_has_prompt(""));
    CHECK(!sched_loop_has_prompt("  15m "));
    CHECK(!sched_loop_has_prompt("every 20m"));
    CHECK(!sched_loop_has_prompt("every 2 hours"));

    /* L2 /loop PROMPT: Claude Code's skill and the input in a turn;
     * ScheduleWakeup declared; a delay under a minute clamped to 60 s; the
     * wakeup in Stop's session_crons, not in the session's own cron file */
    setup_in(&r, none, root);
    sp.fake_now = t0;
    add_answer("toolu_W1", "ScheduleWakeup", "{\"delaySeconds\":30,\"reason\":\"the build is short\",\"prompt\":"
                                            "\"/loop check the deploy\",\"noop\":false}", 0);
    add_answer(0, 0, 0, "Checked; again in a minute.");
    repl_line(&r, "/loop check the deploy");
    CHECK_INT(sb.nreq, 2);
    CHECK(sb.nreq == 2 && strstr(sb.body[0], "# /loop: a recurring or self-paced prompt") != 0 &&
          strstr(sb.body[0], "## Input\\n\\ncheck the deploy") != 0 &&
          strstr(sb.body[0], "Default loop prompt") == 0);
    CHECK(sb.nreq == 2 && strstr(sb.body[0], "{\"name\":\"ScheduleWakeup\",") != 0);
    CHECK(strstr(last_body(), "Next /loop wakeup in 1 minute (the shortest wait is 1 minute): the build is short") != 0);
    j = tasks_wakeup_get(r.tools.tasks);
    CHECK(j && j->fire == t0 + 60 && !strcmp(j->prompt, "/loop check the deploy") && !j->recurring);
    CHECK(tasks_loop_on(r.tools.tasks));
    jw_init(&w);
    tasks_crons_json(r.tools.tasks, &w, 0, 1);
    CHECK(w.p && strstr(w.p, "\"recurring\":false,\"prompt\":\"/loop check the deploy\"") != 0);
    jw_reset(&w);
    tasks_crons_json(r.tools.tasks, &w, 0, 0);
    CHECK_STR(w.p ? w.p : "", "[]");    /* Claude Code: a self-paced loop is not restored on a resume */
    jw_free(&w);

    /* L3 it fires between turns at its time, not before: "Claude resuming
     * /loop wakeup", the /loop line again; an hour is the longest wait */
    add_answer("toolu_W2", "ScheduleWakeup", "{\"delaySeconds\":99999,\"reason\":\"quiet\",\"prompt\":"
                                            "\"/loop check the deploy\",\"noop\":true}", 0);
    add_answer(0, 0, 0, "Still quiet.");
    sp.fake_now = t0 + 59;
    CHECK_INT(sched_line_mode(&r), 0);
    sp.fake_now = t0 + 60;
    CHECK_INT(sched_line_mode(&r), 1);
    CHECK_INT((int)r.n_loop_ticks, 1);
    CHECK(strstr(cn.screen.p, "Claude resuming /loop wakeup") != 0);
    CHECK(strstr(user_tail(2), "## Input\\n\\ncheck the deploy") != 0);
    CHECK(strstr(last_body(), "Next /loop wakeup in 1 hour (the longest wait is 1 hour): quiet") != 0);
    j = tasks_wakeup_get(r.tools.tasks);
    CHECK(j && j->fire == t0 + 60 + 3600);

    /* L4 a prompt typed meanwhile leaves the loop as it is */
    add_answer(0, 0, 0, "Hello.");
    repl_line(&r, "hello");
    CHECK(tasks_wakeup_get(r.tools.tasks) != 0);

    /* L5 an iteration that sets no wakeup: one fallback 20 minutes on, with
     * the same prompt; the next such iteration ends the loop */
    add_answer(0, 0, 0, "Forgot to reschedule.");
    sp.fake_now = t0 + 3660;
    CHECK_INT(sched_line_mode(&r), 1);
    j = tasks_wakeup_get(r.tools.tasks);
    CHECK(j && j->fire == t0 + 3660 + 1200 && !strcmp(j->prompt, "/loop check the deploy"));
    CHECK(strstr(cn.screen.p, "one more in 20 minutes") != 0);
    add_answer(0, 0, 0, "Again nothing.");
    sp.fake_now = t0 + 3660 + 1200;
    CHECK_INT(sched_line_mode(&r), 1);
    CHECK(tasks_wakeup_get(r.tools.tasks) == 0 && !tasks_loop_on(r.tools.tasks));
    CHECK(strstr(cn.screen.p, "The /loop has ended: its fallback iteration set no next wakeup.") != 0);
    CHECK_INT((int)r.n_loop_ticks, 3);

    /* L6 stop: true alone ends it, the pending wakeup cancelled */
    add_answer("toolu_W3", "ScheduleWakeup", "{\"delaySeconds\":120,\"reason\":\"r\",\"prompt\":\"/loop x\"}", 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "/loop x");
    CHECK(tasks_wakeup_get(r.tools.tasks) != 0);
    add_answer("toolu_W4", "ScheduleWakeup", "{\"stop\":true}", 0);
    add_answer(0, 0, 0, "The loop is done.");
    repl_line(&r, "stop the loop");
    CHECK(tasks_wakeup_get(r.tools.tasks) == 0 && !tasks_loop_on(r.tools.tasks));
    CHECK(strstr(last_body(), "Loop stopped: the pending wakeup is cancelled.") != 0);
    /* ... and a call without its prompt says what it needs */
    add_answer("toolu_W5", "ScheduleWakeup", "{\"delaySeconds\":120,\"reason\":\"r\"}", 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "go");
    CHECK(strstr(last_body(), "prompt is required") != 0 && tasks_wakeup_get(r.tools.tasks) == 0);
    repl_free(&r);

    /* L7 bare /loop: the built-in maintenance prompt after the skill; a
     * loop.md in its place (the project's before the user's, cut at 25000
     * bytes); the sentinel's fire reads loop.md again; /proactive is /loop */
    user_loop_md(&r, 0);
    remove_rel(root, ".claude/loop.md");
    setup_in(&r, none, root);
    sp.fake_now = t0;
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "/loop");
    CHECK(strstr(user_tail(sb.nreq - 1), "## Default loop prompt\\n\\nWork through the following, in order:") != 0);
    user_loop_md(&r, "USER-LOOP-MD");
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "/proactive");
    CHECK(strstr(user_tail(sb.nreq - 1), "# /loop: a recurring") != 0 &&
          strstr(user_tail(sb.nreq - 1), "USER-LOOP-MD") != 0);
    xput(root, ".claude/loop.md", "LOOP-MD-ONE");
    add_answer("toolu_W6", "ScheduleWakeup", "{\"delaySeconds\":300,\"reason\":\"r\",\"prompt\":"
                                            "\"<<autonomous-loop-dynamic>>\"}", 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "/loop");
    CHECK(strstr(user_tail(sb.nreq - 2), "LOOP-MD-ONE") != 0 && strstr(user_tail(sb.nreq - 2), "USER-LOOP-MD") == 0);
    xput(root, ".claude/loop.md", "LOOP-MD-TWO");        /* edited while it waits */
    add_answer(0, 0, 0, "stopped by forgetting");
    sp.fake_now = t0 + 300;
    CHECK_INT(sched_line_mode(&r), 1);
    CHECK(strstr(user_tail(sb.nreq - 1), "LOOP-MD-TWO") != 0 && strstr(user_tail(sb.nreq - 1), "LOOP-MD-ONE") == 0 &&
          strstr(user_tail(sb.nreq - 1), "(An iteration of a self-paced /loop with no prompt of its own.") != 0 &&
          strstr(user_tail(sb.nreq - 1), "# /loop: a recurring") == 0);
    {
        /* loop.md beyond 25000 bytes is cut */
        static char big[30010];
        memset(big, 'A', 30000);
        strcpy(big + 30000, "TAIL");
        xput(root, ".claude/loop.md", big);
        add_answer(0, 0, 0, "ok");
        repl_line(&r, "/loop");
        CHECK(strstr(user_tail(sb.nreq - 1), "AAAA") != 0 && strstr(user_tail(sb.nreq - 1), "TAIL") == 0);
    }
    tasks_wakeup_cancel(r.tools.tasks);
    repl_free(&r);
    remove_rel(root, ".claude/loop.md");
    user_loop_md(&r, 0);

    /* L8 /loop INTERVAL alone: the model's CronCreate with the fixed
     * sentinel; at its fire the default prompt, with no self-pacing note */
    setup_in(&r, none, root);
    sp.fake_now = t0;
    add_answer("toolu_C1", "CronCreate", "{\"cron\":\"*/15 * * * *\",\"prompt\":\"<<autonomous-loop>>\",\"recurring\":true}",
               0);
    add_answer(0, 0, 0, "Scheduled.");
    repl_line(&r, "/loop 15m");
    CHECK(sb.nreq == 2 && strstr(sb.body[0], "## Input\\n\\n15m\\n\\n## Default loop prompt\\n\\nWork through") != 0);
    add_answer(0, 0, 0, "Nothing pending.");
    sp.fake_now = t0 + 3600;
    CHECK_INT(sched_line_mode(&r), 1);
    CHECK_INT((int)r.n_loop_ticks, 0);
    CHECK(strstr(cn.screen.p, ": /loop (the default loop prompt)") != 0);
    CHECK(strstr(user_tail(sb.nreq - 1), "Work through the following") != 0 &&
          strstr(user_tail(sb.nreq - 1), "(An iteration of a self-paced") == 0);
    repl_free(&r);
    remove_rel(root, ".claude/scheduled_tasks.json");

    /* L9 seven days: a wakeup past the loop's seventh day is refused and the
     * loop ends; CronDelete of the wakeup ends it too */
    setup_in(&r, none, root);
    CHECK_INT(tasks_wakeup_set(r.tools.tasks, 60, "/loop x", t0, id, err, sizeof(err)), 0);
    CHECK_INT(tasks_wakeup_set(r.tools.tasks, 3600, "/loop x", t0 + 7L * 86400L - 1000, id, err, sizeof(err)), -1);
    CHECK(strstr(err, "seven days") != 0 && !tasks_loop_on(r.tools.tasks) && !tasks_wakeup_get(r.tools.tasks));
    CHECK_INT(tasks_wakeup_set(r.tools.tasks, 60, "/loop x", t0, id, err, sizeof(err)), 0);
    CHECK_INT(tasks_cron_delete(r.tools.tasks, id), 0);
    CHECK(!tasks_loop_on(r.tools.tasks));
    repl_free(&r);

    /* L10 print mode: interactive only, no request; CLAUDE_CODE_DISABLE_CRON:
     * /loop unavailable, ScheduleWakeup not declared */
    setup_in(&r, none, root);
    run_print(&r, "-p /loop check the deploy", 0);
    CHECK_INT(sb.nreq, 0);
    cl_copy(txt, outp(), sizeof(txt));
    cl_cat(txt, pc.err.p ? pc.err.p : "", sizeof(txt));
    CHECK(strstr(txt, "/loop runs in an interactive session only") != 0);
    repl_free(&r);
    setenv("CLAUDE_CODE_DISABLE_CRON", "1", 1);
    setup_in(&r, none, root);
    repl_line(&r, "/loop check the deploy");
    CHECK_INT(sb.nreq, 0);
    CHECK(strstr(cn.screen.p, "/loop is not available: CLAUDE_CODE_DISABLE_CRON") != 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "hi");
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "{\"name\":\"ScheduleWakeup\"") == 0);
    repl_free(&r);
    unsetenv("CLAUDE_CODE_DISABLE_CRON");

    /* L11 the rig's recording (tools/claude_fixture.py, "loop test"):
     * ScheduleWakeup for a minute with the /loop line again, then the
     * update */
    setup_in(&r, none, root);
    sp.fake_now = t0;
    add_stream("tool_loop.sse");
    add_stream("loop_final.sse");
    repl_line(&r, "/loop loop test");
    CHECK_INT(sb.nreq, 2);
    j = tasks_wakeup_get(r.tools.tasks);
    CHECK(j && j->fire == t0 + 60 && !strcmp(j->prompt, "/loop loop test"));
    CHECK(strstr(cn.screen.p, "Self-pacing: nothing changed") != 0);
    repl_free(&r);

    test_loop_screen(root);
}

/* ---- an http hook's server: one canned answer to each POST, the request kept ---- */

static const char *hk_answer;   /* the whole HTTP response */
static long hk_pos;
static jw hk_req;
static int hk_posts;

static int hk_open(void *u, const char *host, int port, int tls)
{
    (void)u;
    (void)host;
    (void)port;
    (void)tls;
    hk_pos = 0;
    hk_posts++;
    return 0;
}

static long hk_send(void *u, const char *b, long n)
{
    (void)u;
    jw_raw(&hk_req, b, n);
    return n;
}

static long hk_recv(void *u, char *b, long cap, int timeout_ms)
{
    long n = (long)strlen(hk_answer) - hk_pos;
    (void)u;
    (void)timeout_ms;
    if (n > cap)
        n = cap;
    memcpy(b, hk_answer + hk_pos, (size_t)n);
    hk_pos += n;
    return n;
}

static void hk_close(void *u)
{
    (void)u;
}

/* Hooks: H1 agent, H2 http, H3 async, H4 frontmatter, H5 FileChanged and
 * watchPaths, H6 MessageDisplay, H8 defer, H9 workspace trust, H10
 * PermissionRequest's updatedInput */
static void test_gaps2_hooks(void)
{
    static const char *none[] = { 0 };
    static const char *say_no[] = { "n", 0 };
    static const char *say_yes[] = { "y", "/exit", 0 };
    static cl_repl r;
    char root[600], p[1400], txt[4096];
    cl_net hweb;
    strcpy(root, dir);
    strcat(root, "/gaps2h");
    mkdir(root, 0700);
    xput(root, "S/Startup-Sequence", "SetPatch QUIET\n");
    xput(root, "mark.sh", "cat > \"$1\"\n");

    /* H2 an http hook: the event POSTed (headers with the allowed variable), a
     * JSON answer read as a command hook's -- here a deny */
    setenv("GAPS_HOOK_TOKEN", "tok123", 1);
    xput(root, ".claude/settings.json",
         "{\"hooks\":{\"PreToolUse\":[{\"matcher\":\"Bash\",\"hooks\":[{\"type\":\"http\",\"url\":"
         "\"http://hooks.example.org/pre\",\"headers\":{\"Authorization\":\"Bearer $GAPS_HOOK_TOKEN\","
         "\"X-Other\":\"[$HOME]\"},\"allowedEnvVars\":[\"GAPS_HOOK_TOKEN\"]}]}]}}");
    setup_in(&r, none, root);
    jw_init(&hk_req);
    hk_posts = 0;
    hk_answer = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 123\r\n\r\n"
                "{\"hookSpecificOutput\":{\"hookEventName\":\"PreToolUse\",\"permissionDecision\":\"deny\","
                "\"permissionDecisionReason\":\"HTTP-SAYS-NO\"}}  ";
    hweb.u = 0;
    hweb.open = hk_open;
    hweb.send = hk_send;
    hweb.recv = hk_recv;
    hweb.close = hk_close;
    hweb.err = s_err;
    r.tools.web = &hweb;
    add_answer("toolu_HH1", "Bash", "{\"command\":\"echo hi\"}", 0);
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p --allowedTools Bash -- go", 0), 0);
    CHECK_INT(hk_posts, 1);
    CHECK(hk_req.p && strstr(hk_req.p, "POST /pre HTTP/1.1\r\nHost: hooks.example.org") == hk_req.p);
    CHECK(hk_req.p && strstr(hk_req.p, "Content-Type: application/json\r\n") != 0 &&
          strstr(hk_req.p, "Authorization: Bearer tok123\r\n") != 0 && strstr(hk_req.p, "X-Other: []\r\n") != 0);
    CHECK(hk_req.p && strstr(hk_req.p, "\"hook_event_name\":\"PreToolUse\"") != 0 &&
          strstr(hk_req.p, "\"tool_name\":\"Bash\"") != 0);
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "PreToolUse hook blocked this call: HTTP-SAYS-NO") != 0);
    repl_free(&r);
    /* a non-2xx answer: an error shown, the call goes on */
    setup_in(&r, none, root);
    jw_reset(&hk_req);
    hk_answer = "HTTP/1.1 500 Oops\r\nContent-Length: 0\r\n\r\n";
    r.tools.web = &hweb;
    add_answer("toolu_HH2", "Bash", "{\"command\":\"echo ran-anyway\"}", 0);
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p --allowedTools Bash -- go", 0), 0);
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "ran-anyway") != 0);
    repl_free(&r);
    jw_free(&hk_req);
    unsetenv("GAPS_HOOK_TOKEN");

    /* H1 an agent hook on Stop: a subagent with Read, Grep, Glob says not yet,
     * Claude goes on with its reason; then yes */
    xput(root, ".claude/settings.json",
         "{\"hooks\":{\"Stop\":[{\"hooks\":[{\"type\":\"agent\",\"prompt\":\"Are the tests done? $ARGUMENTS\"}]}]}}");
    setup_in(&r, none, root);
    add_answer(0, 0, 0, "first answer");
    add_answer(0, 0, 0, "{\"ok\": false, \"reason\": \"AGENT-NOT-YET\"}");
    add_answer(0, 0, 0, "second answer");
    add_answer(0, 0, 0, "{\"ok\": true}");
    repl_line(&r, "work");
    CHECK_INT(sb.nreq, 4);
    CHECK(sb.nreq == 4 && strstr(sb.body[1], "You verify a condition for a hook") != 0 &&
          strstr(sb.body[1], "{\"name\":\"Grep\",") != 0 && strstr(sb.body[1], "{\"name\":\"Bash\",") == 0 &&
          strstr(sb.body[1], "Are the tests done? {") != 0);
    CHECK(sb.nreq == 4 && strstr(sb.body[2], "AGENT-NOT-YET") != 0);
    repl_free(&r);

    /* H3 an async hook: started in the background, its additionalContext to
     * Claude beside a later round's results */
    xput(root, "async.sh", "sleep 1\necho '{\"hookSpecificOutput\":{\"hookEventName\":\"PostToolUse\","
                           "\"additionalContext\":\"ASYNC-CONTEXT\"}}'\n");
    strcpy(p, "{\"hooks\":{\"PostToolUse\":[{\"matcher\":\"Read\",\"hooks\":[{\"type\":\"command\",\"async\":true,"
              "\"command\":\"sh ");
    strcat(p, root);
    strcat(p, "/async.sh\"}]}]}}");
    xput(root, ".claude/settings.json", p);
    setup_in(&r, none, root);
    add_answer("toolu_HA1", "Read", "{\"file_path\":\"S/Startup-Sequence\"}", 0);
    add_answer("toolu_HA2", "Bash", "{\"command\":\"sleep 2\"}", 0);
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p --allowedTools Bash -- go", 0), 0);
    CHECK_INT((int)r.hooks.n_async, 1);
    CHECK(sb.nreq == 3 && strstr(sb.body[1], "ASYNC-CONTEXT") == 0);
    CHECK(sb.nreq == 3 && strstr(sb.body[2], "Async hook (PostToolUse)") != 0 &&
          strstr(sb.body[2], "ASYNC-CONTEXT") != 0);
    repl_free(&r);

    /* H4 a skill's frontmatter hooks: from its first use on (once: one run);
     * an agent's while it runs, its Stop as SubagentStop */
    remove_rel(root, ".claude/settings.json");
    strcpy(p, "---\nname: guarded\ndescription: guards Bash\nhooks:\n  PreToolUse:\n    - matcher: \"Bash\"\n"
              "      hooks:\n        - type: command\n          command: \"sh ");
    strcat(p, root);
    strcat(p, "/mark.sh ");
    strcat(p, root);
    strcat(p, "/skillhook.json\"\n          once: true\n---\nGuarded work.\n");
    xput(root, ".claude/skills/guarded/SKILL.md", p);
    strcpy(p, "---\nname: stopper\ndescription: an agent with hooks\ntools: Read\nhooks:\n  Stop:\n    - hooks:\n"
              "        - type: command\n          command: \"sh ");
    strcat(p, root);
    strcat(p, "/mark.sh ");
    strcat(p, root);
    strcat(p, "/agentstop.json\"\n---\nYou stop.\n");
    xput(root, ".claude/agents/stopper.md", p);
    setup_in(&r, none, root);
    add_answer("toolu_HS1", "Bash", "{\"command\":\"echo before\"}", 0);
    add_answer(0, 0, 0, "ok");
    add_answer("toolu_HS2", "Bash", "{\"command\":\"echo after\"}", 0);
    add_answer(0, 0, 0, "ok");
    add_answer("toolu_HS3", "Bash", "{\"command\":\"echo again\"}", 0);
    add_answer("toolu_HS4", "Task", "{\"description\":\"stop\",\"prompt\":\"p\",\"subagent_type\":\"stopper\"}", 0);
    add_answer(0, 0, 0, "AGENT-DONE");
    add_answer(0, 0, 0, "ok");
    CHECK_INT(run_print(&r, "-p --allowedTools Bash -- one", 0), 0);
    CHECK(!marker(root, "skillhook.json"));         /* not before the skill was used */
    r.no_person = 0;
    repl_line(&r, "/guarded");
    CHECK(marker(root, "skillhook.json"));
    remove_rel(root, "skillhook.json");
    r.no_person = 1;
    r.tools.perm.session |= 1ul << T_BASH;
    repl_line(&r, "and the agent");
    CHECK(!marker(root, "skillhook.json"));         /* once: not a second time */
    slurp(root, "agentstop.json", txt, sizeof(txt));
    CHECK(strstr(txt, "\"hook_event_name\":\"SubagentStop\"") != 0 && strstr(txt, "\"agent_type\":\"stopper\"") != 0 &&
          strstr(txt, "\"last_assistant_message\":\"AGENT-DONE\"") != 0);
    CHECK(r.hooks.extra && r.hooks.extra->nhooks == 1);    /* the agent's went with it, the skill's stays */
    repl_free(&r);
    remove_rel(root, ".claude/skills/guarded/SKILL.md");
    remove_rel(root, ".claude/agents/stopper.md");

    /* H5 FileChanged: a matcher's file in the start directory, and watchPaths
     * from SessionStart; a change runs the hooks (file_path, event) */
    xput(root, "watched.txt", "one\n");
    xput(root, "dyn.txt", "dyn\n");
    strcpy(p, "{\"hooks\":{\"FileChanged\":[{\"matcher\":\"watched.txt\",\"hooks\":[{\"type\":\"command\",\"command\":"
              "\"sh ");
    strcat(p, root);
    strcat(p, "/mark.sh ");
    strcat(p, root);
    strcat(p, "/fc.json\"}]},{\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    strcat(p, root);
    strcat(p, "/mark.sh ");
    strcat(p, root);
    strcat(p, "/fc2.json\"}]}],\"SessionStart\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    strcat(p, root);
    strcat(p, "/watch.sh\"}]}]}}");
    xput(root, ".claude/settings.json", p);
    strcpy(p, "echo '{\"hookSpecificOutput\":{\"hookEventName\":\"SessionStart\",\"watchPaths\":[\"");
    {
        char real[600];
        if (!realpath(root, real))
            strcpy(real, root);
        strcat(p, real);
    }
    strcat(p, "/dyn.txt\"]}}'\n");
    xput(root, "watch.sh", p);
    setup_in(&r, none, root);
    add_answer(0, 0, 0, "hi");
    repl_line(&r, "hi");                            /* SessionStart: the watch starts */
    xput(root, "watched.txt", "two\n");
    strcpy(p, root);
    strcat(p, "/watched.txt");
    age_file(p, 50);
    xput(root, "dyn.txt", "changed\n");
    strcpy(p, root);
    strcat(p, "/dyn.txt");
    age_file(p, 50);
    add_answer("toolu_HF1", "Read", "{\"file_path\":\"S/Startup-Sequence\"}", 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "look");
    CHECK((int)r.n_file_changed >= 2);
    slurp(root, "fc.json", txt, sizeof(txt));
    CHECK(strstr(txt, "\"hook_event_name\":\"FileChanged\"") != 0 && strstr(txt, "/watched.txt\"") != 0 &&
          strstr(txt, "\"event\":\"change\"") != 0);
    slurp(root, "fc2.json", txt, sizeof(txt));
    CHECK(strstr(txt, "/dyn.txt\"") != 0 || strstr(txt, "/watched.txt\"") != 0);
    remove_rel(root, "watched.txt");
    CHECK_INT(pol_files_changed(&r, 0), 0);          /* within two seconds: not looked at */
    cn.clock += 3000;
    CHECK(pol_files_changed(&r, 0) >= 1);
    slurp(root, "fc.json", txt, sizeof(txt));
    CHECK(strstr(txt, "\"event\":\"unlink\"") != 0);
    repl_free(&r);
    remove_rel(root, ".claude/settings.json");

    /* H6 MessageDisplay: the screen draws the hook's displayContent, the
     * conversation keeps the text; print mode: once a message, all its text */
    xput(root, "md.sh", "cat > /dev/null\nprintf '%s\\n' '{\"hookSpecificOutput\":{\"hookEventName\":\"MessageDisplay\","
                        "\"displayContent\":\"SHOWN-X\\n\"}}'\n");
    strcpy(p, "{\"hooks\":{\"MessageDisplay\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    strcat(p, root);
    strcat(p, "/md.sh\"}]}]}}");
    xput(root, ".claude/settings.json", p);
    setup_in(&r, none, root);
    add_answer(0, 0, 0, "line one\nline two\n");
    add_answer(0, 0, 0, "next");
    repl_line(&r, "say it");
    CHECK(snt.text.p && strstr(snt.text.p, "SHOWN-X") != 0 && strstr(snt.text.p, "line one") == 0);
    CHECK(r.n_md >= 2);
    repl_line(&r, "again");
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "line one\\nline two\\n") != 0);  /* the original kept */
    repl_free(&r);
    strcpy(p, "{\"hooks\":{\"MessageDisplay\":[{\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    strcat(p, root);
    strcat(p, "/mark.sh ");
    strcat(p, root);
    strcat(p, "/md.json\"}]}]}}");
    xput(root, ".claude/settings.json", p);
    setup_in(&r, none, root);
    add_answer(0, 0, 0, "alpha\nbeta");
    CHECK_INT(run_print(&r, "-p go", 0), 0);
    slurp(root, "md.json", txt, sizeof(txt));
    CHECK(strstr(txt, "\"index\":0,\"final\":true,\"delta\":\"alpha\\nbeta\"") != 0 &&
          strstr(txt, "\"turn_id\":\"") != 0 && strstr(txt, "\"message_id\":\"") != 0);
    CHECK(strstr(outp(), "alpha\nbeta") != 0);
    repl_free(&r);

    /* H8 PreToolUse defer (print mode, one call): the run stops with
     * tool_deferred; --resume runs the call (PreToolUse again: now allow) */
    xput(root, "defer.sh", "cat > /dev/null\nif [ -f \"$1\" ]; then\n"
                           "echo '{\"hookSpecificOutput\":{\"hookEventName\":\"PreToolUse\",\"permissionDecision\":\"allow\"}}'\n"
                           "else\ntouch \"$1\"\n"
                           "echo '{\"hookSpecificOutput\":{\"hookEventName\":\"PreToolUse\",\"permissionDecision\":\"defer\"}}'\n"
                           "fi\n");
    strcpy(p, "{\"hooks\":{\"PreToolUse\":[{\"matcher\":\"Bash\",\"hooks\":[{\"type\":\"command\",\"command\":\"sh ");
    strcat(p, root);
    strcat(p, "/defer.sh ");
    strcat(p, root);
    strcat(p, "/deferred.flag\"}]}]}}");
    xput(root, ".claude/settings.json", p);
    {
        char id[40];
        setup_in(&r, none, root);
        add_answer("toolu_HD1", "Bash", "{\"command\":\"echo DEFERRED-RAN\"}", 0);
        CHECK_INT(run_print(&r, "-p --output-format json -- go", 0), 0);
        CHECK(strstr(outp(), "\"stop_reason\":\"tool_deferred\",\"deferred_tool_use\":{\"id\":\"toolu_HD1\",\"name\":"
                             "\"Bash\",\"input\":{\"command\":\"echo DEFERRED-RAN\"}}") != 0);
        CHECK_INT(sb.nreq, 1);
        cl_copy(id, r.sess.id, sizeof(id));
        repl_free(&r);
        setup_in(&r, none, root);
        add_answer(0, 0, 0, "resumed and done");
        strcpy(p, "-p --allowedTools Bash --resume ");
        strcat(p, id);
        CHECK_INT(run_print(&r, p, 0), 0);
        CHECK(sb.nreq == 1 && strstr(sb.body[0], "DEFERRED-RAN\\n") != 0 &&
              strstr(sb.body[0], "Continue from where you left off.") != 0);
        CHECK(strstr(outp(), "resumed and done") != 0);
        repl_free(&r);
        remove_rel(root, "deferred.flag");
    }

    /* H10 PermissionRequest's updatedInput: allow with another input -- the
     * call runs with it */
    strcpy(p, "cat > /dev/null\necho '{\"hookSpecificOutput\":{\"hookEventName\":\"PermissionRequest\","
              "\"decision\":{\"behavior\":\"allow\",\"updatedInput\":{\"command\":\"touch ");
    strcat(p, root);
    strcat(p, "/updated.txt\"}}}}'\n");
    xput(root, "permreq.sh", p);
    strcpy(p, "{\"hooks\":{\"PermissionRequest\":[{\"matcher\":\"Bash\",\"hooks\":[{\"type\":\"command\",\"command\":"
              "\"sh ");
    strcat(p, root);
    strcat(p, "/permreq.sh\"}]}]}}");
    xput(root, ".claude/settings.json", p);
    setup_in(&r, none, root);
    strcpy(p, "{\"command\":\"touch ");
    strcat(p, root);
    strcat(p, "/original.txt\"}");
    add_answer("toolu_HP1", "Bash", p, 0);
    add_answer(0, 0, 0, "done");
    CHECK_INT(run_print(&r, "-p go", 0), 0);
    CHECK(sb.nreq == 2 && marker(root, "updated.txt") && !marker(root, "original.txt"));
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "/original.txt\"}") != 0);  /* the conversation keeps the call */
    CHECK_INT((int)r.n_perm_rerun, 1);
    repl_free(&r);
    remove_rel(root, ".claude/settings.json");

    /* H9 workspace trust: a folder never trusted is asked about once; no
     * ends the program before any hook, yes is remembered and the settings
     * count; print mode is never asked: the project's allow rules wait */
    {
        char troot[600], t2[600], real[600], home[600];
        const char *base = getenv("TMPDIR");
        strcpy(troot, base && *base ? base : "/tmp");
        if (troot[strlen(troot) - 1] == '/')
            troot[strlen(troot) - 1] = 0;
        strcat(troot, "/claude_trust_XXXXXX");
        if (mkdtemp(troot)) {
            strcpy(p, "{\"permissions\":{\"allow\":[\"Bash(touch *)\"]},\"hooks\":{\"SessionStart\":[{\"hooks\":"
                      "[{\"type\":\"command\",\"command\":\"sh ");
            strcat(p, root);
            strcat(p, "/mark.sh ");
            strcat(p, troot);
            strcat(p, "/started.json\"}]}]}}");
            xput(troot, ".claude/settings.json", p);
            setup_in(&r, say_no, troot);
            repl_run(&r);
            CHECK(strstr(cn.screen.p, "Do you trust the files in this folder?") != 0);
            CHECK(strstr(cn.screen.p, "This folder's settings would add: 1 allow rule, 1 hook") != 0);
            CHECK(!marker(troot, "started.json"));
            CHECK_INT(sb.nreq, 0);
            repl_free(&r);
            setup_in(&r, say_yes, troot);
            repl_run(&r);
            CHECK(marker(troot, "started.json"));       /* the hooks ran once it was trusted */
            repl_free(&r);
            strcpy(home, dir);
            strcat(home, "/home");
            slurp(home, "claude.json", txt, sizeof(txt));
            if (!realpath(troot, real))
                strcpy(real, troot);
            CHECK(strstr(txt, real) != 0 && strstr(txt, "\"hasTrustDialogAccepted\":true") != 0);
            /* a folder inside the trusted one is trusted too: its allow rule used */
            strcpy(t2, troot);
            strcat(t2, "/sub");
            mkdir(t2, 0700);
            xput(t2, ".claude/settings.json", "{\"permissions\":{\"allow\":[\"Bash(touch *)\"]}}");
            setup_in(&r, none, t2);
            strcpy(p, "{\"command\":\"touch ");
            strcat(p, t2);
            strcat(p, "/made.txt\"}");
            add_answer("toolu_HT1", "Bash", p, 0);
            add_answer(0, 0, 0, "done");
            CHECK_INT(run_print(&r, "-p go", 0), 0);
            CHECK(strstr(pc.err.p ? pc.err.p : "", "not been trusted") == 0);
            CHECK(marker(t2, "made.txt"));
            repl_free(&r);
            /* another folder, print mode: never asked, its allow rule not used */
            strcpy(t2, base && *base ? base : "/tmp");
            if (t2[strlen(t2) - 1] == '/')
                t2[strlen(t2) - 1] = 0;
            strcat(t2, "/claude_trust_XXXXXX");
            if (mkdtemp(t2)) {
                xput(t2, ".claude/settings.json", "{\"permissions\":{\"allow\":[\"Bash(touch *)\"]}}");
                setup_in(&r, none, t2);
                strcpy(p, "{\"command\":\"touch ");
                strcat(p, t2);
                strcat(p, "/made.txt\"}");
                add_answer("toolu_HT2", "Bash", p, 0);
                add_answer(0, 0, 0, "done");
                CHECK_INT(run_print(&r, "-p go", 0), 0);
                CHECK(strstr(pc.err.p ? pc.err.p : "", "Warning: this workspace has not been trusted") != 0);
                CHECK(!marker(t2, "made.txt"));
                repl_free(&r);
                strcpy(p, "rm -rf ");
                strcat(p, t2);
                if (system(p))
                    printf("  [ERROR] could not remove %s\n", t2);
            }
        }
        strcpy(p, "rm -rf ");
        strcat(p, troot);
        if (system(p))
            printf("  [ERROR] could not remove %s\n", troot);
    }
}

/* S1 settings warnings, S2 CLAUDE_CODE_* variables, A1 an agent's memory and
 * colour, A2 skillOverrides and disableSkillShellExecution, A3 /skill-doctor's
 * counts and /skills NAME STATE, A4 /simplify, C1 --system-prompt-snapshot, M1
 * memory imports, V1 /advisor */
static void test_gaps2_more(void)
{
    static const char *none[] = { 0 };
    static const char *doctor[] = { "/doctor", "/exit", 0 };
    static const char *say_yes[] = { "y", "/exit", 0 };
    static cl_repl r;
    char root[600], p[1400], txt[4096], id[40];
    strcpy(root, dir);
    strcat(root, "/gaps2m");
    mkdir(root, 0700);
    xput(root, "S/Startup-Sequence", "SetPatch QUIET\n");

    /* S1 a malformed entry: a warning each, the rest in effect */
    xput(root, ".claude/settings.json",
         "{\"permissions\":{\"allow\":[\"Bash(\",\"Read\"]},\"hooks\":{\"NoSuchEvent\":[]},"
         "\"skillOverrides\":{\"x\":\"sometimes\"}}");
    setup_in(&r, doctor, root);
    repl_run(&r);
    CHECK(strstr(cn.screen.p, "Settings Warning") != 0);
    CHECK(strstr(cn.screen.p, "malformed permission rule in permissions.allow: \"Bash(\" skipped") != 0);
    CHECK(strstr(cn.screen.p, "unknown hook event \"NoSuchEvent\" skipped") != 0);
    CHECK(strstr(cn.screen.p, "skillOverrides.x is not on, name-only, user-invocable-only or off; skipped") != 0);
    CHECK_INT(r.cfg.nwarn, 3);
    CHECK(r.cfg.nrules == 1);                       /* Read stays */
    {
        const char *d1 = strstr(cn.screen.p, "Settings entries skipped: 3");
        CHECK(d1 && strstr(d1, "NoSuchEvent") != 0);   /* /doctor lists them again */
    }
    repl_free(&r);

    /* S2 the variables, from the settings' env block too */
    xput(root, "CLAUDE.md", "MEMO-MARK\n");
    xput(root, ".claude/settings.json",
         "{\"env\":{\"CLAUDE_CODE_MAX_OUTPUT_TOKENS\":\"1234\",\"CLAUDE_CODE_EXTRA_BODY\":"
         "\"{\\\"metadata\\\":{\\\"user_id\\\":\\\"u-extra\\\"}}\",\"CLAUDE_CODE_DISABLE_CRON\":\"1\","
         "\"CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC\":\"1\",\"CLAUDE_CODE_DISABLE_CLAUDE_MDS\":\"1\"}}");
    setup_in(&r, none, root);
    add_answer(0, 0, 0, "hi");
    CHECK_INT(run_print(&r, "-p hi", 0), 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "\"max_tokens\":1234,") != 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "\"metadata\":{\"user_id\":\"u-extra\"}") != 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "{\"name\":\"CronCreate\"") == 0 &&
          strstr(sb.body[0], "{\"name\":\"Monitor\"") == 0 && strstr(sb.body[0], "{\"name\":\"TaskStop\"") != 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "MEMO-MARK") == 0);
    repl_free(&r);
    unsetenv("CLAUDE_CODE_MAX_OUTPUT_TOKENS");
    unsetenv("CLAUDE_CODE_EXTRA_BODY");
    unsetenv("CLAUDE_CODE_DISABLE_CRON");
    unsetenv("CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC");
    unsetenv("CLAUDE_CODE_DISABLE_CLAUDE_MDS");
    remove_rel(root, ".claude/settings.json");
    remove_rel(root, "CLAUDE.md");

    /* A1 an agent with memory: project (its MEMORY.md in its prompt, Write
     * and Edit to its directory) and color: cyan (its call's header) */
    xput(root, ".claude/agents/keeper.md",
         "---\nname: keeper\ndescription: keeps notes\ntools: Read\nmemory: project\ncolor: cyan\n---\nYou keep.\n");
    xput(root, ".claude/agent-memory/keeper/MEMORY.md", "KEEPER-NOTE\n");
    setup_in(&r, none, root);
    add_answer("toolu_GA1", "Task", "{\"description\":\"keep\",\"prompt\":\"p\",\"subagent_type\":\"keeper\"}", 0);
    add_answer("toolu_GA2", "Write",
               "{\"file_path\":\".claude/agent-memory/keeper/notes.md\",\"content\":\"NEW-NOTE\\n\"}", 0);
    add_answer(0, 0, 0, "KEPT");
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "keep notes");
    CHECK_INT(sb.nreq, 4);
    CHECK(sb.nreq == 4 && strstr(sb.body[1], "# Your memory") != 0 && strstr(sb.body[1], "KEEPER-NOTE") != 0 &&
          strstr(sb.body[1], "{\"name\":\"Write\",") != 0 && strstr(sb.body[1], "{\"name\":\"Edit\",") != 0);
    slurp(root, ".claude/agent-memory/keeper/notes.md", txt, sizeof(txt));
    CHECK_STR(txt, "NEW-NOTE\n");
    CHECK(strstr(cn.screen.p, "\033[36mTask") != 0);
    repl_free(&r);
    remove_rel(root, ".claude/agents/keeper.md");

    /* A2 skillOverrides: off and user-invocable-only kept from Claude,
     * name-only without its description; disableSkillShellExecution */
    xput(root, ".claude/skills/g2hush/SKILL.md", "---\ndescription: HUSH-DESC\n---\nHUSH-BODY\n");
    xput(root, ".claude/skills/g2bare/SKILL.md", "---\ndescription: BARE-DESC\n---\nBARE-BODY\n");
    xput(root, ".claude/skills/g2mine/SKILL.md", "---\ndescription: MINE-DESC\n---\nMINE-BODY !`echo SHELL-RAN`\n");
    xput(root, ".claude/skills/g2plain/SKILL.md", "---\ndescription: PLAIN-DESC\n---\nPLAIN-BODY\n");
    xput(root, ".claude/settings.json",
         "{\"skillOverrides\":{\"g2hush\":\"off\",\"g2bare\":\"name-only\",\"g2mine\":\"user-invocable-only\"},"
         "\"disableSkillShellExecution\":true}");
    setup_in(&r, none, root);
    add_answer("toolu_GS1", "Skill", "{\"skill\":\"g2mine\"}", 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "hi");
    CHECK(sb.nreq == 2 && strstr(sb.body[0], "PLAIN-DESC") != 0 && strstr(sb.body[0], "g2bare") != 0 &&
          strstr(sb.body[0], "BARE-DESC") == 0 && strstr(sb.body[0], "HUSH-DESC") == 0 &&
          strstr(sb.body[0], "MINE-DESC") == 0);
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "Unknown skill: g2mine") != 0);  /* not listed to Claude */
    stub_reset();
    repl_line(&r, "/g2hush");
    CHECK_INT(sb.nreq, 0);
    CHECK(strstr(cn.screen.p, "This skill is turned off by skillOverrides in the settings: g2hush") != 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "/g2mine");
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "MINE-BODY [shell command execution disabled by policy]") != 0 &&
          strstr(sb.body[0], "SHELL-RAN") == 0);

    /* A3 /skill-doctor: the cost and the uses (g2mine used once, the rest
     * never; the unused flagged); /skills NAME off saves the state */
    stub_reset();
    repl_line(&r, "/skill-doctor");
    CHECK(strstr(cn.screen.p, "g2mine: ~0 tokens, used 1 time") != 0);
    CHECK(strstr(cn.screen.p, "g2plain: ~") != 0 &&
          strstr(strstr(cn.screen.p, "g2plain: ~"), "used never  <- turn it off?") != 0);
    CHECK(strstr(cn.screen.p, "simplify:") == 0);   /* the bundled ones are not counted */
    repl_line(&r, "/skills g2plain off");
    CHECK(strstr(cn.screen.p, "g2plain is now off") != 0);
    slurp(root, ".claude/settings.local.json", txt, sizeof(txt));
    CHECK(strstr(txt, "\"skillOverrides\"") != 0 && strstr(txt, "\"g2plain\": \"off\"") != 0);
    CHECK_STR(cfg_skill_state(&r.cfg, "g2plain"), "off");
    stub_reset();
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "again");
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "PLAIN-DESC") == 0);
    repl_free(&r);
    remove_rel(root, ".claude/settings.local.json");
    remove_rel(root, ".claude/settings.json");

    /* A4 /simplify: four reviews, one after another (no parallel agents here) */
    setup_in(&r, none, root);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "/simplify");
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "Run four reviews with the Task tool (general-purpose agents), one "
                                             "after another") != 0);
    repl_free(&r);

    /* C1 --system-prompt-snapshot: the first launch's flags' text kept by a
     * --continue with other flags; off takes this launch's */
    setup_in(&r, none, root);
    add_answer(0, 0, 0, "one");
    CHECK_INT(run_print(&r, "-p --append-system-prompt APPEND-ONE -- hi", 0), 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "APPEND-ONE") != 0);
    cl_copy(id, r.sess.id, sizeof(id));
    repl_free(&r);
    setup_in(&r, none, root);
    add_answer(0, 0, 0, "two");
    strcpy(p, "-p --resume ");
    strcat(p, id);
    strcat(p, " --append-system-prompt APPEND-TWO -- again");
    CHECK_INT(run_print(&r, p, 0), 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "APPEND-ONE") != 0 && strstr(sb.body[0], "APPEND-TWO") == 0);
    repl_free(&r);
    setup_in(&r, none, root);
    add_answer(0, 0, 0, "three");
    strcpy(p, "-p --system-prompt-snapshot off --resume ");
    strcat(p, id);
    strcat(p, " --append-system-prompt APPEND-TWO -- again");
    CHECK_INT(run_print(&r, p, 0), 0);
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "APPEND-ONE") == 0 && strstr(sb.body[0], "APPEND-TWO") != 0);
    repl_free(&r);

    /* M1 imports: four hops deep, an escaped space, a quoted path not
     * imported; one outside the folder held until approved (asked once) */
    {
        xput(dir, "g2outside.md", "MEM-OUT\n");
        xput(root, "CLAUDE.md", "@a.md @my\\ notes.md @\"q.md\" `@k.md` @../g2outside.md\n");
        xput(root, "a.md", "MEM-A @b.md\n");
        xput(root, "b.md", "MEM-B @c.md\n");
        xput(root, "c.md", "MEM-C @d.md\n");
        xput(root, "d.md", "MEM-D @e.md\n");
        xput(root, "e.md", "MEM-E\n");
        xput(root, "my notes.md", "MEM-SPACE\n");
        xput(root, "q.md", "MEM-Q\n");
        xput(root, "k.md", "MEM-K\n");
        setup_in(&r, none, root);
        add_answer(0, 0, 0, "ok");
        CHECK_INT(run_print(&r, "-p hi", 0), 0);
        CHECK(sb.nreq == 1 && strstr(sb.body[0], "MEM-A") && strstr(sb.body[0], "MEM-B") &&
              strstr(sb.body[0], "MEM-C") && strstr(sb.body[0], "MEM-D") && !strstr(sb.body[0], "MEM-E"));
        CHECK(sb.nreq == 1 && strstr(sb.body[0], "MEM-SPACE") && !strstr(sb.body[0], "MEM-Q") &&
              !strstr(sb.body[0], "MEM-K") && !strstr(sb.body[0], "MEM-OUT"));
        repl_free(&r);
        setup_in(&r, say_yes, root);
        repl_run(&r);
        CHECK(strstr(cn.screen.p, "imports files from outside the start directory") != 0 &&
              strstr(cn.screen.p, "g2outside.md") != 0);
        repl_free(&r);
        setup_in(&r, none, root);
        add_answer(0, 0, 0, "ok");
        CHECK_INT(run_print(&r, "-p hi", 0), 0);
        CHECK(sb.nreq == 1 && strstr(sb.body[0], "MEM-OUT") != 0);
        repl_free(&r);
        setup_in(&r, none, root);
        repl_run(&r);
        CHECK(strstr(cn.screen.p, "imports files from outside") == 0);  /* asked once */
        repl_free(&r);
        remove_rel(root, "CLAUDE.md");
        remove_rel(dir, "g2outside.md");
    }

    /* V1 /advisor: the advisor server tool and its beta in the requests;
     * off takes it away; --advisor haiku is refused at launch */
    setup_in(&r, none, root);
    repl_line(&r, "/advisor fable");
    CHECK(strstr(cn.screen.p, "Advisor set to ") != 0);
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "hi");
    CHECK(sb.nreq == 1 && strstr(sb.body[0], "{\"type\":\"advisor_20260301\",\"name\":\"advisor\",\"model\":") != 0);
    CHECK(sb.nreq == 1 && strstr(sb.head[0], "advisor-tool-2026-03-01") != 0);
    repl_line(&r, "/advisor off");
    add_answer(0, 0, 0, "ok");
    repl_line(&r, "again");
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "advisor_20260301") == 0 && strstr(sb.head[1], "advisor-tool") == 0);
    repl_free(&r);
    setup_in(&r, none, root);
    {
        cl_cli c;
        cli_init(&c);
        CHECK_INT(cli_parse_line(&c, "-p --advisor haiku -- hi"), 0);
        CHECK_INT(cli_apply(&c, &r), -1);
        CHECK(strstr(c.err, "Error: --advisor haiku cannot advise this model") == c.err);
        cli_free(&c);
    }
    repl_free(&r);
}

/* ---- A4 gaps 3 (TUI side): the screen's rows, each driven once through
 * the REPL core on the engine's screen (ledger
 * thoughts/shared/plans/2026-10-05-a4-gaps3-tui-progress.md) ---- */

/* a REPL with its screen in root (made), the keys typed in turn */
static void g3_screen(cl_repl *r, const char **keys, const char *sub_dir, char *root)
{
    strcpy(root, dir);
    strcat(root, "/");
    strcat(root, sub_dir);
    mkdir(root, 0700);
    cs_open(80, 30, keys);
    cs_io(&io);
    io.log = 0;
    net.u = 0;
    net.open = s_open;
    net.send = s_send;
    net.recv = s_recv;
    net.close = s_close;
    net.err = s_err;
    sys_posix_init(&sp, &sys);
    CHECK_INT(repl_init(r, &io, &net, &sys, CL_DEFAULT_URL, "test-key-not-real", root), 0);
}

static void g3_dump(void)
{
    if (getenv("CL_DUMP")) {
        int k;
        for (k = 0; k < cs.rows; k++)
            printf("%2d|%s\n", k, cs_row(k));
    }
}

/* G3: --allow-dangerously-skip-permissions puts bypass in Shift+Tab's
 * cycle; three presses reach it and Claude's Write runs unasked */
static void test_gaps3_bypass(void)
{
    static const char *keys[] = { "\033[Z", "\033[Z", "\033[Z", "write it\r", "/exit\r", 0 };
    static cl_repl r;
    char root[600];
    stub_reset();
    g3_screen(&r, keys, "g3bypass", root);
    r.allow_bypass = 1;             /* what cli_apply sets for the flag */
    repl_load(&r);
    add_answer("toolu_G3W", "Write", "{\"file_path\":\"g3.txt\",\"content\":\"bypassed\\n\"}", 0);
    add_answer(0, 0, 0, "Written.");
    CHECK_INT(repl_screen(&r), 0);
    repl_run(&r);
    g3_dump();
    CHECK_INT(cs.next, 5);                  /* no question took a key */
    CHECK(strstr(cs.sent.p, " bypass permissions on\033[0m\033[2m (shift+tab to cycle)") != 0);
    CHECK_INT(r.tools.perm.mode, PERM_BYPASS);
    CHECK(has("g3bypass/g3.txt", "bypassed"));
    repl_free(&r);
    cs_close();
}

/* the box's frame colour (its top-left corner's foreground) seen before each read */
static int g3_fg[8];
static void g3_look_frame(void)
{
    int row = cs_find("\342\225\255");
    if (cs.next < 8)
        g3_fg[cs.next] = row >= 0 ? h_cell(cs.vt, 0, row)->fg : -1;
}

/* G4: /color red draws the prompt bar red, default puts the theme's grey
 * back, a name that is no colour says how */
static void test_gaps3_color(void)
{
    static const char *keys[] = { "/color red\r", "/color default\r", "/color mauve\r", "/color\r", "/exit\r", 0 };
    static cl_repl r;
    char root[600];
    stub_reset();
    g3_screen(&r, keys, "g3color", root);
    CHECK_INT(repl_screen(&r), 0);
    memset(g3_fg, 0, sizeof(g3_fg));
    cs.before_read = g3_look_frame;
    repl_run(&r);
    cs.before_read = 0;
    g3_dump();
    CHECK_INT(cs.next, 5);
    CHECK_INT(g3_fg[0], 8);                 /* the theme's grey */
    CHECK_INT(g3_fg[1], 1);                 /* red */
    CHECK_INT(g3_fg[2], 8);                 /* default */
    CHECK(strstr(cs.sent.p, "Usage: /color [red|blue|green|yellow|purple|orange|pink|cyan|default]") != 0);
    CHECK(r.bar_color[0] && theme_named(r.bar_color) && g3_fg[4] != 8);    /* no argument: one of the eight */
    repl_free(&r);
    cs_close();
}

/* G5: Alt+T turns thinking off for the next turn on a model that may go
 * without (thinking disabled, effort xhigh sent as high) and on again */
static void test_gaps3_think(void)
{
    static const char *keys[] = { "\033t", "hi\r", "\033t", "again\r", "/exit\r", 0 };
    static cl_repl r;
    char root[600];
    stub_reset();
    g3_screen(&r, keys, "g3think", root);
    cl_copy(r.model, "claude-opus-4-7", sizeof(r.model));
    cl_copy(r.effort, "xhigh", sizeof(r.effort));
    add_answer(0, 0, 0, "one");
    add_answer(0, 0, 0, "two");
    CHECK_INT(repl_screen(&r), 0);
    repl_run(&r);
    g3_dump();
    CHECK_INT(cs.next, 5);
    CHECK_INT(sb.nreq, 2);
    CHECK(sb.nreq == 2 && strstr(sb.body[0], "\"thinking\":{\"type\":\"disabled\"}") != 0);
    CHECK(sb.nreq == 2 && strstr(sb.body[0], "\"output_config\":{\"effort\":\"high\"}") != 0);
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "\"thinking\":{\"type\":\"adaptive\"") != 0);
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "\"effort\":\"xhigh\"") != 0);
    CHECK(strstr(cs.sent.p, "Thinking off") != 0 && strstr(cs.sent.p, "Thinking on") != 0);
    repl_free(&r);
    cs_close();
}

/* G6: /add-dir's argument completed with Tab from the start directory's
 * subdirectories, through the REPL's own completion (ui_attach) */
static void test_gaps3_dirs(void)
{
    static const char *keys[] = { "/add-dir proj", "\t", "\r", "/exit\r", 0 };
    static cl_repl r;
    char root[600];
    stub_reset();
    g3_screen(&r, keys, "g3dirs", root);
    xput(root, "projects/readme.txt", "x\n");
    xput(root, "proj.txt", "x\n");
    CHECK_INT(repl_screen(&r), 0);
    repl_run(&r);
    g3_dump();
    CHECK_INT(cs.next, 4);
    CHECK(cs_find("> /add-dir projects/") >= 0);    /* Tab took the directory, not proj.txt */
    CHECK(cs_find("Claude may now read") >= 0);
    CHECK(r.cfg.ndirs == 1 && strstr(r.cfg.dirs[0], "g3dirs/projects") != 0);
    repl_free(&r);
    cs_close();
}

/* G7: @ offers the project's subagent by name; Tab writes @agent-NAME;
 * the prompt goes with the note that has Claude invoke that agent; an
 * @agent- of no agent is left alone */
static void test_gaps3_agent_mention(void)
{
    static const char *keys[] = { "ask @code", "\t", "to look\r", "and @agent-nobody here\r", "/exit\r", 0 };
    static cl_repl r;
    char root[600];
    const char *once;
    stub_reset();
    g3_screen(&r, keys, "g3agent", root);
    xput(root, ".claude/agents/code-reviewer.md",
         "---\nname: code-reviewer\ndescription: Reviews code\n---\nReview it.\n");
    repl_load(&r);
    add_answer(0, 0, 0, "Asking it.");
    add_answer(0, 0, 0, "No such agent.");
    CHECK_INT(repl_screen(&r), 0);
    repl_run(&r);
    g3_dump();
    CHECK_INT(cs.next, 5);
    CHECK_INT(sb.nreq, 2);
    CHECK(sb.nreq >= 1 && strstr(sb.body[0], "ask @agent-code-reviewer to look") != 0);
    CHECK(sb.nreq >= 1 && strstr(sb.body[0], "The user has expressed a desire to invoke the agent "
                                             "\\\"code-reviewer\\\".") != 0);
    CHECK(sb.nreq >= 2 && (once = strstr(sb.body[1], "desire to invoke")) != 0 &&
          strstr(once + 1, "desire to invoke") == 0);     /* the first prompt's note only */
    repl_free(&r);
    cs_close();
}

/* G8: askUserQuestionTimeout's clock: each wait in the open menu passes
 * 25 s; while the window reports the focus it does not count */
static cl_repl *g3_r;
static int g3_countdown, g3_modal_reads;
static void g3_afk_clock(void)
{
    if (g3_r && g3_r->tui && g3_r->tui->modal) {
        g3_modal_reads++;
        cs.clock += 25000;
        if (cs_find("No answer: going on without you in") >= 0)
            g3_countdown = 1;
    }
}

static void test_gaps3_afk(void)
{
    static const char *keys[] = { "which size?\r", "\033[I", "", "", "", "\033[O", "", "", "", "/exit\r", 0 };
    static cl_repl r;
    char root[600];
    stub_reset();
    setenv("CLAUDE_AFK_TIMEOUT_MS", "60000", 1);
    g3_screen(&r, keys, "g3afk", root);
    add_answer("toolu_G3Ask", "AskUserQuestion",
               "{\"questions\":[{\"question\":\"Which size?\",\"header\":\"Size\",\"options\":[{\"label\":\"80x24\","
               "\"description\":\"classic\"},{\"label\":\"132x50\",\"description\":\"big\"}],\"multiSelect\":false}]}",
               0);
    add_answer(0, 0, 0, "I will pick 80x24.");
    CHECK_INT(repl_screen(&r), 0);
    CHECK_INT(r.ui.afk_ms, 60000);
    g3_r = &r;
    g3_countdown = g3_modal_reads = 0;
    cs.before_read = g3_afk_clock;
    repl_run(&r);
    cs.before_read = 0;
    g3_r = 0;
    unsetenv("CLAUDE_AFK_TIMEOUT_MS");
    g3_dump();
    CHECK_INT(cs.next, 10);
    /* the menu's reads: focus in, three waits that do not count, focus out
     * (25 s), 25 s more (the countdown), 25 s more: gone on (without the
     * focus pause it would have gone after three) */
    CHECK_INT(g3_modal_reads, 7);
    CHECK_INT(sb.nreq, 2);
    CHECK(sb.nreq == 2 && strstr(sb.body[1], "The user did not answer your questions in time and may be away "
                                             "from the keyboard.") != 0);
    CHECK_INT(g3_countdown, 1);             /* the last 20 s counted down on the screen */
    repl_free(&r);
    cs_close();
}

/* G9: after a turn with a warm cache, the box shows the next prompt Claude
 * predicts (a background request, no spinner); Tab puts it in, Enter sends
 * it; after a cold-cache turn none is asked for */
static void test_gaps3_suggest(void)
{
    static const char *keys[] = { "hi\r", "\t", "\r", "/exit\r", 0 };
    static cl_repl r;
    char root[600];
    stub_reset();
    setenv("CLAUDE_CODE_ENABLE_PROMPT_SUGGESTION", "true", 1);
    g3_screen(&r, keys, "g3suggest", root);
    add_stream("tool_final.sse");           /* cache_read_input_tokens 1500: warm */
    add_answer(0, 0, 0, "run the tests\n");
    add_answer(0, 0, 0, "ok");              /* a cold one: no suggestion after it */
    CHECK_INT(repl_screen(&r), 0);
    repl_run(&r);
    setenv("CLAUDE_CODE_ENABLE_PROMPT_SUGGESTION", "false", 1);
    g3_dump();
    CHECK_INT(cs.next, 4);
    CHECK_INT(sb.nreq, 3);
    CHECK(sb.nreq >= 2 && strstr(sb.body[1], "Predict what the user is most likely to type next") != 0);
    CHECK(sb.nreq >= 3 && strstr(sb.body[2], "{\"type\":\"text\",\"text\":\"run the tests\"}") != 0);
    CHECK_INT(r.n_suggested, 1);
    CHECK(cs_find("> run the tests") >= 0);
    CHECK(strstr(cs.sent.p, "Predict what") == 0);  /* nothing of it on the screen */
    repl_free(&r);
    cs_close();
}

/* The rig's claude2-rewind symptom, C:Claude's side: a prompt typed (with
 * its Return) while the prompt suggestion's background request is being
 * answered stops the request and is still sent as one line, and the next
 * line ("/help") is still a command; the same after the suggestion is
 * shown. (Passes on main: given '\r' and '/', the box is right. The rig
 * box -- Return a new line, a keypad '/' gone -- is the window's own
 * cooked line editor, i.e. the console left termios mode.) */
static const char *g3_ta_keys[] = { "hi\r", "please edit it\r", "/help\r", "/exit\r", 0 };
static void g3_typeahead(void)
{
    /* typed only while the suggestion's answer is coming in */
    g3_ta_keys[1] = sb.nreq == 2 && sb.cur == 1 && sb.pos > 0 ? "!please edit it\r" : "please edit it\r";
}

static void test_gaps3_suggest_typeahead(void)
{
    static cl_repl r;
    char root[600];
    stub_reset();
    setenv("CLAUDE_CODE_ENABLE_PROMPT_SUGGESTION", "true", 1);
    g3_screen(&r, g3_ta_keys, "g3suggest_ta", root);
    add_stream("tool_final.sse");           /* cache_read_input_tokens 1500: warm */
    add_answer(0, 0, 0, "run the tests\n");
    add_answer(0, 0, 0, "edited");
    CHECK_INT(repl_screen(&r), 0);
    cs.before_read = g3_typeahead;
    repl_run(&r);
    cs.before_read = 0;
    g3_ta_keys[1] = "please edit it\r";
    setenv("CLAUDE_CODE_ENABLE_PROMPT_SUGGESTION", "false", 1);
    g3_dump();
    CHECK_INT(cs.next, 4);
    CHECK_INT(sb.nreq, 3);
    CHECK(sb.nreq >= 2 && strstr(sb.body[1], "Predict what the user is most likely to type next") != 0);
    /* the typed prompt went out as one line, its Return no newline */
    CHECK(sb.nreq >= 3 && strstr(sb.body[2], "{\"type\":\"text\",\"text\":\"please edit it\"}") != 0);
    CHECK_INT(r.n_suggested, 0);            /* the key dropped it */
    CHECK(strstr(cs.sent.p, "Not on the Amiga (type one to see why)") != 0);   /* /help ran */
    repl_free(&r);
    cs_close();
    {
        /* the rig's order: the suggestion (the fixture's text.sse) shown, then typed */
        static const char *k2[] = { "hi\r", "please edit it", "\r", "/help", "\r", "/exit\r", 0 };
        stub_reset();
        setenv("CLAUDE_CODE_ENABLE_PROMPT_SUGGESTION", "true", 1);
        g3_screen(&r, k2, "g3suggest_ta", root);
        add_stream("tool_final.sse");
        add_stream("text.sse");
        add_answer(0, 0, 0, "edited");
        CHECK_INT(repl_screen(&r), 0);
        repl_run(&r);
        setenv("CLAUDE_CODE_ENABLE_PROMPT_SUGGESTION", "false", 1);
        g3_dump();
        CHECK_INT(cs.next, 6);
        CHECK_INT(sb.nreq, 3);
        CHECK(sb.nreq >= 3 && strstr(sb.body[2], "{\"type\":\"text\",\"text\":\"please edit it\"}") != 0);
        CHECK_INT(r.n_suggested, 1);
        CHECK(strstr(cs.sent.p, "Not on the Amiga (type one to see why)") != 0);
        repl_free(&r);
        cs_close();
    }
}

/* G10: after three prompts, three minutes with no key (the terminal never
 * reported its focus): the recap, made in the background, once; a fourth
 * idle minute makes no second one */
static void g3_away_clock(void)
{
    if (cs.next >= 3 && cs.script[cs.next] && !cs.script[cs.next][0])
        cs.clock += 61000;          /* each empty read: a minute and a bit */
}

static void test_gaps3_recap(void)
{
    static const char *keys[] = { "one\r", "two\r", "three\r", "", "", "", "", "/exit\r", 0 };
    static cl_repl r;
    char root[600];
    stub_reset();
    setenv("CLAUDE_CODE_ENABLE_AWAY_SUMMARY", "1", 1);
    g3_screen(&r, keys, "g3recap", root);
    add_answer(0, 0, 0, "1");
    add_answer(0, 0, 0, "2");
    add_answer(0, 0, 0, "3");
    add_answer(0, 0, 0, "We counted to three.");
    CHECK_INT(repl_screen(&r), 0);
    cs.before_read = g3_away_clock;
    repl_run(&r);
    cs.before_read = 0;
    setenv("CLAUDE_CODE_ENABLE_AWAY_SUMMARY", "0", 1);
    g3_dump();
    CHECK_INT(cs.next, 8);
    CHECK_INT(sb.nreq, 4);
    CHECK(sb.nreq == 4 && strstr(sb.body[3], "recap this session so far") != 0);
    CHECK_INT((int)r.n_recaps, 1);
    CHECK(cs_find("Recap: We counted to three.") >= 0);
    repl_free(&r);
    cs_close();
    {
        /* the window reports it has the focus: the user is there, no recap */
        static const char *k2[] = { "one\r", "two\r", "three\r", "\033[I", "", "", "", "/exit\r", 0 };
        stub_reset();
        setenv("CLAUDE_CODE_ENABLE_AWAY_SUMMARY", "1", 1);
        g3_screen(&r, k2, "g3recap", root);
        add_answer(0, 0, 0, "1");
        add_answer(0, 0, 0, "2");
        add_answer(0, 0, 0, "3");
        CHECK_INT(repl_screen(&r), 0);
        cs.before_read = g3_away_clock;
        repl_run(&r);
        cs.before_read = 0;
        setenv("CLAUDE_CODE_ENABLE_AWAY_SUMMARY", "0", 1);
        CHECK_INT(cs.next, 8);
        CHECK_INT(sb.nreq, 3);
        CHECK_INT((int)r.n_recaps, 0);
        repl_free(&r);
        cs_close();
    }
}

/* G11: /keybindings writes the defaults to <home>/keybindings.json (and
 * opens it: EDITOR is `true` here); the file changed on disk is read again
 * while the screen waits; its Ctrl+Y -> chat:modelPicker then opens the
 * picker */
static cl_repl *g3_kr;
static char g3_kfile[600];
static int g3_kdefaults;
static void g3_keys_edit(void)
{
    if (cs.next == 1 && g3_kr && g3_kr->n_keys_loads == 1) {
        struct utimbuf tb;
        char *d = 0;
        long dn = 0;
        FILE *f;
        /* what /keybindings wrote: the defaults, Claude Code's format */
        g3_kdefaults = sys.read(sys.u, g3_kfile, 100000, &d, &dn) == 0 && d &&
                       strstr(d, "\"$schema\": \"https://www.schemastore.org/claude-code-keybindings.json\"") &&
                       strstr(d, "\"ctrl+x ctrl+e\": \"chat:externalEditor\"") &&
                       strstr(d, "\"context\": \"Select\"");
        free(d);
        f = fopen(g3_kfile, "wb");
        if (f) {
            fputs("{\"bindings\":[{\"context\":\"Chat\",\"bindings\":{\"ctrl+y\":\"chat:modelPicker\"}}]}", f);
            fclose(f);
        }
        tb.actime = tb.modtime = time(0) + 10;  /* a different mtime than the one read */
        utime(g3_kfile, &tb);
        cs.clock += 2500;                       /* past the 2 s between looks */
    }
}

static void test_gaps3_keybindings(void)
{
    static const char *keys[] = { "/keybindings\r", "", "\031", "\033", "/exit\r", 0 };
    static cl_repl r;
    char root[600];
    stub_reset();
    g3_kdefaults = 0;
    setenv("EDITOR", "true", 1);
    g3_screen(&r, keys, "g3keys", root);
    CHECK(repl_keys_file(&r, g3_kfile, sizeof(g3_kfile)) == 0);
    remove(g3_kfile);
    CHECK_INT(repl_screen(&r), 0);
    CHECK_INT((int)r.n_keys_loads, 0);      /* no file yet: the defaults */
    g3_kr = &r;
    cs.before_read = g3_keys_edit;
    repl_run(&r);
    cs.before_read = 0;
    g3_kr = 0;
    unsetenv("EDITOR");
    g3_dump();
    CHECK_INT(cs.next, 5);
    CHECK_INT(g3_kdefaults, 1);
    CHECK(cs_find("Created with the default bindings") >= 0);
    CHECK_INT((int)r.n_keys_loads, 2);      /* after /keybindings, after the change */
    CHECK(strstr(cs.sent.p, "Select a model") != 0);
    remove(g3_kfile);
    repl_free(&r);
    cs_close();
}

/* G12: /focus says why it is not here */
static void test_gaps3_na(void)
{
    static const char *none[] = { 0 };
    static cl_repl r;
    stub_reset();
    setup(&r, none);
    repl_line(&r, "/focus");
    CHECK(cn.screen.p && strstr(cn.screen.p, "/focus is not available on the Amiga: it is a view of Claude Code's "
                                             "fullscreen renderer") != 0);
    repl_free(&r);
}

static void test_gaps3(void)
{
    test_gaps3_na();
    test_gaps3_keybindings();
    test_gaps3_recap();
    test_gaps3_suggest();
    test_gaps3_suggest_typeahead();
    test_gaps3_afk();
    test_gaps3_agent_mention();
    test_gaps3_bypass();
    test_gaps3_color();
    test_gaps3_think();
    test_gaps3_dirs();
}

void suite_claude_repl(void)
{
    mk_tree();
    test_reach();
    test_unfinished();
    test_req_golden();
    test_commands();
    test_screen();
    test_wp1();
    test_input_rest();
    test_wp2();
    test_wp3();
    test_wp3_commands();
    test_wp4();
    test_wiring();
    test_fetch_screen();
    test_websearch_screen();
    test_continue_header_first();
    test_gaps_print();
    test_gaps_verbose();
    test_gaps_commands();
    test_gaps_perm_menu();
    test_rule_ask_comment();
    test_gaps_ext();
    test_gaps_hooks();
    test_gaps_more();
    test_gaps2_tools();
    test_gaps2_hooks();
    test_gaps2_more();
    test_gaps3();
    test_gaps3_loop();
    stub_reset();
    jw_free(&cn.screen);
    jw_free(&snt.text);
    jw_free(&sb.req);
    rm_tree();
}
