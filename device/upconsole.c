/* UPConsole -- switch the system's CON: and RAW: to UP-Term (the vtcon
 * handler) and back: H5.4 of thoughts/shared/plans/2026-09-30-console-device.md
 * (design DD2, DD19, DD20, DD21).
 *
 *   UPConsole CON ON      new CON:/RAW: windows run L:vtcon-handler
 *   UPConsole CON OFF     back to the ROM con-handler (open windows keep theirs)
 *   UPConsole STATUS      which handler serves CON:/RAW:, and console.device
 *   HANDLER <file>        another handler file (default L:vtcon-handler; the rig)
 *
 * CON ON loads the handler once and keeps it loaded for good (open windows
 * may run it), then, under the DosList write lock, points the CON and RAW
 * entries at it. What it replaces is kept in a public SignalSemaphore named
 * "UP-Term console", found again by later runs, and put back by CON OFF.
 * It refuses, and changes nothing, when:
 *   - the entries are not the ones the ROM leaves (DP3, measured on 39.106,
 *     40.63, 40.71 and 47.115): another console replacement (KingCON,
 *     ViNCEd, ...) is installed -- conflicts refuse, never stack (DD21);
 * (3.2's Shell uses the con-handler's medium mode, SetMode 2: UP-Term has
 * it since T3 -- plan 2026-10-03-amigaos32.md.)
 *
 *   UPConsole DEVICE ON   console.device is UP-Term's (DEVS:up-console.device,
 *                         or FILE <path>) for every unit opened from now on
 *   UPConsole DEVICE OFF  the ROM's again; UP-Term's code stays until its
 *                         last unit closes
 * DEVICE ON (DD15, DD21) loads the device, gives its RomTag to InitResident
 * (it arrives as "UP-Term console.device" and keeps the ROM device open for
 * good), then, under Forbid, takes the ROM node out of the device list and
 * names ours "console.device". It refuses when a console.device vector is
 * patched (points outside the ROM module: SetFunction), or when UP-Term's
 * device is on already.
 *
 *   UPConsole EXCLUDE <task name>  that program gets the ROM's console units
 *   UPConsole EXCLUDE CLEAR        nobody is excluded (DD16: the escape hatch
 *                                  for a program that needs ROM internals) */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <dos/rdargs.h>
#include <exec/resident.h>
#include <exec/io.h>
#include <devices/conunit.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "upc_public.h"

extern struct DosLibrary *DOSBase;
#define UPASSIGN_DOS
#include "../config/upassign.h"

static const char vers[] = "$VER: UPConsole 0.1 (30.9.2026)";

#define SEM_NAME "UP-Term console"

/* what CON ON replaced in one DosList entry */
typedef struct {
    BPTR seglist;
    BSTR handler;
    LONG stack, pri, globvec;
    BPTR startup;
} saved_entry;

typedef struct {
    struct SignalSemaphore ss;
    char name[sizeof(SEM_NAME)];
    BPTR seg;          /* the handler, loaded once, never unloaded */
    int con_on;
    saved_entry saved[2]; /* CON, RAW */
    /* DEVICE ON: our device, the ROM's node it replaced, our own name */
    struct Library *dev, *rom;
    char *dev_name;
    BPTR dev_seg;
} upc_state;


static const char *const entry_name[2] = { "CON", "RAW" };

/* Under the DosList lock (either kind): the device entry `name`. */
static struct DeviceNode *find_entry(struct DosList *locked, const char *name)
{
    return (struct DeviceNode *)FindDosEntry(locked, (STRPTR)name, LDF_DEVICES);
}

static void bstr_copy(char *out, int max, BSTR b)
{
    const UBYTE *s = b ? (const UBYTE *)BADDR(b) : 0;
    int n = s ? s[0] : 0, i;
    if (n > max - 1)
        n = max - 1;
    for (i = 0; i < n; i++)
        out[i] = (char)s[i + 1];
    out[n] = 0;
}

/* Is this the entry the ROM leaves (DP3's pristine values)? If not, `why`
 * says what differs. */
static int pristine(struct DeviceNode *d, int raw, char *why, int max)
{
    char h[80];
    LONG stack = DOSBase->dl_lib.lib_Version >= 47 ? 4096 : 3200;
    bstr_copy(h, sizeof(h), d->dn_Handler);
    if (h[0]) {
        snprintf(why, (size_t)max, "served by %s", h);
        return 0;
    }
    if (d->dn_Type != 0 || d->dn_GlobalVec != -1 || d->dn_Priority != 5 || !d->dn_SegList ||
        d->dn_Startup != (BPTR)raw || d->dn_StackSize != stack) {
        snprintf(why, (size_t)max, "not the ROM's entry (type %ld stack %ld pri %ld startup %ld globvec %ld)",
                 (long)d->dn_Type, (long)d->dn_StackSize, (long)d->dn_Priority, (long)d->dn_Startup,
                 (long)d->dn_GlobalVec);
        return 0;
    }
    return 1;
}

static int word_is(const char *s, const char *upper)
{
    for (; *s && *upper; s++, upper++)
        if ((*s >= 'a' && *s <= 'z' ? *s - 32 : *s) != *upper)
            return 0;
    return !*s && !*upper;
}

static upc_state *find_state(void)
{
    upc_state *st;
    Forbid();
    st = (upc_state *)FindSemaphore((STRPTR)SEM_NAME);
    Permit();
    return st;
}

static upc_state *make_state(void)
{
    upc_state *st = find_state();
    if (st)
        return st;
    st = (upc_state *)AllocMem(sizeof(*st), MEMF_PUBLIC | MEMF_CLEAR);
    if (!st)
        return 0;
    strcpy(st->name, SEM_NAME);
    st->ss.ss_Link.ln_Name = st->name;
    st->ss.ss_Link.ln_Pri = 0;
    AddSemaphore(&st->ss); /* stays for good: it owns the loaded handler */
    return st;
}

static int con_on(const char *file)
{
    upc_state *st;
    struct DeviceNode *d[2];
    struct DosList *dl;
    char why[120];
    int i, bad = -1;
    st = make_state();
    if (!st) {
        printf("UPConsole: no memory\n");
        return RETURN_FAIL;
    }
    ObtainSemaphore(&st->ss);
    if (st->con_on) {
        ReleaseSemaphore(&st->ss);
        printf("UPConsole: CON: is UP-Term already\n");
        return RETURN_OK;
    }
    if (!st->seg)
        st->seg = LoadSeg((STRPTR)file); /* before the DosList lock: it does DOS I/O */
    if (!st->seg) {
        ReleaseSemaphore(&st->ss);
        printf("UPConsole: cannot load %s\n", file);
        return RETURN_FAIL;
    }
    why[0] = 0;
    dl = LockDosList(LDF_DEVICES | LDF_WRITE);
    for (i = 0; i < 2 && bad < 0; i++) {
        d[i] = find_entry(dl, entry_name[i]);
        if (!d[i]) {
            strcpy(why, "missing");
            bad = i;
        } else if (!pristine(d[i], i, why, sizeof(why))) {
            bad = i;
        }
    }
    if (bad < 0)
        for (i = 0; i < 2; i++) {
            saved_entry *s = &st->saved[i];
            s->seglist = d[i]->dn_SegList;
            s->handler = d[i]->dn_Handler;
            s->stack = d[i]->dn_StackSize;
            s->pri = d[i]->dn_Priority;
            s->globvec = d[i]->dn_GlobalVec;
            s->startup = d[i]->dn_Startup;
            d[i]->dn_SegList = st->seg;
            d[i]->dn_StackSize = 16000;
            d[i]->dn_GlobalVec = -1;
        }
    UnLockDosList(LDF_DEVICES | LDF_WRITE);
    if (bad >= 0) {
        ReleaseSemaphore(&st->ss);
        printf("UPConsole: not switched: %s: is %s.\n"
               "Remove that console replacement first; UP-Term does not stack on another.\n",
               entry_name[bad], why);
        return RETURN_WARN;
    }
    st->con_on = 1;
    ReleaseSemaphore(&st->ss);
    printf("UPConsole: new CON: and RAW: windows are UP-Term\n");
    return RETURN_OK;
}

static int con_off(void)
{
    upc_state *st = find_state();
    struct DosList *dl;
    int i, changed = 0;
    if (!st) {
        printf("UPConsole: CON: is not UP-Term\n");
        return RETURN_OK;
    }
    ObtainSemaphore(&st->ss);
    if (!st->con_on) {
        ReleaseSemaphore(&st->ss);
        printf("UPConsole: CON: is not UP-Term\n");
        return RETURN_OK;
    }
    dl = LockDosList(LDF_DEVICES | LDF_WRITE);
    for (i = 0; i < 2; i++) {
        struct DeviceNode *d = find_entry(dl, entry_name[i]);
        saved_entry *s = &st->saved[i];
        if (!d || d->dn_SegList != st->seg) {
            changed |= 1 << i; /* someone else changed it since: leave it */
            continue;
        }
        d->dn_SegList = s->seglist;
        d->dn_Handler = s->handler;
        d->dn_StackSize = s->stack;
        d->dn_Priority = s->pri;
        d->dn_GlobalVec = s->globvec;
        d->dn_Startup = s->startup;
    }
    UnLockDosList(LDF_DEVICES | LDF_WRITE);
    st->con_on = 0; /* the handler stays loaded: open windows may run it */
    ReleaseSemaphore(&st->ss);
    for (i = 0; i < 2; i++)
        if (changed & (1 << i))
            printf("UPConsole: %s: was changed by another program since CON ON; left as it is\n",
                   entry_name[i]);
    printf("UPConsole: new CON: and RAW: windows are the ROM's again\n");
    return changed ? RETURN_WARN : RETURN_OK;
}

/* TRACE: each step of DEVICE ON/OFF to the serial port (exec RawPutChar,
 * LVO -516): a step that takes the machine down still shows how far it got */
static int tracing;
static void ser_putc(__reg("d0") char ch) =
    "\tmove.l\ta6,-(sp)\n\tmove.l\t4.w,a6\n\tjsr\t-516(a6)\n\tmove.l\t(sp)+,a6";
static void trace(const char *s)
{
    if (!tracing)
        return;
    while (*s)
        ser_putc(*s++);
    ser_putc('\n');
}

static void trace_hex(const char *s, ULONG v)
{
    int k;
    if (!tracing)
        return;
    while (*s)
        ser_putc(*s++);
    ser_putc(' ');
    for (k = 28; k >= 0; k -= 4)
        ser_putc("0123456789abcdef"[(v >> k) & 15]);
    ser_putc('\n');
}

/* the seglist's hunks: address and size of each */
static void trace_seg(BPTR seg)
{
    while (seg && tracing) {
        ULONG *p = (ULONG *)BADDR(seg);
        trace_hex("  hunk", (ULONG)p);
        trace_hex("  size", p[-1]);
        seg = (BPTR)p[0];
    }
}

/* A console.device vector patched with SetFunction points outside the ROM
 * module: its `jmp abs.l` target is not in [resident, rt_EndSkip). */
static int patched_vector(struct Library *rom)
{
    struct Resident *rt = FindResident((STRPTR)"console.device");
    int lvo;
    if (!rt)
        return 0; /* not a ROM module (a loaded one): nothing to compare with */
    for (lvo = 6; lvo <= 72; lvo += 6) {
        UBYTE *v = (UBYTE *)rom - lvo;
        ULONG target;
        if (v[0] != 0x4E || v[1] != 0xF9)
            return lvo; /* not a jmp: someone rewrote it */
        target = *(ULONG *)(v + 2);
        if (target < (ULONG)rt || target >= (ULONG)rt->rt_EndSkip)
            return lvo;
    }
    return 0;
}

/* The RomTag right after the file's first 4 bytes (moveq #-1,d0; rts). */
static struct Resident *find_romtag(BPTR seg)
{
    UWORD *code = (UWORD *)((UBYTE *)BADDR(seg) + 4);
    struct Resident *rt = (struct Resident *)(code + 2);
    if (code[0] != 0x70FF || code[1] != 0x4E75 || rt->rt_MatchWord != RTC_MATCHWORD || rt->rt_MatchTag != rt)
        return 0;
    return rt;
}

static void unload_retired(upc_state *st);

static int device_on(const char *file)
{
    upc_state *st = make_state();
    struct Library *rom, *ours;
    struct Resident *rt;
    BPTR seg;
    int lvo;
    if (!st) {
        printf("UPConsole: no memory\n");
        return RETURN_FAIL;
    }
    ObtainSemaphore(&st->ss);
    Forbid();
    rom = (struct Library *)FindName(&SysBase->DeviceList, (STRPTR)"console.device");
    Permit();
    if (st->dev || upc_is_upterm(rom)) {
        ReleaseSemaphore(&st->ss);
        printf("UPConsole: console.device is UP-Term already\n");
        return RETURN_OK;
    }
    if (!rom) {
        ReleaseSemaphore(&st->ss);
        printf("UPConsole: not switched: there is no console.device\n");
        return RETURN_WARN;
    }
    unload_retired(st); /* gone since the last run: forget it */
    if (st->dev_seg) {
        /* switched off while windows still used it: it is still loaded,
         * under its own name, its expunge pending. Switch that one back
         * on (refusing left DEVICE ON failing until every such window
         * closed, the install's DEVICE step among them). */
        struct Library *retired;
        Forbid();
        retired = (struct Library *)FindName(&SysBase->DeviceList, (STRPTR)st->dev_name);
        if (retired && upc_is_upterm(retired)) {
            retired->lib_Flags &= ~LIBF_DELEXP; /* no expunge at its last close now */
            Remove(&rom->lib_Node);
            retired->lib_Node.ln_Name = *(char **)((UBYTE *)retired + UPC_CONNAME_OFFSET);
            st->dev = retired;
            st->rom = rom;
        }
        Permit();
        ReleaseSemaphore(&st->ss);
        if (!st->dev) {
            printf("UPConsole: not switched: the UP-Term device switched off before is not found\n");
            return RETURN_WARN;
        }
        printf("UPConsole: console.device is UP-Term again (the one still in use)\n");
        return RETURN_OK;
    }
    lvo = patched_vector(rom);
    if (lvo) {
        ReleaseSemaphore(&st->ss);
        printf("UPConsole: not switched: console.device vector -%d is patched (SetFunction).\n"
               "Remove that patch first; UP-Term does not stack on another.\n", lvo);
        return RETURN_WARN;
    }
    seg = LoadSeg((STRPTR)file);
    if (!seg) {
        ReleaseSemaphore(&st->ss);
        printf("UPConsole: cannot load %s\n", file);
        return RETURN_FAIL;
    }
    trace_hex("upconsole on: seg", (ULONG)seg);
    trace_seg(seg);
    rt = find_romtag(seg);
    ours = rt ? (struct Library *)InitResident(rt, seg) : 0;
    if (!ours || !upc_is_upterm(ours)) {
        UnLoadSeg(seg);
        ReleaseSemaphore(&st->ss);
        printf("UPConsole: %s is not UP-Term's console.device, or it did not start\n", file);
        return RETURN_FAIL;
    }
    trace_hex("upconsole on: base", (ULONG)ours);
    trace_hex("  negsize", ours->lib_NegSize);
    trace_hex("  possize", ours->lib_PosSize);
    trace_seg(seg);
    Forbid();
    Remove(&rom->lib_Node);
    st->dev_name = ours->lib_Node.ln_Name;
    ours->lib_Node.ln_Name = *(char **)((UBYTE *)ours + UPC_CONNAME_OFFSET); /* the device's own string */
    Permit();
    st->dev = ours;
    st->rom = rom;
    st->dev_seg = seg;
    ReleaseSemaphore(&st->ss);
    printf("UPConsole: console.device is UP-Term now (new units)\n");
    return RETURN_OK;
}

/* A device DEVICE OFF left for its last unit's Close: once its node is gone,
 * its Expunge ran and the system unloaded its code with the seglist Expunge
 * returned (measured on KS 40.63: RemDevice frees the whole seglist; our own
 * UnLoadSeg after it was a double free that took the machine down). Every
 * run forgets it then. */
static void unload_retired(upc_state *st)
{
    int gone;
    if (!st || !st->dev_seg || st->dev)
        return;
    Forbid();
    gone = !FindName(&SysBase->DeviceList, (STRPTR)st->dev_name);
    Permit();
    trace(gone ? "upconsole retired: gone (unloaded by RemDevice)" : "upconsole retired: still there");
    if (gone)
        st->dev_seg = 0;
}

static int device_off(void)
{
    upc_state *st = find_state();
    struct Library *ours;
    if (!st || !st->dev) {
        printf("UPConsole: console.device is not UP-Term\n");
        return RETURN_OK;
    }
    ObtainSemaphore(&st->ss);
    ours = st->dev;
    trace_hex("upconsole off: base", (ULONG)ours);
    trace_hex("  negsize", ours->lib_NegSize);
    trace_hex("  possize", ours->lib_PosSize);
    trace_seg(st->dev_seg);
    trace("upconsole off: swap");
    Forbid();
    Remove(&ours->lib_Node);
    ours->lib_Node.ln_Name = st->dev_name;
    Enqueue(&SysBase->DeviceList, &st->rom->lib_Node);
    AddTail(&SysBase->DeviceList, &ours->lib_Node);
    trace("upconsole off: RemDevice");
    /* its Expunge frees it now, or at its last unit's Close (LIBF_DELEXP) */
    RemDevice((struct Device *)ours);
    trace("upconsole off: RemDevice returned");
    Permit();
    trace("upconsole off: permitted");
    st->dev = 0;
    st->rom = 0;
    unload_retired(st);
    trace("upconsole off: unloaded");
    ReleaseSemaphore(&st->ss);
    printf("UPConsole: console.device is the ROM's again%s\n",
           st->dev_seg ? " (UP-Term's stays until its units close)" : "");
    return RETURN_OK;
}

/* A private command to UP-Term's device on the library unit; -1 when
 * console.device is not UP-Term's, else io_Error. */
static int lib_cmd(UWORD cmd, APTR data, ULONG len, ULONG *actual)
{
    struct MsgPort *p = CreateMsgPort();
    struct IOStdReq *io = p ? (struct IOStdReq *)CreateIORequest(p, sizeof(struct IOStdReq)) : 0;
    int rc = -1;
    if (io && !OpenDevice((STRPTR)"console.device", (ULONG)CONU_LIBRARY, (struct IORequest *)io, 0)) {
        if (upc_is_upterm((struct Library *)io->io_Device)) {
            io->io_Command = cmd;
            io->io_Data = data;
            io->io_Length = len;
            rc = DoIO((struct IORequest *)io);
            if (actual)
                *actual = io->io_Actual;
        }
        CloseDevice((struct IORequest *)io);
    }
    if (io)
        DeleteIORequest((struct IORequest *)io);
    if (p)
        DeleteMsgPort(p);
    return rc;
}

static int exclude(const char *name)
{
    int clear = word_is(name, "CLEAR"), rc;
    rc = lib_cmd(UPCMD_EXCLUDE, clear ? 0 : (APTR)name, clear ? 0 : strlen(name) + 1, 0);
    if (rc < 0) {
        printf("UPConsole: EXCLUDE works with UP-Term's console.device (DEVICE ON)\n");
        return RETURN_WARN;
    }
    if (rc) {
        printf("UPConsole: the list is full (%d names) or the name too long\n", UPC_MAXEXCLUDE);
        return RETURN_WARN;
    }
    if (clear)
        printf("UPConsole: no program is excluded now\n");
    else
        printf("UPConsole: %s gets the ROM's console units from its next open\n", name);
    return RETURN_OK;
}

static void status(void)
{
    upc_state *st = find_state();
    struct DosList *dl;
    char h[2][80], why[120];
    int i, ours[2], rom[2];
    dl = LockDosList(LDF_DEVICES | LDF_READ);
    for (i = 0; i < 2; i++) {
        struct DeviceNode *d = find_entry(dl, entry_name[i]);
        h[i][0] = 0;
        ours[i] = d && st && st->seg && d->dn_SegList == st->seg;
        rom[i] = d && !ours[i] && pristine(d, i, why, sizeof(why));
        if (d && !ours[i] && !rom[i])
            bstr_copy(h[i], sizeof(h[i]), d->dn_Handler);
    }
    UnLockDosList(LDF_DEVICES | LDF_READ);
    for (i = 0; i < 2; i++)
        printf("%s: %s%s\n", entry_name[i], ours[i] ? "UP-Term" : rom[i] ? "ROM" : "other",
               !ours[i] && !rom[i] && h[i][0] ? (sprintf(why, " (%s)", h[i]), why) : "");
    {
        struct Library *d;
        Forbid();
        d = (struct Library *)FindName(&SysBase->DeviceList, (STRPTR)"console.device");
        i = upc_is_upterm(d);
        Permit();
        printf("console.device: %s\n", i ? "UP-Term" : d ? "ROM" : "none");
    }
    {
        static char names[UPC_MAXEXCLUDE * UPC_EXCLUDE_LEN + 1];
        ULONG n = 0, k;
        if (lib_cmd(UPCMD_EXCLUDED, names, sizeof(names) - 1, &n) == 0 && n) {
            names[n] = 0;
            for (k = 0; k < n; k++)
                if (names[k] == '\n')
                    names[k] = ' ';
            printf("excluded (the ROM's units): %s\n", names);
        }
    }
    /* the kit's VERSIONS file (the Install copies it): its first line says
     * which kit and which commit this is */
    {
        /* UP-Term: is an assign: with none (not installed, or removed) the Open
         * must answer, not ask for a volume in a requester */
        struct Process *me = (struct Process *)FindTask(0);
        APTR oldwin = me->pr_WindowPtr;
        BPTR fh;
        upassign_ensure(); /* made from ENVARC:up-term/Dir when a boot lost it */
        me->pr_WindowPtr = (APTR)-1;
        fh = Open((STRPTR)"UP-Term:VERSIONS", MODE_OLDFILE);
        me->pr_WindowPtr = oldwin;
        if (fh) {
            char line[120];
            if (FGets(fh, (STRPTR)line, sizeof(line)) && line[0]) {
                int n = (int)strlen(line);
                while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
                    line[--n] = 0;
                printf("kit: %s\n", line);
            }
            Close(fh);
        }
    }
}

int main(void)
{
    static const char tmpl[] = "CON/K,DEVICE/K,EXCLUDE/K,STATUS/S,HANDLER/K,FILE/K,TRACE/S";
    LONG args[7] = { 0, 0, 0, 0, 0, 0, 0 };
    struct RDArgs *rd = ReadArgs((STRPTR)tmpl, args, 0);
    int rc = RETURN_OK;
    if (!rd) {
        PrintFault(IoErr(), (STRPTR)"UPConsole");
        return RETURN_FAIL;
    }
    if (!vers[0])
        rc = RETURN_FAIL; /* keeps the version string linked in */
    tracing = args[6] != 0;
    {
        upc_state *st = find_state();
        if (st) {
            ObtainSemaphore(&st->ss);
            unload_retired(st);
            ReleaseSemaphore(&st->ss);
        }
    }
    if (args[2])
        rc = exclude((const char *)args[2]);
    if (args[1]) {
        const char *v = (const char *)args[1];
        if (word_is(v, "ON"))
            rc = device_on(args[5] ? (const char *)args[5] : "DEVS:up-console.device");
        else if (word_is(v, "OFF"))
            rc = device_off();
        else {
            printf("UPConsole: DEVICE takes ON or OFF\n");
            rc = RETURN_ERROR;
        }
    }
    if (args[0]) {
        const char *v = (const char *)args[0];
        if (word_is(v, "ON"))
            rc = con_on(args[4] ? (const char *)args[4] : "L:vtcon-handler");
        else if (word_is(v, "OFF"))
            rc = con_off();
        else {
            printf("UPConsole: CON takes ON or OFF\n");
            rc = RETURN_ERROR;
        }
    }
    if (args[3] || (!args[0] && !args[1] && !args[2]))
        status();
    FreeArgs(rd);
    return rc;
}
