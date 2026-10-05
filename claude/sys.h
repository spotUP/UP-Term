/* sys -- what the Claude client's tools need from the machine: files,
 * directories, the canonical name of a path, and running a command.
 * sys_amiga.c is AmigaDOS (commands through vsh); sys_posix.c is the host
 * build's, for the tests on a temporary tree. */
#ifndef CL_SYS_H
#define CL_SYS_H

typedef struct cl_dirent {
    char name[108];
    int dir;
    long size;
    long mtime;                 /* seconds since some fixed day, 0 unknown */
} cl_dirent;

typedef int (*cl_dir_fn)(void *c, const cl_dirent *e);   /* nonzero stops */

#define SYS_TOO_BIG  (-2)
#define SYS_TIMEOUT  (-2)
#define SYS_BREAK    (-3)

typedef struct cl_sys {
    void *u;
    /* A whole file, malloc'ed and terminated, *n its length: 0, -1
     * (err() says why) or SYS_TOO_BIG when longer than max. */
    int (*read)(void *u, const char *path, long max, char **out, long *n);
    /* Creates or replaces a file: 0, -1. */
    int (*write)(void *u, const char *path, const char *s, long n);
    /* The entries of a directory: 0, -1. */
    int (*list)(void *u, const char *path, cl_dir_fn fn, void *c);
    /* 1 a file, 2 a directory, 0 nothing there */
    int (*kind)(void *u, const char *path);
    /* The canonical name of an existing object (assigns and links
     * resolved): 0, -1. */
    int (*canon)(void *u, const char *path, char *out, long cap);
    /* A command line, its output (and errors) captured up to cap bytes,
     * *rc its return code: 0 ran, -1 did not start, SYS_TIMEOUT,
     * SYS_BREAK (Ctrl+C; the command was sent a break). */
    int (*run)(void *u, const char *cmd, int timeout_s, char *out, long cap, long *outn, long *rc);
    const char *(*err)(void *u);
    /* ---- ledger A4 WP2; each may be 0 (the feature is then refused) ---- */
    /* the time an object was last changed, seconds since some fixed day: -1 unknown */
    long (*mtime)(void *u, const char *path);
    /* A command started in the background (Bash run_in_background), its
     * output and errors going to a file: 0 with *job set, -1. */
    int (*bg_start)(void *u, const char *cmd, long *job);
    /* Its output from byte from on (up to cap): 0 with *outn; *running 1
     * while it runs, else 0 with its return code in *rc; -1 no such job. */
    int (*bg_read)(void *u, long job, long from, char *out, long cap, long *outn, int *running, long *rc);
    /* a break sent to it (Ctrl+C; a POSIX host sends SIGINT then SIGKILL): 0, -1 */
    int (*bg_kill)(void *u, long job);
    /* forgotten: its files removed once it has ended (a running one is left alone) */
    void (*bg_drop)(void *u, long job);
} cl_sys;

#endif
