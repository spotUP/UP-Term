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
 * Prints "wbrun: started TOOL" once the message is sent, then waits for the
 * message to come back (it holds the tool's code until then): run it with
 * Run >NIL: for a tool that stays open. */
#include <string.h>
#include <exec/memory.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>

#define MAXARGS 8
#define MINSTACK 4096

struct Library *IconBase;

struct wb_path { BPTR next, lock; }; /* a cli_CommandDir node */

/* NAME as a WBArg: its drawer and its name in it.  A project that is only an
 * icon (SYS:System/Shell) is found by its .info, as Workbench hands it over. */
static BPTR parent_and_name(const char *path, char *name, int size)
{
    char info[256];
    BPTR lock = Lock((STRPTR)path, SHARED_LOCK), dir;
    if (!lock && strlen(path) + 6 <= sizeof(info)) {
        strcpy(info, path);
        strcat(info, ".info");
        lock = Lock((STRPTR)info, SHARED_LOCK);
    }
    if (!lock || (int)strlen((const char *)FilePart((STRPTR)path)) >= size) {
        if (lock)
            UnLock(lock);
        return 0;
    }
    strcpy(name, (const char *)FilePart((STRPTR)path));
    dir = ParentDir(lock);
    UnLock(lock);
    return dir;
}

/* LoadSeg TOOL relative to DIR; its drawer and name on success */
static BPTR load_in(BPTR dir, const char *tool, BPTR *tdir, char *tname, int size)
{
    BPTR old = CurrentDir(dir), seg = LoadSeg((STRPTR)tool);
    if (seg && !(*tdir = parent_and_name(tool, tname, size))) {
        UnLoadSeg(seg);
        seg = 0;
    }
    CurrentDir(old);
    return seg;
}

/* the project's default tool, found as Workbench finds it */
static BPTR load_default_tool(BPTR pdir, const char *tool, BPTR *tdir, char *tname, int size)
{
    struct CommandLineInterface *cli = Cli();
    BPTR seg = load_in(pdir, tool, tdir, tname, size), p, c;
    if (seg || strchr(tool, ':') || strchr(tool, '/'))
        return seg;
    for (p = cli ? cli->cli_CommandDir : 0; p && !seg; p = ((struct wb_path *)BADDR(p))->next)
        seg = load_in(((struct wb_path *)BADDR(p))->lock, tool, tdir, tname, size);
    if (!seg && (c = Lock((STRPTR)"C:", SHARED_LOCK))) {
        seg = load_in(c, tool, tdir, tname, size);
        UnLock(c);
    }
    return seg;
}

static LONG tooltype_num(struct DiskObject *d, const char *key, LONG dflt)
{
    STRPTR v = d ? FindToolType((STRPTR *)d->do_ToolTypes, (STRPTR)key) : 0;
    LONG n;
    return v && StrToLong(v, &n) > 0 ? n : dflt;
}

int main(int argc, char **argv)
{
    static char names[MAXARGS][108];
    static struct WBArg args[MAXARGS];
    struct DiskObject *first, *icon;
    struct WBStartup *wb = 0;
    struct MsgPort *port = 0;
    struct Process *p;
    STRPTR toolwin;
    BPTR seg = 0;
    LONG stack, pri;
    int n = 0, i, rc = 20;

    if (argc < 2 || argc > MAXARGS) {
        Printf("usage: wbrun NAME [NAME ...] (at most %ld)\n", (LONG)MAXARGS - 1);
        return 20;
    }
    if (!(IconBase = OpenLibrary((STRPTR)"icon.library", 36)))
        return 20;
    first = GetDiskObject((STRPTR)argv[1]);
    if (first && first->do_Type == WBPROJECT) {
        /* args[0] the default tool, args[1] the project */
        BPTR pdir = parent_and_name(argv[1], names[1], sizeof(names[1]));
        if (!pdir || !first->do_DefaultTool || !*first->do_DefaultTool) {
            Printf("wbrun: %s: project without a default tool\n", (ULONG)argv[1]);
            goto out;
        }
        args[1].wa_Lock = pdir;
        args[1].wa_Name = (STRPTR)names[1];
        n = 2;
        seg = load_default_tool(pdir, (char *)first->do_DefaultTool, &args[0].wa_Lock, names[0],
                                sizeof(names[0]));
        if (!seg) {
            Printf("wbrun: default tool %s not found\n", (ULONG)first->do_DefaultTool);
            goto out;
        }
        icon = first;
    } else {
        /* args[0] the tool, then the shift-selected icons */
        seg = LoadSeg((STRPTR)argv[1]);
        if (!seg || !(args[0].wa_Lock = parent_and_name(argv[1], names[0], sizeof(names[0])))) {
            Printf("wbrun: %s: not a program\n", (ULONG)argv[1]);
            goto out;
        }
        n = 1;
        icon = first;
    }
    args[0].wa_Name = (STRPTR)names[0];
    for (i = 2; i < argc; i++, n++) {
        if (!(args[n].wa_Lock = parent_and_name(argv[i], names[n], sizeof(names[n])))) {
            Printf("wbrun: %s: not found\n", (ULONG)argv[i]);
            goto out;
        }
        args[n].wa_Name = (STRPTR)names[n];
    }

    stack = icon && icon->do_StackSize > MINSTACK ? icon->do_StackSize : MINSTACK;
    pri = tooltype_num(icon, "TOOLPRI", 0);
    if (pri < -128) pri = -128;
    if (pri > 127) pri = 127;
    toolwin = icon ? FindToolType((STRPTR *)icon->do_ToolTypes, (STRPTR)"TOOLWINDOW") : 0;

    port = CreateMsgPort();
    wb = AllocVec(sizeof(*wb), MEMF_PUBLIC | MEMF_CLEAR);
    if (!port || !wb) {
        Printf("wbrun: no memory\n");
        goto out;
    }
    p = CreateNewProcTags(NP_Seglist, seg, NP_FreeSeglist, FALSE, NP_Name, (ULONG)names[0],
                          NP_StackSize, stack, NP_Priority, pri,
                          NP_Input, 0, NP_Output, 0, NP_CloseInput, FALSE, NP_CloseOutput, FALSE,
                          NP_ConsoleTask, 0, NP_WindowPtr, 0,
                          NP_CurrentDir, DupLock(args[0].wa_Lock), NP_HomeDir, DupLock(args[0].wa_Lock),
                          NP_Cli, FALSE, TAG_END);
    if (!p) {
        Printf("wbrun: no process\n");
        goto out;
    }
    wb->sm_Message.mn_ReplyPort = port;
    wb->sm_Message.mn_Length = sizeof(*wb);
    wb->sm_Process = &p->pr_MsgPort;
    wb->sm_Segment = seg;
    wb->sm_NumArgs = n;
    wb->sm_ToolWindow = toolwin;
    wb->sm_ArgList = args;
    PutMsg(&p->pr_MsgPort, &wb->sm_Message);
    Printf("wbrun: started %s (%ld args, stack %ld, pri %ld)\n", (ULONG)names[0], (LONG)n, stack, pri);
    Flush(Output());
    WaitPort(port);
    GetMsg(port);
    Printf("wbrun: %s returned its message\n", (ULONG)names[0]);
    rc = 0;
out:
    if (wb)
        FreeVec(wb);
    if (port)
        DeleteMsgPort(port);
    if (seg)
        UnLoadSeg(seg);
    for (i = 0; i < MAXARGS; i++)
        if (args[i].wa_Lock)
            UnLock(args[i].wa_Lock);
    if (first)
        FreeDiskObject(first);
    CloseLibrary(IconBase);
    return rc;
}
