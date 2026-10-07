/* vw_plat for the host (macOS, Linux): stdio, isatty, TIOCGWINSZ. */
#define _POSIX_C_SOURCE 200112L
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include "vw_plat.h"

const int vw_fail_code = 1;

struct vw_file {
    FILE *f;
    int is_stdin;
};

static int failed;

vw_file *vw_open(const char *name)
{
    vw_file *f = (vw_file *)malloc(sizeof(vw_file));
    if (!f)
        return 0;
    f->is_stdin = !name || !strcmp(name, "-");
    f->f = f->is_stdin ? stdin : fopen(name, "rb");
    if (!f->f) {
        free(f);
        return 0;
    }
    return f;
}

long vw_read(vw_file *f, char *buf, long n)
{
    size_t k = fread(buf, 1, (size_t)n, f->f);
    if (k == 0 && ferror(f->f))
        return -1;
    return (long)k;
}

void vw_close(vw_file *f)
{
    if (!f->is_stdin)
        fclose(f->f);
    free(f);
}

void vw_write(void *u, const char *s, long n)
{
    (void)u;
    if (fwrite(s, 1, (size_t)n, stdout) != (size_t)n)
        failed = 1;
}

int vw_write_failed(void)
{
    if (fflush(stdout))
        failed = 1;
    return failed;
}

void vw_say(const char *s)
{
    fputs(s, stderr);
}

int vw_out_is_tty(void)
{
    return isatty(1);
}

int vw_columns(void)
{
    struct winsize ws;
    int fd;
    /* standard output's terminal, else standard error's or input's (a pipe
     * into a pager has no width, the terminal around it has) */
    for (fd = 1; fd != 3; fd = fd == 1 ? 2 : fd == 2 ? 0 : 3)
        if (ioctl(fd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
            return ws.ws_col;
    return 0;
}

int vw_env(const char *name, char *buf, int n)
{
    const char *v = getenv(name);
    if (!v || n <= 0)
        return 0;
    strncpy(buf, v, (size_t)n - 1);
    buf[n - 1] = 0;
    return 1;
}

int vw_break(void)
{
    return 0;
}

/* the real cat: the first cat on PATH that is not this program (same device and inode as argv[0]) */
static int is_self(const char *path, const char *argv0)
{
    struct stat a, b;
    const char *p, *e;
    char full[1024];
    if (stat(path, &a))
        return 0;
    if (strchr(argv0, '/'))
        return !stat(argv0, &b) && a.st_dev == b.st_dev && a.st_ino == b.st_ino;
    p = getenv("PATH");
    for (; p && *p; p = *e ? e + 1 : e) {
        size_t l;
        e = strchr(p, ':');
        if (!e)
            e = p + strlen(p);
        l = (size_t)(e - p);
        if (l + strlen(argv0) + 2 > sizeof(full))
            continue;
        memcpy(full, p, l);
        full[l] = 0;
        strcat(full, l ? "/" : "./");
        strcat(full, argv0);
        if (!stat(full, &b) && a.st_dev == b.st_dev && a.st_ino == b.st_ino)
            return 1;
    }
    return 0;
}

int vw_exec_cat(int argc, char **argv)
{
    const char *p = getenv("PATH");
    const char *e;
    char full[1024];
    char **av;
    int i, n = 0;
    if (getenv("HL_EXEC_CAT")) {
        vw_say("hl: no cat other than hl found\n");
        return vw_fail_code;
    }
    av = (char **)malloc(sizeof(char *) * (argc + 2));
    if (!av)
        return vw_fail_code;
    av[n++] = (char *)"cat";
    for (i = 1; i < argc; i++)
        if (strcmp(argv[i], "-p") && strcmp(argv[i], "--plain"))
            av[n++] = argv[i];
    av[n] = 0;
    setenv("HL_EXEC_CAT", "1", 1);
    for (; p && *p; p = *e ? e + 1 : e) {
        size_t l;
        e = strchr(p, ':');
        if (!e)
            e = p + strlen(p);
        l = (size_t)(e - p);
        if (l + 5 > sizeof(full))
            continue;
        memcpy(full, p, l);
        full[l] = 0;
        strcat(full, l ? "/cat" : "./cat");
        if (access(full, X_OK) || is_self(full, argv[0]))
            continue;
        execv(full, av);
    }
    free(av);
    vw_say("hl: cannot run cat\n");
    return vw_fail_code;
}
