/* A recorded stream (tests/claude/NAME.sse) read whole, for the Claude
 * client's suites. */
#ifndef CLAUDE_LOAD_H
#define CLAUDE_LOAD_H

/* malloc'ed, terminated; 0 when the file is missing. *n its length. */
char *claude_load(const char *name, long *n);

/* the same for a file that may be absent (a recording's .headers): a miss is
 * not an error and prints nothing. */
char *claude_load_opt(const char *name, long *n);

/* how many claude_load calls found no file (each printed [ERROR]) */
extern int claude_load_errors;

/* got against tests/claude/<name> (CL_UPDATE_GOLDEN=1 writes the file
 * first): 1 equal; a difference is reported as a failed check */
int claude_golden(const char *name, const char *got);

#endif
