/* sys_amiga -- sys.h on AmigaDOS for C:Claude.
 *
 * run_command: the command line goes into a script file in T: and runs as
 * "vsh <script>" with no input (NIL:) and its output to a second T: file,
 * from a runner process the way vsh runs its own jobs (a synchronous
 * SystemTags with the Shell process named; the runner replies with the
 * return code). While it runs, a Ctrl+C or the time limit sends that
 * process a break (vsh passes it to the command) and waits for it to end.
 * Writing the command to a file means no quoting rules stand between
 * Claude's text and vsh. */
#include <string.h>
#include <stdlib.h>
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "sys_amiga.h"
#include "util.h"

static void set_err(sys_amiga *s, const char *what)
{
    char f[120];
    LONG e = IoErr();
    cl_copy(s->err, what, sizeof(s->err));
    if (e && Fault(e, 0, (STRPTR)f, sizeof(f))) {
        cl_cat(s->err, ": ", sizeof(s->err));
        cl_cat(s->err, f, sizeof(s->err));
    }
}

static int a_read(void *u, const char *path, long max, char **out, long *n)
{
    sys_amiga *s = (sys_amiga *)u;
    BPTR f = Open((STRPTR)path, MODE_OLDFILE);
    struct FileInfoBlock *fib;
    long len = -1;
    char *b;
    if (!f) {
        set_err(s, path);
        return -1;
    }
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib && ExamineFH(f, fib))
        len = fib->fib_Size;
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    if (len < 0) {
        set_err(s, path);
        Close(f);
        return -1;
    }
    if (len > max) {
        Close(f);
        return SYS_TOO_BIG;
    }
    b = (char *)malloc((size_t)len + 1);
    if (!b || Read(f, b, len) != len) {
        set_err(s, b ? path : "out of memory");
        free(b);
        Close(f);
        return -1;
    }
    Close(f);
    b[len] = 0;
    *out = b;
    *n = len;
    return 0;
}

static int a_write(void *u, const char *path, const char *str, long n)
{
    sys_amiga *s = (sys_amiga *)u;
    BPTR f = Open((STRPTR)path, MODE_NEWFILE);
    LONG w;
    if (!f) {
        set_err(s, path);
        return -1;
    }
    w = n ? Write(f, (APTR)str, n) : 0;
    if (w != n) {
        set_err(s, path);
        Close(f);
        return -1;
    }
    return Close(f) ? 0 : -1;
}

static int a_list(void *u, const char *path, cl_dir_fn fn, void *c)
{
    sys_amiga *s = (sys_amiga *)u;
    BPTR l = Lock((STRPTR)path, SHARED_LOCK);
    struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    int rc = -1;
    if (l && fib && Examine(l, fib) && fib->fib_DirEntryType > 0) {
        rc = 0;
        while (ExNext(l, fib)) {
            cl_dirent e;
            memset(&e, 0, sizeof(e));
            cl_copy(e.name, (const char *)fib->fib_FileName, sizeof(e.name));
            e.dir = fib->fib_DirEntryType > 0;
            e.size = fib->fib_Size;
            if (fn(c, &e))
                break;
        }
    } else
        set_err(s, path);
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    if (l)
        UnLock(l);
    return rc;
}

static int a_kind(void *u, const char *path)
{
    BPTR l = Lock((STRPTR)path, SHARED_LOCK);
    struct FileInfoBlock *fib;
    int k = 0;
    (void)u;
    if (!l)
        return 0;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib && Examine(l, fib))
        k = fib->fib_DirEntryType > 0 ? 2 : 1;
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    UnLock(l);
    return k;
}

static int a_canon(void *u, const char *path, char *out, long cap)
{
    BPTR l = Lock((STRPTR)path, SHARED_LOCK);
    int ok;
    (void)u;
    if (!l)
        return -1;
    ok = NameFromLock(l, (STRPTR)out, cap) != 0;
    UnLock(l);
    return ok ? 0 : -1;
}

/* ---- run_command ---- */

/* vsh's job pattern (shell/vsh.c runner/job_signal, tested on the rig): a
 * runner process makes a synchronous SystemTags call with the command's
 * Shell process named, so a break can find it, and replies when it is
 * back with the return code. */
typedef struct runjob {
    struct Message msg;
    char line[64];              /* "vsh T:Claude-command-..." */
    char child[40];             /* the name of the Shell process */
    BPTR in, out;
    LONG rc;
    volatile int done;
} runjob;

static void runner(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    runjob *j;
    WaitPort(&me->pr_MsgPort);
    j = (runjob *)GetMsg(&me->pr_MsgPort);
    j->rc = SystemTags((STRPTR)j->line, SYS_Input, j->in, SYS_Output, j->out, SYS_UserShell, TRUE,
                       NP_Name, (ULONG)j->child, TAG_END);
    Forbid();                       /* the reply and our end, before Claude can free anything */
    j->done = 1;
    ReplyMsg(&j->msg);
}

static void send_break(runjob *j)
{
    struct Task *t;
    Forbid();
    if (!j->done && (t = FindTask((STRPTR)j->child)) != 0)
        Signal(t, SIGBREAKF_CTRL_C);
    Permit();
}

static int a_run(void *u, const char *cmd, int timeout_s, char *out, long cap, long *outn, long *rc)
{
    sys_amiga *s = (sys_amiga *)u;
    char script[48], output[48], num[12];
    struct MsgPort *port;
    struct Process *p;
    runjob *j;
    long waited = 0, limit = (long)timeout_s * 50, n = 0;
    int broke = 0, ended = 0;
    cl_ltoa((long)FindTask(0), num);
    cl_copy(script, "T:Claude-command-", sizeof(script));
    cl_cat(script, num, sizeof(script));
    cl_copy(output, "T:Claude-output-", sizeof(output));
    cl_cat(output, num, sizeof(output));
    *outn = 0;
    *rc = -1;
    {
        /* the script: the command line and a newline */
        long cl = (long)strlen(cmd);
        char *text = (char *)malloc((size_t)cl + 2);
        int bad;
        if (!text) {
            cl_copy(s->err, "out of memory", sizeof(s->err));
            return -1;
        }
        memcpy(text, cmd, (size_t)cl);
        text[cl] = '\n';
        bad = a_write(u, script, text, cl + 1);
        free(text);
        if (bad)
            return -1;
    }
    /* the job and its port outlive this call when a command will not end */
    j = (runjob *)AllocVec(sizeof(runjob), MEMF_PUBLIC | MEMF_CLEAR);
    port = CreateMsgPort();
    if (!j || !port) {
        cl_copy(s->err, "out of memory", sizeof(s->err));
        goto fail;
    }
    cl_copy(j->line, "vsh ", sizeof(j->line));
    cl_cat(j->line, script, sizeof(j->line));
    cl_copy(j->child, "Claude command ", sizeof(j->child));
    cl_cat(j->child, num, sizeof(j->child));
    j->in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    j->out = Open((STRPTR)output, MODE_NEWFILE);
    if (!j->in || !j->out) {
        set_err(s, "cannot open the command's input or output");
        goto fail;
    }
    j->msg.mn_ReplyPort = port;
    j->msg.mn_Length = sizeof(runjob);
    p = CreateNewProcTags(NP_Entry, (ULONG)runner, NP_Name, (ULONG)"Claude runner", NP_StackSize, 8000,
                          NP_Cli, TRUE, TAG_END);
    if (!p) {
        set_err(s, "the command did not start");
        goto fail;
    }
    PutMsg(&p->pr_MsgPort, &j->msg);
    for (;;) {
        ULONG mask;
        if (GetMsg(port)) {
            ended = 1;
            break;
        }
        mask = SetSignal(0, 0);
        if (!broke && (mask & SIGBREAKF_CTRL_C)) {
            SetSignal(0, SIGBREAKF_CTRL_C);
            send_break(j);
            broke = SYS_BREAK;
            limit = waited + 500;   /* ten more seconds to end */
        } else if (!broke && waited >= limit) {
            send_break(j);
            broke = SYS_TIMEOUT;
            limit = waited + 500;
        } else if (broke && waited >= limit)
            break;                  /* it does not end: leave it running */
        Delay(5);
        waited += 5;
    }
    if (!ended) {
        /* the runner still owns j, its streams and the port: leave them */
        cl_copy(s->err, "the command did not end after a break; it was left running", sizeof(s->err));
        return -1;
    }
    *rc = j->rc;
    Close(j->in);
    Close(j->out);
    FreeVec(j);
    DeleteMsgPort(port);
    {
        char *b = 0;
        if (a_read(u, output, 0x7fffffffL, &b, &n) == 0) {
            if (n > cap)
                n = cap;            /* longer than the cap: its start */
            memcpy(out, b, (size_t)n);
            free(b);
        } else
            n = 0;
    }
    DeleteFile((STRPTR)output);
    DeleteFile((STRPTR)script);
    *outn = n;
    return broke;
fail:
    if (j) {
        if (j->in)
            Close(j->in);
        if (j->out)
            Close(j->out);
        FreeVec(j);
    }
    if (port)
        DeleteMsgPort(port);
    DeleteFile((STRPTR)script);
    DeleteFile((STRPTR)output);
    return -1;
}

static const char *a_err(void *u)
{
    return ((sys_amiga *)u)->err;
}

void sys_amiga_init(sys_amiga *s, cl_sys *sys)
{
    memset(s, 0, sizeof(*s));
    sys->u = s;
    sys->read = a_read;
    sys->write = a_write;
    sys->list = a_list;
    sys->kind = a_kind;
    sys->canon = a_canon;
    sys->run = a_run;
    sys->err = a_err;
}
