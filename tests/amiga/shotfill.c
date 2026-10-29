/* shotfill: paints two known colours on the front (RTG truecolour) Workbench screen with cgx FillPixelArray, for the deep
 * capture proof (tools/rig/amarchy_cap_rig.py). Rect 1: x 100..115, y 100..107 = 0x00FF8040. Rect 2: x 300..303,
 * y 200..203 = 0x001020FF. */
#include <stdio.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>

long __stack = 65536;
const char *shotfill_cookie = "$STACK:65536";

struct IntuitionBase *IntuitionBase;
void cgx_fill(__reg("a6") struct Library *, __reg("a1") struct RastPort *rp, __reg("d0") UWORD x, __reg("d1") UWORD y,
              __reg("d2") UWORD w, __reg("d3") UWORD h, __reg("d4") ULONG argb) = "\tjsr\t-150(a6)";

int main(void)
{
    struct Library *cgx;
    struct Screen *sc;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    cgx = OpenLibrary("cybergraphics.library", 41);
    if (!IntuitionBase || !cgx) return 20;
    sc = LockPubScreen(0);
    if (!sc) return 20;
    cgx_fill(cgx, &sc->RastPort, 100, 100, 16, 8, 0x00FF8040UL);
    cgx_fill(cgx, &sc->RastPort, 300, 200, 4, 4, 0x001020FFUL);
    printf("shotfill %ldx%ld\n", (long)sc->Width, (long)sc->Height);
    UnlockPubScreen(0, sc);
    CloseLibrary(cgx);
    return 0;
}
