/* net_amiga -- net.h on bsdsocket.library (Roadshow, Miami, AmiTCP) with
 * TLS from tls.h (AmiSSL). No ixemul.
 *
 * The socket is non-blocking and every wait is a WaitSelect that also
 * waits for Ctrl+C, so a break ends a connect, a handshake or a quiet
 * stream at once (NET_BREAK). Name lookup (gethostbyname) is the one call
 * that cannot be broken; it is short on a working network. bsdsocket.library
 * opens on the first connection and AmiSSL on the first https one, so a
 * plain http URL (the rig's fixture server) works without AmiSSL. */
#include <string.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <sys/socket.h>
#include <sys/filio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <proto/bsdsocket.h>
#include "net_amiga.h"
#include "tls.h"
#include "util.h"

struct Library *SocketBase;

#ifndef EINPROGRESS
#define EINPROGRESS 36
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK 35
#endif
#ifndef EINTR
#define EINTR 4
#endif

static int tls_ready;               /* tls_init done */

static void set_err(net_amiga *n, const char *a, const char *b)
{
    cl_copy(n->err, a, sizeof(n->err));
    if (b)
        cl_cat(n->err, b, sizeof(n->err));
}

/* 1 ready, 0 timeout, NET_BREAK, NET_ERROR */
static int wait_sock(net_amiga *n, int for_write, long timeout_ms)
{
    fd_set fs;
    struct timeval tv;
    ULONG sigs = SIGBREAKF_CTRL_C;
    LONG rc;
    FD_ZERO(&fs);
    FD_SET(n->sock, &fs);
    tv.tv_secs = timeout_ms / 1000;
    tv.tv_micro = (timeout_ms % 1000) * 1000;
    rc = WaitSelect(n->sock + 1, for_write ? 0 : (APTR)&fs, for_write ? (APTR)&fs : 0, 0,
                    (void *)&tv, &sigs);
    if (sigs & SIGBREAKF_CTRL_C)
        return NET_BREAK;
    if (rc < 0) {
        set_err(n, "waiting on the connection failed", 0);
        return NET_ERROR;
    }
    return rc > 0 ? 1 : 0;
}

static void drop(net_amiga *n)
{
    if (n->tls)
        tls_free((cl_tls *)n->tls);
    n->tls = 0;
    if (n->sock >= 0)
        CloseSocket(n->sock);
    n->sock = -1;
}

static int a_open(void *u, const char *host, int port, int tls)
{
    net_amiga *n = (net_amiga *)u;
    struct sockaddr_in sa;
    struct hostent *he;
    LONG one = 1, rc;
    n->err[0] = 0;
    drop(n);
    if (!SocketBase)
        SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 4);
    if (!SocketBase) {
        set_err(n, "no TCP/IP stack is running (bsdsocket.library)", 0);
        return NET_ERROR;
    }
    memset(&sa, 0, sizeof(sa));
    sa.sin_len = sizeof(sa);
    sa.sin_family = AF_INET;
    sa.sin_port = (unsigned short)port;     /* the 68k is big-endian: network order */
    sa.sin_addr.s_addr = inet_addr((STRPTR)host);
    if (sa.sin_addr.s_addr == (in_addr_t)-1) {
        he = gethostbyname((STRPTR)host);
        if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
            set_err(n, "cannot find the address of ", host);
            return NET_ERROR;
        }
        memcpy(&sa.sin_addr, he->h_addr_list[0], 4);
    }
    n->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (n->sock < 0) {
        set_err(n, "cannot make a socket", 0);
        return NET_ERROR;
    }
    IoctlSocket(n->sock, FIONBIO, (APTR)&one);
    rc = connect(n->sock, (struct sockaddr *)&sa, sizeof(sa));
    if (rc < 0 && Errno() == EINPROGRESS) {
        LONG e = 0;
        socklen_t el = sizeof(e);
        int w = wait_sock(n, 1, 30000);
        if (w <= 0) {
            if (!w)
                set_err(n, "no answer from ", host);
            drop(n);
            return w ? w : NET_ERROR;
        }
        getsockopt(n->sock, SOL_SOCKET, SO_ERROR, (APTR)&e, &el);
        rc = e ? -1 : 0;
    }
    if (rc < 0) {
        set_err(n, "cannot connect to ", host);
        drop(n);
        return NET_ERROR;
    }
    if (!tls)
        return 0;
    if (!tls_ready) {
        if (tls_init(SocketBase, n->err, sizeof(n->err))) {
            drop(n);
            return NET_ERROR;
        }
        tls_ready = 1;
    }
    n->tls = tls_new(n->sock, host, n->err, sizeof(n->err));
    if (!n->tls) {
        drop(n);
        return NET_ERROR;
    }
    for (;;) {
        int r = tls_handshake((cl_tls *)n->tls, n->err, sizeof(n->err)), w;
        if (!r)
            return 0;
        if (r == TLS_ERROR) {
            drop(n);
            return NET_ERROR;
        }
        w = wait_sock(n, r == TLS_WANT_WRITE, 30000);
        if (w <= 0) {
            if (!w)
                set_err(n, "the TLS handshake timed out", 0);
            drop(n);
            return w ? w : NET_ERROR;
        }
    }
}

static long a_send(void *u, const char *b, long len)
{
    net_amiga *n = (net_amiga *)u;
    long done = 0;
    while (done < len) {
        long r;
        int w;
        if (n->tls) {
            r = tls_send((cl_tls *)n->tls, b + done, len - done);
            if (r > 0) {
                done += r;
                continue;
            }
            if (r != TLS_WANT_READ && r != TLS_WANT_WRITE) {
                set_err(n, "sending failed (TLS)", 0);
                return NET_ERROR;
            }
            w = wait_sock(n, r == TLS_WANT_WRITE, 60000);
        } else {
            r = send(n->sock, (APTR)(b + done), len - done, 0);
            if (r > 0) {
                done += r;
                continue;
            }
            if (r < 0 && Errno() != EWOULDBLOCK && Errno() != EINTR) {
                set_err(n, "sending failed", 0);
                return NET_ERROR;
            }
            w = wait_sock(n, 1, 60000);
        }
        if (w < 0)
            return w;
        if (!w) {
            set_err(n, "sending timed out", 0);
            return NET_ERROR;
        }
    }
    return len;
}

static long a_recv(void *u, char *b, long cap, int timeout_ms)
{
    net_amiga *n = (net_amiga *)u;
    for (;;) {
        long r;
        int w, wr = 0;
        if (n->tls && tls_pending((cl_tls *)n->tls) > 0)
            return tls_recv((cl_tls *)n->tls, b, cap);
        w = wait_sock(n, 0, timeout_ms);
        if (w < 0)
            return w;
        if (!w)
            return NET_TIMEOUT;
        if (n->tls) {
            r = tls_recv((cl_tls *)n->tls, b, cap);
            if (r >= 0)
                return r;
            if (r == TLS_WANT_WRITE)
                wr = 1;
            else if (r != TLS_WANT_READ) {
                set_err(n, "receiving failed (TLS)", 0);
                return NET_ERROR;
            }
            if (wr) {
                w = wait_sock(n, 1, timeout_ms);
                if (w < 0)
                    return w;
            }
            continue;
        }
        r = recv(n->sock, (APTR)b, cap, 0);
        if (r >= 0)
            return r;
        if (Errno() != EWOULDBLOCK && Errno() != EINTR) {
            set_err(n, "receiving failed", 0);
            return NET_ERROR;
        }
    }
}

static void a_close(void *u)
{
    drop((net_amiga *)u);
}

static const char *a_err(void *u)
{
    return ((net_amiga *)u)->err;
}

void net_amiga_init(net_amiga *n, cl_net *net)
{
    memset(n, 0, sizeof(*n));
    n->sock = -1;
    net->u = n;
    net->open = a_open;
    net->send = a_send;
    net->recv = a_recv;
    net->close = a_close;
    net->err = a_err;
}

void net_amiga_exit(net_amiga *n)
{
    drop(n);
    if (tls_ready)
        tls_exit();
    tls_ready = 0;
    if (SocketBase)
        CloseLibrary(SocketBase);
    SocketBase = 0;
}
