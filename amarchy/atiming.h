/* Opt-in timing marks for the quick menu's open latency (plan 2026-10-22-amarchy-commodity.md, Phase D1).
 * Off unless the AmigaDOS variable AMARCHY_TIMING is set (`SetEnv AMARCHY_TIMING 1`). When set, at_mark(tag)
 * appends "<tag> <ticks>" to RAM:amarchy.timing, ticks = DateStamp in 1/50 s since midnight (the commodity and
 * the helper are two programs; DateStamp is the one clock both read cheaply). tools/rig/amarchy_timing_rig.py
 * reads the file. Marks of one warm open: "hotkey" (commodity), "send" (commodity, after the PutMsg to the
 * AMARCHY.MENUSRV port, menu_open.h), "start" (server, message taken), "terminal" (ui_start returned), "ready"
 * (menu painted). A header of static code so both the vbcc commodity and the gcc helper take it unchanged. */
#ifndef AMARCHY_ATIMING_H
#define AMARCHY_ATIMING_H

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/datetime.h>
#include <proto/dos.h>

/* DateStamp ticks (1/50 s) since midnight: the steady clock of the menu's Backspace repeat guard. */
static long at_ticks(void)
{
    struct DateStamp ds;
    DateStamp(&ds);
    return ds.ds_Minute * 3000L + ds.ds_Tick;
}

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

/* Opt-in note for the rig: the global variable `name` (ENV:) holds `value` in hex, so a script can read which task
 * owned the menu port at an open (a stale port keeps its dead owner). Same switch as at_mark. */
static void at_note(const char *name, unsigned long value)
{
    char var[4], hex[12];
    int i;
    if (GetVar((STRPTR)"AMARCHY_TIMING", (STRPTR)var, sizeof(var), 0) < 0)
        return;
    for (i = 0; i < 8; i++)
        hex[i] = "0123456789abcdef"[(value >> (28 - 4 * i)) & 15];
    hex[8] = 0;
    SetVar((STRPTR)name, (STRPTR)hex, 8, GVF_GLOBAL_ONLY);
}

#endif
