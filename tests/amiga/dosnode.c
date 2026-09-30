/* dosnode [NAME...] -- DP3 of the console.device plan
 * (thoughts/shared/plans/2026-09-30-console-device.md): how the running
 * dos.library made its CON:, RAW: and (when mounted) XCON: DosList entries,
 * so `UPConsole CON ON` (DD19) points them at L:vtcon-handler the same way
 * and DD21 can tell a pristine entry from a third-party console's.
 *
 * Per entry (CON, RAW, XCON and any NAME given): dn_Type, dn_Task,
 * dn_Handler, dn_StackSize, dn_Priority, dn_Startup, dn_SegList, dn_GlobalVec,
 * and where the seglist lives: inside which exec resident module (ROM or a
 * ROM-update module), whether DOS's resident segment list holds it (and
 * under which name), whether the memory is RAM or ROM. Then one PRISTINE
 * line per entry, a C initializer for the table in device/upconsole.c, and
 * a short list of every DLT_DEVICE entry (third-party consoles show there).
 * Needs no input. Output also in RAM:dosnode.log. */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/resident.h>
#include <exec/execbase.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "probeout.h"

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

/* A BSTR as a C string, or "" (BSTRs are not NUL-terminated). */
static const char *bstr(BSTR b, char *buf, int max)
{
    UBYTE *p = (UBYTE *)BADDR(b);
    int n;
    buf[0] = 0;
    if (!p)
        return buf;
    n = p[0] < max - 1 ? p[0] : max - 1;
    memcpy(buf, p + 1, n);
    buf[n] = 0;
    return buf;
}

/* The exec resident module whose [romtag, rt_EndSkip) holds addr. */
static struct Resident *resident_of(APTR addr)
{
    ULONG *rm = (ULONG *)SysBase->ResModules;
    while (rm && *rm) {
        if (*rm & 0x80000000) {
            rm = (ULONG *)(*rm & 0x7FFFFFFF);
            continue;
        }
        {
            struct Resident *rt = (struct Resident *)*rm;
            if ((APTR)rt <= addr && addr < rt->rt_EndSkip)
                return rt;
        }
        rm++;
    }
    return 0;
}

/* The DOS resident segment (di_ResList) whose seg_Seg is seglist:
 * its name into buf and its seg_UC, or 0. */
static int resseg_of(BPTR seglist, char *buf, int max, LONG *uc)
{
    struct RootNode *root = (struct RootNode *)DOSBase->dl_Root;
    struct DosInfo *di = (struct DosInfo *)BADDR(root->rn_Info);
    struct Segment *s;
    int found = 0;
    buf[0] = 0;
    Forbid();
    for (s = (struct Segment *)BADDR(di->di_ResList); s; s = (struct Segment *)BADDR(s->seg_Next))
        if (s->seg_Seg == seglist) {
            int n = s->seg_Name[0] < max - 1 ? s->seg_Name[0] : max - 1;
            memcpy(buf, &s->seg_Name[1], n);
            buf[n] = 0;
            *uc = s->seg_UC;
            found = 1;
            break;
        }
    Permit();
    return found;
}

static const char *memkind(APTR p)
{
    if (!p)
        return "none";
    return TypeOfMem(p) ? ((TypeOfMem(p) & MEMF_CHIP) ? "chip" : "fast") : "rom";
}

/* One entry in detail, and its PRISTINE line. Fields are copied under
 * the DosList lock and printed after it: output may need DOS itself. */
static int dump(const char *name)
{
    struct DosList *dl;
    struct DeviceNode dn;
    char handler[80], segname[40], rtname[40], startup[80];
    APTR seg;
    struct Resident *rt;
    LONG uc = 0;
    int inres;
    dl = FindDosEntry(LockDosList(LDF_DEVICES | LDF_READ), (STRPTR)name, LDF_DEVICES);
    if (dl) {
        dn = *(struct DeviceNode *)dl;
        bstr(dn.dn_Handler, handler, sizeof(handler));
        if ((ULONG)dn.dn_Startup >= 0x10000)
            sprintf(startup, "%08lx bstr \"%s\"", (ULONG)dn.dn_Startup, bstr((BSTR)dn.dn_Startup, segname, 30));
        else
            sprintf(startup, "%ld", (long)dn.dn_Startup);
    }
    UnLockDosList(LDF_DEVICES | LDF_READ);
    if (!dl) {
        po_line("entry %s: not in the DosList\n", name);
        return 0;
    }
    seg = dn.dn_SegList ? BADDR(dn.dn_SegList) : 0;
    rt = seg ? resident_of(seg) : 0;
    rtname[0] = 0;
    if (rt && rt->rt_Name) {
        strncpy(rtname, (char *)rt->rt_Name, 39);
        rtname[39] = 0;
    }
    inres = seg ? resseg_of(dn.dn_SegList, segname, sizeof(segname), &uc) : 0;
    if (!inres)
        segname[0] = 0;
    po_line("entry %s: type %ld task %08lx lock %08lx handler \"%s\" stack %ld pri %ld startup %s\n",
            name, (long)dn.dn_Type, (ULONG)dn.dn_Task, (ULONG)dn.dn_Lock, handler,
            (long)dn.dn_StackSize, (long)dn.dn_Priority, startup);
    po_line("entry %s: seglist %08lx (%s memory) resident-module \"%s\" dos-resident %s \"%s\" uc %ld globvec %ld\n",
            name, (ULONG)seg, memkind(seg), rtname, inres ? "yes" : "no", segname, (long)uc,
            (long)dn.dn_GlobalVec);
    po_line("PRISTINE { %d, \"%s\", %ld, \"%s\", %ld, %ld, %ld, %s, \"%s\", %ld },\n",
            DOSBase->dl_lib.lib_Version, name, (long)dn.dn_Type, handler, (long)dn.dn_StackSize,
            (long)dn.dn_Priority, (long)((ULONG)dn.dn_Startup < 0x10000 ? dn.dn_Startup : -1),
            !seg ? "SEG_NONE" : inres ? "SEG_DOSRES" : rt ? "SEG_ROMTAG" : "SEG_OTHER",
            inres ? segname : rtname, (long)dn.dn_GlobalVec);
    return 1;
}

int main(int argc, char **argv)
{
    struct Library *con;
    struct DosList *dl;
    int i;
    po_start("RAM:dosnode.log");
    con = (struct Library *)FindName(&SysBase->DeviceList, (STRPTR)"console.device");
    po_line("RESULT versions exec %d.%d kickstart %d.%d dos %d.%d console.device %d.%d\n",
            SysBase->LibNode.lib_Version, SysBase->LibNode.lib_Revision,
            SysBase->LibNode.lib_Version, SysBase->SoftVer,
            DOSBase->dl_lib.lib_Version, DOSBase->dl_lib.lib_Revision,
            con ? con->lib_Version : 0, con ? con->lib_Revision : 0);
    po_check(dump("CON"), "CON is in the DosList", 0);
    po_check(dump("RAW"), "RAW is in the DosList", 0);
    dump("XCON");
    for (i = 1; i < argc; i++)
        dump(argv[i]);

    /* every device entry, short: a third-party console shows here */
    {
        static char lines[64][120];
        int n = 0;
        dl = LockDosList(LDF_DEVICES | LDF_READ);
        while ((dl = NextDosEntry(dl, LDF_DEVICES)) != 0 && n < 64) {
            char nm[40], h[60];
            struct DeviceNode *dn = (struct DeviceNode *)dl;
            sprintf(lines[n++], "  device %-12s handler \"%s\" seglist %08lx startup %08lx task %08lx",
                    bstr(dn->dn_Name, nm, 40), bstr(dn->dn_Handler, h, 60),
                    (ULONG)BADDR(dn->dn_SegList), (ULONG)dn->dn_Startup, (ULONG)dn->dn_Task);
        }
        UnLockDosList(LDF_DEVICES | LDF_READ);
        po_line("DLT_DEVICE entries (%d):\n", n);
        for (i = 0; i < n; i++)
            po_line("%s\n", lines[i]);
    }
    return po_end();
}
