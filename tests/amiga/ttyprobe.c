/* ttyprobe -- the termios packets (handler/vtcon_packets.h) as a patched
 * ixemul will send them, run in an XCON: window by the rig
 * (tools/rig/ttyprobe_rig.py types into it and reads the screen).
 * Each step prints what it saw; the rig compares. */
#include <string.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "../../handler/vtcon_packets.h"
#include "../../tty/ldisc.h"

static struct FileHandle *in;

static LONG pkt(LONG action, APTR a2, LONG a3)
{
    return DoPkt(in->fh_Type, action, in->fh_Arg1, (LONG)a2, a3, 0, 0);
}

static void say(const char *s)
{
    Write(Output(), (APTR)s, (LONG)strlen(s));
}

static void set(vt_termios *t)
{
    if (!pkt(ACTION_VTCON_TCSETA, t, LD_TCSANOW))
        say("TCSETA refused\n");
}

int main(void)
{
    vt_termios t, def;
    vt_winsize ws;
    char buf[128];
    LONG n;
    struct DateStamp d0, d1;
    in = (struct FileHandle *)BADDR(Input());
    if (!in || !in->fh_Type)
        return 20;
    if (!pkt(ACTION_VTCON_TCGETA, &def, 0)) {
        say("no termios packets here\n");
        return 10;
    }
    Printf("1 tcgetattr: icanon %ld echo %ld isig %ld\n", (def.c_lflag & LD_ICANON) != 0,
           (def.c_lflag & LD_ECHO) != 0, (def.c_lflag & LD_ISIG) != 0);
    if (pkt(ACTION_VTCON_GWINSZ, &ws, 0))
        Printf("2 winsize: %ld x %ld (%ld x %ld px)\n", (LONG)ws.ws_col, (LONG)ws.ws_row,
               (LONG)ws.ws_xpixel, (LONG)ws.ws_ypixel);
    Flush(Output());
    /* raw, one byte */
    t = def;
    t.c_lflag &= ~(LD_ICANON | LD_ECHO);
    t.c_cc[LD_VMIN] = 1;
    t.c_cc[LD_VTIME] = 0;
    set(&t);
    say("3 raw: press a key\n");
    n = Read(Input(), buf, 1);
    Printf("3 raw got %ld byte 0x%02lx\n", n, n > 0 ? (LONG)(unsigned char)buf[0] : 0L);
    /* VMIN 0 VTIME 10: nothing within a second */
    t.c_cc[LD_VMIN] = 0;
    t.c_cc[LD_VTIME] = 10;
    set(&t);
    DateStamp(&d0);
    n = Read(Input(), buf, 10);
    DateStamp(&d1);
    Printf("4 vtime read %ld bytes after %ld ticks\n", n,
           (d1.ds_Minute - d0.ds_Minute) * 3000 + d1.ds_Tick - d0.ds_Tick);
    /* canonical without echo: a password */
    t = def;
    t.c_lflag &= ~LD_ECHO;
    set(&t);
    say("5 password: ");
    n = Read(Input(), buf, sizeof(buf) - 1);
    buf[n > 0 ? n : 0] = 0;
    Printf("\n5 read %ld: %s", n, buf);
    /* ISIG: ^C breaks */
    set(&def);
    SetSignal(0, SIGBREAKF_CTRL_C);
    say("6 type ^C: ");
    Flush(Output());
    while (!(SetSignal(0, 0) & SIGBREAKF_CTRL_C))
        Delay(5);
    say("\n6 got the break\n");
    /* OPOST off: LF is only a line feed */
    t = def;
    t.c_oflag &= ~LD_OPOST;
    set(&t);
    Write(Output(), "7 A\nB", 5);
    set(&def);
    Write(Output(), "\n8 A\nB\n", 7);
    say("9 done\n");
    return 0;
}
