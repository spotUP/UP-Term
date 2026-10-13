/* net_amiga -- net.h on bsdsocket.library + tls.h (see net_amiga.c). */
#ifndef CL_NET_AMIGA_H
#define CL_NET_AMIGA_H

#include "net.h"

typedef struct net_amiga {
    long sock;
    void *tls;                  /* cl_tls *, 0 for a plain connection */
    char err[200];
} net_amiga;

void net_amiga_init(net_amiga *n, cl_net *net);
/* the IPv4 address host resolves to (amiga-pi's LAN rule): 0, -1 */
int net_amiga_resolve(const char *host, unsigned char ip[4]);
/* n secure random bytes from the TLS library (opening it when no connection has yet): 0, -1 without TLS in this build */
int net_amiga_random(unsigned char *buf, long n);
/* closes the connection and the libraries (at the program's end) */
void net_amiga_exit(net_amiga *n);

#endif
