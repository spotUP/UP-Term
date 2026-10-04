/* http -- HTTP/1.1 for the Claude client: the URL, the request head, a
 * copy of the head with the API key blanked for the debug log, and the
 * response read incrementally (status line, headers, then the body as it
 * arrives: chunked, by Content-Length, or until the connection closes).
 * Portable C89, host-tested (tests/test_claude_http.c). */
#ifndef CL_HTTP_H
#define CL_HTTP_H

typedef struct http_url {
    int tls;                    /* https */
    int port;
    char host[128];
    char path[256];
} http_url;

/* "https://host[:port]/path" or "http://...". 0, or -1 when it is not one. */
int http_parse_url(const char *url, http_url *u);

typedef struct http_req {
    const char *host;
    int port;                   /* in the Host header when not 80/443 */
    int tls;
    const char *path;
    const char *key;            /* x-api-key; 0 or "" sends none */
    const char *beta;           /* anthropic-beta; 0 or "" sends none */
    long body_len;
} http_req;

/* The request head into out (cap bytes): its length, or -1 when it does
 * not fit. */
long http_request_head(const http_req *r, char *out, long cap);

/* head[0..n) with every x-api-key header's value replaced by "[redacted]"
 * into out: its length, or -1 when it does not fit. The only form of a
 * request head that may reach a log. */
long http_redact(const char *head, long n, char *out, long cap);

typedef void (*http_body_fn)(void *u, const char *s, long n);

enum { HR_STATUS, HR_HEADERS, HR_BODY, HR_CHUNK_SIZE, HR_CHUNK_DATA, HR_CHUNK_CRLF,
       HR_TRAILER, HR_DONE, HR_BAD };

typedef struct http_resp {
    int state;
    int status;                 /* 200, 429, ... */
    long retry_after;           /* seconds, -1 none */
    int chunked;
    long length;                /* Content-Length, -1 none */
    long left;                  /* body or chunk bytes to come */
    int close;                  /* Connection: close, or no length: read to the end */
    int event_stream;           /* Content-Type: text/event-stream */
    char request_id[64];
    char line[1024];            /* the header line being read (longer lines are cut) */
    int ll;
    http_body_fn body;
    void *u;
} http_resp;

void http_resp_init(http_resp *r, http_body_fn body, void *u);
/* The next bytes from the connection, any split. 0, or -1 when the
 * response is malformed. state HR_DONE once the body is complete. */
int http_resp_feed(http_resp *r, const char *s, long n);
/* The connection closed: 0 when that ends the response properly (a body
 * read to the end), -1 when the response was cut off. */
int http_resp_eof(http_resp *r);

#endif
