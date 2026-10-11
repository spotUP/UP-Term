/* sys_amiga -- sys.h on AmigaDOS for C:Claude.
 *
 * Bash: the command line goes into a script file in T: and runs as
 * "vsh <script>" (or "Execute <script>" without vsh) with no input (NIL:)
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
#include "datadir_amiga.h"
#include "util.h"
#include "../handler/clip.h"
extern struct DosLibrary *DOSBase;
#define UPASSIGN_DOS
#include "../config/upassign.h"

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

/* vsh when it is installed (UP-Term:bin/vsh, else C:vsh of an older install), else the AmigaShell */
static const char *have_vsh(sys_amiga *s)
{
    if (!s->vsh_known) {
        s->vsh = upassign_vsh();
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
        char *text = (char *)CL_ALLOC((size_t)cl + 2);
        int bad;
        if (!text) {
            cl_copy(s->dos.err, "out of memory", sizeof(s->dos.err));
            return -1;
        }
        memcpy(text, cmd, (size_t)cl);
        text[cl] = '\n';
        bad = dda_write(s, script, text, cl + 1);
        CL_FREE(text);
        if (bad)
            return -1;
    }
    /* the job and its port outlive this call when a command will not end */
    j = (runjob *)AllocVec(sizeof(runjob), MEMF_PUBLIC | MEMF_CLEAR);
    port = CreateMsgPort();
    if (!j || !port) {
        cl_copy(s->dos.err, "out of memory", sizeof(s->dos.err));
        goto fail;
    }
    /* the full name: the line is run with no assumption about C: or the path */
    if (have_vsh(s)) {
        cl_copy(j->line, s->vsh, sizeof(j->line));
        cl_cat(j->line, " ", sizeof(j->line));
    } else
        cl_copy(j->line, "Execute ", sizeof(j->line));
    cl_cat(j->line, script, sizeof(j->line));
    cl_copy(j->child, "Claude command ", sizeof(j->child));
    cl_cat(j->child, num, sizeof(j->child));
    cl_cat(j->child, "-", sizeof(j->child));
    cl_cat(j->child, t2, sizeof(j->child));
    DeleteFile((STRPTR)output);
    j->in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    j->out = Open((STRPTR)output, MODE_READWRITE);
    if (!j->in || !j->out) {
        dda_err(&s->dos, "cannot open the command's input or output");
        goto fail;
    }
    j->msg.mn_ReplyPort = port;
    j->msg.mn_Length = sizeof(runjob);
    p = CreateNewProcTags(NP_Entry, (ULONG)runner, NP_Name, (ULONG)"Claude runner", NP_StackSize, 8000,
                          NP_Cli, TRUE, TAG_END);
    if (!p) {
        dda_err(&s->dos, "the command did not start");
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
        cl_copy(s->dos.err, "the command did not end after a break; it was left running", sizeof(s->dos.err));
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
        cl_copy(s->dos.err, "too many background commands", sizeof(s->dos.err));
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
        cl_copy(s->dos.err, "no such command", sizeof(s->dos.err));
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
        t = dda_seconds(&fib->fib_Date);
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
        dda_err(&s->dos, path);
        return -1;
    }
    Seek(f, 0, OFFSET_END);
    w = n ? Write(f, (APTR)str, n) : 0;
    if (w != n) {
        dda_err(&s->dos, path);
        Close(f);
        return -1;
    }
    return Close(f) ? 0 : -1;
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
        cl_copy(s->dos.err, "the clipboard (clipboard.device unit 0) did not take the text", sizeof(s->dos.err));
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
        const char *vp = upassign_vsh();
        int k = vp ? 1 : 0;
        /* without vsh the Bash tool runs commands in the AmigaShell (run_cmd's
         * fallback): a note, not an error */
        cl_copy(out, k == 1 ? (!strcmp(vp, UPASSIGN_VSH_PRIMARY) ? "UP-Term:bin/vsh is there" : "C:vsh is there")
                         : "UP-Term:bin/vsh and C:vsh are not there: commands run in the AmigaShell "
                                                 "(install the UP-Term kit for Unix-style commands)", cap);
        return k == 1 ? 1 : -1;
    }
    cl_copy(out, "unknown", cap);
    return -1;
}

static const char *a_err(void *u)
{
    return ((sys_amiga *)u)->dos.err;
}

/* ---- A4 gaps 2: the clock, a wait, a job's output size and file, rename ---- */

/* DateStamp is the local time since 1978-01-01 (as the Workbench clock is set) */
static long a_now(void *u)
{
    struct DateStamp d;
    (void)u;
    DateStamp(&d);
    return dda_seconds(&d);
}

/* Delay in ticks (1/50 s), a Ctrl+C on this task looked at every tenth of a second */
static int a_pause(void *u, long ms)
{
    long ticks = (ms + 19) / 20;
    (void)u;
    while (ticks > 0) {
        long d = ticks > 5 ? 5 : ticks;
        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
            SetSignal(0, SIGBREAKF_CTRL_C);
            return 1;
        }
        Delay(d);
        ticks -= d;
    }
    if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
        SetSignal(0, SIGBREAKF_CTRL_C);
        return 1;
    }
    return 0;
}

static long a_bg_size(void *u, long job)
{
    sys_amiga *s = (sys_amiga *)u;
    BPTR l;
    struct FileInfoBlock *fib;
    long n = 0;
    if (job < 0 || job >= SA_JOBS || !s->jobs[job].used)
        return -1;
    l = Lock((STRPTR)s->jobs[job].output, SHARED_LOCK);
    if (!l)
        return 0;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib && Examine(l, fib))
        n = fib->fib_Size;
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    UnLock(l);
    return n;
}

static const char *a_bg_file(void *u, long job)
{
    sys_amiga *s = (sys_amiga *)u;
    return job >= 0 && job < SA_JOBS && s->jobs[job].used ? s->jobs[job].output : "";
}

/* ---- spawn2: a child on two PTY: pipe pairs (amiga-pi K1, ARCHITECTURE.md) ----
 * PIPE: holds a line until its writer closes (a JSON-RPC request would never
 * reach the server); PTY:<id>/w + PTY:<id>/r answers a Read with what it
 * holds and gives EOF when the writer closes. The child is a Shell process
 * (SystemTags, SYS_Asynch: it closes its ends when it ends) named so a
 * break can find it. Reads are ACTION_READ packets kept pending, so a read
 * can wait a given time. AmigaOS gives System() no separate error stream:
 * what the server writes to stderr goes with its output. */
typedef struct sa_child {
    BPTR in_w, out_r;
    struct MsgPort *port;
    struct StandardPacket *sp;
    int pending, eof;
    long have, off;
    char name[40];              /* the Shell process's name */
    char buf[4096];
} sa_child;

static int pty_mounted(void)
{
    struct DosList *dl = LockDosList(LDF_DEVICES | LDF_READ);
    int found = FindDosEntry(dl, (STRPTR)"PTY", LDF_DEVICES) != 0;
    UnLockDosList(LDF_DEVICES | LDF_READ);
    return found;
}

/* the local variables a server is started with (a_spawn2): the value each had before, put back after */
#define ENV_KEEP 16
typedef struct env_saved {
    char name[64];
    char val[256];
    int had;
} env_saved;
static env_saved env_old[ENV_KEEP];     /* spawn2 runs on one task at a time */

static void env_set(const char *const *env)
{
    const char *const *e;
    int n = 0;
    for (e = env; e && *e && n < ENV_KEEP; e++) {
        const char *eq = strchr(*e, '=');
        long k = eq ? (long)(eq - *e) : 0;
        env_saved *v = &env_old[n];
        if (k <= 0 || k >= (long)sizeof(v->name))
            continue;
        memcpy(v->name, *e, (size_t)k);
        v->name[k] = 0;
        v->had = GetVar((STRPTR)v->name, (STRPTR)v->val, sizeof(v->val), GVF_LOCAL_ONLY) >= 0;
        SetVar((STRPTR)v->name, (STRPTR)(eq + 1), -1, GVF_LOCAL_ONLY);
        n++;
    }
    if (n < ENV_KEEP)
        env_old[n].name[0] = 0;
}

static void env_restore(void)
{
    int n;
    for (n = 0; n < ENV_KEEP && env_old[n].name[0]; n++) {
        if (env_old[n].had)
            SetVar((STRPTR)env_old[n].name, (STRPTR)env_old[n].val, -1, GVF_LOCAL_ONLY);
        else
            DeleteVar((STRPTR)env_old[n].name, GVF_LOCAL_ONLY);
    }
}

static void read_start(sa_child *c)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(c->out_r);
    memset(c->sp, 0, sizeof(*c->sp));
    c->sp->sp_Msg.mn_Node.ln_Name = (char *)&c->sp->sp_Pkt;
    c->sp->sp_Msg.mn_ReplyPort = c->port;
    c->sp->sp_Pkt.dp_Link = &c->sp->sp_Msg;
    c->sp->sp_Pkt.dp_Port = c->port;
    c->sp->sp_Pkt.dp_Type = ACTION_READ;
    c->sp->sp_Pkt.dp_Arg1 = fh->fh_Arg1;
    c->sp->sp_Pkt.dp_Arg2 = (LONG)c->buf;
    c->sp->sp_Pkt.dp_Arg3 = sizeof(c->buf);
    PutMsg(fh->fh_Type, &c->sp->sp_Msg);
    c->pending = 1;
}

/* the pending read's answer within ms: 1 answered (have, eof set), 0 not yet */
static int read_wait(sa_child *c, long ms)
{
    long t = 0;
    for (;;) {
        if (GetMsg(c->port)) {
            c->pending = 0;
            c->have = c->sp->sp_Pkt.dp_Res1 > 0 ? c->sp->sp_Pkt.dp_Res1 : 0;
            c->off = 0;
            if (c->sp->sp_Pkt.dp_Res1 <= 0)
                c->eof = 1;
            return 1;
        }
        if (t >= ms)
            return 0;
        Delay(1);
        t += 20;
    }
}

static int a_spawn2(void *u, const char *const *argv, const char *const *env, const char *dir, void **h)
{
    sys_amiga *s = (sys_amiga *)u;
    static unsigned long serial;
    char id[16], nm[48], line[1024], num[12];
    BPTR in_r = 0, out_w = 0, dl = 0, old = 0;
    sa_child *c;
    if (!pty_mounted()) {
        cl_copy(s->dos.err, "PTY: is not mounted (the UP-Term kit mounts it)", sizeof(s->dos.err));
        return -1;
    }
    c = (sa_child *)AllocVec(sizeof(sa_child), MEMF_PUBLIC | MEMF_CLEAR);
    if (!c)
        goto oom;
    c->port = CreateMsgPort();
    c->sp = (struct StandardPacket *)AllocVec(sizeof(struct StandardPacket), MEMF_PUBLIC | MEMF_CLEAR);
    if (!c->port || !c->sp)
        goto oom;
    /* "<serial><task>" in hex, at most 15 characters (pty_name.h) */
    cl_ltoa((long)(++serial & 0xffff), id);
    cl_ltoa((long)((unsigned long)FindTask(0) & 0xfffff), num);
    cl_cat(id, "x", sizeof(id));
    cl_cat(id, num, sizeof(id));
    cl_copy(nm, "PTY:i", sizeof(nm));
    cl_cat(nm, id, sizeof(nm));
    cl_cat(nm, "/w", sizeof(nm));
    c->in_w = Open((STRPTR)nm, MODE_NEWFILE);
    nm[strlen(nm) - 1] = 'r';
    in_r = c->in_w ? Open((STRPTR)nm, MODE_OLDFILE) : 0;
    cl_copy(nm, "PTY:o", sizeof(nm));
    cl_cat(nm, id, sizeof(nm));
    cl_cat(nm, "/w", sizeof(nm));
    out_w = in_r ? Open((STRPTR)nm, MODE_NEWFILE) : 0;
    nm[strlen(nm) - 1] = 'r';
    c->out_r = out_w ? Open((STRPTR)nm, MODE_OLDFILE) : 0;
    if (!c->out_r) {
        dda_err(&s->dos, "cannot open the PTY: pipes");
        goto fail;
    }
    /* the command line: the program, then each argument quoted */
    if (cl_shell_line(line, sizeof(line), argv)) {
        cl_copy(s->dos.err, "the server's command line is longer than 1023 bytes", sizeof(s->dos.err));
        goto fail;
    }
    cl_copy(c->name, "amiga-pi server ", sizeof(c->name));
    cl_cat(c->name, id, sizeof(c->name));
    /* its environment: local variables, which the new Shell copies; ours put back after */
    env_set(env);
    if (dir && *dir) {
        dl = Lock((STRPTR)dir, SHARED_LOCK);
        if (!dl) {
            dda_err(&s->dos, dir);
            goto unvar;
        }
        old = CurrentDir(dl);
    }
    if (SystemTags((STRPTR)line, SYS_Input, in_r, SYS_Output, out_w, SYS_Asynch, TRUE, SYS_UserShell, TRUE,
                   NP_Name, (ULONG)c->name, TAG_END) == -1) {
        dda_err(&s->dos, "the server did not start");
        if (dl)
            UnLock(CurrentDir(old));
        dl = 0;
        goto unvar;
    }
    if (dl)
        UnLock(CurrentDir(old));
    env_restore();
    *h = c;
    return 0;
unvar:
    env_restore();
    goto fail;
oom:
    cl_copy(s->dos.err, "out of memory", sizeof(s->dos.err));
fail:
    if (out_w)
        Close(out_w);
    if (in_r)
        Close(in_r);
    if (c) {
        if (c->out_r)
            Close(c->out_r);
        if (c->in_w)
            Close(c->in_w);
        if (c->sp)
            FreeVec(c->sp);
        if (c->port)
            DeleteMsgPort(c->port);
        FreeVec(c);
    }
    return -1;
}

static int a_spawn2_write(void *u, void *h, const char *str, long n)
{
    sa_child *c = (sa_child *)h;
    (void)u;
    return c->in_w && Write(c->in_w, (APTR)str, n) == n ? 0 : -1;
}

static long a_spawn2_read(void *u, void *h, char *out, long cap, long ms)
{
    sa_child *c = (sa_child *)h;
    long k;
    (void)u;
    if (c->off >= c->have) {
        if (c->eof)
            return 0;
        if (!c->pending)
            read_start(c);
        if (!read_wait(c, ms))
            return -2;
        if (c->eof && !c->have)
            return 0;
    }
    k = c->have - c->off < cap ? c->have - c->off : cap;
    memcpy(out, c->buf + c->off, (size_t)k);
    c->off += k;
    return k;
}

static long a_spawn2_close(void *u, void *h, long ms)
{
    sa_child *c = (sa_child *)h;
    int round;
    (void)u;
    Close(c->in_w);
    c->in_w = 0;
    /* its output read to the end (EOF once it has ended); a break after ms, then 2 s more */
    for (round = 0; round < 2 && !c->eof; round++) {
        long t = 0;
        if (round) {
            struct Task *t2;
            Forbid();
            if ((t2 = FindTask((STRPTR)c->name)) != 0)
                Signal(t2, SIGBREAKF_CTRL_C);
            Permit();
        }
        while (!c->eof && t < (round ? 2000 : ms)) {
            if (!c->pending)
                read_start(c);
            read_wait(c, 100);
            t += 100;
        }
    }
    if (c->pending) {
        /* a read still out: its buffer must stay (the child never ended). Its reply, whenever it comes, must
         * not signal this task (gone by then, or busy with something else): the port ignores it */
        c->port->mp_Flags = PA_IGNORE;
        return -1;
    }
    Close(c->out_r);
    FreeVec(c->sp);
    DeleteMsgPort(c->port);
    FreeVec(c);
    return -1;                  /* SYS_Asynch gives no return code */
}

void sys_amiga_init(sys_amiga *s, cl_sys *sys)
{
    memset(s, 0, sizeof(*s));
    sys->u = s;
    sys->read = dda_read;
    sys->write = dda_write;
    sys->list = dda_list;
    sys->kind = dda_kind;
    sys->canon = a_canon;
    sys->run = a_run;
    sys->err = a_err;
    sys->mtime = a_mtime;
    sys->bg_start = a_bg_start;
    sys->bg_read = a_bg_read;
    sys->bg_kill = a_bg_kill;
    sys->bg_drop = a_bg_drop;
    sys->append = a_append;
    sys->mkdir = dda_mkdir;
    sys->remove = dda_remove;
    sys->getenv = a_getenv;
    sys->setenv = a_setenv;
    sys->clip = a_clip;
    sys->info = a_info;
    sys->now = a_now;
    sys->pause = a_pause;
    sys->bg_size = a_bg_size;
    sys->bg_file = a_bg_file;
    sys->rename = dda_rename;
    sys->quiet_kind = dda_quiet_kind;
    sys->pread = dda_pread;
    sys->spawn2 = a_spawn2;
    sys->spawn2_write = a_spawn2_write;
    sys->spawn2_read = a_spawn2_read;
    sys->spawn2_close = a_spawn2_close;
}
