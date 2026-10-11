/* request -- one LLM request over HTTP, for C:Claude and amiga-pi (amiga-pi
 * plan B3, B7, B8).
 *
 * llm_pool: one cl_net connection per host:port, at most LLM_POOL open,
 * the least recently used closed first (a model switch, a subagent on
 * another host, MCP over HTTP; a TLS reconnect costs seconds on a 68k).
 *
 * llm_post: one attempt. A pooled connection (a kept one whose server
 * closed it is opened again once), the head and body sent, the answer read
 * with an idle limit and the caller's break check, the HTTP body to SSE
 * and each event to the caller and/or a backend's decoder. The outcome is
 * a code; what to say about it is the caller's (C:Claude keeps its own
 * words and retries).
 *
 * llm_request: amiga-pi's whole request after pi-ai: the body from the
 * backend, the LAN rule for plain http (llm_lan_ok; a key is never sent
 * over http), retries (pi's defaults: 3, 2000 ms doubling, 60000 ms cap,
 * Retry-After honoured) while nothing of the answer has arrived, and every
 * failure as an event in the stream (error, or aborted on the break).
 * Portable C89 over cl_net, host-tested (tests/test_pi_llm.c). */
#ifndef LLM_REQUEST_H
#define LLM_REQUEST_H

#include "../claude/net.h"
#include "../claude/http.h"
#include "../claude/sse.h"
#include "backend.h"

#define LLM_POOL 4

typedef struct llm_conn {
    cl_net *net;
    char host[128];
    int port, tls;
    int open;
    unsigned long last;         /* the pool's tick when last used */
} llm_conn;

typedef struct llm_pool {
    llm_conn c[LLM_POOL];
    int n;
    unsigned long tick;
    long opens;                 /* connections opened (the tests count them) */
    cl_net *failed;             /* the transport whose open failed last */
} llm_pool;

/* nets: n (1..LLM_POOL) transports, one per slot */
void llm_pool_init(llm_pool *p, cl_net *const *nets, int n);
/* the connection to host:port: an open one (*reused 1), else the least
 * recently used slot closed and opened there. 0 with *rc (NET_ERROR,
 * NET_BREAK) when the open fails. */
llm_conn *llm_pool_get(llm_pool *p, const char *host, int port, int tls, int *reused, int *rc);
void llm_pool_drop(llm_conn *c);            /* close one */
void llm_pool_close(llm_pool *p);           /* close all */
int llm_pool_open(const llm_pool *p);       /* how many are open */

/* the LAN rule (owner, 2026-10-07): plain http only to 127/8, 10/8,
 * 172.16/12, 192.168/16 (the address the name resolved to; ip 0 when
 * unknown) or a name ending in ".local" */
int llm_lan_ok(const char *host, const unsigned char *ip);
/* a dotted IPv4 literal into ip: 1, else 0 */
int llm_ipv4(const char *host, unsigned char ip[4]);

enum { LP_OK, LP_CONNECT, LP_SEND, LP_RECV, LP_EOF, LP_BADHTTP, LP_IDLE, LP_BREAK };
enum { LPS_CONNECT, LPS_WAIT };

typedef struct llm_hooks {
    void *u;
    int (*poll)(void *u);                   /* nonzero: the user stopped it (Ctrl+C) */
    void (*status)(void *u, int what);      /* LPS_*: connecting, waiting for the answer */
    unsigned long (*ms)(void *u);           /* a millisecond clock, 0 none */
    /* the head for this attempt (C:Claude logs it redacted here): 0, -1 */
    int (*head)(void *u, const char **head, long *hn);
    void (*begin)(void *u);                 /* sent: the answer's reading starts */
    void (*restart)(void *u);               /* a kept connection died early: everything read is dropped */
    void (*event)(void *u, const char *event, const char *data, long n);   /* each SSE event, 0 none */
    int (*sleep)(void *u, long ms);         /* llm_request's wait: 1 when broken meanwhile */
    void (*notice)(void *u, const char *text);  /* llm_request's notices (a key not sent) */
    /* llm_request: the address host resolves to (the LAN rule): 0, -1 */
    int (*resolve)(void *u, const char *host, unsigned char ip[4]);
    /* llm_request's debug log: the head with the credential redacted, then each event; 0 none */
    void (*log)(void *u, const char *s, long n);
    /* llm_post: every response header, name in lower case; 0 none */
    void (*header)(void *u, const char *name, const char *value);
    /* llm_post: the body of a 2xx answer that is not an event stream (MCP's
     * application/json answers), as it arrives; 0: kept in errbody as before */
    void (*body)(void *u, const char *s, long n);
} llm_hooks;

typedef struct llm_call {
    llm_pool *pool;
    const char *host;
    int port, tls;
    const char *body;
    long bn;
    llm_hooks h;
    const llm_backend *be;      /* with dec and sink: the events decoded here too */
    void *dec;
    const llm_sink *sink;
    long idle_ms;               /* no byte for this long: LP_IDLE */
    char *buf;                  /* the receive buffer */
    long bufcap;
    http_resp *resp;            /* the caller's: status, Retry-After, request id */
    sse *sse;
    jw *errbody;                /* a refusal's body (up to 16 KB) */
    /* out */
    llm_conn *conn;
    int got;                    /* bytes of the answer arrived */
    int reused;
    int bad;                    /* the backend's decoder refused the stream */
    unsigned long t_open, t_first;
    const char *err;            /* the transport's words for LP_CONNECT / LP_SEND / LP_RECV */
} llm_call;

/* one attempt: LP_* (LP_OK: the HTTP answer is complete; its status is in resp) */
int llm_post(llm_call *c);

typedef struct llm_req {
    llm_pool *pool;
    const llm_model *model;
    const llm_backend *be;
    const llm_ctx *ctx;
    const char *key;            /* 0 none */
    const char *user_agent;
    const char *xhead;          /* extra header lines "Name: value\r\n" added to the head (models.json), 0 none */
    llm_hooks h;
    const llm_sink *sink;
    int retries;                /* 0: 3 */
    long base_ms, cap_ms, idle_ms;  /* 0: 2000, 60000, 300000 */
    /* out */
    int attempts;
    int key_sent;               /* the credential header went out */
    long slept_ms;              /* waits between attempts, summed */
    char head[1536];            /* the last head sent (the tests read it) */
    long hn;
} llm_req;

/* the request: 0 when the answer completed (LE_DONE reached the sink), -1
 * when it failed (an LE_ERROR reached the sink) */
int llm_request(llm_req *q);

#endif
