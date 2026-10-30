/* sysiprobe -- FLAT-R3: the Hook layout of intuition's sysiclass (plan 2026-10-29-amarchy-flat-ui-patch.md).
 *   sysiprobe   create a sysiclass image (SYSIA_DrawInfo of the Workbench screen, SYSIA_Which CLOSEIMAGE), then print
 *               CLASS <addr> id=<cl_ID> super=<addr> and HOOK entry=<h_Entry> sub=<h_SubEntry> data=<h_Data> for it,
 *               HEX <addr>: 32 bytes at h_Entry, and the same HOOK line for imageclass, frameiclass and gadgetclass.
 *   sysiprobe ALL  also create one image per SYSIA_Which (1..13) and print whether the class pointer is the same. */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/imageclass.h>
#include <intuition/screens.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/intuition.h>

struct IntuitionBase *IntuitionBase;
long __stack = 8192;
const char sysiprobe_stack_cookie[] = "$STACK: 8192";

static Object *mk(const char *cls, struct DrawInfo *di, ULONG which)
{
    struct TagItem t[3];
    t[0].ti_Tag = SYSIA_DrawInfo; t[0].ti_Data = (ULONG)di;
    t[1].ti_Tag = SYSIA_Which; t[1].ti_Data = which;
    t[2].ti_Tag = TAG_DONE; t[2].ti_Data = 0;
    return NewObjectA(NULL, (UBYTE *)cls, which ? t : t + 2);
}

static void hook_line(const char *tag, struct IClass *c)
{
    struct Hook *h = &c->cl_Dispatcher;
    printf("HOOK %s class=%08lx id=%s super=%08lx entry=%08lx sub=%08lx data=%08lx\n", tag, (unsigned long)c,
           c->cl_ID ? (const char *)c->cl_ID : "(null)", (unsigned long)c->cl_Super, (unsigned long)h->h_Entry,
           (unsigned long)h->h_SubEntry, (unsigned long)h->h_Data);
}

static void hex(unsigned long addr, int n)
{
    const unsigned char *p = (const unsigned char *)addr;
    int i;
    printf("HEX %08lx:", addr);
    for (i = 0; i < n; i++)
        printf(" %02x", p[i]);
    printf("\n");
}

int main(int argc, char **argv)
{
    struct Screen *s;
    struct DrawInfo *di;
    Object *o;
    struct IClass *c;
    static const char *names[] = { "imageclass", "frameiclass", "gadgetclass", "propgclass" };
    int i, w;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    if (!IntuitionBase)
        return 20;
    printf("INTUITION %ld.%ld\n", (long)IntuitionBase->LibNode.lib_Version, (long)IntuitionBase->LibNode.lib_Revision);
    s = LockPubScreen(NULL);
    di = s ? GetScreenDrawInfo(s) : NULL;
    if (!di)
        return 10;
    o = mk("sysiclass", di, CLOSEIMAGE);
    if (!o) {
        printf("NOOBJ\n");
    } else {
        c = OCLASS(o);
        hook_line("sysiclass", c);
        hex((unsigned long)c->cl_Dispatcher.h_Entry, 32);
        if (c->cl_Dispatcher.h_SubEntry)
            hex((unsigned long)c->cl_Dispatcher.h_SubEntry, 32);
        DisposeObject(o);
    }
    for (i = 0; i < 4; i++) {
        o = mk(names[i], di, 0);
        if (o) {
            hook_line(names[i], OCLASS(o));
            DisposeObject(o);
        }
    }
    if (argc > 1 && !strcmp(argv[1], "ALL")) {
        struct IClass *first = NULL;
        for (w = 0; w <= 24; w++) {
            o = mk("sysiclass", di, w);
            if (o) {
                if (!first)
                    first = OCLASS(o);
                printf("WHICH %d class=%08lx same=%d\n", w, (unsigned long)OCLASS(o), OCLASS(o) == first);
                DisposeObject(o);
            } else
                printf("WHICH %d NOOBJ\n", w);
        }
    }
    FreeScreenDrawInfo(s, di);
    UnlockPubScreen(NULL, s);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
