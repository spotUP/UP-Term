/* datadir -- where C:Claude and amiga-pi keep what grows (sessions,
 * history, agent memory, skills): one rule for both programs.
 *
 * ENVARC: is for small configuration only. Workbench 3.1's Startup-Sequence
 * copies all of ENVARC: into RAM:ENV at every boot, so a session file kept
 * there is loaded into RAM on each boot (3.2 links ENV: to ENVARC: instead).
 * The growing files go to the kit's own drawer, UP-Term:var/<program>
 * (UP-Term: is the Installer's assign; bin/, share/, etc/ are beside var/).
 * Without the kit (no UP-Term: assign) they stay in ENVARC:<program>, as
 * before. A file still in the old place is moved on the program's first
 * run (dd_migrate).
 *
 * Portable C89 over cl_sys, host-tested (tests/test_claude_config.c). */
#ifndef CL_DATADIR_H
#define CL_DATADIR_H

#include "sys.h"

#define DD_KIT   "UP-Term:"          /* the kit's assign: present when the kit is installed */
#define DD_DISK  "UP-Term:var"       /* the growing files' drawer, one per program inside */
#define DD_CONF  "ENVARC:"           /* the small configuration, ENVARC:<program> */

/* the data directory of program prog ("Claude", "amiga-pi") into out:
 * 1 on disk (UP-Term:var/<prog>), 0 the fallback ENVARC:<prog>, -1 too long */
int dd_dir(cl_sys *sys, const char *prog, char *out, long cap);

/* every directory of path made, from its volume down: 0, -1 */
int dd_mkdirs(cl_sys *sys, const char *path);

/* from (a file or a directory tree) moved to to when from exists and to
 * does not: rename, else copied and then deleted (another volume).
 * 1 moved; 0 nothing to move; 2 both exist (nothing touched: to is the one
 * used); -1 failed (from left as it was, a partial copy removed) */
int dd_migrate(cl_sys *sys, const char *from, const char *to);

/* path and everything below it deleted: the number of files and
 * directories removed */
long dd_remove_tree(cl_sys *sys, const char *path);

#endif
