/* titleprobe -- FLAT-TITLE-ACCENT rig probe: ReadPixel of the title band, side border and title text of an active and
   an inactive window. Prints counts per pen; the driver compares them with the palette. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <stdio.h>
#include <string.h>

long __stack = 16384;
const char rp_cookie[] = "$STACK: 16384";
struct IntuitionBase *IntuitionBase; struct GfxBase *GfxBase;
static struct Window *A, *B;

static void pump(struct Window *w)
{
    struct IntuiMessage *m;
    while ((m = (struct IntuiMessage *)GetMsg(w->UserPort))) {
        if (m->Class == IDCMP_REFRESHWINDOW) { BeginRefresh(w); EndRefresh(w, TRUE); }
        ReplyMsg((struct Message *)m);
    }
}
static void settle(void) { Delay(50); pump(A); pump(B); Delay(25); pump(A); pump(B); }

/* hist of pens over a box in window coordinates, printed as pens p0..p7 */
static void box(const char *nm, struct Window *w, const char *what, int x0, int y0, int x1, int y1)
{
    int h[8], bad = 0, x, y; LONG p;
    memset(h, 0, sizeof h);
    for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) {
        p = ReadPixel(w->RPort, x, y);
        if (p >= 0 && p < 8) h[p]++; else bad++;
    }
    printf("BOX %s %s (%d,%d)-(%d,%d) pens0-7=%d,%d,%d,%d,%d,%d,%d,%d other=%d\n", nm, what, x0, y0, x1, y1,
           h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7], bad);
}
static void report(const char *step, const char *nm, struct Window *w)
{
    int bt = w->BorderTop, W = w->Width, H = w->Height;
    printf("WIN %s %s %dx%d BorderTop=%d BorderLeft=%d active=%d\n", step, nm, W, H, bt, w->BorderLeft, IntuitionBase->ActiveWindow == w);
    box(nm, w, "bandLeftOfText", 2, 1, 5, bt - 2);                   /* left of close gadget area edge: band or gadget */
    box(nm, w, "titleText", w->BorderLeft + 20, 1, W / 2, bt - 2);   /* text zone between gadgets */
    box(nm, w, "bandRight", W / 2 + 10, 1, W - 60, bt - 2);          /* empty band right of the text */
    box(nm, w, "sideBorder", 1, bt + 4, 3, H - 6);
    box(nm, w, "body", w->BorderLeft + 4, bt + 4, W - 10, H - 10);
}

int main(void)
{
    ULONG c[3]; int i; struct Screen *scr;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 36);
    if (!IntuitionBase || !GfxBase) return 20;
    scr = LockPubScreen(NULL);
    printf("SCREEN depth %d\n", (int)scr->RastPort.BitMap->Depth);
    for (i = 0; i < 8; i++) { GetRGB32(scr->ViewPort.ColorMap, i, 1, c); printf("PEN %d rgb %02lx%02lx%02lx\n", i, c[0] >> 24, c[1] >> 24, c[2] >> 24); }
    A = OpenWindowTags(NULL, WA_Left, 20, WA_Top, 60, WA_Width, 240, WA_Height, 90, WA_Title, "title A active", WA_CloseGadget, 1,
        WA_DepthGadget, 1, WA_DragBar, 1, WA_SmartRefresh, 1, WA_IDCMP, IDCMP_REFRESHWINDOW, TAG_END);
    B = OpenWindowTags(NULL, WA_Left, 300, WA_Top, 60, WA_Width, 240, WA_Height, 90, WA_Title, "title B inactive", WA_CloseGadget, 1,
        WA_DepthGadget, 1, WA_DragBar, 1, WA_SmartRefresh, 1, WA_IDCMP, IDCMP_REFRESHWINDOW, TAG_END);
    UnlockPubScreen(NULL, scr);
    if (!A || !B) { puts("open failed"); return 20; }
    ActivateWindow(A); settle();
    report("A active", "A", A); report("A active", "B", B);
    ActivateWindow(B); settle();
    report("B active", "A", A); report("B active", "B", B);
    ActivateWindow(A); settle();
    puts("HOLD");
    Delay(150);
    CloseWindow(B); CloseWindow(A);
    CloseLibrary((struct Library *)GfxBase); CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
