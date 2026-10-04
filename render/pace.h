/* pace.h -- the frame clock's interval from what the last frame cost (ledger
 * S1, creep's CCON 1.2.8: "screen updates paced to what each flush costs").
 *
 * A frame that cost c waits 1.5 c before the next one, between 20 ms and
 * 160 ms. On a stock A1200 a full repaint costs most of a frame's time:
 * with a fixed 50 ms clock the screen was redrawn as fast as it could be,
 * and the output waited behind it. Paced, more output goes into each
 * frame, and a cheap frame (an echo, a prompt) still comes at once.
 * WaitForChar does not wait for the clock: it draws what is pending first.
 * Pure, host-tested (tests/test_pace.c). */
#ifndef VT_PACE_H
#define VT_PACE_H

#define VT_PACE_MIN_US 20000UL
#define VT_PACE_MAX_US 160000UL

static unsigned long vt_pace_next(unsigned long cost_us)
{
    unsigned long n = cost_us + cost_us / 2;
    if (n < VT_PACE_MIN_US)
        return VT_PACE_MIN_US;
    if (n > VT_PACE_MAX_US)
        return VT_PACE_MAX_US;
    return n;
}

/* EClock ticks to microseconds at freq ticks a second, exact in 32 bits
 * (C89: no long long): whole seconds, then milliseconds, then the rest. */
static unsigned long vt_pace_us(unsigned long ticks, unsigned long freq)
{
    unsigned long rem, ms;
    if (!freq)
        return 0;
    rem = ticks % freq;
    ms = rem * 1000UL / freq;                 /* rem < freq < 4.3 M */
    rem = rem * 1000UL % freq;
    return ticks / freq * 1000000UL + ms * 1000UL + rem * 1000UL / freq;
}

#endif
