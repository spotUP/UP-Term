/* trust -- per-project state kept in <home>/claude.json (Claude Code's
 * ~/.claude.json "projects"): workspace trust, the external-imports
 * approval of CLAUDE.md (A4 gaps 2). See trust.c. */
#ifndef CL_TRUST_H
#define CL_TRUST_H

#include "sys.h"

#define TRUST_ACCEPTED "hasTrustDialogAccepted"
#define TRUST_IMPORTS "hasClaudeMdExternalIncludesApproved"
#define TRUST_IMPORTS_ASKED "hasClaudeMdExternalIncludesWarningShown"

/* a project's boolean: 1 true, 0 false, -1 not kept; inherit: a directory
 * above dir counts too (workspace trust) */
int trust_get(cl_sys *sys, const char *home, const char *dir, const char *key, int inherit);
/* ... set for dir (the file's other keys kept): 0, -1 */
int trust_set(cl_sys *sys, const char *home, const char *dir, const char *key, int value);

/* pi's trust.json (amiga-pi, pi f10993bc core/trust-manager.ts): file holds
 * {"<canonical dir>": true | false}. The nearest decision for dir -- dir, then
 * each parent: 1, 0, -1 none (no file), -2 the file is there but pi would refuse it
 * (trust_check says why); the directory it came from, as dir spells it, into where
 * (0 not wanted). Paths are the Amiga's Latin-1 (or UTF-8): they are compared in the
 * UTF-8 the keys are stored in, ASCII letters without case (FFS). */
int trust_near(cl_sys *sys, const char *file, const char *dir, char *where, long cap);
/* path's decision set (1, 0) or removed (-1), the file written as pi writes it:
 * keys sorted, two-space JSON, a final newline. 0, -1 (also: a file that pi
 * would refuse is left alone, never rewritten) */
int trust_put(cl_sys *sys, const char *file, const char *path, int value);
/* pi's readTrustFile checks: 0 (also no file), -1 when pi would stop with an error
 * (not JSON, not an object, a value other than true, false, null, unreadable); pi's
 * words for it into err (0 not wanted) */
int trust_check(cl_sys *sys, const char *file, char *err, long cap);

#endif
