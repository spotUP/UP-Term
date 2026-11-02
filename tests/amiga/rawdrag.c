/* rawdrag: a title-bar drag made of the events a real mouse sends (IECLASS_RAWMOUSE with relative
 * movement and the left-button qualifier held), for proving SolidWin on a rig. wasabi's mouse command
 * moves with IECLASS_POINTERPOS (absolute), which SolidWin does not follow.
 * Usage: rawdrag DX DY STEPS [HOLD]   pointer must already sit on the title bar.
 * Presses, moves half the steps, and with HOLD waits until the file RAM:rawdrag.go exists (the
 * mid-drag screenshot is taken then), moves the rest, releases. */
#include <exec/types.h>
#include <exec/io.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdlib.h>
#include <string.h>

static struct IOStdReq *io;

static void send(UWORD code, UWORD qual, WORD dx, WORD dy)
{
    struct InputEvent ie;
    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_RAWMOUSE;
    ie.ie_Code = code;
    ie.ie_Qualifier = qual;
    ie.ie_X = dx;
    ie.ie_Y = dy;
    io->io_Command = IND_WRITEEVENT;
    io->io_Data = &ie;
    io->io_Length = sizeof(ie);
    DoIO((struct IORequest *)io);
}

int main(int argc, char **argv)
{
    struct MsgPort *mp;
    int dx, dy, n, i;
    if (argc < 4) return 10;
    dx = atoi(argv[1]); dy = atoi(argv[2]); n = atoi(argv[3]);
    if (n < 2) return 10;
    mp = CreateMsgPort();
    if (!mp) return 20;
    io = (struct IOStdReq *)CreateIORequest(mp, sizeof(*io));
    if (!io || OpenDevice("input.device", 0, (struct IORequest *)io, 0)) return 20;
    send(IECODE_LBUTTON, IEQUALIFIER_LEFTBUTTON, 0, 0);
    Delay(10);
    for (i = 0; i < n; i++) {
        send(IECODE_NOBUTTON, IEQUALIFIER_LEFTBUTTON, dx / n, dy / n);
        Delay(4);
        if (i == n / 2 - 1 && argc > 4) {
            BPTR f;
            while (!(f = Lock("RAM:rawdrag.go", ACCESS_READ))) Delay(10);
            UnLock(f);
        }
    }
    Delay(10);
    send(IECODE_LBUTTON | IECODE_UP_PREFIX, 0, 0, 0);
    CloseDevice((struct IORequest *)io);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(mp);
    return 0;
}
