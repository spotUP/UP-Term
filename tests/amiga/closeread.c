/* closeread [N] -- DV5 of the console.device plan: CloseDevice on the very
 * request that holds a CMD_READ still pending, as the 3.1 ROM con-handler
 * closes a CON: window (romprobe through CON: over UP-Term's device hung the
 * rig, 2026-10-08). N times (default 20): a window, a CONU_SNIPMAP unit,
 * SendIO(CMD_READ) with no key typed, CloseDevice on that request (no
 * AbortIO). Counts, after CloseDevice returns: the request coming back
 * with IOERR_ABORTED (UP-Term's unit aborted the read into the closer's
 * own reply port, so the closer went on and freed the unit while its
 * process still ran -- the hang), and the read replied to the caller's
 * port. The ROM device: neither. Prints
 * "CLOSEREAD n <N> aborted <a> replied <r>". */
#include <stdio.h>
#include <stdlib.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>

struct IntuitionBase *IntuitionBase;

int main(int argc, char **argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 20, i, aborted = 0, replied = 0;
    struct MsgPort *p = CreateMsgPort();
    struct IOStdReq *io = p ? (struct IOStdReq *)CreateIORequest(p, sizeof(struct IOStdReq)) : 0;
    static char buf[16];
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    if (!io || !IntuitionBase)
        return 20;
    for (i = 0; i < n; i++) {
        struct Window *w = OpenWindowTags(0, WA_Left, 0, WA_Top, 20, WA_Width, 400, WA_Height, 100,
                                          WA_Title, (ULONG)"closeread: CloseDevice with a read pending",
                                          WA_DragBar, TRUE, WA_SimpleRefresh, TRUE, WA_IDCMP, 0, TAG_DONE);
        if (!w)
            return 20;
        io->io_Data = w;
        io->io_Length = sizeof(struct Window);
        if (OpenDevice((STRPTR)"console.device", CONU_SNIPMAP, (struct IORequest *)io, 0)) {
            CloseWindow(w);
            return 20;
        }
        io->io_Command = CMD_READ;
        io->io_Data = buf;
        io->io_Length = sizeof(buf);
        SendIO((struct IORequest *)io);
        Delay(5);
        io->io_Error = 0;
        CloseDevice((struct IORequest *)io);
        aborted += io->io_Error == IOERR_ABORTED;
        Delay(5);
        while (GetMsg(p))
            replied++;
        CloseWindow(w);
    }
    printf("CLOSEREAD n %d aborted %d replied %d\n", n, aborted, replied);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(p);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
