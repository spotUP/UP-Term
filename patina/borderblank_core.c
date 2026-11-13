/* See borderblank_core.h. */
#include "borderblank_core.h"

int bb_native_mode(unsigned long modeid)
{
    if (modeid == 0xFFFFFFFFUL)
        return 0;
    return (modeid & 0xFFFF1000UL) <= 0x000A1000UL;
}

void bb_init(bb_table *t)
{
    t->n = 0;
}

int bb_has(const bb_table *t, const void *scr)
{
    int i;
    for (i = 0; i < t->n; i++)
        if (t->scr[i] == scr)
            return 1;
    return 0;
}

int bb_add(bb_table *t, const void *scr)
{
    if (!scr || bb_has(t, scr))
        return 0;
    if (t->n >= BB_MAX)
        return -1;
    t->scr[t->n++] = scr;
    return 1;
}

int bb_drop(bb_table *t, const void *scr)
{
    int i;
    for (i = 0; i < t->n; i++)
        if (t->scr[i] == scr) {
            t->scr[i] = t->scr[--t->n];
            return 1;
        }
    return 0;
}
