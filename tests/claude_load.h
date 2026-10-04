/* A recorded stream (tests/claude/NAME.sse) read whole, for the Claude
 * client's suites. */
#ifndef CLAUDE_LOAD_H
#define CLAUDE_LOAD_H

/* malloc'ed, terminated; 0 when the file is missing. *n its length. */
char *claude_load(const char *name, long *n);

#endif
