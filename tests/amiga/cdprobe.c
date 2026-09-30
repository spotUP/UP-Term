/* cdprobe [NOSCROLL] [RESET] [UNIT=n] -- DP4 of the console.device plan
 * (thoughts/shared/plans/2026-09-30-console-device.md): what the running
 * ROM console.device answers, where the documents say nothing (research
 * section 1.1: CMD_RESET/UPDATE/STOP/START/FLUSH, the 3.2 scrollback
 * commands, NSCMD_DEVICEQUERY's list), and two behaviours the device
 * phases copy (CONFLAG_NODRAW_ON_NEWSIZE, re-wrap of a wrapped line on
 * resize: D1.8, RN-CH 47.2).
 *
 * 1. Census. For units 0 (STANDARD), 1 (CHARMAP) and 3 (SNIPMAP), each on
 *    its own SIMPLE_REFRESH window with IDCMP 0: commands 0-14 and
 *    NSCMD_DEVICEQUERY, each sent with SendIO; a command still pending after
 *    one second is AbortIO'd (CMD_READ with no input: io_Error after the
 *    abort is what AbortIO of a queued read gives, D1.2). One line per
 *    command and unit, then one RESULT line per command over the units.
 *    CD_SETKEYMAP writes back what CD_ASKKEYMAP read; CD_SETDEFAULTKEYMAP
 *    sets keymap.library's current default (AskKeyMapDefault), i.e. no
 *    change and no dangling pointer (since V36 the device keeps the pointer).
 *    CD_SETUPSCROLLBACK/CD_SETSCROLLBACKPOSITION get a zeroed struct
 *    ConsoleScrollback; NOSCROLL skips them (their semantics are unknown, a
 *    3.2 ROM might dereference the NULL gadget). Each command's line is
 *    printed before it is sent, so a crash names it (RAM:cdprobe.log).
 *    Also: CMD_FLUSH with a CMD_READ pending -- what the read returns.
 * 2. NODRAW. A CHARMAP unit with flags 0 and one with
 *    CONFLAG_NODRAW_ON_NEWSIZE: three lines written, the window shrunk to
 *    half and grown back; ink pixels in the inner area before and after.
 * 3. Re-wrap. A SNIPMAP window NARROW columns wide gets a LINE-character
 *    line (no newline), the window is widened to WIDE columns; the cursor
 *    position (CSI 6n) before and after says whether the console re-wrapped
 *    the line. The plan's numbers (60/100/120) are used when 120 columns fit
 *    the screen; otherwise WIDE = what fits, NARROW = WIDE/2, LINE = WIDE-2.
 *    "SHOT rewrap-before"/"SHOT rewrap-after" lines: the window stays up
 *    4 s after each for the runner's screenshot.
 * CMD_RESET is skipped unless RESET is given: on console.device 40.2
 * (KS 40.63) SendIO of CMD_RESET on unit 0 never returns (measured
 * 2026-09-30, the probe hangs inside BeginIO). UNIT=n runs the census on
 * that unit only and skips parts 2 and 3 (RESET UNIT=1: one reboot per unit);
 * NOCENSUS runs parts 2 and 3 only.
 * Output also in RAM:cdprobe.log. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <exec/execbase.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <devices/keymap.h>
#include <devices/newstyle.h>
#include <intuition/intuition.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/keymap.h>
#include "probeout.h"

extern struct ExecBase *SysBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *KeymapBase;

#define NCMD 16                         /* 0..14, and NSCMD_DEVICEQUERY last */
static const UWORD cmds[NCMD] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, NSCMD_DEVICEQUERY };
static const char *const cmdname[NCMD] = {
    "CMD_INVALID", "CMD_RESET", "CMD_READ", "CMD_WRITE", "CMD_UPDATE", "CMD_CLEAR",
    "CMD_STOP", "CMD_START", "CMD_FLUSH", "CD_ASKKEYMAP", "CD_SETKEYMAP",
    "CD_ASKDEFAULTKEYMAP", "CD_SETDEFAULTKEYMAP", "CD_SETUPSCROLLBACK",
    "CD_SETSCROLLBACKPOSITION", "NSCMD_DEVICEQUERY"
};
static const LONG units[3] = { CONU_STANDARD, CONU_CHARMAP, CONU_SNIPMAP };
/* per unit and command: io_Error, or 1000 queued+aborted (+ its error), 2000 skipped, 3000 no unit */
static LONG census[3][NCMD];

typedef struct {
    struct Window *win;
    struct MsgPort *port;
    struct IOStdReq *io;               /* the unit's own request */
    struct IOStdReq *cio;              /* a second one on the same unit */
} unit;

static struct Window *open_win(const char *title, WORD w, WORD h)
{
    return OpenWindowTags(0, WA_Left, 10, WA_Top, 20, WA_Width, w, WA_Height, h,
                          WA_MinWidth, 60, WA_MinHeight, 30, WA_MaxWidth, -1, WA_MaxHeight, -1,
                          WA_Title, (ULONG)title, WA_IDCMP, 0, WA_SimpleRefresh, TRUE,
                          WA_SizeGadget, TRUE, WA_DragBar, TRUE, WA_DepthGadget, TRUE, TAG_DONE);
}

static void close_unit(unit *u)
{
    if (u->cio)
        DeleteIORequest((struct IORequest *)u->cio);
    if (u->io) {
        if (u->io->io_Device)
            CloseDevice((struct IORequest *)u->io);   /* the console before its window (AM-30) */
        DeleteIORequest((struct IORequest *)u->io);
    }
    if (u->port)
        DeleteMsgPort(u->port);
    if (u->win)
        CloseWindow(u->win);
    memset(u, 0, sizeof(*u));
}

static int open_unit(unit *u, const char *title, WORD w, WORD h, LONG unitno, ULONG flags)
{
    memset(u, 0, sizeof(*u));
    u->win = open_win(title, w, h);
    u->port = CreateMsgPort();
    if (u->port) {
        u->io = (struct IOStdReq *)CreateIORequest(u->port, sizeof(struct IOStdReq));
        u->cio = (struct IOStdReq *)CreateIORequest(u->port, sizeof(struct IOStdReq));
    }
    if (!u->win || !u->io || !u->cio) {
        close_unit(u);
        return 0;
    }
    u->io->io_Data = (APTR)u->win;
    u->io->io_Length = sizeof(struct Window);
    if (OpenDevice((STRPTR)"console.device", (ULONG)unitno, (struct IORequest *)u->io, flags)) {
        po_line("OpenDevice unit %ld flags %lu: io_Error %d\n", unitno, flags, u->io->io_Error);
        u->io->io_Device = 0;
        close_unit(u);
        return 0;
    }
    u->cio->io_Device = u->io->io_Device;
    u->cio->io_Unit = u->io->io_Unit;
    return 1;
}

/* SendIO, wait up to `ticks` for the reply; a request still pending is
 * AbortIO'd. Returns 1 when it came back by itself. */
static int trace;                       /* census: log each step, so a hang names it */

static int send_wait(struct IOStdReq *r, int ticks)
{
    int t;
    SendIO((struct IORequest *)r);
    if (trace)
        po_line("  SendIO returned, io_Flags %u\n", r->io_Flags);
    for (t = 0; t < ticks && !CheckIO((struct IORequest *)r); t++)
        Delay(1);
    if (CheckIO((struct IORequest *)r)) {
        WaitIO((struct IORequest *)r);
        return 1;
    }
    if (trace)
        po_line("  pending after %d ticks, AbortIO\n", ticks);
    AbortIO((struct IORequest *)r);
    if (trace)
        po_line("  AbortIO returned, WaitIO\n");
    WaitIO((struct IORequest *)r);
    return 0;
}

static void write_str(unit *u, const char *s)
{
    u->cio->io_Command = CMD_WRITE;
    u->cio->io_Data = (APTR)s;
    u->cio->io_Length = (ULONG)-1;
    DoIO((struct IORequest *)u->cio);
}

/* CSI 6n: the cursor's row and column (1-based) from the reply
 * CSI row ; col R, or 0 when no reply came within two seconds. */
static int cursor_pos(unit *u, int *row, int *col)
{
    char buf[64], one[16];
    int n = 0, i;
    write_str(u, "\x9b" "6n");
    while (n < (int)sizeof(buf) - 16) {
        u->cio->io_Command = CMD_READ;
        u->cio->io_Data = one;
        u->cio->io_Length = sizeof(one);
        if (!send_wait(u->cio, 100) || u->cio->io_Error)
            return 0;
        memcpy(buf + n, one, u->cio->io_Actual);
        n += (int)u->cio->io_Actual;
        for (i = 0; i < n; i++)
            if (buf[i] == 'R') {
                char *p = buf, *e;
                while (p < buf + n && (unsigned char)*p != 0x9b)
                    p++;
                if (p >= buf + n)
                    return 0;
                *row = (int)strtol(p + 1, &e, 10);
                *col = *e == ';' ? (int)strtol(e + 1, 0, 10) : 0;
                return 1;
            }
    }
    return 0;
}

/* Pixels in the inner area whose pen is not 0 (the background). One
 * ReadPixelLine8 per row: ReadPixel per pixel took minutes on the rig's RTG
 * screen (measured 2026-09-30, the probe sat in this count). */
static LONG ink(struct Window *w)
{
    static UBYTE line[1024];
    struct RastPort tmp;
    LONG x, y, n = 0, x0 = w->BorderLeft, width = w->Width - w->BorderLeft - w->BorderRight;
    if (width > (LONG)sizeof(line) - 16)
        width = sizeof(line) - 16;
    tmp = *w->RPort;
    tmp.Layer = 0;
    tmp.BitMap = AllocBitMap((ULONG)(width + 15) & ~15UL, 1, GetBitMapAttr(w->RPort->BitMap, BMA_DEPTH), 0,
                             w->RPort->BitMap);
    if (!tmp.BitMap)
        return -1;
    for (y = w->BorderTop; y < w->Height - w->BorderBottom; y++) {
        ReadPixelLine8(w->RPort, x0, y, (ULONG)width, line, &tmp);
        for (x = 0; x < width; x++)
            if (line[x])
                n++;
    }
    FreeBitMap(tmp.BitMap);
    return n;
}

static int noscroll, sendreset;

static void census_unit(int ui)
{
    unit u;
    char title[40];
    static char buf[256];
    static struct KeyMap km, dkm;
    static UBYTE nsq[64];
    static struct ConsoleScrollback cs;
    int ci, askok = 0;
    sprintf(title, "cdprobe unit %ld", units[ui]);
    if (!po_check(open_unit(&u, title, 400, 100, units[ui], 0), title, "OpenDevice failed")) {
        for (ci = 0; ci < NCMD; ci++)
            census[ui][ci] = 3000;
        return;
    }
    write_str(&u, "cdprobe census\n");
    for (ci = 0; ci < NCMD; ci++) {
        struct IOStdReq *r = u.cio;
        int back;
        memset(buf, 0, sizeof(buf));
        r->io_Command = cmds[ci];
        r->io_Flags = 0;
        r->io_Data = buf;
        r->io_Length = 0;
        r->io_Actual = 0;
        switch (cmds[ci]) {
        case CMD_READ: r->io_Length = 16; break;
        case CMD_WRITE: r->io_Data = "w"; r->io_Length = 1; break;
        case CD_ASKKEYMAP: r->io_Data = &km; r->io_Length = sizeof(km); break;
        case CD_SETKEYMAP: r->io_Data = &km; r->io_Length = sizeof(km); break;
        case CD_ASKDEFAULTKEYMAP: r->io_Data = &dkm; r->io_Length = sizeof(dkm); break;
        case CD_SETDEFAULTKEYMAP:
            r->io_Data = KeymapBase ? (APTR)AskKeyMapDefault() : 0;
            r->io_Length = sizeof(struct KeyMap);
            break;
        case CD_SETUPSCROLLBACK: case CD_SETSCROLLBACKPOSITION:
            memset(&cs, 0, sizeof(cs));
            r->io_Data = &cs;
            r->io_Length = sizeof(cs);
            break;
        case NSCMD_DEVICEQUERY: r->io_Data = nsq; r->io_Length = sizeof(nsq); break;
        }
        if ((cmds[ci] == CMD_RESET && !sendreset)
            || (cmds[ci] == CD_SETKEYMAP && !askok) || (cmds[ci] == CD_SETDEFAULTKEYMAP && !r->io_Data)
            || (noscroll && (cmds[ci] == CD_SETUPSCROLLBACK || cmds[ci] == CD_SETSCROLLBACKPOSITION))) {
            census[ui][ci] = 2000;
            po_line("unit %ld cmd %04x %-24s skipped\n", units[ui], cmds[ci], cmdname[ci]);
            continue;
        }
        po_line("unit %ld cmd %04x %-24s sending\n", units[ui], cmds[ci], cmdname[ci]);
        trace = 1;
        back = send_wait(r, 50);
        trace = 0;
        census[ui][ci] = back ? r->io_Error : 1000 + r->io_Error;
        if (cmds[ci] == CD_ASKKEYMAP && back && !r->io_Error)
            askok = 1;
        po_line("unit %ld cmd %04x %-24s %s io_Error %d io_Actual %lu io_Length %lu\n", units[ui],
                cmds[ci], cmdname[ci], back ? "done" : "queued, aborted:", r->io_Error,
                r->io_Actual, r->io_Length);
        if (cmds[ci] == NSCMD_DEVICEQUERY && back && !r->io_Error) {
            struct NSDeviceQueryResult *q = (struct NSDeviceQueryResult *)nsq;
            UWORD *c = (UWORD *)q->nsdqr_SupportedCommands;
            char list[200], *p = list;
            list[0] = 0;
            while (c && *c && p < list + 190)
                p += sprintf(p, " %04x", *c++);
            po_line("RESULT unit %ld devicequery type %u subtype %u size %lu commands%s\n", units[ui],
                    q->nsdqr_DeviceType, q->nsdqr_DeviceSubType, q->nsdqr_SizeAvailable, list);
        }
    }
    /* CMD_FLUSH with a read pending: what does the read get? */
    {
        struct IOStdReq *rd = u.cio, *fl = u.io;
        rd->io_Command = CMD_READ;
        rd->io_Data = buf;
        rd->io_Length = 16;
        SendIO((struct IORequest *)rd);
        Delay(10);
        if (!CheckIO((struct IORequest *)rd)) {
            int t;
            fl->io_Command = CMD_FLUSH;
            fl->io_Flags = 0;
            DoIO((struct IORequest *)fl);
            for (t = 0; t < 50 && !CheckIO((struct IORequest *)rd); t++)
                Delay(1);
            if (CheckIO((struct IORequest *)rd)) {
                WaitIO((struct IORequest *)rd);
                po_line("RESULT unit %ld flush-with-read-pending: flush io_Error %d, read replied io_Error %d\n",
                        units[ui], fl->io_Error, rd->io_Error);
            } else {
                AbortIO((struct IORequest *)rd);
                WaitIO((struct IORequest *)rd);
                po_line("RESULT unit %ld flush-with-read-pending: flush io_Error %d, read still pending (aborted: %d)\n",
                        units[ui], fl->io_Error, rd->io_Error);
            }
        } else {
            WaitIO((struct IORequest *)rd);
            po_line("unit %ld flush test: the read came back at once (io_Error %d)\n", units[ui], rd->io_Error);
        }
    }
    close_unit(&u);
}

static void nodraw(ULONG flags)
{
    unit u;
    WORD w0, h0;
    LONG before, after;
    if (!po_check(open_unit(&u, flags ? "cdprobe nodraw 1" : "cdprobe nodraw 0", 400, 100, CONU_CHARMAP, flags),
                  flags ? "CHARMAP unit with CONFLAG_NODRAW_ON_NEWSIZE" : "CHARMAP unit with flags 0", 0))
        return;
    write_str(&u, "\x0c" "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n"
              "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n" "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
    Delay(25);
    po_line("  nodraw %lu: written, counting ink\n", flags);
    before = ink(u.win);
    w0 = u.win->Width;
    h0 = u.win->Height;
    po_line("  nodraw %lu: ink %ld, shrinking\n", flags, before);
    {
        struct ConUnit *cu = (struct ConUnit *)u.io->io_Unit;
        int r, c;
        po_line("RESULT nodraw %lu before: unit %d x %d\n", flags, cu->cu_XMax + 1, cu->cu_YMax + 1);
        ChangeWindowBox(u.win, u.win->LeftEdge, u.win->TopEdge, w0 / 2, h0 / 2);
        Delay(50);
        write_str(&u, ""); /* the ROM recomputes its size on a write (R-1.4) */
        r = c = 0;
        cursor_pos(&u, &r, &c);
        po_line("RESULT nodraw %lu shrunk: unit %d x %d, cursor %d;%d\n", flags, cu->cu_XMax + 1,
                cu->cu_YMax + 1, r, c);
        ChangeWindowBox(u.win, u.win->LeftEdge, u.win->TopEdge, w0, h0);
        Delay(50);
        write_str(&u, "");
        r = c = 0;
        cursor_pos(&u, &r, &c);
        po_line("RESULT nodraw %lu grown: unit %d x %d, cursor %d;%d\n", flags, cu->cu_XMax + 1,
                cu->cu_YMax + 1, r, c);
    }
    after = ink(u.win);
    po_line("RESULT nodraw flags %lu: ink before %ld, after shrink+grow %ld (%s)\n", flags, before, after,
            after >= before ? "redrawn" : "not redrawn");
    close_unit(&u);
}

static void rewrap(void)
{
    unit u;
    struct Screen *s = LockPubScreen(0);
    WORD fw = s ? s->RastPort.TxWidth : 8, bw;
    int wide, narrow, len, r0 = 0, c0 = 0, r1 = 0, c1 = 0, i;
    static char line[200];
    bw = s ? s->WBorLeft + s->WBorRight + 18 : 30;     /* + the size gadget's column */
    wide = s ? (s->Width - bw) / fw - 1 : 78;
    if (s)
        UnlockPubScreen(0, s);
    if (wide >= 120) {
        wide = 120;
        narrow = 60;
        len = 100;
    } else {
        narrow = wide / 2;
        len = wide - 2;
    }
    po_line("rewrap: font width %d, narrow %d, line %d, wide %d columns\n", fw, narrow, len, wide);
    if (!po_check(open_unit(&u, "cdprobe rewrap", (WORD)(narrow * fw + bw), 100, CONU_SNIPMAP, 0),
                  "SNIPMAP unit for the re-wrap", 0))
        return;
    /* the window as opened may round: measure the unit's real width */
    {
        struct ConUnit *cu = (struct ConUnit *)u.io->io_Unit;
        po_line("rewrap: the unit's cu_XMax+1 = %d\n", cu->cu_XMax + 1);
        narrow = cu->cu_XMax + 1;
        if (len <= narrow)
            len = narrow + narrow / 2;
    }
    for (i = 0; i < len; i++)
        line[i] = (char)('0' + i % 10);
    line[len] = 0;
    write_str(&u, "\x0c");
    write_str(&u, line);
    po_check(cursor_pos(&u, &r0, &c0), "CSI 6n answered before the resize", 0);
    po_line("SHOT rewrap-before\n");
    Delay(200);
    ChangeWindowBox(u.win, u.win->LeftEdge, u.win->TopEdge, (WORD)(wide * fw + bw), u.win->Height);
    Delay(75);
    po_check(cursor_pos(&u, &r1, &c1), "CSI 6n answered after the resize", 0);
    /* the cursor after `len` characters from column 1 of row 1 in a unit
     * `w` columns wide is row len/w+1, column len%w+1: a re-wrap moves it
     * there for the new width, no re-wrap leaves it where it was */
    wide = ((struct ConUnit *)u.io->io_Unit)->cu_XMax + 1;
    po_line("rewrap: the unit's cu_XMax+1 after = %d\n", wide);
    po_line("RESULT rewrap line %d narrow %d wide %d: cursor before %d;%d after %d;%d -> %s\n", len, narrow, wide,
            r0, c0, r1, c1, (r1 == r0 && c1 == c0) ? "not re-wrapped" :
            (r1 == len / wide + 1 && c1 == len % wide + 1) ? "re-wrapped" : "other (see the screenshot)");
    po_line("SHOT rewrap-after\n");
    Delay(200);
    close_unit(&u);
}

int main(int argc, char **argv)
{
    int ui, ci, i, only = -1;
    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "NOSCROLL"))
            noscroll = 1;
        else if (!strcmp(argv[i], "RESET"))
            sendreset = 1;
        else if (!strcmp(argv[i], "NOCENSUS"))
            only = -2;
        else if (!strncmp(argv[i], "UNIT=", 5))
            only = atoi(argv[i] + 5);
    po_start("RAM:cdprobe.log");
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 37);
    KeymapBase = OpenLibrary((STRPTR)"keymap.library", 37);
    if (!po_check(IntuitionBase && GfxBase, "intuition and graphics 37", 0))
        return po_end();
    {
        struct Library *con = (struct Library *)FindName(&SysBase->DeviceList, (STRPTR)"console.device");
        po_line("RESULT console.device %d.%d kickstart %d.%d\n", con ? con->lib_Version : 0,
                con ? con->lib_Revision : 0, SysBase->LibNode.lib_Version, SysBase->SoftVer);
    }
    for (ui = 0; ui < 3; ui++)
        if (only == -1 || units[ui] == only)
            census_unit(ui);
        else
            for (ci = 0; ci < NCMD; ci++)
                census[ui][ci] = 2000;
    for (ci = 0; ci < NCMD; ci++) {
        char row[120], *p = row;
        for (ui = 0; ui < 3; ui++) {
            LONG v = census[ui][ci];
            p += v == 2000 ? sprintf(p, " unit%ld skipped", units[ui])
               : v == 3000 ? sprintf(p, " unit%ld no-unit", units[ui])
               : v >= 500 ? sprintf(p, " unit%ld queued(abort %ld)", units[ui], v - 1000)
               : sprintf(p, " unit%ld %ld", units[ui], v);
        }
        po_line("RESULT cmd %04x %-24s%s\n", cmds[ci], cmdname[ci], row);
    }
    if (only < 0) {
        po_line("nodraw part\n");
        nodraw(0);
        nodraw(CONFLAG_NODRAW_ON_NEWSIZE);
        rewrap();
    }
    if (KeymapBase)
        CloseLibrary(KeymapBase);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return po_end();
}
