/* request -- see request.h. */
#include <stdlib.h>
#include <string.h>
#include "request.h"
#include "../claude/util.h"

/* ---- the pool ---- */

void llm_pool_init(llm_pool *p, cl_net *const *nets, int n)
{
    int i;
    memset(p, 0, sizeof(*p));
    p->n = n > LLM_POOL ? LLM_POOL : n;
    for (i = 0; i < p->n; i++)
        p->c[i].net = nets[i];
}

static llm_conn *pool_find(llm_pool *p, const char *host, int port, int tls)
{
    int i;
    for (i = 0; i < p->n; i++) {
        llm_conn *c = &p->c[i];
        if (c->open && c->port == port && c->tls == tls && cl_strieq(c->host, host))
            return c;
    }
    return 0;
}

llm_conn *llm_pool_get(llm_pool *p, const char *host, int port, int tls, int *reused, int *rc)
{
    llm_conn *c = pool_find(p, host, port, tls);
    int i;
    *rc = 0;
    *reused = c != 0;
    if (!c) {
        /* a closed slot, else the least recently used one */
        for (i = 0; i < p->n && !c; i++)
            if (!p->c[i].open)
                c = &p->c[i];
        for (i = 0; i < p->n && (!c || c->open); i++)
            if (!c || p->c[i].last < c->last)
                c = &p->c[i];
        if (!c) {
            *rc = NET_ERROR;
            return 0;
        }
        llm_pool_drop(c);
        *rc = c->net->open(c->net->u, host, port, tls);
        if (*rc) {
            p->failed = c->net;     /* it says why */
            return 0;
        }
        c->open = 1;
        cl_copy(c->host, host, sizeof(c->host));
        c->port = port;
        c->tls = tls;
        p->opens++;
    }
    c->last = ++p->tick;
    return c;
}

void llm_pool_drop(llm_conn *c)
{
    if (c && c->open)
        c->net->close(c->net->u);
    if (c)
        c->open = 0;
}

void llm_pool_close(llm_pool *p)
{
    int i;
    for (i = 0; i < p->n; i++)
        llm_pool_drop(&p->c[i]);
}

int llm_pool_open(const llm_pool *p)
{
    int i, k = 0;
    for (i = 0; i < p->n; i++)
        k += p->c[i].open;
    return k;
}

/* ---- the LAN rule ---- */

int llm_ipv4(const char *host, unsigned char ip[4])
{
    int i;
    const char *s = host;
    for (i = 0; i < 4; i++) {
        long v = 0;
        int d = 0;
        while (*s >= '0' && *s <= '9' && d < 4) {
            v = v * 10 + (*s++ - '0');
            d++;
        }
        if (!d || v > 255)
            return 0;
        ip[i] = (unsigned char)v;
        if (i < 3) {
            if (*s != '.')
                return 0;
            s++;
        }
    }
    return *s == 0;
}

int llm_lan_ok(const char *host, const unsigned char *ip)
{
    long n = (long)strlen(host);
    if (n > 6 && cl_strieq(host + n - 6, ".local"))
        return 1;
    if (!ip)
        return 0;
    return ip[0] == 127 || ip[0] == 10 || (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) ||
           (ip[0] == 192 && ip[1] == 168);
}

/* ---- one attempt ---- */

static unsigned long now(llm_call *c)
{
    return c->h.ms ? c->h.ms(c->h.u) : 0;
}

static void on_sse(void *u, const char *event, const char *data, long n)
{
    llm_call *c = (llm_call *)u;
    if (c->h.event)
        c->h.event(c->h.u, event, data, n);
    if (c->be && c->dec && !c->bad && c->be->feed(c->dec, event, data, n, c->sink))
        c->bad = 1;
}

static void on_body(void *u, const char *s, long n)
{
    llm_call *c = (llm_call *)u;
    if (c->resp->status == 200 && c->resp->event_stream)
        sse_feed(c->sse, s, n);
    else if (c->errbody->n < 16384)
        jw_raw(c->errbody, s, n);
}

static void drop(llm_call *c)
{
    llm_pool_drop(c->conn);
}

int llm_post(llm_call *c)
{
    int again = 1;
    c->err = "";
    c->got = 0;
top:
    {
        int rc, reused;
        llm_conn *k = pool_find(c->pool, c->host, c->port, c->tls);
        unsigned long t0 = 0;
        if (!k) {
            if (c->h.status)
                c->h.status(c->h.u, LPS_CONNECT);
            t0 = now(c);
        }
        k = llm_pool_get(c->pool, c->host, c->port, c->tls, &reused, &rc);
        if (!k) {
            if (rc == NET_BREAK)
                return LP_BREAK;
            c->err = c->pool->failed ? c->pool->failed->err(c->pool->failed->u) : "";
            return LP_CONNECT;
        }
        if (!reused)
            c->t_open = now(c) - t0;
        c->conn = k;
        c->reused = reused;
    }
    {
        const char *head;
        long hn, s1, s2, idle = 0;
        unsigned long t0;
        cl_net *net = c->conn->net;
        if (!c->h.head || c->h.head(c->h.u, &head, &hn))
            return -1;
        t0 = now(c);
        s1 = net->send(net->u, head, hn);
        s2 = s1 == hn ? net->send(net->u, c->body, c->bn) : s1;
        if (s1 == NET_BREAK || s2 == NET_BREAK) {
            drop(c);
            return LP_BREAK;
        }
        if (s1 != hn || s2 != c->bn) {
            drop(c);
            if (c->reused && again--)
                goto top;           /* the server closed the kept connection */
            c->err = net->err(net->u);
            return LP_SEND;
        }
        http_resp_init(c->resp, on_body, c);
        sse_init(c->sse, on_sse, c);
        jw_init(c->errbody);
        c->bad = 0;
        if (c->h.begin)
            c->h.begin(c->h.u);
        c->got = 0;
        for (;;) {
            long n = net->recv(net->u, c->buf, c->bufcap, 250);
            if (n == NET_TIMEOUT) {
                if (c->h.poll && c->h.poll(c->h.u)) {
                    drop(c);
                    return LP_BREAK;
                }
                if (c->h.status)
                    c->h.status(c->h.u, LPS_WAIT);
                idle += 250;
                if (idle < c->idle_ms)
                    continue;
                drop(c);
                return LP_IDLE;
            }
            if (n == NET_BREAK) {
                drop(c);
                return LP_BREAK;
            }
            if (n < 0 || (n == 0 && http_resp_eof(c->resp))) {
                drop(c);
                if (!c->got && c->reused && again--) {
                    if (c->h.restart)
                        c->h.restart(c->h.u);
                    goto top;
                }
                c->err = n < 0 ? net->err(net->u) : "";
                return n < 0 ? LP_RECV : LP_EOF;
            }
            if (n == 0)
                break;              /* the body ran to the close, complete */
            if (!c->got) {
                c->got = 1;
                c->t_first = now(c) - t0;
            }
            idle = 0;
            if (http_resp_feed(c->resp, c->buf, n)) {
                drop(c);
                return LP_BADHTTP;
            }
            if (c->h.poll && c->h.poll(c->h.u)) {
                drop(c);
                return LP_BREAK;
            }
            if (c->resp->state == HR_DONE)
                break;
        }
    }
    if (c->resp->close)
        drop(c);
    return LP_OK;
}

