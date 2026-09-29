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
    if (fh)
        Close((BPTR)fh);
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

/* A background command runs in a runner process of its own, which calls
 * SystemTags and replies with the exit status. */
typedef struct job {
    struct Message msg;
    char *cmd;
    BPTR in, out;
    int close_in, close_out;
    LONG rc;
} job;

static void runner(void)
{
    struct Process *me = (struct Process *)FindTask(0);
    job *j;
    WaitPort(&me->pr_MsgPort);
    j = (job *)GetMsg(&me->pr_MsgPort);
    /* the runner has the shell's current directory (CreateNewProc copies it) */
    j->rc = SystemTags((STRPTR)j->cmd, SYS_Input, j->in, SYS_Output, j->out,
                       SYS_UserShell, TRUE, TAG_END);
    if (j->close_in)
        Close(j->in);
    if (j->close_out)
        Close(j->out);
    Forbid(); /* the reply and our end, before the shell can free anything */
    ReplyMsg(&j->msg);
}

static struct MsgPort *job_port;

static long os_run(void *os, char **argv, const sh_io *io, int wait)
{
    sh_shell *sh = (sh_shell *)os;
    char *cmd = command_line(argv);
    sh_var *v;
    LONG rc;
    if (!cmd)
        return -1;
    /* exported variables are local variables for the command */
    for (v = sh->ctx.vars; v; v = v->next)
        if (v->exported)
            SetVar((STRPTR)v->name, (STRPTR)v->value, -1, GVF_LOCAL_ONLY);
    if (wait) {
        rc = SystemTags((STRPTR)cmd, SYS_Input, (BPTR)io->in, SYS_Output, (BPTR)io->out,
                        SYS_UserShell, TRUE, TAG_END);
        free(cmd);
        if ((io->owned & SH_OWN_IN) && io->in)
            Close((BPTR)io->in);
        if ((io->owned & SH_OWN_OUT) && io->out)
            Close((BPTR)io->out);
        return rc < 0 ? -1 : rc;
    }
    {
        job *j = (job *)AllocVec(sizeof(job), MEMF_PUBLIC | MEMF_CLEAR);
        struct Process *p;
        if (!j) {
            free(cmd);
            return -1;
        }
        j->msg.mn_ReplyPort = job_port;
        j->msg.mn_Length = sizeof(job);
        j->cmd = cmd;
        /* streams it does not own are the shell's: it gets its own handles
         * on the same console (or NIL: for input), not the shell's */
        j->in = (io->owned & SH_OWN_IN) ? (BPTR)io->in : Open((STRPTR)"NIL:", MODE_OLDFILE);
        j->out = (io->owned & SH_OWN_OUT) ? (BPTR)io->out : Open((STRPTR)"*", MODE_NEWFILE);
        j->close_in = j->close_out = 1;
        p = CreateNewProcTags(NP_Entry, (ULONG)runner, NP_Name, (ULONG)"vsh job", NP_StackSize, 8000,
                              NP_Cli, TRUE, TAG_END);
        if (!p) {
            Close(j->in);
            Close(j->out);
            free(cmd);
            FreeVec(j);
            return -1;
        }
        PutMsg(&p->pr_MsgPort, &j->msg);
        return (long)j;
    }
}

static long os_wait(void *os, long id)
{
    job *j = (job *)id, *m;
    LONG rc;
    (void)os;
    /* replies come in any order: wait until this one is among them */
    for (;;) {
        Forbid();
        for (m = (job *)job_port->mp_MsgList.lh_Head; m->msg.mn_Node.ln_Succ;
             m = (job *)m->msg.mn_Node.ln_Succ)
            if (m == j)
                break;
        if (m->msg.mn_Node.ln_Succ) {
            Remove(&m->msg.mn_Node);
            Permit();
            break;
        }
        Permit();
        Wait(1UL << job_port->mp_SigBit); /* set by every reply: look again */
    }
    rc = j->rc;
    free(j->cmd);
    FreeVec(j);
    return rc;
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

/* $(cmd): run it into a T: file, read it back. */
static sh_shell *the_shell;

static char *subst(sh_ctx *c, const char *cmd)
{
    sh_shell *sh = the_shell;
    char path[40];
    sh_io io;
    BPTR fh;
    char *out = 0;
    long len = 0, n;
    char buf[256];
    int inc = 0;
    (void)c;
    strcpy(path, "T:vsh-subst");
    fh = Open((STRPTR)path, MODE_NEWFILE);
    if (!fh)
        return 0;
    io = sh->io;
    io.out = (sh_fh)fh;
    io.owned = 0;
    {
        sh_io saved = sh->io;
        sh->io = io;
        sh_run_text(sh, cmd, &inc);
        sh->io = saved;
    }
    Close(fh);
    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh)
        return 0;
    while ((n = Read(fh, buf, sizeof(buf))) > 0) {
        char *t = (char *)realloc(out, len + n + 1);
        if (!t)
            break;
        out = t;
        memcpy(out + len, buf, n);
        len += n;
        out[len] = 0;
    }
    Close(fh);
    DeleteFile((STRPTR)path);
    return out ? out : (char *)calloc(1, 1);
}

/* ---- the prompt and the main loop ----------------------------------------------- */

static void prompt(sh_shell *sh, int more)
{
    const char *ps = sh_get(&sh->ctx, more ? "PS2" : "PS1");
    char *cwd;
    if (!ps)
        ps = more ? "> " : "\\w> ";
    for (; *ps; ps++) {
        if (ps[0] == '\\' && ps[1] == 'w') {
            cwd = os_cwd(sh);
            if (cwd) {
                Write(Output(), cwd, (LONG)strlen(cwd));
                free(cwd);
            }
            ps++;
        } else if (ps[0] == '\\' && ps[1] == 'e') {
            Write(Output(), "\033", 1);
            ps++;
        } else {
            Write(Output(), (APTR)ps, 1);
        }
    }
}

int main(int argc, char **argv)
{
    static sh_shell sh;
    char line[1024];
    char *text = 0;
    long len = 0;
    int i;
    (void)version;
    job_port = CreateMsgPort();
    if (!job_port)
        return 20;
    the_shell = &sh;
    sh_shell_init(&sh);
    sh.os.open = os_open;
    sh.os.close = os_close;
    sh.os.pipe = os_pipe;
    sh.os.run = os_run;
    sh.os.wait = os_wait;
    sh.os.write = os_write;
    sh.os.read_line = os_read_line;
    sh.os.chdir = os_chdir;
    sh.os.cwd = os_cwd;
    sh.os.exists = os_exists;
    sh.os.data = &sh;
    sh.ctx.listdir = list_dir;
    sh.ctx.subst = subst;
    sh.ctx.nocase = 1;
    sh.ctx.pid = (long)FindTask(0);
    sh.io.in = (sh_fh)Input();
    sh.io.out = (sh_fh)Output();
    sh.io.err = (sh_fh)Output();
    sh.io.owned = 0;
    sh_set(&sh.ctx, "HOME", "SYS:");
    {
        char home[256];
        if (GetVar((STRPTR)"HOME", (STRPTR)home, sizeof(home), 0) > 0)
            sh_set(&sh.ctx, "HOME", home);
    }
    for (i = 1; i < argc; i++)
        sh_list_add(&sh.ctx.args, argv[i]);
    {
        /* ENVARC:vsh/vshrc, then a script named on the command line */
        BPTR rc = Lock((STRPTR)"ENV:vsh/vshrc", SHARED_LOCK);
        if (rc) {
            UnLock(rc);
            sh_run_text(&sh, "source ENV:vsh/vshrc", 0);
        }
    }
    for (;;) {
        long n;
        int incomplete = 0;
        if (sh.exiting)
            break;
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
        DeleteMsgPort(job_port);
        return (int)st;
    }
}
