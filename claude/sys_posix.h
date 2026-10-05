/* sys_posix -- sys.h on POSIX (host tests only; see sys_posix.c). */
#ifndef CL_SYS_POSIX_H
#define CL_SYS_POSIX_H

#include "sys.h"

#define SP_JOBS 16

typedef struct sp_job {
    int used, ended;
    long pid, rc;
    char file[300];
} sp_job;

typedef struct sys_posix {
    char err[200];
    sp_job jobs[SP_JOBS];
    char clip[4096];            /* what clip() was given (the tests read it) */
    long clipn;
} sys_posix;

void sys_posix_init(sys_posix *p, cl_sys *s);

#endif
