/* rgbprobe: does SetRPAttrs RPTAG_PenMode / RPTAG_FgColor / RPTAG_BgColor (Picasso96's 24-bit pens) colour Text
 * and RectFill on this Workbench screen without a pen? Opens a window at 0/0 on it and draws, in 24-bit pen mode,
 * HELLO in 0xFF8000 on 0x0040FF at (10,20), then a RectFill in FgColor 0x00C000 at (10,40) 60x10. Prints the
 * attribute call results; the rig script reads the pixels of the screen shot (W-T13). */
#include <stdio.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/rpattr.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>

#ifndef RPTAG_PenMode
#define RPTAG_PenMode 0x80000080UL
#define RPTAG_FgColor 0x80000081UL
#define RPTAG_BgColor 0x80000082UL
#endif

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

int main(int argc, char **argv)
{
    struct Window *w;
    struct ColorMap *cm;
    int held = 0;
    struct RastPort *rp;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase)
        return 20;
    w = OpenWindowTags(0, WA_Left, 0, WA_Top, 40, WA_Width, 200, WA_Height, 80, WA_Title, (ULONG)"rgbprobe",
                       WA_PubScreenName, (ULONG)"Workbench", WA_Activate, TRUE, TAG_DONE);
    if (!w) {
        printf("no window\n");
        return 10;
    }
    rp = w->RPort;
    cm = w->WScreen->ViewPort.ColorMap;
    /* HOG: take every free pen first, as a Workbench whose map is used up (rig 2 on 2026-10-14) */
    while (argc > 1 && argv[0] && held < 300 && (LONG)ObtainPen(cm, (ULONG)-1, 0, 0, 0, PEN_EXCLUSIVE | PEN_NO_SETCOLOR) >= 0)
        held++;
    printf("held %d pens\n", held);
    SetRPAttrs(rp, RPTAG_PenMode, FALSE, RPTAG_FgColor, 0xFF8000UL, RPTAG_BgColor, 0x0040FFUL, TAG_DONE);
    SetRPAttrs(rp, RPTAG_PenMode, TRUE, TAG_DONE);
    SetDrMd(rp, JAM2);
    Move(rp, 10, 30);
    Text(rp, "HELLO", 5);
    SetRPAttrs(rp, RPTAG_FgColor, 0x00C000UL, TAG_DONE);
    RectFill(rp, 10, 50, 69, 59);
    printf("drawn\n");
    Delay(50 * 20);
    CloseWindow(w);
    return 0;
}
