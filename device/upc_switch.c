/* upc_switch.c -- see upc_switch.h. Moved out of upconsole.c unchanged in
 * behaviour so the ROM module (device/uprom.c) switches exactly as
 * UPConsole CON ON / DEVICE ON do. */
#include <exec/memory.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "upc_public.h"
#include "upc_switch.h"

const char *const upc_entry_name[2] = { "CON", "RAW" };

upc_state *upc_state_find(struct ExecBase *SysBase)
{
    upc_state *st;
    Forbid();
    st = (upc_state *)FindSemaphore((STRPTR)UPC_SEM_NAME);
    Permit();
    return st;
}

upc_state *upc_state_make(struct ExecBase *SysBase)
{
    upc_state *st = upc_state_find(SysBase);
    const char *s = UPC_SEM_NAME;
    int i;
    if (st)
        return st;
    st = (upc_state *)AllocMem(sizeof(*st), MEMF_PUBLIC | MEMF_CLEAR);
    if (!st)
        return 0;
    for (i = 0; s[i]; i++)
        st->name[i] = s[i];
    st->ss.ss_Link.ln_Name = st->name;
    st->ss.ss_Link.ln_Pri = 0;
    AddSemaphore(&st->ss); /* stays for good: it owns the loaded handler */
    return st;
}

struct DeviceNode *upc_find_entry(struct DosLibrary *DOSBase, struct DosList *locked, const char *name)
{
    return (struct DeviceNode *)FindDosEntry(locked, (STRPTR)name, LDF_DEVICES);
}

/* Is this the entry the ROM leaves (DP3's pristine values)? */
int upc_con_pristine(struct DosLibrary *DOSBase, struct DeviceNode *d, int raw)
{
    LONG stack = DOSBase->dl_lib.lib_Version >= 47 ? 4096 : 3200;
    if (d->dn_Handler && ((const UBYTE *)BADDR(d->dn_Handler))[0])
        return UPC_SERVED_BY;
    if (d->dn_Type != 0 || d->dn_GlobalVec != -1 || d->dn_Priority != 5 || !d->dn_SegList ||
        d->dn_Startup != (BPTR)raw || d->dn_StackSize != stack)
        return UPC_NOT_ROMS;
    return UPC_PRISTINE;
}

int upc_con_switch(struct DosLibrary *DOSBase, upc_state *st, int *bad, int *why)
{
    struct DeviceNode *d[2];
    struct DosList *dl;
    int i;
    if (st->con_on)
        return UPC_SW_ALREADY;
    *bad = -1;
    *why = UPC_PRISTINE;
    dl = LockDosList(LDF_DEVICES | LDF_WRITE);
    for (i = 0; i < 2 && *bad < 0; i++) {
        d[i] = upc_find_entry(DOSBase, dl, upc_entry_name[i]);
        *why = d[i] ? upc_con_pristine(DOSBase, d[i], i) : UPC_MISSING;
        if (*why != UPC_PRISTINE)
            *bad = i;
    }
    if (*bad < 0)
        for (i = 0; i < 2; i++) {
            upc_saved_entry *s = &st->saved[i];
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
    if (*bad >= 0)
        return UPC_SW_CONFLICT;
    st->con_on = 1;
    return UPC_SW_DONE;
}

int upc_patched_vector(struct ExecBase *SysBase, struct Library *rom)
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

struct Resident *upc_find_romtag(BPTR seg)
{
    UWORD *code = (UWORD *)((UBYTE *)BADDR(seg) + 4);
    struct Resident *rt = (struct Resident *)(code + 2);
    if (code[0] != 0x70FF || code[1] != 0x4E75 || rt->rt_MatchWord != RTC_MATCHWORD || rt->rt_MatchTag != rt)
        return 0;
    return rt;
}

struct Library *upc_device_start(struct ExecBase *SysBase, upc_state *st, struct Library *rom, BPTR seg)
{
    struct Resident *rt = upc_find_romtag(seg);
    struct Library *ours = rt ? (struct Library *)InitResident(rt, seg) : 0;
    if (!ours || !upc_is_upterm(ours))
        return 0;
    Forbid();
    Remove(&rom->lib_Node);
    st->dev_name = ours->lib_Node.ln_Name;
    ours->lib_Node.ln_Name = *(char **)((UBYTE *)ours + UPC_CONNAME_OFFSET); /* the device's own string */
    Permit();
    st->dev = ours;
    st->rom = rom;
    st->dev_seg = seg;
    return ours;
}
