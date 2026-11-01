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
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
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
static struct BitMap **bm, **mask;
static short *iw, *ih;
static int nslots;
static long loads, restored;
static int tried, gbw, gbh;     /* the restore ran at the first draw; the box it ran for */

static void slot_free(int s)
{
    if (bm[s])
        FreeBitMap(bm[s]);
    if (mask[s])
        FreeBitMap(mask[s]);
    bm[s] = mask[s] = 0;
}

int ig_open(struct Screen *s, int slots)
{
    scr = s;
    if (!s || s->BitMap.Depth > MAX_DEPTH || s->RastPort.BitMap->Depth > MAX_DEPTH)
        return 0;
    if (!ic_init(&cache, slots + 8))
        return 0;
    nslots = cache.slots;
    bm = AllocVec(nslots * sizeof *bm, MEMF_ANY | MEMF_CLEAR);
    mask = AllocVec(nslots * sizeof *mask, MEMF_ANY | MEMF_CLEAR);
    iw = AllocVec(nslots * sizeof *iw, MEMF_ANY | MEMF_CLEAR);
    ih = AllocVec(nslots * sizeof *ih, MEMF_ANY | MEMF_CLEAR);
    if (!bm || !mask || !iw || !ih) {
        ig_close();
        return 0;
    }
    IconBase = OpenLibrary((STRPTR) "icon.library", 44);
    if (!IconBase)
        ig_close();
    return IconBase != 0;
}

long ig_loads(void)
{
    return loads;
}

long ig_restored(void)
{
    return restored;
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

/* ---- the persistent cache (icon_core.h file format) ---- */

/* WritePixelArray8 and ReadPixelArray8 want a one row temp RastPort at least as wide as the array row,
 * and an array whose rows are padded to a multiple of 16 bytes: the cache file stores dw bytes per row, so
 * the rows are copied one by one with ROW_STRIDE. */
#define ROW_STRIDE(w) (((w) + 15) & ~15)
typedef struct pixctx {
    struct BitMap *tbm;
    struct RastPort trp;
    unsigned char *buf;
    unsigned char *pm;          /* pens then mask of one icon, for the encoder */
} pixctx;

static int pix_open(pixctx *p)
{
    memset(p, 0, sizeof *p);
    p->tbm = AllocBitMap(IC_FILE_MAXDIM, 1, scr->RastPort.BitMap->Depth, 0, scr->RastPort.BitMap);
    p->buf = AllocVec(((IC_FILE_MAXDIM + 15) & ~15) * (IC_FILE_MAXDIM + 1), MEMF_ANY);
    p->pm = AllocVec(2 * IC_FILE_MAXPIX, MEMF_ANY);
    if (!p->tbm || !p->buf || !p->pm) {
        if (p->tbm)
            FreeBitMap(p->tbm);
        if (p->buf)
            FreeVec(p->buf);
        if (p->pm)
            FreeVec(p->pm);
        return 0;
    }
    InitRastPort(&p->trp);
    p->trp.BitMap = p->tbm;
    return 1;
}

static void pix_close(pixctx *p)
{
    FreeBitMap(p->tbm);
    FreeVec(p->buf);
    FreeVec(p->pm);
}

static void bm_rastport(struct RastPort *rp, struct BitMap *b)
{
    InitRastPort(rp);
    rp->BitMap = b;
}

/* Reads the file once and fills a slot per entry. Runs at the first draw: the box and depth are known. */
static void restore(int bw, int bh)
{
    BPTR f;
    long len;
    unsigned char *data;
    ic_dec *d;
    pixctx px;
    struct RastPort rp;
    int n, i, k, s, stride, depth = scr->RastPort.BitMap->Depth;

    f = Open((STRPTR)IG_CACHE_PATH, MODE_OLDFILE);
    if (!f)
        return;
    Seek(f, 0, OFFSET_END);
    len = Seek(f, 0, OFFSET_BEGINNING);
    data = len > IC_FILE_HEAD ? AllocVec(len, MEMF_ANY) : 0;
    if (!data || Read(f, data, len) != len) {
        Close(f);
        if (data)
            FreeVec(data);
        return;
    }
    Close(f);
    d = AllocVec(nslots * sizeof *d, MEMF_ANY);
    if (d && pix_open(&px)) {
        n = ic_file_decode(data, len, bw, bh, depth, d, nslots);
        for (i = 0; i < n; i++) {
            struct BitMap *a, *m;
            int npx = d[i].dw * d[i].dh;
            if (d[i].dw > bw || d[i].dh > bh)
                continue;
            a = make_bm(d[i].dw, d[i].dh);
            m = make_bm(d[i].dw, d[i].dh);
            if (!a || !m) {
                if (a)
                    FreeBitMap(a);
                if (m)
                    FreeBitMap(m);
                break;
            }
            stride = ROW_STRIDE(d[i].dw);
            for (k = 0; k < d[i].dh; k++)
                memcpy(px.buf + k * stride, d[i].pens + k * d[i].dw, d[i].dw);
            bm_rastport(&rp, a);
            WritePixelArray8(&rp, 0, 0, d[i].dw - 1, d[i].dh - 1, px.buf, &px.trp);
            for (k = 0; k < npx; k++)
                px.buf[k / d[i].dw * stride + k % d[i].dw] = d[i].mask[k] ? (unsigned char)((1UL << depth) - 1) : 0;
            bm_rastport(&rp, m);
            WritePixelArray8(&rp, 0, 0, d[i].dw - 1, d[i].dh - 1, px.buf, &px.trp);
            s = ic_claim(&cache, d[i].path);
            if (s < 0) {
                FreeBitMap(a);
                FreeBitMap(m);
                continue;
            }
            slot_free(s);
            bm[s] = a;
            mask[s] = m;
            iw[s] = (short)d[i].dw;
            ih[s] = (short)d[i].dh;
            ic_set(&cache, s, IC_READY);
            restored++;
        }
        WaitBlit();
        pix_close(&px);
    }
    if (d)
        FreeVec(d);
    FreeVec(data);
}

/* Writes every ready slot to IG_CACHE_TMP, then swaps it in (the way index.c writes the apps index). */
static void save(void)
{
    long size = IC_FILE_HEAD, off, w;
    unsigned char *data;
    pixctx px;
    struct RastPort rp;
    int i, k, npx, stride, depth = scr->RastPort.BitMap->Depth;
    BPTR f;

    for (i = 0; i < nslots; i++)
        if (cache.state[i] == IC_READY)
            size += (long)strlen(cache.path[i]) + 1 + 8 + 2L * iw[i] * ih[i];
    data = AllocVec(size, MEMF_ANY);
    if (!data)
        return;
    if (!pix_open(&px)) {
        FreeVec(data);
        return;
    }
    off = ic_file_head(data, size, gbw, gbh, depth);
    for (i = 0; off > 0 && i < nslots; i++) {
        unsigned char *pens, *mk;
        if (cache.state[i] != IC_READY)
            continue;
        npx = iw[i] * ih[i];
        pens = px.pm;
        mk = px.pm + IC_FILE_MAXPIX;
        stride = ROW_STRIDE(iw[i]);
        bm_rastport(&rp, bm[i]);
        ReadPixelArray8(&rp, 0, 0, iw[i] - 1, ih[i] - 1, px.buf, &px.trp);
        for (k = 0; k < ih[i]; k++)
            memcpy(pens + k * iw[i], px.buf + k * stride, iw[i]);
        bm_rastport(&rp, mask[i]);
        ReadPixelArray8(&rp, 0, 0, iw[i] - 1, ih[i] - 1, px.buf, &px.trp);
        for (k = 0; k < npx; k++)
            mk[k] = px.buf[k / iw[i] * stride + k % iw[i]] ? 1 : 0;
        w = ic_file_entry(data + off, size - off, cache.path[i], iw[i], ih[i], (gbw - iw[i]) / 2,
                          (gbh - ih[i]) / 2, pens, mk);
        off = w < 0 ? -1 : off + w;
    }
    pix_close(&px);
    if (off == size) {
        UnLock(CreateDir((STRPTR) "ENVARC:Amarchy")); /* fails when the drawer exists, which is fine */
        f = Open((STRPTR)IG_CACHE_TMP, MODE_NEWFILE);
        if (f) {
            w = Write(f, data, size);
            if (!Close(f) || w != size) {
                DeleteFile((STRPTR)IG_CACHE_TMP);
            } else {
                DeleteFile((STRPTR)IG_CACHE_PATH);
                if (!Rename((STRPTR)IG_CACHE_TMP, (STRPTR)IG_CACHE_PATH))
                    DeleteFile((STRPTR)IG_CACHE_TMP);
            }
        }
    }
    FreeVec(data);
}

void ig_close(void)
{
    int i;
    if (scr && gbw && loads > 0 && cache.slots)
        save();
    for (i = 0; bm && i < nslots; i++)
        slot_free(i);
    ic_free(&cache);
    if (bm)
        FreeVec(bm);
    if (mask)
        FreeVec(mask);
    if (iw)
        FreeVec(iw);
    if (ih)
        FreeVec(ih);
    bm = mask = 0;
    iw = ih = 0;
    nslots = 0;
    if (IconBase)
        CloseLibrary(IconBase);
    IconBase = 0;
    scr = 0;
    tried = gbw = gbh = 0;
}

int ig_draw(struct RastPort *rp, const char *path, int x, int y, int bw, int bh)
{
    int s, dx, dy;
    if (!IconBase)
        return 0;
    if (!tried) {
        tried = 1;
        gbw = bw;
        gbh = bh;
        restore(bw, bh);
    }
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
