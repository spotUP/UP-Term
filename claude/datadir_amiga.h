/* datadir_amiga -- the AmigaDOS file calls of cl_sys that claude/datadir.c needs (read, write, list, kind, the
 * kind that asks without a volume requester, mkdir, remove, rename): one unit linked by C:Claude and amiga-pi
 * (sys_amiga.c takes its file calls from here) and by the console handler (its history file,
 * handler/histfile.c), which needs nothing else of sys_amiga. */
#ifndef CL_DATADIR_AMIGA_H
#define CL_DATADIR_AMIGA_H

#include <dos/dos.h>
#include "sys.h"

/* The state of the file calls: the last error's text. A cl_sys's u points at one; sys_amiga has it as its first
 * member, so the same u serves both. */
typedef struct dd_amiga {
    char err[200];
} dd_amiga;

/* s->err = what, then the DOS error's text (IoErr) */
void dda_err(dd_amiga *s, const char *what);
/* a DateStamp in seconds since 1978 */
long dda_seconds(const struct DateStamp *d);

int dda_read(void *u, const char *path, long max, char **out, long *n);
int dda_pread(void *u, const char *path, long off, char *out, long cap, long *n);
int dda_write(void *u, const char *path, const char *str, long n);
int dda_list(void *u, const char *path, cl_dir_fn fn, void *c);
int dda_kind(void *u, const char *path);
/* kind() with the volume requesters off: a missing assign (UP-Term: before the kit) is an answer */
int dda_quiet_kind(void *u, const char *path);
int dda_mkdir(void *u, const char *path);
int dda_remove(void *u, const char *path);
int dda_rename(void *u, const char *from, const char *to);

/* sys with u = s and the calls above (the others left as they were): what datadir.c runs on */
void dda_init(dd_amiga *s, cl_sys *sys);

#endif
