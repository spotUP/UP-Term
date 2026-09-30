/* stamp FILE -- the E-clock into FILE as "hi lo frequency" (DV3's timing: a
 * script stamps before and after the run it measures; the rig script does
 * the arithmetic -- no floating point here). */
#include <stdio.h>
#include <exec/io.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/timer.h>

struct Device *TimerBase;

int main(int argc, char **argv)
{
    struct timerequest tr;
    struct EClockVal ev;
    ULONG f;
    FILE *out;
    if (argc < 2 || OpenDevice((STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0))
        return 20;
    TimerBase = tr.tr_node.io_Device;
    f = ReadEClock(&ev);
    out = fopen(argv[1], "w");
    if (out) {
        fprintf(out, "%lu %lu %lu\n", ev.ev_hi, ev.ev_lo, f);
        fclose(out);
    }
    CloseDevice((struct IORequest *)&tr);
    return 0;
}
