/* hl_view -- hl's output for one line: the gutter (line number), the
 * tokens in their theme's colours, tabs expanded when a gutter moves the
 * tab stops. Portable, host-tested; hl_main.c is the command around it. */
#ifndef HL_VIEW_H
#define HL_VIEW_H

#include "hl_lex.h"
#include "vw_text.h"

typedef struct hl_view {
    vw_out *out;
    hl_state st;
    int numbers;            /* the gutter */
    int tabs;               /* expand tabs to this width, 0 keeps them */
    long line;              /* the line's number, from 1 */
    char num[10];           /* the same in decimal, right-aligned */
    int col;                /* the column at tab_end */
    const char *tab_end;    /* where the last tab ended (the line's start) */
} hl_view;

void hl_view_begin(hl_view *v, vw_out *out, const hl_lang *lang, int numbers, int tabs);
/* one line without its newline; nl: end it with one */
void hl_view_line(hl_view *v, const char *s, long n, int nl);

#endif
