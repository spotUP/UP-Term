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
 * each parent: 1, 0, -1 none (no file, not an object); the directory it came
 * from into where (0 not wanted). Keys compare without case (FFS). */
int trust_near(cl_sys *sys, const char *file, const char *dir, char *where, long cap);
/* path's decision set (1, 0) or removed (-1), the file written as pi writes it:
 * keys sorted, two-space JSON, a final newline. 0, -1 (also: a file that is
 * there but not a JSON object is left alone) */
int trust_put(cl_sys *sys, const char *file, const char *path, int value);

#endif
