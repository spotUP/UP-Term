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
    char et[48], em[LLM_ERR_MAX];
    char raw[48];               /* the held error's stop_raw (the provider's own reason), "" none */
} gate;

static void gate_ev(void *u, const llm_event *e)
{
    gate *g = (gate *)u;
    if (e->type == LE_ERROR) {
        long n = e->n < (long)sizeof(g->em) - 1 ? e->n : (long)sizeof(g->em) - 1;
        g->err = 1;
        cl_copy(g->et, e->err_type ? e->err_type : "error", sizeof(g->et));
        cl_copy(g->raw, e->stop_raw ? e->stop_raw : "", sizeof(g->raw));
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

static void final_error_raw(const llm_sink *k, int stop, const char *type, const char *msg, const char *raw)
{
    llm_event e;
    llm_event_init(&e, LE_ERROR);
    e.stop = stop;
    e.stop_raw = raw && *raw ? raw : 0;
    e.err_type = type;
    e.p = msg;
    e.n = (long)strlen(msg);
    if (k && k->ev)
        k->ev(k->u, &e);
}

static void final_error(const llm_sink *k, int stop, const char *type, const char *msg)
{
    final_error_raw(k, stop, type, msg, 0);
}

typedef struct rq {
    llm_req *q;
    llm_hooks inner;
    char errtype[100];          /* the answer's x-amzn-errortype header (AWS APIs), "" none */
    char ctype[100];            /* its Content-Type header, "" none */
    http_resp *resp;            /* the attempt's answer */
    jw hdrs;                    /* its headers as name NUL value NUL ..., for the tap */
    int told;                   /* the tap has been told of this answer */
} rq;

/* tap->response, once per answer: a 2xx status, the headers as one JSON object (lower-case names; a repeated one joined by ", ") */
static void tap_response(rq *r)
{
    const llm_tap *t = r->q->tap;
    jw o;
    const char *p = r->hdrs.p;
    long i = 0, n = r->hdrs.n;
    if (r->told || !t || !t->response || !r->resp || r->resp->status < 200 || r->resp->status > 299)
        return;
    r->told = 1;
    jw_init(&o);
    jw_rawz(&o, "{");
    while (i < n) {
        const char *name = p + i, *val = name + strlen(name) + 1, *q;
        long k, first = 1, j;
        int dup = 0;
        /* a name seen before is part of an earlier member */
        for (j = 0; j < i;) {
            const char *pn = p + j;
            if (!strcmp(pn, name)) {
                dup = 1;
                break;
            }
            j += (long)strlen(pn) + 1;
            j += (long)strlen(p + j) + 1;
        }
        if (!dup) {
            if (o.n > 1)
                jw_rawz(&o, ",");
            jw_strz(&o, name);
            jw_rawz(&o, ":");
            jw_rawz(&o, "\"");
            for (k = i; k < n;) {
                const char *pn = p + k, *pv = pn + strlen(pn) + 1;
                if (!strcmp(pn, name)) {
                    jw v;
                    if (!first)
                        jw_rawz(&o, ", ");
                    first = 0;
                    jw_init(&v);
                    jw_strz(&v, pv);
                    if (v.n >= 2)
                        jw_raw(&o, v.p + 1, v.n - 2);       /* the escaped text without its quotes */
                    jw_free(&v);
                }
                k += (long)strlen(pn) + 1 + (long)strlen(pv) + 1;
            }
            jw_rawz(&o, "\"");
        }
        q = val + strlen(val) + 1;
        i = (long)(q - p);
    }
    jw_rawz(&o, "}");
    if (!o.oom)
        t->response(t->u, r->q->model, r->resp->status, o.p, o.n);
    jw_free(&o);
}

static int rq_head(void *u, const char **head, long *hn)
{
    llm_req *q = ((rq *)u)->q;
    *head = q->head;
    *hn = q->hn;
    return q->hn < 0 ? -1 : 0;
}

static void rq_event(void *u, const char *event, const char *data, long n)
{
    rq *r = (rq *)u;
    const llm_tap *t = r->q->tap;
    (void)event;
    tap_response(r);                    /* pi: onResponse comes before the first event of the stream */
    if (t && t->stream && n > 0 && data[0] == '{')
        t->stream(t->u, r->q->model, data, n);
    if (r->inner.log) {
        r->inner.log(r->inner.u, data, n);
        r->inner.log(r->inner.u, "\n", 1);
    }
}

static int rq_poll(void *u)
{
    rq *r = (rq *)u;
    return r->inner.poll ? r->inner.poll(r->inner.u) : 0;
}

static void rq_header(void *u, const char *name, const char *value)
{
    rq *r = (rq *)u;
    if (r->q->tap && r->q->tap->response) {
        int i;
        char lower[100];
        for (i = 0; name[i] && i < 99; i++)
            lower[i] = name[i] >= 'A' && name[i] <= 'Z' ? (char)(name[i] + 32) : name[i];
        lower[i] = 0;
        jw_raw(&r->hdrs, lower, (long)strlen(lower) + 1);
        jw_raw(&r->hdrs, value, (long)strlen(value) + 1);
    }
    if (!strcmp(name, "content-type"))
        cl_copy(r->ctype, value, sizeof(r->ctype));
    if (!strcmp(name, "x-amzn-errortype"))
        cl_copy(r->errtype, value, sizeof(r->errtype));
    if (r->inner.header)
        r->inner.header(r->inner.u, name, value);
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

static void http_error(const llm_req *q, const rq *r, const http_resp *resp, jw *body, char *out, long cap)
{
    jv v, e, x;
    char num[16], msg[200];
    if (q->be->http_err) {
        llm_http_fail f;
        f.status = resp->status;
        f.reason = resp->reason;
        f.ctype = r->ctype;
        f.errtype = r->errtype;
        f.body = body->p ? body->p : "";
        f.bn = body->n;
        f.model = q->model;
        q->be->http_err(&f, out, cap);
        return;
    }
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
    cl_ltoa(resp->status, num);
    cl_cat(out, num, cap);
    if (msg[0]) {
        cl_cat(out, ": ", cap);
        cl_cat(out, msg, cap);
    }
}

/* Copilot's per-request headers (api/github-copilot-headers.ts): X-Initiator by the last message's role,
 * Openai-Intent, Copilot-Vision-Request when a user or tool-result message carries an image */
static void copilot_headers(const llm_ctx *c, char *out, long cap)
{
    int i, images = 0, agent = 0;
    for (i = 0; i < c->nmsg; i++) {
        jv m, r, ct, b, t;
        jit it;
        if (json_parse(c->msgs[i], c->msg_n[i], &m) || !json_get(m, "role", &r))
            continue;
        if (i == c->nmsg - 1)
            agent = !json_streq(r, "user");
        if ((json_streq(r, "user") || json_streq(r, "toolResult")) && json_get(m, "content", &ct) &&
            json_type(ct) == J_ARR) {
            json_iter(ct, &it);
            while (json_next(&it, 0, &b))
                if (json_type(b) == J_OBJ && json_get(b, "type", &t) && json_streq(t, "image"))
                    images = 1;
        }
    }
    cl_copy(out, "X-Initiator: ", cap);
    cl_cat(out, agent ? "agent" : "user", cap);
    cl_cat(out, "\r\nOpenai-Intent: conversation-edits\r\n", cap);
    if (images)
        cl_cat(out, "Copilot-Vision-Request: true\r\n", cap);
}

static int ieq_n(const char *a, const char *b, long n)
{
    long i;
    for (i = 0; i < n; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z')
            x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z')
            y = (char)(y + 32);
        if (x != y)
            return 0;
    }
    return 1;
}

/* the header lines of xb (each "Name: value\r\n") replace headers of the same name in head[0..n): pi's defaults give way
 * to the model's, the options' and the provider's headers. The new length */
static long strip_dups(char *head, long n, const char *xb)
{
    const char *x = xb;
    head[n] = 0;
    while (*x) {
        const char *colon = strchr(x, ':'), *eol = strstr(x, "\r\n");
        long nl;
        char *ln;
        if (!colon || !eol || colon > eol)
            break;
        nl = (long)(colon - x);
        ln = strstr(head, "\r\n");
        while (ln && ln[2] && !(ln[2] == '\r' && ln[3] == '\n')) {
            char *line = ln + 2, *le = strstr(line, "\r\n");
            long ll;
            if (!le)
                break;
            ll = (long)(le - line);
            if (ll > nl && line[nl] == ':' && ieq_n(line, x, nl)) {
                long cut = ll + 2;
                memmove(line, line + cut, (size_t)(n - (line - head) - cut) + 1);
                n -= cut;
                continue;       /* the next line is at the same place now */
            }
            ln = le;
        }
        x = eol + 2;
    }
    return n;
}

/* header lines a + b (each part may be empty) before the blank line that ends the head; eol: a CRLF ends the lines
 * (the extra headers of xb carry theirs). 0, -1 when they do not fit */
static int add_line(llm_req *q, const char *a, long an, const char *b, long bn, int eol)
{
    long add = an + bn + (eol ? 2 : 0);
    if (q->hn < 4 || q->hn + add >= (long)sizeof(q->head))
        return -1;
    memcpy(q->head + q->hn - 2, a, (size_t)an);
    memcpy(q->head + q->hn - 2 + an, b, (size_t)bn);
    if (eol)
        memcpy(q->head + q->hn - 2 + an + bn, "\r\n", 2);
    memcpy(q->head + q->hn - 2 + add, "\r\n", 2);
    q->hn += add;
    q->head[q->hn] = 0;
    return 0;
}

/* the head of the request for key: the backend's, then the extra header lines (models.json's, Copilot's, the OAuth
 * credential's Bearer) before the blank line. 0, or -1 with the error in the stream */
static int mk_head(llm_req *q, const http_url *u, const char *path, const char *key, const jw *bodyw)
{
    static char xb[4096];
    char cx[200], err[300];
    const char *hkey;
    int bearer = 0;
    long body_n = bodyw->n;
    int key_given = key && *key;       /* a bearer credential is configured, sent or not */
    xb[0] = 0;
    q->signed_auth = 0;
    if (!u->tls && key && *key) {
        key_notice(q);
        key = 0;
    }
    hkey = key;
    if (q->bearer && key && *key) {
        bearer = 1;
        hkey = 0;
    }
    q->hn = q->be->head(u->host, u->port, u->tls, path, hkey, q->user_agent, body_n, q->head, sizeof(q->head));
    q->key_sent = key && *key && q->hn > 0;
    if (q->be->xhead)
        q->be->xhead(q->ctx, xb, 512);
    if (q->xhead && strlen(q->xhead) + strlen(xb) < 1024)
        strcat(xb, q->xhead);
    if (q->copilot) {
        copilot_headers(q->ctx, cx, sizeof(cx));
        strcat(xb, cx);
    }
    if (q->be->sign) {
        /* an authentication over the finished request (Signature Version 4): its header lines follow */
        long sn = q->be->sign(q->ctx, u->host, u->port, u->tls, path, bodyw->p ? bodyw->p : "", body_n, key_given,
                              xb + strlen(xb), (long)sizeof(xb) - (long)strlen(xb) - 1, err, sizeof(err));
        if (sn < 0) {
            final_error(q->sink, LS_ERROR, "auth", err);
            return -1;
        }
        q->signed_auth = sn > 0;
    }
    if (q->hn >= 4 && xb[0])
        q->hn = strip_dups(q->head, q->hn, xb);
    if (q->hn >= 4 && xb[0] && add_line(q, xb, (long)strlen(xb), "", 0, 0)) {
        final_error(q->sink, LS_ERROR, "head", "the request headers are too long");
        return -1;
    }
    if (bearer && q->hn >= 4) {
        long kl = (long)strlen(key);
        q->hn = strip_dups(q->head, q->hn, "Authorization: x\r\n");
        if (add_line(q, "Authorization: Bearer ", 22, key, kl, 1)) {
            final_error(q->sink, LS_ERROR, "head", "the request headers are too long");
            return -1;
        }
    }
    if (q->h.log && q->hn > 0) {
        /* the only form of the head a log gets: the credential's value blanked */
        static char red[6200], red2[6200];
        long rn = http_redact_header(q->head, q->hn, q->be->secret_header, red, sizeof(red));
        if (rn > 0) {
            long r2 = http_redact_header(red, rn, "Authorization", red2, sizeof(red2));
            if (r2 > 0) {
                memcpy(red, red2, (size_t)r2);
                rn = r2;
            }
        }
        if (rn > 0 && q->signed_auth) {
            long r2 = http_redact_header(red, rn, "x-amz-security-token", red2, sizeof(red2));
            if (r2 > 0) {
                memcpy(red, red2, (size_t)r2);
                rn = r2;
            }
        }
        if (rn > 0)
            q->h.log(q->h.u, red, rn);
    }
    return 0;
}

static int request_run(llm_req *q)
{
    http_url u;
    char path[300], buf[2048], rerr[300];
    static char err[LLM_ERR_MAX];
    jw body, errbody;
    http_resp resp;
    sse ss;
    llm_evs es;
    static gate g;
    llm_sink gk;
    rq r;
    int retries = q->retries < 0 ? 0 : q->retries > 0 ? q->retries : 3, attempt;
    long base = q->base_ms > 0 ? q->base_ms : 2000, cap = q->cap_ms > 0 ? q->cap_ms : 60000;
    q->attempts = 0;
    q->key_sent = 0;
    q->slept_ms = 0;
    q->hn = -1;
    q->status = 0;
    q->reauthed = 0;
    rerr[0] = 0;
    q->signed_auth = 0;
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
    if (q->be->route) {
        char rt[240];
        if (q->be->route(q->ctx, rt, sizeof(rt)) < 0) {
            final_error(q->sink, LS_ERROR, "config", "the request path is too long");
            return -1;
        }
        cl_cat(path, rt, sizeof(path));
    } else
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
    }
    jw_init(&body);
    jw_init(&errbody);
    sse_init(&ss, 0, 0);            /* freed after every attempt, also one that never reached llm_post's sse_init */
    if (q->be->body(q->ctx, &body) || body.oom) {
        jw_free(&body);
        final_error(q->sink, LS_ERROR, "body", "the request could not be built");
        return -1;
    }
    if (q->tap && q->tap->payload) {
        /* pi's onPayload (the extensions' before_provider_request): once, before the head is made; a 401's second attempt sends the same body */
        if (q->tap->payload(q->tap->u, q->model, &body) || body.oom) {
            jw_free(&body);
            final_error(q->sink, LS_ERROR, "body", "the request could not be built");
            return -1;
        }
    }
    if (mk_head(q, &u, path, q->key, &body)) {
        jw_free(&body);
        jw_free(&errbody);
        return -1;
    }
    r.q = q;
    r.inner = q->h;
    jw_init(&r.hdrs);
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
        c.h.header = rq_header;
        c.es = &es;
        c.h.event = q->h.log || (q->tap && (q->tap->stream || q->tap->response)) ? rq_event : 0;
        r.resp = &resp;
        r.told = 0;
        jw_reset(&r.hdrs);
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
        g.raw[0] = 0;
        r.errtype[0] = 0;
        r.ctype[0] = 0;
        memset(&es, 0, sizeof(es));
        q->attempts = attempt;
        rc = llm_post(&c);
        err[0] = 0;
        q->status = rc == LP_OK ? resp.status : 0;
        if (rc == LP_OK)
            tap_response(&r);           /* an answer with no events (or none that came through) is told once all the same */
        if (rc == LP_BREAK) {
            final_error(q->sink, LS_ABORTED, "aborted", "Request aborted");
        } else if (rc == LP_OK && resp.status == 200 && resp.event_stream) {
            if (resp.aws_stream && !c.bad && llm_evs_end(&es))
                c.bad = 1;          /* the answer ended inside a message */
            if (!c.bad && !g.done && !g.err && q->be->fin)
                q->be->fin(dec, &gk);       /* the body ended: an API without a terminal event ends here */
            if (c.bad)
                cl_copy(err, resp.aws_stream && es.err[0] ? es.err : "the answer's stream was malformed", sizeof(err));
            else if (g.done && !g.err) {
                sse_free(&ss);
                llm_evs_free(&es);
                jw_free(&errbody);
                q->be->dec_free(dec);
                jw_free(&body);
                jw_free(&r.hdrs);
                return 0;
            } else if (g.err) {
                cl_copy(err, g.em[0] ? g.em : g.et, sizeof(err));
                retry = !g.content && (!strcmp(g.et, "overloaded_error") || !strcmp(g.et, "api_error"));
            } else {
                cl_copy(err, q->be->cut ? q->be->cut : "the answer was cut off", sizeof(err));
                retry = !g.content;
            }
        } else if (rc == LP_OK) {
            http_error(q, &r, &resp, &errbody, err, sizeof(err));
            retry = resp.status == 429 || resp.status == 408 || (resp.status >= 500 && resp.status <= 599);
            if (resp.retry_after > 0)
                wait = resp.retry_after * 1000L;
        } else {
            /* no request reached a server (connect or send failed): pi's SDKs' APIConnectionError (core/error.js),
             * a text pi's agent retry patterns match; the transport's reason is not part of it */
            static const char *const why[] = { "", "Connection error.", "Connection error.", "the connection failed",
                                               "the connection closed early", "the answer was not valid HTTP",
                                               "no answer within the idle limit" };
            cl_copy(err, rc > 0 && rc <= LP_IDLE ? why[rc] : "the request failed", sizeof(err));
            if (rc != LP_CONNECT && rc != LP_SEND && c.err && *c.err) {
                cl_cat(err, ": ", sizeof(err));
                cl_cat(err, c.err, sizeof(err));
            } else if (c.err && *c.err && q->h.log) {
                /* the reason pi's text leaves out (no route, no AmiSSL, the certificate) goes to the debug log */
                q->h.log(q->h.u, "llm: ", 5);
                q->h.log(q->h.u, err, (long)strlen(err));
                q->h.log(q->h.u, ": ", 2);
                q->h.log(q->h.u, c.err, (long)strlen(c.err));
                q->h.log(q->h.u, "\n", 1);
            }
            retry = rc != LP_BADHTTP && !g.content;
        }
        sse_free(&ss);
        llm_evs_free(&es);
        jw_free(&errbody);
        q->be->dec_free(dec);
        if (rc == LP_BREAK)
            break;
        if (rc == LP_OK && resp.status == 401 && q->reauth && !q->reauthed) {
            /* the credential was refused: renewed once, the request goes again at once with the new one */
            q->reauthed = 1;
            if (q->reauth(q->reauth_u, q, rerr, sizeof(rerr)) == 0) {
                if (mk_head(q, &u, path, q->key, &body))
                    break;
                attempt--;
                continue;
            }
            if (rerr[0])
                cl_copy(err, rerr, sizeof(err));
        }
        if (!retry || attempt > retries) {
            final_error_raw(q->sink, LS_ERROR, g.err ? g.et : "error", err, g.err ? g.raw : 0);
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
            final_error(q->sink, LS_ABORTED, "aborted", "Request aborted");
            break;
        }
    }
    jw_free(&body);
    jw_free(&r.hdrs);
    return -1;
}

int llm_request(llm_req *q)
{
    const char *xh0 = q->xhead;
    jw lines;
    int rc;
    if (q->tap && q->tap->headers) {
        /* pi's transformHeaders (before_provider_headers): the extra header lines as the extension sees them, before the body is built */
        jw_init(&lines);
        if (xh0)
            jw_rawz(&lines, xh0);
        if (q->tap->headers(q->tap->u, q->model, &lines) == 0 && !lines.oom) {
            jw_raw(&lines, "", 1);          /* a jw is not terminated; the head is made from a C string */
            q->xhead = lines.n > 1 ? lines.p : 0;
        }
    }
    rc = request_run(q);
    if (q->tap && q->tap->headers) {
        q->xhead = xh0;
        jw_free(&lines);
    }
    return rc;
}
