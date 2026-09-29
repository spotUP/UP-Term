/* vsh -- a Unix-style shell for AmigaOS 3.x (vtcon phase S).
 *
 * The grammar, expansion and control logic are the portable core
 * (sh_parse.c, sh_expand.c, sh_exec.c); this file is the AmigaDOS side:
 * streams are DOS file handles, pipes are PIPE: channels, commands run
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
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdlib.h>
#include <string.h>
#include "sh_exec.h"

static const char version[] = "$VER: vsh 0.1 (29.9.2026)";

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
} job;

/* The pipes vsh made. AmigaOS has no SIGPIPE: a PIPE: writer whose reader
 * has gone blocks for ever (rig: List | less, q, and the shell never came
 * back). So closing a read end is the broken pipe: the writer gets Ctrl-C
 * and the pipe is read dry, which lets its blocked Write return. */
typedef struct pipe_rec {
    BPTR rd, wr;
    job *writer;            /* the command writing it, if one runs */
} pipe_rec;

static pipe_rec pipe_tab[32];

/* Ctrl-C to a running job: its runner, which is the command's process
 * for a loaded command, and the Shell process SystemTags spawned for a
 * Resident command or a script (found by the name vsh gave it). Call
 * under Forbid. */
static void job_break(job *j)
{
    struct Task *t;
    if (j->done)
        return;
    Signal(j->task, SIGBREAKF_CTRL_C);
    if (j->child[0] && (t = FindTask((STRPTR)j->child)) != 0)
        Signal(t, SIGBREAKF_CTRL_C);
}

/* Close a stream; every close of a stream vsh handed out comes here. */
static void close_stream(BPTR fh)
{
    pipe_rec *pr = 0;
    int i;
    if (!fh)
        return;
    Forbid();
    for (i = 0; i < 32; i++)
        if (pipe_tab[i].rd == fh || pipe_tab[i].wr == fh)
            pr = &pipe_tab[i];
    if (pr && pr->rd == fh) {
        if (pr->writer)
            job_break(pr->writer);
        Permit();
        {
            static char sink[512]; /* only read into, never used: shared is fine */
            while (Read(fh, sink, sizeof(sink)) > 0)
                ;
        }
        Forbid();
        pr->rd = 0;
    } else if (pr) {
        pr->wr = 0;
        pr->writer = 0;
    }
    Permit();
    Close(fh);
}

static sh_fh os_open(void *os, const char *path, int mode)
{
    BPTR fh;
    (void)os;
    if (mode == SH_OPEN_READ)
        return (sh_fh)Open((STRPTR)path, MODE_OLDFILE);
    if (mode == SH_OPEN_WRITE)
        return (sh_fh)Open((STRPTR)path, MODE_NEWFILE);
    fh = Open((STRPTR)path, MODE_READWRITE);
    if (fh)
        Seek(fh, 0, OFFSET_END);
    return (sh_fh)fh;
}

static void os_close(void *os, sh_fh fh)
{
    (void)os;
    close_stream((BPTR)fh);
}

static long pipes;

static int os_pipe(void *os, sh_fh *rd, sh_fh *wr)
{
    char name[48];
    long n = ++pipes, k;
    char digits[12];
    int d = 0;
    (void)os;
    strcpy(name, "PIPE:vsh.");
    k = (long)FindTask(0);
    do
        digits[d++] = "0123456789abcdef"[k & 15];
    while ((k >>= 4) && d < 8);
    while (d)
        strncat(name, &digits[--d], 1);
    strcat(name, ".");
    d = 0;
    do
        digits[d++] = (char)('0' + n % 10);
    while ((n /= 10) > 0);
    while (d)
        strncat(name, &digits[--d], 1);
    *wr = (sh_fh)Open((STRPTR)name, MODE_NEWFILE);
    *rd = (sh_fh)Open((STRPTR)name, MODE_OLDFILE);
    if (!*wr || !*rd) {
        if (*wr)
            Close((BPTR)*wr);
        if (*rd)
            Close((BPTR)*rd);
        return -1;
    }
    Forbid();
    for (k = 0; k < 32; k++)
        if (!pipe_tab[k].rd && !pipe_tab[k].wr) {
            pipe_tab[k].rd = (BPTR)*rd;
            pipe_tab[k].wr = (BPTR)*wr;
            pipe_tab[k].writer = 0;
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

static void runner(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    job *j;
    WaitPort(&me->pr_MsgPort);
    j = (job *)GetMsg(&me->pr_MsgPort);
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
    /* the runner has the shell's current directory (CreateNewProc copies it) */
    if (j->seg) {
        BPTR oin = SelectInput(j->in), oout = SelectOutput(j->out);
        BPTR oerr = me->pr_CES;
        if (j->err)
            me->pr_CES = j->err;
        SetProgramName((STRPTR)j->name);
        j->rc = RunCommand(j->seg, 16000, (STRPTR)j->args, (LONG)strlen(j->args));
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
    if (j->close_in)
        close_stream(j->in);
    if (j->close_out)
        close_stream(j->out);
    if (j->close_err && j->err != j->out)
        close_stream(j->err);
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

/* A command name to something to run: *seg a loaded command file, or 0
 * for SystemTags (Resident commands, scripts). -1: found nowhere. The
 * Shell's order: a path as given; else the current directory, the
 * Shell's path, C:. */
static int resolve(const char *name, BPTR *seg)
{
    struct CommandLineInterface *cli = Cli();
    BPTR lock, old;
    BPTR *node;
    *seg = 0;
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
    if ((lock = Lock((STRPTR)name, SHARED_LOCK)) != 0) {
        UnLock(lock);
        *seg = LoadSeg((STRPTR)name);
        return 0; /* not loadable (a script): SystemTags */
    }
    if (strchr(name, ':') || strchr(name, '/'))
        return -1;
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
    if (!cmd)
        return -1;
    if (resolve(argv[0], &seg) < 0) {
        free(cmd);
        if ((io->owned & SH_OWN_IN) && io->in)
            close_stream((BPTR)io->in);
        if ((io->owned & SH_OWN_OUT) && io->out)
            close_stream((BPTR)io->out);
        return -1;
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
            if (v->exported)
                need += (long)strlen(v->name) + (long)strlen(v->value) + 2;
        j->env = e = (char *)malloc(need);
        if (e) {
            for (v = sh->ctx.vars; v; v = v->next)
                if (v->exported) {
                    strcpy(e, v->name);
                    e += strlen(e) + 1;
                    strcpy(e, v->value);
                    e += strlen(e) + 1;
                }
            *e = 0;
        }
    }
    j->cmd = cmd;
    j->seg = seg;
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
    } else {
        /* streams it does not own are the shell's: it gets its own handles
         * on the same console (or NIL: for input), not the shell's */
        j->in = (io->owned & SH_OWN_IN) ? (BPTR)io->in : Open((STRPTR)"NIL:", MODE_OLDFILE);
        j->out = (io->owned & SH_OWN_OUT) ? (BPTR)io->out : Open((STRPTR)"*", MODE_NEWFILE);
        j->err = (io->owned & SH_OWN_ERR) ? (BPTR)io->err : 0;
        j->close_in = j->close_out = 1;
        j->close_err = (io->owned & SH_OWN_ERR) != 0;
    }
    p = (j->name && j->args)
        ? CreateNewProcTags(NP_Entry, (ULONG)runner, NP_Name, (ULONG)"vsh job", NP_StackSize, 8000,
                            NP_Cli, TRUE, TAG_END)
        : 0;
    if (!p) {
        if (j->close_in)
            close_stream(j->in);
        if (j->close_out)
            close_stream(j->out);
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
            if (pipe_tab[i].wr == j->out)
                pipe_tab[i].writer = j;
        Permit();
    }
    SetSignal(0, SIGBREAKF_CTRL_C); /* an old Ctrl-C is not for this command */
    PutMsg(&p->pr_MsgPort, &j->msg);
    if (wait)
        return os_wait(os, (long)j);
    return (long)j;
}

/* Wait for a job. Ctrl-C while waiting goes on to the job's runner, the
 * command's process, as long as it runs (the console signals the shell). */
static long os_wait(void *os, long id)
{
    job *j = (job *)id;
    LONG rc;
    int broke = 0;
    for (;;) {
        ULONG got;
        Forbid();
        if (replied(PORT(os), j)) {
            Remove(&j->msg.mn_Node);
            Permit();
            break;
        }
        Permit();
        got = Wait((1UL << PORT(os)->mp_SigBit) | SIGBREAKF_CTRL_C);
        if (got & SIGBREAKF_CTRL_C) {
            Forbid(); /* still running: its reply is not in yet */
            if (!replied(PORT(os), j))
                job_break(j);
            Permit();
            broke = 1;
        }
    }
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

static long os_write(void *os, sh_fh fh, const char *b, long n)
{
    (void)os;
    return Write((BPTR)fh, (APTR)b, n);
}

static long os_read_line(void *os, sh_fh fh, char *buf, long max)
{
    (void)os;
    if (!FGets((BPTR)fh, (STRPTR)buf, max))
        return -1;
    return (long)strlen(buf);
}

/* Unix-style directory names: .. is the parent (/), . and "" stay. */
static int os_chdir(void *os, const char *path)
{
    char p[256];
    BPTR lock;
    (void)os;
    if (!strcmp(path, ".."))
        strcpy(p, "/");
    else if (!strcmp(path, "."))
        strcpy(p, "");
    else {
        strncpy(p, path, sizeof(p) - 1);
        p[sizeof(p) - 1] = 0;
    }
    lock = Lock((STRPTR)p, SHARED_LOCK);
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

static int os_exists(void *os, const char *path, int want_dir)
{
    BPTR lock = Lock((STRPTR)path, SHARED_LOCK);
    struct FileInfoBlock *fib;
    int r = 0;
    (void)os;
    if (!lock)
        return 0;
    if (want_dir < 0) {
        UnLock(lock);
        return 1;
    }
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib && Examine(lock, fib))
        r = want_dir ? fib->fib_DirEntryType > 0 : fib->fib_DirEntryType < 0;
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
        if (!wait) {
            /* in the background: streams it does not own are the shell's;
             * it gets its own handles on the same console (NIL: for input) */
            if (!(io->owned & SH_OWN_IN)) {
                j->io.in = (sh_fh)Open((STRPTR)"NIL:", MODE_OLDFILE);
                j->io.owned |= SH_OWN_IN;
            }
            if (!(io->owned & SH_OWN_OUT)) {
                j->io.out = (sh_fh)Open((STRPTR)"*", MODE_NEWFILE);
                j->io.owned |= SH_OWN_OUT;
            }
            if (!(io->owned & SH_OWN_ERR))
                j->io.err = j->io.out;
        }
        p = CreateNewProcTags(NP_Entry, (ULONG)subshell_proc, NP_Name, (ULONG)"vsh subshell",
                              NP_StackSize, 32000, NP_Cli, TRUE, TAG_END);
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
            if (pipe_tab[i].wr == (BPTR)j->io.out)
                pipe_tab[i].writer = j;
        Permit();
    }
    PutMsg(&p->pr_MsgPort, &j->msg);
    if (wait)
        return os_wait(os, (long)j);
    return (long)j;
}

/* ---- the prompt and the main loop ----------------------------------------------- */

static void prompt(sh_shell *sh, int more)
{
    const char *ps = sh_get(&sh->ctx, more ? "PS2" : "PS1");
    char *text = sh_prompt(sh, ps ? ps : more ? "> " : "%F{cyan}%~%f %# ");
    if (text) {
        Write(Output(), text, (LONG)strlen(text));
        free(text);
    }
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

int main(int argc, char **argv)
{
    static sh_shell sh;
    char line[1024];
    char *text = 0;
    long len = 0;
    int i;
    static vproc vp;
    (void)version;
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
    sh.os.spawn = os_spawn;
    sh.os.read = os_read;
    sh.os.interrupted = os_interrupted;
    sh.os.write = os_write;
    sh.os.read_line = os_read_line;
    sh.os.chdir = os_chdir;
    sh.os.cwd = os_cwd;
    sh.os.exists = os_exists;
    sh.os.data = &vp;
    sh.ctx.listdir = list_dir;
    sh.ctx.nocase = 1;
    sh.ctx.pid = (long)FindTask(0);
    sh.io.in = (sh_fh)Input();
    sh.io.out = (sh_fh)Output();
    sh.io.err = (sh_fh)Output();
    sh.io.owned = 0;
    sh_set(&sh.ctx, "HOME", "SYS:");
    import_var(&sh, "HOME");
    import_var(&sh, "USER");
    import_var(&sh, "HOST");
    import_var(&sh, "HOSTNAME");
    for (i = 1; i < argc; i++)
        sh_list_add(&sh.ctx.args, argv[i]);
    /* AmigaDOS leaves a command's argument line in its input buffer (for
     * ReadArgs); unread, vsh took it as its first, empty, command line
     * and printed a second prompt (rig, 2026-09-29) */
    Flush(Input());
    /* the system's startup file (ENVARC:vsh/vshrc, copied to ENV: at boot),
     * then the user's */
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
            source_if(&sh, p);
            free(p);
        }
        canonical_home(&sh);
    }
    for (;;) {
        long n;
        int incomplete = 0;
        if (sh.exiting)
            break;
        if (!text)
            sh_notify(&sh);
        prompt(&sh, text != 0);
        if (!FGets(Input(), (STRPTR)line, sizeof(line)))
            break;
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
        sh_run_text(&sh, text, &incomplete);
        if (incomplete)
            continue;
        free(text);
        text = 0;
        len = 0;
    }
    free(text);
    {
        long st = sh.exiting ? sh.exit_status : sh.ctx.status;
        sh_shell_free(&sh);
        DeleteMsgPort(vp.port);
        return (int)st;
    }
}
