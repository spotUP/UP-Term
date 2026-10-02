/* sigprobe -- DV6 of the console.device plan (the ibmcon 1.8 regression,
 * research R-3): a task that holds signal bit 31 opens and closes a console
 * unit on its window 100 times, writing each time; afterwards bit 31 must
 * still be the task's (a device that allocates or frees signals in the
 * opener's task can take it). Prints "SIG31 held after 100 opens" or
 * "SIG31 lost at open N", and the free memory before and after. */
#include <stdio.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <exec/tasks.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>

struct IntuitionBase *IntuitionBase;

int main(void)
{
    struct Task *me = FindTask(0);
    struct Window *w;
    struct MsgPort *p;
    struct IOStdReq *io;
    ULONG before, after;
    int i, lost = 0;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    if (!IntuitionBase || AllocSignal(31) != 31) {
        printf("SIG31 not free at the start\n");
        return 20;
    }
    p = CreateMsgPort();
    io = p ? (struct IOStdReq *)CreateIORequest(p, sizeof(struct IOStdReq)) : 0;
    w = OpenWindowTags(0, WA_Left, 20, WA_Top, 30, WA_Width, 300, WA_Height, 80, WA_Title, (ULONG)"sigprobe",
                       WA_DragBar, TRUE, WA_IDCMP, 0, TAG_DONE);
    if (!io || !w)
        return 20;
    before = AvailMem(MEMF_ANY);
    for (i = 1; i <= 100 && !lost; i++) {
        io->io_Data = w;
        io->io_Length = sizeof(struct Window);
        if (OpenDevice((STRPTR)"console.device", CONU_SNIPMAP, (struct IORequest *)io, 0)) {
            printf("OpenDevice failed at %d\n", i);
            break;
        }
        io->io_Command = CMD_WRITE;
        io->io_Data = (APTR)"sigprobe\n";
        io->io_Length = (ULONG)-1;
        DoIO((struct IORequest *)io);
        CloseDevice((struct IORequest *)io);
        if (!(me->tc_SigAlloc & (1UL << 31)))
            lost = i;
    }
    after = AvailMem(MEMF_ANY);
    Delay(50); /* the last unit's process may still be ending */
    printf("MEM one second later %lu\n", AvailMem(MEMF_ANY));
    if (lost)
        printf("SIG31 lost at open %d\n", lost);
    else
        printf("SIG31 held after 100 opens\n");
    printf("MEM before %lu after %lu\n", before, after);
    CloseWindow(w);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(p);
    FreeSignal(31);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
