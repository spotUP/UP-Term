/* pollbench: what a zero-timeout input poll on the console costs (ncurses'
 * typeahead check in doupdate: select() with a zero timeout, which ixemul
 * turns into WaitForChar(fh, 0), an ACTION_WAIT_CHAR packet with dp_Arg1 0).
 * N rounds each of: WaitForChar(Input(), 0); select() on fd 0 with a zero
 * timeout; WaitForChar after a one-byte write (a render is pending); and
 * IsInteractive (no packet) for the floor. E-clock timing, microseconds a
 * call; the results on stdout. ixemul. Run with the console as input:
 *   VTC:pollbench <XCON:0/0/640/200/PB/AMIGA >RAM:pb.txt */
#ifdef POLLBENCH_ECLOCK
/* the E-clock half: devices/timer.h and ixemul's sys/time.h both define
 * struct timeval, so it is a translation unit of its own */
#include <exec/types.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/timer.h>
struct Device *TimerBase;
static struct timerequest tr;
static ULONG hz;
ULONG ec_open(void)
{
    struct EClockVal e;
    if (OpenDevice((STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0))
        return 0;
    TimerBase = tr.tr_node.io_Device;
    return hz = ReadEClock(&e);
}
ULONG ec_lo(void)
{
    struct EClockVal e;
    ReadEClock(&e);
    return e.ev_lo;
}
void ec_close(void)
{
    CloseDevice((struct IORequest *)&tr);
}
#else
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

ULONG ec_open(void);
ULONG ec_lo(void);
void ec_close(void);
static ULONG hz;

static ULONG since(ULONG a)
{
    return (ULONG)(((double)(ec_lo() - a)) * 1000000.0 / hz);
}

#define N 100

static void report(const char *what, ULONG total, ULONG max)
{
    printf("%-26s %5lu us/call (max %lu us, n=%d)\n", what, (unsigned long)(total / N), (unsigned long)max, N);
}

int main(int argc, char **argv)
{
    ULONG c;
    BPTR out = Input(); /* the console; a CON handle reads and writes */
    ULONG t, tot, mx;
    int i, r = 0;
    if (!(hz = ec_open()))
        return 20;
    printf("pollbench: eclock %lu Hz\n", (unsigned long)hz);
    fflush(stdout);

    tot = mx = 0;
    for (i = 0; i < N; i++) {
        c = ec_lo();
        r |= WaitForChar(out, 0) ? 1 : 0;
        t = since(c);
        tot += t;
        if (t > mx)
            mx = t;
    }
    report("WaitForChar(in,0)", tot, mx);
    fflush(stdout);

    tot = mx = 0;
    for (i = 0; i < N; i++) {
        fd_set in;
        struct timeval tv;
        tv.tv_sec = tv.tv_usec = 0;
        FD_ZERO(&in);
        FD_SET(0, &in);
        c = ec_lo();
        r |= select(1, &in, 0, 0, &tv) > 0 ? 2 : 0;
        t = since(c);
        tot += t;
        if (t > mx)
            mx = t;
    }
    report("select(fd 0, tv 0)", tot, mx);
    fflush(stdout);

    tot = mx = 0;
    for (i = 0; i < N; i++) {
        Write(out, (APTR) ".", 1);
        c = ec_lo();
        r |= WaitForChar(out, 0) ? 4 : 0;
        t = since(c);
        tot += t;
        if (t > mx)
            mx = t;
    }
    Write(out, (APTR) "\n", 1);
    report("write+WaitForChar(in,0)", tot, mx);

    tot = mx = 0;
    for (i = 0; i < N; i++) {
        Write(out, (APTR) ".", 1);
        c = ec_lo();
        IsInteractive(out);
        t = since(c);
        tot += t;
        if (t > mx)
            mx = t;
    }
    Write(out, (APTR) "\n", 1);
    report("write+IsInteractive", tot, mx);
    printf("pollbench: done (ready bits %d)\n", r);
    ec_close();
    return 0;
}
#endif
