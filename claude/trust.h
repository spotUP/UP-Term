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

#endif
