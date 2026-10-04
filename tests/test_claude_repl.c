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
#include "../claude/tui.h"
#include "../claude/show.h"
#include "claude_screen.h"

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
    CHECK(json_get(b, "tools", &x) && json_count(x) == 7);
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

static void test_screen(void)
{
    static const char *keys[] = {
        "hello\r", "show me S/Startup-Sequence\r", "2", "please edit the greeting\r", "\r", "/cost\r", "/exit\r", 0
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
    CHECK_INT(r.show->n_tools, 3);          /* read, list, edit (todo_write has no header call) */
    CHECK_INT(r.show->n_diffs, 1);
    CHECK(r.tui->n_lines > 20);
    /* five requests, every key used, the edit made */
    CHECK_INT(sb.nreq, 5);
    CHECK_INT(cs.next, 7);
    CHECK_INT(sys.read(sys.u, p, 1000, &after, &an), 0);
    CHECK_STR(after ? after : "", "hello from the Amiga\n");
    free(after);
    /* the reads were allowed for the session with "2": one menu for two calls */
    CHECK(sb.nreq < 3 || strstr(sb.body[2], "Startup-Sequence  15") != 0);
    CHECK(r.tools.perm.session & (1u << T_LIST_DIR));
    /* the todo list and the edit's result reached the history and the screen */
    CHECK(sb.nreq < 5 || strstr(sb.body[4], "Todos updated.") != 0);
    CHECK(sb.nreq < 5 || strstr(sb.body[4], "Edited ") != 0);
    row = cs_find("\342\226\240 Change the greeting");     /* the todo list, in progress */
    CHECK(row >= 0);
    CHECK(cs_find(SC "  Updated claude-test.txt with 1 addition and 1 removal") > row);
    CHECK(cs_find("1 - hello") > row);
    CHECK(cs_find("1 + hello from the Amiga") > row);
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
    CHECK(strstr(cs.sent.p, "\033[2J") == 0);
    CHECK(strstr(cs.sent.p, "test-key-not-real") == 0);
    repl_free(&r);
    /* raw mode off, the scroll region and the modes reset */
    CHECK_INT(cs.raw_on, 0);
    CHECK(strstr(cs.sent.p + cs.sent.n - 80, "\033[r") != 0);
    CHECK(strstr(cs.sent.p + cs.sent.n - 80, "\033[?2004l") != 0);
    cs_close();
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
        "show me S/Startup-Sequence\r", "!also say hi\r", "2",   /* typed ahead during the tool round */
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
    CHECK_INT(cs.next, 10);                 /* every key used */
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

void suite_claude_repl(void)
{
    mk_tree();
    test_reach();
    test_unfinished();
    test_commands();
    test_screen();
    test_wp1();
    stub_reset();
    jw_free(&cn.screen);
    jw_free(&snt.text);
    jw_free(&sb.req);
    rm_tree();
}
