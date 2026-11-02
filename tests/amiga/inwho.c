/* inwho -- whose stream is this program's input? (tools/rig/shstdin_rig.py)
 *
 * Prints one line: Input()'s handler (the task behind the handle's port),
 * whether it is interactive, and the handler of pr_ConsoleTask, the port
 * Open("*") goes to. A program vsh starts in the foreground on vsh's own
 * input must get that input, not a second handle on some other console.
 *
 * `inwho FILE` also appends a second line to FILE (a pane whose output is
 * lost still answers there): the port address and handler of Input(),
 * Output(), cli_StandardInput/Output and pr_ConsoleTask (TMUX-PANE-CONSOLE). */
#include <stdio.h>
#include <string.h>
#include <exec/execbase.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>

static const char *port_task(struct MsgPort *p)
{
    struct Task *t;
    if (!p)
        return "none";
    t = (struct Task *)p->mp_SigTask;
    return t && t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "?";
}

int main(int argc, char **argv)
{
    struct Process *me = (struct Process *)FindTask(0);
    BPTR in = Input();
    struct FileHandle *fh = in ? (struct FileHandle *)BADDR(in) : 0;
    printf("in=%s interactive=%d console=%s same=%d\n", fh ? port_task(fh->fh_Type) : "0",
           in ? (int)(IsInteractive(in) != 0) : -1, port_task(me->pr_ConsoleTask),
           fh && fh->fh_Type == me->pr_ConsoleTask);
    if (argc > 1) {
        struct CommandLineInterface *cli = Cli();
        FILE *f = fopen(argv[1], "a");
        if (f) {
            BPTR s[4];
            static const char *nm[4] = {"in", "out", "clin", "clout"};
            int k;
            s[0] = in;
            s[1] = Output();
            s[2] = cli ? cli->cli_StandardInput : 0;
            s[3] = cli ? cli->cli_StandardOutput : 0;
            for (k = 0; k < 4; k++) {
                struct FileHandle *h = s[k] ? (struct FileHandle *)BADDR(s[k]) : 0;
                fprintf(f, "%s=%lx/%lx:%s ", nm[k], (unsigned long)s[k], h ? (unsigned long)h->fh_Type : 0UL,
                        h ? port_task(h->fh_Type) : "0");
            }
            fprintf(f, "console=%lx:%s\n", (unsigned long)me->pr_ConsoleTask, port_task(me->pr_ConsoleTask));
            fclose(f);
        }
    }
    return 0;
}
