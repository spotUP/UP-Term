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
#include "leaf.h"

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

/* One launch's memory: the WBStartup message (first, so a reply's pointer is the record), the argument
 * list it points to and the names those point to. Freed by wbl_free once the program replied. */
struct wbl_rec {
    struct WBStartup wb;
    struct WBArg args[MAXARGS];
    char names[MAXARGS][108];
    BPTR seg;
};

void wbl_free(struct WBStartup *msg)
{
    struct wbl_rec *r = (struct wbl_rec *)msg;
    int i;
    if (!r)
        return;
    if (r->seg)
        UnLoadSeg(r->seg);
    for (i = 0; i < MAXARGS; i++)
        if (r->args[i].wa_Lock)
            UnLock(r->args[i].wa_Lock);
    FreeVec(r);
}

int wbl_start(const char *const *names_in, int count, struct MsgPort *reply, char *err, int errsz)
{
    struct DiskObject *first, *icon;
    struct wbl_rec *r = 0;
    struct MsgPort *port = 0;
    struct Process *p;
    STRPTR toolwin;
    LONG stack, pri;
    int n = 0, i, rc = -1, started = 0;

    if (count < 1 || count > MAXARGS) {
        fail(err, errsz, "too many icons", 0);
        return -1;
    }
    if (!(IconBase = OpenLibrary((STRPTR) "icon.library", 36))) {
        fail(err, errsz, "no icon.library", 0);
        return -1;
    }
    r = AllocVec(sizeof(*r), MEMF_PUBLIC | MEMF_CLEAR);
    port = reply ? 0 : CreateMsgPort();
    if (!r || (!reply && !port)) {
        fail(err, errsz, "out of memory", 0);
        first = 0;
        goto out;
    }
    first = GetDiskObject((STRPTR)names_in[0]);
    if (first && first->do_Type == WBPROJECT) {
        /* args[0] the default tool, args[1] the project */
        BPTR pdir = parent_and_name(names_in[0], r->names[1], sizeof(r->names[1]));
        if (!pdir || !first->do_DefaultTool || !*first->do_DefaultTool) {
            fail(err, errsz, "a project without a default tool:", names_in[0]);
            goto out;
        }
        r->args[1].wa_Lock = pdir;
        r->args[1].wa_Name = (STRPTR)r->names[1];
        n = 2;
        r->seg = load_default_tool(pdir, (char *)first->do_DefaultTool, &r->args[0].wa_Lock, r->names[0],
                                   sizeof(r->names[0]));
        if (!r->seg) {
            fail(err, errsz, "the default tool was not found:", (const char *)first->do_DefaultTool);
            goto out;
        }
        icon = first;
    } else {
        /* args[0] the tool, then the shift-selected icons */
        r->seg = LoadSeg((STRPTR)names_in[0]);
        if (!r->seg || !(r->args[0].wa_Lock = parent_and_name(names_in[0], r->names[0], sizeof(r->names[0])))) {
            fail(err, errsz, "not a program:", names_in[0]);
            goto out;
        }
        n = 1;
        icon = first;
    }
    r->args[0].wa_Name = (STRPTR)r->names[0];
    for (i = 1; i < count; i++, n++) {
        if (!(r->args[n].wa_Lock = parent_and_name(names_in[i], r->names[n], sizeof(r->names[n])))) {
            fail(err, errsz, "not found:", names_in[i]);
            goto out;
        }
        r->args[n].wa_Name = (STRPTR)r->names[n];
    }

    stack = icon && icon->do_StackSize > MINSTACK ? icon->do_StackSize : MINSTACK;
    pri = tooltype_num(icon, "TOOLPRI", 0);
    if (pri < -128) pri = -128;
    if (pri > 127) pri = 127;
    toolwin = icon ? FindToolType((STRPTR *)icon->do_ToolTypes, (STRPTR) "TOOLWINDOW") : 0;

    /* No NP_CurrentDir: dos does not unlock it when the process ends. Workbench gives none either;
     * the program's startup code goes to wa_Lock. NP_HomeDir (PROGDIR:) is unlocked at exit. */
    p = CreateNewProcTags(NP_Seglist, r->seg, NP_FreeSeglist, FALSE, NP_Name, (ULONG)r->names[0],
                          NP_StackSize, stack, NP_Priority, pri,
                          NP_Input, 0, NP_Output, 0, NP_CloseInput, FALSE, NP_CloseOutput, FALSE,
                          NP_ConsoleTask, 0, NP_WindowPtr, 0,
                          NP_CurrentDir, 0, NP_HomeDir, DupLock(r->args[0].wa_Lock),
                          NP_Cli, FALSE, TAG_END);
    if (!p) {
        fail(err, errsz, "no process for", r->names[0]);
        goto out;
    }
    r->wb.sm_Message.mn_ReplyPort = reply ? reply : port;
    r->wb.sm_Message.mn_Length = sizeof(r->wb);
    r->wb.sm_Process = &p->pr_MsgPort;
    r->wb.sm_Segment = r->seg;
    r->wb.sm_NumArgs = n;
    r->wb.sm_ToolWindow = toolwin;
    r->wb.sm_ArgList = r->args;
    PutMsg(&p->pr_MsgPort, &r->wb.sm_Message);
    started = 1;
    rc = 0;
    if (!reply) {
        WaitPort(port);
        GetMsg(port);
        wbl_free(&r->wb);
    }
out:
    /* With a reply port the program owns the record until its reply reaches that port: the caller
     * gets it there and calls wbl_free. Never a port that is deleted while a launch is out. */
    if (!started)
        wbl_free(r ? &r->wb : 0);
    if (port)
        DeleteMsgPort(port);
    if (first)
        FreeDiskObject(first);
    CloseLibrary(IconBase);
    return rc;
}

/* The request to the running Amarchy (it keeps the reply port of its launches, so the WBStartup
 * message outlives this process). 1 and *rc/err set when Amarchy answered, 0 when it is not there. */
static int ask_amarchy(const char *path, char *err, int errsz, int *rc)
{
    struct wbl_msg { struct Message msg; lf_launch b; } m;
    struct MsgPort *rp, *port;
    if (!lf_launch_fill(&m.b, path) || !(rp = CreateMsgPort()))
        return 0;
    memset(&m.msg, 0, sizeof m.msg);
    m.msg.mn_ReplyPort = rp;
    m.msg.mn_Length = sizeof m;
    Forbid();                       /* find and put under one Forbid: the port cannot go between */
    port = FindPort((STRPTR)LF_LAUNCH_PORT);
    if (port)
        PutMsg(port, &m.msg);
    Permit();
    if (port) {
        WaitPort(rp);
        GetMsg(rp);
        *rc = (int)m.b.rc;
        fail(err, errsz, m.b.err, 0);
    }
    DeleteMsgPort(rp);
    return port != 0;
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
    if (ask_amarchy(path, err, errsz, &ok))
        return ok;
    /* No Amarchy: stay alive until the program replied, so nothing is freed under it */
    return wbl_start(&path, 1, 0, err, errsz);
}
