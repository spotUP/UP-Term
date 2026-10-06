/* uptelnet -- a telnet client for an UP-Term window (ledger A1.1): Claude
 * Code on the Mac, this Amiga its terminal over the LAN (tools/uptelnetd.py
 * is the Mac end). No ixemul: bsdsocket.library (Roadshow, AmiTCP, Miami)
 * and plain AmigaDOS, so it starts at once and fits the kit.
 *
 *   uptelnet HOST [PORT] [TERM name]
 *
 * PORT defaults to 23, TERM to xterm-256color (what an UP-Term window is
 * to a Unix machine). Ctrl-] ends the session; so does a Break (Ctrl-C
 * from Status/Break, or the window's own Ctrl-C on a console without
 * termios). The protocol is net/tn.c's (host-tested); this file is the
 * socket, the console and the loop.
 *
 * The console: the window goes raw (SetMode 1), then on an UP-Term window
 * into termios mode with no line editing, no signals and no output
 * processing (ACTION_VTCON_TCSETA), so Ctrl-C, Ctrl-Z and Ctrl-\ go to
 * the far end as bytes. Its size comes by ACTION_VTCON_GWINSZ, asked again
 * every half second: the raw resize report (CSI 12 {) arrives in-band as
 * 0x9B..., which is also a UTF-8 continuation byte and cannot be told from
 * typed text.
 *
 * The loop: one ACTION_WAIT_CHAR is always out on the console (a half
 * second's timeout), and WaitSelect waits on the socket and on that
 * packet's port at once. A WAIT_CHAR, unlike a pending READ, comes back by
 * itself, so leaving never strands a packet in the console. */
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <proto/bsdsocket.h>
#include "tn.h"
#include "../handler/vtcon_packets.h"
#include "../tty/ldisc.h"
#include "../tty/bmsg.h"

static const char vers[] = "$VER: uptelnet 1.0 (4.10.2026) UP-Term";

struct Library *SocketBase;

#define WAIT_US 500000L            /* the console's WAIT_CHAR timeout: also the size poll */
#define ESCAPE_KEY 0x1D            /* Ctrl-] */

typedef struct sess {
    LONG sock;
    BPTR in, out;
    struct MsgPort *port;          /* the console packet's replies */
    struct StandardPacket pkt;
    int pkt_busy;
    int termios;                   /* the window took ACTION_VTCON_TCSETA */
    int dead;                      /* the socket closed or failed */
    tn t;
} sess;

static void sock_out(void *u, const unsigned char *b, int n)
{
    sess *s = (sess *)u;
    while (n > 0 && !s->dead) {
        LONG k = send(s->sock, (APTR)b, n, 0);
        if (k <= 0) {
            s->dead = 1;
            return;
        }
        b += k;
        n -= (int)k;
    }
}

static void con_show(void *u, const unsigned char *b, int n)
{
    sess *s = (sess *)u;
    Write(s->out, (APTR)b, n);
}

static void wait_char(sess *s)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(s->in);
    s->pkt.sp_Msg.mn_Node.ln_Name = (char *)&s->pkt.sp_Pkt;
    s->pkt.sp_Pkt.dp_Link = &s->pkt.sp_Msg;
    s->pkt.sp_Pkt.dp_Port = s->port;
    s->pkt.sp_Pkt.dp_Type = ACTION_WAIT_CHAR;
    s->pkt.sp_Pkt.dp_Arg1 = WAIT_US;
    PutMsg(fh->fh_Type, &s->pkt.sp_Msg);
    s->pkt_busy = 1;
}

/* The window's size in cells; 0 on a console that does not say. */
static int win_size(sess *s, int *cols, int *rows)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(s->in);
    static vt_winsize w;
    if (!fh || !fh->fh_Type)
        return 0;
    if (!DoPkt(fh->fh_Type, ACTION_VTCON_GWINSZ, fh->fh_Arg1, (LONG)&w, 0, 0, 0) || !w.ws_col || !w.ws_row)
        return 0;
    *cols = w.ws_col;
    *rows = w.ws_row;
    return 1;
}

/* Raw both ways; on an UP-Term window termios with nothing processed. */
static void con_raw(sess *s)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(s->in);
    static vt_termios t;
    SetMode(s->in, 1);
    if (!fh || !fh->fh_Type)
        return;
    if (!DoPkt(fh->fh_Type, ACTION_VTCON_TCGETA, fh->fh_Arg1, (LONG)&t, 0, 0, 0))
        return;
    t.c_iflag &= ~(ld_flag)(LD_ICRNL | LD_INLCR | LD_IGNCR | LD_ISTRIP | LD_IXON | LD_IXOFF | LD_BRKINT);
    t.c_oflag &= ~(ld_flag)LD_OPOST;
    t.c_lflag &= ~(ld_flag)(LD_ICANON | LD_ECHO | LD_ECHONL | LD_ISIG | LD_IEXTEN);
    t.c_cc[LD_VMIN] = 1;
    t.c_cc[LD_VTIME] = 0;
    s->termios = DoPkt(fh->fh_Type, ACTION_VTCON_TCSETA, fh->fh_Arg1, (LONG)&t, LD_TCSANOW, 0, 0) != 0;
}

/* "what failed: the meaning" for the errors bmsg knows, "what failed (error N)" otherwise */
static void report(const char *what, LONG err)
{
    const char *why = bmsg_net_errno(err);
    if (why)
        Printf("%s: %s\n", (LONG)what, (LONG)why);
    else
        Printf("%s (error %ld)\n", (LONG)what, err);
}

static LONG connect_to(const char *host, LONG port)
{
    struct sockaddr_in a;
    struct hostent *h;
    LONG sock;
    memset(&a, 0, sizeof(a));
    a.sin_len = sizeof(a);
    a.sin_family = AF_INET;
    a.sin_port = htons((UWORD)port);
    a.sin_addr.s_addr = inet_addr((STRPTR)host);
    if (a.sin_addr.s_addr == INADDR_NONE) {
        h = gethostbyname((STRPTR)host);
        if (!h) {
            Printf("uptelnet: %s: no such host\n", (LONG)host);
            return -1;
        }
        memcpy(&a.sin_addr, h->h_addr, sizeof(a.sin_addr));
    }
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        report("no socket", Errno());
        return -1;
    }
    if (connect(sock, (struct sockaddr *)&a, sizeof(a)) < 0) {
        Printf("uptelnet: %s port %ld: ", (LONG)host, port);
        report("connection failed", Errno());
        CloseSocket(sock);
        return -1;
    }
    return sock;
}

static void session(sess *s)
{
    static unsigned char buf[2048];
    ULONG portsig = 1UL << s->port->mp_SigBit;
    int cols, rows;
    wait_char(s);
    while (!s->dead) {
        fd_set rd;
        ULONG sigs = portsig | SIGBREAKF_CTRL_C;
        LONG r;
        FD_ZERO(&rd);
        FD_SET(s->sock, &rd);
        r = WaitSelect(s->sock + 1, &rd, 0, 0, 0, &sigs);
        if (r < 0)
            break;
        if (sigs & SIGBREAKF_CTRL_C)
            break;
        if (r > 0 && FD_ISSET(s->sock, &rd)) {
            LONG n = recv(s->sock, buf, sizeof(buf), 0);
            if (n <= 0)
                break; /* the far end closed */
            tn_recv(&s->t, buf, (int)n);
        }
        if (s->pkt_busy && GetMsg(s->port)) {
            s->pkt_busy = 0;
            if (s->pkt.sp_Pkt.dp_Res1) {
                LONG n = Read(s->in, buf, sizeof(buf));
                LONG i;
                if (n <= 0)
                    break;
                for (i = 0; i < n; i++)
                    if (buf[i] == ESCAPE_KEY)
                        break;
                if (i)
                    tn_send(&s->t, buf, (int)i);
                if (i < n)
                    break; /* Ctrl-]: leave */
            }
            if (win_size(s, &cols, &rows))
                tn_size(&s->t, cols, rows);
            wait_char(s);
        }
    }
}

int main(void)
{
    static const char tmpl[] = "HOST/A,PORT/N,TERM/K";
    LONG args[3] = { 0, 0, 0 };
    struct RDArgs *rd = ReadArgs((STRPTR)tmpl, args, 0);
    static sess s;
    LONG port;
    int cols = 80, rows = 24, rc = RETURN_OK;
    if (!rd) {
        PrintFault(IoErr(), (STRPTR)"uptelnet");
        return RETURN_FAIL;
    }
    if (!vers[0])
        return RETURN_FAIL; /* keeps the version string linked in */
    port = args[1] ? *(LONG *)args[1] : 23;
    SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 4);
    if (!SocketBase) {
        Printf("uptelnet: %s\n", (LONG)bmsg_no_stack());
        FreeArgs(rd);
        return RETURN_FAIL;
    }
    memset(&s, 0, sizeof(s));
    s.in = Input();
    s.out = Output();
    s.port = CreateMsgPort();
    s.sock = s.port ? connect_to((const char *)args[0], port) : -1;
    if (s.sock < 0) {
        rc = RETURN_ERROR;
        goto out;
    }
    win_size(&s, &cols, &rows);
    Printf("uptelnet: connected to %s port %ld, %ld x %ld. Ctrl-] ends it.\n", args[0], port, (LONG)cols,
           (LONG)rows);
    tn_init(&s.t, args[2] ? (const char *)args[2] : "xterm-256color", cols, rows, sock_out, con_show, &s);
    con_raw(&s);
    tn_start(&s.t);
    session(&s);
    if (s.pkt_busy) {
        /* the WAIT_CHAR comes back within its timeout */
        WaitPort(s.port);
        GetMsg(s.port);
        s.pkt_busy = 0;
    }
    /* what the far side left on (alternate screen, scroll region, mouse...) */
    {
        const char *bye = tn_goodbye(&s.t);
        Write(s.out, (APTR)bye, (LONG)strlen(bye));
    }
    SetMode(s.in, 0); /* cooked again; on an UP-Term window it also ends termios mode */
    PutStr((STRPTR)"\nuptelnet: connection closed\n");
    CloseSocket(s.sock);
out:
    if (s.port)
        DeleteMsgPort(s.port);
    CloseLibrary(SocketBase);
    FreeArgs(rd);
    return rc;
}
