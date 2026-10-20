/* upicon -- the kit's icon tool (Installer): a Shell icon's WINDOW tooltype
 * pointed at another console device, the window itself unchanged, and
 * with TOOL its default tool another shell.
 *
 *   upicon NAME WINDOWDEV=XCON [TOOL=UP-Term:bin/vsh]
 *
 * NAME is the icon without .info (SYS:System/Shell). The WINDOW tooltype's
 * device is swapped (iconspec.c); an icon with none gets the Shell's
 * default window on that device. Every other tooltype stays. TOOL makes
 * the icon a project whose default tool is that program (vsh opens the
 * WINDOW tooltype's window itself when Workbench starts it). Return code
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
#include "../tty/qmark.h"

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
    static const char tmpl[] = "NAME/A,WINDOWDEV/K/A,TOOL/K";
    LONG args[3] = { 0, 0, 0 };
    struct RDArgs *rd;
    struct DiskObject *d;
    STRPTR *old, *tt = 0, oldtool;
    UBYTE oldtype;
    char value[256 + 8];
    const char *was = 0;
    int n = 0, i, at = -1, r, rc = 20;
    if (qmark_line((const char *)GetArgStr())) {
        PutStr((STRPTR)tmpl);
        PutStr((STRPTR)"\n");
        return RETURN_OK;
    }
    rd = ReadArgs((STRPTR)tmpl, args, 0);
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
    oldtool = d->do_DefaultTool;
    oldtype = d->do_Type;
    if (args[2] && (d->do_Type != WBPROJECT || !oldtool || strcmp((const char *)oldtool, (const char *)args[2]))) {
        d->do_DefaultTool = (STRPTR)args[2];
        d->do_Type = WBPROJECT;
    } else if (r == 0) {
        rc = 5; /* already that device and that tool */
        goto free;
    }
    tt = (STRPTR *)AllocVec((n + 2) * sizeof(STRPTR), MEMF_CLEAR);
    if (!tt)
        goto free;
    for (i = 0; i < n; i++)
        tt[i] = old[i];
    if (r)
        tt[at >= 0 ? at : n] = (STRPTR)value;
    d->do_ToolTypes = tt;
    rc = PutDiskObject((STRPTR)args[0], d) ? 0 : 20;
    if (rc)
        PrintFault(IoErr(), (STRPTR)"upicon");
    d->do_ToolTypes = old; /* FreeDiskObject frees what GetDiskObject gave */
    d->do_DefaultTool = oldtool;
    d->do_Type = oldtype;
    FreeVec(tt);
free:
    FreeDiskObject(d);
out:
    CloseLibrary(IconBase);
    FreeArgs(rd);
    return rc;
}
