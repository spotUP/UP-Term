/* copcheck: Workbench bitmap end vs GfxBase->LOFlist, and a sanity verdict on the copper list's first moves. */
#include <stdio.h>
#include <exec/types.h>
#include <graphics/gfxbase.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
int main(void)
{
    struct Screen *s;
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    if (!GfxBase || !IntuitionBase) return 20;
    s = LockPubScreen((STRPTR)"Workbench");
    struct BitMap *bm;
    struct Window *w;
    UWORD *cl;
    ULONG end, sum = 0, i;
    int bad = 0;
    if (!s) { printf("copcheck no-wb\n"); return 20; }
    bm = s->RastPort.BitMap;
    end = (ULONG)bm->Planes[0] + (ULONG)bm->BytesPerRow * bm->Rows;
    if ((ULONG)bm->Planes[1] - (ULONG)bm->Planes[0] != (ULONG)bm->BytesPerRow / bm->Depth)
        end = (ULONG)bm->Planes[bm->Depth - 1] + (ULONG)bm->BytesPerRow * bm->Rows;
    cl = (UWORD *)GfxBase->LOFlist;
    for (i = 0; i < 0x400; i++)
        sum = sum * 31 + ((UBYTE *)end)[i];
    for (i = 0; i < 16; i += 2)
        if (!(cl[i] & 1) && (cl[i] >= 0x200))
            bad++;
    printf("copcheck pl0=%08lx pl1=%08lx bpr=%u rows=%u depth=%u end=%08lx lof=%08lx off=%ld tailsum=%08lx\n",
           (ULONG)bm->Planes[0], (ULONG)bm->Planes[1], bm->BytesPerRow, bm->Rows, bm->Depth, end, (ULONG)cl,
           (long)((ULONG)cl - end), sum);
    printf("copcheck words");
    for (i = 0; i < 16; i++)
        printf(" %04x", cl[i]);
    printf("\ncopcheck verdict=%s win=", bad ? "CORRUPT" : "OK");
    for (w = s->FirstWindow; w; w = w->NextWindow)
        printf("[%s %d,%d %dx%d]", w->Title ? (char *)w->Title : "-", w->LeftEdge, w->TopEdge, w->Width, w->Height);
    printf("\n");
    UnlockPubScreen(NULL, s);
    return bad ? 5 : 0;
}
