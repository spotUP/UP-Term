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

/* A table's column widths: the natural ones when they fit in room
 * columns, else the widest are capped (the largest cap that fits) and what
 * is left goes to the capped ones. Every width at least 1. */
void md_fit_columns(const int *natural, int ncols, int room, int *width);

#endif
