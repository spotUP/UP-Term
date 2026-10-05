/* glob -- file name patterns for C:Claude's Glob and Grep tools (ledger
 * A4 WP2): '*' (any run within one name), '**' (any number of whole
 * directories, alone or after "dir/"), '?' (one character), [abc] [a-z]
 * [!x] [^x], {one,two} (alternatives, which may hold wildcards). Matching
 * ignores case, as AmigaDOS names do. AmigaDOS's own patterns are
 * understood too: #? is '*', (a|b) is {a,b}, ? is '?'.
 * Portable C89, host-tested (tests/test_claude_match.c). */
#ifndef CL_GLOB_H
#define CL_GLOB_H

/* Does path ('/'-separated, relative to the search's root) match pat? */
int glob_match(const char *pat, const char *path);
/* An AmigaDOS pattern written as a glob into out: 0, or -1 with the
 * reason in out when it uses what a glob cannot say (~, #x other than #?). */
int glob_from_amiga(const char *in, char *out, long cap);
/* Does the pattern hold a wildcard at all? */
int glob_wild(const char *pat);
/* The number of directory levels the pattern can reach below the root:
 * -1 any ("**"). */
int glob_depth(const char *pat);

#endif
