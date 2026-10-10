/* retry -- llm_request (request.h): amiga-pi's whole request over
 * llm_post, with pi-ai's retries and failures as stream events. */
#include <stdlib.h>
#include <string.h>
#include "request.h"
#include "../claude/util.h"

/* ---- the whole request (amiga-pi) ---- */

/* the stream as the caller sees it: an attempt's error held back until it
 * is known to be the last; whether any of the answer came */
typedef struct gate {
    const llm_sink *out;
    int content, done, err;
    char et[48], em[256];
} gate;

static void gate_ev(void *u, const llm_event *e)
{
    gate *g = (gate *)u;
    if (e->type == LE_ERROR) {
        long n = e->n < (long)sizeof(g->em) - 1 ? e->n : (long)sizeof(g->em) - 1;
        g->err = 1;
        cl_copy(g->et, e->err_type ? e->err_type : "error", sizeof(g->et));
        if (n > 0 && e->p)
            memcpy(g->em, e->p, (size_t)n);
        else
            n = 0;
        g->em[n] = 0;
        return;
    }
    if (e->type == LE_DONE)
        g->done = 1;
    if (e->type == LE_BLOCK_START || e->type == LE_DELTA || e->type == LE_SIG_DELTA || e->type == LE_BLOCK_END)
        g->content = 1;
    if (g->out && g->out->ev)
        g->out->ev(g->out->u, e);
}

static void final_error(const llm_sink *k, int stop, const char *type, const char *msg)
{
    llm_event e;
    llm_event_init(&e, LE_ERROR);
    e.stop = stop;
    e.err_type = type;
    e.p = msg;
    e.n = (long)strlen(msg);
    if (k && k->ev)
        k->ev(k->u, &e);
}

typedef struct rq {
    llm_req *q;
    llm_hooks inner;
} rq;

static int rq_head(void *u, const char **head, long *hn)
{
    llm_req *q = ((rq *)u)->q;
    *head = q->head;
    *hn = q->hn;
    return q->hn < 0 ? -1 : 0;
}

static int rq_poll(void *u)
{
    rq *r = (rq *)u;
    return r->inner.poll ? r->inner.poll(r->inner.u) : 0;
}

static void rq_status(void *u, int what)
{
    rq *r = (rq *)u;
    if (r->inner.status)
        r->inner.status(r->inner.u, what);
}

static unsigned long rq_ms(void *u)
{
    rq *r = (rq *)u;
    return r->inner.ms ? r->inner.ms(r->inner.u) : 0;
}

/* providers already told that their key stays home on plain http */
static char told[8][32];

static void key_notice(llm_req *q)
{
    int i;
    char m[160];
    const char *prov = q->model->provider ? q->model->provider : "";
    for (i = 0; i < 8 && told[i][0]; i++)
        if (!strcmp(told[i], prov))
            return;
    if (i < 8)
        cl_copy(told[i], prov, sizeof(told[i]));
    cl_copy(m, "key for ", sizeof(m));
    cl_cat(m, prov, sizeof(m));
    cl_cat(m, " not sent: plain http", sizeof(m));
    if (q->h.notice)
        q->h.notice(q->h.u, m);
}

static void http_error(jw *body, int status, char *out, long cap)
{
    jv v, e, x;
    char num[16], msg[200];
    msg[0] = 0;
    if (body->p && json_parse(body->p, body->n, &v) == 0) {
        if (json_get(v, "error", &e)) {
            if (json_type(e) == J_STR)
                json_str(e, msg, sizeof(msg));
            else if (json_get(e, "message", &x))
                json_str(x, msg, sizeof(msg));
        }
    }
    cl_copy(out, "HTTP ", cap);
    cl_ltoa(status, num);
    cl_cat(out, num, cap);
    if (msg[0]) {
        cl_cat(out, ": ", cap);
        cl_cat(out, msg, cap);
    }
}

int llm_request(llm_req *q)
{
    http_url u;
    char path[300], buf[2048], err[300];
    const char *key = q->key;
    jw body, errbody;
    http_resp resp;
    sse ss;
    gate g;
    llm_sink gk;
    rq r;
    int retries = q->retries > 0 ? q->retries : 3, attempt;
    long base = q->base_ms > 0 ? q->base_ms : 2000, cap = q->cap_ms > 0 ? q->cap_ms : 60000;
    q->attempts = 0;
    q->key_sent = 0;
    q->slept_ms = 0;
    q->hn = -1;
    memset(&g, 0, sizeof(g));
    g.out = q->sink;
    gk.u = &g;
    gk.ev = gate_ev;
    if (!q->model->base_url || http_parse_url(q->model->base_url, &u)) {
        final_error(q->sink, LS_ERROR, "config", "the model's base URL is not an http:// or https:// URL");
        return -1;
    }
    cl_copy(path, u.path, sizeof(path));
    if (path[0] && path[strlen(path) - 1] == '/')
        path[strlen(path) - 1] = 0;
    cl_cat(path, q->be->path, sizeof(path));
    if (!u.tls) {
        unsigned char ip[4];
        int have = llm_ipv4(u.host, ip) || (q->h.resolve && q->h.resolve(q->h.u, u.host, ip) == 0);
        if (!llm_lan_ok(u.host, have ? ip : 0)) {
            cl_copy(err, "plain http is only for the LAN (10/8, 172.16/12, 192.168/16, 127/8 or a .local name); ", sizeof(err));
            cl_cat(err, u.host, sizeof(err));
            cl_cat(err, " is not one: use https", sizeof(err));
            final_error(q->sink, LS_ERROR, "lan_rule", err);
            return -1;
        }
        if (key && *key) {
            key_notice(q);
            key = 0;
        }
    }
    jw_init(&body);
    if (q->be->body(q->ctx, &body) || body.oom) {
        jw_free(&body);
        final_error(q->sink, LS_ERROR, "body", "the request could not be built");
        return -1;
    }
    q->hn = q->be->head(u.host, u.port, u.tls, path, key, q->user_agent, body.n, q->head, sizeof(q->head));
    q->key_sent = key && *key && q->hn > 0;
    r.q = q;
    r.inner = q->h;
    for (attempt = 1;; attempt++) {
        llm_call c;
        void *dec = q->be->dec_new();
        int rc, retry = 0;
        long wait = -1;
        memset(&c, 0, sizeof(c));
        c.pool = q->pool;
        c.host = u.host;
        c.port = u.port;
        c.tls = u.tls;
        c.body = body.p;
        c.bn = body.n;
        c.h.u = &r;
        c.h.poll = rq_poll;
        c.h.status = rq_status;
        c.h.ms = rq_ms;
        c.h.head = rq_head;
        c.be = q->be;
        c.dec = dec;
        c.sink = &gk;
        c.idle_ms = q->idle_ms > 0 ? q->idle_ms : 300000;
        c.buf = buf;
        c.bufcap = sizeof(buf);
        c.resp = &resp;
        c.sse = &ss;
        c.errbody = &errbody;
        g.err = 0;
        q->attempts = attempt;
        rc = llm_post(&c);
        err[0] = 0;
        if (rc == LP_BREAK) {
            final_error(q->sink, LS_ABORTED, "aborted", "Stopped.");
        } else if (rc == LP_OK && resp.status == 200 && resp.event_stream) {
            if (c.bad)
                cl_copy(err, "the answer's stream was malformed", sizeof(err));
            else if (g.done && !g.err) {
                sse_free(&ss);
                jw_free(&errbody);
                q->be->dec_free(dec);
                jw_free(&body);
                return 0;
            } else if (g.err) {
                cl_copy(err, g.em[0] ? g.em : g.et, sizeof(err));
                retry = !g.content && (!strcmp(g.et, "overloaded_error") || !strcmp(g.et, "api_error"));
            } else {
                cl_copy(err, "the answer was cut off", sizeof(err));
                retry = !g.content;
            }
        } else if (rc == LP_OK) {
            http_error(&errbody, resp.status, err, sizeof(err));
            retry = resp.status == 429 || resp.status == 408 || (resp.status >= 500 && resp.status <= 599);
            if (resp.retry_after > 0)
                wait = resp.retry_after * 1000L;
        } else {
            static const char *const why[] = { "", "cannot connect", "sending failed", "the connection failed",
                                               "the connection closed early", "the answer was not valid HTTP",
                                               "no answer within the idle limit" };
            cl_copy(err, rc > 0 && rc <= LP_IDLE ? why[rc] : "the request failed", sizeof(err));
            if (c.err && *c.err) {
                cl_cat(err, ": ", sizeof(err));
                cl_cat(err, c.err, sizeof(err));
            }
            retry = rc != LP_BADHTTP && !g.content;
        }
        sse_free(&ss);
        jw_free(&errbody);
        q->be->dec_free(dec);
        if (rc == LP_BREAK)
            break;
        if (!retry || attempt > retries) {
            final_error(q->sink, LS_ERROR, g.err ? g.et : "error", err);
            break;
        }
        if (wait < 0) {
            long k;
            wait = base;
            for (k = 1; k < attempt && wait < cap; k++)
                wait *= 2;
        }
        if (wait > cap)
            wait = cap;
        q->slept_ms += wait;
        if (q->h.sleep && q->h.sleep(q->h.u, wait)) {
            final_error(q->sink, LS_ABORTED, "aborted", "Stopped.");
            break;
        }
    }
    jw_free(&body);
    return -1;
}
