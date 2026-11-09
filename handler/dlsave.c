/* dlsave.c -- see dlsave.h. */
#include <string.h>
#include "dlsave.h"

static int put_leaf(char *out, int *o, int n, const char *s, int max)
{
    int k;
    for (k = 0; s[k] && k < max; k++) {
        unsigned char c = (unsigned char)s[k];
        if (*o + 1 >= n)
            return 0;
        out[(*o)++] = (c < 0x20 || c == '/' || c == ':' || c == 0x7F) ? '_' : (char)c;
    }
    return 1;
}

int dl_path(char *out, int n, const char *host, const char *name)
{
    int o = 0, hl = (int)strlen(DL_ROOT);
    if (n < hl + 4 || !name || !*name) {
        if (n > 0)
            out[0] = 0;
        return 0;
    }
    memcpy(out, DL_ROOT, (size_t)hl);
    o = hl;
    out[o++] = '/';
    if (!put_leaf(out, &o, n, host && *host ? host : "local", DL_LEAF_MAX))
        goto bad;
    if (o + 1 >= n)
        goto bad;
    out[o++] = '/';
    if (!put_leaf(out, &o, n, name, DL_LEAF_MAX))
        goto bad;
    out[o] = 0;
    return 1;
bad:
    out[0] = 0;
    return 0;
}

void dl_host_of_cwd(const char *cwd, char *out, int n)
{
    int o = 0;
    if (n <= 0)
        return;
    out[0] = 0;
    if (!cwd || strncmp(cwd, "file://", 7) != 0)
        return;
    for (cwd += 7; *cwd && *cwd != '/' && o + 1 < n; cwd++)
        out[o++] = *cwd;
    out[o] = 0;
}

int dl_save_with(const dl_os *os, const char *host, const char *name, const vt_u8 *data, long len)
{
    char path[160];
    int i, slashes = 0;
    if (!dl_path(path, (int)sizeof path, host, name))
        return 0;
    /* each level below the volume: "ENVARC:UP-Term", ".../Downloads", ".../<host>" */
    for (i = 0; path[i]; i++) {
        if (path[i] != '/')
            continue;
        if (++slashes > 3)
            break;
        path[i] = 0;
        if (!os->mkdir(path)) {
            path[i] = '/';
            return 0;
        }
        path[i] = '/';
    }
    return os->write(path, data, len);
}

#ifdef DLSAVE_DOS
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>

/* the CreateDir pattern of complete.c co_save: Lock first, CreateDir when missing */
static int dos_mkdir(const char *path)
{
    BPTR l = Lock((STRPTR)path, ACCESS_READ);
    if (!l)
        l = CreateDir((STRPTR)path);
    if (!l)
        return 0;
    UnLock(l);
    return 1;
}

static int dos_write(const char *path, const vt_u8 *data, long len)
{
    BPTR f = Open((STRPTR)path, MODE_NEWFILE);
    long n;
    if (!f)
        return 0;
    n = Write(f, (APTR)data, len);
    Close(f);
    if (n != len) {
        DeleteFile((STRPTR)path); /* cut short: gone rather than half */
        return 0;
    }
    return 1;
}

static const dl_os dos_os = { dos_mkdir, dos_write };

int dl_save(const char *host, const char *name, const vt_u8 *data, long len)
{
    return dl_save_with(&dos_os, host, name, data, len);
}
#endif
