/* A recorded stream (tests/claude/NAME.sse) read whole, for the Claude
 * client's suites. */
#ifndef CLAUDE_LOAD_H
#define CLAUDE_LOAD_H

/* malloc'ed, terminated; 0 when the file is missing. *n its length. */
char *claude_load(const char *name, long *n);

/* got against tests/claude/<name> (CL_UPDATE_GOLDEN=1 writes the file
 * first): 1 equal; a difference is reported as a failed check */
int claude_golden(const char *name, const char *got);

#endif
