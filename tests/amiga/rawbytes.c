/* rawbytes -- the serial login's line, byte for byte (ledger T4 G2): in
 * raw mode (SetMode 1, as sz and rz use it) write every byte value 0-255
 * between two markers, then read 256 bytes from the line and write them,
 * as hex, to RAM:rawbytes.in. A host at the far end compares both ways. */
#include <stdio.h>
#include <proto/exec.h>
#include <proto/dos.h>

int main(void)
{
    static UBYTE b[256];
    BPTR in = Input(), out = Output(), f;
    int i, got = 0;
    for (i = 0; i < 256; i++)
        b[i] = (UBYTE)i;
    SetMode(in, 1);
    Write(out, (APTR)"<<RAW", 5);
    Write(out, b, 256);
    Write(out, (APTR)"RAW>>", 5);
    while (got < 256 && WaitForChar(in, 10000000L)) {
        LONG n = Read(in, b + got, 256 - got);
        if (n <= 0)
            break;
        got += (int)n;
    }
    SetMode(in, 0);
    if ((f = Open((STRPTR)"RAM:rawbytes.in", MODE_NEWFILE)) != 0) {
        char h[4];
        for (i = 0; i < got; i++) {
            sprintf(h, "%02x", b[i]);
            Write(f, h, 2);
        }
        Close(f);
    }
    return 0;
}
