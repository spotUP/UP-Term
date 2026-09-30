/* cudump -- D3.2 of the console.device plan: the public struct ConUnit of a
 * console unit, field by field, after a fixed set of writes. Run once with
 * the ROM device and once with UP-Term's (UPConsole DEVICE ON): every line
 * should be equal; tools/rig/cudump_rig.py compares them.
 *
 * A 500 x 150 SIMPLE_REFRESH window at 10,20 with a CONU_SNIPMAP unit; the
 * writes: text, a tab stop set (ESC H) at column 20, auto-wrap off (CSI ?7l,
 * the Amiga's AWM reset) and on again, raw events 11 and 12 asked for
 * (CSI 11;12{), linefeed mode set (CSI 20h), the cursor to row 3 column 7
 * (CSI 3;7H). Pointer fields print as "set" or "0" (their values differ
 * between runs by nature), cu_Font as "window font" when it is the window's. */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>

struct IntuitionBase *IntuitionBase;

static void w(struct IOStdReq *io, const char *s)
{
    io->io_Command = CMD_WRITE;
    io->io_Data = (APTR)s;
    io->io_Length = (ULONG)-1;
    DoIO((struct IORequest *)io);
}

int main(void)
{
    struct Window *win;
    struct MsgPort *p = CreateMsgPort();
    struct IOStdReq *io = p ? (struct IOStdReq *)CreateIORequest(p, sizeof(struct IOStdReq)) : 0;
    struct ConUnit *cu;
    int i;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    if (!io || !IntuitionBase)
        return 20;
    win = OpenWindowTags(0, WA_Left, 10, WA_Top, 20, WA_Width, 500, WA_Height, 150, WA_Title, (ULONG)"cudump",
                         WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_SizeGadget, TRUE, WA_SimpleRefresh, TRUE,
                         WA_IDCMP, 0, TAG_DONE);
    if (!win)
        return 20;
    io->io_Data = win;
    io->io_Length = sizeof(struct Window);
    if (OpenDevice((STRPTR)"console.device", CONU_SNIPMAP, (struct IORequest *)io, 0)) {
        printf("OpenDevice failed %d\n", io->io_Error);
        CloseWindow(win);
        return 20;
    }
    cu = (struct ConUnit *)io->io_Unit;
    w(io, "cudump: the ConUnit after fixed writes\n");
    w(io, "\x9b" "20G" "\x1bH");            /* column 20, set a tab stop there */
    w(io, "\r\x9b" "?7l" "\x9b" "?7h");      /* auto-wrap off and on */
    w(io, "\x9b" "11;12{");                  /* raw events: close gadget, resize */
    w(io, "\x9b" "20h");                     /* linefeed mode */
    w(io, "\x9b" "3;7H");                    /* row 3, column 7 */
    Delay(25);                                /* a unit process has drawn by then */
    printf("cu_Window %s\n", cu->cu_Window == win ? "the window" : cu->cu_Window ? "other" : "0");
    printf("cu_XCP %d cu_YCP %d\n", cu->cu_XCP, cu->cu_YCP);
    printf("cu_XMax %d cu_YMax %d\n", cu->cu_XMax, cu->cu_YMax);
    printf("cu_XRSize %d cu_YRSize %d\n", cu->cu_XRSize, cu->cu_YRSize);
    printf("cu_XROrigin %d cu_YROrigin %d\n", cu->cu_XROrigin, cu->cu_YROrigin);
    printf("cu_XRExtant %d cu_YRExtant %d\n", cu->cu_XRExtant, cu->cu_YRExtant);
    printf("cu_XMinShrink %d cu_YMinShrink %d\n", cu->cu_XMinShrink, cu->cu_YMinShrink);
    printf("cu_XCCP %d cu_YCCP %d\n", cu->cu_XCCP, cu->cu_YCCP);
    printf("cu_KeyMapStruct lo %s hi %s\n", cu->cu_KeyMapStruct.km_LoKeyMap ? "set" : "0",
           cu->cu_KeyMapStruct.km_HiKeyMap ? "set" : "0");
    printf("cu_TabStops");
    for (i = 0; i < MAXTABS; i++)
        printf(" %u", (unsigned)cu->cu_TabStops[i]);
    printf("\n");
    printf("cu_Mask %u cu_FgPen %u cu_BgPen %u cu_AOLPen %u cu_DrawMode %u\n", (unsigned)cu->cu_Mask,
           (unsigned)cu->cu_FgPen, (unsigned)cu->cu_BgPen, (unsigned)cu->cu_AOLPen, (unsigned)cu->cu_DrawMode);
    printf("cu_Font %s\n", cu->cu_Font == win->RPort->Font ? "window font" : cu->cu_Font ? "other" : "0");
    printf("cu_AlgoStyle %u cu_TxFlags %u cu_TxHeight %u cu_TxWidth %u cu_TxBaseline %u cu_TxSpacing %d\n",
           (unsigned)cu->cu_AlgoStyle, (unsigned)cu->cu_TxFlags, (unsigned)cu->cu_TxHeight,
           (unsigned)cu->cu_TxWidth, (unsigned)cu->cu_TxBaseline, (int)cu->cu_TxSpacing);
    printf("cu_Modes %02x %02x %02x\n", cu->cu_Modes[0], cu->cu_Modes[1], cu->cu_Modes[2]);
    printf("cu_RawEvents %02x %02x %02x\n", cu->cu_RawEvents[0], cu->cu_RawEvents[1], cu->cu_RawEvents[2]);
    CloseDevice((struct IORequest *)io);
    CloseWindow(win);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(p);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
