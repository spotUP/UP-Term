/* flatwin -- FLAT-C1 rig helper (tools/rig/amarchy_flat_rig.py): windows whose close / depth / size / zoom gadgets are
 * sysiclass images, opened and closed in a loop, and a hash of their frame pixels.
 *   flatwin N   open and close N windows on the Workbench screen (each one draws its system gadgets through sysiclass)
 *               and print "flatwin cycles=N hash=<hex>": FNV-1a over every pixel of the first window's title bar and
 *               bottom border (ReadPixel). The windows are opened inactive at a fixed place, so the hash depends only
 *               on what sysiclass drew: the same with and without amarchy-flat's pass-through. */
#include <stdio.h>
#include <stdlib.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
long __stack = 16384;
const char flatwin_stack_cookie[] = "$STACK: 16384";

static struct Window *mkwin(void)
{
    struct TagItem t[12];
    int n = 0;
    t[n].ti_Tag = WA_Left;         t[n++].ti_Data = 40;
    t[n].ti_Tag = WA_Top;          t[n++].ti_Data = 30;
    t[n].ti_Tag = WA_Width;        t[n++].ti_Data = 220;
    t[n].ti_Tag = WA_Height;       t[n++].ti_Data = 90;
    t[n].ti_Tag = WA_Title;        t[n++].ti_Data = (ULONG)"flatwin";
    t[n].ti_Tag = WA_CloseGadget;  t[n++].ti_Data = TRUE;
    t[n].ti_Tag = WA_DepthGadget;  t[n++].ti_Data = TRUE;
    t[n].ti_Tag = WA_SizeGadget;   t[n++].ti_Data = TRUE;
    t[n].ti_Tag = WA_DragBar;      t[n++].ti_Data = TRUE;
    t[n].ti_Tag = WA_Activate;     t[n++].ti_Data = FALSE;
    t[n].ti_Tag = WA_SmartRefresh; t[n++].ti_Data = TRUE;
    t[n].ti_Tag = TAG_DONE;        t[n].ti_Data = 0;
    return OpenWindowTagList(NULL, t);
}

int main(int argc, char **argv)
{
    long cycles = argc > 1 ? atol(argv[1]) : 1, i, x, y;
    unsigned long h = 2166136261UL;
    if (!(IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36)))
        return 10;
    if (!(GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 36)))
        return 10;
    for (i = 0; i < cycles; i++) {
        struct Window *w = mkwin();
        if (!w) {
            printf("flatwin: OpenWindow failed at cycle %ld\n", i);
            return 5;
        }
        if (i == 0) {
            Delay(25);                                   /* the borders are drawn after the open returns */
            for (y = 0; y < w->Height; y++) {
                if (y >= w->BorderTop && y < w->Height - w->BorderBottom)
                    continue;
                for (x = 0; x < w->Width; x++) {
                    h ^= (unsigned long)(ReadPixel(w->RPort, x, y) & 0xFF);
                    h *= 16777619UL;
                }
            }
        }
        CloseWindow(w);
    }
    printf("flatwin cycles=%ld hash=%08lx\n", cycles, h);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
