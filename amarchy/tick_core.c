#include <stddef.h>
#include "tick_core.h"

void tick_hooks_init(tick_hooks *h)
{
    h->n = 0;
    h->ticks = 0;
}

int tick_hooks_add(tick_hooks *h, tick_fn fn)
{
    int i;
    if (!fn || h->n >= TICK_MAX_HOOKS)
        return 0;
    for (i = 0; i < h->n; i++)
        if (h->fn[i] == fn)
            return 0;
    h->fn[h->n++] = fn;
    return 1;
}

void tick_hooks_run(tick_hooks *h, long now)
{
    int i, n = h->n;           /* a hook that adds a hook runs it from the next tick */
    h->ticks++;
    for (i = 0; i < n; i++)
        h->fn[i](now);
}
