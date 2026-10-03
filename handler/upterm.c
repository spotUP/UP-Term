/* C:UPTerm -- UP-Term's slash commands from a script or another program
 * (ledger C1): "UPTerm cursor bar" does in the window of its output what
 * "/cursor bar" typed at the prompt does, and prints the answer.
 *
 *   UPTerm COMMAND/F
 *   UPTerm help          the list
 *
 * The command goes to the window of the output, or, when the output is
 * redirected (">NIL:", a file), to the window of the input.
 *
 * Return code 0 done, 10 refused (the answer says why), 20 not an UP-Term
 * window or not a command. No ixemul, no C library beyond vbcc's startup. */
#include <string.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "vtcon_packets.h"

static const char vers[] = "$VER: UPTerm 1.0 (3.10.2026) UP-Term";

int main(void)
{
    LONG args[1] = { 0 };
    struct RDArgs *rd = ReadArgs((STRPTR)"COMMAND/F", args, 0);
    static char line[256], ans[4096];
    struct FileHandle *fh;
    BPTR out = Output();
    LONG r;
    int i;
    const char *a;
    if (!rd) {
        PrintFault(IoErr(), (STRPTR)"UPTerm");
        return RETURN_FAIL;
    }
    a = args[0] ? (const char *)args[0] : "help";
    while (*a == ' ' || *a == '/')
        a++; /* "UPTerm /cursor bar" works too */
    line[0] = '/';
    strncpy(line + 1, a, sizeof(line) - 2);
    line[sizeof(line) - 1] = 0;
    FreeArgs(rd);
    /* the window of the output; redirected ("UPTerm find x >NIL:"), the
     * window of the input -- the answer still goes to the output */
    r = 0;
    for (i = 0; i < 2 && !r; i++) {
        BPTR h = i ? Input() : out;
        fh = h ? (struct FileHandle *)BADDR(h) : 0;
        if (!fh || !fh->fh_Type)
            continue;
        r = DoPkt(fh->fh_Type, ACTION_VTCON_COMMAND, fh->fh_Arg1, (LONG)line, (LONG)ans, sizeof(ans), 0);
        if (!r && IoErr() != ERROR_ACTION_NOT_KNOWN && IoErr() != ERROR_REQUIRED_ARG_MISSING)
            break; /* an UP-Term window that said no: not a command */
    }
    if (r == 0) {
        if (IoErr() == ERROR_ACTION_NOT_KNOWN)
            PutStr((STRPTR)"UPTerm: not an UP-Term window\n");
        else {
            PutStr((STRPTR)"UPTerm: no such command: ");
            PutStr((STRPTR)(line + 1));
            PutStr((STRPTR)" (UPTerm help lists them)\n");
        }
        return RETURN_FAIL;
    }
    PutStr((STRPTR)ans);
    return r == 1 ? RETURN_OK : RETURN_ERROR;
}
