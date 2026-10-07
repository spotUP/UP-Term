/* vsh's executor: runs a parsed tree. The control logic (lists, && ||,
 * if/while/until/for/case, functions, builtins, redirections, pipelines)
 * is portable; everything the OS does goes through sh_os, which the
 * Amiga shell implements with DOS and the host tests fake. */
#ifndef SH_EXEC_H
#define SH_EXEC_H
#include "sh_parse.h"
#include "sh_expand.h"

/* An open stream: an opaque handle of the OS layer (a BPTR on the Amiga). */
typedef long sh_fh;
#define SH_NOFH 0L

/* A command's standard streams. */
typedef struct sh_io {
    sh_fh in, out, err;
    int owned;             /* SH_OWN_*: streams opened for this command, closed when it ends */
    int fdmark;            /* with SH_OWN_FDS: the shell's fd table undo height before this command's redirections */
} sh_io;

#define SH_OWN_IN  1
#define SH_OWN_OUT 2
#define SH_OWN_ERR 4
#define SH_OWN_FDS 8   /* the command changed the shell's fd table (fds 3 and up): undone when it ends */
#define SH_FDMAX   61  /* the table holds fds 3 .. 63 */
#define SH_FDUNDO  64

#define SH_OPEN_READ   0
#define SH_OPEN_WRITE  1   /* create / truncate */
#define SH_OPEN_APPEND 2
#define SH_OPEN_RDWR   3   /* read and write an existing or new file, no truncation (<>) */

struct sh_shell;

/* What stat says about a name (test -e -f -d -s -r -w -x -L -nt -ef ...). */
#define SH_ST_FILE  1
#define SH_ST_DIR   2
#define SH_ST_CHAR  3
#define SH_ST_BLOCK 4
#define SH_ST_FIFO  5
#define SH_ST_SOCK  6
#define SH_ST_OTHER 7
typedef struct sh_stat {
    int type;              /* SH_ST_* */
    int link;              /* a symbolic link (nofollow lookup only) */
    long size;
    long mtime, atime;     /* seconds */
    unsigned mode;         /* rwxrwxrwx plus 04000 set-uid, 02000 set-gid, 01000 sticky */
    unsigned access;       /* what this user may do: 4 read, 2 write, 1 execute */
    int owned, group;      /* owned by the effective user / group */
    long dev, ino;         /* identity, for -ef */
} sh_stat;

/* run(wait) and wait: the command was suspended (^Z) and still exists;
 * sh_os.stopped holds its job id (see sh_os.cont). */
#define SH_STOPPED (-1000L)
/* $? of a suspended command: 128 + SIGTSTP (18, as ixemul numbers it) */
#define SH_STATUS_STOPPED 146

/* signals 1..31 as ixemul (BSD) numbers them; index 0 of the trap table is EXIT */
#define SH_NSIG 32
#define TRAP_ERR   SH_NSIG
#define TRAP_DEBUG (SH_NSIG + 1)
#define TRAP_RETURN (SH_NSIG + 2)
#define SH_NTRAP   (SH_NSIG + 3)

#define SH_ID_PPID 0
#define SH_ID_UID  1
#define SH_ID_EUID 2

typedef struct sh_os {
    sh_fh (*open)(void *os, const char *path, int mode);
    void  (*close)(void *os, sh_fh fh);
    /* a pipe: data written to *wr is read from *rd; 0 = ok */
    int   (*pipe)(void *os, sh_fh *rd, sh_fh *wr);
    /* start argv as a command with io; the streams io->owned marks become
     * the OS layer's to close -- when the command ends, or at once if it
     * cannot start (the caller never closes them again). wait: return its exit
     * status (-1: not found); !wait: return a job id (> 0) for sh_os.wait,
     * or -1 */
    long  (*run)(void *os, char **argv, const sh_io *io, int wait);
    long  (*wait)(void *os, long job);            /* its exit status */
    int   (*done)(void *os, long job);            /* 1: it has ended (wait returns at once) */
    /* Run a subshell: tree in a process of its own with child, a clone of
     * the shell (sh_shell_clone). The OS layer sets child->os.data (and
     * ctx.pid) for that process and calls sh_run_child there, which runs
     * the tree, closes io's owned streams and frees child and tree. The
     * streams io->owned marks are the OS layer's from here, as with run.
     * wait: the exit status; !wait: a job id for wait/done. -1: it could
     * not start -- child and tree stay the caller's (the streams do not).
     * 0 (no spawn): subshells run in the shell's own process. */
    long  (*spawn)(void *os, struct sh_shell *child, sh_parse *tree, const sh_io *io, int wait);
    long  (*read)(void *os, sh_fh fh, char *buf, long max); /* raw; 0 at the end */
    int   (*interrupted)(void *os);  /* Ctrl-C arrived since the last call (0 = none) */
    long  (*write)(void *os, sh_fh fh, const char *buf, long n);
    long  (*read_line)(void *os, sh_fh fh, char *buf, long max); /* -1 at the end */
    int   (*chdir)(void *os, const char *path);    /* 0 = ok */
    /* 0 = ok; nofollow: do not follow a symbolic link (test -L) */
    int   (*stat)(void *os, const char *path, sh_stat *st, int nofollow);
    char *(*cwd)(void *os);                        /* malloc'ed */
    int   (*isatty)(void *os, sh_fh fh);           /* test -t (0 = none: never) */
    /* the stack commands get, in bytes: set it (bytes > 0), and return it
     * (0 = none: the builtin stack says so) */
    long  (*stack)(void *os, long bytes);
    /* Job control (cont 0: none). While the shell sets `suspendable` (it
     * waits for one simple command in the foreground), run(wait) and wait
     * may return SH_STOPPED: the command was suspended and still exists,
     * and `stopped` is its job id for wait/done/cont. cont continues a
     * stopped job (SIGCONT); 0 = ok. */
    int   (*cont)(void *os, long job);
    int   suspendable;
    long  stopped;
    /* The umask builtin's mask, passed to the OS layer so commands it starts
     * inherit it (0 = none: the mask is kept by the shell only). */
    void  (*umask)(void *os, int mask);
    /* kill: send sig (0: only test it exists) to a process, or a job id (is_job) of this shell; 0 = ok;
     * 0 in the table: kill reports it cannot */
    int   (*signal)(void *os, long target, int sig, int is_job);
    /* read -t: 1 when input is waiting (or at its end) within ms milliseconds, 0 on timeout;
     * 0 in the table: always ready */
    int   (*ready)(void *os, sh_fh fh, long ms);
    /* read -s: terminal echo off (0) or on (1) for fh; 0 in the table: no effect */
    void  (*echo)(void *os, sh_fh fh, int on);
    /* the time of day: seconds since 1970 and, in *usec, the microseconds (SECONDS, EPOCHSECONDS,
     * EPOCHREALTIME, the RANDOM seed); 0 in the table: always 0 */
    long  (*now)(void *os, long *usec);
    /* PPID, UID, EUID: what is SH_ID_*; 0 in the table: 0 */
    long  (*sysid)(void *os, int what);
    /* delete a file (process substitution and here-document temp files); 0 = ok; 0 in the table: files stay */
    int   (*remove)(void *os, const char *path);
    /* the directory prefix of process substitution temp files, ending in : or / ("T:", "RAM:", "/tmp/"); 0 in the table: "T:" */
    const char *(*tmpdir)(void *os);
    void *data;
} sh_os;

typedef struct sh_func {
    char *name;
    char *src;             /* the file it was defined in (BASH_SOURCE) */
    sh_parse body;         /* its own copy of the body (body.tree) */
    int busy;              /* running: a redefinition retires the old body */
    struct sh_func *next;
} sh_func;

/* One level of the call stack: a function call or a sourced file (FUNCNAME, BASH_SOURCE, BASH_LINENO) */
typedef struct sh_frame {
    char *name, *src;      /* the function (or "source") and the file its code is in */
    long line;             /* the line it was called from */
} sh_frame;

typedef struct sh_retired {
    sh_parse p;            /* a function body replaced while it ran */
    struct sh_retired *next;
} sh_retired;

/* Shell options (sh_shell.opts): one table in sh_exec.c names them, gives their
 * letter for set/$-/invocation. SO_INTERACTIVE, SO_STDIN, SO_COMMAND are the
 * invocation's i, s, c. */
#define SO_ALLEXPORT   0x1UL
#define SO_BRACEEXPAND 0x2UL
#define SO_ERREXIT     0x4UL
#define SO_HASHALL     0x8UL
#define SO_NOCLOBBER   0x10UL
#define SO_NOEXEC      0x20UL
#define SO_NOGLOB      0x40UL
#define SO_NOUNSET     0x80UL
#define SO_PIPEFAIL    0x100UL
#define SO_VERBOSE     0x200UL
#define SO_XTRACE      0x400UL
#define SO_INTERACTIVE 0x800UL
#define SO_COMMAND     0x1000UL
#define SO_STDIN       0x2000UL
#define SO_POSIX       0x4000UL
#define SO_LASTPIPE    0x8000UL   /* shopt lastpipe: the last stage of a pipeline runs in the shell */
#define SO_ICOMMENTS   0x10000UL
#define SO_ERRTRACE    0x40000UL  /* set -E: functions and subshells inherit the ERR trap */
#define SO_FUNCTRACE   0x80000UL  /* set -T: functions and subshells inherit DEBUG and RETURN */
#define SO_INERT       0x20000UL  /* the accepted names with no effect (emacs vi history ...): first of many */

/* a temp file of a here-document or process substitution: removed when the command using it ends;
 * cmd (>( ) only) runs first, reading the file */
typedef struct sh_tmp {
    char *path, *cmd;
} sh_tmp;

typedef struct sh_shell {
    sh_ctx ctx;
    unsigned long opts;    /* SO_* */
    int cond_depth;        /* errexit is ignored while > 0: if/while conditions, !, && || lists */
    int xlevel;            /* xtrace nesting: $( ) adds a PS4 character */
    char flagbuf[40];      /* $- */
    sh_os os;
    sh_io io;              /* the shell's own streams */
    sh_func *funcs;
    sh_list aliases;       /* "name=value" */
    int exiting;           /* exit ran: stop */
    long exit_status;
    int breaking;          /* break / continue levels pending */
    int continuing;
    int returning;         /* return in a function */
    int loop_depth, func_depth;
    long jobs[32];         /* background job ids, 0 = free */
    char *job_text[32];
    char job_stopped[32];  /* suspended (^Z), until fg or bg */
    int warned_stopped;    /* exit said once that jobs are stopped */
    sh_retired *retired;   /* freed with the shell */
    int intr;              /* Ctrl-C: unwinding to the prompt */
    unsigned long stack_limit; /* the OS layer's: below this stack address the
                               * interpreter stops ("nested too deeply"), 0 = none */
    int subst_ran;         /* a $( ) ran in the current command ... */
    long subst_status;     /* ... with this status (assignments only: the command's $?) */
    int heredocs;          /* numbering for here-document temp files */
    struct sh_tmp *tmps;   /* temp files to remove when the command that uses them ends (and >( ) bodies to run first) */
    int ntmp, captmp;
    struct { sh_fh fh; int own; } fdt[SH_FDMAX]; /* the shell's fds 3 and up (index = fd - 3); own: closed when no slot or stream uses it */
    struct { int slot; sh_fh fh; int own; } fdundo[SH_FDUNDO]; /* values the current commands' redirections replaced */
    int nundo;
    struct sh_fddefer *fddefer; /* handles a running background job still uses, closed when it is waited for */
    sh_fh closed[8];       /* null-device streams standing for a stream closed with n>&- (see put) */
    int nclosed;
    int wfail;             /* a builtin wrote to one of them: its status becomes 1 */
    char *traps[SH_NTRAP]; /* trap actions: [0] EXIT, [n] signal n, then ERR DEBUG RETURN (0: none, "": ignored) */
    int debug_done;        /* the DEBUG trap already ran for the simple command about to run (a pipeline stage) */
    signed char shopt_v[64]; /* shopt options that have no flag of their own: -1 the default, 0 off, 1 on */
    int trap_busy;         /* bit per ERR DEBUG RETURN trap that is running (a trap does not fire inside itself) */
    int in_trap;           /* a trap action is running */
    int exit_trap_ran;     /* the EXIT trap has run (once per shell) */
    void *locals;          /* local's saved variables, a stack (sh_exec.c: saved_var) */
    int n_locals, cap_locals;
    int local_mark;        /* the running function's locals are locals[local_mark..n_locals) */
    int umask;             /* the umask builtin's mask (default 022) */
    int optpos;            /* getopts: the next character inside a cluster (-abc), 0 = at an argument */
    long optind_seen;      /* the OPTIND getopts left, to notice a script resetting it */
    long lineno;           /* LINENO: the line of the command running */
    sh_frame *frames;      /* the call stack, innermost last */
    int nframes, capframes;
    const char *main_src;  /* the script file (BASH_SOURCE of the bottom frame); 0: -c, stdin, interactive */
    int main_run;          /* the script file has been started (the driver runs it as `source "$0"`) */
    const char *cur_src;   /* the file whose commands run now: new functions record it */
    unsigned long rseed, srnd; /* RANDOM and SRANDOM generators */
    long last_rand;
    int seeded;
    long secs0;            /* the time SECONDS counts from */
    int special_busy;      /* the store is being brought up to date (no recursion) */
} sh_shell;

/* What the command line asked for (sh_invoke). */
typedef struct sh_invoke_info {
    const char *command;   /* -c text, 0 = none */
    const char *script;    /* the file to run, 0 = none (stdin, or -c) */
    int norc;              /* --norc / --noprofile: skip the startup files */
    int login;             /* -l / --login */
    int exit_now;          /* --version, --help, or an error: end with status */
    int status;
} sh_invoke_info;

/* Parse vsh's command line (bash's: -c -e -u -x -v -f -C -a -n -h -i -l -s -o NAME +o NAME,
 * clusters, --, -, --login --norc --noprofile --posix --version --help) into sh's options,
 * $0 and positionals. tty: standard input is a terminal. Used by vsh.c and vsh_host.c. */
void sh_invoke(sh_shell *sh, int argc, char **argv, int tty, sh_invoke_info *info);

void sh_shell_init(sh_shell *sh);
void sh_shell_free(sh_shell *sh);

/* A subshell's copy of sh: variables (and which are exported), $0 $1.. $?,
 * functions (bodies copied), aliases, the OS table and streams; no jobs.
 * malloc'ed; 0 when memory runs out. */
sh_shell *sh_shell_clone(const sh_shell *sh);

/* In the subshell's process: run tree with io, close io's owned streams,
 * free child and tree (both malloc'ed); the exit status. */
long sh_run_child(sh_shell *child, sh_parse *tree, const sh_io *io);

#define SH_WORDS_COMMANDS 1   /* builtins, functions, aliases */
#define SH_WORDS_VARIABLES 2  /* variable names */

/* The names the shell knows, NUL-separated, into out (at most max bytes,
 * whole names only). Returns the bytes used. For the console's
 * completion and command colouring. */
long sh_word_list(const sh_shell *sh, int kind, char *out, long max);

/* The next directory of a Unix $PATH ("/gg/bin:/c:."), as AmigaDOS names
 * it, into dir (at most max bytes): /vol/rest is vol:rest, /vol is vol:,
 * an empty entry or "." the current directory (""), a relative entry
 * stays relative. "/" (the volume list) and entries too long for dir are
 * skipped. *p moves past the entry; 0 when the list is done. The same
 * $PATH is what ixemul programs search (execvp), so both agree. */
int sh_path_next(const char **p, char *dir, long max);

/* A Unix absolute name as AmigaDOS has it: "/vol/rest" is "vol:rest" and
 * "/vol" is "vol:", into out (max bytes). 0 for anything else ("/" alone,
 * "//x", a relative name, too long). On AmigaDOS a leading "/" is the
 * parent directory, so vsh tries a name that way first and this one only
 * when there is nothing by the Amiga meaning (vsh.c lock_name). */
int sh_unix_root(const char *in, char *out, long max);

/* A signal reached the shell (2 = INT, 15 = TERM): run its trap action. 1 =
 * a trap took it (the action ran, or the signal is ignored with trap '' SIG),
 * 0 = no trap: the caller's own handling follows. INT is polled by the core
 * itself (sh_os.interrupted); the OS layer calls this for TERM. */
int sh_trap_signal(sh_shell *sh, int sig);

/* Run the EXIT trap, once. The caller runs it where the shell ends (end of
 * input, after exit, a subshell's end -- sh_run_child does it for those). */
void sh_exit_trap(sh_shell *sh);

/* Run one input text (a line, or a script). Returns the exit status of
 * its last command; *incomplete is set when the text needs more lines. */
long sh_run_text(sh_shell *sh, const char *text, int *incomplete);

/* The prompt text for PS1/PS2 value ps: bash escapes (\w \W \u \h \$ \e \n,
 * \[ \] ignored) and zsh escapes (%~ %/ %c %n %m %# %? %F{c} %f %K{c} %k
 * %B %b %U %u %S %s %%), then parameter, command and arithmetic expansion.
 * $HOME in the directory shows as ~. malloc'ed. */
char *sh_prompt(sh_shell *sh, const char *ps);

/* Report background jobs that have ended ("[1] Done  cmd") on the
 * shell's error stream and forget them; the shell calls it before each
 * prompt. */
void sh_notify(sh_shell *sh);

/* Run a parsed tree with io. */
long sh_exec(sh_shell *sh, const struct sh_node *n, const sh_io *io);

#endif
