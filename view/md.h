/* md -- Markdown rendered for a terminal, the way glow and mdcat show it:
 * headings, emphasis, code spans, fenced code highlighted by hl_lex, block
 * quotes, nested lists, tables fitted to the width with box drawing, rules,
 * links (OSC 8 and the URL shown), images as their alt text, everything
 * word-wrapped to the width. CommonMark-ish: block structure by container
 * matching, emphasis by delimiter runs, reference links, GFM tables, task
 * lists, strikethrough and bare URLs. Portable C89, host-tested. */
#ifndef MD_H
#define MD_H

#include "vw_text.h"

typedef struct md_opts {
    int width;              /* the columns to fill */
    int urls;               /* show a link's URL after its text */
} md_opts;

/* doc[0..n) to out (its theme, depth, character set and osc8 decide the
 * look). 0, or -1 when memory ran out. */
int md_render(const char *doc, long n, const md_opts *opt, vw_out *out);

/* The same as a stream, for text that arrives in pieces (C:Claude's
 * answers): md_feed takes any split of the document, a block is drawn as
 * soon as a later line closes it, md_close draws what is still open and
 * frees the stream. opt is copied; out must live until md_close. Reference
 * definitions resolve only for links after them (no whole-document pass
 * on a stream). md_feed, md_close: 0, or -1 when memory ran out (md_close
 * frees everything all the same). md_open: 0 when out of memory. */
typedef struct md md;
md *md_open(const md_opts *opt, vw_out *out);
int md_feed(md *m, const char *s, long n);
/* is anything held back (a line not ended, a block not closed)? */
int md_pending(const md *m);
int md_close(md *m);

/* A table's column widths: the natural ones when they fit in room
 * columns, else the widest are capped (the largest cap that fits) and what
 * is left goes to the capped ones. Every width at least 1. */
void md_fit_columns(const int *natural, int ncols, int room, int *width);

#endif
