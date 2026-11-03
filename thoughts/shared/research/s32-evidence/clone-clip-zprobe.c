/* zprobe: real window gadget geometry vs the Amarchy clone's private sysiclass image (clone_object/clone_render replica) */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
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
static UWORD mpens[16];
static struct DrawInfo md;
#define PAD 4

static void dump(const char *tag, struct RastPort *rp, int w, int h)
{
    int x, y; char line[100];
    Printf((STRPTR)"%s (cols 0..%ld, image origin at col %ld)\n", (LONG)tag, (LONG)(w - 1), (LONG)PAD);
    for (y = 0; y < h; y++) {
        for (x = 0; x < w && x < 90; x++) {
            LONG p = ReadPixel(rp, x, y);
            line[x] = p < 0 ? '?' : p < 10 ? (char)('0' + p) : p < 36 ? (char)('a' + p - 10) : '#';
        }
        line[x] = 0;
        Printf((STRPTR)"  %s\n", (LONG)line);
    }
}

static void draw_at(struct Image *ii, struct DrawInfo *di, int ox, int w, int h, const char *tag)
{
    struct BitMap *bm; struct RastPort rp;
    if (!(bm = AllocBitMap(w + 2 * PAD, h, 8, BMF_CLEAR, NULL))) return;
    InitRastPort(&rp); rp.BitMap = bm;
    DrawImageState(&rp, ii, ox, 0, IDS_NORMAL, di); WaitBlit();
    dump(tag, &rp, w + 2 * PAD, h);
    FreeBitMap(bm);
}

static void one(struct DrawInfo *di, struct Gadget *g)
{
    struct Image *ii = (struct Image *)g->GadgetRender;
    ULONG which = 0xFFFF;
    Object *po; struct TagItem t[5];
    GetAttr(SYSIA_Which, (Object *)ii, &which);
    Printf((STRPTR)"== gad type %04lx flags %04lx box le=%ld w=%ld te=%ld h=%ld | image which=%ld le=%ld te=%ld w=%ld h=%ld\n",
           (LONG)g->GadgetType, (LONG)g->Flags, (LONG)g->LeftEdge, (LONG)g->Width, (LONG)g->TopEdge, (LONG)g->Height,
           (LONG)which, (LONG)ii->LeftEdge, (LONG)ii->TopEdge, (LONG)ii->Width, (LONG)ii->Height);
    /* REAL image drawn so its LeftEdge lands at PAD (like the window draws it relative to the box) */
    draw_at(ii, &md, PAD, ii->Width, ii->Height, "REAL marker dri, offset PAD (image at PAD+le)");
    draw_at(ii, &md, PAD - ii->LeftEdge, ii->Width, ii->Height, "REAL marker dri, offset PAD-le (image left at PAD)");
    t[0].ti_Tag = SYSIA_DrawInfo; t[0].ti_Data = (ULONG)&md;
    t[1].ti_Tag = SYSIA_Which; t[1].ti_Data = which;
    t[2].ti_Tag = IA_Width; t[2].ti_Data = (ULONG)ii->Width;
    t[3].ti_Tag = IA_Height; t[3].ti_Data = (ULONG)ii->Height;
    t[4].ti_Tag = TAG_DONE; t[4].ti_Data = 0;
    po = (Object *)NewObjectA(NULL, (UBYTE *)"sysiclass", t);
    if (!po) { Printf((STRPTR)"private NewObject failed\n"); return; }
    Printf((STRPTR)"   private image le=%ld te=%ld w=%ld h=%ld\n", (LONG)((struct Image *)po)->LeftEdge,
           (LONG)((struct Image *)po)->TopEdge, (LONG)((struct Image *)po)->Width, (LONG)((struct Image *)po)->Height);
    draw_at((struct Image *)po, &md, PAD, ii->Width, ii->Height, "PRIVATE, offset PAD (= clone_render offset 0 shifted by PAD)");
    DisposeObject(po);
}

int main(void)
{
    struct Screen *s; struct DrawInfo *di; struct Window *w; struct Gadget *g; int i;
    struct TagItem t[10]; WORD zoom[4] = { 0, 0, 200, 50 };
    if (!stackcookie[0]) return 5;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase) return 20;
    Printf((STRPTR)"intuition %ld.%ld\n", (LONG)IntuitionBase->LibNode.lib_Version, (LONG)IntuitionBase->LibNode.lib_Revision);
    s = LockPubScreen(NULL); if (!s) return 20;
    di = GetScreenDrawInfo(s);
    md = *di; for (i = 0; i < 16; i++) mpens[i] = (UWORD)(i + 1); md.dri_Pens = mpens; md.dri_Depth = 8;
    t[0].ti_Tag = WA_Left; t[0].ti_Data = 100; t[1].ti_Tag = WA_Top; t[1].ti_Data = 60;
    t[2].ti_Tag = WA_Width; t[2].ti_Data = 300; t[3].ti_Tag = WA_Height; t[3].ti_Data = 80;
    t[4].ti_Tag = WA_Flags; t[4].ti_Data = WFLG_CLOSEGADGET | WFLG_DEPTHGADGET | WFLG_DRAGBAR | WFLG_SIZEGADGET | WFLG_ACTIVATE;
    t[5].ti_Tag = WA_Title; t[5].ti_Data = (ULONG)"zprobe"; t[6].ti_Tag = WA_PubScreen; t[6].ti_Data = (ULONG)s;
    t[7].ti_Tag = WA_Zoom; t[7].ti_Data = (ULONG)zoom; t[8].ti_Tag = TAG_DONE;
    w = OpenWindowTagList(NULL, t);
    if (w) {
        Printf((STRPTR)"window w=%ld borderTop=%ld\n", (LONG)w->Width, (LONG)w->BorderTop);
        Delay(25);
        for (g = w->FirstGadget; g; g = g->NextGadget)
            if ((g->GadgetType & GTYP_SYSGADGET) && (g->Flags & GFLG_GADGIMAGE) && g->GadgetRender)
                one(di, g);
        CloseWindow(w);
    }
    FreeScreenDrawInfo(s, di); UnlockPubScreen(NULL, s);
    CloseLibrary((struct Library *)GfxBase); CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
