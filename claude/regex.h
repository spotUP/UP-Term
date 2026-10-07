/* regex -- the regular expressions of C:Claude's Grep tool (ledger A4 WP2).
 *
 * The syntax is what Claude writes for ripgrep, as far as a POSIX ERE
 * engine plus Perl's common escapes go: literals, '.', [...] and [^...]
 * with ranges and [:alpha:]-style classes, \d \w \s \D \W \S, \b \B,
 * ^ and $ (line anchors), ( ), (?: ), |, * + ? {m} {m,} {m,n} (a trailing
 * '?' -- the lazy forms -- is accepted and matches the same text first),
 * \t \n \r \f \v and an escaped punctuation character. No back references,
 * no look-around: they are refused with a reason, never guessed.
 *
 * Matching runs over code points (UTF-8 where the bytes are UTF-8, else
 * each byte is a Latin-1 character -- an Amiga file's own text) in a Pike
 * VM: every alternative advances in step, so the time is linear in the
 * text for any pattern -- no backtracking blow-up on a 68k. Leftmost-first
 * match semantics, as Perl and ripgrep.
 * Portable C89, host-tested (tests/test_claude_match.c). */
#ifndef CL_REGEX_H
#define CL_REGEX_H

#define RE_ICASE  1             /* case-insensitive (ASCII and Latin-1 letters) */
#define RE_DOTALL 2             /* '.' also matches a newline (Grep's multiline) */
#define RE_POSIX  4             /* POSIX ERE as bash's [[ =~ ]] reads it: a backslash makes the next character
                                 * itself (no \d \w \s \b), ^ and $ only at the ends of the text, '.' matches
                                 * a newline, no (?: ) and no lazy forms; the match is leftmost-longest */

typedef struct cl_re cl_re;

/* 0 with the reason in err when the pattern is not one */
cl_re *re_compile(const char *pat, int flags, char *err, long cap);
void re_free(cl_re *re);
/* The first match in s[0..n) at or after from: 1 with [*ms, *me), 0 none,
 * -1 out of memory. Anchors see the whole of s (^ after any '\n'). */
int re_search(const cl_re *re, const char *s, long n, long from, long *ms, long *me);

/* The same search with capture groups: caps gets ncap pairs (start, end) of byte offsets, pair 0 the whole
 * match, -1 for a group that did not take part. Returns 1, 0 or -1 (out of memory). */
int re_search_groups(const cl_re *re, const char *s, long n, long from, long *caps, int ncap);
/* the number of ( ) groups of the pattern, the whole match not counted */
int re_ngroups(const cl_re *re);

#endif
