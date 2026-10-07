/* prefs_dos (see prefs_dos.h). */
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include "prefs_dos.h"

const char *const prefs_conf_dir[2] = { "ENV:up-term", "ENVARC:up-term" };
const char *const prefs_conf_path[2] = { "ENV:up-term/up-term", "ENVARC:up-term/up-term" };
static const char *const conf_tmp[2] = { "ENV:up-term/up-term.new", "ENVARC:up-term/up-term.new" };
static const char *const conf_orig[2] = { "ENV:up-term/up-term.orig", "ENVARC:up-term/up-term.orig" };

static int dos_write(void *ctx, const char *path, const char *buf, long len)
{
    BPTR f = Open((STRPTR)path, MODE_NEWFILE);
    LONG w, closed;
    (void)ctx;
    if (!f)
        return 0;
    w = Write(f, (APTR)buf, len);
    closed = Close(f); /* a buffered write can fail only here */
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
    return IoErr() == ERROR_OBJECT_NOT_FOUND;
}

static int dos_rename(void *ctx, const char *from, const char *to)
{
    (void)ctx;
    return Rename((STRPTR)from, (STRPTR)to) ? 1 : 0;
}

const prefs_fs prefs_dos_fs = { 0, dos_write, dos_exists, dos_remove, dos_rename };

int prefs_dos_dir(int t)
{
    BPTR l = CreateDir((STRPTR)prefs_conf_dir[t]); /* fails when it exists: fine */
    if (l)
        UnLock(l);
    l = Lock((STRPTR)prefs_conf_dir[t], SHARED_LOCK);
    if (!l)
        return 0;
    UnLock(l);
    return 1;
}

int prefs_dos_save(const char *buf, long len, int keep, int *failed, int hlcat)
{
    int t, r;
    for (t = keep ? PREFS_T_ENVARC : PREFS_T_ENV; t >= PREFS_T_ENV; t--) {
        *failed = t;
        if (!prefs_dos_dir(t))
            return PREFS_DOS_NODIR;
        r = prefs_install(&prefs_dos_fs, prefs_conf_path[t], conf_tmp[t], conf_orig[t], buf, len);
        if (r != PREFS_INSTALL_OK)
            return r;
        {   /* the variable is a few bytes: written in place, no backup */
            char path[48];
            const char *v = prefs_hlcat_text(hlcat);
            strcpy(path, prefs_conf_dir[t]);
            strcat(path, "/" PREFS_HLCAT_VAR);
            if (!dos_write(0, path, v, (long)strlen(v)))
                return PREFS_INSTALL_WRITE;
        }
    }
    return PREFS_INSTALL_OK;
}

void prefs_dos_theme_drawer(const char *given, const char *home, char *out, int cap)
{
    prefs_theme_drawer(&prefs_dos_fs, given, home, out, cap);
}
