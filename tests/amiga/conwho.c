/* conwho [RAW] -- who serves this console? H5.6 of the console.device plan.
 *
 * conwho          sends ACTION_VTCON_GWINSZ to the process's console ("*"):
 *                 only the vtcon handler knows it. Prints "UP-Term rows cols"
 *                 or "ROM error N" (the ROM con-handler: 209,
 *                 ERROR_ACTION_NOT_KNOWN).
 * conwho RAW      opens RAW:0/20/300/80/conwho-raw, waits up to 10 s for one
 *                 key and prints "RAW byte HH" -- a RAW: window hands over a
 *                 key without RETURN. */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "../../handler/vtcon_packets.h"

struct ws { UWORD ws_row, ws_col, ws_xpixel, ws_ypixel; }; /* ixemul 48.2's winsize */

int main(int argc, char **argv)
{
    BPTR fh;
    if (argc > 1 && !strcmp(argv[1], "RAW")) {
        char c;
        fh = Open((STRPTR)"RAW:0/20/300/80/conwho-raw", MODE_NEWFILE);
        if (!fh) {
            printf("RAW open failed\n");
            return 20;
        }
        if (WaitForChar(fh, 10000000) && Read(fh, &c, 1) == 1)
            printf("RAW byte %02x\n", (unsigned char)c);
        else
            printf("RAW no key in 10 s\n");
        Close(fh);
        return 0;
    }
    fh = Open((STRPTR)"*", MODE_OLDFILE);
    if (!fh) {
        printf("no console\n");
        return 20;
    }
    {
        struct FileHandle *h = (struct FileHandle *)BADDR(fh);
        static struct ws w;
        LONG r = DoPkt(h->fh_Type, ACTION_VTCON_GWINSZ, h->fh_Arg1, (LONG)&w, 0, 0, 0);
        if (r)
            printf("UP-Term %d %d\n", (int)w.ws_row, (int)w.ws_col);
        else
            printf("ROM error %ld\n", IoErr());
    }
    Close(fh);
    return 0;
}
