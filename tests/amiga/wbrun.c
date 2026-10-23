/* wbrun NAME [NAME ...] -- what a Workbench double-click on NAME does,
 * without Workbench (rigs and the A1200 run Directory Opus in its place,
 * so workbench.library's OpenWorkbenchObjectA() answers FALSE with IoErr
 * 103, and OS 3.1 has no such call; findings in
 * thoughts/shared/research/2026-10-17_wb-icon-launch.md).
 *
 * As Workbench 3.x launches an icon:
 * - NAME's icon a project (WBPROJECT): its default tool is the program,
 *   looked up from the project's drawer, then this shell's path, then C:;
 *   the arguments are the tool, then the project.  Stack and TOOLPRI from
 *   the project icon.
 * - otherwise NAME is the tool itself; further NAMEs are the icons
 *   shift-selected with it (wbrun TOOL PROJECT).  Stack and TOOLPRI from
 *   the tool's icon, 4096 bytes without one.
 * - the process: no Input()/Output(), no console task, no window pointer,
 *   current directory and PROGDIR: the tool's drawer; sm_ToolWindow from
 *   the tool icon's TOOLWINDOW.  The launched program reads its own tool
 *   types (WINDOW, FROM, ...) from the WBArgs: wbrun does not interpret them.
 * - the Shell icon is a project whose tool SYS:System/CLI (cli 38.1 on 3.1,
 *   47.2 on 3.2) reads WINDOW, FROM and STACK and starts the shell itself.
 *
 * Waits for the message to come back (it holds the tool's code until then): run it with
 * Run >NIL: for a tool that stays open. The launch itself is amarchy/wblaunch.c, shared with
 * Amarchy's app index rows. */
#include <dos/dos.h>
#include <proto/dos.h>
#include "../../amarchy/wblaunch.h"

#define MAXARGS 8

int main(int argc, char **argv)
{
    char err[160];
    if (argc < 2 || argc > MAXARGS) {
        Printf("usage: wbrun NAME [NAME ...] (at most %ld)\n", (LONG)MAXARGS - 1);
        return 20;
    }
    if (wbl_start((const char *const *)argv + 1, argc - 1, 1, err, sizeof err) != 0) {
        Printf("wbrun: %s\n", (ULONG)err);
        return 20;
    }
    Printf("wbrun: %s returned its message\n", (ULONG)argv[1]);
    return 0;
}
