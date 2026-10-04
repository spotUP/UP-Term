/* The Claude client's HTTP layer (claude/http.c) and the host transport
 * (claude/net_posix.c) against a fixture server forked on 127.0.0.1. */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "harness.h"
#include "../claude/http.h"
#include "../claude/net_posix.h"

typedef struct sink {
    char b[4096];
    long n;
} sink;

static void to_sink(void *u, const char *s, long n)
{
    sink *k = (sink *)u;
    if (k->n + n < (long)sizeof(k->b)) {
        memcpy(k->b + k->n, s, (size_t)n);
        k->n += n;
    }
    k->b[k->n] = 0;
}

static void test_url(void)
{
    http_url u;
    CHECK_INT(http_parse_url("https://api.anthropic.com/v1/messages", &u), 0);
    CHECK_INT(u.tls, 1);
    CHECK_INT(u.port, 443);
    CHECK_STR(u.host, "api.anthropic.com");
    CHECK_STR(u.path, "/v1/messages");
    CHECK_INT(http_parse_url("http://192.168.1.5:8080/v1/messages", &u), 0);
    CHECK_INT(u.tls, 0);
    CHECK_INT(u.port, 8080);
    CHECK_STR(u.host, "192.168.1.5");
    CHECK_INT(http_parse_url("HTTP://host", &u), 0);
    CHECK_STR(u.path, "/");
    CHECK_INT(http_parse_url("ftp://host/x", &u), -1);
    CHECK_INT(http_parse_url("http://:80/x", &u), -1);
    CHECK_INT(http_parse_url("http://h:0/x", &u), -1);
    CHECK_INT(http_parse_url("http://h:99999/x", &u), -1);
    CHECK_INT(http_parse_url("http://h:8a/x", &u), -1);
}

static const char golden_head[] =
    "POST /v1/messages HTTP/1.1\r\n"
    "Host: api.anthropic.com\r\n"
    "User-Agent: UP-Term-Claude/1.0 (AmigaOS)\r\n"
    "Accept: text/event-stream\r\n"
    "Content-Type: application/json\r\n"
    "anthropic-version: 2023-06-01\r\n"
    "anthropic-beta: server-side-fallback-2026-07-01\r\n"
    "x-api-key: test-key-not-real\r\n"
    "Content-Length: 1234\r\n"
    "Connection: keep-alive\r\n"
    "\r\n";

static void test_request(void)
{
    http_req r;
    char out[1024], red[1024];
    long n, m;
    memset(&r, 0, sizeof(r));
    r.host = "api.anthropic.com";
    r.port = 443;
    r.tls = 1;
    r.path = "/v1/messages";
    r.key = "test-key-not-real";
    r.beta = "server-side-fallback-2026-07-01";
    r.body_len = 1234;
    n = http_request_head(&r, out, sizeof(out));
    CHECK_INT(n, (long)strlen(golden_head));
    out[n > 0 ? n : 0] = 0;
    CHECK_STR(out, golden_head);
    /* too small a buffer is refused, not cut */
    CHECK_INT(http_request_head(&r, out, 40), -1);

    /* the debug log's copy: the key gone, the rest byte-exact */
    m = http_redact(out, n, red, sizeof(red) - 1);
    CHECK(m > 0);
    red[m > 0 ? m : 0] = 0;
    CHECK(strstr(red, "test-key-not-real") == 0);
    CHECK(strstr(red, "x-api-key: [redacted]\r\n") != 0);
    CHECK(strstr(red, "Content-Length: 1234\r\nConnection: keep-alive\r\n\r\n") != 0);
    CHECK(strstr(red, "anthropic-beta: server-side-fallback-2026-07-01\r\n") != 0);
    /* a differently-cased header name is redacted too */
    m = http_redact("X-API-KEY: abc\r\n", 16, red, sizeof(red) - 1);
    red[m > 0 ? m : 0] = 0;
    CHECK_STR(red, "x-api-key: [redacted]\r\n");

    /* no key, no beta: neither header; a port of its own in Host */
    r.key = 0;
    r.beta = "";
    r.tls = 0;
    r.port = 8080;
    r.host = "10.0.2.2";
    n = http_request_head(&r, out, sizeof(out));
    out[n > 0 ? n : 0] = 0;
    CHECK(strstr(out, "Host: 10.0.2.2:8080\r\n") != 0);
    CHECK(strstr(out, "x-api-key") == 0);
    CHECK(strstr(out, "anthropic-beta") == 0);
}

static const char chunked_resp[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/event-stream; charset=utf-8\r\n"
    "Transfer-Encoding: chunked\r\n"
    "request-id: req_011test\r\n"
    "\r\n"
    "1a\r\nevent: ping\ndata: {\"a\":1}\n\r\n"
    "5;ext=1\r\n\nabcd\r\n"
    "0\r\n"
    "\r\n";
static const char chunked_body[] = "event: ping\ndata: {\"a\":1}\n\nabcd";

static void feed_split(const char *resp, long n, long at, sink *k, http_resp *r)
{
    k->n = 0;
    k->b[0] = 0;
    http_resp_init(r, to_sink, k);
    if (at < 0) {
        long i;
        for (i = 0; i < n; i++)
            http_resp_feed(r, resp + i, 1);
    } else {
        http_resp_feed(r, resp, at);
        http_resp_feed(r, resp + at, n - at);
    }
}

static void test_response(void)
{
    http_resp r;
    sink k;
    long n = (long)strlen(chunked_resp), at;
    int bad = 0;
    /* every split point, and byte by byte */
    for (at = -1; at <= n; at++) {
        feed_split(chunked_resp, n, at, &k, &r);
        if (r.state != HR_DONE || strcmp(k.b, chunked_body) || r.status != 200 ||
            !r.event_stream || strcmp(r.request_id, "req_011test"))
            bad++;
    }
    CHECK_INT(bad, 0);

    /* an error answer with a length and retry-after */
    {
        const char e[] = "HTTP/1.1 529 Overloaded\r\nretry-after: 7\r\nContent-Length: 5\r\n"
                         "\r\nhello-and-more";
        feed_split(e, (long)strlen(e), -1, &k, &r);
        CHECK_INT(r.status, 529);
        CHECK_INT(r.retry_after, 7);
        CHECK_INT(r.state, HR_DONE);
        CHECK_STR(k.b, "hello");
    }
    /* no length: the body runs to the close */
    {
        const char e[] = "HTTP/1.0 401 Unauthorized\r\n\r\nbody";
        feed_split(e, (long)strlen(e), 10, &k, &r);
        CHECK_INT(r.state, HR_BODY);
        CHECK_INT(http_resp_eof(&r), 0);
        CHECK_INT(r.state, HR_DONE);
        CHECK_STR(k.b, "body");
    }
    /* an interim 100 Continue before the answer */
    {
        const char e[] = "HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
        feed_split(e, (long)strlen(e), -1, &k, &r);
        CHECK_INT(r.status, 200);
        CHECK_STR(k.b, "ok");
    }
    /* cut off inside a chunk: eof is an error */
    feed_split(chunked_resp, n - 12, -1, &k, &r);
    CHECK_INT(http_resp_eof(&r), -1);
    /* malformed */
    {
        const char e[] = "SMTP 220 hello\r\n\r\n";
        http_resp_init(&r, to_sink, &k);
        CHECK_INT(http_resp_feed(&r, e, (long)strlen(e)), -1);
    }
    {
        const char e[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n";
        http_resp_init(&r, to_sink, &k);
        CHECK_INT(http_resp_feed(&r, e, (long)strlen(e)), -1);
    }
}

/* ---- the fixture server: one connection, one canned answer ---- */

static int fixture_listen(int *port)
{
    struct sockaddr_in a;
    socklen_t al = sizeof(a);
    int fd = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    if (fd < 0)
        return -1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(0x7f000001UL);
    a.sin_port = 0;
    if (bind(fd, (struct sockaddr *)&a, sizeof(a)) || listen(fd, 1) ||
        getsockname(fd, (struct sockaddr *)&a, &al)) {
        close(fd);
        return -1;
    }
    *port = ntohs(a.sin_port);
    return fd;
}

/* the child: read the request head and the body its length names, write
 * the answer back in pieces, then the request it got (for the parent) to
 * a pipe */
static void fixture_serve(int lfd, int report, const char *answer)
{
    char req[4096];
    long n = 0, need = -1;
    int c = accept(lfd, 0, 0);
    if (c < 0)
        _exit(1);
    while (n < (long)sizeof(req) - 1) {
        long r = (long)read(c, req + n, sizeof(req) - 1 - (size_t)n);
        char *e;
        if (r <= 0)
            break;
        n += r;
        req[n] = 0;
        e = strstr(req, "\r\n\r\n");
        if (e && need < 0) {
            char *cl = strstr(req, "Content-Length: ");
            need = (long)(e + 4 - req) + (cl ? atol(cl + 16) : 0);
        }
        if (need >= 0 && n >= need)
            break;
    }
    {
        long al = (long)strlen(answer), i;
        for (i = 0; i < al; i += 7)
            if (write(c, answer + i, (size_t)(al - i < 7 ? al - i : 7)) < 0)
                break;
    }
    if (write(report, req, (size_t)n) < 0)
        _exit(1);
    close(c);
    _exit(0);
}

static void test_fixture(void)
{
    int port, lfd, pfd[2];
    pid_t pid;
    net_posix np;
    cl_net net;
    http_req rq;
    http_resp r;
    sink k;
    char head[1024], buf[256], got[4096];
    const char body[] = "{\"model\":\"claude-opus-5-5\"}";
    long hn, gn = 0;
    lfd = fixture_listen(&port);
    CHECK(lfd >= 0);
    if (lfd < 0 || pipe(pfd))
        return;
    pid = fork();
    if (pid == 0) {
        close(pfd[0]);
        fixture_serve(lfd, pfd[1], chunked_resp);
    }
    close(lfd);
    close(pfd[1]);
    net_posix_init(&np, &net);
    CHECK_INT(net.open(net.u, "127.0.0.1", port, 1), NET_ERROR);   /* no TLS on the host */
    CHECK_INT(net.open(net.u, "127.0.0.1", port, 0), 0);
    memset(&rq, 0, sizeof(rq));
    rq.host = "127.0.0.1";
    rq.port = port;
    rq.path = "/v1/messages";
    rq.key = "fixture-key";
    rq.body_len = (long)strlen(body);
    hn = http_request_head(&rq, head, sizeof(head));
    CHECK_INT(net.send(net.u, head, hn), hn);
    CHECK_INT(net.send(net.u, body, rq.body_len), rq.body_len);
    k.n = 0;
    http_resp_init(&r, to_sink, &k);
    while (r.state != HR_DONE && r.state != HR_BAD) {
        long m = net.recv(net.u, buf, sizeof(buf), 2000);
        if (m <= 0) {
            if (m == 0)
                http_resp_eof(&r);
            break;
        }
        http_resp_feed(&r, buf, m);
    }
    net.close(net.u);
    CHECK_INT(r.state, HR_DONE);
    CHECK_STR(k.b, chunked_body);
    for (;;) {
        long m = (long)read(pfd[0], got + gn, sizeof(got) - 1 - (size_t)gn);
        if (m <= 0)
            break;
        gn += m;
    }
    got[gn] = 0;
    close(pfd[0]);
    waitpid(pid, 0, 0);
    /* the server got the head and the body intact */
    CHECK(strstr(got, "POST /v1/messages HTTP/1.1\r\n") == got);
    CHECK(strstr(got, "\r\n\r\n{\"model\":\"claude-opus-5-5\"}") != 0);
    /* a closed port is an error with a reason, not a hang */
    CHECK_INT(net.open(net.u, "127.0.0.1", 1, 0), NET_ERROR);
    CHECK(net.err(net.u)[0] != 0);
}

void suite_claude_http(void)
{
    test_url();
    test_request();
    test_response();
    test_fixture();
}
