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
} cl_net;

#endif
