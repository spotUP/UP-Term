/* penprobe [SCREEN]: what a window on a public screen (default Workbench) can get for direct colours, the
 * question vtcon's renderer asks at a window's start (render/amiga_render.c: two exclusive scratch pens make
 * it a true-colour window). One line:
 *   depth D entries N exclusive P1 P2 exact P (215,135,0) shared-free F */
#include <stdio.h>
#include <exec/types.h>
#include <intuition/screens.h>
#include <graphics/view.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#include <proto/intuition.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

int main(int argc, char **argv)
{
    struct Screen *s;
    struct ColorMap *cm;
    LONG a, b, e, free = 0, p;
    static LONG got[256];
    static const struct TagItem exact[] = { { OBP_Precision, (ULONG)PRECISION_EXACT }, { OBP_FailIfBad, TRUE }, { TAG_DONE, 0 } };
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase)
        return 20;
    s = LockPubScreen((STRPTR)(argc > 1 ? argv[1] : 0));
    if (!s) {
        printf("no screen\n");
        return 10;
    }
    cm = s->ViewPort.ColorMap;
    a = ObtainPen(cm, (ULONG)-1, 0, 0, 0, PEN_EXCLUSIVE | PEN_NO_SETCOLOR);
    b = ObtainPen(cm, (ULONG)-1, 0, 0, 0, PEN_EXCLUSIVE | PEN_NO_SETCOLOR);
    e = ObtainBestPenA(cm, 215 * 0x01010101UL, 135 * 0x01010101UL, 0, (struct TagItem *)exact);
    while (free < 256 && (p = ObtainPen(cm, (ULONG)-1, 0, 0, 0, 0)) >= 0)
        got[free++] = p;
    printf("depth %ld entries %ld exclusive %ld %ld exact %ld shared-free %ld\n",
           (long)GetBitMapAttr(s->RastPort.BitMap, BMA_DEPTH), (long)cm->Count, (long)a, (long)b, (long)e, (long)free);
    while (free > 0)
        ReleasePen(cm, (ULONG)got[--free]);
    if (e >= 0)
        ReleasePen(cm, (ULONG)e);
    if (b >= 0)
        ReleasePen(cm, (ULONG)b);
    if (a >= 0)
        ReleasePen(cm, (ULONG)a);
    UnlockPubScreen(0, s);
    return 0;
}
