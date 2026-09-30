/* chainprobe SECONDS -- DP1 of the console.device plan
 * (thoughts/shared/plans/2026-09-30-console-device.md): which input events
 * reach the input.device chain below Intuition, and where the ROM
 * console.device's own handler sits in it.
 *
 * Opens a SIMPLE_REFRESH window titled "chainprobe" with IDCMP 0 (what a
 * console.device unit's window looks like to Intuition: nobody asked for
 * messages) and a CON: window titled "chainprobe-con", adds handlers at
 * priorities 9, 5 and -5, and for SECONDS counts every event per class:
 * all of them, those whose ie_EventAddress is the probe window / the CON:
 * window, and those that arrive while the probe window / the CON: window is
 * IntuitionBase->ActiveWindow. The rig script (tools/rig/chainprobe_rig.py)
 * meanwhile types, clicks, drags, resizes, depth-arranges and closes.
 *
 * Prints input.device's handler list (name, priority, whose code: the
 * handler whose is_Code lies inside the console.device resident, or whose
 * is_Data is the console base, is the ROM console's), the count table, and
 * RESULT lines answering DD6 (the console handler's priority and name) and
 * DD9 (which window classes arrive for an IDCMP-less window).
 * Output also in RAM:chainprobe.log. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/resident.h>
#include <exec/interrupts.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include "probeout.h"

#define NPRI 3
#define NCLS 32                 /* classes above IECLASS_MAX land in the last */
enum { C_ALL, C_ADDR_PROBE, C_ADDR_CON, C_ACT_PROBE, C_ACT_CON, C_N };

struct IntuitionBase *IntuitionBase;
static struct Window *probewin, *conwin;
static volatile LONG counts[NPRI][NCLS][C_N];
static const BYTE pris[NPRI] = { 9, 5, -5 };
static char *const names[NPRI] = { "chainprobe 9", "chainprobe 5", "chainprobe -5" };

static struct InputEvent *count(__reg("a0") struct InputEvent *ev, __reg("a1") APTR data)
{
    volatile LONG (*c)[C_N] = (volatile LONG (*)[C_N])data;
    struct InputEvent *e;
    struct Window *act = IntuitionBase->ActiveWindow;
    for (e = ev; e; e = e->ie_NextEvent) {
        int k = e->ie_Class < NCLS ? e->ie_Class : NCLS - 1;
        c[k][C_ALL]++;
        if (probewin && e->ie_EventAddress == (APTR)probewin)
            c[k][C_ADDR_PROBE]++;
        if (conwin && e->ie_EventAddress == (APTR)conwin)
            c[k][C_ADDR_CON]++;
        if (probewin && act == probewin)
            c[k][C_ACT_PROBE]++;
        if (conwin && act == conwin)
            c[k][C_ACT_CON]++;
    }
    return ev;
}

static const char *class_name(int k)
{
    static const char *const n[] = {
        "NULL", "RAWKEY", "RAWMOUSE", "EVENT", "POINTERPOS", "05", "TIMER",
        "GADGETDOWN", "GADGETUP", "REQUESTER", "MENULIST", "CLOSEWINDOW",
        "SIZEWINDOW", "REFRESHWINDOW", "NEWPREFS", "DISKREMOVED",
        "DISKINSERTED", "ACTIVEWINDOW", "INACTIVEWINDOW", "NEWPOINTERPOS",
        "MENUHELP", "CHANGEWINDOW"
    };
    return k <= IECLASS_MAX ? n[k] : "ABOVE-MAX";
}

/* The ROM console's handler: code inside the console.device resident, or
 * data = the console base. The rest of the list is printed as found. */
static void handler_list(struct Interrupt *ours, struct Library *conbase, int final)
{
    struct Resident *rt = FindResident((STRPTR)"console.device");
    APTR lo = rt, hi = rt ? rt->rt_EndSkip : 0;
    struct Node *n = &ours->is_Node;
    int found = 0;
    Forbid();
    while (n->ln_Pred && n->ln_Pred->ln_Pred)
        n = n->ln_Pred;
    /* printing under Forbid() would break it; copy first */
    {
        static struct { BYTE pri; char name[40]; APTR code, data; } h[32];
        int nh = 0, i;
        for (; n->ln_Succ && nh < 32; n = n->ln_Succ, nh++) {
            struct Interrupt *irq = (struct Interrupt *)n;
            h[nh].pri = n->ln_Pri;
            strncpy(h[nh].name, n->ln_Name ? n->ln_Name : "(no name)", 39);
            h[nh].name[39] = 0;
            h[nh].code = (APTR)irq->is_Code;
            h[nh].data = irq->is_Data;
        }
        Permit();
        po_line("input.device handlers (%s), console.device resident %08lx-%08lx, base %08lx:\n",
                final ? "at the end" : "at the start", (ULONG)lo, (ULONG)hi, (ULONG)conbase);
        for (i = 0; i < nh; i++) {
            int in_rt = rt && h[i].code >= lo && h[i].code < hi;
            int is_con = in_rt || (conbase && h[i].data == (APTR)conbase);
            po_line("  handler pri %4d code %08lx data %08lx %-24s%s%s\n", h[i].pri,
                    (ULONG)h[i].code, (ULONG)h[i].data, h[i].name,
                    in_rt ? " [code in console.device]" : "",
                    (conbase && h[i].data == (APTR)conbase) ? " [data = console base]" : "");
            if (is_con && !final) {
                po_line("RESULT console-handler pri %d name \"%s\"\n", h[i].pri, h[i].name);
                found++;
            }
        }
        if (!final)
            po_check(found == 1, "the ROM console.device handler is identified in the chain",
                     found ? "more than one candidate" : "no handler with console code or data");
    }
}

/* The window behind a CON: file handle: ACTION_DISK_INFO's id_VolumeNode. */
static struct Window *con_window(BPTR fh)
{
    struct InfoData *id = AllocMem(sizeof(*id), MEMF_PUBLIC | MEMF_CLEAR);
    struct FileHandle *h = (struct FileHandle *)BADDR(fh);
    struct Window *w = 0;
    if (id && h->fh_Type && DoPkt(h->fh_Type, ACTION_DISK_INFO, MKBADDR(id), 0, 0, 0, 0))
        w = (struct Window *)id->id_VolumeNode;
    if (id)
        FreeMem(id, sizeof(*id));
    return w;
}

int main(int argc, char **argv)
{
    struct MsgPort *port = 0;
    struct IOStdReq *io = 0, conio;
    struct Interrupt irq[NPRI];
    struct Library *conbase = 0;
    BPTR con = 0;
    LONG secs = argc > 1 ? atol(argv[1]) : 60;
    int i, k, added = 0;

    po_start("RAM:chainprobe.log");
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    if (!po_check(IntuitionBase != 0, "intuition.library 37", 0))
        return po_end();
    memset(&conio, 0, sizeof(conio));
    if (!OpenDevice((STRPTR)"console.device", (ULONG)CONU_LIBRARY, (struct IORequest *)&conio, 0))
        conbase = (struct Library *)conio.io_Device;
    po_check(conbase != 0, "console.device CONU_LIBRARY", 0);

    probewin = OpenWindowTags(0, WA_Left, 20, WA_Top, 30, WA_Width, 280, WA_Height, 120,
                              WA_MinWidth, 80, WA_MinHeight, 40, WA_MaxWidth, -1, WA_MaxHeight, -1,
                              WA_Title, (ULONG)"chainprobe", WA_IDCMP, 0,
                              WA_SimpleRefresh, TRUE, WA_SizeGadget, TRUE, WA_DragBar, TRUE,
                              WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, TAG_DONE);
    con = Open((STRPTR)"CON:320/30/280/120/chainprobe-con/CLOSE", MODE_NEWFILE);
    if (con) {
        conwin = con_window(con);
        Write(con, "chainprobe CON: window\n", 23);
    }
    if (!po_check(probewin && con && conwin, "the IDCMP-less window and the CON: window are open", 0))
        goto out;
    po_line("RESULT window chainprobe %d %d %d %d idcmp %08lx flags %08lx\n", probewin->LeftEdge,
            probewin->TopEdge, probewin->Width, probewin->Height, probewin->IDCMPFlags, probewin->Flags);
    po_line("RESULT window chainprobe-con %d %d %d %d idcmp %08lx flags %08lx\n", conwin->LeftEdge,
            conwin->TopEdge, conwin->Width, conwin->Height, conwin->IDCMPFlags, conwin->Flags);

    port = CreateMsgPort();
    io = (struct IOStdReq *)CreateIORequest(port, sizeof(*io));
    if (!po_check(io && !OpenDevice((STRPTR)"input.device", 0, (struct IORequest *)io, 0), "input.device", 0)) {
        if (io)
            DeleteIORequest((struct IORequest *)io);
        io = 0;
        goto out;
    }
    memset(irq, 0, sizeof(irq));
    for (i = 0; i < NPRI; i++) {
        irq[i].is_Code = (void (*)())count;
        irq[i].is_Data = (APTR)counts[i];
        irq[i].is_Node.ln_Pri = pris[i];
        irq[i].is_Node.ln_Name = names[i];
        io->io_Data = &irq[i];
        io->io_Command = IND_ADDHANDLER;
        if (!DoIO((struct IORequest *)io))
            added++;
    }
    po_check(added == NPRI, "handlers added at priorities 9, 5, -5", 0);
    handler_list(&irq[0], conbase, 0);
    po_line("READY counting for %ld s\n", secs);
    Delay(secs * 50);
    handler_list(&irq[0], conbase, 1);
    for (i = 0; i < NPRI; i++) {
        io->io_Data = &irq[i];
        io->io_Command = IND_REMHANDLER;
        DoIO((struct IORequest *)io);
    }

    /* the table: per class, per handler: all / addr=probe / addr=con / active=probe / active=con */
    po_line("class               |      pri 9 all/aP/aC/actP/actC |      pri 5 all/aP/aC/actP/actC |     pri -5 all/aP/aC/actP/actC\n");
    for (k = 0; k < NCLS; k++) {
        char row[400], *p = row;
        LONG any = 0;
        for (i = 0; i < NPRI; i++)
            any |= counts[i][k][C_ALL];
        if (!any)
            continue;
        p += sprintf(p, "%02x %-16s", k, class_name(k));
        for (i = 0; i < NPRI; i++)
            p += sprintf(p, " | %5ld/%4ld/%4ld/%5ld/%5ld", counts[i][k][C_ALL], counts[i][k][C_ADDR_PROBE],
                         counts[i][k][C_ADDR_CON], counts[i][k][C_ACT_PROBE], counts[i][k][C_ACT_CON]);
        po_line("%s\n", row);
    }
    po_check(counts[0][IECLASS_RAWKEY][C_ALL] > 0 && counts[0][IECLASS_RAWMOUSE][C_ALL] > 0,
             "keys and mouse reached the chain (the rig drove the machine)", 0);

    /* DD9 / DD8 / DD7: window classes addressed to the IDCMP-less window */
    {
        static const int wc[] = { IECLASS_SIZEWINDOW, IECLASS_REFRESHWINDOW, IECLASS_CLOSEWINDOW,
                                  IECLASS_ACTIVEWINDOW, IECLASS_INACTIVEWINDOW, IECLASS_CHANGEWINDOW,
                                  IECLASS_GADGETDOWN, IECLASS_GADGETUP };
        for (i = 0; i < (int)(sizeof(wc) / sizeof(wc[0])); i++)
            po_line("RESULT idcmp0-window %s addressed pri9 %ld pri5 %ld pri-5 %ld\n", class_name(wc[i]),
                    counts[0][wc[i]][C_ADDR_PROBE], counts[1][wc[i]][C_ADDR_PROBE], counts[2][wc[i]][C_ADDR_PROBE]);
        for (i = 0; i < (int)(sizeof(wc) / sizeof(wc[0])); i++)
            po_line("RESULT con-window %s addressed pri9 %ld pri5 %ld pri-5 %ld\n", class_name(wc[i]),
                    counts[0][wc[i]][C_ADDR_CON], counts[1][wc[i]][C_ADDR_CON], counts[2][wc[i]][C_ADDR_CON]);
    }
    /* R1: keys and mouse while each window is active; a drop between two
     * priorities is a handler in between consuming them (the ROM console) */
    po_line("RESULT rawkey active=idcmp0 pri9 %ld pri5 %ld pri-5 %ld\n", counts[0][1][C_ACT_PROBE],
            counts[1][1][C_ACT_PROBE], counts[2][1][C_ACT_PROBE]);
    po_line("RESULT rawkey active=con pri9 %ld pri5 %ld pri-5 %ld\n", counts[0][1][C_ACT_CON],
            counts[1][1][C_ACT_CON], counts[2][1][C_ACT_CON]);
    po_line("RESULT rawmouse active=idcmp0 pri9 %ld pri5 %ld pri-5 %ld\n", counts[0][2][C_ACT_PROBE],
            counts[1][2][C_ACT_PROBE], counts[2][2][C_ACT_PROBE]);
    po_line("RESULT timer pri9 %ld pri5 %ld pri-5 %ld\n", counts[0][IECLASS_TIMER][C_ALL],
            counts[1][IECLASS_TIMER][C_ALL], counts[2][IECLASS_TIMER][C_ALL]);

out:
    if (io) {
        CloseDevice((struct IORequest *)io);
        DeleteIORequest((struct IORequest *)io);
    }
    if (port)
        DeleteMsgPort(port);
    if (con)
        Close(con);
    if (probewin)
        CloseWindow(probewin);
    if (conbase)
        CloseDevice((struct IORequest *)&conio);
    CloseLibrary((struct Library *)IntuitionBase);
    return po_end();
}
