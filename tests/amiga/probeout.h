/* probeout.h -- the output of the Phase D probes (chainprobe, dosnode,
 * cdprobe; plan 2026-09-30-console-device.md, phase DP).
 *
 * Every line goes to stdout and is appended to RAM:<probe>.log, the file
 * closed after each line: a probe that hangs or crashes the machine still
 * shows how far it got. Lines are:
 *   ok N what            a step of the probe itself worked
 *   FAIL N what: seen    it did not (the measurement is then incomplete)
 *   RESULT key value...  a measured fact, for the plan and the research
 *   anything else        a table row
 * The last line is "passed P of N". Include once, after <stdio.h>. */
#ifndef PROBEOUT_H
#define PROBEOUT_H
#include <stdarg.h>
#include <string.h>
#include <dos/dos.h>
#include <proto/dos.h>

static const char *po_log;
static int po_passed, po_total;

#ifdef __VBCC__
#pragma dontwarn 79  /* vbcc's va_start on the last named argument */
#endif
static void po_line(const char *fmt, ...)
{
    char line[512];
    va_list ap;
    BPTR f;
    va_start(ap, fmt);
    vsprintf(line, fmt, ap);
    va_end(ap);
    fputs(line, stdout);
    fflush(stdout);
    if (po_log && (f = Open((STRPTR)po_log, MODE_READWRITE)) != 0) {
        Seek(f, 0, OFFSET_END);
        Write(f, line, (LONG)strlen(line));
        Close(f);
    }
}

/* Starts RAM:<name>.log afresh. */
static void po_start(const char *logname)
{
    BPTR f;
    po_log = logname;
    if ((f = Open((STRPTR)logname, MODE_NEWFILE)) != 0)
        Close(f);
}

static int po_check(int ok, const char *what, const char *seen)
{
    po_total++;
    if (ok)
        po_passed++;
    po_line("%s %d %s%s%s\n", ok ? "ok" : "FAIL", po_total, what,
            (!ok && seen) ? ": " : "", (!ok && seen) ? seen : "");
    return ok;
}

static int po_end(void)
{
    po_line("passed %d of %d\n", po_passed, po_total);
    return po_passed == po_total ? 0 : 10;
}
#endif
