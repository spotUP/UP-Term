/* upcon_device.c -- the device half of UP-Term's console.device (plan
 * 2026-09-30-console-device.md, D1.2 and D1.5): Init, Open, Close, Expunge,
 * BeginIO, AbortIO. Built into up-console.device after upcon_rom.s, whose
 * RomTag points at upc_init; UPConsole DEVICE ON loads it and gives it to
 * InitResident (DD15).
 *
 * Every command answers as the ROM device does where DP4 measured it (all
 * four Kickstarts the same): CMD_INVALID/UPDATE and the 3.2 scrollback
 * commands IOERR_NOCMD, CMD_STOP/START -1, CMD_FLUSH -1 with no read queued
 * (0, and the reads IOERR_ABORTED, with some). One decided difference:
 * CMD_RESET resets the unit's terminal and replies; on the ROM it never
 * returns (DP4). */
#include <string.h>
#include <stddef.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/errors.h>
#include <devices/keymap.h>
#include <devices/newstyle.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <intuition/intuitionbase.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/keymap.h>
#include "upcon.h"

/* upcon_rom.s, UPConsole and tools read these fields at fixed offsets */
typedef char check_magic[offsetof(struct upc_base, magic) == UPC_MAGIC_OFFSET ? 1 : -1];
typedef char check_rom[offsetof(struct upc_base, rom) == UPC_ROMBASE_OFFSET ? 1 : -1];
typedef char check_conname[offsetof(struct upc_base, con_name) == UPC_CONNAME_OFFSET ? 1 : -1];

/* library bases: the same value for every unit (the only globals) */
struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *LayersBase;
struct Library *DiskfontBase;           /* stays 0: units draw with the window's font */
struct Library *KeymapBase;
struct Device *ConsoleDevice;           /* the ROM device: vtwin_key's RawKeyConvert */

extern char upc_name[], upc_idstring[], upc_con_name[];
extern void upc_fwd_cdinputhandler(void), upc_fwd_rawkeyconvert(void);
extern void upc_fwd_54(void), upc_fwd_60(void), upc_fwd_66(void), upc_fwd_72(void);
/* the ROM device's own Open and BeginIO, called with its base in a6 */
extern void upc_rom_open(__reg("a1") struct IORequest *io, __reg("d0") ULONG unit,
                         __reg("d1") ULONG flags, __reg("a0") struct Library *rom);
extern void upc_rom_beginio(__reg("a1") struct IORequest *io, __reg("a0") struct Library *rom);

static void upc_open(__reg("a1") struct IOStdReq *io, __reg("d0") ULONG unitno,
                     __reg("d1") ULONG flags, __reg("a6") struct upc_base *b);
static BPTR upc_close(__reg("a1") struct IOStdReq *io, __reg("a6") struct upc_base *b);
static BPTR upc_expunge(__reg("a6") struct upc_base *b);
static ULONG upc_null(void);
static void upc_beginio(__reg("a1") struct IOStdReq *io, __reg("a6") struct upc_base *b);
static ULONG upc_abortio(__reg("a1") struct IOStdReq *io);

static const APTR functable[] = {
    (APTR)upc_open, (APTR)upc_close, (APTR)upc_expunge, (APTR)upc_null,
    (APTR)upc_beginio, (APTR)upc_abortio,
    (APTR)upc_fwd_cdinputhandler, (APTR)upc_fwd_rawkeyconvert,
    (APTR)upc_fwd_54, (APTR)upc_fwd_60, (APTR)upc_fwd_66, (APTR)upc_fwd_72,
    (APTR)-1
};

/* NSCMD_DEVICEQUERY's list: what BeginIO answers other than IOERR_NOCMD */
static const UWORD supported[] = {
    CMD_RESET, CMD_READ, CMD_WRITE, CMD_CLEAR, CMD_FLUSH, CD_ASKKEYMAP, CD_SETKEYMAP,
    CD_ASKDEFAULTKEYMAP, CD_SETDEFAULTKEYMAP, NSCMD_DEVICEQUERY, 0
};

/* rt_Init (not RTF_AUTOINIT): d0 = 0, a0 = the seglist, a6 = exec. The ROM
 * device must still be the one named "console.device" (ours is named
 * "UP-Term console.device" until UPConsole renames it). */
struct upc_base *upc_init(__reg("a0") BPTR seglist, __reg("a6") struct ExecBase *sys)
{
    struct upc_base *b;
    struct Library *rom;
    SysBase = sys;
    Forbid();
    rom = (struct Library *)FindName(&SysBase->DeviceList, (STRPTR)"console.device");
    Permit();
    if (!rom)
        return 0;
    b = (struct upc_base *)MakeLibrary((APTR)functable, 0, 0, sizeof(struct upc_base), 0);
    if (!b)
        return 0;
    memset((char *)b + sizeof(struct Library), 0, sizeof(struct upc_base) - sizeof(struct Library));
    b->lib.lib_Node.ln_Type = NT_DEVICE;
    b->lib.lib_Node.ln_Name = upc_name;
    b->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    b->lib.lib_Version = rom->lib_Version; /* version checks see the running system (DD13) */
    b->lib.lib_Revision = rom->lib_Revision;
    b->lib.lib_IdString = (APTR)upc_idstring;
    b->magic = UPC_MAGIC;
    b->rom = rom;
    b->con_name = upc_con_name;
    b->seglist = seglist;
    InitSemaphore(&b->lock);
    DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR)"dos.library", 37);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 37);
    LayersBase = OpenLibrary((STRPTR)"layers.library", 37);
    KeymapBase = OpenLibrary((STRPTR)"keymap.library", 37);
    /* the ROM device stays open for good: its vectors are ours to call */
    b->romlib.io_Message.mn_Length = sizeof(b->romlib);
    if (!DOSBase || !IntuitionBase || !GfxBase || !LayersBase || !KeymapBase ||
        OpenDevice((STRPTR)"console.device", (ULONG)CONU_LIBRARY, (struct IORequest *)&b->romlib, 0)) {
        if (KeymapBase) CloseLibrary(KeymapBase);
        if (LayersBase) CloseLibrary(LayersBase);
        if (GfxBase) CloseLibrary((struct Library *)GfxBase);
        if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
        if (DOSBase) CloseLibrary((struct Library *)DOSBase);
        FreeMem((UBYTE *)b - b->lib.lib_NegSize, (ULONG)(b->lib.lib_NegSize + b->lib.lib_PosSize));
        return 0;
    }
    ConsoleDevice = b->romlib.io_Device;
    AddDevice((struct Device *)b);
    return b;
}

#ifdef UPCON_DEBUG
static void ser_putc(__reg("d0") char ch) =
    "\tmove.l\ta6,-(sp)\n\tmove.l\t4.w,a6\n\tjsr\t-516(a6)\n\tmove.l\t(sp)+,a6";

void upc_dbg(const char *what, const char *s, LONG v)
{
    int k;
    ser_putc('u');
    ser_putc(' ');
    while (*what)
        ser_putc(*what++);
    if (s) {
        ser_putc(' ');
        while (*s)
            ser_putc(*s++);
    }
    ser_putc(' ');
    for (k = 28; k >= 0; k -= 4)
        ser_putc("0123456789abcdef"[((ULONG)v >> k) & 15]);
    ser_putc('\n');
}
#endif

void upc_reply_io(struct IOStdReq *io, long actual, BYTE error)
{
    io->io_Actual = (ULONG)actual;
    io->io_Error = error;
    ReplyMsg((struct Message *)io);
}

static void done(struct IOStdReq *io)
{
    if (!(io->io_Flags & IOF_QUICK))
        ReplyMsg((struct Message *)io);
}

/* a queued CMD_READ answered from the unit process (upc_rq_input) */
static void rq_reply(void *user, void *req, long actual)
{
    struct upc_unit *u = (struct upc_unit *)user;
    u->base->answered++;
    upc_reply_io((struct IOStdReq *)req, actual, 0);
}

static void free_unit(struct upc_unit *u)
{
    FreeMem(u, sizeof(*u));
}

/* A new unit on the opener's window: its process, which attaches vtwin. */
static struct upc_unit *new_unit(struct upc_base *b, LONG unitno, ULONG flags, struct Window *win)
{
    struct upc_unit *u = (struct upc_unit *)AllocMem(sizeof(*u), MEMF_PUBLIC | MEMF_CLEAR);
    struct MsgPort *rp;
    int i;
    if (!u)
        return 0;
    u->base = b;
    u->unitno = unitno;
    u->flags = flags;
    u->win = win;
    u->slot = -1;
    InitSemaphore(&u->lock);
    upc_rq_init(&u->rq, rq_reply, 0, u);
    upc_ev_init(&u->ring);
    if (KeymapBase)
        CopyMem((APTR)AskKeyMapDefault(), &u->cu.cu_KeyMapStruct, sizeof(struct KeyMap)); /* DD12 */
    rp = CreateMsgPort();
    if (!rp) {
        free_unit(u);
        return 0;
    }
    u->startup_reply = rp;
    u->proc = CreateNewProcTags(NP_Entry, (ULONG)upc_unit_entry, NP_Name, (ULONG)"UP-Term console unit",
                                NP_StackSize, 12000, NP_Priority, 5, NP_Input, 0, NP_Output, 0,
                                NP_CloseInput, FALSE, NP_CloseOutput, FALSE, NP_ConsoleTask, 0,
                                NP_WindowPtr, -1, TAG_DONE);
    if (!u->proc) {
        DeleteMsgPort(rp);
        free_unit(u);
        return 0;
    }
    u->startup.mn_ReplyPort = rp;
    u->startup.mn_Length = sizeof(u->startup);
    u->startup.mn_Node.ln_Name = (char *)u;
    PutMsg(&u->proc->pr_MsgPort, &u->startup);
    WaitPort(rp);
    GetMsg(rp);
    DeleteMsgPort(rp);
    u->startup_reply = 0;
    if (!u->ok) {
        /* the process ended (it could not attach): nothing of it is left */
        free_unit(u);
        return 0;
    }
    Forbid(); /* the input handler reads units[] without a lock */
    for (i = 0; i < UPC_MAXUNITS; i++)
        if (!b->units[i]) {
            b->units[i] = u;
            u->slot = i;
            break;
        }
    Permit();
    return u;
}

/* Is the opening task on the exclusion list (UPConsole EXCLUDE)? */
static int excluded(struct upc_base *b)
{
    const char *me = FindTask(0)->tc_Node.ln_Name;
    int i;
    if (!me)
        return 0;
    for (i = 0; i < b->nexclude; i++)
        if (!strcmp(b->exclude[i], me))
            return 1;
    return 0;
}

static void upc_open(__reg("a1") struct IOStdReq *io, __reg("d0") ULONG unitno,
                     __reg("d1") ULONG flags, __reg("a6") struct upc_base *b)
{
    struct upc_unit *u;
    LONG n = (LONG)unitno;
    b->lib.lib_OpenCnt++; /* no expunge while this Open waits for its unit */
    b->lib.lib_Flags &= ~LIBF_DELEXP;
    io->io_Error = 0;
    /* the opener census (D1.7): who opens which unit with which flags */
    UPC_DBG("open", FindTask(0)->tc_Node.ln_Name ? FindTask(0)->tc_Node.ln_Name : "-", (unitno << 16) | (flags & 0xFFFF));
    if (!(flags & UPCONFLAG_ROM) && excluded(b)) /* DD16: this program gets the ROM's unit */
        flags |= UPCONFLAG_ROM;
    if (flags & UPCONFLAG_ROM) {
        /* DD16: the caller gets the ROM's unit and talks to the ROM from now on */
        io->io_Device = (struct Device *)b->rom;
        upc_rom_open((struct IORequest *)io, unitno, flags & ~UPCONFLAG_ROM, b->rom);
        b->lib.lib_OpenCnt--;
        return;
    }
    if (n == CONU_LIBRARY) {
        io->io_Unit = 0; /* the vectors and the device-wide commands */
        return;
    }
    if ((n != CONU_STANDARD && n != CONU_CHARMAP && n != CONU_SNIPMAP) || !io->io_Data) {
        io->io_Error = IOERR_OPENFAIL;
        io->io_Device = 0;
        b->lib.lib_OpenCnt--;
        return;
    }
    ObtainSemaphore(&b->lock);
    u = new_unit(b, n, flags, (struct Window *)io->io_Data);
    if (u) {
        int any = 0, i;
        for (i = 0; i < UPC_MAXUNITS; i++)
            any += b->units[i] != 0;
        if (u->slot < 0 || (any == 1 && !upc_input_add(b))) {
            /* no slot, or no input handler: the unit cannot hear keys */
            struct IOStdReq die = *io;
            die.io_Command = UPCMD_DIE;
            die.io_Flags = 0;
            die.io_Message.mn_ReplyPort = CreateMsgPort();
            if (die.io_Message.mn_ReplyPort) {
                PutMsg(&u->cu.cu_MP, (struct Message *)&die);
                WaitPort(die.io_Message.mn_ReplyPort);
                GetMsg(die.io_Message.mn_ReplyPort);
                DeleteMsgPort(die.io_Message.mn_ReplyPort);
            }
            Forbid();
            if (u->slot >= 0)
                b->units[u->slot] = 0;
            Permit();
            free_unit(u);
            u = 0;
        }
    }
    ReleaseSemaphore(&b->lock);
    if (!u) {
        io->io_Error = IOERR_OPENFAIL;
        io->io_Device = 0;
        b->lib.lib_OpenCnt--;
        return;
    }
    io->io_Unit = (struct Unit *)&u->cu;
}

static BPTR upc_close(__reg("a1") struct IOStdReq *io, __reg("a6") struct upc_base *b)
{
    struct upc_unit *u = (struct upc_unit *)io->io_Unit;
    UPC_DBG("close", FindTask(0)->tc_Node.ln_Name ? FindTask(0)->tc_Node.ln_Name : "-", u ? u->unitno : -1);
    if (u) {
        struct MsgPort *rp = CreateMsgPort(), *old = io->io_Message.mn_ReplyPort;
        int i, any = 0;
        ObtainSemaphore(&b->lock);
        Forbid(); /* no more events for it */
        if (u->slot >= 0)
            b->units[u->slot] = 0;
        Permit();
        for (i = 0; i < UPC_MAXUNITS; i++)
            any += b->units[i] != 0;
        if (!any)
            upc_input_rem(b);
        ReleaseSemaphore(&b->lock);
        /* the unit process draws what is pending, aborts its reads, ends */
        io->io_Command = UPCMD_DIE;
        io->io_Flags = 0;
        io->io_Message.mn_ReplyPort = rp;
        if (rp) {
            PutMsg(&u->cu.cu_MP, (struct Message *)io);
            WaitPort(rp);
            GetMsg(rp);
            DeleteMsgPort(rp);
        }
        io->io_Message.mn_ReplyPort = old;
        free_unit(u);
    }
    io->io_Unit = (struct Unit *)-1;
    io->io_Device = (struct Device *)-1;
    if (--b->lib.lib_OpenCnt == 0 && (b->lib.lib_Flags & LIBF_DELEXP))
        return upc_expunge(b);
    return 0;
}

/* Only when nothing is open and UPConsole took us out of service: a
 * low-memory flush while we are "console.device" must not remove the
 * system's console (the ROM node is out of the list then). UPConsole
 * DEVICE OFF puts the ROM node back, renames us and calls RemDevice. */
static BPTR upc_expunge(__reg("a6") struct upc_base *b)
{
    BPTR seg;
    if (b->lib.lib_OpenCnt || b->lib.lib_Node.ln_Name == b->con_name) {
        b->lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    Remove(&b->lib.lib_Node);
    CloseDevice((struct IORequest *)&b->romlib);
    CloseLibrary(KeymapBase);
    CloseLibrary(LayersBase);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    CloseLibrary((struct Library *)DOSBase);
    seg = b->seglist;
    FreeMem((UBYTE *)b - b->lib.lib_NegSize, (ULONG)(b->lib.lib_NegSize + b->lib.lib_PosSize));
    return seg;
}

static ULONG upc_null(void)
{
    return 0;
}

static void to_unit(struct upc_unit *u, struct IOStdReq *io)
{
    io->io_Flags &= ~IOF_QUICK;
    PutMsg(&u->cu.cu_MP, (struct Message *)io);
}

static void upc_beginio(__reg("a1") struct IOStdReq *io, __reg("a6") struct upc_base *b)
{
    struct upc_unit *u = (struct upc_unit *)io->io_Unit;
    io->io_Error = 0;
    io->io_Message.mn_Node.ln_Type = NT_MESSAGE;
    switch (io->io_Command) {
    case UPCMD_STATS: {
        struct upc_stats st;
        int i;
        st.units = 0;
        for (i = 0; i < UPC_MAXUNITS; i++)
            st.units += b->units[i] != 0;
        st.written = b->written;
        st.answered = b->answered;
        st.dropped = b->dropped;
        st.events = b->events;
        st.mice = b->mice;
        st.drags = b->drags;
        st.pointer = b->pointer;
        io->io_Actual = io->io_Length < sizeof(st) ? io->io_Length : sizeof(st);
        CopyMem(&st, io->io_Data, io->io_Actual);
        done(io);
        return;
    }
    case NSCMD_DEVICEQUERY: {
        struct NSDeviceQueryResult *q = (struct NSDeviceQueryResult *)io->io_Data;
        if (b->rom->lib_Version < 46) {
            io->io_Error = IOERR_NOCMD; /* as the running ROM (DP4: 46.1 answers, 39-40 do not) */
        } else if (io->io_Length < 16) {
            io->io_Error = IOERR_BADLENGTH;
        } else {
            q->nsdqr_DevQueryFormat = 0;
            q->nsdqr_SizeAvailable = 16;
            q->nsdqr_DeviceType = NSDEVTYPE_CONSOLE;
            q->nsdqr_DeviceSubType = 0;
            q->nsdqr_SupportedCommands = (APTR)supported;
            io->io_Actual = 16;
        }
        done(io);
        return;
    }
    case UPCMD_EXCLUDE: {
        const char *name = (const char *)io->io_Data;
        int i, known = 0;
        Forbid(); /* Open reads the list */
        if (!name || !io->io_Length || !name[0]) {
            b->nexclude = 0;
        } else {
            for (i = 0; i < b->nexclude; i++)
                known |= !strcmp(b->exclude[i], name);
            if (known) {
            } else if (b->nexclude == UPC_MAXEXCLUDE || strlen(name) >= UPC_EXCLUDE_LEN) {
                io->io_Error = IOERR_BADLENGTH;
            } else {
                strcpy(b->exclude[b->nexclude++], name);
            }
        }
        Permit();
        done(io);
        return;
    }
    case UPCMD_EXCLUDED: {
        char *out = (char *)io->io_Data;
        ULONG n = 0;
        int i;
        for (i = 0; i < b->nexclude; i++) {
            ULONG k = strlen(b->exclude[i]);
            if (n + k + 1 > io->io_Length)
                break;
            CopyMem(b->exclude[i], out + n, k);
            n += k;
            out[n++] = '\n';
        }
        io->io_Actual = n;
        done(io);
        return;
    }
    case CD_ASKDEFAULTKEYMAP:
    case CD_SETDEFAULTKEYMAP:
        /* one system default: the ROM device keeps it (DD12) */
        upc_rom_beginio((struct IORequest *)io, b->rom);
        return;
    default:
        break;
    }
    if (!u) {
        io->io_Error = IOERR_NOCMD; /* the library open has no unit */
        done(io);
        return;
    }
    switch (io->io_Command) {
    case CMD_READ: {
        long actual = 0;
        int r;
        ObtainSemaphore(&u->lock);
        r = upc_rq_read(&u->rq, io, (unsigned char *)io->io_Data, (long)io->io_Length, &actual);
        if (r == UPC_READ_QUEUED)
            io->io_Flags &= ~IOF_QUICK; /* answered later, by the unit process */
        ReleaseSemaphore(&u->lock);
        if (r == UPC_READ_DONE) {
            b->answered++;
            io->io_Actual = (ULONG)actual;
            done(io);
        } else if (r == UPC_READ_FULL) {
            io->io_Error = IOERR_UNITBUSY;
            done(io);
        }
        return;
    }
    case CMD_CLEAR:
        ObtainSemaphore(&u->lock);
        upc_rq_clear(&u->rq);
        ReleaseSemaphore(&u->lock);
        done(io);
        return;
    case CMD_FLUSH: {
        void *r;
        int n = 0;
        ObtainSemaphore(&u->lock);
        while ((r = upc_rq_take(&u->rq)) != 0) {
            upc_reply_io((struct IOStdReq *)r, 0, IOERR_ABORTED);
            n++;
        }
        ReleaseSemaphore(&u->lock);
        io->io_Error = n ? 0 : (BYTE)-1; /* DP4: -1 with nothing queued */
        done(io);
        return;
    }
    case CMD_STOP:
    case CMD_START:
        io->io_Error = -1; /* DP4 */
        done(io);
        return;
    case CD_ASKKEYMAP:
    case CD_SETKEYMAP: {
        ULONG n = io->io_Length < sizeof(struct KeyMap) ? io->io_Length : sizeof(struct KeyMap);
        if (io->io_Command == CD_ASKKEYMAP)
            CopyMem(&u->cu.cu_KeyMapStruct, io->io_Data, n);
        else
            CopyMem(io->io_Data, &u->cu.cu_KeyMapStruct, n);
        io->io_Actual = n;
        done(io);
        return;
    }
    case CMD_WRITE:
        upc_unit_write(u, io); /* in the caller's task, as the ROM (DD5 amended) */
        done(io);
        return;
    case CMD_RESET:
        to_unit(u, io);
        return;
    default:
        io->io_Error = IOERR_NOCMD; /* DD14 */
        done(io);
        return;
    }
}

static ULONG upc_abortio(__reg("a1") struct IOStdReq *io)
{
    struct upc_unit *u = (struct upc_unit *)io->io_Unit;
    int removed = 0;
    if (!u || io->io_Command != CMD_READ)
        return 0;
    ObtainSemaphore(&u->lock);
    removed = upc_rq_abort(&u->rq, io);
    ReleaseSemaphore(&u->lock);
    if (removed)
        upc_reply_io(io, 0, IOERR_ABORTED);
    return 0;
}
