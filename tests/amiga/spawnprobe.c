/* spawnprobe -- amiga-pi phase K1: which pipe handler gives a child started
 * by a native (vbcc, no ixemul) program a line-buffered stdin and stdout,
 * with end of file once the parent closes its end?  MCP's stdio transport
 * writes one JSON-RPC line to the server and must get the server's line
 * back before anything else is written, and the server must see EOF when
 * the client goes.
 *
 * `spawnprobe` runs one case per handler:
 *   PIPE:  PIPE:spi<n> / PIPE:spo<n> (one name, write end MODE_NEWFILE,
 *          read end MODE_OLDFILE)
 *   PTY:   PTY:spi<n>/w + /r and PTY:spo<n>/w + /r (pty-handler's pipe pair)
 * Each case: the child is `VTC:spawnprobe child <tag>` started with
 * SystemTags(SYS_Asynch) on the input pipe's read end and the output pipe's
 * write end. The child Read()s 4096 bytes at a time (a typical server's
 * buffered read), appends "got <n>" to RAM:spawnprobe-<tag>.log and writes
 * "echo <n>\n" back; at Read() == 0 it logs and writes "eof\n" and ends,
 * which closes both of its handles. The parent writes "ping\n" and sends one
 * ACTION_READ for 4096 bytes on the output pipe: an answer within 3 s means
 * a line goes both ways without filling a buffer. Then the parent closes
 * its write end and reads until a Read answers 0 (the child's close).
 * IXPIPE: is tried by name too: ixpipe-handler clones an ixemul process's
 * descriptor (ACTION_IXEMUL_MAGIC), so a native program is expected to get
 * no pipe from it.
 *
 * The child's log is shown before the close too: "got 5" there means the
 * child's stdin had the line while the parent's stdout read still waited.
 * Prints one verdict line per case:
 *   <tag>: line <yes|no> eof <yes|no> (child: <its log>)
 * rc 0 when at least one handler passes both. */
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>

static void say(const char *s)
{
    Write(Output(), (APTR)s, strlen(s));
}

static void num(long n)
{
    char b[12];
    int i = 11;
    b[i] = 0;
    if (n < 0) {
        say("-");
        n = -n;
    }
    if (!n)
        b[--i] = '0';
    while (n > 0) {
        b[--i] = (char)('0' + n % 10);
        n /= 10;
    }
    say(b + i);
}

static void numstr(char *out, long n)
{
    char b[12];
    int i = 11;
    b[i] = 0;
    if (!n)
        b[--i] = '0';
    while (n > 0) {
        b[--i] = (char)('0' + n % 10);
        n /= 10;
    }
    strcpy(out, b + i);
}

/* ---- the child ---- */
static void child_log(const char *tag, const char *what, long n)
{
    char name[48], line[48];
    BPTR f;
    strcpy(name, "RAM:spawnprobe-");
    strcat(name, tag);
    strcat(name, ".log");
    f = Open((STRPTR)name, MODE_READWRITE);
    if (!f)
        return;
    Seek(f, 0, OFFSET_END);
    strcpy(line, what);
    numstr(line + strlen(line), n);
    strcat(line, " ");
    Write(f, line, strlen(line));
    Close(f);
}

static int child(const char *tag)
{
    static char buf[4096];
    char line[24];
    long n;
    for (;;) {
        n = Read(Input(), buf, sizeof(buf));
        if (n <= 0)
            break;
        child_log(tag, "got ", n);
        strcpy(line, "echo ");
        numstr(line + 5, n);
        strcat(line, "\n");
        Write(Output(), line, strlen(line));
    }
    child_log(tag, "read0 ", n);
    Write(Output(), "eof\n", 4);
    return 0;
}

/* ---- the parent ---- */
static char rbuf[4096];
static struct StandardPacket *sp;
static struct MsgPort *port;
static int pending;

static void read_start(BPTR fh)
{
    struct FileHandle *h = (struct FileHandle *)BADDR(fh);
    memset(sp, 0, sizeof(*sp));
    sp->sp_Msg.mn_Node.ln_Name = (char *)&sp->sp_Pkt;
    sp->sp_Msg.mn_ReplyPort = port;
    sp->sp_Pkt.dp_Link = &sp->sp_Msg;
    sp->sp_Pkt.dp_Port = port;
    sp->sp_Pkt.dp_Type = ACTION_READ;
    sp->sp_Pkt.dp_Arg1 = h->fh_Arg1;
    sp->sp_Pkt.dp_Arg2 = (LONG)rbuf;
    sp->sp_Pkt.dp_Arg3 = sizeof(rbuf);
    PutMsg(h->fh_Type, &sp->sp_Msg);
    pending = 1;
}

/* the pending read's answer within ticks/50 s: bytes (0 = EOF), -2 none yet */
static long read_wait(int ticks)
{
    int t;
    for (t = 0; t < ticks; t += 5) {
        if (GetMsg(port)) {
            pending = 0;
            return sp->sp_Pkt.dp_Res1;
        }
        Delay(5);
    }
    return -2;
}

static void show_bytes(long n)
{
    long i;
    say(" '");
    for (i = 0; i < n && i < 40; i++)
        Write(Output(), rbuf[i] == '\n' ? "\\n" : rbuf + i, rbuf[i] == '\n' ? 2 : 1);
    say("'");
}

static void show_log(const char *tag, int keep)
{
    char name[48], b[200];
    long n;
    BPTR f;
    strcpy(name, "RAM:spawnprobe-");
    strcat(name, tag);
    strcat(name, ".log");
    f = Open((STRPTR)name, MODE_OLDFILE);
    say(" (child: ");
    if (f) {
        n = Read(f, b, sizeof(b) - 1);
        if (n > 0)
            Write(Output(), b, n);
        Close(f);
        if (!keep)
            DeleteFile((STRPTR)name);
    } else
        say("no log");
    say(")\n");
}

/* one handler: in/out pipe names (write end, read end) */
static int probe(const char *tag, const char *in_w, const char *in_r, const char *out_w, const char *out_r)
{
    BPTR iw, ir, ow, orr;
    char cmd[64];
    long n, got = 0;
    int line = 0, eof = 0, t;
    iw = Open((STRPTR)in_w, MODE_NEWFILE);
    ir = iw ? Open((STRPTR)in_r, MODE_OLDFILE) : 0;
    ow = ir ? Open((STRPTR)out_w, MODE_NEWFILE) : 0;
    orr = ow ? Open((STRPTR)out_r, MODE_OLDFILE) : 0;
    if (!orr) {
        say(tag);
        say(": cannot open the pipes, IoErr ");
        num(IoErr());
        say("\n");
        if (ow)
            Close(ow);
        if (ir)
            Close(ir);
        if (iw)
            Close(iw);
        return 0;
    }
    strcpy(cmd, "VTC:spawnprobe child ");
    strcat(cmd, tag);
    if (SystemTags((STRPTR)cmd, SYS_Input, ir, SYS_Output, ow, SYS_Asynch, TRUE, TAG_END) == -1) {
        say(tag);
        say(": SystemTags failed\n");
        Close(ow);
        Close(ir);
        Close(iw);
        Close(orr);
        return 0;
    }
    Write(iw, "ping\n", 5);
    read_start(orr);
    n = read_wait(150);
    say(tag);
    say(": first read ");
    if (n == -2)
        say("no answer in 3 s");
    else {
        num(n);
        show_bytes(n);
        line = n > 0 && !strncmp(rbuf, "echo 5\n", 7);
    }
    say(", before the close");
    show_log(tag, 1);
    Close(iw);
    for (t = 0; t < 8 && !eof; t++) {
        if (!pending)
            read_start(orr);
        n = read_wait(250);
        if (n == -2) {
            say(tag);
            say(": no answer in 5 s after the close\n");
            break;
        }
        say(tag);
        say(": read ");
        num(n);
        show_bytes(n);
        say("\n");
        if (n == 0)
            eof = 1;
        else if (n > 0)
            got += n;
        else
            break;
    }
    say(tag);
    say(": line ");
    say(line ? "yes" : "no");
    say(" eof ");
    say(eof ? "yes" : "no");
    show_log(tag, 0);
    if (pending) {
        /* the read never came back: leave the handle and packet alone */
        say(tag);
        say(": a read is still pending, handle left open\n");
        return -1;
    }
    Close(orr);
    return line && eof;
}

int main(int argc, char **argv)
{
    int ok = 0, r;
    BPTR ix;
    if (argc > 2 && !strcmp(argv[1], "child"))
        return child(argv[2]);
    port = CreateMsgPort();
    sp = (struct StandardPacket *)AllocMem(sizeof(*sp), MEMF_PUBLIC | MEMF_CLEAR);
    if (!port || !sp)
        return 20;
    r = probe("PIPE", "PIPE:spi1", "PIPE:spi1", "PIPE:spo1", "PIPE:spo1");
    if (r < 0)
        return 10; /* a pending packet points at sp */
    ok |= r;
    r = probe("PTY", "PTY:spi1/w", "PTY:spi1/r", "PTY:spo1/w", "PTY:spo1/r");
    if (r < 0)
        return 10;
    ok |= r;
    ix = Open((STRPTR)"IXPIPE:spawnprobe", MODE_NEWFILE);
    say("IXPIPE: open by name ");
    if (ix) {
        say("succeeded\n");
        Close(ix);
    } else {
        say("failed, IoErr ");
        num(IoErr());
        say("\n");
    }
    FreeMem(sp, sizeof(*sp));
    DeleteMsgPort(port);
    return ok ? 0 : 5;
}
