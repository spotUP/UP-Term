/* struttest: BAR-STRIP-RESERVE probe and rig opener. Opens a window on the Workbench screen and prints the box it got.
 *   struttest <left> <top> <width> <height> [flags] [hold]
 * flags (letters): s size gadget, d drag bar, z ZipWindow twice (prints the alternate box and the box after each), n open with
 * NewWindow/OpenWindow (no tags), b borderless (no drag bar, no size gadget), i height as WA_InnerHeight, g WA_Zoom = the whole screen below the title bar.
 * hold = seconds the window stays open (default 2). Output lines: "BOX l t w h" after open, "ZIP l t w h" per zip,
 * "SCR w h" the screen size. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct IntuitionBase *IntuitionBase;
const long __stack = 65536;
const char struttest_stack_cookie[] = "$STACK:65536";

static void show(const char *tag, struct Window *w)
{
    printf("%s %d %d %d %d\n", tag, (int)w->LeftEdge, (int)w->TopEdge, (int)w->Width, (int)w->Height);
}

int main(int argc, char **argv)
{
    struct Window *w;
    struct Screen *s;
    const char *fl = argc > 5 ? argv[5] : "";
    int hold = argc > 6 ? atoi(argv[6]) : 2;
    int l, t, wd, h;
    if (argc < 5)
        return 5;
    l = atoi(argv[1]); t = atoi(argv[2]); wd = atoi(argv[3]); h = atoi(argv[4]);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
    if (!IntuitionBase)
        return 20;
    s = LockPubScreen("Workbench");
    if (!s)
        return 10;
    printf("SCR %d %d\n", (int)s->Width, (int)s->Height);
    if (strchr(fl, 'n')) {
        struct NewWindow nw;
        memset(&nw, 0, sizeof nw);
        nw.LeftEdge = l; nw.TopEdge = t; nw.Width = wd; nw.Height = h;
        nw.DetailPen = 255; nw.BlockPen = 255;
        nw.MinWidth = 60; nw.MinHeight = 40; nw.MaxWidth = 0xFFFF; nw.MaxHeight = 0xFFFF;
        nw.Title = (UBYTE *)"struttest";
        nw.Type = WBENCHSCREEN;
        nw.Flags = WFLG_ACTIVATE;
        if (strchr(fl, 's')) nw.Flags |= WFLG_SIZEGADGET;
        if (strchr(fl, 'd')) nw.Flags |= WFLG_DRAGBAR;
        if (strchr(fl, 'b')) nw.Flags |= WFLG_BORDERLESS;
        w = OpenWindow(&nw);
    } else {
        int sg = strchr(fl, 's') != 0, dg = strchr(fl, 'd') != 0, bl = strchr(fl, 'b') != 0;
        WORD zoomfull[4];
        ULONG hk = strchr(fl, 'i') ? WA_InnerHeight : WA_Height;
        zoomfull[0] = 0; zoomfull[1] = 11; zoomfull[2] = (WORD)s->Width; zoomfull[3] = (WORD)(s->Height - 11);
        w = OpenWindowTags(0, WA_Left, l, WA_Top, t, WA_Width, wd, hk, h, WA_Title, (ULONG)"struttest", WA_PubScreen, (ULONG)s,
                           WA_SizeGadget, sg, WA_DragBar, dg, WA_Borderless, bl, WA_MinWidth, 60, WA_MinHeight, 40,
                           WA_MaxWidth, 0xFFFF, WA_MaxHeight, 0xFFFF, WA_Activate, 1,
                           strchr(fl, 'g') ? WA_Zoom : TAG_IGNORE, (ULONG)zoomfull, TAG_DONE);
    }
    UnlockPubScreen(0, s);
    if (!w) {
        printf("FAIL\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return 10;
    }
    show("BOX", w);
    if (strchr(fl, 'z')) {
        ZipWindow(w); Delay(25); show("ZIP", w);
        ZipWindow(w); Delay(25); show("ZIP", w);
    }
    Delay(50L * hold);
    CloseWindow(w);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
