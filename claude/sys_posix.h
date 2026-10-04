/* sys_posix -- sys.h on POSIX (host tests only; see sys_posix.c). */
#ifndef CL_SYS_POSIX_H
#define CL_SYS_POSIX_H

#include "sys.h"

typedef struct sys_posix {
    char err[200];
} sys_posix;

void sys_posix_init(sys_posix *p, cl_sys *s);

#endif
