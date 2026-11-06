/* iconpens -- s28 F15: the shared icon pen holder and the icon pens' colours as text (a probe tool, not shipped).
 * Prints, one line each:
 *   DEPTH <n>                           depth of the Workbench screen
 *   HOLDER owners=<bits> held=<n> refs=<n> slot0=<n> first=<n> count=<n> screen=<hex>   or  HOLDER none (record freed)
 *   HAVE <16 digits>                    have[i] of the record
 *   PEN <n> <rrggbb>                    GetRGB32 of pens 32..47 (the stored icon pens; depth >= 6)
 * The record layout is ipd_rec of amarchy/iconpen_dos.h (the real struct): semaphore, 20 name bytes, the per-screen table (slot 0 = Workbench). */
#include <stdio.h>
#include <exec/semaphores.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include "../../amarchy/iconpen_dos.h"

long __stack = 8192;
const char iconpens_stack_cookie[] = "$STACK: 8192";
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

int main(void)
{
    struct Screen *sc;
    ip_shared s;
    ipd_rec *r;
    int have = 0, i, depth;
    ULONG rgb[3];
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase)
        return 20;
    Forbid();
    r = (ipd_rec *)FindSemaphore((STRPTR)IPD_SEM_NAME);
    if (r) { s = r->t.slot[0].sh; have = 1; }
    Permit();
    sc = LockPubScreen((STRPTR)"Workbench");
    if (!sc) { printf("no Workbench\n"); return 10; }
    depth = sc->RastPort.BitMap->Depth;
    printf("DEPTH %d\n", depth);
    if (have) {
        printf("HOLDER owners=%lu held=%d refs=%d slot0=%d first=%d count=%d screen=%lx%s\n", (unsigned long)s.owners, s.h.held,
               (int)(((s.owners & IP_OWNER_FLAT) != 0) + ((s.owners & IP_OWNER_ICONS) != 0)), s.h.slot0, s.h.first, s.h.count, (unsigned long)s.h.screen,
               (s.h.screen == (const void *)sc) ? " (the Workbench screen)" : " (NOT the Workbench screen)");
        printf("HAVE ");
        for (i = 0; i < IP_PENS; i++) printf("%d", (int)s.h.have[i]);
        printf("\n");
    } else
        printf("HOLDER none\n");
    for (i = 32; i < 48 && depth >= 6; i++) {
        GetRGB32(sc->ViewPort.ColorMap, (ULONG)i, 1, rgb);
        printf("PEN %d %02lx%02lx%02lx\n", i, rgb[0] >> 24, rgb[1] >> 24, rgb[2] >> 24);
    }
    UnlockPubScreen(0, sc);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
