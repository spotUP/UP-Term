/* upgetty -- a Unix shell over the serial port (ledger T4): serial.device on
 * one side, a PTY: pair in the middle, vsh on the slave. Whatever terminal
 * is on the other end of the cable (an xterm running `screen /dev/ttyUSB0`,
 * PuTTY, a second Amiga) then talks to a real tty: termios, the window size,
 * and Ctrl-C/Z/\ as on a Unix tty (only the bytes cross the cable; the PTY's
 * line discipline turns them into signals), so screen, tmux and vim work
 * over the wire.
 *
 *   upgetty [UNIT n] [BAUD n] [ROWS n] [COLS n] [TERM name] [SHELL cmd] [DEVICE name] [LOOP]
 *           [RTSCTS]
 *
 * Defaults: serial.device unit 0, 19200 baud, 8N1, no flow control: what
 * any cable carries (a three-wire null-modem has no CTS, and with RTS/CTS
 * on nothing would leave the port). Faster needs the far end held back:
 * RTSCTS with a full cable, then BAUD 115200 (measured on the rig: 115200
 * without it overran, zmodem stalled both ways). 24 x 80, TERM
 * xterm-256color, SHELL UP-Term:bin/vsh (C:vsh of an older install). LOOP starts a new shell when one ends (as a
 * getty); without it upgetty ends with its shell. Ctrl-C to upgetty itself
 * (Break) ends it; the terminal's Ctrl-C goes to the shell, never to us.
 *
 * The pump: one CMD_READ of one byte pending on the serial port (then
 * everything SDCMD_QUERY says is buffered), one ACTION_READ pending on the
 * PTY: master; whichever comes back is written to the other side. The
 * window size goes to the master by ACTION_VTCON_SWINSZ, as a pty master
 * sets it. */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/serial.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <dos/rdargs.h>
#include <dos/var.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "../handler/vtcon_packets.h"
extern struct DosLibrary *DOSBase;
#define UPASSIGN_DOS
#include "../config/upassign.h"

static const char vers[] = "$VER: upgetty 0.1 (30.9.2026)";

struct ws { UWORD ws_row, ws_col, ws_xpixel, ws_ypixel; }; /* ixemul 48.2's winsize */

typedef struct {
    struct MsgPort *port;               /* serial replies */
    struct MsgPort *pport;              /* the master's packet replies: a port of their own */
    struct IOExtSer *rd, *wr;
    int rd_busy;
    UBYTE rbyte, rbuf[256];
    BPTR master;
    struct StandardPacket pkt;
    int pkt_busy;
    UBYTE mbuf[512];
    char id[16];
} getty;

#ifdef UPGETTY_DEBUG
/* the trace in memory (a file write per event slowed the pump enough to
 * hide a race); Ctrl-E writes it to RAM:upgetty.log */
static char lgbuf[8192];
static long lglen;
static void lg(const char *w, LONG a)
{
    char b[80];
    long n;
    sprintf(b, "%s %ld\n", w, a);
    n = (long)strlen(b);
    if (lglen + n < (long)sizeof(lgbuf)) {
        memcpy(lgbuf + lglen, b, n);
        lglen += n;
    }
}
static void lg_dump(void)
{
    BPTR f = Open((STRPTR)"RAM:upgetty.log", MODE_NEWFILE);
    if (f) {
        Write(f, lgbuf, lglen);
        Close(f);
    }
}
#define LG(w, a) lg(w, (LONG)(a))
#define LG_DUMP() lg_dump()
#else
/* the value is still evaluated: a call written inside LG() must not vanish
 * with the trace (Write and ACTION_CHANGE_SIGNAL once did) */
#define LG(w, a) ((void)(a))
#define LG_DUMP()
#endif

static void serial_read(getty *g)
{
    g->rd->IOSer.io_Command = CMD_READ;
    g->rd->IOSer.io_Data = &g->rbyte;
    g->rd->IOSer.io_Length = 1;
    SendIO((struct IORequest *)g->rd);
    g->rd_busy = 1;
}

static void serial_write(getty *g, const UBYTE *b, LONG n)
{
    g->wr->IOSer.io_Command = CMD_WRITE;
    g->wr->IOSer.io_Data = (APTR)b;
    g->wr->IOSer.io_Length = (ULONG)n;
    DoIO((struct IORequest *)g->wr);
}

/* An ACTION_READ on the master, answered later at our port. */
static void master_read(getty *g)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(g->master);
    g->pkt.sp_Msg.mn_Node.ln_Name = (char *)&g->pkt.sp_Pkt;
    g->pkt.sp_Pkt.dp_Link = &g->pkt.sp_Msg;
    g->pkt.sp_Pkt.dp_Port = g->pport;
    g->pkt.sp_Pkt.dp_Type = ACTION_READ;
    g->pkt.sp_Pkt.dp_Arg1 = fh->fh_Arg1;
    g->pkt.sp_Pkt.dp_Arg2 = (LONG)g->mbuf;
    g->pkt.sp_Pkt.dp_Arg3 = sizeof(g->mbuf);
    PutMsg(fh->fh_Type, &g->pkt.sp_Msg);
    g->pkt_busy = 1;
}

/* A free PTY: pair: the first id whose master opens (a used one refuses). */
static int open_master(getty *g)
{
    int i;
    char name[32];
    for (i = 0; i < 64; i++) {
        sprintf(g->id, "ser%d", i);
        sprintf(name, "PTY:%s/m", g->id);
        g->master = Open((STRPTR)name, MODE_OLDFILE);
        if (g->master)
            return 1;
    }
    return 0;
}

static void set_size(getty *g, int rows, int cols)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(g->master);
    static struct ws w;
    w.ws_row = (UWORD)rows;
    w.ws_col = (UWORD)cols;
    DoPkt(fh->fh_Type, ACTION_VTCON_SWINSZ, fh->fh_Arg1, (LONG)&w, 0, 0, 0);
}

/* The shell on the slave, its console the slave (Open("*"), job control),
 * and the slave's signal keys to the shell: PTY: sends them to whoever
 * opened the slave -- that is us, and a getty must never get the
 * terminal's Ctrl-C (the rig: Ctrl-C over the wire stopped upgetty and with
 * it the pump). The shell is started with CreateNewProc so we know its
 * process, then ACTION_CHANGE_SIGNAL names it. */
static int start_shell(getty *g, const char *shell, const char *term)
{
    char name[32];
    BPTR in, out, seg;
    struct Process *pr;
    struct FileHandle *fh;
    sprintf(name, "PTY:%s/s", g->id);
    in = Open((STRPTR)name, MODE_OLDFILE);
    out = Open((STRPTR)name, MODE_OLDFILE);
    seg = LoadSeg((STRPTR)shell);
    if (!in || !out || !seg) {
        if (in)
            Close(in);
        if (out)
            Close(out);
        if (seg)
            UnLoadSeg(seg);
        return 0;
    }
    SetVar((STRPTR)"TERM", (STRPTR)term, -1, GVF_LOCAL_ONLY); /* NP_CopyVars: the shell gets our locals */
    fh = (struct FileHandle *)BADDR(in);
    pr = CreateNewProcTags(NP_Seglist, (ULONG)seg, NP_FreeSeglist, TRUE, NP_Name, (ULONG)"upgetty shell",
                           NP_Input, (ULONG)in, NP_Output, (ULONG)out, NP_CloseInput, TRUE,
                           NP_CloseOutput, TRUE, NP_Cli, TRUE, NP_CopyVars, TRUE, NP_StackSize, 16000,
                           NP_ConsoleTask, (ULONG)fh->fh_Type, NP_Arguments, (ULONG)"\n", TAG_DONE);
    if (!pr) {
        Close(in);
        Close(out);
        UnLoadSeg(seg);
        return 0;
    }
    LG("change signal", DoPkt(fh->fh_Type, ACTION_CHANGE_SIGNAL, fh->fh_Arg1, (LONG)&pr->pr_MsgPort, 0, 0, 0));
    LG("shell process", pr);
    return 1;
}

int main(void)
{
    static const char tmpl[] = "UNIT/K/N,BAUD/K/N,ROWS/K/N,COLS/K/N,TERM/K,SHELL/K,DEVICE/K,LOOP/S,RTSCTS/S";
    LONG args[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    struct RDArgs *rd = ReadArgs((STRPTR)tmpl, args, 0);
    getty g;
    LONG unit, baud, rows, cols;
    const char *term, *shell, *dev;
    int loop, rc = RETURN_OK, running = 1;
    if (!rd) {
        PrintFault(IoErr(), (STRPTR)"upgetty");
        return RETURN_FAIL;
    }
    if (!vers[0])
        return RETURN_FAIL; /* keeps the version string linked in */
    unit = args[0] ? *(LONG *)args[0] : 0;
    baud = args[1] ? *(LONG *)args[1] : 19200;
    rows = args[2] ? *(LONG *)args[2] : 24;
    cols = args[3] ? *(LONG *)args[3] : 80;
    term = args[4] ? (const char *)args[4] : "xterm-256color";
    shell = args[5] ? (const char *)args[5] : upassign_vsh();
    if (!shell)
        shell = UPASSIGN_VSH_FALLBACK; /* start_shell reports the LoadSeg failure */
    dev = args[6] ? (const char *)args[6] : "serial.device";
    loop = args[7] != 0;
    memset(&g, 0, sizeof(g));
    g.port = CreateMsgPort();
    g.pport = CreateMsgPort();
    g.rd = g.port && g.pport ? (struct IOExtSer *)CreateIORequest(g.port, sizeof(struct IOExtSer)) : 0;
    g.wr = g.port ? (struct IOExtSer *)CreateIORequest(g.port, sizeof(struct IOExtSer)) : 0;
    if (!g.rd || !g.wr || OpenDevice((STRPTR)dev, (ULONG)unit, (struct IORequest *)g.rd, 0)) {
        printf("upgetty: cannot open %s unit %ld\n", dev, unit);
        rc = RETURN_FAIL;
        goto out;
    }
    /* 8N1, no XON/XOFF (zmodem's data must pass), RTS/CTS only when asked
     * (FS-UAE's serial port and a three-wire cable never raise CTS) */
    g.rd->io_Baud = (ULONG)baud;
    g.rd->io_ReadLen = g.rd->io_WriteLen = 8;
    g.rd->io_StopBits = 1;
    g.rd->io_SerFlags = SERF_XDISABLED | SERF_RAD_BOOGIE | (args[8] ? SERF_7WIRE : 0);
    g.rd->IOSer.io_Command = SDCMD_SETPARAMS;
    DoIO((struct IORequest *)g.rd);
    *g.wr = *g.rd;
    g.wr->IOSer.io_Message.mn_ReplyPort = g.port;
    if (!open_master(&g)) {
        printf("upgetty: no free PTY: pair (is PTY: mounted?)\n");
        rc = RETURN_FAIL;
        goto close_serial;
    }
    set_size(&g, (int)rows, (int)cols);
    if (!start_shell(&g, shell, term)) {
        printf("upgetty: cannot start %s on PTY:%s/s\n", shell, g.id);
        rc = RETURN_FAIL;
        goto close_master;
    }
    printf("upgetty: %s on %s unit %ld at %ld baud, PTY:%s\n", shell, dev, unit, baud, g.id);
    serial_read(&g);
    master_read(&g);
    while (running) {
        ULONG got = Wait((1UL << g.port->mp_SigBit) | (1UL << g.pport->mp_SigBit) | SIGBREAKF_CTRL_C |
                         SIGBREAKF_CTRL_E);
        if (got & SIGBREAKF_CTRL_E)
            LG_DUMP();
        if (got & SIGBREAKF_CTRL_C) {
            LG("ctrl-c to upgetty", 0);
            break;
        }
        if (g.rd_busy && CheckIO((struct IORequest *)g.rd)) {
            WaitIO((struct IORequest *)g.rd);
            g.rd_busy = 0;
            if (g.rd->IOSer.io_Error == 0) {
                /* the byte, then all the port has buffered */
                LONG n = 1;
                g.rbuf[0] = g.rbyte;
                g.rd->IOSer.io_Command = SDCMD_QUERY;
                DoIO((struct IORequest *)g.rd);
                if (g.rd->IOSer.io_Actual) {
                    ULONG k = g.rd->IOSer.io_Actual < sizeof(g.rbuf) - 1 ? g.rd->IOSer.io_Actual : sizeof(g.rbuf) - 1;
                    g.rd->IOSer.io_Command = CMD_READ;
                    g.rd->IOSer.io_Data = g.rbuf + 1;
                    g.rd->IOSer.io_Length = k;
                    DoIO((struct IORequest *)g.rd);
                    n += (LONG)g.rd->IOSer.io_Actual;
                }
                LG("serial in", n);
                /* typing on the terminal (LG evaluates its value in every
                 * build: when it did not, this Write was the debug build's
                 * only, and no key reached the shell) */
                LG("  master write", Write(g.master, g.rbuf, n));
            }
            serial_read(&g);
        }
        if (g.pkt_busy && GetMsg(g.pport)) {
            LONG n = g.pkt.sp_Pkt.dp_Res1;
            g.pkt_busy = 0;
            LG("master out", n);
            if (n > 0) {
                serial_write(&g, g.mbuf, n);
                master_read(&g);
            } else if (loop) {
                /* the shell ended (the last slave closed): a new one */
                Close(g.master);
                g.master = 0;
                if (!open_master(&g) || (set_size(&g, (int)rows, (int)cols), !start_shell(&g, shell, term)))
                    running = 0;
                else
                    master_read(&g);
            } else {
                running = 0;
            }
        }
    }
    if (g.pkt_busy) {
        /* a read still out on the master: closing the master ends it */
        Close(g.master);
        g.master = 0;
        WaitPort(g.pport);
        GetMsg(g.pport);
    }
close_master:
    if (g.master)
        Close(g.master);
close_serial:
    if (g.rd_busy) {
        AbortIO((struct IORequest *)g.rd);
        WaitIO((struct IORequest *)g.rd);
    }
    CloseDevice((struct IORequest *)g.rd);
out:
    if (g.wr)
        DeleteIORequest((struct IORequest *)g.wr);
    if (g.rd)
        DeleteIORequest((struct IORequest *)g.rd);
    if (g.port)
        DeleteMsgPort(g.port);
    if (g.pport)
        DeleteMsgPort(g.pport);
    FreeArgs(rd);
    return rc;
}
