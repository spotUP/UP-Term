/* uprom.c -- UP-Term in a Kickstart ROM (ledger R1, research
 * thoughts/shared/research/2026-10-04_upterm-in-rom.md).
 *
 * The init of the RTF_AFTERDOS RomTag in uprom_tag.s. It runs from ROM, so
 * it keeps nothing in writable statics: no C library, the library bases are
 * locals named as the inline calls expect (SysBase, DOSBase).
 *
 * up-console.device and the vtcon handler are ordinary vbcc builds with
 * data and BSS of their own: they cannot run from ROM. They lie in ROM as
 * their hunk files and are loaded into RAM here with dos.library's own
 * loader (InternalLoadSeg, reading from ROM), so they get the seglist a
 * disk LoadSeg gives them and run exactly as installed from disk. Then:
 *   - DEVICE ON as UPConsole does it: InitResident, our node is named
 *     console.device (the ROM's stays loaded underneath: ours forwards to
 *     it). Skipped on dos 47 (3.2), as UPConsole refuses it there.
 *   - CON ON as UPConsole does it: the CON and RAW entries run the handler.
 *   - XCON: mounted with the values of the kit's DEVS:DOSDrivers/XCON.
 * The state goes into the "UP-Term console" semaphore, so C:UPConsole
 * STATUS / CON OFF / DEVICE OFF work on a ROM boot as on a disk install.
 * Each step reports to the serial port (exec RawPutChar). */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "upc_public.h"
#include "upc_switch.h"

typedef struct {
    const UBYTE *pos;
    ULONG left;
} uprom_cursor;

extern const UBYTE uprom_device[], uprom_device_end[], uprom_handler[], uprom_handler_end[];
extern void uprom_read(void), uprom_alloc(void), uprom_free(void);

static void ser_putc(__reg("d0") char ch) =
    "\tmove.l\ta6,-(sp)\n\tmove.l\t4.w,a6\n\tjsr\t-516(a6)\n\tmove.l\t(sp)+,a6";

static void say(const char *s)
{
    const char *p = "UP-Term ROM: ";
    while (*p)
        ser_putc(*p++);
    while (*s)
        ser_putc(*s++);
    ser_putc('\n');
}

/* a hunk file that lies in ROM, loaded into RAM by dos.library */
static BPTR load(struct DosLibrary *DOSBase, const UBYTE *from, const UBYTE *to)
{
    LONG funcs[3];
    LONG stack = 0;
    uprom_cursor c;
    funcs[0] = (LONG)uprom_read;
    funcs[1] = (LONG)uprom_alloc;
    funcs[2] = (LONG)uprom_free;
    c.pos = from;
    c.left = (ULONG)(to - from);
    return InternalLoadSeg((BPTR)&c, 0, funcs, &stack);
}

/* XCON: as DEVS:DOSDrivers/XCON mounts it (Priority 5, StackSize 16000,
 * GlobVec -1), started from the loaded seglist instead of L: */
static void mount_xcon(struct DosLibrary *DOSBase, BPTR seg)
{
    struct DeviceNode *d = (struct DeviceNode *)MakeDosEntry((STRPTR)"XCON", DLT_DEVICE);
    if (!d) {
        say("XCON: no memory");
        return;
    }
    d->dn_SegList = seg;
    d->dn_StackSize = 16000;
    d->dn_Priority = 5;
    d->dn_GlobalVec = -1;
    if (!AddDosEntry((struct DosList *)d)) {
        FreeDosEntry((struct DosList *)d);
        say("XCON: there is one already");
        return;
    }
    say("XCON: mounted");
}

static void device_on(struct ExecBase *SysBase, struct DosLibrary *DOSBase, upc_state *st)
{
    struct Library *rom;
    BPTR seg;
    if (DOSBase->dl_lib.lib_Version >= 47) {
        say("console.device left to the ROM: 3.2 (as UPConsole DEVICE ON)");
        return;
    }
    Forbid();
    rom = (struct Library *)FindName(&SysBase->DeviceList, (STRPTR)"console.device");
    Permit();
    if (!rom || st->dev || upc_is_upterm(rom)) {
        say("console.device: none, or UP-Term's already");
        return;
    }
    if (upc_patched_vector(SysBase, rom)) {
        say("console.device: patched (SetFunction), left alone");
        return;
    }
    seg = load(DOSBase, uprom_device, uprom_device_end);
    if (!seg) {
        say("console.device: InternalLoadSeg failed");
        return;
    }
    if (!upc_device_start(SysBase, st, rom, seg)) {
        UnLoadSeg(seg);
        say("console.device: did not start");
        return;
    }
    say("console.device is UP-Term");
}

static void con_on(struct DosLibrary *DOSBase, upc_state *st)
{
    int bad, why;
    if (!st->seg)
        st->seg = load(DOSBase, uprom_handler, uprom_handler_end);
    if (!st->seg) {
        say("handler: InternalLoadSeg failed");
        return;
    }
    switch (upc_con_switch(DOSBase, st, &bad, &why)) {
    case UPC_SW_DONE:
        say("CON: and RAW: are UP-Term");
        break;
    case UPC_SW_ALREADY:
        say("CON: is UP-Term already");
        break;
    default:
        say(why == UPC_MISSING ? "CON:/RAW: not switched: an entry is missing"
                               : "CON:/RAW: not switched: not the ROM's entries");
        break;
    }
    mount_xcon(DOSBase, st->seg);
}

ULONG uprom_init(__reg("a6") struct ExecBase *SysBase)
{
    struct DosLibrary *DOSBase;
    upc_state *st;
    say("start");
    DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR)"dos.library", 37);
    if (!DOSBase) {
        say("no dos.library");
        return 0;
    }
    st = upc_state_make(SysBase);
    if (!st) {
        say("no memory");
        CloseLibrary((struct Library *)DOSBase);
        return 0;
    }
    ObtainSemaphore(&st->ss);
    device_on(SysBase, DOSBase, st);
    con_on(DOSBase, st);
    ReleaseSemaphore(&st->ss);
    CloseLibrary((struct Library *)DOSBase);
    return 0;
}
