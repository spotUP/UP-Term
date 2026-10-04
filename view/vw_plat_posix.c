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
    if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
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
