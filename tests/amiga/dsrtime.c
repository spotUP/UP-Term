/* dsrtime: the console's answer time for Neovim 0.12's startup query
 * (OSC 11 colour + DSR 5n, runtime/lua/vim/_core/defaults.lua: it waits
 * 100 ms for the DSR answer, else E1568). Raw mode, ten rounds, the time
 * from Write to the DSR answer's last byte, E-clock. AmigaDOS only. */
#include <exec/types.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>

struct Device *TimerBase;

int main(void)
{
    struct timerequest tr;
    struct EClockVal a, b;
    BPTR in = Input(), out = Output();
    static const char q[] = "\033]11;?\007\033[5n";
    char buf[128];
    ULONG hz, us[10];
    int i;
    if (OpenDevice((STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0))
        return 20;
    TimerBase = tr.tr_node.io_Device;
    SetMode(in, 1);
    for (i = 0; i < 10; i++) {
        int got = 0, done = 0;
        hz = ReadEClock(&a);
        Write(out, (APTR)q, sizeof(q) - 1);
        while (!done && WaitForChar(in, 1000000)) {
            LONG n = Read(in, buf, sizeof(buf));
            int k;
            for (k = 0; k < n; k++)
                if (buf[k] == 'n')
                    done = 1;
            got += (int)n;
        }
        ReadEClock(&b);
        us[i] = done ? ((b.ev_lo - a.ev_lo) * 1000UL) / (hz / 1000UL) : 0; /* no floats: no math library */
    }
    SetMode(in, 0);
    {
        /* the results to RAM:dsrtime.txt: the window is the thing measured */
        BPTR f = Open((STRPTR)"RAM:dsrtime.txt", MODE_NEWFILE);
        for (i = 0; f && i < 10; i++)
            FPrintf(f, (STRPTR)"round %ld: %ld us\n", (LONG)i, (LONG)us[i]);
        if (f)
            Close(f);
    }
    CloseDevice((struct IORequest *)&tr);
    return 0;
}
