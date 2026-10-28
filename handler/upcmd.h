/* upcmd.h -- send one slash line to the UP-Term window a program runs in (shared by C:UPTerm and C:Agent).
 *
 * The window of the output; redirected ("UPTerm find x >NIL:"), the window of the input; neither
 * ("x=$(UPTerm state tabs-opened)": vsh gives a command substitution NIL: and a pipe), the console
 * of the process, "*" at pr_ConsoleTask. The answer text goes to ans. Returns DoPkt's result:
 * 0 and IoErr() ERROR_ACTION_NOT_KNOWN when none of them is an UP-Term window, 0 and another IoErr()
 * when a window said no, else 1 (done) or 2 (a failed command). Amiga-only; static, one include per tool. */
#ifndef UPCMD_H
#define UPCMD_H
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include "vtcon_packets.h"

static LONG upterm_window_command(const char *line, char *ans, ULONG cap)
{
    LONG r = 0;
    int i;
    for (i = 0; i < 3 && !r; i++) {
        BPTR h = i == 0 ? Output() : i == 1 ? Input() : Open((STRPTR)"*", MODE_OLDFILE);
        struct FileHandle *fh = h ? (struct FileHandle *)BADDR(h) : 0;
        if (fh && fh->fh_Type)
            r = DoPkt(fh->fh_Type, ACTION_VTCON_COMMAND, fh->fh_Arg1, (LONG)line, (LONG)ans, (LONG)cap, 0);
        else
            SetIoErr(ERROR_ACTION_NOT_KNOWN);
        if (i == 2 && h) {
            LONG e = IoErr();
            Close(h);
            SetIoErr(e);
        }
        if (!r && IoErr() != ERROR_ACTION_NOT_KNOWN && IoErr() != ERROR_REQUIRED_ARG_MISSING)
            break; /* an UP-Term window that said no: not a command */
    }
    return r;
}
#endif
