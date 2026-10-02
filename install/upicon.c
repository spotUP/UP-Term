/* upicon -- the kit's icon tool (Installer): a Shell icon's WINDOW tooltype
 * pointed at another console device, the window itself unchanged.
 *
 *   upicon NAME WINDOWDEV=XCON
 *
 * NAME is the icon without .info (SYS:System/Shell). The WINDOW tooltype's
 * device is swapped (iconspec.c); an icon with none gets the Shell's
 * default window on that device. Every other tooltype stays. Return code
 * 0 changed, 5 already so, 10 no such icon, 20 failed. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <string.h>
#include "iconspec.h"

struct Library *IconBase;

static int is_window(const char *t)
{
    static const char k[] = "WINDOW=";
    int i;
    for (i = 0; k[i]; i++)
        if ((t[i] | 0x20) != (k[i] | 0x20) && !(k[i] == '=' && t[i] == '='))
            return 0;
    return 1;
}

int main(void)
{
    LONG args[2] = { 0, 0 };
    struct RDArgs *rd = ReadArgs((STRPTR)"NAME/A,WINDOWDEV/K/A", args, 0);
    struct DiskObject *d;
    STRPTR *old, *tt = 0;
    char value[256 + 8];
    const char *was = 0;
    int n = 0, i, at = -1, r, rc = 20;
    if (!rd) {
        PrintFault(IoErr(), (STRPTR)"upicon");
        return 20;
    }
    if (!(IconBase = OpenLibrary((STRPTR)"icon.library", 36))) {
        FreeArgs(rd);
        return 20;
    }
    d = GetDiskObject((STRPTR)args[0]);
    if (!d) {
        Printf("upicon: no icon %s.info\n", args[0]);
        rc = 10;
        goto out;
    }
    old = d->do_ToolTypes;
    for (n = 0; old && old[n]; n++)
        if (at < 0 && is_window((const char *)old[n])) {
            at = n;
            was = (const char *)old[n] + 7;
        }
    strcpy(value, "WINDOW=");
    r = iconspec_window_device(was, (const char *)args[1], value + 7, (int)sizeof(value) - 7);
    if (r < 0) {
        Printf("upicon: the WINDOW tooltype is too long\n");
        goto free;
    }
    if (r == 0) {
        rc = 5; /* already that device */
        goto free;
    }
    tt = (STRPTR *)AllocVec((n + 2) * sizeof(STRPTR), MEMF_CLEAR);
    if (!tt)
        goto free;
    for (i = 0; i < n; i++)
        tt[i] = old[i];
    tt[at >= 0 ? at : n] = (STRPTR)value;
    d->do_ToolTypes = tt;
    rc = PutDiskObject((STRPTR)args[0], d) ? 0 : 20;
    if (rc)
        PrintFault(IoErr(), (STRPTR)"upicon");
    d->do_ToolTypes = old; /* FreeDiskObject frees what GetDiskObject gave */
    FreeVec(tt);
free:
    FreeDiskObject(d);
out:
    CloseLibrary(IconBase);
    FreeArgs(rd);
    return rc;
}
