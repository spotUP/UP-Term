/* clprobe: does sysiclass draw a window-gadget image into an offscreen rastport? */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/imageclass.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <stdio.h>

long __stack = 32768;
static const char stackcookie[] = "$STACK: 32768";

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static UWORD mpens[NUMDRIPENS + 4];

static void dump(const char *tag, struct RastPort *rp, int w, int h)
{
    int x, y, nz = 0;
    char line[80];
    Printf((STRPTR)"%s\n", (LONG)tag);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w && x < 70; x++) {
            LONG p = ReadPixel(rp, x, y);
            if (p != 0) nz++;
            line[x] = p < 0 ? '?' : p < 10 ? (char)('0' + p) : p < 36 ? (char)('a' + p - 10) : '#';
        }
        line[x] = 0;
        Printf((STRPTR)"  %s\n", (LONG)line);
    }
    Printf((STRPTR)"  nonzero=%ld\n", (LONG)nz);
}

static void probe_im(struct Screen *s, struct DrawInfo *di, struct Image *ii, const char *name, int screen)
{
    struct BitMap *bm;
    struct RastPort rp;
    struct DrawInfo md;
    int i, w, h, depth = GetBitMapAttr(s->RastPort.BitMap, BMA_DEPTH);
    w = ii->Width; h = ii->Height;
    Printf((STRPTR)"== %s %ldx%ld le=%ld te=%ld scrdepth=%ld dri_Version=%ld\n", (LONG)name, (LONG)w, (LONG)h,
           (LONG)ii->LeftEdge, (LONG)ii->TopEdge, (LONG)depth, (LONG)di->dri_Version);
    /* B: plain 8 plane offscreen, real DrawInfo */
    if ((bm = AllocBitMap(w, h, 8, BMF_CLEAR, NULL))) {
        InitRastPort(&rp); rp.BitMap = bm;
        DrawImageState(&rp, ii, -ii->LeftEdge, -ii->TopEdge, IDS_NORMAL, di); WaitBlit();
        dump("B 8pl nofriend realdri", &rp, w, h);
        FreeBitMap(bm);
    }
    /* C: screen depth, friend */
    if ((bm = AllocBitMap(w, h, depth, BMF_CLEAR, s->RastPort.BitMap))) {
        InitRastPort(&rp); rp.BitMap = bm;
        DrawImageState(&rp, ii, -ii->LeftEdge, -ii->TopEdge, IDS_NORMAL, di); WaitBlit();
        dump("C scrdepth friend realdri", &rp, w, h);
        FreeBitMap(bm);
    }
    /* D: screen depth, no friend */
    if ((bm = AllocBitMap(w, h, depth, BMF_CLEAR, NULL))) {
        InitRastPort(&rp); rp.BitMap = bm;
        DrawImageState(&rp, ii, -ii->LeftEdge, -ii->TopEdge, IDS_NORMAL, di); WaitBlit();
        dump("D scrdepth nofriend realdri", &rp, w, h);
        FreeBitMap(bm);
    }
    /* E: 8 planes, marker DrawInfo copy (pen i -> i+1) */
    if ((bm = AllocBitMap(w, h, 8, BMF_CLEAR, NULL))) {
        md = *di;
        for (i = 0; i < NUMDRIPENS + 4; i++) mpens[i] = (UWORD)(i + 1);
        md.dri_Pens = mpens;
        InitRastPort(&rp); rp.BitMap = bm;
        DrawImageState(&rp, ii, -ii->LeftEdge, -ii->TopEdge, IDS_NORMAL, &md); WaitBlit();
        dump("E 8pl nofriend markerdri", &rp, w, h);
        FreeBitMap(bm);
    }
    if (!screen) return;
    /* G: the screen's own rastport (control), top-left, read back */
    DrawImageState(&s->RastPort, ii, 0 - ii->LeftEdge, 0 - ii->TopEdge, IDS_NORMAL, di); WaitBlit();
    dump("G screen rastport realdri", &s->RastPort, w, h);
}

static void probe(struct Screen *s, struct DrawInfo *di, ULONG which, const char *name)
{
    struct TagItem t[3];
    Object *im;
    t[0].ti_Tag = SYSIA_DrawInfo; t[0].ti_Data = (ULONG)di;
    t[1].ti_Tag = SYSIA_Which; t[1].ti_Data = which;
    t[2].ti_Tag = TAG_DONE;
    im = NewObjectA(NULL, (UBYTE *)"sysiclass", t);
    if (!im) { Printf((STRPTR)"%s: NewObject failed\n", (LONG)name); return; }
    probe_im(s, di, (struct Image *)im, name, 1);
    DisposeObject(im);
}

static void probe_win(struct Screen *s, struct DrawInfo *di)
{
    struct TagItem t[8];
    struct Window *w;
    struct Gadget *g;
    char nm[40];
    t[0].ti_Tag = WA_Left; t[0].ti_Data = 100;
    t[1].ti_Tag = WA_Top; t[1].ti_Data = 60;
    t[2].ti_Tag = WA_Width; t[2].ti_Data = 300;
    t[3].ti_Tag = WA_Height; t[3].ti_Data = 80;
    t[4].ti_Tag = WA_Flags; t[4].ti_Data = WFLG_CLOSEGADGET | WFLG_DEPTHGADGET | WFLG_DRAGBAR | WFLG_SIZEGADGET | WFLG_ACTIVATE;
    t[5].ti_Tag = WA_Title; t[5].ti_Data = (ULONG)"clprobe";
    t[6].ti_Tag = WA_PubScreen; t[6].ti_Data = (ULONG)s;
    t[7].ti_Tag = TAG_DONE;
    w = OpenWindowTagList(NULL, t);
    if (!w) { Printf((STRPTR)"no window\n"); return; }
    Delay(25);
    for (g = w->FirstGadget; g; g = g->NextGadget) {
        Printf((STRPTR)"gadget type %04lx flags %04lx render %08lx\n", (LONG)g->GadgetType, (LONG)g->Flags, (LONG)g->GadgetRender);
        if ((g->GadgetType & GTYP_SYSGADGET) && (g->Flags & GFLG_GADGIMAGE) && g->GadgetRender) {
            sprintf(nm, "WIN gad %04lx", (unsigned long)g->GadgetType);
            probe_im(s, di, (struct Image *)g->GadgetRender, nm, 0);
        }
    }
    CloseWindow(w);
}

int main(void)
{
    struct Screen *s;
    struct DrawInfo *di;
    if (!stackcookie[0]) return 5;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase) return 20;
    Printf((STRPTR)"intuition %ld.%ld\n", (LONG)IntuitionBase->LibNode.lib_Version, (LONG)IntuitionBase->LibNode.lib_Revision);
    s = LockPubScreen(NULL);
    if (!s) return 20;
    di = GetScreenDrawInfo(s);
    if (di) {
        probe(s, di, DEPTHIMAGE, "DEPTH");
        probe(s, di, CLOSEIMAGE, "CLOSE");
        probe_win(s, di);
        FreeScreenDrawInfo(s, di);
    }
    UnlockPubScreen(NULL, s);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
