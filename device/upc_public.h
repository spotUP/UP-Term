/* upc_public.h -- what programs outside the device know about UP-Term's
 * console.device (plan 2026-09-30-console-device.md, DD13, DD16, D1.5):
 * how to tell it is in, the flag that asks it for the ROM's unit, and its
 * private commands. Used by the device, C:UPConsole and the XCON: handler. */
#ifndef UPC_PUBLIC_H
#define UPC_PUBLIC_H

#include <exec/types.h>
#include <exec/libraries.h>

#define UPC_MAGIC 0x55505443UL          /* 'UPTC' */
#define UPC_MAGIC_OFFSET 36             /* in the device base, after struct Library */
/* char *: the device's own "console.device" string. UPConsole names the
 * device with it: a name must live as long as the node, and the device
 * outlives the command that installs it. */
#define UPC_CONNAME_OFFSET 44

/* OpenDevice flag: open the ROM's unit instead (DD16); send it only to a
 * device upc_is_upterm() accepts */
#define UPCONFLAG_ROM 0x40000000UL

/* private commands (D1.5) */
#define UPCMD_STATS 0x7F00              /* io_Data: struct upc_stats, io_Length its size */
#define UPCMD_DIE   0x7FF0              /* the device's own: Close -> unit process */
/* the exclusion list (DD16): programs, by task name, that get the ROM's
 * units. EXCLUDE: io_Data a name to add, or 0 / io_Length 0 to clear
 * (IOERR_BADLENGTH when full). EXCLUDED: the names into io_Data, each
 * '\n'-ended, io_Actual the bytes. Both on the library unit. */
#define UPCMD_EXCLUDE  0x7F01
#define UPCMD_EXCLUDED 0x7F02
#define UPC_MAXEXCLUDE 8
#define UPC_EXCLUDE_LEN 32

struct upc_stats {
    ULONG units;                        /* units open now */
    ULONG written;                      /* bytes written, all units, since install */
    ULONG answered;                     /* reads answered */
    ULONG dropped;                      /* input events and bytes lost to full buffers */
    ULONG events;                       /* input events the units handled */
    ULONG mice;                         /* of them mouse events */
    ULONG drags;                        /* selections started (SNIPMAP) */
    ULONG pointer;                      /* the last pointer a unit saw: x << 16 | y (window) */
};

/* Is this device base UP-Term's? (never by version: DD13) */
#define upc_is_upterm(d) ((d) && (d)->lib_PosSize >= UPC_MAGIC_OFFSET + 8 && \
                          *(ULONG *)((UBYTE *)(d) + UPC_MAGIC_OFFSET) == UPC_MAGIC)

#endif
