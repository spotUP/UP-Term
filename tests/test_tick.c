/* The commodity tick's hook list (amarchy/tick_core.c; the timer is tick.c, proved on the rig by ticks= in STATUS). */
#include "../amarchy/tick_core.h"
#include "harness.h"

static long seen_a, seen_b, now_a, calls_a, calls_b;
static tick_hooks *g_reg;

static void hook_a(long now) { calls_a++; now_a = now; seen_a++; }
static void hook_b(long now) { (void)now; calls_b++; seen_b++; }
static void hook_adds(long now) { (void)now; tick_hooks_add(g_reg, hook_b); }

static void test_hooks(void)
{
    tick_hooks h;
    tick_hooks_init(&h);
    calls_a = calls_b = seen_a = seen_b = 0;
    tick_hooks_run(&h, 1);                              /* an empty list is a tick that calls nothing */
    CHECK_INT(h.ticks, 1);
    CHECK_INT(tick_hooks_add(&h, hook_a), 1);
    CHECK_INT(tick_hooks_add(&h, hook_a), 0);           /* never twice per tick */
    CHECK_INT(tick_hooks_add(&h, 0), 0);
    CHECK_INT(tick_hooks_add(&h, hook_b), 1);
    tick_hooks_run(&h, 7);
    CHECK_INT(calls_a, 1);
    CHECK_INT(calls_b, 1);
    CHECK_INT(now_a, 7);                                /* the hook gets the tick's seconds */
    tick_hooks_run(&h, 8);
    CHECK_INT(calls_a, 2);
    CHECK_INT(h.ticks, 3);
}

static void test_full_and_reentry(void)
{
    tick_hooks h;
    int added = 0;
    tick_hooks_init(&h);
    added += tick_hooks_add(&h, hook_a);
    added += tick_hooks_add(&h, hook_b);
    CHECK_INT(added, 2);
    h.n = TICK_MAX_HOOKS;                               /* a full list refuses */
    CHECK_INT(tick_hooks_add(&h, hook_adds), 0);
    /* a hook that adds a hook: the new one starts next tick */
    tick_hooks_init(&h);
    calls_b = 0;
    g_reg = &h;
    tick_hooks_add(&h, hook_adds);
    tick_hooks_run(&h, 1);
    CHECK_INT(calls_b, 0);
    tick_hooks_run(&h, 2);
    CHECK_INT(calls_b, 1);
}

void suite_tick(void)
{
    test_hooks();
    test_full_and_reentry();
}
