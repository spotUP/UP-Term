/* The test C:Type 47.4 (AmigaOS 3.2) makes before it reads its arguments: Input() is not the CLI's
 * standard input, and the two are not both interactive on one handler, means "my input is
 * redirected", and Type takes its template without FROM, so `Type file` fails with 118 "wrong number
 * of arguments". The Shell keeps cli_StandardInput on its terminal; vsh's runner left the NIL: that
 * CreateNewProc put there (vshexec_rig.py case 6). Prints both, returns 5 when Type would take the
 * redirected path. */
#include <proto/dos.h>
#include <dos/dosextens.h>
#include <stdio.h>

int main(void)
{
    struct CommandLineInterface *cli = Cli();
    BPTR in = Input(), std = cli ? cli->cli_StandardInput : 0;
    int redirected = 0;
    if (std != in)
        redirected = !std || !in || !IsInteractive(std) || !IsInteractive(in) ||
                     ((struct FileHandle *)BADDR(std))->fh_Type != ((struct FileHandle *)BADDR(in))->fh_Type;
    printf("in=%lx interactive=%ld std=%lx interactive=%ld redirected=%d\n", (unsigned long)in,
           in ? (long)IsInteractive(in) : 0L, (unsigned long)std, std ? (long)IsInteractive(std) : 0L, redirected);
    return redirected ? 5 : 0;
}
