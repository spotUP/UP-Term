/* util -- small string helpers shared by the Claude client's modules
 * (C89 has no snprintf; the host's sprintf is deprecated). */
#ifndef CL_UTIL_H
#define CL_UTIL_H

/* v in decimal into out (at least 12 bytes): the length. */
int cl_ltoa(long v, char *out);
/* src into dst of cap bytes, cut to fit, always terminated: dst. */
char *cl_copy(char *dst, const char *src, long cap);
/* src appended to dst of cap bytes, cut to fit: dst. */
char *cl_cat(char *dst, const char *src, long cap);
/* case-insensitive (ASCII) compares */
int cl_strieq(const char *a, const char *b);
int cl_strnieq(const char *a, const char *b, long n);
/* argv as one AmigaDOS Shell command line in line of cap bytes: the program (in quotes only when it holds a
 * space, tab, quote, * or one of ; < > | & ` $ { } or is empty), then each argument quoted with "*" escapes
 * (*" ** *N). 0, or -1 when it does not fit (line then holds what fitted; nothing is cut silently). */
int cl_shell_line(char *line, long cap, const char *const *argv);
/* a random (version 4) UUID into out (37 bytes) from the generator *seed
 * (an LCG the caller seeds from its clock), *seed advanced */
void cl_uuid(unsigned long *seed, char *out);
/* The Shell runs a command through RunCommand, which puts a copy of the argument line into the program's Input() handle
 * buffer (for ReadArgs). A program that takes its arguments from registers leaves the copy unread and dos.library counts
 * it as read-ahead of the file, so a redirected input starts with "arg arg" instead of the file's first line. Given the
 * handle's buffer (buf, the next unread position pos, the end of the buffered bytes) and GetArgStr()'s text: the number
 * of bytes to read out (the argument line's length) when the buffer holds exactly that line, else 0 (a handle with real
 * read-ahead, a program started any other way, is left alone). Same decision as ixemul's consume_runcommand_args
 * (library/_cli_parse.c). */
long cl_runcommand_args(const char *buf, long pos, long end, const char *args);

#endif
