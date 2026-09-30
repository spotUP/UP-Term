/* rkcprobe TABLE <file> | SWAP -- D2.3 of the console.device plan.
 *
 * TABLE: RawKeyConvert, through console.device CONU_LIBRARY, for every raw
 *        code 0-127 with no qualifier, Shift, Alt and Ctrl, default keymap:
 *        one line per code and qualifier into <file>. Run before and after
 *        UPConsole DEVICE ON, the files must be byte-identical (our vector
 *        goes to the ROM's).
 * SWAP:  a window with a CONU_CHARMAP unit; the unit's keymap (CD_ASKKEYMAP)
 *        gets the entries of raw keys 0x20 (a) and 0x35 (b) swapped
 *        (CD_SETKEYMAP); prints READY, reads what the rig types for 20 s and
 *        writes "READ hex" per read to RAM:rkcswap.log. Typing "a" must read "b": the unit
 *        converts with its own map (DD12). */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <devices/keymap.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/console.h>
#include <proto/dos.h>

struct Device *ConsoleDevice;
struct IntuitionBase *IntuitionBase;

static int table(const char *file)
{
    static const UWORD quals[4] = { 0, IEQUALIFIER_LSHIFT, IEQUALIFIER_LALT, IEQUALIFIER_CONTROL };
    FILE *f = fopen(file, "w");
    int code, q;
    if (!f)
        return 20;
    for (code = 0; code < 128; code++)
        for (q = 0; q < 4; q++) {
            struct InputEvent ie;
            UBYTE buf[32];
            LONG n, i;
            memset(&ie, 0, sizeof(ie));
            ie.ie_Class = IECLASS_RAWKEY;
            ie.ie_Code = (UWORD)code;
            ie.ie_Qualifier = quals[q];
            n = RawKeyConvert(&ie, (STRPTR)buf, sizeof(buf), 0);
            fprintf(f, "%02x %04x %ld", code, quals[q], n);
            for (i = 0; i < n && i < (LONG)sizeof(buf); i++)
                fprintf(f, " %02x", buf[i]);
            fprintf(f, "\n");
        }
    fclose(f);
    printf("table written\n");
    return 0;
}

/* a line to RAM:rkcswap.log, the file closed after it: the rig reads it
 * while the probe runs */
static void say(const char *s)
{
    BPTR f = Open((STRPTR)"RAM:rkcswap.log", MODE_READWRITE);
    if (f) {
        Seek(f, 0, OFFSET_END);
        Write(f, (APTR)s, (LONG)strlen(s));
        Close(f);
    }
}

static int swap(void)
{
    struct Window *w;
    struct MsgPort *p = CreateMsgPort();
    struct IOStdReq *io = p ? (struct IOStdReq *)CreateIORequest(p, sizeof(struct IOStdReq)) : 0;
    static struct KeyMap km;
    char buf[16];
    int t;
    if (!io)
        return 20;
    w = OpenWindowTags(0, WA_Left, 10, WA_Top, 20, WA_Width, 300, WA_Height, 80, WA_Title, (ULONG)"rkcprobe",
                       WA_Activate, TRUE, WA_DragBar, TRUE, WA_IDCMP, 0, TAG_DONE);
    if (!w)
        return 20;
    io->io_Data = w;
    io->io_Length = sizeof(struct Window);
    if (OpenDevice((STRPTR)"console.device", CONU_CHARMAP, (struct IORequest *)io, 0)) {
        printf("OpenDevice failed %d\n", io->io_Error);
        CloseWindow(w);
        return 20;
    }
    io->io_Command = CD_ASKKEYMAP;
    io->io_Data = &km;
    io->io_Length = sizeof(km);
    DoIO((struct IORequest *)io);
    {
        /* raw keys 0x20 and 0x35 are in the low map: swap type and mapping */
        UBYTE *types = (UBYTE *)km.km_LoKeyMapTypes;
        ULONG *map = (ULONG *)km.km_LoKeyMap;
        static UBYTE newtypes[64];
        static ULONG newmap[64];
        UBYTE tt;
        ULONG tm;
        CopyMem(types, newtypes, 64);
        CopyMem(map, newmap, 64 * 4);
        tt = newtypes[0x20]; newtypes[0x20] = newtypes[0x35]; newtypes[0x35] = tt;
        tm = newmap[0x20]; newmap[0x20] = newmap[0x35]; newmap[0x35] = tm;
        km.km_LoKeyMapTypes = newtypes;
        km.km_LoKeyMap = newmap;
    }
    io->io_Command = CD_SETKEYMAP;
    io->io_Data = &km;
    io->io_Length = sizeof(km);
    DoIO((struct IORequest *)io);
    say("READY\n");
    for (t = 0; t < 20; t++) {
        io->io_Command = CMD_READ;
        io->io_Data = buf;
        io->io_Length = sizeof(buf);
        SendIO((struct IORequest *)io);
        Delay(50);
        if (!CheckIO((struct IORequest *)io)) {
            AbortIO((struct IORequest *)io);
            WaitIO((struct IORequest *)io);
            continue;
        }
        WaitIO((struct IORequest *)io);
        {
            ULONG i;
            char line[80];
            int k = sprintf(line, "READ");
            for (i = 0; i < io->io_Actual && k < 70; i++)
                k += sprintf(line + k, " %02x", (UBYTE)buf[i]);
            strcpy(line + k, "\n");
            say(line);
        }
    }
    CloseDevice((struct IORequest *)io);
    CloseWindow(w);
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(p);
    return 0;
}

int main(int argc, char **argv)
{
    struct IOStdReq lib;
    int rc;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    memset(&lib, 0, sizeof(lib));
    if (OpenDevice((STRPTR)"console.device", (ULONG)CONU_LIBRARY, (struct IORequest *)&lib, 0))
        return 20;
    ConsoleDevice = lib.io_Device;
    if (argc > 2 && !strcmp(argv[1], "TABLE"))
        rc = table(argv[2]);
    else if (argc > 1 && !strcmp(argv[1], "SWAP"))
        rc = swap();
    else {
        printf("usage: rkcprobe TABLE <file> | SWAP\n");
        rc = 10;
    }
    CloseDevice((struct IORequest *)&lib);
    CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
