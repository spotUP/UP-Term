/* ringprobe -- scratch: outer-edge ring pixel counts across resize / depth / simple-refresh damage. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
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
static struct Screen *scr;

static void pump(struct Window *w)
{
    struct IntuiMessage *m;
    while ((m = (struct IntuiMessage *)GetMsg(w->UserPort))) {
        if (m->Class == IDCMP_REFRESHWINDOW) { BeginRefresh(w); EndRefresh(w, TRUE); }
        ReplyMsg((struct Message *)m);
    }
}
static void settle(void) { Delay(40); pump(A); pump(B); Delay(25); pump(A); pump(B); }

static void measure(const char *step, const char *nm, struct Window *w)
{
    int x, W = w->Width, H = w->Height, tot = 0, bad = 0, i;
    int hist[8]; struct RastPort *rp = w->RPort; LONG p;
    memset(hist, 0, sizeof hist);
    for (i = 0; i < 4; i++) {
        int n = (i < 2) ? W : H;
        for (x = 0; x < n; x++) {
            int px = (i == 0) ? x : (i == 1) ? x : (i == 2) ? 0 : W - 1;
            int py = (i == 0) ? 0 : (i == 1) ? H - 1 : x;
            p = ReadPixel(rp, px, py);
            if (p < 0) continue;
            tot++; if (p < 8) hist[p]++; else bad++;
        }
    }
    printf("RING %s %s %dx%d valid=%d pen3=%d pens0-7=%d,%d,%d,%d,%d,%d,%d,%d other=%d active=%d\n", step, nm, W, H, tot, hist[3],
           hist[0], hist[1], hist[2], hist[3], hist[4], hist[5], hist[6], hist[7], bad, (IntuitionBase->ActiveWindow == w));
}
static void both(const char *step) { settle(); measure(step, "A", A); measure(step, "B", B); }

int main(void)
{
    ULONG c[3]; int i;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 36);
    if (!IntuitionBase || !GfxBase) return 20;
    scr = LockPubScreen(NULL);
    for (i = 0; i < 4; i++) { GetRGB32(scr->ViewPort.ColorMap, i, 1, c); printf("PEN %d rgb %02lx%02lx%02lx\n", i, c[0] >> 24, c[1] >> 24, c[2] >> 24); }
    A = OpenWindowTags(NULL, WA_Left, 20, WA_Top, 60, WA_Width, 200, WA_Height, 90, WA_Title, "ring A", WA_CloseGadget, 1,
        WA_DepthGadget, 1, WA_SizeGadget, 1, WA_DragBar, 1, WA_MinWidth, 60, WA_MinHeight, 40, WA_MaxWidth, 600, WA_MaxHeight, 180, WA_SmartRefresh, 1, WA_IDCMP, IDCMP_REFRESHWINDOW, WA_PubScreen, scr, TAG_END);
    B = OpenWindowTags(NULL, WA_Left, 300, WA_Top, 60, WA_Width, 200, WA_Height, 90, WA_Title, "ring B simple", WA_CloseGadget, 1,
        WA_DepthGadget, 1, WA_SizeGadget, 1, WA_DragBar, 1, WA_MinWidth, 60, WA_MinHeight, 40, WA_MaxWidth, 600, WA_MaxHeight, 180, WA_SimpleRefresh, 1, WA_IDCMP, IDCMP_REFRESHWINDOW, WA_PubScreen, scr, TAG_END);
    UnlockPubScreen(NULL, scr);
    if (!A || !B) { puts("open failed"); return 20; }
    both("open(B active)");
    ActivateWindow(A); both("activate A");
    ActivateWindow(B); both("activate B");
    SizeWindow(A, 40, 20); both("resize A +40,+20");
    SizeWindow(B, -30, -10); both("resize B -30,-10");
    WindowToBack(B); both("B to back");
    WindowToFront(B); both("B to front");
    WindowToBack(A); both("A to back");
    WindowToFront(A); both("A to front");
    MoveWindow(A, 280, 10); both("A drag over simple-refresh B");
    MoveWindow(A, -280, 0); both("A dragged off B");
    MoveWindow(A, 280, 0); settle(); WindowToBack(A); both("A over B, sent to back (B on top)");
    WindowToFront(A); MoveWindow(A, -280, 0); both("A off again, final");
    CloseWindow(B); CloseWindow(A);
    CloseLibrary((struct Library *)GfxBase); CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
