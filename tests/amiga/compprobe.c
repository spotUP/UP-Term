/* compprobe -- the console side of vsh's programmable completion (V93), without vsh.
 * Arms the console (ACTION_VTCON_COMPLETE, VTCON_COMP_ARM) and reads lines from it. A line that is the
 * completion marker is answered with the words given as arguments (one packet, NUL-separated; the argument
 * "-n" first: VTCON_COMP_NOSPACE, "-d": VTCON_COMP_DEFAULT) and read again; the first other line ends it.
 * Every line read goes to RAM:compprobe.txt, the marker's ESC written as "<M>". What it shows: a Read
 * answered with the marker leaves the console's line editor with its line, and the words come in. */
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include "../../handler/vtcon_packets.h"

static char buf[600], words[600];

int main(int argc, char **argv)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(Input());
    BPTR out = Open((STRPTR) "RAM:compprobe.txt", MODE_NEWFILE);
    long n, wl = 0, flags = 0;
    int i, m = (int)strlen(VTCON_COMPLETE_MARK);
    if (!out || !fh || !fh->fh_Type)
        return 20;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-n"))
            flags |= VTCON_COMP_NOSPACE;
        else if (!strcmp(argv[i], "-d"))
            flags |= VTCON_COMP_DEFAULT;
        else if (wl + (long)strlen(argv[i]) + 1 < (long)sizeof(words)) {
            strcpy(words + wl, argv[i]);
            wl += (long)strlen(argv[i]) + 1;
        }
    }
    if (!DoPkt(fh->fh_Type, ACTION_VTCON_COMPLETE, fh->fh_Arg1, 0, 0, VTCON_COMP_ARM, 0)) {
        Write(out, (APTR) "refused\n", 8);
        Close(out);
        return 10;
    }
    for (;;) {
        n = Read(Input(), buf, sizeof(buf) - 1);
        if (n <= 0)
            break;
        buf[n] = 0;
        if (n > m && !memcmp(buf, VTCON_COMPLETE_MARK, m)) {
            Write(out, (APTR) "<M>", 3);
            Write(out, buf + m, n - m);
            DoPkt(fh->fh_Type, ACTION_VTCON_COMPLETE, fh->fh_Arg1, (LONG)words, wl, flags, 0);
            continue;
        }
        Write(out, buf, n);
        break;
    }
    Close(out);
    return 0;
}
