/* memprobe STANDARD|CHARMAP -- DV4 of the console.device plan: the memory a
 * console unit costs. A window for 80 x 25 cells of the screen's font, free
 * memory (AvailMem MEMF_ANY) before OpenDevice, after it, after 300 lines
 * written (the CHARMAP scrollback full at 200), after CloseDevice. Prints
 * "MEM <unit> open <bytes> full <bytes> leak <bytes>". */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>

struct IntuitionBase *IntuitionBase;

int main(int argc, char **argv)
{
    int charmap = argc > 1 && !strcmp(argv[1], "CHARMAP"), i;
    struct Screen *s;
    struct Window *w;
    struct MsgPort *p = CreateMsgPort();
    struct IOStdReq *io = p ? (struct IOStdReq *)CreateIORequest(p, sizeof(struct IOStdReq)) : 0;
    ULONG a0, a1, a2, a3;
    WORD fw, fh;
    char line[64];
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    s = LockPubScreen(0);
    if (!io || !s)
        return 20;
    fw = s->RastPort.TxWidth;
    fh = s->RastPort.TxHeight;
    w = OpenWindowTags(0, WA_Left, 0, WA_Top, 20, WA_InnerWidth, 80 * fw, WA_InnerHeight, 25 * fh,
                       WA_Title, (ULONG)"memprobe", WA_DragBar, TRUE, WA_SimpleRefresh, (ULONG)charmap,
                       WA_PubScreen, (ULONG)s, WA_IDCMP, 0, TAG_DONE);
    UnlockPubScreen(0, s);
    if (!w)
        return 20;
    Delay(10);
    a0 = AvailMem(MEMF_ANY);
    io->io_Data = w;
    io->io_Length = sizeof(struct Window);
    if (OpenDevice((STRPTR)"console.device", charmap ? CONU_CHARMAP : CONU_STANDARD, (struct IORequest *)io, 0)) {
        CloseWindow(w);
        return 20;
    }
    Delay(10);
    a1 = AvailMem(MEMF_ANY);
    for (i = 0; i < 300; i++) {
        sprintf(line, "line %d of 300: the scrollback fills up\n", i);
        io->io_Command = CMD_WRITE;
        io->io_Data = line;
        io->io_Length = (ULONG)-1;
        DoIO((struct IORequest *)io);
    }
    Delay(25);
    a2 = AvailMem(MEMF_ANY);
    CloseDevice((struct IORequest *)io);
    Delay(25);
    a3 = AvailMem(MEMF_ANY);
    printf("MEM %s open %ld full %ld leak %ld\n", charmap ? "CHARMAP" : "STANDARD", (long)(a0 - a1),
           (long)(a0 - a2), (long)(a0 - a3));
    CloseWindow(w);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(p);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
