/* C:UPTerm -- UP-Term's slash commands from a script or another program
 * (ledger C1): "UPTerm cursor bar" does in the window of its output what
 * "/cursor bar" typed at the prompt does, and prints the answer.
 *
 *   UPTerm COMMAND/F
 *   UPTerm help          the list
 *   UPTerm state [KEY]   what the user did in the window (counters; a KEY prints its bare value,
 *                        an unknown one returns 5 with a message): the tutorial's checks
 *
 * The command goes to the window of the output, or, when the output is
 * redirected (">NIL:", a file), to the window of the input, and with
 * neither on a window (a command substitution), to the process's console.
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
#include "../tty/qmark.h"

static const char vers[] = "$VER: UPTerm 1.0 (3.10.2026) UP-Term";

int main(void)
{
    LONG args[1] = { 0 };
    static const char tmpl[] = "COMMAND/F";
    struct RDArgs *rd;
    static char line[256], ans[4096];
    struct FileHandle *fh;
    BPTR out = Output();
    LONG r;
    int i;
    const char *a;
    if (qmark_line((const char *)GetArgStr())) {
        PutStr((STRPTR)tmpl);
        PutStr((STRPTR)"\n");
        return RETURN_OK;
    }
    rd = ReadArgs((STRPTR)tmpl, args, 0);
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
     * window of the input; neither ("x=$(UPTerm state tabs-opened)": vsh
     * gives a command substitution NIL: and a pipe), the console of the
     * process, "*" at pr_ConsoleTask -- the answer still goes to the output */
    r = 0;
    for (i = 0; i < 3 && !r; i++) {
        BPTR h = i == 0 ? out : i == 1 ? Input() : Open((STRPTR)"*", MODE_OLDFILE);
        fh = h ? (struct FileHandle *)BADDR(h) : 0;
        if (fh && fh->fh_Type)
            r = DoPkt(fh->fh_Type, ACTION_VTCON_COMMAND, fh->fh_Arg1, (LONG)line, (LONG)ans, sizeof(ans), 0);
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
