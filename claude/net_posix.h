/* net_posix -- net.h over POSIX TCP (host tests only; see net_posix.c). */
#ifndef CL_NET_POSIX_H
#define CL_NET_POSIX_H

#include "net.h"

typedef struct net_posix {
    int fd;
    char err[160];
} net_posix;

void net_posix_init(net_posix *p, cl_net *n);

#endif
