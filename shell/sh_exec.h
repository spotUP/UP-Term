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
    long  (*write)(void *os, sh_fh fh, const char *buf, long n);
    long  (*read_line)(void *os, sh_fh fh, char *buf, long max); /* -1 at the end */
    int   (*chdir)(void *os, const char *path);    /* 0 = ok */
    int   (*exists)(void *os, const char *path, int want_dir); /* test -e / -f / -d */
    char *(*cwd)(void *os);                        /* malloc'ed */
    void *data;
} sh_os;

typedef struct sh_func {
    char *name;
    const struct sh_node *body; /* in one of sh_shell.kept */
    struct sh_func *next;
} sh_func;

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
    sh_parse kept[16];     /* parses holding function bodies */
    int n_kept, keep_parse;
    int heredocs;          /* numbering for here-document temp files */
} sh_shell;

void sh_shell_init(sh_shell *sh);
void sh_shell_free(sh_shell *sh);

/* Run one input text (a line, or a script). Returns the exit status of
 * its last command; *incomplete is set when the text needs more lines. */
long sh_run_text(sh_shell *sh, const char *text, int *incomplete);

/* Run a parsed tree with io. */
long sh_exec(sh_shell *sh, const struct sh_node *n, const sh_io *io);

#endif
