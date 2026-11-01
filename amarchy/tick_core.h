/* The pure half of the commodity tick (tick.c owns the timer): a fixed list of hooks, each called once per tick with
 * the seconds since the start. No allocation, no OS types; host-tested by tests/test_tick.c. */
#ifndef AMARCHY_TICK_CORE_H
#define AMARCHY_TICK_CORE_H

#define TICK_MAX_HOOKS 8

typedef void (*tick_fn)(long now);

typedef struct tick_hooks {
    tick_fn fn[TICK_MAX_HOOKS];
    int n;
    long ticks;            /* ticks run since init */
} tick_hooks;

void tick_hooks_init(tick_hooks *h);
/* 1 when added; 0 for a null function, one already in the list (a hook never runs twice per tick) or a full list. */
int tick_hooks_add(tick_hooks *h, tick_fn fn);
/* One tick: counts it and calls each hook once, in the order added. */
void tick_hooks_run(tick_hooks *h, long now);

#endif
