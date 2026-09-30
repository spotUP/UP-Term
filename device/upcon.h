/* upcon -- UP-Term's console.device (plan 2026-09-30-console-device.md,
 * phase D1): the device base, the unit, and what the three parts share.
 *
 *   upcon_rom.s     first in the file: the RomTag and the vectors that go to
 *                   the ROM device unchanged (DD11)
 *   upcon_device.c  Init, Open, Close, Expunge, BeginIO, AbortIO
 *   upcon_unit.c    the unit process: one per unit (DD5), on render/vtwin
 *   upcon_input.c   the input handler that feeds the units (DD6, DD7)
 *   upc_core.c      the portable logic (read queue, event ring, routing)
 *
 * No mutable statics except library bases: every unit runs this code. */
#ifndef UPCON_H
#define UPCON_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/devices.h>
#include <exec/io.h>
#include <exec/semaphores.h>
#include <exec/interrupts.h>
#include <devices/conunit.h>
#include <devices/console.h>
#include <dos/dos.h>
#include "upc_public.h"
#include "upc_core.h"
#include "../render/vtwin.h"

#define UPC_ROMBASE_OFFSET 40           /* upcon_rom.s reads the ROM base here */
#define UPC_MAXUNITS 16                 /* windows served at once */

struct upc_unit;

struct upc_base {
    struct Library lib;                 /* 0..33 */
    UWORD pad;                          /* 34 */
    ULONG magic;                        /* 36 (UPC_MAGIC_OFFSET): UPC_MAGIC */
    struct Library *rom;                /* 40: the ROM console.device (UPC_ROMBASE_OFFSET) */
    char *con_name;                     /* 44 (UPC_CONNAME_OFFSET): "console.device", ours */
    struct IOStdReq romlib;             /* CONU_LIBRARY on the ROM, open for good */
    BPTR seglist;
    struct SignalSemaphore lock;        /* units[], the input handler */
    struct upc_unit *units[UPC_MAXUNITS];
    struct Interrupt ih;                /* our input handler (DD6) */
    int ih_added;
    ULONG written, answered, dropped;   /* UPCMD_STATS */
    ULONG events, mice, drags, pointer;
};

struct upc_unit {
    struct ConUnit cu;                  /* first: io_Unit points here; cu_MP is the unit's port */
    struct upc_base *base;
    LONG unitno;                        /* CONU_STANDARD, CONU_CHARMAP, CONU_SNIPMAP */
    ULONG flags;                        /* CONFLAG_* */
    struct Window *win;                 /* the opener's window */
    int slot;                           /* index in base->units */
    int opens;
    struct Process *proc;
    struct Task *task;
    ULONG ev_sig;                       /* the input handler signals this */
    struct SignalSemaphore lock;        /* rq: the caller's quick reads and the process */
    upc_rq rq;
    upc_evring ring;
    vtwin w;
    struct Message startup;             /* Open waits for the process on it */
    struct MsgPort *startup_reply;
    int ok;                             /* the process attached its window */
    WORD inner_w, inner_h;              /* the window's inner size when last seen (DD8) */
};

extern struct ExecBase *SysBase;

/* DEBUG=1 (UPCON_DEBUG): a trace to the serial port through exec's
 * RawPutChar -- no DOS, so safe in Open under Forbid, in the unit process
 * and anywhere else (the rig writes the port to build/rig/serial.log).
 * UPC_DBG(what, text or 0, a number). */
#ifdef UPCON_DEBUG
void upc_dbg(const char *what, const char *s, LONG v);
#define UPC_DBG(w, s, v) upc_dbg(w, s, (LONG)(v))
#else
#define UPC_DBG(w, s, v)
#endif

/* upcon_unit.c */
void upc_unit_entry(void);
/* upcon_input.c */
int  upc_input_add(struct upc_base *b);
void upc_input_rem(struct upc_base *b);
/* upcon_device.c: a read the unit process answered from the queue */
void upc_reply_io(struct IOStdReq *io, long actual, BYTE error);

#endif
