/* Amiga volumes for the Claude suites on the host: a cl_sys that maps
 * "ENVARC:" and "UP-Term:" (claude/datadir.h) to host directories and
 * hands everything else to the inner one (sys_posix). With no UP-Term:
 * directory the kit is "not installed"; no_rename makes rename fail, as
 * across two Amiga volumes. */
#ifndef CLAUDE_VOL_H
#define CLAUDE_VOL_H

#include "../claude/sys.h"

typedef struct cl_vol {
    cl_sys inner;
    char envarc[400];           /* the host directory ENVARC: stands for */
    char kit[400];              /* the host directory UP-Term: stands for ("" none) */
    int no_rename;
    long renames;               /* renames that went through (a moved tree counts once) */
    long quiet_kinds;           /* quiet_kind calls (datadir asks without a requester) */
} cl_vol;

/* out: inner with every path-taking call mapped; v must outlive out */
void vol_init(cl_vol *v, const cl_sys *inner, cl_sys *out);

#endif
