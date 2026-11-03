/* wpaprobe -- K40 gate: does the rig's cybergraphics.library export WritePixelArrayAlpha (LVO -216)?
 * Prints lib_Version, lib_NegSize and the screen depth; on version >= 43 fills a 2x1 rectangle with 0x00204060
 * (FillPixelArray -150), draws one 2x1 ARGB source of alpha 0x80, colour FF0000 over it (WritePixelArrayAlpha -216,
 * globalalpha 0xFFFFFFFF), reads it back (ReadPixelArray -120, RECTFMT_ARGB) and prints "BLEND aarrggbb aarrggbb".
#include <stdio.h>
#include <exec/types.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#define RECTFMT_ARGB 2

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
long __stack = 16384;
const char wpaprobe_stack_cookie[] = "$STACK: 16384";

ULONG cg_fill(__reg("a6") struct Library *, __reg("a1") struct RastPort *rp, __reg("d0") UWORD x,
              __reg("d1") UWORD y, __reg("d2") UWORD w, __reg("d3") UWORD h,
              __reg("d4") ULONG argb) = "\tjsr\t-150(a6)";
ULONG cg_wpa(__reg("a6") struct Library *, __reg("a0") APTR src, __reg("d0") UWORD sx, __reg("d1") UWORD sy,
             __reg("d2") UWORD smod, __reg("a1") struct RastPort *rp, __reg("d3") UWORD dx, __reg("d4") UWORD dy,
             __reg("d5") UWORD w, __reg("d6") UWORD h, __reg("d7") ULONG ga) = "\tjsr\t-216(a6)";
ULONG cg_rpa(__reg("a6") struct Library *, __reg("a0") APTR dst, __reg("d0") UWORD dx, __reg("d1") UWORD dy,
             __reg("d2") UWORD dmod, __reg("a1") struct RastPort *rp, __reg("d3") UWORD sx, __reg("d4") UWORD sy,
             __reg("d5") UWORD w, __reg("d6") UWORD h, __reg("d7") UBYTE fmt) = "\tjsr\t-120(a6)";

int main(void)
{
    struct Library *cg;
    struct Window *w;
    struct Screen *scr;
    ULONG src[2] = {0x80FF0000UL, 0x80FF0000UL}, got[2] = {0, 0};
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 36);
    cg = OpenLibrary((STRPTR)"cybergraphics.library", 0);
    if (!IntuitionBase || !GfxBase || !cg) { printf("OPEN FAIL cg=%ld\n", (long)cg); return 5; }
    printf("CGX version %ld.%ld NegSize %ld\n", (long)cg->lib_Version, (long)cg->lib_Revision, (long)cg->lib_NegSize);
    scr = LockPubScreen(NULL);
    printf("DEPTH %ld\n", (long)GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH));
    if (cg->lib_NegSize >= 222) {
        w = OpenWindowTags(NULL, WA_Left, 100, WA_Top, 100, WA_Width, 64, WA_Height, 32, WA_Borderless, TRUE,
                           WA_PubScreen, scr, TAG_END);
        if (w) {
            cg_fill(cg, w->RPort, 0, 0, 2, 1, 0x00204060UL);
            cg_wpa(cg, src, 0, 0, 8, w->RPort, 0, 0, 2, 1, 0xFFFFFFFFUL);
            cg_rpa(cg, got, 0, 0, 8, w->RPort, 0, 0, 2, 1, RECTFMT_ARGB);
            printf("BLEND %08lx %08lx\n", got[0], got[1]);
            {
                static const ULONG gas[3] = {0x80000000UL, 0xFF000000UL, 0x00000080UL};
                int k;
                for (k = 0; k < 3; k++) {
                    cg_fill(cg, w->RPort, 0, 0, 2, 1, 0x00204060UL);
                    cg_wpa(cg, src, 0, 0, 8, w->RPort, 0, 0, 2, 1, gas[k]);
                    cg_rpa(cg, got, 0, 0, 8, w->RPort, 0, 0, 2, 1, RECTFMT_ARGB);
                    printf("BLEND ga=%08lx %08lx %08lx\n", gas[k], got[0], got[1]);
                }
            }
            {
                long off;
                for (off = 204; off <= 228; off += 6) {
                    UWORD *e = (UWORD *)((UBYTE *)cg - off);
                    printf("LVO -%ld op %04x target %04x%04x\n", off, e[0], e[1], e[2]);
                }
            }
            CloseWindow(w);
        } else printf("NOWINDOW\n");
    } else printf("NOEXPORT\n");
    UnlockPubScreen(NULL, scr);
    CloseLibrary(cg);
    return 0;
}
