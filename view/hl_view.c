/* hl_view -- one line of hl's output (hl_view.h). */
#include <string.h>
#include "hl_view.h"

void hl_view_begin(hl_view *v, vw_out *out, const hl_lang *lang, int numbers, int tabs)
{
    v->out = out;
    hl_begin(&v->st, lang);
    v->numbers = numbers;
    v->tabs = tabs;
    v->line = 0;
    v->col = 0;
    v->tab_end = 0;
    memset(v->num, ' ', sizeof(v->num));
}

static void token(void *u, int cls, const char *s, long n)
{
    hl_view *v = (hl_view *)u;
    vw_out *o = v->out;
    const char *t = 0;
    long k;
    vo_class(o, cls);
    if (v->tabs)
        for (k = 0; k < n; k++)
            if (s[k] == '\t') {
                t = s + k;
                break;
            }
    if (!t) {
        vo_text(o, s, n);
        return;
    }
    /* a tab: the column is measured from the last tab's end only now (the
     * tokens of a line are consecutive bytes of it) */
    while (t) {
        int to;
        v->col += vw_width(v->tab_end, (long)(t - v->tab_end));
        to = v->tabs - v->col % v->tabs;
        vo_text(o, s, (long)(t - s));
        vo_spaces(o, to);
        v->col += to;
        n -= (long)(t - s) + 1;
        s = t + 1;
        v->tab_end = s;
        t = (const char *)memchr(s, '\t', n);
    }
    vo_text(o, s, n);
}

static void gutter(hl_view *v)
{
    /* the number counts up in decimal text: no division per line (a
     * 68020's divu.l is ~44 cycles, ten of them a line) */
    char *d = v->num + sizeof(v->num) - 1;
    int k;
    for (;;) {
        if (*d == ' ')
            *d = '0';
        if (*d < '9') {
            (*d)++;
            break;
        }
        *d = '0';
        if (d == v->num)
            break;
        d--;
    }
    for (k = 0; k < (int)sizeof(v->num) - 5 && v->num[k] == ' '; k++)
        ;
    vo_class(v->out, HL_LINENO);
    vo_raw(v->out, v->num + k, (long)sizeof(v->num) - k);
    if (v->out->cs == VW_UTF8)
        vo_raw(v->out, " \342\224\202 ", 5); /* U+2502 */
    else
        vo_raw(v->out, " | ", 3);
}

void hl_view_line(hl_view *v, const char *s, long n, int nl)
{
    v->line++;
    v->col = 0;
    v->tab_end = s;
    if (n > 0 && s[n - 1] == '\r')
        n--;
    if (v->numbers)
        gutter(v);
    hl_line(&v->st, s, n, token, v);
    vo_reset(v->out);
    if (nl)
        vo_raw(v->out, "\n", 1);
}
