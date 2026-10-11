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
/* a random (version 4) UUID into out (37 bytes) from the generator *seed
 * (an LCG the caller seeds from its clock), *seed advanced */
void cl_uuid(unsigned long *seed, char *out);

#endif
