/* vsh -- a Unix-style shell for AmigaOS 3.x (vtcon phase S).
 *
 * The grammar, expansion and control logic are the portable core
 * (sh_parse.c, sh_expand.c, sh_exec.c); this file is the AmigaDOS side:
 * streams are DOS file handles, pipes are PTY: pipes (PIPE: without PTY:), commands run
 * through SystemTags (Resident commands, C:, the path, files), and
 * background or pipelined commands in a small runner process each, which
 * reports the exit status back on a message port.
 *
 * Lines are read in cooked mode, so XCON's line editor gives history,
 * Ctrl-R search, suggestions and completion. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <dos/var.h>
#include <ctype.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <proto/intuition.h>
#include <intuition/intuition.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <stdlib.h>
#include <string.h>
#include "sh_exec.h"
#include "sh_pipe.h"
#include "../handler/vtcon_packets.h"
#include "../tty/ldisc.h"
#include "../tty/bmsg.h"
#define UPASSIGN_DOS
#include "../config/upassign.h"
#include "../config/termurl.h"

#ifdef VSH_DEBUG
/* A trace on the serial port (the rig captures it in build/rig/serial.log). */
void vsh_rawputchar(__reg("a6") struct ExecBase *, __reg("d0") UBYTE) = "\tjsr\t-516(a6)";
static void tr(const char *s, long a, long b)
{
    static const char hex[] = "0123456789abcdef";
    struct ExecBase *eb = *(struct ExecBase **)4L;
    long v[2];
    int i, k;
    v[0] = a;
    v[1] = b;
    vsh_rawputchar(eb, 'V');
    vsh_rawputchar(eb, ' ');
    while (*s)
        vsh_rawputchar(eb, (UBYTE)*s++);
    for (i = 0; i < 2; i++) {
        vsh_rawputchar(eb, ' ');
        for (k = 28; k >= 0; k -= 4)
            vsh_rawputchar(eb, (UBYTE)hex[(v[i] >> k) & 15]);
    }
    vsh_rawputchar(eb, '\n');
}
#define TR(s, a, b) tr(s, (long)(a), (long)(b))
#else
#define TR(s, a, b)
#endif

static const char version[] = "$VER: vsh 0.1 (29.9.2026)";
/* the stack vsh wants: AmigaOS 3.2 and 4 start it with that, and so does
 * a vsh it runs; otherwise it swaps to 64 KB itself (main) */
static const char stack_cookie[] = "$STACK: 327680";

/* ---- the OS layer ------------------------------------------------------------- */

/* Every external command runs in a runner process of its own. A command
 * file found on disk is loaded here and run by RunCommand in the runner,
 * as the AmigaDOS Shell runs commands in its process: then the runner is
 * the command's process, and Ctrl-C can be passed on to it. Resident
 * commands and scripts go through SystemTags in the runner instead. */
typedef struct job {
    struct Message msg;
    char *cmd;              /* the whole line, for SystemTags */
    char *name;             /* the program name (SetProgramName) */
    char *args;             /* its argument string, ending in \n */
    BPTR seg;               /* the loaded command, or 0: SystemTags */
    ULONG stack;            /* its stack for RunCommand */
    BPTR in, out, err;
    int close_in, close_out, close_err;
    struct Task *task;      /* the runner, while it runs */
    char child[24];         /* the name of the Shell process SystemTags makes */
    char *env;              /* exported variables: name\0value\0 ... \0 */
    sh_shell *sub;          /* a subshell: this shell clone runs tree with io */
    sh_parse *tree;
    sh_io io;
    int done;               /* set (under Forbid) as the runner ends */
    LONG rc;
    struct Task *held;      /* its process, whose console reads wait (stopped, bg) */
    vt_termios tios;        /* the console's termios when it was suspended ... */
    int tios_saved;         /* ... if the job had set one (S8) */
    int tios_restored;      /* fg put it back: the prompt takes the Amiga mode again */
} job;

/* The pipes vsh made. AmigaOS has no SIGPIPE: a PIPE: writer whose reader
 * has gone blocks for ever (rig: List | less, q, and the shell never came
 * back; a PTY: pipe drops what it writes, and it writes on). So closing a read end reads the pipe dry, which lets a blocked
 * Write return, and a writer that writes to it gets Ctrl-C (sh_pipe.h). */
typedef sh_pipe_rec pipe_rec; /* handles and jobs are kept as void * (shell/sh_pipe.h) */

static pipe_rec pipe_tab[32];

/* Which handles line reads take a byte at a time (os_read_line): a handle
 * and its handler's port (a PIPE: handler process serves one open, so a
 * reused handle with another port is another stream). */
static struct {
    BPTR fh;
    struct MsgPort *port;
    int stream;
} rl_kind[8];
static int rl_next;

/* A break (Ctrl-C, or ^\ as CTRL_E) to a running job: its runner, which
 * is the command's process for a loaded command, and the Shell process
 * SystemTags spawned for a Resident command or a script (found by the
 * name vsh gave it). Call under Forbid. */
static void job_signal(job *j, ULONG sig)
{
    struct Task *t;
    if (j->done)
        return;
    Signal(j->task, sig);
    if (j->child[0] && (t = FindTask((STRPTR)j->child)) != 0)
        Signal(t, sig);
}

static void job_break(job *j)
{
    job_signal(j, SIGBREAKF_CTRL_C);
}

/* A Unix signal to the job's process through ixkill (vsh cannot send
 * one itself): 0 when it went out, -1 when the job is no ixemul process
 * (a native command cannot be stopped) or ixkill is missing. */
/* The job's process: the runner for a loaded command, the Shell process
 * SystemTags made for a Resident command or a script; 0 when it ended. */
static struct Task *job_process(job *j)
{
    struct Task *t;
    Forbid();
    t = j->done ? 0 : (j->child[0] ? FindTask((STRPTR)j->child) : j->task);
    Permit();
    return t;
}

static int resolve(const char *name, const char *path, BPTR *seg, char *found, long max);

/* The ixkill that sends a Unix signal (vsh itself cannot call ixemul): the one beside vsh first (the kit
 * has both in C:; a vsh run from another drawer, the rig's VTC:vsh, has its ixkill there and none in C:,
 * and kill -9 on a job said "No such process"), else where the shell finds a command (path: $PATH,
 * then the Shell's path and C:). 0 when there is none: kill names that, not "No such process". Found
 * once and kept; not found is looked up again next time (it may have been installed since). */
static const char *ixkill_name(const char *path)
{
    static char name[200];
    struct Process *me;
    BPTR l, seg;
    char found[200];
    if (name[0])
        return name;
    me = (struct Process *)FindTask(0);
    if (me->pr_HomeDir && NameFromLock(me->pr_HomeDir, (STRPTR)name + 1, sizeof(name) - 10) &&
        AddPart((STRPTR)name + 1, (STRPTR)"ixkill", sizeof(name) - 10) &&
        (l = Lock((STRPTR)name + 1, SHARED_LOCK)) != 0) {
        UnLock(l);
        name[0] = '"';
        strcat(name, "\"");
        return name;
    }
    name[0] = 0;
    if (resolve("ixkill", path, &seg, found, sizeof(found)) < 0)
        return 0;
    if (seg)
        UnLoadSeg(seg);
    if (found[0]) {
        name[0] = '"';
        strcpy(name + 1, found);
        strcat(name, "\"");
    } else
        strcpy(name, "ixkill");
    return name;
}

static const char ixkill_missing[] = "ixkill not found: looked beside vsh, in $PATH, the Shell's path and C: "
                                     "(without it only INT, QUIT, HUP and TERM reach a command)";

/* 0 when the signal went out, -1 when it did not (the task ended, or ixkill found no ixemul process
 * there), SH_SIG_NOSENDER when there is no ixkill to send it. */
static int task_unix_signal(struct Task *t, const char *sig, const char *path)
{
    char cmd[256];
    static const char hex[] = "0123456789abcdef";
    const char *ixkill;
    unsigned long a;
    int i, k;
    BPTR nil_in, nil_out;
    LONG rc;
    if (!t)
        return -1;
    if (!(ixkill = ixkill_name(path)))
        return SH_SIG_NOSENDER;
    a = (unsigned long)t;
    strcpy(cmd, ixkill);
    strcat(cmd, " -");
    strcat(cmd, sig);
    strcat(cmd, " 0x");
    k = (int)strlen(cmd);
    for (i = 0; i < 8; i++)
        cmd[k++] = hex[(a >> (28 - 4 * i)) & 15];
    cmd[k] = 0;
    nil_in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    nil_out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    rc = SystemTags((STRPTR)cmd, SYS_Input, nil_in, SYS_Output, nil_out, SYS_UserShell, TRUE, TAG_END);
    if (rc == -1) { /* not started: the streams are still ours */
        Close(nil_in);
        Close(nil_out);
    }
    return rc == 0 ? 0 : -1;
}

static int job_unix_signal(job *j, const char *sig, const char *path)
{
    return task_unix_signal(job_process(j), sig, path);
}

/* The console's termios, if a program set one (TCGETA's Res2, see
 * vtcon_packets.h); 0 when it is in the Amiga mode or no vtcon console. */
static int console_termios(vt_termios *t)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(Input());
    if (!fh || !fh->fh_Type)
        return 0;
    return DoPkt(fh->fh_Type, ACTION_VTCON_TCGETA, fh->fh_Arg1, (LONG)t, 0, 0, 0) && IoErr() == 1;
}

/* Reads from process t wait while held (a stopped or background job:
 * ACTION_VTCON_HOLD); a console that does not know it refuses. */
static void console_hold(struct Task *t, int on)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(Input());
    if (t && fh && fh->fh_Type)
        DoPkt(fh->fh_Type, ACTION_VTCON_HOLD, fh->fh_Arg1, (LONG)t, on, 0, 0);
}

static void console_set_termios(const vt_termios *t)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(Input());
    if (fh && fh->fh_Type)
        DoPkt(fh->fh_Type, ACTION_VTCON_TCSETA, fh->fh_Arg1, (LONG)t, LD_TCSANOW, 0, 0);
}

/* Close a stream; every close of a stream vsh handed out comes here. */
static void close_stream(BPTR fh)
{
    pipe_rec *pr = 0;
    int i;
    if (!fh)
        return;
    Forbid();
    for (i = 0; i < 8; i++)
        if (rl_kind[i].fh == fh)
            rl_kind[i].fh = 0;
    for (i = 0; i < 32; i++)
        if (pipe_tab[i].rd == (void *)fh || pipe_tab[i].wr == (void *)fh)
            pr = &pipe_tab[i];
    if (pr && pr->rd == (void *)fh) {
        int drain = sp_reader_closed(pr);
        Permit();
        if (drain) {
            static char sink[512]; /* only read into, never used: shared is fine */
            LONG n;
            while ((n = Read(fh, sink, sizeof(sink))) > 0) {
                Forbid();
                if (sp_drained(pr, (long)n))
                    job_break((job *)pr->writer);
                Permit();
            }
        }
        Forbid();
        pr->rd = 0;
    } else if (pr) {
        sp_writer_gone(pr);
    }
    Permit();
    Close(fh);
}

static BPTR lock_name(const char *path, char *used, int max);
static void amiga_name(const char *in, char *out, int max);

static sh_fh os_open(void *os, const char *path, int mode)
{
    BPTR fh;
    char p[256], an[256];
    (void)os;
    /* "./x" and "../x" as AmigaDOS names them (AmigaDOS has no "." entry: ". ./e.sh" and
     * ENV=./e.sh could not open a file that was there) */
    amiga_name(path, an, sizeof(an));
    path = an;
    if (path[0] == '/' && path[1] && path[1] != '/') {
        /* "/vol/x" as Unix means it when the Amiga meaning (the parent's
         * x) has nothing there: the file itself, or for a new file its
         * directory */
        BPTR l;
        char dir[256];
        const char *slash = strrchr(path, '/');
        if ((l = lock_name(path, p, sizeof(p))) != 0) {
            UnLock(l);
            path = p;
        } else if (mode != SH_OPEN_READ && slash && slash != path && slash - path < (long)sizeof(dir)) {
            memcpy(dir, path, (size_t)(slash - path));
            dir[slash - path] = 0;
            if ((l = lock_name(dir, p, sizeof(p))) != 0) {
                UnLock(l);
                if (strchr(p, ':') && p[0] != '/') {
                    AddPart((STRPTR)p, (STRPTR)(slash + 1), sizeof(p));
                    path = p;
                }
            }
        }
    }
    if (mode == SH_OPEN_READ)
        return (sh_fh)Open((STRPTR)path, MODE_OLDFILE);
    if (mode == SH_OPEN_WRITE)
        return (sh_fh)Open((STRPTR)path, MODE_NEWFILE);
    fh = Open((STRPTR)path, MODE_READWRITE);
    if (fh && mode == SH_OPEN_APPEND)
        Seek(fh, 0, OFFSET_END);
    return (sh_fh)fh;
}

static int os_remove(void *os, const char *path)
{
    char an[256];
    (void)os;
    amiga_name(path, an, sizeof(an));
    return DeleteFile((STRPTR)an) ? 0 : -1;
}

/* where process substitution puts its files: T: (RAM: when no T: is assigned) */
static const char *os_tmpdir(void *os)
{
    BPTR l = Lock((STRPTR)"T:", ACCESS_READ);
    (void)os;
    if (!l)
        return "RAM:";
    UnLock(l);
    return "T:";
}

static void os_close(void *os, sh_fh fh)
{
    (void)os;
    close_stream((BPTR)fh);
}

static long pipes;

static void hex_cat(char *s, unsigned long k)
{
    char digits[12];
    int d = 0;
    do
        digits[d++] = "0123456789abcdef"[k & 15];
    while ((k >>= 4) && d < 8);
    while (d)
        strncat(s, &digits[--d], 1);
}

/* A pipe on PTY: (handler/pty_name.h): a Read answers with what the pipe
 * holds. PIPE: (Queue-Handler) holds a Read until its whole length is there
 * or the writer closes: `coproc cat` never got its line (cat reads 4096
 * bytes), and a pipeline's reader saw nothing until 4 KB had been written
 * (rig: tests/amiga/pipeprobe). 0 when PTY: is not mounted (no "insert
 * volume" requester) or its handler has no pipes: then PIPE: it is. */
static int pty_pipe(sh_fh *rd, sh_fh *wr)
{
    char name[32];
    struct DosList *dl;
    int i, mounted;
    dl = LockDosList(LDF_DEVICES | LDF_READ);
    mounted = FindDosEntry(dl, (STRPTR)"PTY", LDF_DEVICES) != 0;
    UnLockDosList(LDF_DEVICES | LDF_READ);
    if (!mounted)
        return 0;
    for (i = 0; i < 16; i++) {
        long n;
        Forbid();
        n = ++pipes;
        Permit();
        strcpy(name, "PTY:v");
        hex_cat(name, (unsigned long)FindTask(0));
        hex_cat(name, (unsigned long)n & 0xffff); /* the id: 15 characters at most */
        strcat(name, "/w");
        *wr = (sh_fh)Open((STRPTR)name, MODE_NEWFILE);
        if (*wr)
            break;
        if (IoErr() != ERROR_OBJECT_IN_USE)
            return 0;
    }
    if (!*wr)
        return 0;
    name[strlen(name) - 1] = 'r';
    *rd = (sh_fh)Open((STRPTR)name, MODE_OLDFILE);
    if (!*rd) {
        Close((BPTR)*wr);
        *wr = 0;
        return 0;
    }
    return 1;
}

static int os_pipe(void *os, sh_fh *rd, sh_fh *wr)
{
    char name[48];
    long k;
    (void)os;
    *rd = *wr = 0;
    if (!pty_pipe(rd, wr)) {
        strcpy(name, "PIPE:vsh.");
        hex_cat(name, (unsigned long)FindTask(0));
        strcat(name, ".");
        Forbid();
        k = ++pipes;
        Permit();
        hex_cat(name, (unsigned long)k);
        *wr = (sh_fh)Open((STRPTR)name, MODE_NEWFILE);
        *rd = (sh_fh)Open((STRPTR)name, MODE_OLDFILE);
        if (!*wr || !*rd) {
            if (*wr)
                Close((BPTR)*wr);
            if (*rd)
                Close((BPTR)*rd);
            return -1;
        }
    }
    Forbid();
    for (k = 0; k < 32; k++)
        if (!pipe_tab[k].rd && !pipe_tab[k].wr) {
            sp_init(&pipe_tab[k], (void *)*rd, (void *)*wr);
            break;
        }
    Permit();
    return 0;
}

/* argv as an AmigaDOS command line: arguments with spaces, quotes or
 * semicolons in double quotes, " and * escaped with * (AmigaDOS). */
static char *command_line(char **argv)
{
    long len = 1;
    int i;
    char *s, *o;
    for (i = 0; argv[i]; i++)
        len += 2 * (long)strlen(argv[i]) + 3;
    s = (char *)malloc(len);
    if (!s)
        return 0;
    o = s;
    for (i = 0; argv[i]; i++) {
        const char *a = argv[i];
        int quote = !*a || strpbrk(a, " \t\";*=") != 0;
        if (i)
            *o++ = ' ';
        if (quote)
            *o++ = '"';
        for (; *a; a++) {
            if (quote && (*a == '"' || *a == '*'))
                *o++ = '*';
            *o++ = *a;
        }
        if (quote)
            *o++ = '"';
    }
    *o = 0;
    return s;
}

static int is_stream(BPTR fh);

/* Where a command's input file is before it runs: -1 on a console or a pipe. RunCommand (and the Shell
 * of SystemTags) put the argument line into the input handle's buffer; an ixemul command flushes that
 * buffer unread when it ends, and a flush of unread bytes seeks the file BACK by their number. The
 * shell then read the end of its own input again: `ls /nonexist_q` in `vsh -s <file` sent the shell 12
 * bytes back (`!ls` repeated forever, fc listed twice, "vsh: ho: not found"). No command moves its
 * input before where it started: a position below that one is the argument line, and is undone.
 * The patched ixemul (ixemul-vtcon _cli_parse.c) now reads that copy out at startup, as ReadArgs
 * does, so its programs also start reading where the shell's line ended (`head -n 1` in `vsh -s`);
 * this stays for the programs that still leave it: a stock ixemul, a native command without ReadArgs. */
static long input_mark(BPTR in)
{
    if (!in || IsInteractive(in) || is_stream(in))
        return -1;
    Flush(in);
    return Seek(in, 0, OFFSET_CURRENT);
}

static void input_restore(BPTR in, long mark)
{
    if (mark < 0)
        return;
    Flush(in);
    if (Seek(in, 0, OFFSET_CURRENT) < mark)
        Seek(in, mark, OFFSET_BEGINNING);
}

static void runner(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    job *j;
    long mark;
    WaitPort(&me->pr_MsgPort);
    j = (job *)GetMsg(&me->pr_MsgPort);
    TR("runner start", j, me);
    /* the exported variables, as local variables of this process: the
     * command's own (a Shell SystemTags starts copies them) */
    if (j->env) {
        const char *e = j->env;
        while (*e) {
            const char *v = e + strlen(e) + 1;
            SetVar((STRPTR)e, (STRPTR)v, -1, GVF_LOCAL_ONLY);
            e = v + strlen(v) + 1;
        }
    }
    /* its CLI carries the command's stack: a Shell SystemTags starts takes
     * its stack from there, and ixemul's stack extension reads it (it was
     * CreateNewProc's 8000, whatever vsh's stack said) */
    if (Cli())
        Cli()->cli_DefaultStack = (j->stack + 3) / 4;
    mark = input_mark(j->in);
    /* the runner has the shell's current directory (CreateNewProc copies it) */
    if (j->seg) {
        BPTR oin = SelectInput(j->in), oout = SelectOutput(j->out);
        BPTR oerr = me->pr_CES;
        if (j->err)
            me->pr_CES = j->err;
        SetProgramName((STRPTR)j->name);
        j->rc = RunCommand(j->seg, j->stack, (STRPTR)j->args, (LONG)strlen(j->args));
        TR("runcommand back", j, j->rc);
        me->pr_CES = oerr;
        SelectInput(oin);
        SelectOutput(oout);
    } else {
        /* a Shell process of its own: named, so Ctrl-C can find it, and
         * given the error stream (CreateNewProc takes NP_Error from V39) */
        j->rc = SystemTags((STRPTR)j->cmd, SYS_Input, j->in, SYS_Output, j->out,
                           SYS_UserShell, TRUE, NP_Name, (ULONG)j->child,
                           j->err ? NP_Error : TAG_IGNORE, j->err, TAG_END);
    }
    input_restore(j->in, mark);
    if (j->close_in)
        close_stream(j->in);
    if (j->close_out)
        close_stream(j->out);
    if (j->close_err && j->err != j->out)
        close_stream(j->err);
    TR("runner end", j, j->rc);
    Forbid(); /* the reply and our end, before the shell can free anything */
    j->done = 1;
    ReplyMsg(&j->msg);
}

/* The OS layer's data per process that runs the interpreter (the shell,
 * and each subshell): its shell, and the port its jobs report to. */
typedef struct vproc {
    sh_shell *sh;
    struct MsgPort *port;
} vproc;

#define PORT(os) (((vproc *)(os))->port)

/* Has job j replied on port? Call under Forbid. */
static int replied(struct MsgPort *port, job *j)
{
    struct Node *n;
    for (n = port->mp_MsgList.lh_Head; n->ln_Succ; n = n->ln_Succ)
        if (n == &j->msg.mn_Node)
            return 1;
    return 0;
}

/* The stack a command asks for in its file: the "$STACK: n" cookie
 * (AmigaOS 3.2 and 4 honour it; vsh does on 3.1 too). 0: none. */
static ULONG seg_stack(BPTR seg)
{
    static const char key[] = "$STACK:";
    for (; seg; seg = *(BPTR *)BADDR(seg)) {
        const UBYTE *p = (const UBYTE *)BADDR(seg) + 4;
        ULONG n = ((ULONG *)BADDR(seg))[-1];
        const UBYTE *end = (const UBYTE *)BADDR(seg) - 4 + n;
        for (; p + 8 < end; p++) {
            if (*p == '$' && !memcmp(p, key, 7)) {
                const UBYTE *d = p + 7;
                ULONG v = 0;
                while (d < end && *d == ' ')
                    d++;
                while (d < end && *d >= '0' && *d <= '9')
                    v = v * 10 + (*d++ - '0');
                if (v)
                    return v;
            }
        }
    }
    return 0;
}

#define MIN_COMMAND_STACK 16000

/* The stack for a command: its file's $STACK: when that is more, else
 * the Shell's stack setting (vsh's stack builtin), at least 16000. */
static ULONG command_stack(BPTR seg)
{
    struct CommandLineInterface *cli = Cli();
    ULONG st = cli ? (ULONG)cli->cli_DefaultStack * 4 : 0, want = seg ? seg_stack(seg) : 0;
    if (st < MIN_COMMAND_STACK)
        st = MIN_COMMAND_STACK;
    return want > st ? want : st;
}

/* The umask builtin's mask for the commands vsh starts. vsh is a native
 * program (no ixemul), so it cannot call umask() for them: an ixemul program
 * takes its mask from the local variable UMASK at startup (ixemul's
 * ix_open.c), and the processes vsh starts (CreateNewProc, SystemTags) copy
 * vsh's local variables. */
static void os_umask(void *os, int mask)
{
    char v[8];
    int i = 0;
    (void)os;
    v[i++] = '0';
    v[i++] = (char)('0' + ((mask >> 6) & 7));
    v[i++] = (char)('0' + ((mask >> 3) & 7));
    v[i++] = (char)('0' + (mask & 7));
    v[i] = 0;
    SetVar((STRPTR)"UMASK", (STRPTR)v, -1, GVF_LOCAL_ONLY);
}

static long os_stack(void *os, long bytes)
{
    struct CommandLineInterface *cli = Cli();
    (void)os;
    if (!cli)
        return 0;
    if (bytes > 0)
        cli->cli_DefaultStack = (bytes + 3) / 4;
    return (long)cli->cli_DefaultStack * 4;
}

/* A command name to something to run: *seg a loaded command file, or 0
 * for SystemTags (Resident commands, scripts). -1: found nowhere. The
 * Shell's order: a path as given; else the current directory, the
 * Shell's path, C:. */
/* Find a command the way a Shell does: residents, a name with a path or
 * in the current directory, then the directories of $PATH (path, Unix
 * form: sh_path_next), the Shell's path, C:. *seg is the loaded command (0
 * for a script or a resident: SystemTags runs those). A command found
 * through $PATH has its full name in found (found[0] = 0 otherwise), so a
 * script there runs as that file and not as a same-named command of the
 * Shell's path (C:Sort for sort). -1: not found. */
static int resolve(const char *name, const char *path, BPTR *seg, char *found, long max)
{
    struct CommandLineInterface *cli = Cli();
    BPTR lock, old;
    BPTR *node;
    *seg = 0;
    found[0] = 0;
    if (!strchr(name, ':') && !strchr(name, '/')) {
        struct Segment *r;
        Forbid();
        r = FindSegment((STRPTR)name, 0, 0);
        if (!r)
            r = FindSegment((STRPTR)name, 0, 1);
        Permit();
        if (r)
            return 0; /* Resident: the Shell runs it */
    }
    {
        char used[256];
        if ((lock = lock_name(name, used, sizeof(used))) != 0) {
            UnLock(lock);
            *seg = LoadSeg((STRPTR)used);
            if (strcmp(used, name) && (long)strlen(used) < max)
                strcpy(found, used); /* "/vol/x" ran as vol:x: a script there runs as that file */
            return 0; /* not loadable (a script): SystemTags */
        }
    }
    if (strchr(name, ':') || strchr(name, '/'))
        return -1;
    if (path) {
        /* no "insert volume GG" requester for a $PATH volume this machine
         * does not have: the entry is skipped */
        struct Process *me = (struct Process *)FindTask(0);
        APTR win = me->pr_WindowPtr;
        const char *p = path;
        char dir[256];
        BPTR f = 0;
        me->pr_WindowPtr = (APTR)-1;
        while (!f && sh_path_next(&p, dir, sizeof(dir))) {
            if (!dir[0])
                continue; /* the current directory: looked at above */
            if (!(lock = Lock((STRPTR)dir, SHARED_LOCK)))
                continue;
            old = CurrentDir(lock);
            if ((f = Lock((STRPTR)name, SHARED_LOCK)) != 0) {
                UnLock(f);
                *seg = LoadSeg((STRPTR)name);
                if ((long)(strlen(dir) + strlen(name) + 2) <= max) {
                    strcpy(found, dir);
                    AddPart((STRPTR)found, (STRPTR)name, max);
                }
            }
            UnLock(CurrentDir(old));
        }
        me->pr_WindowPtr = win;
        if (f)
            return 0;
    }
    for (node = cli ? (BPTR *)BADDR(cli->cli_CommandDir) : 0; node; node = (BPTR *)BADDR(node[0])) {
        if (!node[1])
            continue;
        old = CurrentDir(node[1]);
        lock = Lock((STRPTR)name, SHARED_LOCK);
        if (lock) {
            UnLock(lock);
            *seg = LoadSeg((STRPTR)name);
            CurrentDir(old);
            return 0;
        }
        CurrentDir(old);
    }
    lock = Lock((STRPTR)"C:", SHARED_LOCK);
    if (lock) {
        BPTR f;
        old = CurrentDir(lock);
        f = Lock((STRPTR)name, SHARED_LOCK);
        if (f) {
            UnLock(f);
            *seg = LoadSeg((STRPTR)name);
        }
        UnLock(CurrentDir(old));
        if (f)
            return 0;
    }
    return -1;
}

static long os_wait(void *os, long id);

static long os_run(void *os, char **argv, const sh_io *io, int wait)
{
    sh_shell *sh = ((vproc *)os)->sh;
    char *cmd = command_line(argv), *sp;
    sh_var *v;
    job *j;
    struct Process *p;
    BPTR seg;
    char found[256];
    if (!cmd)
        return -1;
    if (resolve(argv[0], sh_get(&sh->ctx, "PATH"), &seg, found, sizeof(found)) < 0) {
        free(cmd);
        if ((io->owned & SH_OWN_IN) && io->in)
            close_stream((BPTR)io->in);
        if ((io->owned & SH_OWN_OUT) && io->out)
            close_stream((BPTR)io->out);
        return -1;
    }
    if (found[0]) {
        /* found through $PATH: the command line names that file */
        char *a0 = argv[0], *full;
        argv[0] = found;
        full = command_line(argv);
        argv[0] = a0;
        if (full) {
            free(cmd);
            cmd = full;
        }
    }
    j = (job *)AllocVec(sizeof(job), MEMF_PUBLIC | MEMF_CLEAR);
    if (!j) {
        if (seg)
            UnLoadSeg(seg);
        free(cmd);
        return -1;
    }
    j->msg.mn_ReplyPort = PORT(os);
    j->msg.mn_Length = sizeof(job);
    {
        /* exported variables go to the runner, which makes them its local
         * variables (never the shell's own: NAME=v cmd must not stay) */
        long need = 1;
        char *e;
        for (v = sh->ctx.vars; v; v = v->next)
            if ((v->attr & SH_ATTR_EXPORT) && !v->arr)
                need += (long)strlen(v->name) + (long)strlen(sh_var_str(v) ? sh_var_str(v) : "") + 2;
        j->env = e = (char *)malloc(need);
        if (e) {
            for (v = sh->ctx.vars; v; v = v->next)
                if ((v->attr & SH_ATTR_EXPORT) && !v->arr) {
                    strcpy(e, v->name);
                    e += strlen(e) + 1;
                    strcpy(e, sh_var_str(v) ? sh_var_str(v) : "");
                    e += strlen(e) + 1;
                }
            *e = 0;
        }
    }
    j->cmd = cmd;
    j->seg = seg;
    j->stack = command_stack(seg);
    j->name = (char *)malloc(strlen(argv[0]) + 1);
    if (j->name)
        strcpy(j->name, argv[0]);
    /* the argument string: the line after the name, ending in a newline */
    sp = cmd;
    if (*sp == '"') {
        for (sp++; *sp && *sp != '"'; sp++)
            if (*sp == '*' && sp[1])
                sp++;
        if (*sp)
            sp++;
    } else {
        while (*sp && *sp != ' ')
            sp++;
    }
    while (*sp == ' ')
        sp++;
    j->args = (char *)malloc(strlen(sp) + 2);
    if (j->args) {
        strcpy(j->args, sp);
        strcat(j->args, "\n");
    }
    if (wait) {
        /* in the foreground: the shell's own streams, not handed over */
        j->in = (BPTR)io->in;
        j->out = (BPTR)io->out;
        j->err = (BPTR)io->err;
        j->close_in = (io->owned & SH_OWN_IN) != 0;
        j->close_out = (io->owned & SH_OWN_OUT) != 0;
        j->close_err = (io->owned & SH_OWN_ERR) != 0;
        if (!j->close_in && j->in == Input() && IsInteractive(j->in)) {
            /* its own handle on the console, not the shell's: RunCommand
             * puts the argument line in the input handle's buffer and takes
             * it back when the command returns. A suspended command has not
             * returned: the prompt read its argument line (an empty
             * command, a second prompt), and its late return put the
             * buffer back under the shell (rig: the machine rebooted when a
             * job continued with bg ended). Only on a console: "*" is
             * vsh's console even when its input is a pipe or a file (vsh -c
             * under screen's printcmd: the command waited on screen's
             * window instead of reading the pipe). */
            BPTR own = Open((STRPTR)"*", MODE_OLDFILE);
            if (own) {
                j->in = own;
                j->close_in = 1;
            }
        }
    } else {
        /* streams it does not own are the shell's. Input: NIL:. Output on
         * a console: its own handle on that console, not the shell's.
         * Output to a file or a pipe: the shell's handle itself, as Unix
         * shares the descriptor (one offset; the shell waits or writes
         * after it). "*" there was a console the process may not have:
         * the last stage of "seq 3 | cat" in "vsh script >file" wrote
         * nowhere. */
        j->in = (io->owned & SH_OWN_IN) ? (BPTR)io->in : Open((STRPTR)"NIL:", MODE_OLDFILE);
        j->close_in = 1;
        if (io->owned & SH_OWN_OUT) {
            j->out = (BPTR)io->out;
            j->close_out = 1;
        } else if (io->out && !IsInteractive((BPTR)io->out)) {
            j->out = (BPTR)io->out;
            j->close_out = 0;
        } else {
            j->out = Open((STRPTR)"*", MODE_NEWFILE);
            j->close_out = 1;
        }
        /* errors: the shell's error stream, never 0 (a process without one wrote its errors to
         * its output: `cat nofile | tr` sent cat's message down the pipe) */
        if (io->owned & SH_OWN_ERR) {
            j->err = (BPTR)io->err;
            j->close_err = 1;
        } else if (io->err && !IsInteractive((BPTR)io->err)) {
            j->err = (BPTR)io->err;
            j->close_err = 0;
        } else {
            j->err = Open((STRPTR)"*", MODE_NEWFILE);
            j->close_err = j->err != 0;
        }
    }
    /* NP_CopyVars FALSE: the command's environment is the shell's exported variables (runner sets
     * them) and nothing else; a copy of this process's local variables brought back the ones the
     * shell had unset (`unset SHLVL` in a vsh run by vsh: the child still saw the outer SHLVL) */
    p = (j->name && j->args)
        ? CreateNewProcTags(NP_Entry, (ULONG)runner, NP_Name, (ULONG)"vsh job", NP_StackSize, 8000,
                            NP_Cli, TRUE, NP_CopyVars, FALSE, TAG_END)
        : 0;
    if (!p) {
        if (j->close_in)
            close_stream(j->in);
        if (j->close_out)
            close_stream(j->out);
        if (j->close_err && j->err != j->out)
            close_stream(j->err);
        if (seg)
            UnLoadSeg(seg);
        free(j->name);
        free(j->args);
        free(cmd);
        free(j->env);
        FreeVec(j);
        return -1;
    }
    j->task = &p->pr_Task;
    if (!seg) {
        static const char hex[] = "0123456789abcdef";
        unsigned long a = (unsigned long)j;
        int i;
        strcpy(j->child, "vsh command ");
        for (i = 0; i < 8; i++)
            j->child[12 + i] = hex[(a >> (28 - 4 * i)) & 15];
        j->child[20] = 0;
    }
    if (j->close_out) {
        int i;
        Forbid();
        for (i = 0; i < 32; i++)
            if (pipe_tab[i].wr == (void *)j->out)
                pipe_tab[i].writer = j;
        Permit();
    }
    SetSignal(0, SIGBREAKF_CTRL_C); /* an old Ctrl-C is not for this command */
    PutMsg(&p->pr_MsgPort, &j->msg);
    if (wait)
        return os_wait(os, (long)j);
    return (long)j;
}

/* ^Z while a job runs in the foreground: SIGTSTP to it. It stays (its
 * reply comes when it ends, after fg or bg); the console's termios, if
 * the job had set one, is kept for fg, and the prompt gets the Amiga mode
 * back. 0: suspended. */
static int suspend_job(void *os, job *j)
{
    sh_shell *sh = ((vproc *)os)->sh;
    int rc;
    TR("suspend", j, j->task);
    if ((rc = job_unix_signal(j, "TSTP", sh_get(&sh->ctx, "PATH"))) != 0) {
        const char *m = rc == SH_SIG_NOSENDER ? ixkill_missing : "only ixemul programs can be suspended";
        Write((BPTR)sh->io.err, (APTR)"\nvsh: ", 6);
        Write((BPTR)sh->io.err, (APTR)m, (LONG)strlen(m));
        Write((BPTR)sh->io.err, (APTR)"\n", 1);
        return -1;
    }
    /* a read it sent before it stopped waits: the prompt's line is ours */
    j->held = job_process(j);
    console_hold(j->held, 1);
    j->tios_saved = console_termios(&j->tios);
    SetMode(Input(), 0);
    sh->os.stopped = (long)j;
    return 0;
}

static int os_cont(void *os, long id)
{
    TR("cont", id, 0);
    return job_unix_signal((job *)id, "CONT", sh_get(&((vproc *)os)->sh->ctx, "PATH")) ? -1 : 0;
}

/* Wait for a job. Ctrl-C (and ^\) while waiting goes on to the job's
 * runner, the command's process, as long as it runs (the console signals
 * the shell); ^Z suspends it when the shell allows (SH_STOPPED). */
static long os_wait(void *os, long id)
{
    job *j = (job *)id;
    sh_shell *sh = ((vproc *)os)->sh;
    LONG rc;
    int broke = 0;
    TR("wait", j, sh->os.suspendable);
    if (sh->os.suspendable) {
        SetSignal(0, SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F); /* an old ^Z or ^\ is not for this job */
        /* fg: the terminal is the job's again */
        console_hold(j->held, 0);
        j->held = 0;
    }
    if (sh->os.suspendable && j->tios_saved) {
        /* fg: the job's own terminal settings again */
        console_set_termios(&j->tios);
        j->tios_saved = 0;
        j->tios_restored = 1;
    }
    for (;;) {
        ULONG got;
        Forbid();
        if (replied(PORT(os), j)) {
            Remove(&j->msg.mn_Node);
            Permit();
            break;
        }
        Permit();
        got = Wait((1UL << PORT(os)->mp_SigBit) | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E |
                   SIGBREAKF_CTRL_F);
        if (got & (SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E)) {
            Forbid(); /* still running: its reply is not in yet */
            if (!replied(PORT(os), j))
                job_signal(j, got & (SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E));
            Permit();
            if (got & SIGBREAKF_CTRL_C)
                broke = 1;
        }
        if ((got & SIGBREAKF_CTRL_F) && sh->os.suspendable && !j->done && suspend_job(os, j) == 0)
            return SH_STOPPED;
    }
    TR("waited", j, j->rc);
    if (j->held) /* it ended in the background */
        console_hold(j->held, 0);
    if (j->tios_restored)
        SetMode(Input(), 0); /* the settings were vsh's to put back: the prompt's mode */
    /* the break was for the whole line: raised again for the interpreter,
     * which stops what follows (a loop around the command) */
    if (broke)
        SetSignal(SIGBREAKF_CTRL_C, SIGBREAKF_CTRL_C);
    rc = j->rc;
    {
        int i;
        Forbid();
        for (i = 0; i < 32; i++)
            if (pipe_tab[i].writer == j)
                pipe_tab[i].writer = 0;
        Permit();
    }
    if (j->seg)
        UnLoadSeg(j->seg);
    free(j->name);
    free(j->args);
    free(j->cmd);
    free(j->env);
    FreeVec(j);
    return rc;
}

/* Has a job ended? Its reply is on the job port then. */
static int os_done(void *os, long id)
{
    int r;
    Forbid();
    r = replied(PORT(os), (job *)id);
    Permit();
    return r;
}

/* Is t a task that exists now? (a pid kill is given is a task's address, as ixemul numbers it) */
static int task_alive(struct Task *t)
{
    struct Node *n;
    int found = t == FindTask(0);
    Forbid();
    for (n = SysBase->TaskReady.lh_Head; !found && n->ln_Succ; n = n->ln_Succ)
        found = n == (struct Node *)t;
    for (n = SysBase->TaskWait.lh_Head; !found && n->ln_Succ; n = n->ln_Succ)
        found = n == (struct Node *)t;
    Permit();
    return found;
}

/* kill: sig (0: only test) to a job of this shell, a pid, or the shell. Ctrl-C and ^\ go
 * the way the console's go: the break bits, which a native command honours too. Any other
 * signal goes through ixkill, so only an ixemul process gets it; HUP and TERM fall back to
 * the break to a command that is no ixemul process. The shell itself: INT is a Ctrl-C to
 * it, TERM HUP QUIT and KILL end it, the rest are ignored. 0 = sent. */
static int os_signal(void *os, long target, int sig, int is_job)
{
    sh_shell *sh = ((vproc *)os)->sh;
    struct Task *self = FindTask(0), *t = 0;
    job *j = 0;
    char num[8];
    int i, ok, rc;
    if (!target)
        return -1;
    if (is_job)
        j = (job *)target;
    else
        for (i = 0; i < 32; i++)
            if (sh->jobs[i] == target)
                j = (job *)target;
    if (!j) {
        t = (struct Task *)target;
        if (!task_alive(t))
            return -1;
    } else if (j->done)
        return -1;
    if (sig == 0)
        return 0;
    if (t == self) {
        if (sig == 2)
            Signal(self, SIGBREAKF_CTRL_C);
        else if (sig == 1 || sig == 3 || sig == 9 || sig == 15) {
            sh->exiting = 1;
            sh->exit_status = 128 + sig;
        }
        return 0;
    }
    if (sig == 2 || sig == 3) {
        ULONG bit = sig == 2 ? SIGBREAKF_CTRL_C : SIGBREAKF_CTRL_E;
        Forbid();
        if (j)
            job_signal(j, bit);
        else
            Signal(t, bit);
        Permit();
        return 0;
    }
    sh_ltoa(sig, num);
    rc = j ? job_unix_signal(j, num, sh_get(&sh->ctx, "PATH")) : task_unix_signal(t, num, sh_get(&sh->ctx, "PATH"));
    ok = rc == 0;
    if (!ok && (sig == 1 || sig == 15)) {
        Forbid();
        if (j)
            job_signal(j, SIGBREAKF_CTRL_C);
        else
            Signal(t, SIGBREAKF_CTRL_C);
        Permit();
        ok = 1;
    }
    if (!ok && rc == SH_SIG_NOSENDER) {
        sh->os.signal_why = ixkill_missing;
        return SH_SIG_NOSENDER;
    }
    return ok ? 0 : -1;
}

static int os_isatty(void *os, sh_fh fh)
{
    (void)os;
    return fh && IsInteractive((BPTR)fh);
}

/* read -t: an interactive handle is asked with WaitForChar (microseconds); a file or pipe is
 * ready at once (unconfirmed for pipes: the rig decides) */
/* time of day from the DOS clock (local time, 50 ticks a second), seconds since 1970 */
static long os_now(void *os, long *usec)
{
    struct DateStamp ds;
    (void)os;
    DateStamp(&ds);
    *usec = (ds.ds_Tick % 50) * 20000L;
    return (ds.ds_Days + 2922L) * 86400L + ds.ds_Minute * 60L + ds.ds_Tick / 50L;
}

static int os_ready(void *os, sh_fh fh, long ms)
{
    (void)os;
    if (!IsInteractive((BPTR)fh))
        return 1;
    return WaitForChar((BPTR)fh, ms > 2000000 ? 2000000000L : ms * 1000L) != 0;
}

/* read -s: raw mode has no echo (and no line editing; read -s ends a line at CR) */
static void os_echo(void *os, sh_fh fh, int on)
{
    (void)os;
    SetMode((BPTR)fh, on ? 0 : 1);
}

static long os_write(void *os, sh_fh fh, const char *b, long n)
{
    (void)os;
    return Write((BPTR)fh, (APTR)b, n);
}

/* A stream that cannot seek (a pipe, a socket): Seek fails on it. Asked
 * once per handle (rl_kind), not per line. */
static int is_stream(BPTR fh)
{
    struct FileHandle *f = (struct FileHandle *)BADDR(fh);
    int i, st;
    if (!f || IsInteractive(fh))
        return 0;
    Forbid();
    for (i = 0; i < 8; i++)
        if (rl_kind[i].fh == fh && rl_kind[i].port == f->fh_Type) {
            st = rl_kind[i].stream;
            Permit();
            return st;
        }
    Permit();
    /* the rig's L:Queue-Handler answers Seek with 0 and ERROR_ACTION_NOT_KNOWN, not -1 */
    SetIoErr(0);
    st = Seek(fh, 0, OFFSET_CURRENT) < 0 || IoErr() == ERROR_ACTION_NOT_KNOWN;
    Forbid();
    i = rl_next++ & 7;
    rl_kind[i].fh = fh;
    rl_kind[i].port = f->fh_Type;
    rl_kind[i].stream = st;
    Permit();
    return st;
}

/* One line. From a stream a byte at a time, as bash reads one: a buffered
 * read (FGets) waits on PIPE: until its whole buffer is filled or the
 * writer closes, so a coproc that answered a line and waited for the next
 * never got the line to the shell (both hung), and FGets took bytes past
 * the line that the next reader of the pipe should get. */
static long os_read_line(void *os, sh_fh fh, char *buf, long max)
{
    (void)os;
    if (is_stream((BPTR)fh)) {
        long n = 0;
        char c;
        while (n < max - 1 && Read((BPTR)fh, &c, 1) == 1) {
            buf[n++] = c;
            if (c == '\n')
                break;
        }
        buf[n] = 0;
        TR("read_line stream", fh, n);
        return n ? n : -1;
    }
    if (!FGets((BPTR)fh, (STRPTR)buf, max)) {
        TR("read_line eof", fh, IoErr());
        return -1;
    }
    /* A file: its position is where this line ends, not where the read-ahead stopped (bash seeks back
     * the same way). The handle and its buffer are shared with every command that inherits it: a
     * command that ran after a buffered line (an ixemul ls) left the shell reading the file's last
     * bytes a second time (`!ls` ran forever, fc listed twice) */
    if (!IsInteractive((BPTR)fh))
        Flush((BPTR)fh);
    TR("read_line", fh, strlen(buf));
    return (long)strlen(buf);
}

/* Unix-style directory names: .. is the parent (/), . and "" stay. */
/* A Unix-style relative name as AmigaDOS writes it: each leading ../ is
 * a / (the parent), ./ goes, and . and .. alone are "" and "/". */
static void amiga_name(const char *in, char *out, int max)
{
    int n = 0;
    /* the Unix devices a script names: /dev/null is NIL:, /dev/tty the console (". /dev/null",
     * test -c /dev/null) */
    if (!strcmp(in, "/dev/null") || !strcmp(in, "/dev/tty")) {
        strncpy(out, in[5] == 'n' ? "NIL:" : "*", (size_t)max - 1);
        out[max - 1] = 0;
        return;
    }
    for (;;) {
        if (!strncmp(in, "./", 2))
            in += 2;
        else if (!strncmp(in, "../", 3) || !strcmp(in, "..")) {
            if (n < max - 1)
                out[n++] = '/';
            in += in[2] ? 3 : 2;
        } else if (!strcmp(in, "."))
            in++;
        else
            break;
    }
    out[n] = 0;
    strncat(out, in, max - n - 1);
}

/* path as AmigaDOS names it, into used: the Amiga meaning first ("/x" is
 * the parent's x), and when nothing is there by it, the Unix one ("/vol/x"
 * is vol:x, as $PATH and ixemul programs name it -- V2: "/VTC/bin/nvim"
 * was "not found"). The lock, or 0. */
static BPTR lock_name(const char *path, char *used, int max)
{
    BPTR lock;
    amiga_name(path, used, max);
    if ((lock = Lock((STRPTR)used, SHARED_LOCK)) != 0)
        return lock;
    if (sh_unix_root(path, used, max)) {
        /* the Unix reading is a guess ("/vol/x"): a volume this machine does
         * not have (UP-Term: before its assign) is "no such file", never an
         * "insert volume" requester */
        struct Process *me = (struct Process *)FindTask(0);
        APTR win = me->pr_WindowPtr;
        me->pr_WindowPtr = (APTR)-1;
        lock = Lock((STRPTR)used, SHARED_LOCK);
        me->pr_WindowPtr = win;
        if (lock)
            return lock;
    }
    amiga_name(path, used, max); /* not there either way: the Amiga name */
    return 0;
}

static int os_chdir(void *os, const char *path)
{
    char p[256];
    BPTR lock;
    (void)os;
    lock = lock_name(path, p, sizeof(p));
    if (!lock)
        return -1;
    UnLock(CurrentDir(lock));
    {
        char name[256];
        if (NameFromLock(lock, (STRPTR)name, sizeof(name)))
            SetCurrentDirName((STRPTR)name);
    }
    return 0;
}

static char *os_cwd(void *os)
{
    char *name = (char *)malloc(256);
    (void)os;
    if (name && !NameFromLock(((struct Process *)FindTask(0))->pr_CurrentDir, (STRPTR)name, 256))
        name[0] = 0;
    return name;
}

/* cd -P, pwd -P: a Lock on the path and NameFromLock give the name AmigaDOS itself resolves it to (a
 * soft link followed, an assign turned into its volume and directory). Unconfirmed until the rig: that
 * a soft link in the middle of a path is followed by Lock() on every handler. */
static char *os_realpath(void *os, const char *path)
{
    char p[256], *name = (char *)malloc(256);
    BPTR lock;
    (void)os;
    if (!name)
        return 0;
    lock = lock_name(path, p, sizeof(p));
    if (!lock) {
        free(name);
        return 0;
    }
    if (!NameFromLock(lock, (STRPTR)name, 256)) {
        free(name);
        name = 0;
    }
    UnLock(lock);
    return name;
}

/* test -e -f -d -s -r -w -x ... : a FileInfoBlock. The owner's RWED bits are active-low;
 * the script bit also makes a file executable. Unconfirmed until the rig: how a soft
 * link is detected without following it (nofollow is ignored: -L is never true), and
 * which bits ixemul-built binaries carry. */
static int same_name(const char *a, const char *b)
{
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
        a++;
        b++;
    }
    return !*a && !*b;
}

/* Is `path` itself a soft link (not what it points to)? Lock follows links, so the entry is
 * looked up by name among its directory's entries, where ExNext reports ST_SOFTLINK. */
static int os_is_link(const char *path)
{
    struct Process *me = (struct Process *)FindTask(0);
    APTR win = me->pr_WindowPtr;
    char dir[512];
    const char *leaf = path, *q;
    BPTR lock;
    struct FileInfoBlock *fib;
    int found = 0;
    size_t dn;
    for (q = path; *q; q++)
        if (*q == '/' || *q == ':')
            leaf = q + 1;
    dn = (size_t)(leaf - path);
    if (!*leaf || dn >= sizeof(dir))
        return 0;
    memcpy(dir, path, dn);
    dir[dn] = 0;
    me->pr_WindowPtr = (APTR)-1;
    lock = Lock((STRPTR)dir, SHARED_LOCK);
    me->pr_WindowPtr = win;
    if (!lock)
        return 0;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib && Examine(lock, fib)) {
        while (ExNext(lock, fib)) {
            if (same_name(fib->fib_FileName, leaf)) {
                found = fib->fib_DirEntryType == 3;   /* ST_SOFTLINK */
                break;
            }
        }
    }
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return found;
}

/* Can a file be run? Amiga protection bits say "not forbidden" for every file a program made, a
 * text file included, so a file is executable when E allows it and it is a script (the S bit) or
 * starts with the hunk header 0x000003F3. */
static int os_runnable(const char *path, unsigned long prot)
{
    BPTR fh;
    unsigned long magic = 0;
    if (prot & (1UL << 6))
        return 1;
    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh)
        return 0;
    if (Read(fh, &magic, 4) != 4)
        magic = 0;
    Close(fh);
    return magic == 0x000003F3UL;
}

static int os_stat(void *os, const char *path, sh_stat *st, int nofollow)
{
    struct Process *me = (struct Process *)FindTask(0);
    APTR win = me->pr_WindowPtr;
    BPTR lock;
    struct FileInfoBlock *fib;
    int r = -1;
    char an[256];
    (void)os;
    amiga_name(path, an, sizeof(an)); /* test -r ./x: AmigaDOS has no "." entry */
    path = an;
    if (nofollow && os_is_link(path)) {
        memset(st, 0, sizeof(*st));
        st->type = SH_ST_FILE;
        st->link = 1;
        st->mode = 0777u;
        st->access = 7;
        st->owned = st->group = 1;
        return 0;
    }
    if (!strcmp(path, "NIL:") || !strcmp(path, "*")) {
        /* /dev/null and /dev/tty: character devices anybody reads and writes; no lock names them */
        memset(st, 0, sizeof(*st));
        st->type = SH_ST_CHAR;
        st->access = 6;
        st->mode = 0666u;
        st->owned = st->group = 1;
        return 0;
    }
    /* a question ("is there a file?"), never an "insert volume" requester: a $PATH entry on a
     * volume this machine does not have made `type x` wait for a click nobody gives */
    me->pr_WindowPtr = (APTR)-1;
    lock = Lock((STRPTR)path, SHARED_LOCK);
    me->pr_WindowPtr = win;
    if (!lock)
        return -1;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib && Examine(lock, fib)) {
        long secs = (long)fib->fib_Date.ds_Days * 86400L + (long)fib->fib_Date.ds_Minute * 60L +
                    (long)fib->fib_Date.ds_Tick / 50L;
        unsigned long p = (unsigned long)fib->fib_Protection;
        memset(st, 0, sizeof(*st));
        st->type = fib->fib_DirEntryType > 0 ? SH_ST_DIR : SH_ST_FILE;
        st->size = st->type == SH_ST_DIR ? 512 : fib->fib_Size;   /* a directory takes a block, as on Unix: test -s */
        st->mtime = st->atime = secs;
        st->access = ((p & (1UL << 3)) ? 0u : 4u) | ((p & (1UL << 2)) ? 0u : 2u) |
                     ((!(p & (1UL << 1)) || (p & (1UL << 6))) ? 1u : 0u);
        if (st->type == SH_ST_FILE && (st->access & 1) && !os_runnable(path, p))
            st->access &= ~1u;
        st->mode = ((st->access & 4) ? 0444u : 0u) | ((st->access & 2) ? 0200u : 0u) | ((st->access & 1) ? 0111u : 0u);
        st->owned = st->group = 1;
        st->dev = (long)((struct FileLock *)BADDR(lock))->fl_Volume;
        st->ino = fib->fib_DiskKey;
        r = 0;
    }
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return r;
}

/* For globs: the names in a directory. */
static int list_dir(sh_ctx *c, const char *dir, sh_list *out)
{
    BPTR lock = Lock((STRPTR)dir, SHARED_LOCK);
    struct FileInfoBlock *fib;
    (void)c;
    if (!lock)
        return -1;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib && Examine(lock, fib))
        while (ExNext(lock, fib))
            sh_list_add(out, (const char *)fib->fib_FileName);
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return 0;
}

static long os_read(void *os, sh_fh fh, char *buf, long max)
{
    LONG n;
    (void)os;
    n = Read((BPTR)fh, buf, max);
    return n > 0 ? n : 0;
}

/* Ctrl-C since the last look (the console signals the interpreter's
 * process; os_wait raises it again after passing it on). */
static int os_interrupted(void *os)
{
    (void)os;
    return (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) != 0;
}

#define VSH_STACK_SUB 131072
#define STACK_MARGIN 12288  /* the deepest path below a guard: read's buffers, DOS calls */

/* Where the interpreter must stop in this process: the bottom of its
 * stack plus a margin (the core reports "nested too deeply" there). */
static unsigned long stack_limit_here(void)
{
    struct Task *t = FindTask(0);
    return (unsigned long)t->tc_SPLower + STACK_MARGIN;
}

/* A subshell's process: the shell clone runs its tree (sh_run_child),
 * with a job port of its own for the commands it starts. */
static void subshell_proc(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    job *j;
    vproc vp;
    WaitPort(&me->pr_MsgPort);
    j = (job *)GetMsg(&me->pr_MsgPort);
    vp.sh = j->sub;
    vp.port = CreateMsgPort();
    if (vp.port) {
        j->sub->os.data = &vp;
        j->sub->ctx.pid = (long)me;
        j->sub->stack_limit = stack_limit_here();
        j->rc = sh_run_child(j->sub, j->tree, &j->io);
        DeleteMsgPort(vp.port);
    } else {
        /* cannot run: its streams and memory go all the same */
        if (j->io.owned & SH_OWN_IN)
            close_stream((BPTR)j->io.in);
        if (j->io.owned & SH_OWN_OUT)
            close_stream((BPTR)j->io.out);
        if ((j->io.owned & SH_OWN_ERR) && j->io.err != j->io.out)
            close_stream((BPTR)j->io.err);
        sh_parse_free(j->tree);
        free(j->tree);
        sh_shell_free(j->sub);
        free(j->sub);
        j->rc = 20;
    }
    Forbid(); /* the reply and our end, before the shell can free anything */
    j->done = 1;
    ReplyMsg(&j->msg);
}

static long os_spawn(void *os, sh_shell *child, sh_parse *tree, const sh_io *io, int wait)
{
    job *j = (job *)AllocVec(sizeof(job), MEMF_PUBLIC | MEMF_CLEAR);
    struct Process *p = 0;
    if (j) {
        j->msg.mn_ReplyPort = PORT(os);
        j->msg.mn_Length = sizeof(job);
        j->sub = child;
        j->tree = tree;
        j->io = *io;
        if (wait == SH_SPAWN_BG) {
            /* in the background: streams it does not own are the shell's;
             * it gets its own handles on the same console (NIL: for input) */
            if (!(io->owned & SH_OWN_IN)) {
                j->io.in = (sh_fh)Open((STRPTR)"NIL:", MODE_OLDFILE);
                j->io.owned |= SH_OWN_IN;
            }
            /* output as os_run gives a background command: a file or a pipe is the shell's handle
             * itself (Unix shares the descriptor); "*" was a console the process may not have:
             * `echo bg &` in `vsh script >file` (Run >NIL:) wrote nowhere */
            if (!(io->owned & SH_OWN_OUT) && !(io->out && !IsInteractive((BPTR)io->out))) {
                j->io.out = (sh_fh)Open((STRPTR)"*", MODE_NEWFILE);
                j->io.owned |= SH_OWN_OUT;
            }
            if (!(io->owned & SH_OWN_ERR)) {
                /* the shell's error stream, as Unix shares the descriptor: a file or a pipe is
                 * that handle itself; a console its own handle on it. Never its output: the
                 * errors (and set -x) of a $( ) went into the capture */
                if (io->err && !IsInteractive((BPTR)io->err))
                    j->io.err = io->err;
                else if (!(io->owned & SH_OWN_OUT) && j->io.out != io->out)
                    j->io.err = j->io.out; /* the console handle opened for its output */
                else if ((j->io.err = (sh_fh)Open((STRPTR)"*", MODE_NEWFILE)) != 0)
                    j->io.owned |= SH_OWN_ERR;
            }
        }
        p = CreateNewProcTags(NP_Entry, (ULONG)subshell_proc, NP_Name, (ULONG)"vsh subshell",
                              NP_StackSize, VSH_STACK_SUB, NP_Cli, TRUE, TAG_END);
    }
    if (!p) {
        sh_io *o = j ? &j->io : 0;
        const sh_io *c = o ? o : io;
        if (c->owned & SH_OWN_IN)
            close_stream((BPTR)c->in);
        if (c->owned & SH_OWN_OUT)
            close_stream((BPTR)c->out);
        if ((c->owned & SH_OWN_ERR) && c->err != c->out)
            close_stream((BPTR)c->err);
        if (j)
            FreeVec(j);
        return -1;  /* child and tree stay the caller's */
    }
    j->task = &p->pr_Task;
    if (j->io.owned & SH_OWN_OUT) {
        int i;
        Forbid();
        for (i = 0; i < 32; i++)
            if (pipe_tab[i].wr == (void *)j->io.out)
                pipe_tab[i].writer = j;
        Permit();
    }
    PutMsg(&p->pr_MsgPort, &j->msg);
    if (wait == SH_SPAWN_FG)
        return os_wait(os, (long)j);
    return (long)j;
}

/* The console's completion and colouring learn vsh's own words (its
 * builtins, functions, aliases and variables): sent before a prompt when
 * they have changed. A console that is not vtcon refuses the packet. */
static char words_sent[3][VTCON_WORDS_MAX];
static long words_sent_len[3] = { -1, -1, -1 };

static void send_words(sh_shell *sh)
{
    static char buf[VTCON_WORDS_MAX];
    struct FileHandle *fh = (struct FileHandle *)BADDR(Input());
    int kind;
    static int refused;
    if (refused || !fh || !fh->fh_Type)
        return;
    for (kind = 1; kind <= 2; kind++) {
        long n = sh_word_list(sh, kind == 1 ? SH_WORDS_COMMANDS : SH_WORDS_VARIABLES, buf,
                              sizeof(buf));
        if (n == words_sent_len[kind] && !memcmp(buf, words_sent[kind], n))
            continue;
        if (!DoPkt(fh->fh_Type, ACTION_VTCON_WORDS, fh->fh_Arg1,
                   kind == 1 ? VTCON_WORDS_COMMANDS : VTCON_WORDS_VARIABLES, (LONG)buf, n, 0)) {
            refused = 1; /* not vtcon (CON:): never again */
            return;
        }
        memcpy(words_sent[kind], buf, n);
        words_sent_len[kind] = n;
    }
}

/* W46: the environment the window tells its programs (/term, /colors): asked
 * at the start and before a prompt, applied when it changed -- not every
 * prompt, so a `export TERM=x` typed by hand stays until the window says
 * something new. A console that does not know the packet is not asked again. */
static char env_sent[VTCON_ENV_MAX];
static long env_sent_len = -1;

static void sync_env(sh_shell *sh)
{
    char buf[VTCON_ENV_MAX];
    struct FileHandle *fh = (struct FileHandle *)BADDR(Input());
    static int refused;
    long n = 0;
    if (refused || !fh || !fh->fh_Type)
        return;
    memset(buf, 0, sizeof(buf));
    if (!DoPkt(fh->fh_Type, ACTION_VTCON_ENV, fh->fh_Arg1, (LONG)buf, sizeof(buf) - 2, 0, 0)) {
        refused = 1;
        return;
    }
    while (n < (long)sizeof(buf) - 1 && buf[n])
        n += (long)strlen(buf + n) + 1;
    if (n == env_sent_len && !memcmp(buf, env_sent, n))
        return;
    memcpy(env_sent, buf, n);
    env_sent_len = n;
    sh_apply_env(sh, buf, n);
}

/* The console's line history (history, V88): the vtcon handler owns the list (ACTION_VTCON_HISTORY in
 * vtcon_packets.h). A console that does not know the packet answers ERROR_ACTION_NOT_KNOWN to the first
 * COUNT, and the shell keeps its own list (sh_exec.c) and never asks again. */
static long os_hist(void *os, int op, long arg, char *buf, long max)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(Input());
    LONG r;
    (void)os;
    if (!fh || !fh->fh_Type)
        return -1;
    r = DoPkt(fh->fh_Type, ACTION_VTCON_HISTORY, fh->fh_Arg1, op, arg, (LONG)buf, max);
    if (op == SH_HIST_COUNT && r == DOSFALSE && IoErr() == ERROR_ACTION_NOT_KNOWN)
        return -1;
    if (op == SH_HIST_DEL || op == SH_HIST_ADD || op == SH_HIST_CLEAR || op == SH_HIST_CONFIG)
        return r ? 1 : 0;
    return r;
}

/* ---- the prompt and the main loop ----------------------------------------------- */

/* On a vtcon console (term_marks) vsh tells the terminal where it is and
 * where its prompts are, as fish and VTE's vte.sh do: OSC 7 with the
 * directory (a new tab starts there), OSC 133 A/B around the prompt, C
 * before a command runs, D with its status after -- Right Amiga + Shift +
 * Up / Down then jump between prompts in the scrollback. */
static int term_marks;
static int command_ran;

static void put_str(const char *s)
{
    Write(Output(), (APTR)s, (LONG)strlen(s));
}

static void prompt(sh_shell *sh, int more)
{
    const char *ps;
    char *text;
    if (!more)
        sh_prompt_command(sh);
    ps = sh_get(&sh->ctx, more ? "PS2" : "PS1");
    text = sh_prompt(sh, ps ? ps : more ? "> " : "%F{cyan}%~%f %# ");
    if (term_marks && !more) {
        char b[600];
        char *dir = os_cwd(0);
        const char *host = sh_get(&sh->ctx, "HOSTNAME");
        if (command_ran) {
            char st[24];
            long v = sh->ctx.status, k = 0;
            char d[12];
            int m = 0;
            strcpy(st, "\033]133;D;");
            k = (long)strlen(st);
            do {
                d[m++] = (char)('0' + (v < 0 ? 0 : v) % 10);
                v /= 10;
            } while (v > 0 && m < 10);
            while (m)
                st[k++] = d[--m];
            st[k++] = 7;
            st[k] = 0;
            put_str(st);
            command_ran = 0;
        }
        if (dir && dir[0] && termurl_osc7(dir, host ? host : "", b, sizeof(b)))
            put_str(b);
        free(dir);
        put_str("\033]133;A\007");
    }
    if (text) {
        Write(Output(), text, (LONG)strlen(text));
        free(text);
    }
    if (term_marks && !more)
        put_str("\033]133;B\007");
}

/* $HOME as the name the current directory is shown by ("SYS:" is
 * "System:"), so the prompt can show it as ~ */
static void canonical_home(sh_shell *sh)
{
    const char *home = sh_get(&sh->ctx, "HOME");
    char name[256];
    BPTR lock = home ? Lock((STRPTR)home, SHARED_LOCK) : 0;
    if (!lock)
        return;
    if (NameFromLock(lock, (STRPTR)name, sizeof(name)) && strcmp(name, home))
        sh_set(&sh->ctx, "HOME", name);
    UnLock(lock);
}

/* Run a startup file if it is there. */
static void source_if(sh_shell *sh, const char *path)
{
    BPTR lock = Lock((STRPTR)path, SHARED_LOCK);
    char *cmd;
    if (!lock)
        return;
    UnLock(lock);
    cmd = (char *)malloc(strlen(path) + 12);
    if (!cmd)
        return;
    strcpy(cmd, "source '");
    strcat(cmd, path);
    strcat(cmd, "'");
    sh_run_text(sh, cmd, 0);
    free(cmd);
}

/* An environment variable of the system (ENV:) into the shell, if set. */
static void import_var(sh_shell *sh, const char *name)
{
    char v[256];
    if (GetVar((STRPTR)name, (STRPTR)v, sizeof(v), GVF_GLOBAL_ONLY) > 0)
        sh_set(&sh->ctx, name, v);
}

/* The process's local variables: the environment its parent gave it
 * (a Shell's Set variables; a Unix program -- screen, tcsh -- passing its
 * environment to vsh through ixemul's execve). Into the shell, exported,
 * as a Unix shell takes its environment. */
static void import_locals(sh_shell *sh)
{
    struct Process *me = (struct Process *)FindTask(0);
    struct LocalVar *lv;
    char v[512];
    for (lv = (struct LocalVar *)me->pr_LocalVars.mlh_Head; lv->lv_Node.ln_Succ;
         lv = (struct LocalVar *)lv->lv_Node.ln_Succ) {
        if (lv->lv_Node.ln_Type != LV_VAR || (lv->lv_Flags & GVF_BINARY_VAR) || !lv->lv_Node.ln_Name)
            continue;
        if (GetVar((STRPTR)lv->lv_Node.ln_Name, (STRPTR)v, sizeof(v), GVF_LOCAL_ONLY) < 0)
            continue;
        sh_set(&sh->ctx, lv->lv_Node.ln_Name, v);
        sh_export(&sh->ctx, lv->lv_Node.ln_Name);
    }
}

static int vsh_main(int argc, char **argv)
{
    static sh_shell sh;
    char line[1024];
    char *text = 0;
    long len = 0;
    const char *command = 0, *script = 0;
    int norc = 0, login = 0;
    static vproc vp;
    (void)version;
    (void)stack_cookie;
    upassign_ensure(); /* vshrc's PATH names /UP-Term/bin: the assign first */
    vp.sh = &sh;
    vp.port = CreateMsgPort();
    if (!vp.port)
        return 20;
    sh_shell_init(&sh);
    sh.os.open = os_open;
    sh.os.close = os_close;
    sh.os.pipe = os_pipe;
    sh.os.run = os_run;
    sh.os.wait = os_wait;
    sh.os.done = os_done;
    sh.os.cont = os_cont;
    sh.os.isatty = os_isatty;
    sh.os.stack = os_stack;
    sh.os.umask = os_umask;
    sh.os.spawn = os_spawn;
    sh.os.read = os_read;
    sh.os.ready = os_ready;
    sh.os.now = os_now;
    sh.os.remove = os_remove;
    sh.os.tmpdir = os_tmpdir;
    sh.os.echo = os_echo;
    sh.os.interrupted = os_interrupted;
    sh.os.signal = os_signal;
    sh.os.write = os_write;
    sh.os.read_line = os_read_line;
    sh.os.hist = os_hist;
    sh.os.chdir = os_chdir;
    sh.os.cwd = os_cwd;
    sh.os.realpath = os_realpath;
    sh.os.stat = os_stat;
    sh.os.data = &vp;
    sh.stack_limit = stack_limit_here();
    sh.ctx.listdir = list_dir;
    sh.ctx.nocase = 1;
    sh.ctx.pid = (long)FindTask(0);
    sh.io.in = (sh_fh)Input();
    sh.io.out = (sh_fh)Output();
    /* standard error is the process's error stream (RunCommand/SystemTags set it with SYS_Error),
     * Output() only when there is none: `vsh x 2>f` and a 2>f on a group around vsh must not
     * find the shell's own messages in stdout */
    sh.io.err = (sh_fh)(((struct Process *)FindTask(0))->pr_CES ? ((struct Process *)FindTask(0))->pr_CES : Output());
    sh.io.owned = 0;
    sh_set(&sh.ctx, "HOME", "SYS:");
    import_var(&sh, "HOME");
    import_var(&sh, "USER");
    import_var(&sh, "HOST");
    import_var(&sh, "HOSTNAME");
    import_locals(&sh);
    {
        /* a parent's UMASK (octal) is the shell's own mask too */
        char um[16];
        if (GetVar((STRPTR)"UMASK", (STRPTR)um, sizeof(um), GVF_LOCAL_ONLY) > 0) {
            const char *p = um;
            int m = 0;
            while (*p >= '0' && *p <= '7')
                m = m * 8 + (*p++ - '0');
            if (!*p && p != um && m <= 0777)
                sh.umask = m;
        }
    }
    {
        /* on a vtcon console (it answers TCGETA) the programs vsh runs get
         * its terminal type -- a global TERM is another console's -- and
         * its termcap unless one is set. Not globally: a ROM CON: window
         * cannot show vtcon's sequences (dist/Install). */
        vt_termios t;
        struct FileHandle *fh = (struct FileHandle *)BADDR(Input());
        if (fh && fh->fh_Type && DoPkt(fh->fh_Type, ACTION_VTCON_TCGETA, fh->fh_Arg1, (LONG)&t, 0, 0, 0)) {
            BPTR lock;
            import_var(&sh, "TERMCAP");
            /* a TERM the parent set is the terminal's (screen sets
             * "screen" for its windows, which are PTY: -- vtcon consoles
             * too); a global one is another console's */
            {
                char lt[64];
                if (GetVar((STRPTR)"TERM", (STRPTR)lt, sizeof(lt), GVF_LOCAL_ONLY) <= 0)
                    sh_set(&sh.ctx, "TERM", "vtcon");
            }
            sh_export(&sh.ctx, "TERM");
            term_marks = 1; /* OSC 7 and 133 around the prompts (prompt()) */
            if (!sh_get(&sh.ctx, "TERMCAP") && (lock = Lock((STRPTR)"ENV:up-term/termcap.vtcon", SHARED_LOCK))) {
                UnLock(lock);
                sh_set(&sh.ctx, "TERMCAP", "/ENV/up-term/termcap.vtcon");
            }
            if (sh_get(&sh.ctx, "TERMCAP"))
                sh_export(&sh.ctx, "TERMCAP");
            sync_env(&sh); /* the window's /term and /colors beat the default above */
        }
    }
    /* vsh -c COMMAND [NAME [ARG ...]] and vsh FILE [ARG ...], as sh: run
     * it and end with its status, no prompts ($- has no "i"). screen's
     * printcmd, vim's :! and other ports run $SHELL -c. */
    {
        sh_invoke_info inf;
        sh_invoke(&sh, argc, argv, IsInteractive(Input()), &inf);
        command = inf.command;
        script = inf.script;
        norc = inf.norc;
        login = inf.login;
        if (inf.exit_now) {
            command = "true";
            sh.exiting = 1;
            sh.exit_status = inf.status;
        }
    }
    /* AmigaDOS leaves a command's argument line in its input buffer (for
     * ReadArgs); unread, vsh took it as its first, empty, command line
     * and printed a second prompt (rig, 2026-09-29) */
    Flush(Input());
    /* the system's startup file (ENVARC:vsh/vshrc, copied to ENV: at boot),
     * then the user's */
    if (login && !norc)
        sh_startup_login(&sh);
    if (!norc)
        source_if(&sh, "ENV:vsh/vshrc");
    canonical_home(&sh);
    {
        const char *home = sh_get(&sh.ctx, "HOME");
        char *p = (char *)malloc(strlen(home) + 10);
        if (p) {
            strcpy(p, home);
            if (*p && p[strlen(p) - 1] != ':' && p[strlen(p) - 1] != '/')
                strcat(p, "/");
            strcat(p, ".vshrc");
            if (!norc)
                source_if(&sh, p);
            free(p);
        }
        canonical_home(&sh);
    }
    sh_startup_env(&sh);
    if (!command && !script && IsInteractive(Input())) {
        /* bash's HISTFILE; the vtcon console loads and appends this very file itself */
        if (!sh_get(&sh.ctx, "HISTFILE"))
            sh_set(&sh.ctx, "HISTFILE", "ENVARC:vtcon.history");
        sh_hist_load(&sh);
    }
    if (command)
        sh_run_text(&sh, command, 0);
    else if (script)
        sh_run_script(&sh); /* the arguments stay $1 ... */
    for (;;) {
        long n;
        int incomplete = 0;
        if (sh.exiting || command || script)
            break;
        if (!text) {
            sh_notify(&sh);
            send_words(&sh);
            sync_env(&sh);
            sh_hist_config(&sh);
        }
        if (IsInteractive(Input())) /* a script or a pipe gets no prompts */
            prompt(&sh, text != 0);
        else if (!text && (sh.opts & SO_INTERACTIVE))
            sh_prompt_command(&sh); /* vsh -i reading a pipe: PROMPT_COMMAND still runs, as in bash */
        if (IsInteractive(Input()) ? !FGets(Input(), (STRPTR)line, sizeof(line))
                                   : os_read_line(0, (sh_fh)Input(), line, sizeof(line)) < 0)
            break; /* a file or a pipe: line by line, as the commands that share it must see it */
        if (!text && (sh.opts & SO_INTERACTIVE) && (sh.opts & SO_HISTEXP)) {
            /* the first line of a command: !! !$ ^a^b ... (bash shows the expanded line on its
             * error stream, as vsh_host does: on Output() it was in the command's output) */
            char *ex;
            int po, r = sh_hist_expand(&sh, line, &ex, &po);
            if (r < 0)
                continue;
            if (r > 0) {
                long m = (long)strlen(ex);
                Write((BPTR)sh.io.err, ex, m);
                sh_hist_replace(&sh, ex, po);
                if (po) {
                    free(ex);
                    continue;
                }
                if (m >= (long)sizeof(line))
                    m = (long)sizeof(line) - 1;
                memcpy(line, ex, (size_t)m);
                line[m] = 0;
                free(ex);
            }
        }
        n = (long)strlen(line);
        {
            char *t = (char *)realloc(text, len + n + 1);
            if (!t)
                break;
            text = t;
            memcpy(text + len, line, n + 1);
            len += n;
        }
        SetSignal(0, SIGBREAKF_CTRL_C); /* a Ctrl-C at the prompt is not for this line */
        if (term_marks && !command_ran && text[strspn(text, " \t\n")]) {
            put_str("\033]133;C\007"); /* a command's output starts here */
            command_ran = 1;
        }
        sh.ps0_on = (sh.opts & SO_INTERACTIVE) != 0;
        sh_run_text(&sh, text, &incomplete);
        sh.ps0_on = 0;
        if (incomplete)
            continue;
        free(text);
        text = 0;
        len = 0;
    }
    free(text);
    sh_exit_trap(&sh); /* trap ... EXIT: end of input, exit, a script's end */
    sh_hist_save(&sh);
    {
        long st = sh.exiting ? sh.exit_status : sh.ctx.status;
        TR("exit free", st, 0);
        sh_shell_free(&sh);
        TR("exit port", 0, 0);
        DeleteMsgPort(vp.port);
        TR("exit return", 0, 0);
        return (int)st;
    }
}

/* The interpreter recurses (a function in a loop in a pipeline ...); a
 * Shell's default stack is 4 KB on 3.1, so vsh runs on a 64 KB stack of
 * its own when it was given less, as its subshell processes do. Only
 * statics across the swap: locals of this frame live on the old stack. */
#define VSH_STACK 327680
static struct StackSwapStruct swap;
static int g_argc, g_rc;
static char **g_argv;

/* The call between the two StackSwaps takes no arguments and goes
 * through a volatile pointer: vbcc popped vsh_main's arguments only after
 * the second swap -- 8 bytes off the old stack, so main returned through a
 * wrong address (#8000000B when a vsh started by vsh ended: vsh's runner
 * gives it 16000 bytes, so it swaps). Nothing may be left to pop, and the
 * call must not be inlined into main. */
static int vsh_entry(void)
{
    return vsh_main(g_argc, g_argv);
}

static int (*volatile vsh_entry_ptr)(void) = vsh_entry;

struct Library *IconBase;

#define WB_WINDOW "XCON:0/20/640/400/UP-Term/CLOSE"

/* A copy of the Workbench process's command path, the user's (LoadWB
 * took it from the startup Shell), for the Shell vsh starts from
 * Workbench (NP_Path: that process frees it). 0: none. */
static BPTR wb_path(void)
{
    struct Process *wbp;
    struct CommandLineInterface *cli;
    BPTR *src, head = 0, *tail = &head;
    Forbid();
    wbp = (struct Process *)FindTask((STRPTR)"Workbench");
    cli = wbp && wbp->pr_Task.tc_Node.ln_Type == NT_PROCESS
              ? (struct CommandLineInterface *)BADDR(wbp->pr_CLI) : 0;
    for (src = cli ? (BPTR *)BADDR(cli->cli_CommandDir) : 0; src; src = (BPTR *)BADDR(src[0])) {
        BPTR *n = (BPTR *)AllocVec(2 * sizeof(BPTR), MEMF_PUBLIC | MEMF_CLEAR);
        if (!n)
            break;
        n[1] = DupLock(src[1]);
        *tail = MKBADDR(n);
        tail = &n[0];
    }
    Permit();
    return head;
}

/* No console to print to (Workbench): the XCON: window did not open because
 * the handler is not mounted. Say so in a requester. Only that failure: any
 * other one (no memory, ...) keeps the plain return code. */
struct IntuitionBase *IntuitionBase;

static void wb_no_xcon(LONG err)
{
    if (err != ERROR_DEVICE_NOT_MOUNTED && err != ERROR_OBJECT_NOT_FOUND)
        return;
    if ((IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36)) != 0) {
        struct EasyStruct es;
        es.es_StructSize = sizeof(es);
        es.es_Flags = 0;
        es.es_Title = (UBYTE *)"UP-Term";
        es.es_TextFormat = (UBYTE *)"%s";
        es.es_GadgetFormat = (UBYTE *)"Cancel";
        EasyRequest(0, &es, 0, (LONG)bmsg_xcon_missing());
        CloseLibrary((struct Library *)IntuitionBase);
    }
}

/* Started from Workbench (the UP-Term icon; P8): no console and no CLI.
 * Open the window the icon names (tooltype WINDOW=, from the project icon
 * when there is one) and run vsh in it as a Shell process of its own --
 * a CLI for the path and the stack, the window as its console -- then
 * end: the window belongs to that vsh (it closes when vsh exits). */
static int wb_start(struct WBStartup *wb)
{
    static char spec[256], self[256], cmd[280];
    struct WBArg *icon = &wb->sm_ArgList[wb->sm_NumArgs > 1 ? 1 : 0];
    struct Process *me = (struct Process *)FindTask(0);
    BPTR win, out;
    strcpy(spec, WB_WINDOW);
    if ((IconBase = OpenLibrary((STRPTR)"icon.library", 36)) != 0) {
        BPTR old = CurrentDir(icon->wa_Lock);
        struct DiskObject *d = GetDiskObject((STRPTR)icon->wa_Name);
        CurrentDir(old);
        if (d) {
            char *w = (char *)FindToolType((STRPTR *)d->do_ToolTypes, (STRPTR)"WINDOW");
            if (w && *w) {
                strncpy(spec, w, sizeof(spec) - 1);
                spec[sizeof(spec) - 1] = 0;
            }
            FreeDiskObject(d);
        }
        CloseLibrary(IconBase);
    }
    /* this program's own file, to run it again in the new Shell */
    if (!NameFromLock(wb->sm_ArgList[0].wa_Lock, (STRPTR)self, sizeof(self)) ||
        !AddPart((STRPTR)self, (STRPTR)wb->sm_ArgList[0].wa_Name, sizeof(self)))
        strcpy(self, "C:vsh");
    win = Open((STRPTR)spec, MODE_NEWFILE);
    if (!win) {
        wb_no_xcon(IoErr());
        return 20;
    }
    /* a second handle on the same window (another Open of XCON: would be
     * another window): "*" with the window as our console */
    me->pr_ConsoleTask = ((struct FileHandle *)BADDR(win))->fh_Type;
    out = Open((STRPTR)"*", MODE_NEWFILE);
    if (!out) {
        Close(win);
        return 20;
    }
    strcpy(cmd, "\"");
    strcat(cmd, self);
    strcat(cmd, "\"");
    {
        /* it starts at home ($HOME, else SYS:), not in the drawer vsh is in,
         * with the user's command path */
        char home[256];
        BPTR dir, old = 0, path = wb_path();
        LONG rc;
        if (GetVar((STRPTR)"HOME", (STRPTR)home, sizeof(home), GVF_GLOBAL_ONLY) <= 0)
            strcpy(home, "SYS:");
        if ((dir = Lock((STRPTR)home, SHARED_LOCK)) != 0)
            old = CurrentDir(dir);
        rc = SystemTags((STRPTR)cmd, SYS_Input, win, SYS_Output, out, SYS_UserShell, TRUE,
                        SYS_Asynch, TRUE, NP_ConsoleTask, (ULONG)me->pr_ConsoleTask,
                        path ? NP_Path : TAG_IGNORE, path, TAG_END);
        if (dir)
            UnLock(CurrentDir(old));
        if (rc == -1) {
            /* not started: the path and the streams are still ours */
            while (path) {
                BPTR *n = (BPTR *)BADDR(path);
                path = n[0];
                UnLock(n[1]);
                FreeVec(n);
            }
            Close(out);
            Close(win);
            return 20;
        }
    }
    me->pr_ConsoleTask = 0;
    return 0;
}

int main(int argc, char **argv)
{
    struct Task *me = FindTask(0);
    APTR stack;
    if (argc == 0)
        return wb_start((struct WBStartup *)argv);
    g_argc = argc;
    g_argv = argv;
    if ((ULONG)me->tc_SPUpper - (ULONG)me->tc_SPLower >= VSH_STACK)
        return vsh_main(argc, argv);
    stack = AllocVec(VSH_STACK, MEMF_ANY);
    if (!stack)
        return vsh_main(argc, argv);
    swap.stk_Lower = stack;
    swap.stk_Upper = (ULONG)stack + VSH_STACK;
    swap.stk_Pointer = (APTR)swap.stk_Upper;
    StackSwap(&swap);
    g_rc = vsh_entry_ptr();
    StackSwap(&swap);
    FreeVec(stack);
    return g_rc;
}
