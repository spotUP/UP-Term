/* shotscreen: a chipset front screen (320x200x4 PAL lores) with one active window, for the capture rig
 * (tools/rig/amarchy_cap_rig.py). The rig Workbench is RTG truecolour, which shot.c refuses (SHOT_DEEP, open question
 * Q1 of the P4 probe note), so the capture proof needs a planar screen. Draws a known pattern, holds N seconds
 * (argv[1], default 90) or until the window closes. */
#include <stdio.h>
#include <stdlib.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

long __stack = 65536;
const char *shotscreen_cookie = "$STACK:65536";

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

int main(int argc, char **argv)
{
    struct Screen *sc;
    struct Window *w;
    long secs = argc > 1 ? atol(argv[1]) : 90, i;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
    if (!IntuitionBase || !GfxBase) return 20;
    sc = OpenScreenTags(0, SA_Width, 320, SA_Height, 200, SA_Depth, 4, SA_DisplayID, PAL_MONITOR_ID | LORES_KEY,
                        SA_Quiet, TRUE, SA_Title, (ULONG)"shotscreen", TAG_END);
    if (!sc) return 20;
    w = OpenWindowTags(0, WA_Left, 20, WA_Top, 30, WA_Width, 160, WA_Height, 90, WA_CustomScreen, (ULONG)sc,
                       WA_Title, (ULONG)"shotwin", WA_CloseGadget, TRUE, WA_Activate, TRUE, WA_IDCMP, IDCMP_CLOSEWINDOW,
                       TAG_END);
    if (w) {
        for (i = 0; i < 16; i++) {
            SetAPen(&sc->RastPort, i);
            RectFill(&sc->RastPort, 200 + (i & 3) * 20, 20 + (i >> 2) * 20, 218 + (i & 3) * 20, 38 + (i >> 2) * 20);
        }
        printf("shotscreen up\n");
        fflush(stdout);
        for (i = 0; i < secs * 2; i++) {
            struct IntuiMessage *m;
            int done = 0;
            Delay(25);
            while ((m = (struct IntuiMessage *)GetMsg(w->UserPort)) != 0) {
                if (m->Class == IDCMP_CLOSEWINDOW) done = 1;
                ReplyMsg((struct Message *)m);
            }
            if (done) break;
        }
        CloseWindow(w);
    }
    CloseScreen(sc);
    return 0;
}
