/* gthemes_cur_dos: the small DOS half of the theme loader (read a theme file, name the current theme, load it), split out of
 * gthemes_dos.c so amarchy-flat can read the current theme at its start (FLAT-BOOT-THEME-FIELDS) without linking the
 * Directory Opus, Workbench-pen and requester code of gthemes_dos.c. Declarations: gthemes_dos.h. */
#include <string.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include "gthemes_dos.h"
#include "amconf_dos.h"

long gt_dos_read(void *ctx, const char *path, char *buf, long cap)
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

void gt_dos_load(const char *name, gtheme *out)
{
    gt_load(gt_dos_read, 0, name, out);
}

void gt_dos_current(char *out, int cap)
{
    am_prefs *ap;
    const char *v;
    ap = amconf_borrow();
    v = ap->val[AM_GTHEME];
    if (!*v || (int)strlen(v) >= cap)
        v = GT_DEFAULT;
    strcpy(out, v);
    amconf_return(ap);
}

void gt_dos_load_current(gtheme *out)
{
    char name[GT_NAME];
    gt_dos_current(name, sizeof name);
    gt_dos_load(name, out);
}
