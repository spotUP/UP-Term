/* btnbgprobe -- s28 FLAT-BUTTON-ROUND-BG: a window whose interior is a 2x2 checker of pens 2 and 3 (never pen 0), with or
 * without one GadTools button on it, so a host script can compare the button's corner pixels with the background pen
 * that was there before (tools/rig/amarchy_btnbg_rig.py).
 *   btnbgprobe NOGAD   pattern only, window stays open until Ctrl-C
 *   btnbgprobe GAD     the same pattern, then one 80x14 button added over it (AddGList + RefreshGList), until Ctrl-C
 * Prints "WIN left top w h" and "GAD x y w h" in screen coordinates, "DEPTH n", "READY". */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;
long __stack = 16384;
const char btnbgprobe_stack_cookie[] = "$STACK: 16384";

#define BX 20
#define BY 30
#define BW 80
#define BH 14

int main(int argc, char **argv)
{
    struct Screen *scr;
    struct Window *w;
    struct Gadget *gl = NULL, *g;
    struct NewGadget ng;
    void *vi;
    struct TagItem t[10];
    int i = 0, x, y, wantgad = argc > 1 && !strcmp(argv[1], "GAD");
    if (argv && argv[0] == NULL)
        return 5;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 36);
    GadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 36);
    if (!IntuitionBase || !GfxBase || !GadToolsBase)
        return 20;
    scr = LockPubScreen(NULL);
    vi = GetVisualInfoA(scr, NULL);
    g = CreateContext(&gl);
    memset(&ng, 0, sizeof ng);
    ng.ng_VisualInfo = vi; ng.ng_TextAttr = scr->Font;
    ng.ng_LeftEdge = BX; ng.ng_TopEdge = BY; ng.ng_Width = BW; ng.ng_Height = BH;
    ng.ng_GadgetText = (UBYTE *)"Button"; ng.ng_GadgetID = 1;
    g = CreateGadgetA(BUTTON_KIND, g, &ng, NULL);
    t[i].ti_Tag = WA_Left; t[i++].ti_Data = 40;
    t[i].ti_Tag = WA_Top; t[i++].ti_Data = 40;
    t[i].ti_Tag = WA_Width; t[i++].ti_Data = 160;
    t[i].ti_Tag = WA_Height; t[i++].ti_Data = 100;
    t[i].ti_Tag = WA_Title; t[i++].ti_Data = (ULONG)"btnbg";
    t[i].ti_Tag = WA_SmartRefresh; t[i++].ti_Data = 1;
    t[i].ti_Tag = WA_IDCMP; t[i++].ti_Data = IDCMP_CLOSEWINDOW | BUTTONIDCMP;
    t[i].ti_Tag = TAG_DONE; t[i].ti_Data = 0;
    w = OpenWindowTagList(NULL, t);
    if (!w) { printf("NOWIN\n"); return 10; }
    for (y = w->BorderTop; y < w->Height - w->BorderBottom; y++)
        for (x = w->BorderLeft; x < w->Width - w->BorderRight; x++) {
            SetAPen(w->RPort, ((x >> 1) + (y >> 1)) & 1 ? 2 : 3);
            WritePixel(w->RPort, x, y);
        }
    if (wantgad) {
        AddGList(w, gl, 0xFFFFUL, -1L, NULL);
        RefreshGList(gl, w, NULL, -1L);
        GT_RefreshWindow(w, NULL);
    }
    {   /* a file closed before the host reads it: an open redirect keeps the file locked against readers */
        FILE *f = fopen("T:btnbg.out", "w");
        if (f) {
            fprintf(f, "DEPTH %ld\nWIN %d %d %d %d\nGAD %d %d %d %d\nREADY\n", (long)scr->RastPort.BitMap->Depth,
                    w->LeftEdge, w->TopEdge, w->Width, w->Height, w->LeftEdge + BX, w->TopEdge + BY, BW, BH);
            fclose(f);
        }
    }
    Wait(SIGBREAKF_CTRL_C | (1L << w->UserPort->mp_SigBit));
    if (wantgad) RemoveGList(w, gl, -1L);
    CloseWindow(w);
    FreeGadgets(gl);
    FreeVisualInfo(vi);
    UnlockPubScreen(NULL, scr);
    return 0;
}
