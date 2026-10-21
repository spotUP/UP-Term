/* uptelnet -- a telnet client for an UP-Term window (ledger A1.1): Claude
 * Code on the Mac, this Amiga its terminal over the LAN (tools/uptelnetd.py
 * is the Mac end). No ixemul: bsdsocket.library (Roadshow, AmiTCP, Miami)
 * and plain AmigaDOS, so it starts at once and fits the kit.
 *
 *   uptelnet HOST [PORT] [TERM name] [KEY file] [AGENT file] [AGENTPORT n] [NOAGENT]
 *
 * PORT defaults to 23, TERM to xterm-256color (what an UP-Term window is
 * to a Unix machine). KEY names the key file ("ID hexkey", C:Claude passes
 * ENVARC:Claude/nas-key): the login then goes by challenge-response and the
 * password is not typed (net/tn.h holds the protocol). A missing or empty
 * file asks the server to pair this Amiga: type the password once, the key
 * the server hands over is written to the file (and, for ENVARC:..., to its
 * ENV: copy), and the next start needs no password.
 *
 * The session daemon (plan nas-claude-local, NL14): after a key login (not after a
 * password) uptelnet starts wasabid (AGENT, default UP-Term:bin/wasabid, then C:wasabid) on
 * AGENTPORT (1240) in a shell of its own, with the key both ends derive for this login
 * (tn_session_key) as that shell's LOCAL variable wasabi.key, so the boot wasabid on 1234 and
 * its ENV: key are not touched. It tells the NAS the port (net/upagent.h; 0: no daemon) so
 * Claude Code there can drive this Amiga, and stops the daemon when the session ends by any
 * path. NOAGENT starts none.
 *
 * Ctrl-] ends the session; so does a Break (Ctrl-C
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
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <proto/bsdsocket.h>
#include <devices/timer.h>
#include <proto/timer.h>
#include "tn.h"
#include "upagent.h"
#include "../llm/sha256.h"
#include "../handler/vtcon_packets.h"
#include "../tty/ldisc.h"
#include "../tty/bmsg.h"
#include "../tty/qmark.h"

/* the base version and build day (upterm_version.h, written by the Makefile) */
#include "upterm_version.h"
static const char vers[] = "$VER: uptelnet " UPTERM_VERSION " (" UPTERM_DATE ") UP-Term";

struct Library *SocketBase;
struct Device *TimerBase;

static const char *keypath;        /* KEY: the key file */

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
    int bad_server;                /* the server failed the key check: close the line */
    int want_agent;                /* the key login was accepted: start the session daemon */
    int noagent;                   /* NOAGENT */
    const char *agent;             /* AGENT: the wasabid to run, else the defaults */
    LONG agentport;                /* AGENTPORT */
    int agent_up;                  /* the daemon's shell was started (and is to be stopped) */
    char agent_name[40];           /* the script's name in T: */
    char agent_path[200];          /* the wasabid it runs */
    struct Process *agent_proc;    /* the shell that runs it */
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

/* the key file written, and for ENVARC:x its ENV: copy (Install and C:Claude read either) */
static void write_key_file(const char *path, const char *text, LONG n)
{
    BPTR f = Open((STRPTR)path, MODE_NEWFILE);
    if (f) {
        Write(f, (APTR)text, n);
        Close(f);
    }
    if (!strncmp(path, "ENVARC:", 7)) {
        char env[200];
        char *slash;
        strcpy(env, "ENV:");
        strncat(env, path + 7, sizeof(env) - 5);
        slash = strrchr(env, '/');
        if (slash) {
            BPTR l;
            *slash = 0;
            l = CreateDir((STRPTR)env);
            if (l)
                UnLock(l);
            *slash = '/';
        }
        f = Open((STRPTR)env, MODE_NEWFILE);
        if (f) {
            Write(f, (APTR)text, n);
            Close(f);
        }
    }
}

/* ---- the session daemon ---- */

static void hex8(ULONG v, char *out)
{
    int i;
    for (i = 7; i >= 0; i--, v >>= 4)
        out[i] = "0123456789abcdef"[v & 15];
    out[8] = 0;
}

static int exists(const char *path)
{
    BPTR l = Lock((STRPTR)path, SHARED_LOCK);
    if (l)
        UnLock(l);
    return l != 0;
}

/* The CLI processes running `path` as their command (what Status lists): at most max of them in out.
 * The shell that runs the daemon is found this way, not by name: a process made by SystemTags has
 * no name of ours, and Status itself finds it by its command. */
static int same_name(const UBYTE *b, const char *path)
{
    int i, n = b[0];
    for (i = 0; i < n; i++) {
        char x = (char)b[1 + i], y = path[i];
        if (!y || (x >= 'A' && x <= 'Z' ? x + 32 : x) != (y >= 'A' && y <= 'Z' ? y + 32 : y))
            return 0;
    }
    return path[n] == 0;
}

static int cli_with(const char *path, struct Process **out, int max)
{
    int n = 0;
    ULONG c, top = MaxCli();
    Forbid();
    for (c = 1; c <= top && n < max; c++) {
        struct Process *p = FindCliProc(c);
        struct CommandLineInterface *cli = p ? (struct CommandLineInterface *)BADDR(p->pr_CLI) : 0;
        UBYTE *b = cli && cli->cli_CommandName ? (UBYTE *)BADDR(cli->cli_CommandName) : 0;
        if (b && same_name(b, path))
            out[n++] = p;
    }
    Permit();
    return n;
}

#define CLI_MAX 16

/* 1 when the daemon's process is still one of the CLIs running the daemon; with sigs, signal it then (no
 * other process of that command is touched: the boot wasabid may be the same file) */
static int agent_signal(sess *s, ULONG sigs)
{
    struct Process *l[CLI_MAX];
    int i, n, alive = 0;
    if (!s->agent_proc)
        return 0;
    Forbid();
    n = cli_with(s->agent_path, l, CLI_MAX);
    for (i = 0; i < n; i++)
        if (l[i] == s->agent_proc) {
            alive = 1;
            if (sigs)
                Signal(&l[i]->pr_Task, sigs);
        }
    Permit();
    return alive;
}

static void agent_path(sess *s, char *dest, int n)
{
    const char *want = s->agent ? s->agent : upagent_pick(exists);
    strncpy(dest, want ? want : UPAGENT_FALLBACK, (size_t)n - 1);
    dest[n - 1] = 0;
}

static void send_agent(sess *s, LONG port)
{
    tn_announce_agent(&s->t, (int)port);
}

/* After the key login: the shell (asynchronous, its own input and output) that sets the key and
 * runs the daemon; the NAS is told the port, or 0 when there is no daemon or it left at once */
static void start_agent(sess *s)
{
    char *path = s->agent_path, hex[65], script[360], file[48];
    struct Process *before[CLI_MAX], *after[CLI_MAX];
    int nb, na, k, m;
    BPTR f, in, out;
    int n, i;
    LONG port = 0;
    s->want_agent = 0;
    agent_path(s, path, sizeof(s->agent_path));
    if (!exists(path)) {
        Printf("uptelnet: no session daemon (%s is not there): Claude Code on the NAS cannot drive this Amiga.\r\n",
               (LONG)path);
        send_agent(s, 0);
        return;
    }
    tn_session_key(s->t.key, TN_KEY_LEN, s->t.sn, 16, s->t.cn, 16, hex);
    n = upagent_script(script, sizeof(script), path, s->agentport, hex);
    strcpy(s->agent_name, UPAGENT_NAME ".");
    hex8((ULONG)FindTask(0), s->agent_name + strlen(s->agent_name));
    strcpy(file, "T:");
    strcat(file, s->agent_name);
    f = n ? Open((STRPTR)file, MODE_NEWFILE) : 0;
    if (f) {
        Write(f, script, n);
        Close(f);
        in = Open((STRPTR)"NIL:", MODE_OLDFILE);
        out = Open((STRPTR)"NIL:", MODE_NEWFILE);
        nb = cli_with(path, before, CLI_MAX);
        strcpy(script, "Execute ");
        strcat(script, file);
        if (in && out &&
            SystemTags((STRPTR)script, SYS_Input, in, SYS_Output, out, SYS_Asynch, TRUE, TAG_END) != -1) {
            s->agent_up = 1;
            /* gone within a second or so: it did not start (no room, the port taken, a bad file) */
            for (i = 0; i < 4; i++) {
                Delay(15);
                if (!s->agent_proc) {
                    /* the CLI that was not there before */
                    na = cli_with(path, after, CLI_MAX);
                    for (k = 0; k < na; k++) {
                        for (m = 0; m < nb && before[m] != after[k]; m++)
                            ;
                        if (m == nb)
                            s->agent_proc = after[k];
                    }
                }
            }
            if (agent_signal(s, 0))
                port = s->agentport;
        } else {
            if (in)
                Close(in);
            if (out)
                Close(out);
        }
    }
    if (port)
        Printf("uptelnet: session daemon on port %ld.\r\n", port);
    else
        PutStr((STRPTR)"uptelnet: the session daemon did not start: Claude Code on the NAS cannot drive this Amiga.\r\n");
    send_agent(s, port);
}

/* Every way out of the session passes here: Ctrl-], a Break, the line dropped, a bad server */
static void stop_agent(sess *s)
{
    int i;
    char file[48];
    if (!s->agent_up)
        return;
    s->agent_up = 0;
    if (!s->agent_proc)
        PutStr((STRPTR)"uptelnet: the session daemon's shell was not found: use Status and Break to end it.\r\n");
    agent_signal(s, SIGBREAKF_CTRL_C);
    for (i = 0; i < 25 && agent_signal(s, 0); i++)
        Delay(10);
    if (agent_signal(s, 0)) {
        PutStr((STRPTR)"uptelnet: the session daemon did not stop: use Status and Break to end it.\r\n");
        return;
    }
    strcpy(file, "T:");
    strcat(file, s->agent_name);
    DeleteFile((STRPTR)file);
}

/* the key login moves on (net/tn.h): what to tell the user, before the bytes after it are shown */
static void auth_event(void *u, int state, const char *id, const unsigned char *key)
{
    sess *s = (sess *)u;
    if (state == TN_AUTH_PAIRED) {
        char line[TN_ID_MAX + TN_KEY_LEN * 2 + 3];
        LONG len = (LONG)tn_key_format(id, key, line);
        write_key_file(keypath, line, len);
        Printf("uptelnet: paired with this NAS as %s; the next start needs no password.\r\n", (LONG)id);
    } else if (state == TN_AUTH_OK) {
        s->want_agent = !s->noagent;
    } else if (state == TN_AUTH_REJECTED) {
        PutStr((STRPTR)"uptelnet: the NAS does not know this Amiga's key any more: type the password.\r\n");
    } else if (state == TN_AUTH_BAD_SERVER) {
        PutStr((STRPTR)"uptelnet: this is not the NAS this Amiga paired with (it failed the key check): line closed.\r\n");
        s->bad_server = 1;
    }
}

/* 16 bytes that differ at every start (not secret: the server's nonce is the other half) */
static void make_nonce(unsigned char cn[16])
{
    struct DateStamp ds;
    struct EClockVal ev;
    ULONG parts[6];
    llm_sha256 c;
    unsigned char h[32];
    int i;
    DateStamp(&ds);
    ev.ev_hi = ev.ev_lo = 0;
    if (TimerBase)
        ReadEClock(&ev);
    parts[0] = ev.ev_hi;
    parts[1] = ev.ev_lo;
    parts[2] = (ULONG)ds.ds_Days;
    parts[3] = (ULONG)ds.ds_Minute;
    parts[4] = (ULONG)ds.ds_Tick;
    parts[5] = (ULONG)FindTask(0);
    llm_sha256_init(&c);
    llm_sha256_add(&c, parts, sizeof(parts));
    llm_sha256_end(&c, h);
    for (i = 0; i < 16; i++)
        cn[i] = h[i];
}

/* KEY: set up the key login; the file read here ("ID hexkey"), else a pairing request */
static void setup_key(tn *t)
{
    char text[256], id[TN_ID_MAX + 1];
    unsigned char key[TN_KEY_LEN], cn[16];
    LONG n = 0;
    int have = 0;
    BPTR f = Open((STRPTR)keypath, MODE_OLDFILE);
    if (f) {
        n = Read(f, text, sizeof(text) - 1);
        Close(f);
        text[n > 0 ? n : 0] = 0;
        have = n > 0 && tn_key_parse(text, id, key);
    }
    make_nonce(cn);
    tn_set_key(t, have ? id : 0, key, cn, auth_event);
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
            if (s->bad_server)
                break;
            if (s->want_agent)
                start_agent(s);
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
    static const char tmpl[] = "HOST/A,PORT/N,TERM/K,KEY/K,AGENT/K,AGENTPORT/N,NOAGENT/S";
    LONG args[7] = { 0, 0, 0, 0, 0, 0, 0 };
    struct timerequest treq;
    int timer_open = 0;
    struct RDArgs *rd;
    static sess s;
    LONG port;
    int cols = 80, rows = 24, rc = RETURN_OK;
    if (qmark_line((const char *)GetArgStr())) {
        PutStr((STRPTR)tmpl);
        PutStr((STRPTR)"\n");
        return RETURN_OK;
    }
    rd = ReadArgs((STRPTR)tmpl, args, 0);
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
    s.agent = args[4] ? (const char *)args[4] : 0;
    s.agentport = args[5] ? *(LONG *)args[5] : UPAGENT_PORT;
    s.noagent = args[6] != 0;
    if (args[3]) {
        /* only the key login needs the clock (ReadEClock) */
        memset(&treq, 0, sizeof(treq));
        if (!OpenDevice((STRPTR)"timer.device", UNIT_MICROHZ, (struct IORequest *)&treq, 0)) {
            TimerBase = treq.tr_node.io_Device;
            timer_open = 1;
        }
        keypath = (const char *)args[3];
    }
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
    if (keypath)
        setup_key(&s.t);
    con_raw(&s);
    tn_start(&s.t);
    session(&s);
    stop_agent(&s);
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
    if (s.bad_server)
        rc = RETURN_ERROR;
out:
    if (timer_open)
        CloseDevice((struct IORequest *)&treq);
    if (s.port)
        DeleteMsgPort(s.port);
    CloseLibrary(SocketBase);
    FreeArgs(rd);
    return rc;
}
