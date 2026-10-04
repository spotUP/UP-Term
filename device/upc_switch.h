/* upc_switch.h -- switching CON:/RAW: and console.device to UP-Term, shared
 * by C:UPConsole (loads from disk) and the ROM module device/uprom.c (loads
 * from the Kickstart image; ledger R1). No C library, no writable statics:
 * the ROM module runs this code from ROM. The library bases come in as
 * parameters named as the inline calls expect them (SysBase, DOSBase). */
#ifndef UPC_SWITCH_H
#define UPC_SWITCH_H

#include <exec/types.h>
#include <exec/semaphores.h>
#include <exec/resident.h>
#include <exec/execbase.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>

#define UPC_SEM_NAME "UP-Term console"

/* what CON ON replaced in one DosList entry */
typedef struct {
    BPTR seglist;
    BSTR handler;
    LONG stack, pri, globvec;
    BPTR startup;
} upc_saved_entry;

/* the public SignalSemaphore "UP-Term console": every later UPConsole run
 * finds it again, whoever switched (the command or the ROM module) */
typedef struct {
    struct SignalSemaphore ss;
    char name[sizeof(UPC_SEM_NAME)];
    BPTR seg;          /* the handler, loaded once, never unloaded */
    int con_on;
    upc_saved_entry saved[2]; /* CON, RAW */
    /* DEVICE ON: our device, the ROM's node it replaced, our own name */
    struct Library *dev, *rom;
    char *dev_name;
    BPTR dev_seg;
} upc_state;

/* why a CON/RAW entry is not the ROM's (upc_con_pristine) */
#define UPC_PRISTINE   0
#define UPC_SERVED_BY  1  /* dn_Handler names a handler */
#define UPC_NOT_ROMS   2  /* other values than DP3's */
#define UPC_MISSING    3  /* no such entry */

/* upc_con_switch results */
#define UPC_SW_DONE     0
#define UPC_SW_ALREADY  1
#define UPC_SW_CONFLICT 2 /* *bad = the entry (0 CON, 1 RAW), *why = UPC_* */

extern const char *const upc_entry_name[2];

upc_state *upc_state_find(struct ExecBase *SysBase);
upc_state *upc_state_make(struct ExecBase *SysBase);
/* the DosList entry `name` (the caller holds a DosList lock) */
struct DeviceNode *upc_find_entry(struct DosLibrary *DOSBase, struct DosList *locked, const char *name);
int upc_con_pristine(struct DosLibrary *DOSBase, struct DeviceNode *d, int raw);
/* CON ON with st->seg loaded; the caller holds st->ss */
int upc_con_switch(struct DosLibrary *DOSBase, upc_state *st, int *bad, int *why);
/* the RomTag after the device file's first 4 bytes (moveq #-1,d0; rts) */
struct Resident *upc_find_romtag(BPTR seg);
/* a vector -6..-72 of the ROM console.device that points outside its ROM
 * module (SetFunction): its LVO, else 0 */
int upc_patched_vector(struct ExecBase *SysBase, struct Library *rom);
/* DEVICE ON with `seg` loaded: InitResident, then our node takes the name
 * console.device from `rom`. 0 when it is not UP-Term's or did not start
 * (`seg` is then the caller's to unload). The caller holds st->ss. */
struct Library *upc_device_start(struct ExecBase *SysBase, upc_state *st, struct Library *rom, BPTR seg);

#endif
