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
#include "conv.h"
#include "stream.h"
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

/* One request (GET, or a POST with a body and headers of its own): 0 with
 * the page, -1 with the reason in err, -2 stopped (Ctrl+C). idle_ms: how
 * long without a byte before it is given up. */
/* WebFetch's deadline (CLAUDE_CODE_WEBFETCH_DEADLINE_MS): the tools whose
 * clock counts and the moment, 0 none */
static const cl_tools *dl_tools;
static unsigned long dl_end;

static int request(cl_net *net, const http_url *u, const char *post, long pn, const char *headers, page *p,
                   char *err, long cap, long idle_ms)
{
    char buf[2048], num[16];
    long idle = 0;
    int rc;
    jw head;
    jw_init(&p->head);
    jw_init(&p->body);
    jw_init(&head);
    p->head_done = 0;
    p->cut = 0;
    http_resp_init(&p->resp, on_body, p);
    jw_rawz(&head, post ? "POST " : "GET ");
    jw_rawz(&head, u->path[0] ? u->path : "/");
    jw_rawz(&head, " HTTP/1.1\r\nHost: ");
    jw_rawz(&head, u->host);
    if (u->port != (u->tls ? 443 : 80)) {
        cl_ltoa(u->port, num);
        jw_rawz(&head, ":");
        jw_rawz(&head, num);
    }
    if (post) {
        jw_rawz(&head, "\r\nUser-Agent: C-Claude/1.0 (AmigaOS; UP-Term)\r\nContent-Type: application/json\r\n"
                       "Content-Length: ");
        cl_ltoa(pn, num);
        jw_rawz(&head, num);
        jw_rawz(&head, "\r\n");
        if (headers)
            jw_rawz(&head, headers);
        jw_rawz(&head, "Accept-Encoding: identity\r\nConnection: close\r\n\r\n");
    } else
        jw_rawz(&head, "\r\nUser-Agent: C-Claude/1.0 (AmigaOS; UP-Term)\r\nAccept: text/html, text/markdown, "
                       "text/plain, */*;q=0.8\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");
    if (head.oom) {
        jw_free(&head);
        cl_copy(err, "out of memory", cap);
        return -1;
    }
    rc = net->open(net->u, u->host, u->port, u->tls);
    if (rc == NET_BREAK) {
        jw_free(&head);
        return -2;
    }
    if (rc) {
        jw_free(&head);
        cl_copy(err, "cannot connect: ", cap);
        cl_cat(err, net->err(net->u), cap);
        return -1;
    }
    rc = (int)net->send(net->u, head.p, head.n);
    if (rc == head.n && post && pn)
        rc = (int)net->send(net->u, post, pn) == pn ? (int)head.n : -1;
    if (rc != head.n) {
        jw_free(&head);
        net->close(net->u);
        if (rc == NET_BREAK)
            return -2;
        cl_copy(err, "sending the request failed: ", cap);
        cl_cat(err, net->err(net->u), cap);
        return -1;
    }
    jw_free(&head);
    for (;;) {
        long n = net->recv(net->u, buf, sizeof(buf), 250);
        if (dl_end && dl_tools && dl_tools->clock && dl_tools->clock(dl_tools->u) > dl_end) {
            net->close(net->u);
            cl_copy(err, "the page did not arrive before the deadline (CLAUDE_CODE_WEBFETCH_DEADLINE_MS)", cap);
            return -1;
        }
        if (n == NET_TIMEOUT) {
            idle += 250;
            if (idle < idle_ms)
                continue;
            net->close(net->u);
            cl_copy(err, "no answer in time", cap);
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

static int get(cl_net *net, const http_url *u, page *p, char *err, long cap)
{
    return request(net, u, 0, 0, 0, p, err, cap, FETCH_IDLE);
}

int web_post(cl_net *net, const char *url, const char *headers, const char *body, long bn, int timeout_s,
             int *status, jw *resp, char *err, long cap)
{
    http_url u;
    page p;
    int rc;
    char cur[600];
    memset(&p, 0, sizeof(p));
    cl_copy(cur, url, sizeof(cur));
    if (http_parse_url(cur, &u)) {
        cl_copy(err, "not a usable URL", cap);
        return -1;
    }
    rc = request(net, &u, body, bn, headers, &p, err, cap, (long)(timeout_s > 0 ? timeout_s : 600) * 1000L);
    if (rc == 0) {
        *status = p.resp.status;
        jw_raw(resp, p.body.p ? p.body.p : "", p.body.n);
    } else if (rc == -2)
        cl_copy(err, "stopped (Ctrl+C)", cap);
    page_free(&p);
    return rc ? -1 : 0;
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

/* n bytes as Claude Code's formatFileSize writes them: "512 bytes", "1.5KB",
 * "2MB" (one decimal, a trailing .0 dropped) */
static void file_size(long n, char *out, long cap)
{
    static const char *const unit[3] = { "KB", "MB", "GB" };
    char num[16];
    long tenths;
    int u = 0;
    if (n < 1024) {
        cl_ltoa(n, out);
        cl_cat(out, " bytes", cap);
        return;
    }
    /* tenths of a KB (MB, GB), rounded as toFixed(1) does */
    tenths = (n / 1024) * 10 + ((n % 1024) * 10 + 512) / 1024;
    while (tenths >= 10240 && u < 2) {
        tenths = (tenths + 512) / 1024;
        u++;
    }
    cl_ltoa(tenths / 10, num);
    cl_copy(out, num, cap);
    if (tenths % 10) {
        cl_cat(out, ".", cap);
        cl_ltoa(tenths % 10, num);
        cl_cat(out, num, cap);
    }
    cl_cat(out, unit[u], cap);
}

/* the screen's line for a fetch, Claude Code's "Received 12.3KB (200 OK)":
 * the answer goes to Claude, the user sees what came */
static void received(char *out, long cap, long bytes, int status)
{
    char num[16];
    cl_copy(out, "Received ", cap);
    file_size(bytes, out + strlen(out), cap - (long)strlen(out));
    cl_cat(out, " (", cap);
    cl_ltoa(status, num);
    cl_cat(out, num, cap);
    cl_cat(out, status == 200 ? " OK)" : ")", cap);
}

/* Claude Code keeps a fetched page 15 minutes: the same URL again is not
 * fetched again (the prompt is still answered anew) */
#define CACHE_N 4
#define CACHE_MS (15UL * 60 * 1000)

typedef struct fentry {
    char url[600];
    unsigned long ms;
    char *md;
    long n, body_n;
    int cut;
} fentry;

void webfetch_cache_free(cl_tools *t)
{
    fentry *c = (fentry *)t->fetch_cache;
    int i;
    if (!c)
        return;
    for (i = 0; i < CACHE_N; i++)
        free(c[i].md);
    free(c);
    t->fetch_cache = 0;
}

static fentry *cache_find(cl_tools *t, const char *url)
{
    fentry *c = (fentry *)t->fetch_cache;
    unsigned long now = t->clock ? t->clock(t->u) : 0;
    int i;
    if (!c || !t->clock)
        return 0;
    for (i = 0; i < CACHE_N; i++)
        if (c[i].md && !strcmp(c[i].url, url) && now - c[i].ms < (t->fetch_ttl_ms > 0 ? (unsigned long)t->fetch_ttl_ms : CACHE_MS))
            return &c[i];
    return 0;
}

static void cache_put(cl_tools *t, const char *url, const char *md, long n, long body_n, int cut)
{
    fentry *c = (fentry *)t->fetch_cache, *e;
    int i, old = 0;
    if (!t->clock)
        return;
    if (!c) {
        c = (fentry *)calloc(CACHE_N, sizeof(fentry));
        if (!c)
            return;
        t->fetch_cache = c;
    }
    for (i = 1; i < CACHE_N; i++)
        if (!c[i].md || c[i].ms < c[old].ms)
            old = i;
    e = &c[old];
    free(e->md);
    e->md = (char *)malloc((size_t)n + 1);
    if (!e->md)
        return;
    memcpy(e->md, md, (size_t)n);
    e->md[n] = 0;
    e->n = n;
    e->body_n = body_n;
    e->cut = cut;
    cl_copy(e->url, url, sizeof(e->url));
    e->ms = t->clock(t->u);
}

/* Claude Code's preapproved documentation hosts: WebFetch reads them without
 * a question (an explicit WebFetch(domain:...) rule still decides). The
 * docs name the set without listing it; this is the list Claude Code ships. */
static const char *const preapproved[] = {
    "platform.claude.com", "code.claude.com", "docs.anthropic.com", "modelcontextprotocol.io", "agentskills.io",
    "docs.python.org", "en.cppreference.com", "docs.oracle.com", "learn.microsoft.com", "developer.mozilla.org",
    "go.dev", "pkg.go.dev", "www.php.net", "docs.swift.org", "kotlinlang.org", "ruby-doc.org", "doc.rust-lang.org",
    "www.typescriptlang.org", "react.dev", "angular.io", "vuejs.org", "nextjs.org", "expressjs.com", "nodejs.org",
    "bun.sh", "jquery.com", "getbootstrap.com", "tailwindcss.com", "d3js.org", "threejs.org", "redux.js.org",
    "webpack.js.org", "jestjs.io", "reactrouter.com", "docs.djangoproject.com", "flask.palletsprojects.com",
    "fastapi.tiangolo.com", "pandas.pydata.org", "numpy.org", "www.tensorflow.org", "pytorch.org",
    "scikit-learn.org", "matplotlib.org", "requests.readthedocs.io", "jupyter.org", "laravel.com", "symfony.com",
    "wordpress.org", "docs.spring.io", "hibernate.org", "tomcat.apache.org", "gradle.org", "maven.apache.org",
    "asp.net", "dotnet.microsoft.com", "nuget.org", "blazor.net", "reactnative.dev", "docs.flutter.dev",
    "developer.apple.com", "developer.android.com", "keras.io", "spark.apache.org", "huggingface.co",
    "www.kaggle.com", "www.mongodb.com", "redis.io", "www.postgresql.org", "dev.mysql.com", "www.sqlite.org",
    "graphql.org", "prisma.io", "docs.aws.amazon.com", "cloud.google.com", "kubernetes.io", "www.docker.com",
    "www.terraform.io", "www.ansible.com", "docs.netlify.com", "devcenter.heroku.com", "cypress.io",
    "selenium.dev", "docs.unity.com", "docs.unrealengine.com", "git-scm.com", "nginx.org", "httpd.apache.org", 0
};

int webfetch_preapproved(const char *host)
{
    int i;
    for (i = 0; preapproved[i]; i++)
        if (cl_strieq(host, preapproved[i]))
            return 1;
    return 0;
}

void webfetch_run(cl_tools *t, jw *out, const char *id, jv in)
{
    char *url = tl_prop(in, "url", 0), *prompt = tl_prop(in, "prompt", 0);
    char cur[600], loc[600], next[600], ctype[120], what[400], err[300], num[16];
    http_url u, nu;
    page p;
    int hop, rc, cached = 0;
    jw md, q, ans;
    memset(&p, 0, sizeof(p));
    jw_init(&md);
    jw_init(&q);
    jw_init(&ans);
    if (!url || !prompt) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    cl_copy(cur, url, sizeof(cur));
    if (http_parse_url(cur, &u) || (long)strlen(url) >= (long)sizeof(u.path)) {
        tl_error(t, out, id, "Invalid URL (a full http:// or https:// URL, under 250 characters): ", url);
        goto done;
    }
    if (!strchr(u.host, '.')) {
        /* Claude Code refuses localhost and intranet names before any request */
        tl_error(t, out, id, "WebFetch cannot fetch localhost or other hostnames without a dot. To reach a local "
                             "server, use Bash instead (an HTTP client such as curl or wget, if one is installed).", 0);
        goto done;
    }
    if (!u.tls && !(strchr(cur + 7, ':') && (!strchr(cur + 7, '/') || strchr(cur + 7, ':') < strchr(cur + 7, '/')))) {
        /* Claude Code: http is upgraded to https (a URL with a port of its own
         * names a plain server and stays as it is) */
        cl_copy(next, "https://", sizeof(next));
        cl_cat(next, cur + 7, sizeof(next));
        if (http_parse_url(next, &nu) == 0) {
            cl_copy(cur, next, sizeof(cur));
            u = nu;
        }
    }
    tl_summary(what, sizeof(what), cur, 0);
    if (!t->rule_ask && webfetch_preapproved(u.host) && !perm_refused(&t->perm, T_WEB_FETCH)) {
        if (t->show)
            t->show(t->u, "WebFetch", what);   /* a preapproved documentation host: no question */
    } else if (tl_gate(t, out, id, T_WEB_FETCH, what, 0, 1))
        goto done;
    if (!t->web || !t->api.send) {
        tl_error(t, out, id, "WebFetch is not available here", 0);
        goto done;
    }
    {
        fentry *hit = cache_find(t, url);
        if (hit) {
            jw_raw(&md, hit->md, hit->n);
            p.body.n = hit->body_n;
            p.cut = hit->cut;
            p.resp.status = 200;
            cached = 1;
            t->n_fetch_cached++;
            goto answer;
        }
    }
    for (hop = 0;; hop++) {
        page_free(&p);
        /* Claude Code: the page and its redirects within five minutes (CLAUDE_CODE_WEBFETCH_DEADLINE_MS) */
        dl_tools = t;
        if (!hop)
            dl_end = t->clock && t->fetch_deadline_ms > 0 ? t->clock(t->u) + (unsigned long)t->fetch_deadline_ms : 0;
        rc = get(t->web, &u, &p, err, sizeof(err));
        dl_tools = 0;
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
    if (!md.oom)
        cache_put(t, url, md.p ? md.p : "", md.n, p.body.n, p.cut);
answer:
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
    received(t->brief, sizeof(t->brief), p.body.n, p.resp.status);
    if (cached)
        cl_cat(t->brief, " (cached)", sizeof(t->brief));
    tl_result(t, out, id, ans.p, ans.n, 0);
done:
    page_free(&p);
    jw_free(&md);
    jw_free(&q);
    jw_free(&ans);
    free(url);
    free(prompt);
}

/* ---- WebSearch (A4 gaps 2): Claude Code's client tool; the search is the
 * API's web_search server tool, run in a request of its own with the
 * domain lists, its results' titles and URLs (and the model's summary)
 * the tool's result ---- */

#define SEARCH_CAP 200
#define SEARCH_MAX_TOKENS 8192L

/* a JSON array of strings from the input, as raw JSON ("" none or empty) */
static void domains(jv in, const char *key, jw *w)
{
    jv a;
    if (json_get(in, key, &a) && json_type(a) == J_ARR && json_count(a) > 0)
        jw_raw(w, a.p, a.n);
}

void websearch_run(cl_tools *t, jw *out, const char *id, jv in)
{
    char *query = tl_prop(in, "query", 0), what[300], num[16];
    long cap = t->max_searches > 0 ? t->max_searches : SEARCH_CAP;
    jw al, bl, tools, body, links, text;
    cl_conv c;
    cl_opts o;
    cl_stream st;
    int rc, i, searches = 0, nlinks = 0;
    jw_init(&al);
    jw_init(&bl);
    jw_init(&tools);
    jw_init(&body);
    jw_init(&links);
    jw_init(&text);
    conv_init(&c);
    memset(&st, 0, sizeof(st));
    if (!query) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    tl_summary(what, sizeof(what), query, 0);
    if (tl_gate(t, out, id, T_WEB_SEARCH, what, 0, 1))
        goto done;
    if (!t->api.send) {
        tl_error(t, out, id, "WebSearch is not available here", 0);
        goto done;
    }
    if (t->searches && *t->searches >= cap) {
        /* Claude Code: a notice, not an error that would invite a retry */
        static const char lim[] = "The web search limit of this session is reached: go on with the information you "
                                  "have already gathered. If you need more searches, ask the user to raise "
                                  "CLAUDE_CODE_MAX_WEB_SEARCHES_PER_SESSION.";
        tl_result(t, out, id, lim, (long)sizeof(lim) - 1, 0);
        goto done;
    }
    if (t->searches)
        (*t->searches)++;
    domains(in, "allowed_domains", &al);
    domains(in, "blocked_domains", &bl);
    jw_raw(&tools, "[", 1);
    tools_search_tool(&tools, t->model, al.p, bl.p);
    jw_raw(&tools, "]", 1);
    memset(&o, 0, sizeof(o));
    o.model = t->model ? t->model : "claude-opus-5-5";
    o.effort = "";
    o.max_tokens = SEARCH_MAX_TOKENS;
    o.system = "You are an assistant for performing a web search tool use.";
    o.tools = tools.p;
    jw_rawz(&text, "Perform a web search for the query: ");
    jw_rawz(&text, query);
    if (tools.oom || text.oom || conv_add_user_text(&c, text.p, text.n) || conv_body(&c, &o, &body)) {
        tl_error(t, out, id, "out of memory", 0);
        goto done;
    }
    jw_reset(&text);
    for (i = 0;; i++) {
        int k;
        rc = t->api.send(t->api.u, body.p, body.n, &st);
        if (rc) {
            tl_error(t, out, id, rc == -2 ? "the user stopped the search" : "the search request failed", 0);
            goto done;
        }
        for (k = 0; k < st.nb; k++) {
            sblock *b = &st.b[k];
            jv v, x, e, y;
            jit it;
            if (b->type == B_SERVER)
                searches++;
            else if (b->type == B_TEXT && b->a.n) {
                if (text.n)
                    jw_rawz(&text, "\n\n");
                jw_raw(&text, b->a.p, b->a.n);
            } else if (b->type == B_OTHER && json_parse(b->start.p, b->start.n, &v) == 0 &&
                       json_get(v, "type", &x) && json_streq(x, "web_search_tool_result") &&
                       json_get(v, "content", &x) && json_type(x) == J_ARR) {
                json_iter(x, &it);
                while (json_next(&it, 0, &e)) {
                    if (nlinks)
                        jw_raw(&links, ",", 1);
                    jw_rawz(&links, "{\"title\":");
                    if (json_get(e, "title", &y) && json_type(y) == J_STR)
                        jw_raw(&links, y.p, y.n);
                    else
                        jw_rawz(&links, "\"\"");
                    jw_rawz(&links, ",\"url\":");
                    if (json_get(e, "url", &y) && json_type(y) == J_STR)
                        jw_raw(&links, y.p, y.n);
                    else
                        jw_rawz(&links, "\"\"");
                    jw_raw(&links, "}", 1);
                    nlinks++;
                }
            }
        }
        if (strcmp(st.stop_reason, "pause_turn") || i >= 4)
            break;
        {
            /* the server's loop paused: the turn goes on from where it is */
            jw content;
            jw_init(&content);
            if (stream_content(&st, &content) || conv_add(&c, 0, content.p, content.n)) {
                jw_free(&content);
                tl_error(t, out, id, "out of memory", 0);
                goto done;
            }
            jw_free(&content);
            stream_free(&st);
            memset(&st, 0, sizeof(st));
            jw_reset(&body);
            if (conv_body(&c, &o, &body)) {
                tl_error(t, out, id, "out of memory", 0);
                goto done;
            }
        }
    }
    {
        jw m;
        jw_init(&m);
        jw_rawz(&m, "Web search results for query: \"");
        jw_rawz(&m, query);
        jw_rawz(&m, "\"\n\nLinks: [");
        jw_raw(&m, links.p ? links.p : "", links.n);
        jw_rawz(&m, "]\n\n");
        jw_raw(&m, text.p ? text.p : "", text.n);
        jw_rawz(&m, "\n\nREMINDER: You MUST include the sources above in your response to the user using "
                    "markdown hyperlinks.");
        cl_copy(t->brief, "Did ", sizeof(t->brief));
        cl_ltoa(searches ? searches : 1, num);
        cl_cat(t->brief, num, sizeof(t->brief));
        cl_cat(t->brief, searches > 1 ? " searches" : " search", sizeof(t->brief));
        tl_result(t, out, id, m.p ? m.p : "", m.n, 0);
        jw_free(&m);
    }
done:
    stream_free(&st);
    conv_free(&c);
    jw_free(&al);
    jw_free(&bl);
    jw_free(&tools);
    jw_free(&body);
    jw_free(&links);
    jw_free(&text);
    free(query);
}
