/* bootcap -- run one command line in a new CLI whose standard output is a file (BOOT-CONSOLE-CAPTURE).
 *
 *   bootcap <outfile> <command line...>
 *
 * The rig's boot script ran `C:Execute DH0:S/Startup-Sequence >BOOTX:boot.console` and the file stayed 0 bytes
 * (rig 150, 2026-11-05): Execute only puts the script in front of the calling shell's input and returns; the shell
 * closes the redirect when Execute exits and then runs the script's lines with its own output (the boot console).
 * `Run >file` fails the same way: only Run's own "[CLI n]" reached the file, and the child CLI printed into a console
 * window that raised IPrefs' "attempting to reset the Workbench screen" request. AmigaDOS has no command that starts
 * a CLI with a file as its output, so this one does: SystemTags with SYS_Output = the file, SYS_UserShell, waiting for
 * the CLI to end (the Startup-Sequence's EndCLI), then closes the file, so the host reads it complete.
 * Its stdin is NIL:, so no line of the script can wait on a console. Exit code: the command's, or 20 when the file or
 * the CLI could not be made (the boot script goes on either way; rig.py reports a missing marker). */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>

long __stack = 16384;
const char bootcap_stack_cookie[] = "$STACK: 16384";

int main(int argc, char **argv)
{
    static char cmd[512];
    BPTR out, in;
    LONG rc;
    int i;
    if (argc < 3)
        return 20;
    cmd[0] = 0;
    for (i = 2; i < argc; i++) {
        if (strlen(cmd) + strlen(argv[i]) + 2 >= sizeof cmd)
            return 20;
        if (i > 2)
            strcat(cmd, " ");
        strcat(cmd, argv[i]);
    }
    out = Open((STRPTR)argv[1], MODE_NEWFILE);
    if (!out)
        return 20;
    in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    if (!in) {
        Close(out);
        return 20;
    }
    FPuts(out, (STRPTR)"boot console open\n");
    Flush(out);
    /* synchronous: System() closes neither handle, so both stay ours */
    rc = SystemTags((STRPTR)cmd, SYS_Input, in, SYS_Output, out, SYS_UserShell, TRUE, NP_StackSize, 16384, TAG_END);
    Close(in);
    Close(out);
    return rc == -1 ? 20 : (int)rc;
}
