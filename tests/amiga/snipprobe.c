/* snipprobe [CHARMAP | CLIP] -- D1.3/DD10 of the console.device plan: a unit's text
 * selected with the mouse and copied with Right Amiga C lands on the
 * clipboard (unit 0, FTXT), on a SNIPMAP unit only.
 *
 * Opens a 400 x 80 window at 10,20 with a SNIPMAP unit (CHARMAP with the
 * argument), writes "snipprobe copy me", clears the clipboard's text, writes
 * READY to RAM:snipprobe.log and waits 12 s while the rig drags over the
 * text and presses Right Amiga C; then logs "CLIP <text>" (or "CLIP -").
 * CLIP: only logs the clipboard's text (XCON:'s own copy, snip_rig.py). */
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
#include "../../handler/clip.h"

struct IntuitionBase *IntuitionBase;

static void say(const char *s)
{
    BPTR f = Open((STRPTR)"RAM:snipprobe.log", MODE_READWRITE);
    if (f) {
        Seek(f, 0, OFFSET_END);
        Write(f, (APTR)s, (LONG)strlen(s));
        Close(f);
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "CLIP")) {
        /* just the clipboard's text, for a window someone else drew */
        static char l[300];
        long k;
        char *t = clip_read(&k);
        sprintf(l, "CLIP %.250s\n", t && k > 0 ? t : "-");
        say(l);
        if (t)
            FreeVec(t);
        return 0;
    }
    struct Window *w;
    struct MsgPort *p = CreateMsgPort();
    struct IOStdReq *io = p ? (struct IOStdReq *)CreateIORequest(p, sizeof(struct IOStdReq)) : 0;
    static char line[300];
    long n;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    if (!io || !IntuitionBase)
        return 20;
    w = OpenWindowTags(0, WA_Left, 10, WA_Top, 20, WA_Width, 400, WA_Height, 80, WA_Title, (ULONG)"snipprobe",
                       WA_Activate, TRUE, WA_DragBar, TRUE, WA_SimpleRefresh, TRUE, WA_IDCMP, 0, TAG_DONE);
    if (!w)
        return 20;
    io->io_Data = w;
    io->io_Length = sizeof(struct Window);
    if (OpenDevice((STRPTR)"console.device", argc > 1 && !strcmp(argv[1], "CHARMAP") ? CONU_CHARMAP : CONU_SNIPMAP, (struct IORequest *)io, 0)) {
        say("OpenDevice failed\n");
        CloseWindow(w);
        return 20;
    }
    io->io_Command = CMD_WRITE;
    io->io_Data = (APTR)"snipprobe copy me";
    io->io_Length = (ULONG)-1;
    DoIO((struct IORequest *)io);
    clip_write("-", 1);
    say("READY\n");
    Delay(12 * 50);
    {
        char *t = clip_read(&n);
        sprintf(line, "CLIP %.250s\n", t && n > 0 ? t : "-");
        if (t)
            FreeVec(t);
    }
    say(line);
    CloseDevice((struct IORequest *)io);
    CloseWindow(w);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(p);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
