#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <string.h>
#include "bar_usable.h"
#include "bar_core.h"

int bar_usable(const struct Screen *scr, win_screen *s, win_box *box)
{
    const struct Window *x;
    s->width = scr->Width;
    s->height = scr->Height;
    s->title = scr->BarHeight + 1;
    s->bottom = 0;                                 /* the bar's strut, only from a bar on this screen (D8) */
    for (x = scr->FirstWindow; x; x = x->NextWindow)
        if (x->Title && !strcmp((const char *)x->Title, BAR_TITLE)) {
            win_box bar;
            bar.left = x->LeftEdge;
            bar.top = x->TopEdge;
            bar.width = x->Width;
            bar.height = x->Height;
            s->bottom = win_strut(&bar, s);
            break;
        }
    if (box)
        win_usable(s, box);
    return s->bottom;
}
