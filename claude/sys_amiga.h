/* sys_amiga -- sys.h on AmigaDOS (see sys_amiga.c). */
#ifndef CL_SYS_AMIGA_H
#define CL_SYS_AMIGA_H

#include "sys.h"

typedef struct sys_amiga {
    char err[200];
} sys_amiga;

void sys_amiga_init(sys_amiga *s, cl_sys *sys);

#endif
