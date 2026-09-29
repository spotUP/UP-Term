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
} sh_io;

#define SH_OWN_IN  1
#define SH_OWN_OUT 2
#define SH_OWN_ERR 4

#define SH_OPEN_READ   0
#define SH_OPEN_WRITE  1   /* create / truncate */
#define SH_OPEN_APPEND 2

struct sh_shell;

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
    int   (*exists)(void *os, const char *path, int want_dir); /* test -e / -f / -d */
    char *(*cwd)(void *os);                        /* malloc'ed */
    void *data;
} sh_os;

typedef struct sh_func {
    char *name;
    sh_parse body;         /* its own copy of the body (body.tree) */
    int busy;              /* running: a redefinition retires the old body */
    struct sh_func *next;
} sh_func;

typedef struct sh_retired {
    sh_parse p;            /* a function body replaced while it ran */
    struct sh_retired *next;
} sh_retired;

typedef struct sh_shell {
    sh_ctx ctx;
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
    sh_retired *retired;   /* freed with the shell */
    int intr;              /* Ctrl-C: unwinding to the prompt */
    unsigned long stack_limit; /* the OS layer's: below this stack address the
                               * interpreter stops ("nested too deeply"), 0 = none */
    int subst_ran;         /* a $( ) ran in the current command ... */
    long subst_status;     /* ... with this status (assignments only: the command's $?) */
    int heredocs;          /* numbering for here-document temp files */
} sh_shell;

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
