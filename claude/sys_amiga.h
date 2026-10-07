/* sys_amiga -- sys.h on AmigaDOS (see sys_amiga.c). */
#ifndef CL_SYS_AMIGA_H
#define CL_SYS_AMIGA_H

#include "sys.h"

#define SA_JOBS 16

/* a background command (Bash run_in_background) */
typedef struct sa_job {
    int used, ended;
    void *job, *port;           /* the runner's message and reply port while it runs */
    long rc;
    char script[48], output[48];
} sa_job;

typedef struct sys_amiga {
    char err[200];
    const char *vsh;            /* the vsh to run (UP-Term:bin/vsh or C:vsh), 0 without */
    int vsh_known;
    sa_job jobs[SA_JOBS];
} sys_amiga;

void sys_amiga_init(sys_amiga *s, cl_sys *sys);

#endif
