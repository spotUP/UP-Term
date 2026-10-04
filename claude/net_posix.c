/* net_posix -- net.h over plain POSIX TCP sockets: the host build's
 * transport, used by the tests against a local fixture server. No TLS:
 * an https URL is refused (the host never talks to the real API). */
#define _POSIX_C_SOURCE 200112L
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "net_posix.h"
#include "util.h"

static int p_open(void *u, const char *host, int port, int tls)
{
    net_posix *p = (net_posix *)u;
    struct addrinfo hints, *res = 0, *a;
    char ps[16];
    int rc;
    p->err[0] = 0;
    if (tls) {
        strcpy(p->err, "TLS is not available in the host transport");
        return NET_ERROR;
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    cl_ltoa(port, ps);
    rc = getaddrinfo(host, ps, &hints, &res);
    if (rc) {
        cl_cat(cl_copy(p->err, "cannot resolve ", sizeof(p->err)), host, sizeof(p->err));
        return NET_ERROR;
    }
    p->fd = -1;
    for (a = res; a; a = a->ai_next) {
        int fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0)
            continue;
        if (!connect(fd, a->ai_addr, a->ai_addrlen)) {
            p->fd = fd;
            break;
        }
        close(fd);
    }
    freeaddrinfo(res);
    if (p->fd < 0) {
        cl_cat(cl_copy(p->err, "cannot connect to ", sizeof(p->err)), host, sizeof(p->err));
        return NET_ERROR;
    }
    return 0;
}

static long p_send(void *u, const char *b, long n)
{
    net_posix *p = (net_posix *)u;
    long done = 0;
    while (done < n) {
        long w = (long)write(p->fd, b + done, (size_t)(n - done));
        if (w <= 0) {
            if (w < 0 && errno == EINTR)
                continue;
            strcpy(p->err, "send failed");
            return NET_ERROR;
        }
        done += w;
    }
    return n;
}

static long p_recv(void *u, char *b, long cap, int timeout_ms)
{
    net_posix *p = (net_posix *)u;
    struct pollfd pf;
    long r;
    pf.fd = p->fd;
    pf.events = POLLIN;
    pf.revents = 0;
    r = poll(&pf, 1, timeout_ms);
    if (r == 0)
        return NET_TIMEOUT;
    if (r < 0) {
        strcpy(p->err, "poll failed");
        return NET_ERROR;
    }
    r = (long)read(p->fd, b, (size_t)cap);
    if (r < 0) {
        strcpy(p->err, "receive failed");
        return NET_ERROR;
    }
    return r;
}

static void p_close(void *u)
{
    net_posix *p = (net_posix *)u;
    if (p->fd >= 0)
        close(p->fd);
    p->fd = -1;
}

static const char *p_err(void *u)
{
    return ((net_posix *)u)->err;
}

void net_posix_init(net_posix *p, cl_net *n)
{
    memset(p, 0, sizeof(*p));
    p->fd = -1;
    n->u = p;
    n->open = p_open;
    n->send = p_send;
    n->recv = p_recv;
    n->close = p_close;
    n->err = p_err;
}
