/* pipeprobe -- when does a pipe answer a Read for more bytes than it holds?
 * Opens WNAME for writing and RNAME for reading (both PIPE:pipeprobe by
 * default; PTY:pp/w PTY:pp/r for pty-handler's pipe), writes "hello\n"
 * (6 bytes), then sends ACTION_READ for 4096 bytes and waits up to 3 s for
 * the reply. Prints "read 4096: N" (the bytes it answered with) or
 * "read 4096: no answer" (rc 5). vsh's pipes need the partial answer: a
 * coproc running cat (cat reads 4096 bytes) never got its line on PIPE:.
 * Rig 2, 2026-10-08: PIPE: no answer, PTY: 6. */
#include <dos/dos.h>
#include <dos/dosextens.h>
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
    if (!n)
        b[--i] = '0';
    while (n > 0) {
        b[--i] = (char)('0' + n % 10);
        n /= 10;
    }
    say(b + i);
}

static char buf[4096];

int main(int argc, char **argv)
{
    const char *name = argc > 1 ? argv[1] : "PIPE:pipeprobe";
    const char *rname = argc > 2 ? argv[2] : name;
    BPTR wr, rd;
    struct FileHandle *fh;
    struct MsgPort *port;
    struct StandardPacket *sp;
    int t, rc = 0;
    wr = Open((STRPTR)name, MODE_NEWFILE);
    rd = wr ? Open((STRPTR)rname, MODE_OLDFILE) : 0;
    if (!rd) {
        say("cannot open the pipe\n");
        if (wr)
            Close(wr);
        return 20;
    }
    port = CreateMsgPort();
    sp = (struct StandardPacket *)AllocMem(sizeof(*sp), MEMF_PUBLIC | MEMF_CLEAR);
    Write(wr, "hello\n", 6);
    fh = (struct FileHandle *)BADDR(rd);
    sp->sp_Msg.mn_Node.ln_Name = (char *)&sp->sp_Pkt;
    sp->sp_Msg.mn_ReplyPort = port;
    sp->sp_Pkt.dp_Link = &sp->sp_Msg;
    sp->sp_Pkt.dp_Port = port;
    sp->sp_Pkt.dp_Type = ACTION_READ;
    sp->sp_Pkt.dp_Arg1 = fh->fh_Arg1;
    sp->sp_Pkt.dp_Arg2 = (LONG)buf;
    sp->sp_Pkt.dp_Arg3 = sizeof(buf);
    PutMsg(fh->fh_Type, &sp->sp_Msg);
    for (t = 0; t < 30 && !GetMsg(port); t++)
        Delay(5);
    say("read 4096: ");
    if (t < 30) {
        num(sp->sp_Pkt.dp_Res1);
        say("\n");
    } else {
        say("no answer\n");
        rc = 5;
        Write(wr, buf, sizeof(buf)); /* fill it: the Read returns */
        WaitPort(port);
        GetMsg(port);
    }
    Close(wr);
    Close(rd);
    FreeMem(sp, sizeof(*sp));
    DeleteMsgPort(port);
    return rc;
}
