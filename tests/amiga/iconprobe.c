/* iconprobe NAME -- what icon.library reads from NAME.info (P8: the
 * UP-Term icon tools/mkicon.py writes). */
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>

struct Library *IconBase;

int main(int argc, char **argv)
{
    struct DiskObject *d;
    char **tt;
    if (argc < 2 || !(IconBase = OpenLibrary((STRPTR)"icon.library", 36)))
        return 20;
    d = GetDiskObject((STRPTR)argv[1]);
    if (!d) {
        Printf("no icon (%ld)\n", IoErr());
        CloseLibrary(IconBase);
        return 10;
    }
    Printf("type %ld stack %ld tool %s image %ldx%ld depth %ld pos %ld,%ld\n", (LONG)d->do_Type,
           d->do_StackSize, d->do_DefaultTool ? d->do_DefaultTool : (STRPTR)"-",
           (LONG)d->do_Gadget.Width, (LONG)d->do_Gadget.Height,
           (LONG)((struct Image *)d->do_Gadget.GadgetRender)->Depth, d->do_CurrentX, d->do_CurrentY);
    for (tt = (char **)d->do_ToolTypes; tt && *tt; tt++)
        Printf("tooltype %s\n", *tt);
    FreeDiskObject(d);
    CloseLibrary(IconBase);
    return 0;
}
