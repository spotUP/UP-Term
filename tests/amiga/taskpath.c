/* taskpath NAME -- the command path of process NAME's CLI (P8: where a
 * Workbench-started vsh can take the user's path from). */
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>

int main(int argc, char **argv)
{
    struct Process *p;
    struct CommandLineInterface *cli;
    BPTR *n;
    char buf[256];
    if (argc < 2)
        return 20;
    Forbid();
    p = (struct Process *)FindTask((STRPTR)argv[1]);
    cli = p && p->pr_Task.tc_Node.ln_Type == NT_PROCESS ? (struct CommandLineInterface *)BADDR(p->pr_CLI) : 0;
    Permit();
    Printf("%s: %s, cli %s\n", argv[1], p ? "found" : "none", cli ? "yes" : "no");
    for (n = cli ? (BPTR *)BADDR(cli->cli_CommandDir) : 0; n; n = (BPTR *)BADDR(n[0]))
        if (NameFromLock(n[1], (STRPTR)buf, sizeof(buf)))
            Printf("  %s\n", buf);
    return 0;
}
