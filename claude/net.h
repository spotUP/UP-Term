/* net -- the one transport interface of the Claude client (ledger A2.1).
 *
 * The program talks to a byte pipe: open it to a host and port (TLS or
 * plain), send, receive with a timeout, close. net_amiga.c is
 * bsdsocket.library with TLS from tls.h (AmiSSL); net_posix.c is plain TCP
 * for the host tests' fixture server; the replay tests hand in a stub that
 * serves a recorded response. Nothing above this interface knows which. */
#ifndef CL_NET_H
#define CL_NET_H

#define NET_ERROR   (-1)        /* the connection failed; err() says why */
#define NET_TIMEOUT (-2)        /* nothing arrived within the timeout */
#define NET_BREAK   (-3)        /* the user pressed Ctrl+C */

/* The longest a transport waits without calling idle: the screen's
 * spinner turns every 80 ms (pi's Loader, pi-tui loader.ts
 * DEFAULT_INTERVAL_MS), so a wait in slices of this length keeps it
 * turning and the keys (Esc) read. */
#define NET_SLICE_MS 80

typedef struct cl_net {
    void *u;
    /* Connects to host:port, with TLS (SNI host, certificate verified) when
     * tls is set. 0, or NET_ERROR / NET_BREAK. */
    int (*open)(void *u, const char *host, int port, int tls);
    /* All n bytes: n, or NET_ERROR / NET_BREAK. */
    long (*send)(void *u, const char *b, long n);
    /* Up to cap bytes: the count, 0 when the far end closed, or
     * NET_TIMEOUT / NET_BREAK / NET_ERROR. */
    long (*recv)(void *u, char *b, long cap, int timeout_ms);
    void (*close)(void *u);
    /* The last failure as text ("" none). */
    const char *(*err)(void *u);
    /* Set by the caller, 0 none: called during open, send and recv at
     * least every NET_SLICE_MS while they wait (the connect, the TLS
     * handshake's round trips and between its steps, a full send buffer)
     * so the caller's screen goes on (its spinner, its keys). Nonzero
     * ends the call with NET_BREAK, as Ctrl+C does. A transport that
     * cannot wait in slices never calls it. */
    int (*idle)(void *iu);
    void *iu;
} cl_net;

#endif
