/* The commodity tick, the Amiga half (tick.h). Reuses poll_timer.c; the hook list is tick_core.c. */
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include "tick.h"
#include "poll_timer.h"

static poll_timer g_timer;
static tick_hooks g_hooks;
static int g_inited, g_open;
static long g_start;

static void ensure(void)
{
    if (!g_inited) {
        tick_hooks_init(&g_hooks);
        g_inited = 1;
    }
}

long tick_epoch(void)
{
    struct DateStamp ds;
    DateStamp(&ds);
    return ds.ds_Days * 86400L + ds.ds_Minute * 60L + ds.ds_Tick / TICKS_PER_SECOND;
}

int tick_hook(tick_fn fn)
{
    ensure();
    return tick_hooks_add(&g_hooks, fn);
}

int tick_open(void)
{
    ensure();
    if (g_open)
        return 1;
    if (!pt_open(&g_timer)) {
        pt_close(&g_timer);
        return 0;
    }
    g_start = tick_epoch();
    g_open = 1;
    pt_start(&g_timer, TICK_MICROS);
    return 1;
}

void tick_close(void)
{
    pt_close(&g_timer);
    g_open = 0;
}

unsigned long tick_sigmask(void)
{
    return g_open ? pt_sigmask(&g_timer) : 0;
}

long tick_now(void)
{
    return g_open ? tick_epoch() - g_start : 0;
}

long tick_count(void)
{
    ensure();
    return g_hooks.ticks;
}

void tick_serve(unsigned long sigs)
{
    if (!g_open || !pt_fired(&g_timer, sigs))
        return;
    pt_start(&g_timer, TICK_MICROS);
    tick_hooks_run(&g_hooks, tick_now());
}
