/* ixlink_dos -- the raw dos.library half of ixlink: its own unit, since
 * the NDK's devices/timer.h redefines ixemul's struct timeval. */
#include <stdio.h>
#include <proto/dos.h>

void ixlink_raw(const char *from, const char *to)
{
    BPTR l = Lock((STRPTR) from, ACCESS_READ);
    LONG ok, ioerr;
    if (!l) { printf("  raw MakeLink: Lock(%s) failed IoErr %ld\n", from, (long) IoErr()); return; }
    ok = MakeLink((STRPTR) to, (LONG) l, LINK_HARD);
    ioerr = IoErr();
    UnLock(l);
    printf("  raw MakeLink(%s) %s IoErr %ld\n", to, ok ? "ok" : "failed", (long) ioerr);
    if (ok) DeleteFile((STRPTR) to);
}

