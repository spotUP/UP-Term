/* busyloop SECONDS: keeps the CPU busy at priority 0 for SECONDS (proc_rig.py: top must show it near 100 %). */
#include <stdlib.h>
#include <dos/dos.h>
#include <proto/dos.h>

int main(int argc, char **argv)
{
    struct DateStamp a, b;
    long secs = argc > 1 ? atol(argv[1]) : 10;
    volatile unsigned long spin = 0;
    DateStamp(&a);
    for (;;) {
        int i;
        for (i = 0; i < 20000; i++)
            spin++;
        DateStamp(&b);
        if ((b.ds_Days - a.ds_Days) * 86400L + (b.ds_Minute - a.ds_Minute) * 60L +
                (b.ds_Tick - a.ds_Tick) / TICKS_PER_SECOND >= secs)
            break;
    }
    return 0;
}
