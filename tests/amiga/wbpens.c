/* wbpens -- P1A R1.1f: the Workbench screen's live pens as text, for the rig test that proves a
 * theme moved the desktop (plan 2026-10-29-amarchy-p1a-native-theme.md).
 *
 * Prints, for the Workbench public screen: its depth, GetRGB32 of colours 0-7 and the sprite
 * colours 17-19 (8-bit per gun, the high byte of the 32-bit value), the DrawInfo pens
 * (dri_Pens[0..dri_NumPens-1]) and the screen title. One line each, nothing else:
 *   DEPTH 8
 *   RGB 0 aa aa aa
 *   PEN 5 3
 *   TITLE Workbench Screen */
#include <stdio.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/view.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

/* vbcc's startup runs main on a stack of this size; the cookie says the same to AmigaOS 3.2 and vsh. */
long __stack = 8192;
const char wbpens_stack_cookie[] = "$STACK: 8192";

int main(void)
{
    struct Screen *s;
    struct DrawInfo *dri;
    ULONG rgb[3];
    int i;
    static const int idx[11] = {0, 1, 2, 3, 4, 5, 6, 7, 17, 18, 19};

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 36);
    if (!IntuitionBase || !GfxBase)
        return 20;
    s = LockPubScreen((STRPTR)"Workbench");
    if (!s)
        return 10;
    printf("DEPTH %d\n", (int)s->RastPort.BitMap->Depth);
    for (i = 0; i < 11; i++) {
        GetRGB32(s->ViewPort.ColorMap, (ULONG)idx[i], 1, rgb);
        printf("RGB %d %02lx %02lx %02lx\n", idx[i], rgb[0] >> 24, rgb[1] >> 24, rgb[2] >> 24);
    }
    dri = GetScreenDrawInfo(s);
    if (dri) {
        for (i = 0; i < (int)dri->dri_NumPens; i++)
            printf("PEN %d %d\n", i, (int)dri->dri_Pens[i]);
        FreeScreenDrawInfo(s, dri);
    }
    printf("TITLE %s\n", s->Title ? (char *)s->Title : "-");
    UnlockPubScreen(NULL, s);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
