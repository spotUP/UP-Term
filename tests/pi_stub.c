/* see pi_stub.h */
#include <stdlib.h>
#include <string.h>
#include "pi_stub.h"
#include "claude_load.h"

pstub ps;

void ps_reset(void)
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

/* a body of the content type as a chunked 200 answer in pieces of 100 bytes (cut: no terminating chunk, the connection closes) */
void ps_body_text(const char *sse, long n, const char *ctype, int cut)
{
    long i;
    jw w;
    static const char hex[] = "0123456789abcdef";
    jw_init(&w);
    jw_rawz(&w, "HTTP/1.1 200 OK\r\nContent-Type: ");
    jw_rawz(&w, ctype);
    jw_rawz(&w, "\r\nTransfer-Encoding: chunked\r\n\r\n");
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
    ps.resp[ps.nresp] = w.p;
    ps.rlen[ps.nresp++] = cut ? -w.n : w.n;     /* negative: close after it */
}

/* SSE text as a chunked 200 answer (cut: no terminating chunk, the connection closes) */
void ps_sse_text(const char *sse, long n, int cut)
{
    ps_body_text(sse, n, "text/event-stream", cut);
}

void ps_stream(const char *name, const char *ctype)
{
    long n;
    char *b = claude_load(name, &n);
    if (!b)
        return;
    ps_body_text(b, n, ctype, 0);
    free(b);
}

/* a whole HTTP answer from memory, as it is */
void ps_http_text(const char *http, long n)
{
    char *b = (char *)malloc((size_t)n + 1);
    if (!b)
        return;
    memcpy(b, http, (size_t)n);
    b[n] = 0;
    ps.resp[ps.nresp] = b;
    ps.rlen[ps.nresp++] = n;
}

/* a recording, the same way */
void ps_sse(const char *name, int cut)
{
    long n;
    char *sse = claude_load(name, &n);
    if (!sse)
        return;
    ps_sse_text(sse, n, cut);
    free(sse);
}

void ps_raw(const char *name)
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

int ps_poll(void *u)
{
    (void)u;
    return ps.brk;
}

unsigned long ps_ms(void *u)
{
    (void)u;
    return ps.clock;
}

int ps_sleep(void *u, long ms)
{
    (void)u;
    ps.sleeps++;
    ps.slept += ms;
    return 0;
}

void ps_notice(void *u, const char *t)
{
    (void)u;
    ps.notices++;
    strncpy(ps.notice, t, sizeof(ps.notice) - 1);
}

int ps_resolve(void *u, const char *host, unsigned char ip[4])
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

void ps_log(void *u, const char *s, long n)
{
    (void)u;
    jw_raw(&ps.log, s, n);
}

cl_net ps_nets[LLM_POOL];
static cl_net *netp[LLM_POOL];
llm_pool ps_pool;

void ps_pool_up(int n)
{
    int i;
    for (i = 0; i < LLM_POOL; i++) {
        memset(&ps_nets[i], 0, sizeof(ps_nets[i]));
        ps_nets[i].open = s_open;
        ps_nets[i].send = s_send;
        ps_nets[i].recv = s_recv;
        ps_nets[i].close = s_close;
        ps_nets[i].err = s_err;
        netp[i] = &ps_nets[i];
    }
    llm_pool_init(&ps_pool, netp, n);
}

llm_model ps_model_openai(const char *base)
{
    llm_model m;
    llm_model_init(&m);
    m.id = "qwen2.5-coder:7b";
    m.api = "openai-completions";
    m.provider = "ollama";
    m.base_url = base;
    m.max_tokens = 4096;
    return m;
}

llm_model ps_model_anthropic(void)
{
    llm_model m;
    llm_model_init(&m);
    m.id = "claude-opus-5-5";
    m.api = "anthropic-messages";
    m.provider = "anthropic";
    m.base_url = "https://api.anthropic.com";
    m.max_tokens = 8192;
    return m;
}

