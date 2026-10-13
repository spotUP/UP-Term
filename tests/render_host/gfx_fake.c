/* See gfx_fake.h. Built for the host with the NDK's headers (tests/test_render.c). */
#include <string.h>
#include "gfx_fake.h"

/* not <stdlib.h>: on macOS it brings in the system's struct timeval, which the NDK's devices/timer.h redefines */
void *calloc(unsigned long, unsigned long);
void free(void *);
void abort(void);

#include <graphics/gfx.h>
#include <graphics/view.h>
#include <graphics/scale.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <devices/timer.h>
#include <utility/tagitem.h>
#include "../../render/outline.h"

gf_call gf_log[GF_LOG_MAX];
int gf_n;

struct GfxBase *GfxBase;
static struct GfxBase gfxbase;

static struct Window win;
static struct Screen scr;
static struct RastPort rport;
static struct BitMap bm;
static struct ColorMap cmap;
static struct TextFont font;
static struct DrawInfo dinfo;
static UWORD pens[16];
static int *canvas, cw_px, ch_px, W, H;

static gf_call *rec(enum gf_kind k, long a, long b, long c, long d)
{
    gf_call *g = &gf_log[gf_n < GF_LOG_MAX ? gf_n++ : GF_LOG_MAX - 1];
    memset(g, 0, sizeof(*g));
    g->kind = k;
    g->a = a;
    g->b = b;
    g->c = c;
    g->d = d;
    g->drmd = rport.DrawMode;
    return g;
}

void gf_open(int w, int h, int cw, int ch, int base)
{
    gf_close();
    W = w;
    H = h;
    cw_px = cw;
    ch_px = ch;
    canvas = (int *)calloc((size_t)(w * h), sizeof(int));
    memset(&win, 0, sizeof(win));
    memset(&scr, 0, sizeof(scr));
    memset(&rport, 0, sizeof(rport));
    memset(&bm, 0, sizeof(bm));
    memset(&font, 0, sizeof(font));
    memset(&dinfo, 0, sizeof(dinfo));
    font.tf_XSize = (UWORD)cw;
    font.tf_YSize = (UWORD)ch;
    font.tf_Baseline = (UWORD)base;
    pens[TEXTPEN] = 1;
    pens[BACKGROUNDPEN] = 0;
    dinfo.dri_Pens = pens;
    scr.ViewPort.ColorMap = &cmap;
    scr.RastPort.BitMap = &bm;
    rport.BitMap = &bm;
    rport.Font = &font;
    rport.DrawMode = JAM2;
    rport.Mask = 0xFF;
    win.Width = (WORD)w;
    win.Height = (WORD)h;
    win.RPort = &rport;
    win.WScreen = &scr;
    gfxbase.LibNode.lib_Version = 40;
    GfxBase = &gfxbase;
    gf_n = 0;
}

void gf_close(void)
{
    free(canvas);
    canvas = 0;
}

struct Window *gf_window(void) { return &win; }
struct TextFont *gf_font(void) { return &font; }

int gf_pixel(int x, int y)
{
    return x >= 0 && y >= 0 && x < W && y < H ? canvas[y * W + x] : -1;
}

char gf_cell(int col, int row)
{
    int v = gf_pixel(col * cw_px, row * ch_px);
    return v > 32 && v < 256 ? (char)v : ' ';
}

const char *gf_screen(void)
{
    static char buf[1024];
    int r, c, n = 0, rows = H / ch_px, cols = W / cw_px;
    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++)
            buf[n++] = gf_cell(c, r);
        buf[n++] = r + 1 < rows ? '|' : 0;
    }
    return buf;
}

void gf_clear_log(void) { gf_n = 0; }

int gf_count(enum gf_kind k)
{
    int i, n = 0;
    for (i = 0; i < gf_n; i++)
        n += gf_log[i].kind == k;
    return n;
}

const gf_call *gf_find(enum gf_kind k, int nth)
{
    int i;
    for (i = 0; i < gf_n; i++)
        if (gf_log[i].kind == k && !nth--)
            return &gf_log[i];
    return 0;
}

static void paint(long x0, long y0, long x1, long y1, int v, int flip)
{
    long x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (x >= 0 && y >= 0 && x < W && y < H)
                canvas[y * W + x] = flip ? canvas[y * W + x] ^ GF_FLIP : v;
}

/* ---- graphics.library ---------------------------------------------------- */

VOID Move(struct RastPort *rp, LONG x, LONG y)
{
    rec(GF_MOVE, x, y, 0, 0);
    rp->cp_x = (WORD)x;
    rp->cp_y = (WORD)y;
}

LONG Text(struct RastPort *rp, CONST_STRPTR s, ULONG n)
{
    ULONG i;
    long x = rp->cp_x, y = rp->cp_y - rp->Font->tf_Baseline;
    gf_call *g = rec(GF_TEXT, x, y, (long)n, 0);
    for (i = 0; i < n && i < sizeof(g->str) - 1; i++)
        g->str[i] = s[i];
    for (i = 0; i < n; i++, x += rp->Font->tf_XSize)
        paint(x, y, x + rp->Font->tf_XSize - 1, y + rp->Font->tf_YSize - 1, (unsigned char)s[i], 0);
    rp->cp_x = (WORD)x;
    return 0;
}

VOID SetAPen(struct RastPort *rp, ULONG pen) { rp->FgPen = (UBYTE)pen; }
VOID SetBPen(struct RastPort *rp, ULONG pen) { rp->BgPen = (UBYTE)pen; }
VOID SetDrMd(struct RastPort *rp, ULONG m) { rp->DrawMode = (UBYTE)m; }
VOID SetABPenDrMd(struct RastPort *rp, ULONG a, ULONG b, ULONG m)
{
    rp->FgPen = (UBYTE)a;
    rp->BgPen = (UBYTE)b;
    rp->DrawMode = (UBYTE)m;
}
ULONG SetWriteMask(struct RastPort *rp, ULONG m)
{
    rp->Mask = (UBYTE)m;
    return 1;
}
LONG SetFont(struct RastPort *rp, struct TextFont *f)
{
    rp->Font = f;
    return 0;
}
ULONG SetSoftStyle(struct RastPort *rp, ULONG style, ULONG enable)
{
    (void)rp;
    (void)enable;
    return style;
}
VOID SetAfPt(struct RastPort *rp, UWORD *p, BYTE n) { (void)rp; (void)p; (void)n; }

VOID RectFill(struct RastPort *rp, LONG x0, LONG y0, LONG x1, LONG y1)
{
    rec(GF_RECT, x0, y0, x1, y1);
    paint(x0, y0, x1, y1, GF_FILL | rp->FgPen, rp->DrawMode & COMPLEMENT);
}

VOID ScrollRaster(struct RastPort *rp, LONG dx, LONG dy, LONG x0, LONG y0, LONG x1, LONG y1)
{
    long x, y;
    rec(GF_SCROLL, dx, dy, y0, y1);
    /* dy > 0: the contents move up by dy, the rows below are vacated in the background pen */
    if (dx)
        abort(); /* the renderer scrolls rows only */
    if (dy > 0) {
        for (y = y0; y <= y1; y++)
            for (x = x0; x <= x1; x++)
                canvas[y * W + x] = y + dy <= y1 ? canvas[(y + dy) * W + x] : GF_FILL | rp->BgPen;
    } else if (dy < 0) {
        for (y = y1; y >= y0; y--)
            for (x = x0; x <= x1; x++)
                canvas[y * W + x] = y + dy >= y0 ? canvas[(y + dy) * W + x] : GF_FILL | rp->BgPen;
    }
}

LONG BltBitMap(CONST struct BitMap *s, LONG sx, LONG sy, struct BitMap *d, LONG dx, LONG dy, LONG w, LONG h,
               ULONG mt, ULONG mask, PLANEPTR tmp)
{
    (void)s; (void)sx; (void)sy; (void)d; (void)dx; (void)dy; (void)w; (void)h; (void)mt; (void)mask; (void)tmp;
    rec(GF_BLT, 0, 0, 0, 0);
    return 0;
}

VOID BltTemplate(CONST PLANEPTR src, LONG sx, LONG sm, struct RastPort *rp, LONG x, LONG y, LONG w, LONG h)
{
    (void)src; (void)sx; (void)sm;
    rec(GF_OTHER, x, y, w, h);
    paint(x, y, x + w - 1, y + h - 1, '?', 0); /* a template glyph: some glyph, not a code */
    (void)rp;
}

ULONG GetBitMapAttr(CONST struct BitMap *b, ULONG a)
{
    (void)b;
    return a == BMA_DEPTH ? 8 : 0; /* 8 bits, not BMF_STANDARD: the chunky path, no planar tricks */
}

VOID WaitBlit(VOID) { }
VOID Draw(struct RastPort *rp, LONG x, LONG y) { (void)rp; rec(GF_OTHER, x, y, 0, 0); }
LONG WritePixel(struct RastPort *rp, LONG x, LONG y) { (void)rp; rec(GF_OTHER, x, y, 0, 0); return 0; }
struct BitMap *AllocBitMap(ULONG sx, ULONG sy, ULONG d, ULONG f, CONST struct BitMap *fr)
{
    (void)sx; (void)sy; (void)d; (void)f; (void)fr;
    return 0;
}
VOID FreeBitMap(struct BitMap *b) { (void)b; }
VOID SetRast(struct RastPort *rp, ULONG pen)
{
    (void)rp;
    paint(0, 0, W - 1, H - 1, GF_FILL | (int)pen, 0);
}
VOID InitRastPort(struct RastPort *rp) { memset(rp, 0, sizeof(*rp)); }
VOID BitMapScale(struct BitScaleArgs *a) { (void)a; }
LONG WritePixelArray8(struct RastPort *rp, ULONG a, ULONG b, ULONG c, ULONG d, UBYTE *p, struct RastPort *t)
{
    (void)rp; (void)a; (void)b; (void)c; (void)d; (void)p; (void)t;
    return 0;
}
VOID WriteChunkyPixels(struct RastPort *rp, ULONG a, ULONG b, ULONG c, ULONG d, UBYTE *p, LONG m)
{
    (void)rp; (void)a; (void)b; (void)c; (void)d; (void)p; (void)m;
}
ULONG ObtainPen(struct ColorMap *cm, ULONG n, ULONG r, ULONG g, ULONG b, LONG f)
{
    (void)cm; (void)r; (void)g; (void)b; (void)f;
    return n == (ULONG)-1 ? 9 : n;
}
LONG ObtainBestPenA(struct ColorMap *cm, ULONG r, ULONG g, ULONG b, CONST struct TagItem *t)
{
    (void)cm; (void)r; (void)g; (void)b; (void)t;
    return 2;
}
VOID ReleasePen(struct ColorMap *cm, ULONG n) { (void)cm; (void)n; }
VOID SetRGB32(struct ViewPort *vp, ULONG n, ULONG r, ULONG g, ULONG b) { (void)vp; (void)n; (void)r; (void)g; (void)b; }
VOID GetRGB32(CONST struct ColorMap *cm, ULONG first, ULONG n, ULONG *t)
{
    ULONG i;
    (void)cm; (void)first;
    for (i = 0; i < n; i++)
        t[i * 3] = t[i * 3 + 1] = t[i * 3 + 2] = 0;
}
WORD TextLength(struct RastPort *rp, CONST_STRPTR s, ULONG n)
{
    (void)s;
    return (WORD)(n * rp->Font->tf_XSize);
}

/* ---- intuition, layers, exec, timer: the little the renderer asks ------------ */

struct DrawInfo *GetScreenDrawInfo(struct Screen *s) { (void)s; return &dinfo; }
VOID FreeScreenDrawInfo(struct Screen *s, struct DrawInfo *d) { (void)s; (void)d; }
VOID LockLayer(LONG dummy, struct Layer *l) { (void)dummy; (void)l; }
VOID UnlockLayer(struct Layer *l) { (void)l; }
struct Library *OpenLibrary(CONST_STRPTR n, ULONG v) { (void)n; (void)v; return 0; }
VOID CloseLibrary(struct Library *l) { (void)l; }
APTR AllocVec(ULONG n, ULONG req) { (void)req; return calloc(1, n ? n : 1); }
VOID FreeVec(APTR p) { free(p); }
ULONG TypeOfMem(CONST_APTR a) { (void)a; return 0; }
ULONG ReadEClock(struct EClockVal *e) { (void)e; return 0; }

/* cybergraphics (declared in amiga_render.c; VR_HOST_TEST turns the vbcc inline into a plain call) */
LONG vr_cgx_write_lut(struct Library *l, APTR src, UWORD sx, UWORD sy, UWORD smod, struct RastPort *rp, APTR ctab,
                      UWORD dx, UWORD dy, UWORD w, UWORD h, UBYTE fmt)
{
    (void)l; (void)src; (void)sx; (void)sy; (void)smod; (void)rp; (void)ctab; (void)dx; (void)dy; (void)w;
    (void)h; (void)fmt;
    return 0;
}
ULONG vr_cgx_fill(struct Library *l, struct RastPort *rp, UWORD x, UWORD y, UWORD w, UWORD h, ULONG argb)
{
    (void)l; (void)rp; (void)x; (void)y; (void)w; (void)h; (void)argb;
    return 0;
}

/* the outline font (render/outline.c, bullet.library): no outline font is ever open in these tests */
void vo_set_cell(vo_font *f, WORD cw, WORD ch, WORD base) { (void)f; (void)cw; (void)ch; (void)base; }
const UBYTE *vo_glyph(vo_font *f, ULONG cp, int cells, WORD *bpr) { (void)f; (void)cp; (void)cells; (void)bpr; return 0; }
const UBYTE *vo_mark(vo_font *f, ULONG cp, int cells, WORD *bpr) { (void)f; (void)cp; (void)cells; (void)bpr; return 0; }
