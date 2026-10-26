/* amconf_dos: the DOS side of Amarchy's settings file (amconf_core.h). Linked by the commodity, amarchy-gui
 * and amarchy-index. */
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "amconf_dos.h"
#include "../prefs/prefs_dos.h"

static long dos_read(void *ctx, const char *path, char *buf, long cap)
{
    BPTR f = Open((STRPTR)path, MODE_OLDFILE);
    LONG got;
    (void)ctx;
    if (!f)
        return -1;
    got = Read(f, buf, cap);
    Close(f);
    return got < 0 ? 0 : got;
}

static int dos_dir(const prefs_paths *p, int t)
{
    return prefs_dos_dir(p, t);
}

static const amconf_io dos_io = { &prefs_dos_fs, dos_read, dos_dir };

const amconf_io *amconf_dos_io(void)
{
    return &dos_io;
}

int amconf_load(am_prefs *out)
{
    return amconf_load_with(&dos_io, out);
}

int amconf_save(const am_prefs *p, int keep, int *failed)
{
    return amconf_save_with(&dos_io, p, keep, failed);
}

int amconf_save_fields(am_prefs *cur, int *src, const prefs_fields *f, int keep, int *failed)
{
    return amconf_save_fields_with(&dos_io, cur, src, f, keep, failed);
}

int amconf_migrate(void)
{
    return amconf_migrate_with(&dos_io);
}
