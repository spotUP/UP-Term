/* webfetch -- the WebFetch tool (ledger A4 WP2), as Claude Code's: an
 * HTTP GET over the program's transport (net.h: bsdsocket and AmiSSL on
 * the Amiga, so https works where AmiSSL is installed), redirects on the
 * same host followed, one to another host reported for Claude to fetch
 * itself, the page turned into Markdown (html.h), and the user's prompt
 * answered from it by a small model call (Claude Haiku 4.5) whose answer
 * is the tool's result. */
#include <stdlib.h>
#include <string.h>
#include "tools_int.h"
#include "http.h"
#include "html.h"
#include "util.h"

#define FETCH_MAX    (512L * 1024)   /* the raw page */
#define FETCH_MD_MAX 100000L         /* what the small model reads */
#define FETCH_IDLE   30000L          /* ms without a byte: given up */
#define FETCH_HOPS   5
#define FETCH_MODEL  "claude-haiku-4-5"

typedef struct page {
    http_resp resp;
    jw head;                    /* the response head, raw */
    int head_done;
    jw body;
    int cut;                    /* the body was longer than FETCH_MAX */
} page;

static void on_body(void *u, const char *s, long n)
{
    page *p = (page *)u;
    if (p->body.n + n > FETCH_MAX) {
        n = FETCH_MAX - p->body.n;
        p->cut = 1;
    }
    if (n > 0)
        jw_raw(&p->body, s, n);
}

/* a header's value from the raw head ("" none) */
static void header(const page *p, const char *name, char *out, long cap)
{
    long i, nl = (long)strlen(name);
    out[0] = 0;
    for (i = 0; p->head.p && i + nl + 2 < p->head.n; i++) {
        if (p->head.p[i] == '\n' && cl_strnieq(p->head.p + i + 1, name, nl) && p->head.p[i + 1 + nl] == ':') {
            long a = i + 2 + nl, e;
            while (a < p->head.n && (p->head.p[a] == ' ' || p->head.p[a] == '\t'))
                a++;
            e = a;
            while (e < p->head.n && p->head.p[e] != '\r' && p->head.p[e] != '\n')
                e++;
            if (e - a >= cap)
                e = a + cap - 1;
            memcpy(out, p->head.p + a, (size_t)(e - a));
            out[e - a] = 0;
            return;
        }
    }
}

/* one GET: 0 with the page, -1 with the reason in err, -2 stopped (Ctrl+C) */
static int get(cl_net *net, const http_url *u, page *p, char *err, long cap)
{
    char head[700], buf[2048], num[16];
    long hn, idle = 0;
    int rc;
    jw_init(&p->head);
    jw_init(&p->body);
    p->head_done = 0;
    p->cut = 0;
    http_resp_init(&p->resp, on_body, p);
    cl_copy(head, "GET ", sizeof(head));
    cl_cat(head, u->path[0] ? u->path : "/", sizeof(head));
    cl_cat(head, " HTTP/1.1\r\nHost: ", sizeof(head));
    cl_cat(head, u->host, sizeof(head));
    if (u->port != (u->tls ? 443 : 80)) {
        cl_ltoa(u->port, num);
        cl_cat(head, ":", sizeof(head));
        cl_cat(head, num, sizeof(head));
    }
    cl_cat(head, "\r\nUser-Agent: C-Claude/1.0 (AmigaOS; UP-Term)\r\nAccept: text/html, text/markdown, "
                 "text/plain, */*;q=0.8\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n",
           sizeof(head));
    hn = (long)strlen(head);
    rc = net->open(net->u, u->host, u->port, u->tls);
    if (rc == NET_BREAK)
        return -2;
    if (rc) {
        cl_copy(err, "cannot connect: ", cap);
        cl_cat(err, net->err(net->u), cap);
        return -1;
    }
    rc = (int)net->send(net->u, head, hn);
    if (rc != hn) {
        net->close(net->u);
        if (rc == NET_BREAK)
            return -2;
        cl_copy(err, "sending the request failed: ", cap);
        cl_cat(err, net->err(net->u), cap);
        return -1;
    }
    for (;;) {
        long n = net->recv(net->u, buf, sizeof(buf), 250);
        if (n == NET_TIMEOUT) {
            idle += 250;
            if (idle < FETCH_IDLE)
                continue;
            net->close(net->u);
            cl_copy(err, "no answer for 30 seconds", cap);
            return -1;
        }
        if (n == NET_BREAK) {
            net->close(net->u);
            return -2;
        }
        if (n < 0) {
            net->close(net->u);
            cl_copy(err, "the connection failed: ", cap);
            cl_cat(err, net->err(net->u), cap);
            return -1;
        }
        if (n == 0) {
            net->close(net->u);
            if (http_resp_eof(&p->resp)) {
                cl_copy(err, "the connection closed early", cap);
                return -1;
            }
            return 0;
        }
        idle = 0;
        if (!p->head_done) {
            /* the head, kept raw for Location and Content-Type */
            long k;
            for (k = 0; k < n && !p->head_done && p->head.n < 16384; k++) {
                jw_raw(&p->head, buf + k, 1);
                if (p->head.n >= 4 && !memcmp(p->head.p + p->head.n - 4, "\r\n\r\n", 4))
                    p->head_done = 1;
            }
        }
        if (http_resp_feed(&p->resp, buf, n)) {
            net->close(net->u);
            cl_copy(err, "the server's answer is not valid HTTP", cap);
            return -1;
        }
        if (p->resp.state == HR_DONE || p->cut) {
            net->close(net->u);
            return 0;
        }
    }
}

static void page_free(page *p)
{
    jw_free(&p->head);
    jw_free(&p->body);
}

/* Location against the URL it came from, into out */
static void resolve_url(const http_url *base, const char *loc, char *out, long cap)
{
    char num[16];
    if (!cl_strnieq(loc, "http://", 7) && !cl_strnieq(loc, "https://", 8)) {
        cl_copy(out, base->tls ? "https://" : "http://", cap);
        cl_cat(out, base->host, cap);
        if (base->port != (base->tls ? 443 : 80)) {
            cl_ltoa(base->port, num);
            cl_cat(out, ":", cap);
            cl_cat(out, num, cap);
        }
        if (loc[0] == '/')
            cl_cat(out, loc, cap);
        else {
            /* relative to the directory of the path */
            char dir[256];
            char *slash;
            cl_copy(dir, base->path[0] ? base->path : "/", sizeof(dir));
            slash = strrchr(dir, '/');
            if (slash)
                slash[1] = 0;
            cl_cat(out, dir, cap);
            cl_cat(out, loc, cap);
        }
    } else
        cl_copy(out, loc, cap);
}

/* the same site: the host equal but for a leading "www." */
static int same_host(const char *a, const char *b)
{
    if (cl_strnieq(a, "www.", 4))
        a += 4;
    if (cl_strnieq(b, "www.", 4))
        b += 4;
    return cl_strieq(a, b);
}

static const char fetch_prompt[] =
    "\n---\n\n";
static const char fetch_rules[] =
    "\n\nProvide a concise response based only on the content above. In your response:\n"
    " - Enforce a strict 125-character maximum for quotes from any source document. Open Source Software "
    "is ok as long as we respect the license.\n"
    " - Use quotation marks for exact language from articles; any language outside of the quotation "
    "should never be word-for-word the same.\n";
static const char fetch_rules2[] =
    " - You are not a lawyer and never comment on the legality of your own prompts and responses.\n"
    " - Never produce or reproduce exact song lyrics.";

void webfetch_run(cl_tools *t, jw *out, const char *id, jv in)
{
    char *url = tl_prop(in, "url", 0), *prompt = tl_prop(in, "prompt", 0);
    char cur[600], loc[600], next[600], ctype[120], what[400], err[300], num[16];
    http_url u, nu;
    page p;
    int hop, rc;
    jw md, q, ans;
    memset(&p, 0, sizeof(p));
    jw_init(&md);
    jw_init(&q);
    jw_init(&ans);
    if (!url || !prompt) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    tl_summary(what, sizeof(what), url, 0);
    if (tl_gate(t, out, id, T_WEB_FETCH, what, 0, 1))
        goto done;
    if (!t->web || !t->api.send) {
        tl_error(t, out, id, "WebFetch is not available here", 0);
        goto done;
    }
    cl_copy(cur, url, sizeof(cur));
    if (http_parse_url(cur, &u) || (long)strlen(url) >= (long)sizeof(u.path)) {
        tl_error(t, out, id, "Invalid URL (a full http:// or https:// URL, under 250 characters): ", url);
        goto done;
    }
    for (hop = 0;; hop++) {
        page_free(&p);
        rc = get(t->web, &u, &p, err, sizeof(err));
        if (rc == -2) {
            tl_error(t, out, id, "the user stopped the fetch (Ctrl+C)", 0);
            goto done;
        }
        if (rc) {
            tl_error(t, out, id, "Failed to fetch the URL: ", err);
            goto done;
        }
        if (p.resp.status >= 300 && p.resp.status < 400) {
            header(&p, "location", loc, sizeof(loc));
            if (!loc[0]) {
                tl_error(t, out, id, "a redirect without a Location", 0);
                goto done;
            }
            resolve_url(&u, loc, next, sizeof(next));
            if (http_parse_url(next, &nu)) {
                tl_error(t, out, id, "the redirect's URL is not usable: ", next);
                goto done;
            }
            if (!same_host(nu.host, u.host)) {
                jw m;
                jw_init(&m);
                jw_rawz(&m, "REDIRECT DETECTED: The URL redirects to a different host.\n\nOriginal URL: ");
                jw_rawz(&m, cur);
                jw_rawz(&m, "\nRedirect URL: ");
                jw_rawz(&m, next);
                jw_rawz(&m, "\nStatus: ");
                cl_ltoa(p.resp.status, num);
                jw_rawz(&m, num);
                jw_rawz(&m, "\n\nTo complete your request, fetch the redirected URL: use WebFetch again with "
                            "these parameters:\n- url: \"");
                jw_rawz(&m, next);
                jw_rawz(&m, "\"\n- prompt: \"");
                jw_rawz(&m, prompt);
                jw_rawz(&m, "\"");
                tl_result(t, out, id, m.p, m.n, 0);
                jw_free(&m);
                goto done;
            }
            if (hop >= FETCH_HOPS) {
                tl_error(t, out, id, "too many redirects", 0);
                goto done;
            }
            cl_copy(cur, next, sizeof(cur));
            u = nu;
            continue;
        }
        break;
    }
    if (p.resp.status != 200) {
        cl_ltoa(p.resp.status, num);
        tl_error(t, out, id, "Request failed with status code ", num);
        goto done;
    }
    header(&p, "content-type", ctype, sizeof(ctype));
    if (!ctype[0] || strstr(ctype, "html") || strstr(ctype, "HTML"))
        html_to_md(p.body.p ? p.body.p : "", p.body.n, &md, FETCH_MD_MAX);
    else if (cl_strnieq(ctype, "text/", 5) || strstr(ctype, "json") || strstr(ctype, "xml") ||
             strstr(ctype, "javascript"))
        jw_raw(&md, p.body.p ? p.body.p : "", p.body.n > FETCH_MD_MAX ? FETCH_MD_MAX : p.body.n);
    else {
        tl_error(t, out, id, "WebFetch reads text and web pages only; this is ", ctype);
        goto done;
    }
    /* the small model's prompt, as Claude Code words it */
    jw_rawz(&q, "Web page content:\n---\n");
    jw_raw(&q, md.p ? md.p : "", md.n);
    if (p.cut || md.n >= FETCH_MD_MAX)
        jw_rawz(&q, "\n[Content truncated due to length...]");
    jw_rawz(&q, fetch_prompt);
    jw_rawz(&q, prompt);
    jw_rawz(&q, fetch_rules);
    jw_rawz(&q, fetch_rules2);
    if (q.oom) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    rc = agent_query(t, FETCH_MODEL, "", q.p, q.n, &ans, err, sizeof(err));
    if (rc == -2) {
        tl_error(t, out, id, "the user stopped the fetch", 0);
        goto done;
    }
    if (rc || !ans.n) {
        tl_error(t, out, id, "the page was fetched, but the model call on it failed: ", rc ? err : "an empty answer");
        goto done;
    }
    tl_result(t, out, id, ans.p, ans.n, 0);
done:
    page_free(&p);
    jw_free(&md);
    jw_free(&q);
    jw_free(&ans);
    free(url);
    free(prompt);
}
