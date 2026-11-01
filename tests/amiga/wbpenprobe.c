/* wbpenprobe -- s28 OWNER-ICON-COLOURS-INVERTED: the pens of a rectangle of the Workbench screen as text, so a host script
 * reads icon and label pixels without a picture (a probe tool, not shipped).
 *   wbpenprobe X Y W H   prints DEPTH <n>, PAL <pen> <rrggbb> for pens 0..63 (screen ViewPort), then one ROW <y> line per
 *                        pixel row: the pen of every pixel as two hex digits. */
#include <stdio.h>
#include <stdlib.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

long __stack = 32768;
const char *wbpenprobe_cookie = "$STACK:32768";
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

int main(int argc, char **argv)
{
    struct Screen *s;
    int x0, y0, w, h, x, y, n, i;
    ULONG rgb[3];
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase) return 20;
    if (argc < 5) { printf("usage: wbpenprobe X Y W H\n"); return 10; }
    x0 = atoi(argv[1]); y0 = atoi(argv[2]); w = atoi(argv[3]); h = atoi(argv[4]);
    s = LockPubScreen((STRPTR)"Workbench");
    if (!s) { printf("no Workbench\n"); return 20; }
    printf("DEPTH %d SIZE %d %d\n", (int)s->RastPort.BitMap->Depth, (int)s->Width, (int)s->Height);
    n = 1 << s->RastPort.BitMap->Depth;
    for (i = 0; i < 64 && i < n; i++) {
        GetRGB32(s->ViewPort.ColorMap, i, 1, rgb);
        printf("PAL %d %02lx%02lx%02lx\n", i, rgb[0] >> 24, rgb[1] >> 24, rgb[2] >> 24);
    }
    for (y = y0; y < y0 + h && y < s->Height; y++) {
        printf("ROW %d ", y);
        for (x = x0; x < x0 + w && x < s->Width; x++) printf("%02lx", (ULONG)ReadPixel(&s->RastPort, x, y) & 0xff);
        printf("\n");
    }
    UnlockPubScreen(0, s);
    return 0;
}
