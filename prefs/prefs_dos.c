/* prefs_dos (see prefs_dos.h). */
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include "prefs_dos.h"

static long g_last_err;

long prefs_dos_last_error(void)
{
    return g_last_err;
}

static int dos_write(void *ctx, const char *path, const char *buf, long len)
{
    BPTR f = Open((STRPTR)path, MODE_NEWFILE);
    LONG w, closed;
    (void)ctx;
    if (!f) {
        g_last_err = IoErr();
        return 0;
    }
    w = Write(f, (APTR)buf, len);
    if (w != len)
        g_last_err = IoErr();
    closed = Close(f); /* a buffered write can fail only here */
    if (w == len && !closed)
        g_last_err = IoErr();
    return w == len && closed;
}

static int dos_exists(void *ctx, const char *path)
{
    BPTR l = Lock((STRPTR)path, SHARED_LOCK);
    (void)ctx;
    if (!l)
        return 0;
    UnLock(l);
    return 1;
}

static int dos_remove(void *ctx, const char *path)
{
    (void)ctx;
    if (DeleteFile((STRPTR)path))
        return 1;
    g_last_err = IoErr();
    return g_last_err == ERROR_OBJECT_NOT_FOUND;
}

static int dos_rename(void *ctx, const char *from, const char *to)
{
    (void)ctx;
    if (Rename((STRPTR)from, (STRPTR)to))
        return 1;
    g_last_err = IoErr();
    return 0;
}

const prefs_fs prefs_dos_fs = { 0, dos_write, dos_exists, dos_remove, dos_rename };

int prefs_dos_dir(const prefs_paths *p, int t)
{
    BPTR l = CreateDir((STRPTR)p->dir[t]); /* fails when it exists: fine */
    if (l)
        UnLock(l);
    l = Lock((STRPTR)p->dir[t], SHARED_LOCK);
    if (!l) {
        g_last_err = IoErr();
        return 0;
    }
    UnLock(l);
    return 1;
}

static int dos_save_one(const prefs_paths *p, int t, const char *buf, long len)
{
    if (!prefs_dos_dir(p, t))
        return PREFS_DOS_NODIR;
    return prefs_install_target_at(&prefs_dos_fs, p, t, buf, len);
}

int prefs_dos_save(const char *buf, long len, int keep, int *failed, int hlcat)
{
    int t, r;
    for (t = keep ? PREFS_T_ENVARC : PREFS_T_ENV; t >= PREFS_T_ENV; t--) {
        *failed = t;
        r = dos_save_one(&prefs_paths_upterm, t, buf, len);
        if (r != PREFS_INSTALL_OK)
            return r;
        r = prefs_write_hlcat(&prefs_dos_fs, t, hlcat);
        if (r != PREFS_INSTALL_OK)
            return r;
    }
    return PREFS_INSTALL_OK;
}

void prefs_dos_theme_drawer(const char *given, const char *home, char *out, int cap)
{
    prefs_theme_drawer(&prefs_dos_fs, given, home, out, cap);
}
