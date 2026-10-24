/* icon_gfx: see icon_gfx.h. A slot holds the icon scaled to the size it is drawn at: a colour bitmap and a
 * one-plane-deep mask bitmap, both friends of the screen's bitmap. icon.library draws a palette icon with
 * its transparent pixels left alone, so the icon is drawn twice onto the offscreen bitmap, once on pen 0
 * and once on the last pen: a pixel that differs between the two is transparent. The equality (the first
 * plane of it) is the blit mask; BitMapScale scales colour and mask alike. */
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/scale.h>
#include <intuition/imageclass.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/icon.h>
#include <string.h>

#include "icon_core.h"
#include "icon_gfx.h"

#define MAX_DEPTH 8             /* pens above that are not "all planes set" on a chunky screen */

/* wblaunch.c defines IconBase and opens it for a launch; the menu closes this one (ig_close) first */
extern struct Library *IconBase;
extern struct GfxBase *GfxBase;

static struct Screen *scr;
static ic_cache cache;
static struct BitMap *bm[IC_SLOTS], *mask[IC_SLOTS];
static short iw[IC_SLOTS], ih[IC_SLOTS];
static long loads;

static void slot_free(int s)
{
    if (bm[s])
        FreeBitMap(bm[s]);
    if (mask[s])
        FreeBitMap(mask[s]);
    bm[s] = mask[s] = 0;
}

int ig_open(struct Screen *s)
{
    scr = s;
    if (!s || s->BitMap.Depth > MAX_DEPTH || s->RastPort.BitMap->Depth > MAX_DEPTH)
        return 0;
    IconBase = OpenLibrary((STRPTR) "icon.library", 44);
    return IconBase != 0;
}

void ig_close(void)
{
    int i;
    for (i = 0; i < IC_SLOTS; i++)
        slot_free(i);
    memset(&cache, 0, sizeof cache);
    if (IconBase)
        CloseLibrary(IconBase);
    IconBase = 0;
    scr = 0;
}

long ig_loads(void)
{
    return loads;
}

static struct BitMap *make_bm(int w, int h)
{
    return AllocBitMap((ULONG)w, (ULONG)h, scr->RastPort.BitMap->Depth, BMF_MINPLANES | BMF_CLEAR,
                       scr->RastPort.BitMap);
}

/* src scaled into dst (sw x sh into dw x dh), both of the same depth. */
static void scale(struct BitMap *src, int sw, int sh, struct BitMap *dst, int dw, int dh)
{
    struct BitScaleArgs a;
    memset(&a, 0, sizeof a);
    a.bsa_SrcWidth = (UWORD)sw;
    a.bsa_SrcHeight = (UWORD)sh;
    a.bsa_XSrcFactor = (UWORD)sw;
    a.bsa_XDestFactor = (UWORD)dw;
    a.bsa_YSrcFactor = (UWORD)sh;
    a.bsa_YDestFactor = (UWORD)dh;
    a.bsa_SrcBitMap = src;
    a.bsa_DestBitMap = dst;
    BitMapScale(&a);
    WaitBlit();
}

/* Loads the icon of path into slot s, scaled to a box bw x bh. 1 when the slot is filled. */
static int load(int s, const char *path, int bw, int bh)
{
    struct TagItem gt[4];
    struct TagItem dt[3];
    struct DiskObject *o;
    struct BitMap *c0 = 0, *c1 = 0, *sc = 0, *sx = 0;
    struct RastPort rp;
    int sw, sh, dw, dh, dx, dy, depth = scr->RastPort.BitMap->Depth, ok = 0;

    gt[0].ti_Tag = ICONGETA_Screen;               gt[0].ti_Data = (ULONG)scr;
    gt[1].ti_Tag = ICONGETA_GenerateImageMasks;   gt[1].ti_Data = TRUE;
    gt[2].ti_Tag = ICONGETA_FailIfUnavailable;    gt[2].ti_Data = TRUE;
    gt[3].ti_Tag = TAG_DONE;
    o = GetIconTagList((CONST_STRPTR)path, gt);
    if (!o)
        return 0;
    loads++;
    sw = o->do_Gadget.Width;
    sh = o->do_Gadget.Height;
    if (!ic_fit(sw, sh, bw, bh, &dw, &dh, &dx, &dy))
        goto out;
    dt[0].ti_Tag = ICONDRAWA_Borderless;          dt[0].ti_Data = TRUE;
    dt[1].ti_Tag = ICONDRAWA_EraseBackground;     dt[1].ti_Data = FALSE;
    dt[2].ti_Tag = TAG_DONE;
    c0 = make_bm(sw, sh);
    c1 = make_bm(sw, sh);
    sc = make_bm(dw, dh);
    sx = make_bm(dw, dh);
    if (!c0 || !c1 || !sc || !sx)
        goto out;
    InitRastPort(&rp);
    rp.BitMap = c0;
    SetRast(&rp, 0);
    DrawIconStateA(&rp, o, 0, 0, 0, IDS_NORMAL, dt);
    rp.BitMap = c1;
    SetRast(&rp, (1UL << depth) - 1);
    DrawIconStateA(&rp, o, 0, 0, 0, IDS_NORMAL, dt);
    WaitBlit();
    scale(c0, sw, sh, sc, dw, dh);
    /* c1 = not (c0 xor c1): set where the icon covered the pixel, which is where BltMaskBitMapRastPort copies */
    BltBitMap(c0, 0, 0, c1, 0, 0, sw, sh, 0x90, 0xFF, 0);
    WaitBlit();
    scale(c1, sw, sh, sx, dw, dh);
    bm[s] = sc;
    mask[s] = sx;
    sc = sx = 0;
    iw[s] = (short)dw;
    ih[s] = (short)dh;
    ok = 1;
out:
    if (c0)
        FreeBitMap(c0);
    if (c1)
        FreeBitMap(c1);
    if (sc)
        FreeBitMap(sc);
    if (sx)
        FreeBitMap(sx);
    FreeDiskObject(o);
    return ok;
}

int ig_draw(struct RastPort *rp, const char *path, int x, int y, int bw, int bh)
{
    int s, dx, dy;
    if (!IconBase)
        return 0;
    s = ic_find(&cache, path);
    if (s < 0) {
        s = ic_claim(&cache, path);
        if (s < 0)
            return 0;
        slot_free(s);
        ic_set(&cache, s, load(s, path, bw, bh) ? IC_READY : IC_FAILED);
    }
    if (cache.state[s] != IC_READY)
        return 0;
    /* a slot scaled for a box larger than this one is loaded again */
    if (iw[s] > bw || ih[s] > bh) {
        slot_free(s);
        ic_set(&cache, s, load(s, path, bw, bh) ? IC_READY : IC_FAILED);
        if (cache.state[s] != IC_READY)
            return 0;
    }
    dx = (bw - iw[s]) / 2;
    dy = (bh - ih[s]) / 2;
    BltMaskBitMapRastPort(bm[s], 0, 0, rp, x + dx, y + dy, iw[s], ih[s], 0xE0, (PLANEPTR)mask[s]->Planes[0]);
    return 1;
}
