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
/* closes the connection and the libraries (at the program's end) */
void net_amiga_exit(net_amiga *n);

#endif
