/* flatwin -- FLAT-C1 rig helper (tools/rig/amarchy_flat_rig.py): windows whose close / depth / size / zoom gadgets are
 * sysiclass images, opened and closed in a loop, and a hash of their frame pixels.
 *   flatwin N   open and close N windows on the Workbench screen (each one draws its system gadgets through sysiclass)
 *               and print "flatwin cycles=N hash=<hex>": FNV-1a over every pixel of the first window's title bar and
 *               bottom border (ReadPixel). The windows are opened inactive at a fixed place, so the hash depends only
 *               on what sysiclass drew: the same with and without amarchy-flat's pass-through.
 * FLAT-C2 additions (the rig's c2 step):
 *   flatwin N SET <enabled> <style> <pen_text> <pen_line> <pen_back>
 *               write the switch, style and glyph pens into the running amarchy-flat's state block first (what
 *               Amarchy does); a close/depth/zoom/size window is then opened, hashed, RefreshWindowFrame'd, hashed.
 *   Every run prints, per system gadget, "gad <name> w=<which> open=<hash> refresh=<hash>": FNV-1a over the pixels
 *   of that gadget's box after the open and after RefreshWindowFrame; <which> is GetAttr(SYSIA_Which) on the
 *   gadget's image (-1 when GetAttr answers 0). */
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
#include <intuition/imageclass.h>
#include <intuition/classusr.h>
#include <intuition/classes.h>
#include <intuition/gadgetclass.h>
#include <exec/semaphores.h>
#include "../../amarchy/flat_core.h"

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
long __stack = 16384;
const char flatwin_stack_cookie[] = "$STACK: 16384";

static WORD zoombox[4] = { 40, 30, 120, 40 };

static struct Window *mkwin(void)
{
    struct TagItem t[16];
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
    t[n].ti_Tag = WA_Zoom;         t[n++].ti_Data = (ULONG)zoombox;
    t[n].ti_Tag = TAG_DONE;        t[n].ti_Data = 0;
    return OpenWindowTagList(NULL, t);
}

typedef struct { struct SignalSemaphore ss; char name[16]; struct Task *task; char tname[32]; flat_state st; } flatrec;

static unsigned long gadhash(struct Window *w, struct Gadget *g)
{
    long x = g->LeftEdge + ((g->Flags & GFLG_RELRIGHT) ? w->Width - 1 : 0), y = g->TopEdge + ((g->Flags & GFLG_RELBOTTOM) ? w->Height - 1 : 0);
    long gw = g->Width + ((g->Flags & GFLG_RELWIDTH) ? w->Width : 0), gh = g->Height + ((g->Flags & GFLG_RELHEIGHT) ? w->Height : 0), i, j;
    unsigned long h = 2166136261UL;
    for (j = 0; j < gh; j++)
        for (i = 0; i < gw; i++) {
            h ^= (unsigned long)(ReadPixel(w->RPort, x + i, y + j) & 0xFF);
            h *= 16777619UL;
        }
    return h;
}

static const char *gadname(UWORD t)
{
    switch (t & GTYP_SYSTYPEMASK) {
    case GTYP_CLOSE: return "close";
    case GTYP_WDEPTH: return "depth";
    case GTYP_WZOOM: return "zoom";
    case GTYP_SIZING: return "size";
    default: return 0;
    }
}

static void gadreport(struct Window *w, unsigned long *oh, int pass)
{
    struct Gadget *g;
    int k = 0;
    for (g = w->FirstGadget; g; g = g->NextGadget) {
        const char *nm;
        if (!(g->GadgetType & GTYP_SYSGADGET) || !(nm = gadname(g->GadgetType)))
            continue;
        if (pass == 0) {
            ULONG which = 0xFFFFFFFFUL;
            if (!(g->GadgetRender && (g->Flags & GFLG_GADGIMAGE) && ((struct Image *)g->GadgetRender)->Depth == CUSTOMIMAGEDEPTH))
                which = 0xFFFFFFFEUL;               /* a classic Image is not a BOOPSI object: GetAttr would crash */
            else if (GetAttr(SYSIA_Which, (Object *)g->GadgetRender, &which) == 0)
                which = 0xFFFFFFFFUL;
            printf("gad %s w=%ld ", nm, (long)which);
            oh[k] = gadhash(w, g);
            printf("open=%08lx ", oh[k]);
        } else {
            printf("refresh=%08lx\n", gadhash(w, g));
        }
        k++;
    }
}

int main(int argc, char **argv)
{
    long cycles = argc > 1 ? atol(argv[1]) : 1, i, x, y;
    unsigned long h = 2166136261UL;
    if (!(IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36)))
        return 10;
    if (!(GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 36)))
        return 10;
    if (argc > 7 && argv[2][0] == 'S') {
        flatrec *r;
        Forbid();
        r = (flatrec *)FindSemaphore((STRPTR)FLAT_SEM_NAME);
        if (r) {
            r->st.enabled = (UWORD)atol(argv[3]);
            r->st.style = (UWORD)atol(argv[4]);
            r->st.pen_text = (UBYTE)atol(argv[5]);
            r->st.pen_line = (UBYTE)atol(argv[6]);
            r->st.pen_back = (UBYTE)atol(argv[7]);
        }
        Permit();
        if (!r) {
            printf("flatwin: no Amarchy.Flat record\n");
            return 5;
        }
    }
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
            {
                unsigned long oh[8];
                int pass;
                for (pass = 0; pass < 2; pass++) {
                    if (pass == 1) {
                        RefreshWindowFrame(w);
                        Delay(25);
                    }
                    gadreport(w, oh, pass);
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
