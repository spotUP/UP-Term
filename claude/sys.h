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
} cl_sys;

#endif
