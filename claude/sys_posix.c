/* sys_posix -- sys.h on POSIX, for the host tests (a temporary tree).
 * run uses popen (no timeout: the tests run short commands). */
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "sys_posix.h"
#include "util.h"

static void set_err(sys_posix *p, const char *what)
{
    cl_copy(p->err, what, sizeof(p->err));
    cl_cat(p->err, ": ", sizeof(p->err));
    cl_cat(p->err, strerror(errno), sizeof(p->err));
}

static int x_read(void *u, const char *path, long max, char **out, long *n)
{
    sys_posix *p = (sys_posix *)u;
    FILE *f = fopen(path, "rb");
    long len;
    char *b;
    if (!f) {
        set_err(p, path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len > max) {
        fclose(f);
        return SYS_TOO_BIG;
    }
    b = (char *)malloc((size_t)len + 1);
    if (!b || (long)fread(b, 1, (size_t)len, f) != len) {
        free(b);
        fclose(f);
        cl_copy(p->err, "read failed", sizeof(p->err));
        return -1;
    }
    fclose(f);
    b[len] = 0;
    *out = b;
    *n = len;
    return 0;
}

static int x_write(void *u, const char *path, const char *s, long n)
{
    sys_posix *p = (sys_posix *)u;
    FILE *f = fopen(path, "wb");
    if (!f) {
        set_err(p, path);
        return -1;
    }
    if ((long)fwrite(s, 1, (size_t)n, f) != n) {
        fclose(f);
        set_err(p, path);
        return -1;
    }
    return fclose(f) ? -1 : 0;
}

static int x_list(void *u, const char *path, cl_dir_fn fn, void *c)
{
    sys_posix *p = (sys_posix *)u;
    DIR *d = opendir(path);
    struct dirent *e;
    if (!d) {
        set_err(p, path);
        return -1;
    }
    while ((e = readdir(d)) != 0) {
        cl_dirent de;
        struct stat st;
        char full[1024];
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        cl_copy(full, path, sizeof(full));
        cl_cat(full, "/", sizeof(full));
        cl_cat(full, e->d_name, sizeof(full));
        memset(&de, 0, sizeof(de));
        cl_copy(de.name, e->d_name, sizeof(de.name));
        if (!stat(full, &st)) {
            de.dir = S_ISDIR(st.st_mode);
            de.size = (long)st.st_size;
        }
        if (fn(c, &de))
            break;
    }
    closedir(d);
    return 0;
}

static int x_kind(void *u, const char *path)
{
    struct stat st;
    (void)u;
    if (stat(path, &st))
        return 0;
    return S_ISDIR(st.st_mode) ? 2 : 1;
}

static int x_canon(void *u, const char *path, char *out, long cap)
{
    char buf[PATH_MAX];
    (void)u;
    if (!realpath(path, buf) || (long)strlen(buf) >= cap)
        return -1;
    strcpy(out, buf);
    return 0;
}

static int x_run(void *u, const char *cmd, int timeout_s, char *out, long cap, long *outn, long *rc)
{
    sys_posix *p = (sys_posix *)u;
    char line[1100];
    FILE *f;
    long n = 0;
    int st;
    (void)timeout_s;
    cl_copy(line, cmd, 1000);
    cl_cat(line, " 2>&1", sizeof(line));
    f = popen(line, "r");
    if (!f) {
        set_err(p, "popen");
        return -1;
    }
    n = (long)fread(out, 1, (size_t)cap, f);
    st = pclose(f);
    *outn = n;
    *rc = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    return 0;
}

static const char *x_err(void *u)
{
    return ((sys_posix *)u)->err;
}

void sys_posix_init(sys_posix *p, cl_sys *s)
{
    memset(p, 0, sizeof(*p));
    s->u = p;
    s->read = x_read;
    s->write = x_write;
    s->list = x_list;
    s->kind = x_kind;
    s->canon = x_canon;
    s->run = x_run;
    s->err = x_err;
}
