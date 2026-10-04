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
#include "harness.h"
#include "claude_load.h"
#include "../claude/repl.h"
#include "../claude/sys_posix.h"
#include "../claude/util.h"

/* ---- the stub transport: one canned HTTP response per request ---- */

typedef struct stub {
    char *resp[8];
    long rlen[8];
    int nresp, next, cur;
    long pos;
    jw req;                     /* the request being received */
    char *head[8], *body[8];
    int nreq;
    int opens;
    long chunk;                 /* bytes per recv */
    long brk_at;                /* Ctrl+C once this many bytes of a response went out (0: never) */
    int brk;
} stub;

static stub sb;

static int s_open(void *u, const char *host, int port, int tls)
{
    (void)u;
    (void)host;
    (void)port;
    (void)tls;
    sb.opens++;
    return 0;
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
        if (sb.req.n >= hl + bl && sb.nreq < 8) {
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
    return "stub";
}

/* a recording as the API sends it: chunked, 100 bytes a chunk */
static void add_stream(const char *name)
{
    long n, i;
    char *sse = claude_load(name, &n);
    jw w;
    static const char hex[] = "0123456789abcdef";
    if (!sse)
        return;
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
    free(sse);
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
    jw text;
} sentinel;

static sentinel snt;

static void sn_text(void *u, const char *s, long n)
{
    (void)u;
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

static void mk_tree(void)
{
    const char *base = getenv("TMPDIR");
    char p[600];
    FILE *f;
    strcpy(dir, base && *base ? base : "/tmp");
    if (dir[strlen(dir) - 1] == '/')
        dir[strlen(dir) - 1] = 0;
    strcat(dir, "/claude_repl_XXXXXX");
    if (!mkdtemp(dir))
        return;
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
    strcat(cmd, dir);
    if (system(cmd))
        printf("  [ERROR] could not remove %s\n", dir);
}

static cl_io io;
static cl_net net;
static sys_posix sp;
static cl_sys sys;

static void setup(cl_repl *r, const char **script)
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
    CHECK_INT(repl_init(r, &io, &net, &sys, CL_DEFAULT_URL, "test-key-not-real", dir), 0);
    jw_free(&snt.text);
    memset(&snt, 0, sizeof(snt));
    jw_init(&snt.text);
    snt.inner = r->render;
    r->render.u = 0;
    r->render.text = sn_text;
    r->render.end = sn_end;
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
    static const char *script[] = { "hello", "show me S/Startup-Sequence", "a", "/cost", "/exit", 0 };
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
    CHECK(json_get(b, "tools", &x) && json_count(x) == 6);
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
    CHECK(json_get(x, "content", &e) && json_streq(e, "SetPatch QUIET\n"));
    CHECK(!json_get(x, "is_error", &e));
    CHECK(json_next(&it, 0, &x));
    CHECK(json_get(x, "tool_use_id", &e) && json_streq(e, "toolu_01ListS"));
    CHECK(json_get(x, "content", &e) && json_streq(e, "Startup-Sequence  15\n"));

    /* the screen: the tool calls shown, one question asked, the cost */
    CHECK(strstr(cn.screen.p, "Tool \033[0mread_file") != 0);
    CHECK(strstr(cn.screen.p, "Tool \033[0mlist_dir") != 0);
    CHECK(strstr(cn.screen.p, "Always this session (a)") != 0);
    CHECK(strstr(cn.screen.p, "Requests 3. Tokens: input 855, output 150, cache write 1200, cache read 2700. Cost $") != 0);
    CHECK(strstr(cn.screen.p, "test-key-not-real") == 0);
    CHECK_INT(r.conv.n, 6);
    CHECK_INT(cn.next, 5);
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

static void test_commands(void)
{
    static const char *script[] = { 0 };
    static cl_repl r;
    setup(&r, script);
    CHECK_INT(repl_line(&r, "/model claude-sonnet-5-5"), 0);
    CHECK_STR(r.model, "claude-sonnet-5-5");
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
    /* no key for an https URL: refused before anything is sent */
    {
        static cl_repl r2;
        CHECK_INT(repl_init(&r2, &io, &net, &sys, CL_DEFAULT_URL, "", dir), -1);
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

void suite_claude_repl(void)
{
    mk_tree();
    test_reach();
    test_unfinished();
    test_commands();
    stub_reset();
    jw_free(&cn.screen);
    jw_free(&snt.text);
    jw_free(&sb.req);
    rm_tree();
}
