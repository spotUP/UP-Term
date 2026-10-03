/* dripens NAME: a public screen's DrawInfo pens and the colour of each,
 * two lines (ownscreen_rig: UP-Term's frames in the Workbench's colours):
 *   pens: 16 17 17 18 ...
 *   rgb: 959595 000000 000000 ffffff ... */
#include <exec/types.h>
#include <intuition/screens.h>
#include <graphics/view.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

int main(int argc, char **argv)
{
    struct Screen *s;
    struct DrawInfo *di;
    int i, n;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase)
        return 20;
    s = LockPubScreen((UBYTE *)(argc > 1 ? argv[1] : 0));
    if (!s)
        return 20;
    di = GetScreenDrawInfo(s);
    if (di) {
        n = di->dri_NumPens < 12 ? di->dri_NumPens : 12;
        Printf((STRPTR)"pens:");
        for (i = 0; i < n; i++)
            Printf((STRPTR)" %ld", (LONG)di->dri_Pens[i]);
        Printf((STRPTR)"\nrgb:");
        for (i = 0; i < n; i++) {
            ULONG c[3];
            GetRGB32(s->ViewPort.ColorMap, di->dri_Pens[i], 1, c);
            Printf((STRPTR)" %02lx%02lx%02lx", c[0] >> 24, c[1] >> 24, c[2] >> 24);
        }
        Printf((STRPTR)"\n");
        FreeScreenDrawInfo(s, di);
    }
    UnlockPubScreen(0, s);
    return 0;
}
