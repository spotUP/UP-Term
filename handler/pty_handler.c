/* pty-handler: PTY:, Unix pseudo-terminals for AmigaOS (UP-Term, P5).
 *
 * A pair is a master and a slave with the Unix line discipline between
 * them -- tty/ldisc.c, the one XCON: runs, so a program sees the same
 * terminal in a window and under a multiplexer.
 *
 *   PTY:<id>/m  the master: once per id (a second Open fails with
 *               ERROR_OBJECT_IN_USE, so a program finds a free pair by
 *               scanning ids, as BSD programs scan /dev/ptyXX). Reading it
 *               gives what the slave wrote (after OPOST) and the echo;
 *               writing it is typing on the terminal.
 *   PTY:<id>/s  a slave: any number, while the master is open. A console
 *               for the program on it: SCREEN_MODE, WAIT_CHAR,
 *               CHANGE_SIGNAL and the termios packets (vtcon_packets.h).
 * <id> is 1-15 characters without '/', compared without case.
 *
 * One process serves every pair (dn_Task stays set while a pair exists).
 * Each side of a pair has a port of its own, which its handles carry in
 * fh_Type: ACTION_SCREEN_MODE and ACTION_WAIT_CHAR name no handle, only
 * the port they arrive at tells whose they are. A Shell started on a
 * slave gets the slave port as pr_ConsoleTask, so Open("*") and
 * ACTION_DISK_INFO sent there name the pair too.
 *
 * Signal keys (ISIG) go to the slave's break target, as in XCON:
 * VINTR -> CTRL_C, VQUIT -> CTRL_E, VSUSP -> CTRL_F; a patched ixemul
 * makes them SIGINT, SIGQUIT and SIGTSTP for the foreground group.
 * ACTION_VTCON_SWINSZ on the master answers, in dp_Res2, the slave's
 * break target when the size changed, for ixemul's SIGWINCH.
 *
 * DISK_INFO gives no window (id_VolumeNode 0): a pair has none, and a
 * made-up Window would be dereferenced by native programs. ixemul
 * programs get the size by ACTION_VTCON_GWINSZ.
 *
 * Built without a C startup: `handler_entry` must stay the first function.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <clib/alib_protos.h> /* NewList */

#include <string.h>
#include "vtcon_packets.h"
#include "brk.h"
#include "waitset.h"
#include "../tty/ldisc.h"

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct Device *TimerBase; /* GetSysTime: WAIT_CHAR deadlines */

static LONG handler_main(void);


/* The entry point: first code in the hunk. */
LONG handler_entry(void)
{
    SysBase = *(struct ExecBase **)4L;
    return handler_main();
}

#ifdef PTY_DEBUG
/* A trace on the serial port (the rig captures it in build/rig/serial.log). */
void raw_putchar(__reg("a6") struct ExecBase *, __reg("d0") UBYTE) = "\tjsr\t-516(a6)";
static void tr(const char *s, LONG a, LONG b)
{
    static const char hex[] = "0123456789abcdef";
    int i;
    LONG v[2];
    v[0] = a;
    v[1] = b;
    raw_putchar(SysBase, 'P');
    raw_putchar(SysBase, ' ');
    while (*s)
        raw_putchar(SysBase, (UBYTE)*s++);
    for (i = 0; i < 2; i++) {
        int k;
        raw_putchar(SysBase, ' ');
        for (k = 28; k >= 0; k -= 4)
            raw_putchar(SysBase, (UBYTE)hex[(v[i] >> k) & 15]);
    }
    raw_putchar(SysBase, '\n');
}
#define TR(s, a, b) tr(s, (LONG)(a), (LONG)(b))
#else
#define TR(s, a, b)
#endif

#ifndef VTCON_BUILD
#define VTCON_BUILD unknown
#endif
#define STR_(x) #x
#define STR(x) STR_(x)
static const char vers[] = "$VER: pty-handler 0.1 (UP-Term, build " STR(VTCON_BUILD) ")";

#define ID_MAX 16
#define OUT_MAX 4096   /* slave output and echo, waiting for the master */
#define Q 16           /* packets waiting per queue */

typedef struct pair {
    struct MinNode node;
    char id[ID_MAX];
    struct MsgPort mport, sport; /* fh_Type of master and slave handles */
    ldisc ld;
    vt_termios cooked;           /* the termios before SetMode(fh, 1), for SetMode(fh, 0) */
    int has_cooked;
    vt_winsize ws;
    brk brk;                     /* who the slave's signal keys go to */
    int masters, slaves;
    int slave_ever;              /* a slave was opened: its last close is EOF for the master */
    int ocol;                    /* OPOST column (OXTABS) */
    unsigned char out[OUT_MAX];
    long out_len;
    struct DosPacket *mreads[Q], *sreads[Q], *swrites[Q];
    int nmr, nsr, nsw;
    long swoff;                  /* bytes of swrites[0] already taken */
    waitset mwait, swait;                  /* WAIT_CHAR, per side, one per task (waitset.h) */
    struct timerequest *mtimer, *stimer;   /* their timeouts */
    struct timerequest *vtimer;            /* VTIME */
    int mtimer_busy, stimer_busy, vtimer_busy, vtimer_fired;
} pair;

/* fh_Arg1: the pair, bit 0 set for the master (AllocVec is 8-aligned) */
#define EP_PAIR(a) ((pair *)((a) & ~1L))
#define EP_MASTER(a) ((a) & 1L)

/* Per start of the handler. DOS keeps our seglist (dn_SegList) and
 * starts the same code again after we end, so these are not fresh then:
 * handler_main sets every one of them, and an ending instance works from
 * copies (a new one may already run and set them). */
static struct MsgPort *main_port;
static struct MsgPort *timer_port;
static struct timerequest *timer_proto; /* the opened unit, copied for each request */
static struct MinList pairs;
static int ever_paired; /* a pair existed: only then may no pair mean "done" */

static void reply(struct DosPacket *p, LONG r1, LONG r2)
{
    TR("reply", p->dp_Type, r1);
    ReplyPkt(p, r1, r2);
}

static int id_eq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z')
            x += 32;
        if (y >= 'A' && y <= 'Z')
            y += 32;
        if (x != y)
            return 0;
    }
    return *a == *b;
}

static pair *find_pair(const char *id)
{
    struct MinNode *n;
    for (n = pairs.mlh_Head; n->mln_Succ; n = n->mln_Succ)
        if (id_eq(((pair *)n)->id, id))
            return (pair *)n;
    return 0;
}

/* ---- timers ----------------------------------------------------------------- */

static struct timerequest *new_timer(void)
{
    struct timerequest *t;
    if (!timer_proto)
        return 0;
    t = (struct timerequest *)CreateIORequest(timer_port, sizeof(struct timerequest));
    if (t) {
        t->tr_node.io_Device = timer_proto->tr_node.io_Device;
        t->tr_node.io_Unit = timer_proto->tr_node.io_Unit;
    }
    return t;
}

static void timer_stop(struct timerequest *t, int *busy)
{
    if (*busy) {
        AbortIO((struct IORequest *)t);
        WaitIO((struct IORequest *)t);
        *busy = 0;
    }
}

static void timer_start(struct timerequest *t, int *busy, ULONG micros)
{
    timer_stop(t, busy);
    if (!t)
        return;
    t->tr_node.io_Command = TR_ADDREQUEST;
    t->tr_time.tv_secs = micros / 1000000;
    t->tr_time.tv_micro = micros % 1000000;
    SendIO((struct IORequest *)t);
    *busy = 1;
}

/* ---- the pair's queues ---------------------------------------------------- */

static void shift(struct DosPacket **q, int *n)
{
    int i;
    for (i = 1; i < *n; i++)
        q[i - 1] = q[i];
    (*n)--;
}

static void out_append(pair *p, const unsigned char *b, long n)
{
    if (n > OUT_MAX - p->out_len)
        n = OUT_MAX - p->out_len; /* the master is not reading: echo is lost, as on Unix */
    CopyMem((APTR)b, p->out + p->out_len, n);
    p->out_len += n;
}

static void ld_echo_cb(void *u, const unsigned char *s, int n)
{
    out_append((pair *)u, s, n);
}

static void ld_signal_cb(void *u, int sig)
{
    pair *p = (pair *)u;
    TR("signal to", sig, (LONG)brk_task(&p->brk));
    if (sig == LD_SIGINT)
        brk_send(&p->brk, SIGBREAKF_CTRL_C);
    else if (sig == LD_SIGQUIT)
        brk_send(&p->brk, SIGBREAKF_CTRL_E);
    else if (sig == LD_SIGTSTP)
        brk_send(&p->brk, SIGBREAKF_CTRL_F);
}

static int hung_up(pair *p)
{
    return p->masters <= 0;
}

/* Slave writes into the master's queue, as far as it has room. A write
 * that does not fit waits (the writer blocks, as on Unix). */
static int slave_writes(pair *p)
{
    int moved = 0;
    unsigned char tmp[64 * 8];
    while (p->nsw) {
        struct DosPacket *w = p->swrites[0];
        const unsigned char *b = (const unsigned char *)w->dp_Arg2 + p->swoff;
        long left = w->dp_Arg3 - p->swoff;
        if (!hung_up(p)) {
            if (p->ld.stopped)
                break; /* ^S */
            while (left > 0) {
                long room = OUT_MAX - p->out_len, k, n;
                /* OPOST may turn a byte into 8 (a tab under OXTABS) */
                k = left < 64 ? left : 64;
                if (room < k * 8)
                    k = room / 8;
                if (k <= 0)
                    break;
                n = ld_output(&p->ld.t, b, k, tmp, &p->ocol);
                out_append(p, tmp, n);
                b += k;
                left -= k;
                p->swoff += k;
                moved = 1;
            }
            if (left > 0)
                break;
        }
        /* written, or the master is gone: a hung-up terminal takes output and drops it */
        reply(w, w->dp_Arg3, 0);
        shift(p->swrites, &p->nsw);
        p->swoff = 0;
        moved = 1;
    }
    return moved;
}

static ws_time now_time(void)
{
    struct timeval tv;
    ws_time t;
    GetSysTime(&tv);
    t.s = tv.tv_secs;
    t.us = tv.tv_micro;
    return t;
}

/* the side's timer to the earliest deadline of its waiters, or off */
static void rearm(waitset *w, struct timerequest *t, int *busy, ws_time now)
{
    long next = ws_next(w, now);
    if (next < 0)
        timer_stop(t, busy);
    else
        timer_start(t, busy, next ? (ULONG)next : 1);
}

/* input (or the end of it) for one side: every waiter is answered */
static void finish_waits(waitset *w, struct timerequest *t, int *busy, LONG result)
{
    struct DosPacket *d;
    timer_stop(t, busy);
    while ((d = (struct DosPacket *)ws_take(w)) != 0)
        reply(d, result, 0);
}

static int master_reads(pair *p)
{
    int moved = 0;
    while (p->nmr) {
        struct DosPacket *r = p->mreads[0];
        long n;
        if (p->out_len) {
            n = p->out_len < r->dp_Arg3 ? p->out_len : r->dp_Arg3;
            CopyMem(p->out, (APTR)r->dp_Arg2, n);
            memmove(p->out, p->out + n, p->out_len - n);
            p->out_len -= n;
        } else if (p->slave_ever && p->slaves <= 0) {
            n = 0; /* every slave closed: end of file */
        } else {
            break;
        }
        reply(r, n, 0);
        shift(p->mreads, &p->nmr);
        moved = 1;
    }
    if (p->mwait.n && (p->out_len || (p->slave_ever && p->slaves <= 0)))
        finish_waits(&p->mwait, p->mtimer, &p->mtimer_busy, DOSTRUE);
    return moved;
}

static int slave_reads(pair *p)
{
    int moved = 0;
    while (p->nsr) {
        struct DosPacket *r = p->sreads[0];
        long n = 0;
        int eof, arm;
        if (!hung_up(p)) {
            int act = ld_read_action(&p->ld, p->vtimer_fired, &arm);
            if (act == LD_RD_WAIT) {
                if (arm && !p->vtimer_busy)
                    timer_start(p->vtimer, &p->vtimer_busy, (ULONG)p->ld.t.c_cc[LD_VTIME] * 100000UL);
                break;
            }
            if (act == LD_RD_TAKE)
                n = ld_read(&p->ld, (unsigned char *)r->dp_Arg2, r->dp_Arg3, &eof);
        } /* hung up: end of file */
        timer_stop(p->vtimer, &p->vtimer_busy);
        p->vtimer_fired = 0;
        reply(r, n < 0 ? 0 : n, 0);
        shift(p->sreads, &p->nsr);
        moved = 1;
    }
    if (p->swait.n && (hung_up(p) || ld_input_pending(&p->ld)))
        finish_waits(&p->swait, p->stimer, &p->stimer_busy, DOSTRUE);
    return moved;
}

static void service(pair *p)
{
    int moved;
    do {
        moved = slave_writes(p);
        moved |= master_reads(p);
        moved |= slave_reads(p);
    } while (moved);
}

/* ---- pairs ----------------------------------------------------------------- */

static void init_port(struct MsgPort *mp)
{
    mp->mp_Node.ln_Type = NT_MSGPORT;
    mp->mp_Flags = PA_SIGNAL;
    mp->mp_SigBit = main_port->mp_SigBit; /* one signal wakes us for every port */
    mp->mp_SigTask = main_port->mp_SigTask;
    NewList(&mp->mp_MsgList);
}

static pair *new_pair(const char *id)
{
    pair *p = (pair *)AllocVec(sizeof(pair), MEMF_ANY | MEMF_CLEAR);
    if (!p)
        return 0;
    strcpy(p->id, id);
    init_port(&p->mport);
    init_port(&p->sport);
    p->ld.echo = ld_echo_cb;
    p->ld.signal = ld_signal_cb;
    p->ld.user = p;
    ld_init(&p->ld);
    p->ws.ws_row = 24;
    p->ws.ws_col = 80;
    p->mtimer = new_timer();
    p->stimer = new_timer();
    p->vtimer = new_timer();
    AddTail((struct List *)&pairs, (struct Node *)p);
    ever_paired = 1;
    return p;
}

static void free_pair(pair *p)
{
    struct Message *m;
    finish_waits(&p->mwait, p->mtimer, &p->mtimer_busy, DOSFALSE);
    finish_waits(&p->swait, p->stimer, &p->stimer_busy, DOSFALSE);
    timer_stop(p->vtimer, &p->vtimer_busy);
    if (p->mtimer)
        DeleteIORequest((struct IORequest *)p->mtimer);
    if (p->stimer)
        DeleteIORequest((struct IORequest *)p->stimer);
    if (p->vtimer)
        DeleteIORequest((struct IORequest *)p->vtimer);
    Remove((struct Node *)p);
    /* a packet sent to the pair's ports after its last close: refuse it */
    while ((m = GetMsg(&p->mport)) || (m = GetMsg(&p->sport)))
        reply((struct DosPacket *)m->mn_Node.ln_Name, DOSFALSE, ERROR_OBJECT_NOT_FOUND);
    FreeVec(p);
}

/* A handle closed: a master's last close hangs the pair up (slave reads
 * see end of file), and a pair nobody holds is gone. */
static void end_pair(pair *p)
{
    if (p->masters > 0 || p->slaves > 0) {
        service(p);
        return;
    }
    /* no handle, so no packet of theirs can still wait here */
    free_pair(p);
}

/* ---- packets ----------------------------------------------------------------- */

/* "PTY:<id>/m" -> id and 'm'; "*" and "CONSOLE:" -> 0 (the port's pair). */
static int parse_name(UBYTE *b, char *id, int *master)
{
    char name[256];
    int i, n = b ? b[0] : 0, colon = -1, slash = -1, start, len;
    for (i = 0; i < n && i < 255; i++) {
        name[i] = (char)b[i + 1];
        if (colon < 0 && name[i] == ':')
            colon = i;
    }
    name[i] = 0;
    if (!strcmp(name, "*") || id_eq(name, "CONSOLE:"))
        return 0;
    start = colon + 1;
    for (i = start; name[i]; i++)
        if (name[i] == '/')
            slash = i;
    if (slash < 0 || !name[slash + 1] || name[slash + 2])
        return -1;
    len = slash - start;
    if (len < 1 || len >= ID_MAX)
        return -1;
    memcpy(id, name + start, len);
    id[len] = 0;
    for (i = 0; i < len; i++)
        if (id[i] == '/')
            return -1;
    if (name[slash + 1] == 'm' || name[slash + 1] == 'M')
        *master = 1;
    else if (name[slash + 1] == 's' || name[slash + 1] == 'S')
        *master = 0;
    else
        return -1;
    return 1;
}

/* Which side a port is: SIDE_NONE for the handler's own port. */
#define SIDE_NONE 0
#define SIDE_MASTER 1
#define SIDE_SLAVE 2

static void do_open(struct DosPacket *d, pair *via, int side)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(d->dp_Arg1);
    char id[ID_MAX];
    int master = 0, how = parse_name((UBYTE *)BADDR(d->dp_Arg3), id, &master);
    pair *p;
    if (how == 0) {
        p = via; /* "*" on a slave's port: another slave of it */
        if (!p || side != SIDE_SLAVE) {
            reply(d, DOSFALSE, ERROR_OBJECT_NOT_FOUND);
            return;
        }
    } else if (how < 0) {
        reply(d, DOSFALSE, ERROR_INVALID_COMPONENT_NAME);
        return;
    } else {
        p = find_pair(id);
        if (master) {
            if (p) {
                reply(d, DOSFALSE, ERROR_OBJECT_IN_USE); /* taken: try the next id */
                return;
            }
            if (!(p = new_pair(id))) {
                reply(d, DOSFALSE, ERROR_NO_FREE_STORE);
                return;
            }
        } else if (!p || hung_up(p)) {
            reply(d, DOSFALSE, ERROR_OBJECT_NOT_FOUND);
            return;
        }
    }
    fh->fh_Port = (struct MsgPort *)DOSTRUE; /* interactive */
    if (master) {
        p->masters++;
        fh->fh_Arg1 = (LONG)p | 1L;
        fh->fh_Type = &p->mport;
    } else {
        struct Task *t;
        /* the first slave opener is the break target, as on Unix the
         * first opener gets the controlling terminal; again when it is gone */
        Forbid();
        t = brk_task(&p->brk);
        Permit();
        if (!t)
            brk_open(&p->brk, d->dp_Port);
        p->slaves++;
        p->slave_ever = 1;
        fh->fh_Arg1 = (LONG)p;
        fh->fh_Type = &p->sport;
    }
    reply(d, DOSTRUE, 0);
}

static void set_mode(pair *p, int raw);

/* ACTION_WAIT_CHAR (dp_Arg1 microseconds) and ACTION_SCREEN_MODE
 * (dp_Arg1 0 cooked, else raw) for one side of p. */
static void by_port(struct DosPacket *d, pair *p, int master)
{
    if (d->dp_Type == ACTION_SCREEN_MODE) {
        /* the Amiga way to set a mode, in termios terms (as XCON:'s TCGETA) */
        set_mode(p, d->dp_Arg1 != 0);
        reply(d, DOSTRUE, 0);
        service(p);
        return;
    }
    {
        waitset *w = master ? &p->mwait : &p->swait;
        struct timerequest *t = master ? p->mtimer : p->stimer;
        int *busy = master ? &p->mtimer_busy : &p->stimer_busy;
        int ready = master ? (p->out_len || (p->slave_ever && p->slaves <= 0))
                           : (hung_up(p) || ld_input_pending(&p->ld));
        struct DosPacket *old;
        ws_time now = now_time();
        /* a newer WAIT_CHAR ends the older one of the same task: it is
         * stale (ixemul's select sends one per call, and one with no
         * timeout before it closes a file whose select is still out).
         * Other tasks' wait on: ixemul 80.x selects per process. */
        if ((old = (struct DosPacket *)ws_drop_task(w, d->dp_Port->mp_SigTask)) != 0)
            reply(old, DOSFALSE, 0);
        if (ready) {
            reply(d, DOSTRUE, 0);
        } else if (d->dp_Arg1 <= 0) {
            reply(d, DOSFALSE, 0); /* a poll */
        } else if ((old = (struct DosPacket *)ws_add(w, d, d->dp_Port->mp_SigTask, now, (ULONG)d->dp_Arg1)) != 0) {
            reply(old, DOSFALSE, 0);   /* the set was full: the oldest goes */
        }
        rearm(w, t, busy, now);
    }
}

/* SetMode(fh, 1) is cfmakeraw (ld_make_raw): an Amiga program asking for
 * raw mode gets every byte as it is, both ways -- a zmodem transfer (sz,
 * rz) over a serial login must, and the lflag-only raw kept \n -> \r\n on
 * output and XON/XOFF as flow control. SetMode(fh, 0) puts back the termios
 * from before. */
static void set_mode(pair *p, int raw)
{
    vt_termios t = p->ld.t;
    if (raw) {
        if (!p->has_cooked) {
            p->cooked = p->ld.t;
            p->has_cooked = 1;
        }
        ld_make_raw(&t);
    } else if (p->has_cooked) {
        t = p->cooked;
        p->has_cooked = 0;
    } else {
        vt_termios d;
        ld_defaults(&d);
        t.c_lflag |= d.c_lflag & (LD_ICANON | LD_ECHO | LD_ISIG | LD_IEXTEN);
    }
    ld_set(&p->ld, &t, LD_TCSANOW);
}

/* A packet that arrived at the port of `via`'s `side` (via 0: the
 * handler's own port). */
static void packet(struct DosPacket *d, pair *via, int side)
{
    pair *p;
    int master;
    TR("packet", d->dp_Type, (LONG)via | side);
    switch (d->dp_Type) {
    case ACTION_FINDINPUT:
    case ACTION_FINDOUTPUT:
    case ACTION_FINDUPDATE:
        do_open(d, via, side);
        return;
    case ACTION_IS_FILESYSTEM:
        reply(d, DOSFALSE, 0);
        return;
    case ACTION_DISK_INFO: {
        struct InfoData *id = (struct InfoData *)BADDR(d->dp_Arg1);
        memset(id, 0, sizeof(*id));
        id->id_DiskType = 0x434F4E00L; /* 'CON\0', as XCON: */
        reply(d, DOSTRUE, 0);
        return;
    }
    case ACTION_WAIT_CHAR:
    case ACTION_SCREEN_MODE:
        /* no handle in these: the port says whose they are */
        if (!via) {
            reply(d, DOSFALSE, ERROR_ACTION_NOT_KNOWN);
            return;
        }
        by_port(d, via, side == SIDE_MASTER);
        return;
    default:
        break;
    }
    /* the rest names a handle in dp_Arg1 */
    if (!d->dp_Arg1) {
        reply(d, DOSFALSE, ERROR_ACTION_NOT_KNOWN);
        return;
    }
    p = EP_PAIR(d->dp_Arg1);
    master = EP_MASTER(d->dp_Arg1) != 0;
    switch (d->dp_Type) {
    case ACTION_VTCON_NREAD:
        reply(d, master ? p->out_len : ld_nread(&p->ld), 0);
        return;
    case ACTION_END:
        if (master)
            p->masters--;
        else
            p->slaves--;
        /* the side's last handle closed with a read still out (a program
         * that stops pumping, upgetty at its end): the read ends, 0 bytes
         * -- nobody is left to answer it later */
        if (master && p->masters <= 0)
            while (p->nmr) {
                reply(p->mreads[0], 0, 0);
                shift(p->mreads, &p->nmr);
            }
        if (!master && p->slaves <= 0)
            while (p->nsr) {
                reply(p->sreads[0], 0, 0);
                shift(p->sreads, &p->nsr);
            }
        reply(d, DOSTRUE, 0);
        end_pair(p);
        return;
    case ACTION_READ:
        if (master) {
            if (p->nmr >= Q) {
                reply(d, -1, ERROR_NO_FREE_STORE);
                return;
            }
            p->mreads[p->nmr++] = d;
        } else {
            if (p->nsr >= Q) {
                reply(d, -1, ERROR_NO_FREE_STORE);
                return;
            }
            p->sreads[p->nsr++] = d;
        }
        service(p);
        return;
    case ACTION_WRITE:
        if (master) {
            if (d->dp_Arg3 > 0)
                ld_input(&p->ld, (const unsigned char *)d->dp_Arg2, (int)d->dp_Arg3);
            reply(d, d->dp_Arg3, 0);
        } else {
            if (p->nsw >= Q) {
                reply(d, -1, ERROR_NO_FREE_STORE);
                return;
            }
            p->swrites[p->nsw++] = d;
        }
        service(p);
        return;
    case ACTION_CHANGE_SIGNAL:
        if (!master)
            brk_change(&p->brk, (struct MsgPort *)d->dp_Arg2);
        TR("change signal", master, d->dp_Arg2);
        reply(d, DOSTRUE, 0);
        return;
    case ACTION_VTCON_TCGETA:
        if (!d->dp_Arg2) {
            reply(d, DOSFALSE, ERROR_REQUIRED_ARG_MISSING);
            return;
        }
        CopyMem(&p->ld.t, (APTR)d->dp_Arg2, sizeof(vt_termios));
        reply(d, DOSTRUE, 1); /* Res2 1: always termios mode (vtcon_packets.h) */
        return;
    case ACTION_VTCON_TCSETA:
        if (!d->dp_Arg2) {
            reply(d, DOSFALSE, ERROR_REQUIRED_ARG_MISSING);
            return;
        }
        ld_set(&p->ld, (const vt_termios *)d->dp_Arg2, (int)d->dp_Arg3);
        reply(d, DOSTRUE, 0);
        service(p);
        return;
    case ACTION_VTCON_GWINSZ:
        if (!d->dp_Arg2) {
            reply(d, DOSFALSE, ERROR_REQUIRED_ARG_MISSING);
            return;
        }
        CopyMem(&p->ws, (APTR)d->dp_Arg2, sizeof(vt_winsize));
        reply(d, DOSTRUE, 0);
        return;
    case ACTION_VTCON_SWINSZ: {
        const vt_winsize *ws = (const vt_winsize *)d->dp_Arg2;
        struct Task *t = 0;
        if (!ws) {
            reply(d, DOSFALSE, ERROR_REQUIRED_ARG_MISSING);
            return;
        }
        if (memcmp(ws, &p->ws, sizeof(vt_winsize))) {
            CopyMem((APTR)ws, &p->ws, sizeof(vt_winsize));
            Forbid();
            t = brk_task(&p->brk);
            Permit();
        }
        /* Res2: whom SIGWINCH is for (0: the size did not change) */
        reply(d, DOSTRUE, (LONG)t);
        return;
    }
    case ACTION_VTCON_INTR: {
        /* ixemul's reader caught a signal: give its waiting read back */
        struct DosPacket **q = master ? p->mreads : p->sreads;
        int *n = master ? &p->nmr : &p->nsr;
        struct DosPacket *rp = (struct DosPacket *)d->dp_Arg2;
        int i;
        for (i = 0; i < *n && q[i] != rp; i++)
            ;
        if (i == *n) {
            reply(d, DOSFALSE, 0);
            return;
        }
        if (!master && i == 0) {
            timer_stop(p->vtimer, &p->vtimer_busy); /* VTIME's timer was this read's */
            p->vtimer_fired = 0;
        }
        for (i++; i < *n; i++)
            q[i - 1] = q[i];
        (*n)--;
        reply(rp, -1, ERROR_BREAK);
        reply(d, DOSTRUE, 0);
        service(p); /* the next read, if any, may wait on VTIME now */
        return;
    }
    case ACTION_FLUSH:
        reply(d, DOSTRUE, 0);
        return;
    default:
        reply(d, DOSFALSE, ERROR_ACTION_NOT_KNOWN);
        return;
    }
}

/* ---- main -------------------------------------------------------------------- */

/* a side's timer is up: the waiters whose time it is hear "no" */
static void expire(waitset *w, struct timerequest *t, int *busy)
{
    struct DosPacket *d;
    ws_time now = now_time();
    while ((d = (struct DosPacket *)ws_expired(w, now)) != 0)
        reply(d, DOSFALSE, 0);
    rearm(w, t, busy, now);
}

static void timers_done(void)
{
    struct MinNode *n;
    for (n = pairs.mlh_Head; n->mln_Succ; n = n->mln_Succ) {
        pair *p = (pair *)n;
        if (p->mtimer_busy && CheckIO((struct IORequest *)p->mtimer)) {
            WaitIO((struct IORequest *)p->mtimer);
            p->mtimer_busy = 0;
            expire(&p->mwait, p->mtimer, &p->mtimer_busy);
        }
        if (p->stimer_busy && CheckIO((struct IORequest *)p->stimer)) {
            WaitIO((struct IORequest *)p->stimer);
            p->stimer_busy = 0;
            expire(&p->swait, p->stimer, &p->stimer_busy);
        }
        if (p->vtimer_busy && CheckIO((struct IORequest *)p->vtimer)) {
            WaitIO((struct IORequest *)p->vtimer);
            p->vtimer_busy = 0;
            p->vtimer_fired = 1; /* VTIME is up: the waiting read takes what there is */
            service(p);
        }
    }
}

static LONG handler_main(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    struct DosPacket *d;
    struct DeviceNode *node;
    struct Message *m;

    WaitPort(&me->pr_MsgPort);
    d = (struct DosPacket *)GetMsg(&me->pr_MsgPort)->mn_Node.ln_Name; /* the startup packet */
    main_port = &me->pr_MsgPort;
    ever_paired = 0;
    timer_port = 0;
    timer_proto = 0;
    TR("startup", d->dp_Type, d->dp_Arg3);
    NewList((struct List *)&pairs);

    DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR)"dos.library", 37);
    if (!DOSBase)
        return 0; /* cannot even reply: DOS will see the process end */
    timer_port = CreateMsgPort();
    if (timer_port) {
        timer_proto = (struct timerequest *)CreateIORequest(timer_port, sizeof(struct timerequest));
        if (timer_proto && OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)timer_proto, 0)) {
            DeleteIORequest((struct IORequest *)timer_proto);
            timer_proto = 0;
        }
        if (timer_proto)
            TimerBase = timer_proto->tr_node.io_Device;
    }
    if (!timer_proto || !vers[0]) { /* vers[0]: keeps the $VER string linked in */
        ReplyPkt(d, DOSFALSE, ERROR_NO_FREE_STORE);
        if (timer_port)
            DeleteMsgPort(timer_port);
        CloseLibrary((struct Library *)DOSBase);
        return 0;
    }
    /* every Open of PTY: comes to this process */
    node = (struct DeviceNode *)BADDR(d->dp_Arg3);
    if (node)
        node->dn_Task = main_port;
    ReplyPkt(d, DOSTRUE, 0);
    TR("started", (LONG)main_port, main_port->mp_SigBit);

    for (;;) {
        struct MinNode *n;
        struct MsgPort *tport = timer_port;
        struct timerequest *tproto = timer_proto;
        Wait((1UL << main_port->mp_SigBit) | (1UL << timer_port->mp_SigBit));
        while ((m = GetMsg(main_port)))
            packet((struct DosPacket *)m->mn_Node.ln_Name, 0, SIDE_NONE);
        /* the pairs' ports share our signal; a packet may free its pair */
        for (n = pairs.mlh_Head; n->mln_Succ;) {
            struct MinNode *next = n->mln_Succ;
            pair *p = (pair *)n;
            if ((m = GetMsg(&p->mport))) {
                packet((struct DosPacket *)m->mn_Node.ln_Name, p, SIDE_MASTER);
                next = pairs.mlh_Head; /* the list may have changed: again from the start */
            } else if ((m = GetMsg(&p->sport))) {
                packet((struct DosPacket *)m->mn_Node.ln_Name, p, SIDE_SLAVE);
                next = pairs.mlh_Head;
            }
            n = next;
        }
        timers_done();
        /* no pair left: end, so an idle PTY: costs nothing. Not before the
         * first pair: the startup message leaves our signal set, so the
         * first Wait returns at once, and ending then sent the Open that
         * started us to a port that was gone (rig: ptytest hung in its
         * first Open). Under Forbid no Open can reach the port between the
         * check and dn_Task = 0. */
        if (ever_paired && !pairs.mlh_Head->mln_Succ) {
            int done = 0;
            Forbid();
            if (IsListEmpty(&main_port->mp_MsgList)) {
                if (node && node->dn_Task == main_port)
                    node->dn_Task = 0;
                done = 1;
            }
            Permit();
            if (done) {
                /* from here on a new instance may run: our copies only */
                CloseDevice((struct IORequest *)tproto);
                DeleteIORequest((struct IORequest *)tproto);
                DeleteMsgPort(tport);
                CloseLibrary((struct Library *)DOSBase); /* the same base for every opener */
                return 0;
            }
        }
    }
}
