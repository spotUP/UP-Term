/* muhit [rom] -- one illegal access on purpose, so MuForce (rig.py muforce on)
 * reports a hit: a long read of address 0 (the default), or with "rom" a
 * word write into the Kickstart ROM. The value read is printed so the
 * compiler cannot drop the access. MuForce lets the program go on, so the
 * program ends with rc 0 whether or not MuForce runs. */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "rom") == 0) {
        volatile UWORD *rom = (volatile UWORD *)0x00F80000;
        *rom = 0x4E71;
        printf("muhit: wrote 0x4E71 to 0x00F80000\n");
    } else {
        volatile ULONG *zero = (volatile ULONG *)0;
        ULONG v = *zero;
        printf("muhit: read %08lx from address 0\n", (unsigned long)v);
    }
    return 0;
}
