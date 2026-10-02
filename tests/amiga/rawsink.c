/* rawsink N -- how fast the serial login's line delivers (ledger T4, the rz
 * speed follow-up): raw mode, read N bytes as they come (WaitForChar +
 * Read, as rz does), then print the bytes, the reads it took and the
 * seconds. Splits the line's own speed from the protocol's. */
#include <stdio.h>
#include <stdlib.h>
#include <proto/exec.h>
#include <proto/dos.h>

int main(int argc, char **argv)
{
    static UBYTE b[1024];
    BPTR in = Input();
    long want = argc > 1 ? atol(argv[1]) : 70000, got = 0, reads = 0;
    struct DateStamp a, z;
    long ticks;
    SetMode(in, 1);
    printf("READY\n");
    fflush(stdout);
    DateStamp(&a);
    while (got < want && WaitForChar(in, 20000000L)) {
        LONG n = Read(in, b, sizeof(b));
        if (n <= 0)
            break;
        got += n;
        reads++;
    }
    DateStamp(&z);
    SetMode(in, 0);
    ticks = (z.ds_Days - a.ds_Days) * 86400L * 50 + (z.ds_Minute - a.ds_Minute) * 3000L +
            (z.ds_Tick - a.ds_Tick);
    printf("\nGOT %ld bytes in %ld reads, %ld.%02ld s\n", got, reads, ticks / 50, (ticks % 50) * 2);
    return 0;
}
