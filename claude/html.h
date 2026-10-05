/* html -- a web page as Markdown for WebFetch (ledger A4 WP2): headings,
 * paragraphs, lists, links, emphasis, code and preformatted blocks, table
 * cells; script, style, head, svg and comments dropped; entities decoded
 * (named ones that matter, &#NNN; and &#xHH;); white space collapsed
 * outside <pre>. One pass over the bytes, no tree: a malformed page still
 * gives its text. Portable C89, host-tested (tests/test_claude_match.c). */
#ifndef CL_HTML_H
#define CL_HTML_H

#include "json.h"

/* s[0..n) as Markdown appended to out, cut at max bytes (0: no cut) */
void html_to_md(const char *s, long n, jw *out, long max);

#endif
