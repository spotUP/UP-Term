/* prefs_dos: the profile file on AmigaDOS -- prefs_core's file callbacks
 * and the save into ENV: and ENVARC:. Shared by UP-Term Prefs and the
 * handler's Settings > Save settings to profile (run in a worker: DOS
 * calls). */
#ifndef PREFS_DOS_H
#define PREFS_DOS_H
#include "prefs_core.h"

enum { PREFS_T_ENV, PREFS_T_ENVARC };
extern const char *const prefs_conf_dir[2];   /* ENV:up-term, ENVARC:up-term */
extern const char *const prefs_conf_path[2];  /* .../up-term */
extern const prefs_fs prefs_dos_fs;

/* Make the profile drawer t when it is missing; 1 when it is there. */
int prefs_dos_dir(int t);

/* buf (len bytes) into ENV:'s file, and ENVARC:'s too when keep, each by
 * prefs_install (the old file kept aside until the new one is in place).
 * PREFS_INSTALL_OK, PREFS_DOS_NODIR (a drawer could not be made) or
 * another prefs_install result; *failed is the target that failed. */
#define PREFS_DOS_NODIR (-100)
int prefs_dos_save(const char *buf, long len, int keep, int *failed);

#endif
