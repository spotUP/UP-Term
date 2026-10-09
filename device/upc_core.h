/* upc_core -- the portable part of UP-Term's console.device
 * (thoughts/shared/plans/2026-09-30-console-device.md, D1.1): no OS calls,
 * C89, tested on the host by tests/test_upcon.c (suite `upcon`). The OS
 * side (device/upcon_*.c) owns the IORequests, the window and the engine
 * and calls in here for the logic:
 *
 *   read queue   CMD_READ: input bytes, pending reads, CMD_CLEAR, paste
 *   event ring   input handler -> unit process, 64 events, drops counted
 *   upc_route    which unit an input event is for (DD7)
 *   conunit fill the public struct ConUnit fields, byte for byte (DD17)
 *
 * Nothing here allocates. Every structure lives in the unit (one per
 * console unit: no statics, every unit runs the same code). */
#ifndef UPC_CORE_H
#define UPC_CORE_H

/* ---- read queue --------------------------------------------------------
 * The unit's input stream (keymap-converted keys, key/event reports, CPR
 * replies, pasted text) and the CMD_READ requests waiting for it.
 *
 * A read is answered with what is available, 1..len bytes (NDK CMD_READ:
 * "satisfy with what is available up to io_Length"); it never waits to
 * fill up. Reads are answered in the order they came. A read that finds
 * input and no older read waiting is answered at once (quick I/O); else it
 * is queued and answered through the reply callback when input arrives.
 * A read of length 0 is answered at once with 0 bytes.
 *
 * Paste (RAmiga-V, DD10): the text goes into the stream after the bytes
 * already buffered and BEFORE any byte that arrives later (keys typed
 * while a long paste drains). The text stays the caller's until
 * paste_done is called for it (drained, or dropped by CMD_CLEAR). */

#define UPC_INBUF 512           /* buffered input bytes */
#define UPC_MAXREADS 8          /* queued CMD_READs per unit */

/* A queued read was answered: `actual` bytes are in its buffer. */
typedef void (*upc_reply_fn)(void *user, void *req, long actual);
/* A paste's text is no longer referenced. */
typedef void (*upc_paste_fn)(void *user, const unsigned char *text);

typedef struct {
    void *req;                  /* the caller's handle (the IOStdReq) */
    unsigned char *buf;
    long len;
} upc_read;

typedef struct {
    unsigned char in[UPC_INBUF];
    int head, count;            /* ring: oldest byte at head */
    int before;                 /* ring bytes that precede the paste */
    const unsigned char *paste; /* paste remainder (caller's text) */
    const unsigned char *paste_text;
    long paste_len;
    upc_read reads[UPC_MAXREADS];
    int nreads;                 /* oldest first */
    long dropped;               /* input bytes lost to a full buffer */
    long answered;              /* reads answered (quick or queued) */
    upc_reply_fn reply;
    upc_paste_fn paste_done;    /* may be 0 */
    void *user;
} upc_rq;

enum { UPC_READ_DONE, UPC_READ_QUEUED, UPC_READ_FULL };

void upc_rq_init(upc_rq *q, upc_reply_fn reply, upc_paste_fn paste_done, void *user);
/* CMD_READ: UPC_READ_DONE with *actual set (the caller replies, quick),
 * UPC_READ_QUEUED (reply comes through the callback), or UPC_READ_FULL
 * (UPC_MAXREADS already queued: the caller fails it, IOERR_UNITBUSY). */
int  upc_rq_read(upc_rq *q, void *req, unsigned char *buf, long len, long *actual);
/* Input bytes into the stream; answers queued reads. Returns how many
 * were kept; the rest are dropped and counted. */
long upc_rq_input(upc_rq *q, const unsigned char *s, long n);
/* Start a paste; 0 (and the text untouched) while another is draining. */
int  upc_rq_paste(upc_rq *q, const unsigned char *text, long n);
/* AbortIO of a read: 1 when it was queued and is now removed (the caller
 * replies IOERR_ABORTED), 0 when it is not queued (already answered). */
int  upc_rq_abort(upc_rq *q, void *req);
/* The oldest queued read, removed (for CloseDevice: abort them all), or 0. */
void *upc_rq_take(upc_rq *q);
/* CMD_CLEAR: drop the buffered input and the paste; reads stay queued. */
void upc_rq_clear(upc_rq *q);
long upc_rq_available(const upc_rq *q);

/* ---- event ring --------------------------------------------------------
 * The input handler (input.device's task, must never wait or allocate:
 * DD6) copies each event for a unit into that unit's ring; the unit process
 * takes them out. One producer, one consumer, no lock: the producer writes
 * only `head`, the consumer only `tail` (16-bit counters, free-running). */

#define UPC_EVRING 64

typedef struct {
    unsigned char cls, subclass;        /* ie_Class, ie_SubClass */
    unsigned short code, qual;          /* ie_Code, ie_Qualifier */
    short x, y;                         /* ie_X, ie_Y */
    void *addr;                         /* ie_EventAddress */
    unsigned long secs, micros;         /* ie_TimeStamp */
} upc_event;

typedef struct {
    upc_event ev[UPC_EVRING];
    volatile unsigned short head, tail;
    volatile unsigned long dropped;
} upc_evring;

void upc_ev_init(upc_evring *r);
int  upc_ev_push(upc_evring *r, const upc_event *e);   /* 0 = full, dropped */
int  upc_ev_pop(upc_evring *r, upc_event *e);          /* 0 = empty */
int  upc_ev_count(const upc_evring *r);

/* ---- routing (DD7) -----------------------------------------------------
 * Keys, mouse (RAWMOUSE and the absolute POINTERPOS/NEWPOINTERPOS moves)
 * and timer go to the unit whose window is active; window
 * events go to the unit whose window is ie_EventAddress, else the active
 * one (AROS's rule). CHANGEWINDOW (V39: moved or sized) is routed like
 * the window events: a resize hint for DD8's polling. Other classes go to
 * no unit. wins[i] is unit i's window (0 for a free slot); returns i or -1. */

#define UPC_IE_RAWKEY          0x01
#define UPC_IE_RAWMOUSE        0x02
#define UPC_IE_POINTERPOS      0x04     /* absolute moves: a tablet, a remote pointer */
#define UPC_IE_NEWPOINTERPOS   0x13
#define UPC_IE_TIMER           0x06
#define UPC_IE_CLOSEWINDOW     0x0B
#define UPC_IE_SIZEWINDOW      0x0C
#define UPC_IE_REFRESHWINDOW   0x0D
#define UPC_IE_ACTIVEWINDOW    0x11
#define UPC_IE_INACTIVEWINDOW  0x12
#define UPC_IE_CHANGEWINDOW    0x15

int upc_route(int cls, const void *evaddr, const void *active, const void *const *wins, int nwins);

/* ---- struct ConUnit (DD17) ---------------------------------------------
 * The public 296-byte struct ConUnit (NDK devices/conunit.h; offsets as in
 * the conformance matrix 6.4) written as the 68000 sees it, big-endian
 * byte by byte: on the Amiga `cu` is the unit's struct ConUnit, on the host
 * a byte array. Written: the read-only fields cu_Window..cu_YCCP,
 * cu_TabStops, cu_Mask..cu_DrawMode, cu_Font..cu_TxSpacing, cu_Modes,
 * cu_RawEvents. Never written: cu_MP, cu_KeyMapStruct (a program's
 * CD_SETKEYMAP lives there), cu_Obsolete1/2, cu_Minterms. How the ROM
 * numbers the bits of cu_Modes/cu_RawEvents (here: bit n = byte n/8,
 * 1 << n%8, as the 68000's BSET on a byte) is checked by D3.2's cudump. */

#define UPC_CU_WINDOW     0x022
#define UPC_CU_XCP        0x026
#define UPC_CU_YCP        0x028
#define UPC_CU_XMAX       0x02A
#define UPC_CU_YMAX       0x02C
#define UPC_CU_XRSIZE     0x02E
#define UPC_CU_YRSIZE     0x030
#define UPC_CU_XRORIGIN   0x032
#define UPC_CU_YRORIGIN   0x034
#define UPC_CU_XREXTANT   0x036
#define UPC_CU_YREXTANT   0x038
#define UPC_CU_XMINSHRINK 0x03A
#define UPC_CU_YMINSHRINK 0x03C
#define UPC_CU_XCCP       0x03E
#define UPC_CU_YCCP       0x040
#define UPC_CU_KEYMAP     0x042         /* 32 bytes, not written */
#define UPC_CU_TABSTOPS   0x062
#define UPC_CU_MAXTABS    80
#define UPC_CU_MASK       0x102
#define UPC_CU_FGPEN      0x103
#define UPC_CU_BGPEN      0x104
#define UPC_CU_AOLPEN     0x105
#define UPC_CU_DRAWMODE   0x106
#define UPC_CU_FONT       0x114
#define UPC_CU_ALGOSTYLE  0x118
#define UPC_CU_TXFLAGS    0x119
#define UPC_CU_TXHEIGHT   0x11A
#define UPC_CU_TXWIDTH    0x11C
#define UPC_CU_TXBASELINE 0x11E
#define UPC_CU_TXSPACING  0x120
#define UPC_CU_MODES      0x122         /* 3 bytes */
#define UPC_CU_RAWEVENTS  0x125         /* 3 bytes */
#define UPC_CU_SIZE       0x128
#define UPC_CU_MODE_LNM   20            /* M_LNM */
#define UPC_CU_MODE_ASM   21            /* PMB_ASM */
#define UPC_CU_MODE_AWM   22            /* PMB_AWM */

typedef struct {
    unsigned long window, font;         /* struct Window *, struct TextFont * */
    int cols, rows;                     /* the grid */
    int x, y;                           /* cursor, 0-based (clamped to the grid) */
    int cw, ch;                         /* cell pixels: cu_XRSize/YRSize */
    int ox, oy;                         /* raster origin in the window */
    int minshrink_x, minshrink_y;
    const unsigned short *tabs;         /* tab stop columns, ascending; 0 = every 8 */
    int ntabs;
    int mask, fg, bg, aol, drawmode;
    int algostyle, txflags, txheight, txwidth, txbaseline, txspacing;
    int lnm, asm_, awm;                 /* modes: linefeed-newline, auto-scroll, auto-wrap */
    unsigned long rawevents;            /* CSI n {: bit n = class n (0..23 kept) */
} upc_cu_state;

void upc_conunit_fill(unsigned char *cu, const upc_cu_state *s);

/* The cu_Mask the running ROM's console.device writes, by its version
 * (cudump, D3.2/DV5): 1 from 39-40 (3.0, 3.1, 3.5) and 47 (3.2); 0xFF
 * (all planes) from 45 (3.1.4's console.device 45.4, Kickstart 46.143).
 * Other versions were not measured and keep 1. */
int upc_rom_cu_mask(int rom_version);

#endif
