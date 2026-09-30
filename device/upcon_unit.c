/* upcon_unit.c -- one process per console unit (plan
 * 2026-09-30-console-device.md, DD5, D1.3): it owns the unit's terminal on
 * render/vtwin (the AMIGA personality, DD4), answers CMD_WRITE after
 * feeding the grid (the screen follows at the next frame, as XCON:),
 * turns the input handler's events into the unit's input stream, keeps
 * the public ConUnit fields current (DD17) and ends on UPCMD_DIE.
 *
 * Rules by unit (DD8, DD9, DD14, D1.8): the grid follows the window's size
 * (polled as well as evented; CONFLAG_NODRAW_ON_NEWSIZE: no redraw after
 * it); CHARMAP and SNIPMAP keep 200 lines of scrollback, reflow wrapped
 * lines and repaint on REFRESHWINDOW when the window's owner does not;
 * STANDARD keeps none and leaves refresh to the layers (smart refresh). */
#include <string.h>
#include <stddef.h>
#include <exec/memory.h>
#include <exec/errors.h>
#include <clib/alib_protos.h>
#include <devices/inputevent.h>
#include <dos/dosextens.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include "upcon.h"

/* ---- the unit's input stream (vtwin_host) ---------------------------------- */

static void to_stream(struct upc_unit *u, const vt_u8 *b, long n)
{
    long kept;
    ObtainSemaphore(&u->lock);
    kept = upc_rq_input(&u->rq, b, n);
    ReleaseSemaphore(&u->lock);
    if (kept < n)
        u->base->dropped += (ULONG)(n - kept);
}

static void h_reply(void *user, const vt_u8 *b, long n)
{
    to_stream((struct upc_unit *)user, b, n);
}

static void h_input(void *user, const vt_u8 *b, long n)
{
    to_stream((struct upc_unit *)user, b, n);
}

/* console.device hands keys over as they come: line editing and break keys
 * are the reader's (the con-handler's) business */
static void h_key(void *user, const vt_u8 *b, int n, long key, int mods)
{
    to_stream((struct upc_unit *)user, b, n);
}

static void h_pasted(void *user, const vt_u8 *b, int n, long key)
{
    to_stream((struct upc_unit *)user, b, n);
}

static int h_raw(void *user)
{
    return 1;
}

static void fill_conunit(struct upc_unit *u);
static void fill_after_write(struct upc_unit *u);

static void h_resized(void *user)
{
    fill_conunit((struct upc_unit *)user);
}

static const vtwin_host host = { h_reply, h_input, h_key, h_pasted, h_raw, h_resized };

/* ---- struct ConUnit (DD17) ---------------------------------------------------- */

/* upc_conunit_fill writes the public struct at these offsets (matrix 6.4) */
typedef char check_rawevents[offsetof(struct ConUnit, cu_RawEvents) == UPC_CU_RAWEVENTS ? 1 : -1];
typedef char check_cusize[sizeof(struct ConUnit) == UPC_CU_SIZE ? 1 : -1];
typedef char check_keymap[offsetof(struct ConUnit, cu_KeyMapStruct) == UPC_CU_KEYMAP ? 1 : -1];

/* After a write: the cursor fields only, unless the modes, the raw events
 * or the size changed since the last full fill (a full fill per write cost
 * DV3's 2000-line Type 1.8 s on the cycle-exact rig). */
static void fill_after_write(struct upc_unit *u)
{
    vtwin *w = &u->w;
    int x, y;
    if (!w->t)
        return;
    if (vt_modes(w->t) != u->cu_modes || vt_raw_events(w->t) != u->cu_raw || vt_cols(w->t) != u->cu_cols ||
        vt_rows(w->t) != u->cu_rows) {
        fill_conunit(u);
        return;
    }
    vt_cursor(w->t, &x, &y);
    u->cu.cu_XCP = u->cu.cu_XCCP = (WORD)x;
    u->cu.cu_YCP = u->cu.cu_YCCP = (WORD)y;
}

static void fill_conunit(struct upc_unit *u)
{
    upc_cu_state s;
    vtwin *w = &u->w;
    int x, y;
    if (!w->t)
        return;
    memset(&s, 0, sizeof(s));
    s.window = (unsigned long)u->win;
    s.font = (unsigned long)w->font;
    s.cols = vt_cols(w->t);
    s.rows = vt_rows(w->t);
    vt_cursor(w->t, &x, &y);
    s.x = x;
    s.y = y;
    s.cw = w->font->tf_XSize;
    s.ch = w->font->tf_YSize;
    s.ox = w->r.ox;
    s.oy = w->r.oy;
    /* the values the ROM writes (D3.2 cudump on KS 40.63, RTG and AGA screens
     * alike): MinShrink 9999, mask 1, AOL pen 0, text fields from the
     * window's RastPort (not the font's flags) */
    s.minshrink_x = s.minshrink_y = 9999;
    s.mask = 1;
    s.fg = 1;
    s.bg = 0;
    s.aol = 0;
    s.drawmode = JAM2;
    s.txheight = u->win->RPort->TxHeight;
    s.txwidth = u->win->RPort->TxWidth;
    s.txbaseline = u->win->RPort->TxBaseline;
    s.txflags = u->win->RPort->TxFlags;
    s.algostyle = u->win->RPort->AlgoStyle;
    s.txspacing = u->win->RPort->TxSpacing;
    s.awm = 1;
    s.asm_ = 1;
    s.lnm = (vt_modes(w->t) & VT_MODE_NEWLINE) != 0;
    s.rawevents = vt_raw_events(w->t);
    upc_conunit_fill((unsigned char *)&u->cu, &s);
    u->cu_modes = vt_modes(w->t);
    u->cu_raw = vt_raw_events(w->t);
    u->cu_cols = s.cols;
    u->cu_rows = s.rows;
}

/* ---- the window's size (DD8) ----------------------------------------------------- */

static void check_size(struct upc_unit *u)
{
    struct Window *win = u->win;
    WORD iw = (WORD)(win->Width - win->BorderLeft - win->BorderRight);
    WORD ih = (WORD)(win->Height - win->BorderTop - win->BorderBottom);
    if (iw == u->inner_w && ih == u->inner_h)
        return;
    u->inner_w = iw;
    u->inner_h = ih;
    vtwin_resize(&u->w);
}

/* ---- the process ------------------------------------------------------------------- */

static void event(struct upc_unit *u, const upc_event *e)
{
    u->base->events++;
    switch (e->cls) {
    case UPC_IE_RAWKEY:
        /* ie_EventAddress: the previous two down keys (dead keys) */
        vtwin_key(&u->w, e->code, e->qual, (ULONG)e->addr, e->secs, e->micros);
        break;
    case UPC_IE_RAWMOUSE:
    case UPC_IE_POINTERPOS:
    case UPC_IE_NEWPOINTERPOS:
        u->base->mice++;
        /* drag-select on SNIPMAP units (DD10); the pointer as the window
         * sees it now (a raw mouse event carries deltas, the others screen
         * positions) */
        u->base->pointer = ((ULONG)(UWORD)u->win->MouseX << 16) | (UWORD)u->win->MouseY;
        if (u->unitno == CONU_SNIPMAP) {
            int button = e->cls == UPC_IE_RAWMOUSE && (e->code & ~IECODE_UP_PREFIX) == IECODE_LBUTTON;
            int was = u->w.dragging;
            vtwin_mouse(&u->w, !button, e->code, e->qual, u->win->MouseX, u->win->MouseY);
            u->base->drags += !was && u->w.dragging;
        }
        break;
    case UPC_IE_SIZEWINDOW:
    case UPC_IE_CHANGEWINDOW:
        check_size(u);
        break;
    case UPC_IE_REFRESHWINDOW:
        /* nobody else repaints a simple-refresh window whose owner did
         * not ask for IDCMP_REFRESHWINDOW (DD9) */
        if (u->unitno != CONU_STANDARD && !(u->win->IDCMPFlags & IDCMP_REFRESHWINDOW))
            vtwin_refresh(&u->w);
        break;
    case UPC_IE_CLOSEWINDOW:
        vtwin_raw_report(&u->w, 11);
        break;
    default:
        break;
    }
}

/* CMD_WRITE, in the caller's task (DD5 amended, DV3): the grid now, under the
 * unit's semaphore, the screen at the unit process's next frame. The ROM
 * device writes in the caller's task too; sending every write to the unit
 * process cost two task switches a write -- 1 % of a 2000-line Type on the
 * cycle-exact rig, slower than the ROM. */
void upc_unit_write(struct upc_unit *u, struct IOStdReq *io)
{
    long n = (LONG)io->io_Length;
    const vt_u8 *p = (const vt_u8 *)io->io_Data;
    if (n < 0)
        n = p ? (long)strlen((const char *)p) : 0;
    ObtainSemaphore(&u->lock);
    check_size(u); /* the ROM recomputes on write too (R-1.4) */
    if (n > 0)
        vtwin_write(&u->w, p, n);
    u->base->written += (ULONG)n;
    fill_after_write(u);
    ReleaseSemaphore(&u->lock);
    io->io_Actual = (ULONG)n;
}

static int command(struct upc_unit *u, struct IOStdReq *io)
{
    switch (io->io_Command) {
    case CMD_RESET:
        /* the ROM never returns from it (DP4); here: the terminal reset */
        vt_reset(u->w.t);
        u->w.render_pending = 1;
        vtwin_render(&u->w);
        fill_conunit(u);
        upc_reply_io(io, 0, 0);
        return 1;
    case UPCMD_DIE:
        return 0;
    default:
        upc_reply_io(io, 0, IOERR_NOCMD);
        return 1;
    }
}

void upc_unit_entry(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    struct Message *m;
    struct upc_unit *u;
    struct MsgPort *port;
    struct IOStdReq *die = 0;
    BYTE portsig, evsig;
    WaitPort(&me->pr_MsgPort);
    m = GetMsg(&me->pr_MsgPort);
    u = (struct upc_unit *)m->mn_Node.ln_Name;
    u->task = (struct Task *)me;
    port = &u->cu.cu_MP;
    portsig = AllocSignal(-1);
    evsig = AllocSignal(-1);
    if (portsig < 0 || evsig < 0) {
        if (portsig >= 0)
            FreeSignal(portsig);
        if (evsig >= 0)
            FreeSignal(evsig);
        Forbid();
        ReplyMsg(m); /* u->ok stays 0: Open fails */
        return;
    }
    port->mp_Node.ln_Type = NT_MSGPORT;
    port->mp_Flags = PA_SIGNAL;
    port->mp_SigBit = (UBYTE)portsig;
    port->mp_SigTask = (struct Task *)me;
    NewList(&port->mp_MsgList);
    u->ev_sig = 1UL << evsig;
    /* the terminal: the ROM console's personality on the window's font */
    u->w.pers = VT_AMIGA;
    u->w.fg_rgb = u->w.bg_rgb = VR_KEEP;
    u->w.given_font = u->win->RPort->Font;
    u->w.sb_lines = -1; /* the amiga personality keeps no scrollback, as the ROM (DD14 amended, DV4) */
    u->w.keymap = &u->cu.cu_KeyMapStruct; /* CD_SETKEYMAP changes this unit's keys (DD12) */
    u->w.nodraw_resize = (u->flags & CONFLAG_NODRAW_ON_NEWSIZE) != 0;
    u->w.no_clipboard = u->unitno != CONU_SNIPMAP; /* DD10 */
    u->w.foreign_window = 1;
    vtwin_init(&u->w, &host, u);
    if (!vtwin_attach(&u->w, u->win)) {
        vtwin_cleanup(&u->w);
        FreeSignal(portsig);
        FreeSignal(evsig);
        Forbid();
        ReplyMsg(m);
        return;
    }
    if (u->unitno != CONU_STANDARD)
        vt_set_reflow(u->w.t, 1); /* the ROM's charmap units re-wrap (DP4) */
    u->inner_w = (WORD)(u->win->Width - u->win->BorderLeft - u->win->BorderRight);
    u->inner_h = (WORD)(u->win->Height - u->win->BorderTop - u->win->BorderBottom);
    fill_conunit(u);
    u->ok = 1;
    ReplyMsg(m);

    while (!die) {
        struct IOStdReq *io;
        upc_event e;
        Wait((1UL << port->mp_SigBit) | u->ev_sig | vtwin_sigmask(&u->w));
        /* the terminal is shared with CMD_WRITE's callers: all of it under
         * the unit's semaphore (the read queue nests inside it) */
        ObtainSemaphore(&u->lock);
        while (!die && (io = (struct IOStdReq *)GetMsg(port)) != 0)
            if (!command(u, io))
                die = io;
        while (upc_ev_pop(&u->ring, &e))
            event(u, &e);
        vtwin_tick(&u->w);
        ReleaseSemaphore(&u->lock);
    }
    /* UPCMD_DIE: what is pending on the screen, the queued reads aborted
     * (the V47 con-handler aborts its own read at close, RN-CH 47.1) */
    ObtainSemaphore(&u->lock);
    vtwin_render(&u->w);
    {
        void *r;
        while ((r = upc_rq_take(&u->rq)) != 0)
            upc_reply_io((struct IOStdReq *)r, 0, IOERR_ABORTED);
    }
    vtwin_detach(&u->w);
    vtwin_cleanup(&u->w);
    ReleaseSemaphore(&u->lock);
    FreeSignal(evsig);
    FreeSignal(portsig);
    /* our ports die with us (ibmcon 1.8): the closer frees the unit once we
     * are gone, which Forbid guarantees until this process has ended */
    Forbid();
    ReplyMsg((struct Message *)die);
}
