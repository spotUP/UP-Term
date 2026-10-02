/* wbrun TOOL [PROJECT] -- start TOOL as Workbench starts it: a process
 * of its own with a WBStartup message (the tool, then the project icon
 * the user double-clicked), waiting for the message to come back. For
 * P8's UP-Term icon: vsh's Workbench start on the rig, without clicking. */
#include <string.h>
#include <exec/memory.h>
#include <dos/dostags.h>
#include <workbench/startup.h>
#include <proto/exec.h>
#include <proto/dos.h>

static BPTR parent_and_name(const char *path, char *name)
{
    BPTR lock = Lock((STRPTR)path, SHARED_LOCK), dir;
    if (!lock) {
        /* a project that is only an icon (SYS:System/Shell): Workbench
         * hands it over by its .info's drawer and the name without .info */
        char info[120];
        if (strlen(path) + 6 > sizeof(info))
            return 0;
        strcpy(info, path);
        strcat(info, ".info");
        lock = Lock((STRPTR)info, SHARED_LOCK);
    }
    if (!lock)
        return 0;
    strcpy(name, (const char *)FilePart((STRPTR)path));
    dir = ParentDir(lock);
    UnLock(lock);
    return dir;
}

int main(int argc, char **argv)
{
    static char tname[108], pname[108];
    struct WBStartup *wb;
    struct WBArg *args;
    struct MsgPort *port;
    struct Process *p;
    BPTR seg, tdir, pdir = 0;
    int n = argc > 2 ? 2 : 1;
    if (argc < 2)
        return 20;
    port = CreateMsgPort();
    wb = AllocVec(sizeof(*wb), MEMF_PUBLIC | MEMF_CLEAR);
    args = AllocVec(sizeof(struct WBArg) * 2, MEMF_PUBLIC | MEMF_CLEAR);
    seg = LoadSeg((STRPTR)argv[1]);
    tdir = parent_and_name(argv[1], tname);
    if (argc > 2)
        pdir = parent_and_name(argv[2], pname);
    if (!port || !wb || !args || !seg || !tdir || (argc > 2 && !pdir)) {
        Printf("wbrun: cannot set up\n");
        return 20;
    }
    args[0].wa_Lock = tdir;
    args[0].wa_Name = (BYTE *)tname;
    args[1].wa_Lock = pdir;
    args[1].wa_Name = (BYTE *)pname;
    p = CreateNewProcTags(NP_Seglist, seg, NP_FreeSeglist, FALSE, NP_Name, (ULONG)tname,
                          NP_StackSize, 65536, NP_Input, 0, NP_Output, 0, NP_CloseInput, FALSE,
                          NP_CloseOutput, FALSE, NP_CurrentDir, 0, TAG_END);
    if (!p) {
        Printf("wbrun: no process\n");
        return 20;
    }
    wb->sm_Message.mn_ReplyPort = port;
    wb->sm_Message.mn_Length = sizeof(*wb);
    wb->sm_Process = &p->pr_MsgPort;
    wb->sm_Segment = seg;
    wb->sm_NumArgs = n;
    wb->sm_ArgList = args;
    PutMsg(&p->pr_MsgPort, &wb->sm_Message);
    WaitPort(port);
    GetMsg(port);
    Printf("wbrun: %s returned its message\n", tname);
    UnLoadSeg(seg);
    UnLock(tdir);
    if (pdir)
        UnLock(pdir);
    FreeVec(args);
    FreeVec(wb);
    DeleteMsgPort(port);
    return 0;
}
