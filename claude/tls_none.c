/* tls_none -- tls.h for a build without the AmiSSL SDK: every https
 * connection is refused with a message that says so. */
#include "tls.h"
#include "util.h"
#include "../tty/bmsg.h"

/* the same message as a missing library: from the user's side it is the same
 * fix (the kit's Claude is built with AmiSSL; make amiga AMISSL_SDK=<dir>) */
#define why bmsg_amissl_missing()

int tls_init(void *socketbase, char *err, int cap)
{
    (void)socketbase;
    cl_copy(err, why, cap);
    return TLS_ERROR;
}

void tls_exit(void)
{
}

cl_tls *tls_new(long sock, const char *host, char *err, int cap)
{
    (void)sock;
    (void)host;
    cl_copy(err, why, cap);
    return 0;
}

int tls_handshake(cl_tls *t, char *err, int cap)
{
    (void)t;
    cl_copy(err, why, cap);
    return TLS_ERROR;
}

long tls_send(cl_tls *t, const char *b, long n)
{
    (void)t;
    (void)b;
    (void)n;
    return TLS_ERROR;
}

long tls_recv(cl_tls *t, char *b, long cap)
{
    (void)t;
    (void)b;
    (void)cap;
    return TLS_ERROR;
}

int tls_pending(cl_tls *t)
{
    (void)t;
    return 0;
}

void tls_free(cl_tls *t)
{
    (void)t;
}
