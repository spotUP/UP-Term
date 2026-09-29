/* The slice of the Amiga API that DCTelnet's term-engine.c (retro32-term)
 * touches, on the host: enough to run its direct planar path into a
 * bitmap in host memory. The RastPort path is never taken here (no window
 * RastPort is given), so its calls are empty. */
#ifndef TE_SHIM_H
#define TE_SHIM_H
#include <string.h>

typedef unsigned char UBYTE;
typedef signed char BYTE;
typedef unsigned short UWORD;
typedef short WORD;
typedef unsigned long ULONG;
typedef long LONG;
typedef char *STRPTR;
typedef UBYTE *PLANEPTR;

#define JAM1 0
#define JAM2 1
#define COMPLEMENT 2
#define BMA_FLAGS 0
#define BMF_STANDARD 8

struct BitMap {
    UWORD BytesPerRow, Rows;
    UBYTE Flags, Depth;
    UWORD pad;
    PLANEPTR Planes[8];
};
struct RastPort {
    struct BitMap *BitMap;
    UBYTE Mask;
};
struct Screen {
    struct RastPort RastPort;
    WORD Width, Height;
};
struct TextFont {
    UWORD tf_YSize, tf_XSize, tf_Baseline, tf_Modulo;
    UBYTE tf_LoChar, tf_HiChar;
    void *tf_CharData, *tf_CharLoc;
};
struct Library {
    UWORD lib_Version;
};

/* V37: term_init_area then skips the RTG test (GetBitMapAttr is V39). */
static struct Library te_gfx = {37};
static struct Library *GfxBase = &te_gfx;

static ULONG GetBitMapAttr(struct BitMap *bm, ULONG a)
{
    (void)bm;
    (void)a;
    return BMF_STANDARD;
}

static void WaitBlit(void) {}
static void SetAPen(struct RastPort *rp, int p) { (void)rp; (void)p; }
static void SetBPen(struct RastPort *rp, int p) { (void)rp; (void)p; }
static void SetDrMd(struct RastPort *rp, int m) { (void)rp; (void)m; }
static void SetFont(struct RastPort *rp, struct TextFont *tf) { (void)rp; (void)tf; }
static void Move(struct RastPort *rp, int x, int y) { (void)rp; (void)x; (void)y; }
static void Text(struct RastPort *rp, STRPTR s, int n) { (void)rp; (void)s; (void)n; }
static void RectFill(struct RastPort *rp, int a, int b, int c, int d)
{
    (void)rp; (void)a; (void)b; (void)c; (void)d;
}
static void ScrollRaster(struct RastPort *rp, int dx, int dy, int a, int b, int c, int d)
{
    (void)rp; (void)dx; (void)dy; (void)a; (void)b; (void)c; (void)d;
}

/* BltBitMap for what term-engine asks of it: byte-aligned x and widths,
 * minterm 0xFF (set), 0x00 (clear) or 0xC0 (copy), one bitmap, planes by
 * mask. The copy is overlap-safe, as the blitter's descending mode is. */
static LONG BltBitMap(struct BitMap *s, WORD sx, WORD sy, struct BitMap *d, WORD dx, WORD dy,
                      WORD w, WORD h, UBYTE minterm, UBYTE mask, PLANEPTR tmp)
{
    int p, r, bytes = w >> 3;
    (void)tmp;
    for (p = 0; p < d->Depth; p++) {
        if (!(mask & (1 << p)))
            continue;
        if (minterm == 0xC0) {
            int down = dy > sy;
            for (r = 0; r < h; r++) {
                int rr = down ? h - 1 - r : r;
                memmove(d->Planes[p] + (long)(dy + rr) * d->BytesPerRow + (dx >> 3),
                        s->Planes[p] + (long)(sy + rr) * s->BytesPerRow + (sx >> 3), bytes);
            }
        } else {
            for (r = 0; r < h; r++)
                memset(d->Planes[p] + (long)(dy + r) * d->BytesPerRow + (dx >> 3),
                       minterm == 0xFF ? 0xFF : 0x00, bytes);
        }
    }
    return w;
}

#endif
