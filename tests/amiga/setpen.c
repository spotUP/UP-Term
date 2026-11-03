/* setpen -- rig helper: SetRGB32 one palette pen of the Workbench screen, to simulate a palette load or another program
 * recolouring a pen the flat skin must not draw with (S33-PREFS-SKIN-FAIL).
 *   setpen <pen> <rrggbb>        prints "OK <pen> <rrggbb>" or "ERROR ..." */
#include <stdio.h>
#include <stdlib.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

int main(int argc, char **argv)
{
    struct Screen *scr;
    unsigned long pen, c;
    if (argc < 3) {
        printf("ERROR usage: setpen <pen> <rrggbb>\n");
        return 10;
    }
    pen = strtoul(argv[1], NULL, 10);
    c = strtoul(argv[2], NULL, 16);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 36);
    if (!IntuitionBase || !GfxBase) {
        printf("ERROR libraries\n");
        return 20;
    }
    scr = LockPubScreen((STRPTR)"Workbench");
    if (!scr) {
        printf("ERROR no Workbench screen\n");
        return 20;
    }
    SetRGB32(&scr->ViewPort, pen, ((c >> 16) & 255) * 0x01010101UL, ((c >> 8) & 255) * 0x01010101UL, (c & 255) * 0x01010101UL);
    UnlockPubScreen(NULL, scr);
    printf("OK %lu %06lx\n", pen, c);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
