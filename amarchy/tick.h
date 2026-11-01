/* The commodity tick (roadmap 2026-10-29-amarchy-p2-status-bar.md R2.3a): one poll_timer at TICK_MICROS owned by
 * Amarchy itself, so periodic work (notice expiry, the bar's samplers) runs with the bar off. Opened before
 * bar_open, served in the Wait loop. Amiga only; the hook list is tick_core.c. */
#ifndef AMARCHY_TICK_H
#define AMARCHY_TICK_H

#include "tick_core.h"

#define TICK_MICROS 1000000UL

/* 1 when the timer is running; on 0 the commodity runs without a tick (hooks never fire). */
int tick_open(void);
void tick_close(void);
unsigned long tick_sigmask(void);
/* In the Wait loop with the signals Wait returned: when the timer fired, re-arms it and calls every hook once. */
void tick_serve(unsigned long sigs);
/* Whole seconds since tick_open (0 before it); the value every hook is given. */
long tick_now(void);
/* Ticks served since tick_open (STATUS ticks=). */
long tick_count(void);
/* Adds fn to the per-tick list (fixed array of TICK_MAX_HOOKS, nothing allocated); 1 when added. */
int tick_hook(tick_fn fn);
/* Seconds since the Amiga epoch (DateStamp), the wall clock note_core and the bar are given. */
long tick_epoch(void);

#endif
