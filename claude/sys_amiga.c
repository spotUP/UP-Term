/* sys_amiga -- sys.h on AmigaDOS for C:Claude.
 *
 * Bash: the command line goes into a script file in T: and runs as
 * "vsh <script>" (or "Execute <script>" without C:vsh) with no input (NIL:)
 * and its output to a second T: file,
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
#include <dos/var.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "sys_amiga.h"
#include "util.h"
#include "../handler/clip.h"

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

/* a DateStamp in seconds since 1978 */
static long ds_seconds(const struct DateStamp *d)
{
    return d->ds_Days * 86400L + d->ds_Minute * 60L + d->ds_Tick / TICKS_PER_SECOND;
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
            e.mtime = ds_seconds(&fib->fib_Date);
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

/* ---- Bash ---- */

/* vsh's job pattern (shell/vsh.c runner/job_signal, tested on the rig): a
 * runner process makes a synchronous SystemTags call with the command's
 * Shell process named, so a break can find it, and replies when it is
 * back with the return code. */
typedef struct runjob {
    struct Message msg;
    char line[80];              /* "vsh T:Claude-command-..." or "Execute T:..." */
    char child[48];             /* the name of the Shell process */
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

/* vsh when it is installed (C:vsh, where the kit puts it), else the AmigaShell */
static int have_vsh(sys_amiga *s)
{
    if (!s->vsh_known) {
        BPTR l = Lock((STRPTR)"C:vsh", SHARED_LOCK);
        s->vsh = l != 0;
        if (l)
            UnLock(l);
        s->vsh_known = 1;
    }
    return s->vsh;
}

/* A command started: the script and output names, the job and its reply
 * port; the output file shared (MODE_READWRITE) so it can be read while
 * it grows. 0, -1. tag tells the files of several jobs apart. */
static int start(sys_amiga *s, const char *cmd, long tag, char *script, char *output, runjob **jp,
                 struct MsgPort **pp)
{
    char num[12], t2[12];
    struct MsgPort *port;
    struct Process *p;
    runjob *j;
    cl_ltoa((long)FindTask(0), num);
    cl_ltoa(tag, t2);
    cl_copy(script, "T:Claude-command-", 48);
    cl_cat(script, num, 48);
    cl_cat(script, "-", 48);
    cl_cat(script, t2, 48);
    cl_copy(output, "T:Claude-output-", 48);
    cl_cat(output, num, 48);
    cl_cat(output, "-", 48);
    cl_cat(output, t2, 48);
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
        bad = a_write(s, script, text, cl + 1);
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
    cl_copy(j->line, have_vsh(s) ? "vsh " : "Execute ", sizeof(j->line));
    cl_cat(j->line, script, sizeof(j->line));
    cl_copy(j->child, "Claude command ", sizeof(j->child));
    cl_cat(j->child, num, sizeof(j->child));
    cl_cat(j->child, "-", sizeof(j->child));
    cl_cat(j->child, t2, sizeof(j->child));
    DeleteFile((STRPTR)output);
    j->in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    j->out = Open((STRPTR)output, MODE_READWRITE);
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
    *jp = j;
    *pp = port;
    return 0;
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

/* an ended job's handles and memory */
static void finish(runjob *j, struct MsgPort *port)
{
    Close(j->in);
    Close(j->out);
    FreeVec(j);
    DeleteMsgPort(port);
}

/* up to cap bytes of a file from byte from: the count read */
static long read_from(const char *path, long from, char *out, long cap)
{
    BPTR f = Open((STRPTR)path, MODE_OLDFILE);
    long n = 0;
    if (!f)
        return 0;
    if (Seek(f, from, OFFSET_BEGINNING) >= 0) {
        n = Read(f, out, cap);
        if (n < 0)
            n = 0;
    }
    Close(f);
    return n;
}

static int a_run(void *u, const char *cmd, int timeout_s, char *out, long cap, long *outn, long *rc)
{
    sys_amiga *s = (sys_amiga *)u;
    char script[48], output[48];
    struct MsgPort *port;
    runjob *j;
    long waited = 0, limit = (long)timeout_s * 50;
    int broke = 0, ended = 0;
    *outn = 0;
    *rc = -1;
    if (start(s, cmd, 0, script, output, &j, &port))
        return -1;
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
    finish(j, port);
    *outn = read_from(output, 0, out, cap);   /* longer than the cap: its start */
    DeleteFile((STRPTR)output);
    DeleteFile((STRPTR)script);
    return broke;
}

/* ---- background jobs (Bash run_in_background) ---- */

static int a_bg_start(void *u, const char *cmd, long *job)
{
    sys_amiga *s = (sys_amiga *)u;
    int i;
    for (i = 0; i < SA_JOBS; i++)
        if (!s->jobs[i].used)
            break;
    if (i == SA_JOBS) {
        cl_copy(s->err, "too many background commands", sizeof(s->err));
        return -1;
    }
    if (start(s, cmd, i + 1, s->jobs[i].script, s->jobs[i].output, (runjob **)&s->jobs[i].job,
              (struct MsgPort **)&s->jobs[i].port))
        return -1;
    s->jobs[i].used = 1;
    s->jobs[i].ended = 0;
    *job = i;
    return 0;
}

static void poll_job(sa_job *b)
{
    if (!b->ended && GetMsg((struct MsgPort *)b->port)) {
        runjob *j = (runjob *)b->job;
        b->rc = j->rc;
        finish(j, (struct MsgPort *)b->port);
        b->job = 0;
        b->port = 0;
        b->ended = 1;
    }
}

static int a_bg_read(void *u, long job, long from, char *out, long cap, long *outn, int *running, long *rc)
{
    sys_amiga *s = (sys_amiga *)u;
    sa_job *b;
    if (job < 0 || job >= SA_JOBS || !s->jobs[job].used) {
        cl_copy(s->err, "no such command", sizeof(s->err));
        return -1;
    }
    b = &s->jobs[job];
    poll_job(b);
    *running = !b->ended;
    *rc = b->rc;
    *outn = read_from(b->output, from, out, cap);
    return 0;
}

static int a_bg_kill(void *u, long job)
{
    sys_amiga *s = (sys_amiga *)u;
    sa_job *b;
    if (job < 0 || job >= SA_JOBS || !s->jobs[job].used)
        return -1;
    b = &s->jobs[job];
    poll_job(b);
    if (!b->ended)
        send_break((runjob *)b->job);
    return 0;
}

static void a_bg_drop(void *u, long job)
{
    sys_amiga *s = (sys_amiga *)u;
    sa_job *b;
    if (job < 0 || job >= SA_JOBS || !s->jobs[job].used)
        return;
    b = &s->jobs[job];
    poll_job(b);
    if (!b->ended)
        return;                     /* the runner still owns it: left as it is */
    DeleteFile((STRPTR)b->output);
    DeleteFile((STRPTR)b->script);
    b->used = 0;
}

static long a_mtime(void *u, const char *path)
{
    BPTR l = Lock((STRPTR)path, SHARED_LOCK);
    struct FileInfoBlock *fib;
    long t = -1;
    (void)u;
    if (!l)
        return -1;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib && Examine(l, fib))
        t = ds_seconds(&fib->fib_Date);
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    UnLock(l);
    return t;
}

/* ---- A4 WP3: append, directories, variables, the clipboard, /doctor ---- */

static int a_append(void *u, const char *path, const char *str, long n)
{
    sys_amiga *s = (sys_amiga *)u;
    BPTR f = Open((STRPTR)path, MODE_READWRITE);
    LONG w;
    if (!f) {
        set_err(s, path);
        return -1;
    }
    Seek(f, 0, OFFSET_END);
    w = n ? Write(f, (APTR)str, n) : 0;
    if (w != n) {
        set_err(s, path);
        Close(f);
        return -1;
    }
    return Close(f) ? 0 : -1;
}

static int a_mkdir(void *u, const char *path)
{
    sys_amiga *s = (sys_amiga *)u;
    BPTR l;
    if (a_kind(u, path) == 2)
        return 0;
    l = CreateDir((STRPTR)path);
    if (!l) {
        set_err(s, path);
        return -1;
    }
    UnLock(l);
    return 0;
}

static int a_remove(void *u, const char *path)
{
    sys_amiga *s = (sys_amiga *)u;
    if (!DeleteFile((STRPTR)path)) {
        set_err(s, path);
        return -1;
    }
    return 0;
}

static long a_getenv(void *u, const char *name, char *out, long cap)
{
    LONG n = GetVar((STRPTR)name, (STRPTR)out, cap, 0);
    (void)u;
    return n < 0 ? -1 : (long)n;
}

/* a local variable: the commands started from here (vsh, hooks) inherit it */
static int a_setenv(void *u, const char *name, const char *value)
{
    (void)u;
    return SetVar((STRPTR)name, (STRPTR)value, -1, GVF_LOCAL_ONLY) ? 0 : -1;
}

static int a_clip(void *u, const char *str, long n)
{
    sys_amiga *s = (sys_amiga *)u;
    if (!clip_write(str, n)) {
        cl_copy(s->err, "the clipboard (clipboard.device unit 0) did not take the text", sizeof(s->err));
        return -1;
    }
    return 0;
}

static int lib_check(const char *name, long ver, char *out, long cap)
{
    struct Library *b = OpenLibrary((STRPTR)name, 0);
    char num[16];
    long v;
    if (!b) {
        cl_copy(out, name, cap);
        cl_cat(out, " is not there", cap);
        return 0;
    }
    cl_copy(out, name, cap);
    cl_cat(out, " ", cap);
    v = (long)b->lib_Version;
    cl_ltoa(v, num);
    cl_cat(out, num, cap);
    cl_cat(out, ".", cap);
    cl_ltoa((long)b->lib_Revision, num);
    cl_cat(out, num, cap);
    CloseLibrary(b);
    if (v < ver) {
        cl_cat(out, " (too old)", cap);
        return 0;
    }
    return 1;
}

static int a_info(void *u, const char *what, char *out, long cap)
{
    char num[16];
    (void)u;
    if (!strcmp(what, "os")) {
        cl_copy(out, "exec ", cap);
        cl_ltoa((long)SysBase->LibNode.lib_Version, num);
        cl_cat(out, num, cap);
        cl_cat(out, ".", cap);
        cl_ltoa((long)SysBase->LibNode.lib_Revision, num);
        cl_cat(out, num, cap);
        cl_cat(out, ", CPU ", cap);
        cl_cat(out, (SysBase->AttnFlags & AFF_68060) ? "68060" : (SysBase->AttnFlags & AFF_68040) ? "68040"
                    : (SysBase->AttnFlags & AFF_68030) ? "68030" : (SysBase->AttnFlags & AFF_68020) ? "68020"
                    : "68000", cap);
        return SysBase->LibNode.lib_Version >= 39 ? 1 : 0;
    }
    if (!strcmp(what, "bsdsocket"))
        return lib_check("bsdsocket.library", 4, out, cap);
    if (!strcmp(what, "amissl"))
        return lib_check("amisslmaster.library", 5, out, cap);
    if (!strcmp(what, "vsh")) {
        int k = a_kind(u, "C:vsh");
        /* without vsh the Bash tool runs commands in the AmigaShell (run_cmd's
         * fallback): a note, not an error */
        cl_copy(out, k == 1 ? "C:vsh is there" : "C:vsh is not there: commands run in the AmigaShell "
                                                 "(install the UP-Term kit for Unix-style commands)", cap);
        return k == 1 ? 1 : -1;
    }
    cl_copy(out, "unknown", cap);
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
    sys->mtime = a_mtime;
    sys->bg_start = a_bg_start;
    sys->bg_read = a_bg_read;
    sys->bg_kill = a_bg_kill;
    sys->bg_drop = a_bg_drop;
    sys->append = a_append;
    sys->mkdir = a_mkdir;
    sys->remove = a_remove;
    sys->getenv = a_getenv;
    sys->setenv = a_setenv;
    sys->clip = a_clip;
    sys->info = a_info;
}
