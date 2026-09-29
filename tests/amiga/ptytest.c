/* ptytest -- PTY: (handler/pty_handler.c) through real DOS calls on the
 * rig: every line is "ok" or "FAIL" with what was seen, the last one the
 * count. Needs PTY: mounted; no typing (tools/rig/ptytest_rig.py). */
#include <string.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "../../handler/vtcon_packets.h"
#include "../../tty/ldisc.h"

static int passed, total;

/* Each line also goes to RAM:ptytest.log, closed after every line: a
 * probe that hangs still shows how far it got. */
static void logline(const char *s)
{
    BPTR f = Open((STRPTR)"RAM:ptytest.log", MODE_READWRITE);
    if (f) {
        Seek(f, 0, OFFSET_END);
        Write(f, (APTR)s, (LONG)strlen(s));
        Close(f);
    }
}

static void check(int ok, const char *what, const char *seen)
{
    total++;
    if (ok)
        passed++;
    {
        char line[400];
        LONG a[5];
        a[0] = (LONG)(ok ? "ok" : "FAIL");
        a[1] = (LONG)total;
        a[2] = (LONG)what;
        a[3] = (LONG)(seen ? ": " : "");
        a[4] = (LONG)(seen ? seen : "");
        RawDoFmt((STRPTR)"%s %ld %s%s%s\n", a, (void (*)())"\x16\xc0\x4e\x75", line);
        Write(Output(), line, (LONG)strlen(line));
        logline(line);
    }
}

/* bytes as text, control characters as ^X */
static const char *show(const char *b, LONG n)
{
    static char s[256];
    int i, k = 0;
    for (i = 0; i < n && k < 250; i++) {
        unsigned char c = (unsigned char)b[i];
        if (c < 32) {
            s[k++] = '^';
            s[k++] = (char)(c + 64);
        } else {
            s[k++] = (char)c;
        }
    }
    s[k] = 0;
    return s;
}

static int got(BPTR f, const char *want)
{
    char buf[256];
    LONG n = Read(f, buf, sizeof(buf));
    int ok = n == (LONG)strlen(want) && !memcmp(buf, want, n);
    if (!ok) {
        static char s[300];
        strcpy(s, "got ");
        strcat(s, show(buf, n > 0 ? n : 0));
        return (check(0, want, s), 0);
    }
    return 1;
}

static LONG pkt(BPTR f, LONG action, APTR a2, LONG a3)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(f);
    return DoPkt(fh->fh_Type, action, fh->fh_Arg1, (LONG)a2, a3, 0, 0);
}

static void put(BPTR f, const char *s)
{
    Write(f, (APTR)s, (LONG)strlen(s));
}

int main(void)
{
    BPTR m, m2, s, s2, star;
    struct MsgPort *devport;
    struct Process *me = (struct Process *)FindTask(0);
    vt_termios t, def;
    vt_winsize ws;
    char buf[64];
    LONG n, err;

    DeleteFile((STRPTR)"RAM:ptytest.log");
    devport = DeviceProc((STRPTR)"PTY:");
    if (!devport) {
        Printf("ptytest: PTY: is not mounted\n");
        return 20;
    }
    m = Open((STRPTR)"PTY:t0/m", MODE_OLDFILE);
    check(m != 0, "open master PTY:t0/m", 0);
    if (!m)
        return 20;
    m2 = Open((STRPTR)"PTY:t0/m", MODE_OLDFILE);
    err = IoErr();
    check(!m2 && err == ERROR_OBJECT_IN_USE, "a second master of t0 is refused, object in use", 0);
    s2 = Open((STRPTR)"PTY:none/s", MODE_OLDFILE);
    err = IoErr();
    check(!s2 && err == ERROR_OBJECT_NOT_FOUND, "a slave without a master is refused", 0);
    s = Open((STRPTR)"PTY:t0/s", MODE_OLDFILE);
    check(s != 0, "open slave PTY:t0/s", 0);
    if (!s)
        return 20;
    check(IsInteractive(s) && IsInteractive(m), "both ends are interactive", 0);
    check(((struct FileHandle *)BADDR(s))->fh_Type != devport &&
          ((struct FileHandle *)BADDR(m))->fh_Type != devport &&
          ((struct FileHandle *)BADDR(s))->fh_Type != ((struct FileHandle *)BADDR(m))->fh_Type,
          "each end has its own port in fh_Type", 0);

    /* canonical input with echo */
    put(m, "hello\r");
    if (got(s, "hello\n"))
        check(1, "slave reads the line (ICRNL)", 0);
    if (got(m, "hello\r\n"))
        check(1, "master reads the echo", 0);
    /* output processing */
    put(s, "a\nb");
    if (got(m, "a\r\nb"))
        check(1, "slave output gets ONLCR", 0);
    /* line editing: DEL erases */
    put(m, "ab\x7f" "c\r");
    if (got(s, "ac\n"))
        check(1, "VERASE edits the line", 0);
    Read(m, buf, sizeof(buf)); /* its echo */

    /* WAIT_CHAR and SCREEN_MODE travel to the fh_Type port: they only work
     * if DOS used the port the handler put there */
    check(!WaitForChar(s, 0), "WaitForChar on the slave: nothing yet", 0);
    check(SetMode(s, 1) != 0, "SetMode raw on the slave is answered", 0);
    pkt(s, ACTION_VTCON_TCGETA, &t, 0);
    check(!(t.c_lflag & LD_ICANON) && !(t.c_lflag & LD_ECHO), "raw mode is ICANON and ECHO off", 0);
    put(m, "q");
    check(WaitForChar(s, 0) != 0, "WaitForChar on the slave: a byte", 0);
    n = Read(s, buf, sizeof(buf));
    check(n == 1 && buf[0] == 'q', "raw read takes one byte, no echo", 0);
    check(!WaitForChar(m, 0), "WaitForChar on the master: nothing echoed", 0);
    SetMode(s, 0);
    check(pkt(s, ACTION_VTCON_TCGETA, &def, 0) && (def.c_lflag & LD_ICANON), "SetMode cooked again", 0);

    /* ISIG: ^C to the slave's break target, the opener (us) */
    SetSignal(0, SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F);
    put(m, "\x03");
    check((SetSignal(0, 0) & SIGBREAKF_CTRL_C) != 0, "^C raises CTRL_C at the slave's opener", 0);
    put(m, "\x1a");
    check((SetSignal(0, 0) & SIGBREAKF_CTRL_F) != 0, "^Z raises CTRL_F", 0);
    SetSignal(0, SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F);
    while (WaitForChar(m, 0))
        Read(m, buf, sizeof(buf)); /* the echo of ^C ^Z */

    /* window size: the master sets it, the slave reads it */
    pkt(s, ACTION_VTCON_GWINSZ, &ws, 0);
    check(ws.ws_col == 80 && ws.ws_row == 24, "a new pair is 80 x 24", 0);
    ws.ws_col = 100;
    ws.ws_row = 30;
    {
        struct FileHandle *fh = (struct FileHandle *)BADDR(m);
        LONG r1 = DoPkt(fh->fh_Type, ACTION_VTCON_SWINSZ, fh->fh_Arg1, (LONG)&ws, 0, 0, 0);
        LONG r2 = IoErr();
        check(r1 && r2 == (LONG)me, "SWINSZ answers the slave's break target (for SIGWINCH)", 0);
        r1 = DoPkt(fh->fh_Type, ACTION_VTCON_SWINSZ, fh->fh_Arg1, (LONG)&ws, 0, 0, 0);
        r2 = IoErr();
        check(r1 && r2 == 0, "the same size again: nobody to signal", 0);
    }
    memset(&ws, 0, sizeof(ws));
    pkt(s, ACTION_VTCON_GWINSZ, &ws, 0);
    check(ws.ws_col == 100 && ws.ws_row == 30, "the slave sees 100 x 30", 0);

    /* VMIN 0 VTIME 5: a read that times out empty */
    t = def;
    t.c_lflag &= ~(LD_ICANON | LD_ECHO);
    t.c_cc[LD_VMIN] = 0;
    t.c_cc[LD_VTIME] = 5;
    pkt(s, ACTION_VTCON_TCSETA, &t, LD_TCSANOW);
    {
        struct DateStamp d0, d1;
        LONG ticks;
        DateStamp(&d0);
        n = Read(s, buf, sizeof(buf));
        DateStamp(&d1);
        ticks = (d1.ds_Minute - d0.ds_Minute) * 3000 + d1.ds_Tick - d0.ds_Tick;
        check(n == 0 && ticks >= 20 && ticks <= 50, "VTIME 5 returns empty after half a second", 0);
    }
    pkt(s, ACTION_VTCON_TCSETA, &def, LD_TCSANOW);

    /* flow control: a slave write bigger than the queue waits for the master */
    {
        struct MsgPort *rp = CreateMsgPort();
        struct StandardPacket *sp = AllocVec(sizeof(*sp), MEMF_CLEAR | MEMF_PUBLIC);
        char *big = AllocVec(10000, MEMF_ANY), *back = AllocVec(20000, MEMF_ANY);
        struct FileHandle *fh = (struct FileHandle *)BADDR(s);
        LONG have = 0, i, replied = 0;
        if (rp && sp && big && back) {
            for (i = 0; i < 10000; i++)
                big[i] = (char)('a' + i % 26);
            sp->sp_Msg.mn_Node.ln_Name = (char *)&sp->sp_Pkt;
            sp->sp_Pkt.dp_Link = &sp->sp_Msg;
            sp->sp_Pkt.dp_Port = rp;
            sp->sp_Pkt.dp_Type = ACTION_WRITE;
            sp->sp_Pkt.dp_Arg1 = fh->fh_Arg1;
            sp->sp_Pkt.dp_Arg2 = (LONG)big;
            sp->sp_Pkt.dp_Arg3 = 10000;
            PutMsg(fh->fh_Type, &sp->sp_Msg);
            Delay(10);
            replied = GetMsg(rp) != 0;
            check(!replied, "a 10000-byte slave write waits while the master does not read", 0);
            while (have < 10000 && (n = Read(m, back + have, 20000 - have)) > 0)
                have += n;
            if (!replied) {
                WaitPort(rp);
                GetMsg(rp);
            }
            check(sp->sp_Pkt.dp_Res1 == 10000, "the write completes once the master reads", 0);
            check(have == 10000 && !memcmp(back, big, 10000), "the master reads all 10000 bytes in order", 0);
        } else {
            check(0, "flow control test: no memory", 0);
        }
        if (big)
            FreeVec(big);
        if (back)
            FreeVec(back);
        if (sp)
            FreeVec(sp);
        if (rp)
            DeleteMsgPort(rp);
    }

    /* Open("*") on the slave's port opens another slave of the pair */
    {
        struct MsgPort *old = me->pr_ConsoleTask;
        me->pr_ConsoleTask = ((struct FileHandle *)BADDR(s))->fh_Type;
        star = Open((STRPTR)"*", MODE_OLDFILE);
        me->pr_ConsoleTask = old;
        check(star && ((struct FileHandle *)BADDR(star))->fh_Type == ((struct FileHandle *)BADDR(s))->fh_Type,
              "Open(\"*\") with the slave as console is a slave of the pair", 0);
        if (star) {
            put(m, "via star\r");
            if (got(star, "via star\n"))
                check(1, "the second slave reads the terminal", 0);
            while (WaitForChar(m, 0))
                Read(m, buf, sizeof(buf));
            Close(star);
        }
    }
    /* DISK_INFO: no window */
    {
        struct InfoData *id = AllocVec(sizeof(struct InfoData), MEMF_CLEAR);
        LONG r = id ? DoPkt(((struct FileHandle *)BADDR(s))->fh_Type, ACTION_DISK_INFO, MKBADDR(id), 0, 0, 0, 0) : 0;
        check(r && id->id_VolumeNode == 0 && id->id_InUse == 0, "DISK_INFO answers without a window", 0);
        if (id)
            FreeVec(id);
    }

    /* hang-up: the master's close is end of file for the slave */
    Close(m);
    n = Read(s, buf, sizeof(buf));
    check(n == 0, "after the master closes, a slave read is end of file", 0);
    Close(s);
    m = Open((STRPTR)"PTY:t0/m", MODE_OLDFILE);
    check(m != 0, "t0 is free again once both ends closed", 0);
    /* the slaves' last close is end of file for the master */
    s = m ? Open((STRPTR)"PTY:t0/s", MODE_OLDFILE) : 0;
    if (s)
        Close(s);
    n = m ? Read(m, buf, sizeof(buf)) : -1;
    check(n == 0, "after the last slave closes, a master read is end of file", 0);
    if (m)
        Close(m);

    Printf("ptytest: passed %ld of %ld\n", (LONG)passed, (LONG)total);
    return passed == total ? 0 : 10;
}
