/* wblaunch.c -- see wblaunch.h. The launch rules, as Workbench 3.x does them:
 * - NAME's icon a project (WBPROJECT): its default tool is the program, looked up from the project's
 *   drawer, then the CLI path, then C:; the arguments are the tool, then the project. Stack and TOOLPRI
 *   from the project icon.
 * - otherwise NAME is the tool itself; further names are the icons shift-selected with it. Stack and
 *   TOOLPRI from the tool's icon, 4096 bytes without one.
 * - the process: no Input()/Output(), no console task, no window pointer, PROGDIR: the tool's drawer;
 *   sm_ToolWindow from the tool icon's TOOLWINDOW. */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <proto/wb.h>
#include "wblaunch.h"

#define MAXARGS 8
#define MINSTACK 4096

struct Library *IconBase;
struct Library *WorkbenchBase;

struct wb_path { BPTR next, lock; }; /* a cli_CommandDir node */

/* NAME as a WBArg: its drawer and its name in it. A project that is only an icon (SYS:System/Shell)
 * is found by its .info, as Workbench hands it over. */
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

static void fail(char *err, int errsz, const char *what, const char *name)
{
    if (err && errsz > 0)
        snprintf(err, errsz, "%s%s%s", what, name ? " " : "", name ? name : "");
}

int wbl_start(const char *const *names_in, int count, int wait, char *err, int errsz)
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
    int n = 0, i, rc = -1, started = 0;

    memset(args, 0, sizeof args);
    if (count < 1 || count > MAXARGS) {
        fail(err, errsz, "too many icons", 0);
        return -1;
    }
    if (!(IconBase = OpenLibrary((STRPTR) "icon.library", 36))) {
        fail(err, errsz, "no icon.library", 0);
        return -1;
    }
    first = GetDiskObject((STRPTR)names_in[0]);
    if (first && first->do_Type == WBPROJECT) {
        /* args[0] the default tool, args[1] the project */
        BPTR pdir = parent_and_name(names_in[0], names[1], sizeof(names[1]));
        if (!pdir || !first->do_DefaultTool || !*first->do_DefaultTool) {
            fail(err, errsz, "a project without a default tool:", names_in[0]);
            goto out;
        }
        args[1].wa_Lock = pdir;
        args[1].wa_Name = (STRPTR)names[1];
        n = 2;
        seg = load_default_tool(pdir, (char *)first->do_DefaultTool, &args[0].wa_Lock, names[0],
                                sizeof(names[0]));
        if (!seg) {
            fail(err, errsz, "the default tool was not found:", (const char *)first->do_DefaultTool);
            goto out;
        }
        icon = first;
    } else {
        /* args[0] the tool, then the shift-selected icons */
        seg = LoadSeg((STRPTR)names_in[0]);
        if (!seg || !(args[0].wa_Lock = parent_and_name(names_in[0], names[0], sizeof(names[0])))) {
            fail(err, errsz, "not a program:", names_in[0]);
            goto out;
        }
        n = 1;
        icon = first;
    }
    args[0].wa_Name = (STRPTR)names[0];
    for (i = 1; i < count; i++, n++) {
        if (!(args[n].wa_Lock = parent_and_name(names_in[i], names[n], sizeof(names[n])))) {
            fail(err, errsz, "not found:", names_in[i]);
            goto out;
        }
        args[n].wa_Name = (STRPTR)names[n];
    }

    stack = icon && icon->do_StackSize > MINSTACK ? icon->do_StackSize : MINSTACK;
    pri = tooltype_num(icon, "TOOLPRI", 0);
    if (pri < -128) pri = -128;
    if (pri > 127) pri = 127;
    toolwin = icon ? FindToolType((STRPTR *)icon->do_ToolTypes, (STRPTR) "TOOLWINDOW") : 0;

    wb = AllocVec(sizeof(*wb), MEMF_PUBLIC | MEMF_CLEAR);
    port = wait ? CreateMsgPort() : 0;
    if (!wb || (wait && !port)) {
        fail(err, errsz, "out of memory", 0);
        goto out;
    }
    /* No NP_CurrentDir: dos does not unlock it when the process ends. Workbench gives none either;
     * the program's startup code goes to wa_Lock. NP_HomeDir (PROGDIR:) is unlocked at exit. */
    p = CreateNewProcTags(NP_Seglist, seg, NP_FreeSeglist, FALSE, NP_Name, (ULONG)names[0],
                          NP_StackSize, stack, NP_Priority, pri,
                          NP_Input, 0, NP_Output, 0, NP_CloseInput, FALSE, NP_CloseOutput, FALSE,
                          NP_ConsoleTask, 0, NP_WindowPtr, 0,
                          NP_CurrentDir, 0, NP_HomeDir, DupLock(args[0].wa_Lock),
                          NP_Cli, FALSE, TAG_END);
    if (!p) {
        fail(err, errsz, "no process for", names[0]);
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
    started = 1;
    rc = 0;
    if (wait) {
        WaitPort(port);
        GetMsg(port);
    }
out:
    /* Without wait the program still owns the message, the segment and the locks after we return:
     * they are left to it (a small leak in a short-lived launcher, not a crash). */
    if (!started || wait) {
        if (wb)
            FreeVec(wb);
        if (seg)
            UnLoadSeg(seg);
        for (i = 0; i < MAXARGS; i++)
            if (args[i].wa_Lock)
                UnLock(args[i].wa_Lock);
    }
    if (port)
        DeleteMsgPort(port);
    if (first)
        FreeDiskObject(first);
    CloseLibrary(IconBase);
    return rc;
}

int wbl_open(const char *path, char *err, int errsz)
{
    int ok = 0;
    if ((WorkbenchBase = OpenLibrary((STRPTR) "workbench.library", 44))) {
        ok = OpenWorkbenchObjectA((STRPTR)path, 0) ? 1 : 0;
        CloseLibrary(WorkbenchBase);
    }
    if (ok)
        return 0;
    return wbl_start(&path, 1, 0, err, errsz);
}
