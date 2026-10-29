/* rexxbound -- rexx_send is bounded (REXX-DEAD-AFTER-THEME). Sends to the port given (tests/amiga/deafport, which holds
 * its first reply for 12 s), prints one line per step:
 *   first: rc=<rc> secs=<elapsed> timeouts=<n>   -- must be rc -1 after about REXX_SEND_SECS, timeouts 1
 *   reaped: timeouts=<n>                          -- after the late reply, rexx_send_reap frees the parked message once
 *   second: rc=<rc>                                -- the port answers at once now: rc 0
 * Exits 0 when all hold. Usage: rexxbound PORT */
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include "../../amarchy/rexx_host.h"

static long secs_now(void)
{
    struct DateStamp d;
    DateStamp(&d);
    return d.ds_Minute * 60 + d.ds_Tick / 50;
}

int main(int argc, char **argv)
{
    char res[32];
    long t0, rc, el;
    int bad = 0;
    if (argc < 2)
        return 10;
    t0 = secs_now();
    rc = rexx_send(argv[1], "PING", res, sizeof res);
    el = secs_now() - t0;
    if (el < 0)
        el += 3600;
    printf("first: rc=%ld secs=%ld timeouts=%lu\n", rc, el, rexx_send_timeouts);
    bad += !(rc == -1 && el >= REXX_SEND_SECS - 1 && el <= REXX_SEND_SECS + 3 && rexx_send_timeouts == 1);
    Delay(5 * 50);          /* the peer replies 12 s after it got the message */
    rexx_send_reap();
    rexx_send_reap();       /* a second reap must find nothing to free (no double free) */
    printf("reaped: timeouts=%lu\n", rexx_send_timeouts);
    rc = rexx_send(argv[1], "PING", res, sizeof res);
    printf("second: rc=%ld\n", rc);
    bad += rc != 0;
    printf(bad ? "FAIL\n" : "PASS\n");
    return bad ? 10 : 0;
}
