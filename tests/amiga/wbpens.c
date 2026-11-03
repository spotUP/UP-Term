/* wbpens -- P1A R1.1f: the Workbench screen's live pens as text, for the rig test that proves a
 * theme moved the desktop (plan 2026-10-29-amarchy-p1a-native-theme.md).
 *
 * Prints, for the Workbench public screen: its depth, GetRGB32 of colours 0-7 and the sprite
 * colours 17-19 and on a screen deeper than 8 colours the last four (8-bit per gun, the high byte of the 32-bit value), the DrawInfo pens
 * (dri_Pens[0..dri_NumPens-1]) and the screen title. One line each, nothing else:
 *   DEPTH 8
 *   RGB 0 aa aa aa
 *   PEN 5 3
 *   TITLE Workbench Screen
 *
 * wbpens WATCH <seconds>: every tick, a line when the Workbench screen address, colours 4-7 or the last four change, or when
 * T:amarchy-revert/flatlive.rgb (F, the flat live-pen write) or ENV:AMARCHY_WBCLOSES (D, the apply returned) appears or goes, stamped with ticks since start:
 *   T 123 SCR 0x1234 4 0000ff 5 ff00ff 6 00ffff 7 ffffff L 0000ff 222222 00ffff ffffff F 1 D 0
 *
 * wbpens RGB <n> [<n> ...]: only the RGB lines of the given colour entries (an allocated pen such as
 * amarchy-flat STATUS knob=249), same RGB line form. */
#include <stdio.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/view.h>
#include <stdlib.h>
#include <string.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include "../../amarchy/flat_shared.h"   /* the "Amarchy.Flat" record: st.enabled is the flat state (F marker) */

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

/* vbcc's startup runs main on a stack of this size; the cookie says the same to AmigaOS 3.2 and vsh. */
long __stack = 8192;
const char wbpens_stack_cookie[] = "$STACK: 8192";

static void watch(long secs)
{
    char last[160] = "", cur[160];
    long t;
    for (t = 0; t < secs * 50; t++) {
        struct Screen *s = LockPubScreen((STRPTR)"Workbench");
        flatshared *fr;
        int flat_on;
        int i, n = 0;
        ULONG rgb[3];
        BPTR d = Lock((STRPTR)"ENV:AMARCHY_WBCLOSES", ACCESS_READ);
        Forbid();
        fr = (flatshared *)FindSemaphore((STRPTR)FLAT_SEM_NAME);
        flat_on = fr && fr->st.enabled;
        Permit();
        if (d)
            UnLock(d);
        if (s) {
            int top = 1 << s->RastPort.BitMap->Depth;
            n += sprintf(cur + n, "SCR 0x%lx", (unsigned long)s);
            for (i = 4; i < 8; i++) {
                GetRGB32(s->ViewPort.ColorMap, (ULONG)i, 1, rgb);
                n += sprintf(cur + n, " %d %02lx%02lx%02lx", i, rgb[0] >> 24, rgb[1] >> 24, rgb[2] >> 24);
            }
            n += sprintf(cur + n, " L");
            for (i = top - 4; top > 8 && i < top; i++) {
                GetRGB32(s->ViewPort.ColorMap, (ULONG)i, 1, rgb);
                n += sprintf(cur + n, " %02lx%02lx%02lx", rgb[0] >> 24, rgb[1] >> 24, rgb[2] >> 24);
            }
            UnlockPubScreen(NULL, s);
        } else
            n += sprintf(cur + n, "SCR none");
        sprintf(cur + n, " F %d D %d", flat_on, d ? 1 : 0);
        if (strcmp(cur, last)) {
            printf("T %ld %s\n", t, cur);
            fflush(stdout);
            strcpy(last, cur);
        }
        Delay(1);
    }
}

int main(int argc, char **argv)
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
    if (argc > 2 && !strcmp(argv[1], "WATCH")) {
        watch(atol(argv[2]));
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 0;
    }
    s = LockPubScreen((STRPTR)"Workbench");
    if (!s)
        return 10;
    if (argc > 2 && !strcmp(argv[1], "RGB")) {
        for (i = 2; i < argc; i++) {
            GetRGB32(s->ViewPort.ColorMap, (ULONG)atol(argv[i]), 1, rgb);
            printf("RGB %ld %02lx %02lx %02lx\n", atol(argv[i]), rgb[0] >> 24, rgb[1] >> 24, rgb[2] >> 24);
        }
        UnlockPubScreen(0, s);
        CloseLibrary((struct Library *)GfxBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 0;
    }
    printf("DEPTH %d\n", (int)s->RastPort.BitMap->Depth);
    for (i = 0; i < 11; i++) {
        GetRGB32(s->ViewPort.ColorMap, (ULONG)idx[i], 1, rgb);
        printf("RGB %d %02lx %02lx %02lx\n", idx[i], rgb[0] >> 24, rgb[1] >> 24, rgb[2] >> 24);
    }
    /* the last four colours of a deeper screen (3.x Palette prefs colours 4-7 may land there) */
    if (s->RastPort.BitMap->Depth > 3) {
        int top = 1 << s->RastPort.BitMap->Depth;
        for (i = top - 4; i < top; i++) {
            GetRGB32(s->ViewPort.ColorMap, (ULONG)i, 1, rgb);
            printf("RGB %d %02lx %02lx %02lx\n", i, rgb[0] >> 24, rgb[1] >> 24, rgb[2] >> 24);
        }
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
