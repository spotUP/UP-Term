/* The OS calls shared by win.c and tile.c (win_apply.h). */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/intuition.h>
#include "win_apply.h"

void win_box_of(const struct Window *w, win_box *b, win_screen *s)
{
    b->left = w->LeftEdge;
    b->top = w->TopEdge;
    b->width = w->Width;
    b->height = w->Height;
    b->minw = w->MinWidth;
    b->minh = w->MinHeight;
    b->maxw = w->MaxWidth == 0xFFFF ? 0 : w->MaxWidth;
    b->maxh = w->MaxHeight == 0xFFFF ? 0 : w->MaxHeight;
    b->screen = (long)w->WScreen;
    s->width = w->WScreen->Width;
    s->height = w->WScreen->Height;
    s->title = w->WScreen->BarHeight + 1;
}

void win_apply_box(struct Window *w, const win_box *b)
{
    ChangeWindowBox(w, b->left, b->top, b->width, b->height);
}

void win_raise(struct Window *w)
{
    WindowToFront(w);
}
