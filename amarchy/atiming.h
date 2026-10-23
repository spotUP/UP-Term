/* Opt-in timing marks for the quick menu's open latency (plan 2026-10-22-amarchy-commodity.md, Phase D1).
 * Off unless the AmigaDOS variable AMARCHY_TIMING is set (`SetEnv AMARCHY_TIMING 1`). When set, at_mark(tag)
 * appends "<tag> <ticks>" to RAM:amarchy.timing, ticks = DateStamp in 1/50 s since midnight (the commodity and
 * the helper are two programs; DateStamp is the one clock both read cheaply). tools/rig/amarchy_timing_rig.py
 * reads the file. A header of static code so both the vbcc commodity and the gcc helper take it unchanged. */
#ifndef AMARCHY_ATIMING_H
#define AMARCHY_ATIMING_H

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/datetime.h>
#include <proto/dos.h>

static void at_mark(const char *tag)
{
    char var[4], line[48];
    struct DateStamp ds;
    BPTR f;
    long t;
    int i = 0, n;
    char digits[16];
    if (GetVar((STRPTR)"AMARCHY_TIMING", (STRPTR)var, sizeof(var), 0) < 0)
        return;
    DateStamp(&ds);
    t = ds.ds_Minute * 3000L + ds.ds_Tick;
    while (*tag && i < 20)
        line[i++] = *tag++;
    line[i++] = ' ';
    n = 0;
    do {
        digits[n++] = (char)('0' + t % 10);
        t /= 10;
    } while (t && n < 15);
    while (n)
        line[i++] = digits[--n];
    line[i++] = '\n';
    if ((f = Open((STRPTR)"RAM:amarchy.timing", MODE_READWRITE))) {
        Seek(f, 0, OFFSET_END);
        Write(f, line, i);
        Close(f);
    }
}

#endif
