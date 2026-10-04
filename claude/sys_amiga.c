/* sys_amiga -- sys.h on AmigaDOS for C:Claude.
 *
 * run_command: the command line goes into a script file in T: and runs as
 * "vsh <script>" through SystemTags, asynchronously, with no input (NIL:)
 * and its output to a second T: file. NP_ExitCode signals this task when
 * the command's process ends and hands over its return code. While it
 * runs, a Ctrl+C or the time limit sends the process a break (vsh passes
 * it to the command) and waits for it to end. Writing the command to a
 * file means no quoting rules stand between Claude's text and vsh. */
#include <string.h>
#include <stdlib.h>
#include <exec/types.h>
#include <exec/tasks.h>
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

static struct Task *parent;
static BYTE done_sig = -1;
static volatile LONG child_rc;

/* NP_ExitCode: runs in the command's process as it ends; the return code
 * in d0, the exit data in d1 */
static void child_exit(__reg("d0") LONG rc, __reg("d1") LONG data)
{
    (void)data;
    child_rc = rc;
    Signal(parent, 1UL << done_sig);
}

static void send_break(const char *name)
{
    struct Task *t;
    Forbid();
    t = FindTask((STRPTR)name);
    if (t)
        Signal(t, SIGBREAKF_CTRL_C);
    Permit();
}

static int a_run(void *u, const char *cmd, int timeout_s, char *out, long cap, long *outn, long *rc)
{
    sys_amiga *s = (sys_amiga *)u;
    char script[48], output[48], line[64], name[40], num[12];
    BPTR in, of;
    LONG r;
    long waited = 0, limit = (long)timeout_s * 50, n = 0;
    int result = 0, ended = 0, broke = 0;
    ULONG mask;
    parent = FindTask(0);
    cl_ltoa((long)parent, num);
    cl_copy(script, "T:Claude-command-", sizeof(script));
    cl_cat(script, num, sizeof(script));
    cl_copy(output, "T:Claude-output-", sizeof(output));
    cl_cat(output, num, sizeof(output));
    cl_copy(name, "Claude command ", sizeof(name));
    cl_cat(name, num, sizeof(name));
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
    done_sig = AllocSignal(-1);
    if (done_sig < 0) {
        cl_copy(s->err, "no free signal", sizeof(s->err));
        DeleteFile((STRPTR)script);
        return -1;
    }
    SetSignal(0, 1UL << done_sig);
    in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    of = Open((STRPTR)output, MODE_NEWFILE);
    if (!in || !of) {
        set_err(s, "cannot open the command's input or output");
        if (in)
            Close(in);
        if (of)
            Close(of);
        FreeSignal(done_sig);
        DeleteFile((STRPTR)script);
        return -1;
    }
    cl_copy(line, "vsh ", sizeof(line));
    cl_cat(line, script, sizeof(line));
    child_rc = 0;
    r = SystemTags((STRPTR)line, SYS_Input, in, SYS_Output, of, SYS_Asynch, TRUE, SYS_UserShell, TRUE,
                   NP_Name, (ULONG)name, NP_ExitCode, (ULONG)child_exit, NP_ExitData, 0, TAG_END);
    if (r == -1) {
        set_err(s, "the command did not start");
        Close(in);
        Close(of);
        FreeSignal(done_sig);
        DeleteFile((STRPTR)script);
        DeleteFile((STRPTR)output);
        return -1;
    }
    /* SystemTags closes in and of when the process ends */
    for (;;) {
        mask = SetSignal(0, 0);
        if (mask & (1UL << done_sig)) {
            ended = 1;
            break;
        }
        if (!broke && (mask & SIGBREAKF_CTRL_C)) {
            SetSignal(0, SIGBREAKF_CTRL_C);
            send_break(name);
            broke = SYS_BREAK;
            limit = waited + 500;   /* ten more seconds to end */
        } else if (!broke && waited >= limit) {
            send_break(name);
            broke = SYS_TIMEOUT;
            limit = waited + 500;
        } else if (broke && waited >= limit)
            break;                  /* it does not end: leave it */
        Delay(5);
        waited += 5;
    }
    SetSignal(0, 1UL << done_sig);
    FreeSignal(done_sig);
    done_sig = -1;
    *rc = ended ? child_rc : -1;
    result = broke;
    if (ended) {
        char *b = 0;
        if (a_read(u, output, cap, &b, &n) == 0) {
            memcpy(out, b, (size_t)n);
            free(b);
        } else if (a_read(u, output, 0x7fffffffL, &b, &n) == 0) {
            n = n > cap ? cap : n;  /* longer than the cap: its start */
            memcpy(out, b, (size_t)n);
            free(b);
        } else
            n = 0;
        DeleteFile((STRPTR)output);
        DeleteFile((STRPTR)script);
    } else
        cl_copy(s->err, "the command did not end after a break", sizeof(s->err));
    *outn = n;
    return result;
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
