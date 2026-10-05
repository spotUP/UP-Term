/* http -- see http.h. */
#include <string.h>
#include <stdlib.h>
#include "http.h"
#include "util.h"

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* case-insensitive: does s start with the lower-case prefix p? */
static int starts_ci(const char *s, const char *p)
{
    while (*p) {
        if (lower((unsigned char)*s) != *p)
            return 0;
        s++;
        p++;
    }
    return 1;
}

int http_parse_url(const char *url, http_url *u)
{
    const char *h, *e, *colon;
    long n;
    memset(u, 0, sizeof(*u));
    if (starts_ci(url, "https://")) {
        u->tls = 1;
        u->port = 443;
        h = url + 8;
    } else if (starts_ci(url, "http://")) {
        u->port = 80;
        h = url + 7;
    } else
        return -1;
    e = h;
    while (*e && *e != '/')
        e++;
    colon = h;
    while (colon < e && *colon != ':')
        colon++;
    n = (long)(colon - h);
    if (n <= 0 || n >= (long)sizeof(u->host))
        return -1;
    memcpy(u->host, h, (size_t)n);
    u->host[n] = 0;
    if (colon < e) {
        long p = 0;
        const char *d = colon + 1;
        if (d == e)
            return -1;
        for (; d < e; d++) {
            if (*d < '0' || *d > '9')
                return -1;
            p = p * 10 + (*d - '0');
            if (p > 65535)
                return -1;
        }
        if (p == 0)
            return -1;
        u->port = (int)p;
    }
    if (!*e)
        strcpy(u->path, "/");
    else {
        if (strlen(e) >= sizeof(u->path))
            return -1;
        strcpy(u->path, e);
    }
    return 0;
}

typedef struct obuf {
    char *p;
    long n, cap;
    int over;
} obuf;

static void put(obuf *o, const char *s)
{
    long l = (long)strlen(s);
    if (o->n + l > o->cap) {
        o->over = 1;
        return;
    }
    memcpy(o->p + o->n, s, (size_t)l);
    o->n += l;
}

long http_request_head(const http_req *r, char *out, long cap)
{
    obuf o;
    char num[32];
    o.p = out;
    o.n = 0;
    o.cap = cap;
    o.over = 0;
    put(&o, "POST ");
    put(&o, r->path);
    put(&o, " HTTP/1.1\r\nHost: ");
    put(&o, r->host);
    if (r->port != (r->tls ? 443 : 80)) {
        num[0] = ':';
        cl_ltoa(r->port, num + 1);
        put(&o, num);
    }
    put(&o, "\r\nUser-Agent: UP-Term-Claude/1.0 (AmigaOS)\r\n"
            "Accept: text/event-stream\r\n"
            "Content-Type: application/json\r\n"
            "anthropic-version: 2023-06-01\r\n");
    if (r->beta && *r->beta) {
        put(&o, "anthropic-beta: ");
        put(&o, r->beta);
        put(&o, "\r\n");
    }
    if (r->key && *r->key) {
        put(&o, "x-api-key: ");
        put(&o, r->key);
        put(&o, "\r\n");
    }
    cl_ltoa(r->body_len, num);
    put(&o, "Content-Length: ");
    put(&o, num);
    put(&o, "\r\nConnection: keep-alive\r\n\r\n");
    return o.over ? -1 : o.n;
}

long http_redact(const char *head, long n, char *out, long cap)
{
    long i = 0, o = 0;
    while (i < n) {
        long e = i, l;
        int key;
        while (e < n && head[e] != '\n')
            e++;
        if (e < n)
            e++;                    /* the line with its LF */
        key = e - i >= 10 && starts_ci(head + i, "x-api-key:");
        if (key) {
            const char *rep = "x-api-key: [redacted]\r\n";
            l = (long)strlen(rep);
            if (o + l > cap)
                return -1;
            memcpy(out + o, rep, (size_t)l);
        } else {
            l = e - i;
            if (o + l > cap)
                return -1;
            memcpy(out + o, head + i, (size_t)l);
        }
        o += l;
        i = e;
    }
    return o;
}

void http_resp_init(http_resp *r, http_body_fn body, void *u)
{
    memset(r, 0, sizeof(*r));
    r->state = HR_STATUS;
    r->retry_after = -1;
    r->length = -1;
    r->body = body;
    r->u = u;
}

static const char *trim(char *s)
{
    char *e;
    while (*s == ' ' || *s == '\t')
        s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t'))
        *--e = 0;
    return s;
}

static int status_line(http_resp *r)
{
    const char *s = r->line;
    int st = 0, i;
    if (!starts_ci(s, "http/1."))
        return -1;
    s += 7;
    if (*s != '0' && *s != '1')
        return -1;
    s++;
    if (*s != ' ')
        return -1;
    s++;
    for (i = 0; i < 3; i++, s++) {
        if (*s < '0' || *s > '9')
            return -1;
        st = st * 10 + (*s - '0');
    }
    if (*s && *s != ' ')
        return -1;
    r->status = st;
    return 0;
}

static void header_line(http_resp *r)
{
    char *colon = strchr(r->line, ':');
    const char *v;
    char *k;
    if (!colon)
        return;
    *colon = 0;
    for (k = r->line; *k; k++)
        *k = (char)lower((unsigned char)*k);
    v = trim(colon + 1);
    if (!strcmp(r->line, "content-length")) {
        char *end;
        long l = strtol(v, &end, 10);
        if (end != v && l >= 0)
            r->length = l;
    } else if (!strcmp(r->line, "transfer-encoding")) {
        if (starts_ci(v, "chunked"))
            r->chunked = 1;
    } else if (!strcmp(r->line, "connection")) {
        if (starts_ci(v, "close"))
            r->close = 1;
    } else if (!strcmp(r->line, "content-type")) {
        if (starts_ci(v, "text/event-stream"))
            r->event_stream = 1;
    } else if (!strcmp(r->line, "retry-after")) {
        char *end;
        long s = strtol(v, &end, 10);
        if (end != v && s >= 0)
            r->retry_after = s;
    } else if (!strcmp(r->line, "request-id")) {
        strncpy(r->request_id, v, sizeof(r->request_id) - 1);
        r->request_id[sizeof(r->request_id) - 1] = 0;
    }
}

/* The header block ended: how the body comes. */
static void body_starts(http_resp *r)
{
    if (r->chunked) {
        r->state = HR_CHUNK_SIZE;
        r->length = -1;
    } else if (r->length >= 0) {
        r->left = r->length;
        r->state = r->left ? HR_BODY : HR_DONE;
    } else if (r->status == 204 || r->status == 304) {
        r->state = HR_DONE;
    } else {
        r->close = 1;               /* the body runs to the end of the connection */
        r->left = -1;
        r->state = HR_BODY;
    }
}

static int chunk_size(http_resp *r)
{
    const char *s = r->line;
    long v = 0;
    int digits = 0;
    for (; *s; s++) {
        int c = lower((unsigned char)*s), d;
        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'f')
            d = c - 'a' + 10;
        else
            break;
        if (v > 0x7ffffffL / 16)
            return -1;
        v = v * 16 + d;
        digits++;
    }
    if (!digits || (*s && *s != ';' && *s != ' ' && *s != '\t'))
        return -1;
    r->left = v;
    r->state = v ? HR_CHUNK_DATA : HR_TRAILER;
    return 0;
}

/* A whole line (without its CR LF) is in r->line. */
static int line_done(http_resp *r)
{
    switch (r->state) {
    case HR_STATUS:
        if (status_line(r))
            return -1;
        r->state = HR_HEADERS;
        return 0;
    case HR_HEADERS:
        if (!r->ll) {
            if (r->status >= 100 && r->status < 200) {
                r->state = HR_STATUS;   /* an interim answer; the real one follows */
                return 0;
            }
            body_starts(r);
        } else
            header_line(r);
        return 0;
    case HR_CHUNK_SIZE:
        return chunk_size(r);
    case HR_CHUNK_CRLF:
        if (r->ll)
            return -1;
        r->state = HR_CHUNK_SIZE;
        return 0;
    case HR_TRAILER:
        if (!r->ll)
            r->state = HR_DONE;
        return 0;
    }
    return -1;
}

int http_resp_feed(http_resp *r, const char *s, long n)
{
    long i = 0;
    while (i < n) {
        if (r->state == HR_BAD)
            return -1;
        if (r->state == HR_DONE)
            return 0;               /* bytes past the response are not ours */
        if (r->state == HR_BODY || r->state == HR_CHUNK_DATA) {
            long take = n - i;
            if (r->left >= 0 && take > r->left)
                take = r->left;
            if (take > 0 && r->body)
                r->body(r->u, s + i, take);
            i += take;
            if (r->left >= 0) {
                r->left -= take;
                if (!r->left)
                    r->state = r->state == HR_BODY ? HR_DONE : HR_CHUNK_CRLF;
            }
            continue;
        }
        /* a line-oriented state */
        if (s[i] == '\n') {
            if (r->ll && r->line[r->ll - 1] == '\r')
                r->ll--;
            r->line[r->ll] = 0;
            if (line_done(r)) {
                r->state = HR_BAD;
                return -1;
            }
            r->ll = 0;
        } else if (r->ll < (int)sizeof(r->line) - 1)
            r->line[r->ll++] = s[i];
        i++;
    }
    return 0;
}

int http_resp_eof(http_resp *r)
{
    if (r->state == HR_BODY && r->left < 0) {
        r->state = HR_DONE;
        return 0;
    }
    return r->state == HR_DONE ? 0 : -1;
}
