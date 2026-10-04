/* sbar: the scroll bar's knob arithmetic; see sbar.h. */
#include "sbar.h"

#define SBAR_MAX 0xFFFFUL /* propgclass keeps PGA_* in 16 bits */

void sbar_from_view(sbar_knob *k, long sb, int rows, int view, int alt)
{
    unsigned long total, vis, top, first;
    unsigned char s = 0;
    if (rows < 1)
        rows = 1;
    if (alt || sb <= 0) {
        /* nothing above the screen: the knob is the whole track */
        vis = (unsigned long)rows > SBAR_MAX ? SBAR_MAX : (unsigned long)rows;
        k->total = k->visible = (unsigned short)vis;
        k->top = 0;
        k->shift = 0;
        k->live = 0;
        return;
    }
    if (view < 0)
        view = 0;
    if (view > sb)
        view = (int)sb;
    total = (unsigned long)sb + (unsigned long)rows;
    while ((total >> s) > SBAR_MAX)
        s++;
    vis = (unsigned long)rows >> s;
    if (!vis)
        vis = 1;
    total >>= s;
    first = (unsigned long)(sb - view); /* the first line shown, from the oldest */
    /* the live screen is the bottom exactly, whatever the rounding */
    top = view ? first >> s : total - vis;
    if (top > total - vis)
        top = total - vis;
    k->total = (unsigned short)total;
    k->visible = (unsigned short)vis;
    k->top = (unsigned short)top;
    k->shift = s;
    k->live = 1;
}

int sbar_to_view(const sbar_knob *k, long sb, unsigned long top)
{
    long view;
    if (!k->live || sb <= 0)
        return 0;
    if (top + k->visible >= k->total)
        return 0; /* the knob at the bottom: the live screen */
    view = sb - (long)(top << k->shift);
    if (view < 0)
        view = 0;
    if (view > sb)
        view = sb;
    return (int)view;
}

int sbar_equal(const sbar_knob *a, const sbar_knob *b)
{
    return a->total == b->total && a->visible == b->visible && a->top == b->top &&
           a->shift == b->shift && a->live == b->live;
}
