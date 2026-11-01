/* bprobe -- scratch probe (not committed): order of graphics calls on one window's
   layer during border redraws. Logs RectFill/Move/Draw/SetAPen/DrawImageState on
   window A's layer, with nesting depth inside DrawImageState and the caller address. */
#include <exec/types.h>
#include <exec/execbase.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/rastport.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <stdio.h>

static ULONG __stack = 32768;
static const char stackcookie[] = "$STACK:32768";

struct IntuitionBase *IntuitionBase; struct GfxBase *GfxBase;
struct ev { UBYTE t, dep, ph, pen; WORD a, b, c, d; ULONG ret; };
#define NEV 900
static struct ev evs[NEV];
static volatile int nev, armed, phase, dep, curpen;
static struct Layer *tl; static struct Window *wa;

/* caller return address: the LVO JMP leaves the caller's return address at the top
   of the stack when our function is entered; vbcc inline asm reads it from a5-free frame */
ULONG getret(void) = "\tmove.l\t(a7),d0"; /* placeholder, unused */

static void rec(UBYTE t, WORD a, WORD b, WORD c, WORD d, ULONG ret)
{
    if (nev < NEV) { struct ev *e = &evs[nev++]; e->t = t; e->dep = dep; e->ph = phase; e->pen = curpen;
        e->a = a; e->b = b; e->c = c; e->d = d; e->ret = ret; }
}

typedef void (*rf_t)(__reg("a1") struct RastPort *, __reg("d0") LONG, __reg("d1") LONG, __reg("d2") LONG, __reg("d3") LONG, __reg("a6") struct GfxBase *);
typedef void (*mv_t)(__reg("a1") struct RastPort *, __reg("d0") LONG, __reg("d1") LONG, __reg("a6") struct GfxBase *);
typedef void (*sp_t)(__reg("a1") struct RastPort *, __reg("d0") ULONG, __reg("a6") struct GfxBase *);
typedef void (*di_t)(__reg("a0") struct RastPort *, __reg("a1") struct Image *, __reg("d0") LONG, __reg("d1") LONG, __reg("d2") ULONG, __reg("a2") struct DrawInfo *, __reg("a6") struct IntuitionBase *);
static rf_t o_rf; static mv_t o_mv, o_dr; static sp_t o_sp; static di_t o_di;

static int mine(struct RastPort *rp) { return armed && rp && rp->Layer && (rp->Layer == tl || rp->Layer->Window == (APTR)wa); }

void s_rf(__reg("a1") struct RastPort *rp, __reg("d0") LONG x0, __reg("d1") LONG y0, __reg("d2") LONG x1, __reg("d3") LONG y1, __reg("a6") struct GfxBase *g)
{ if (mine(rp)) rec('R', x0, y0, x1, y1, 0); o_rf(rp, x0, y0, x1, y1, g); }
void s_mv(__reg("a1") struct RastPort *rp, __reg("d0") LONG x, __reg("d1") LONG y, __reg("a6") struct GfxBase *g)
{ if (mine(rp)) rec('M', x, y, 0, 0, 0); o_mv(rp, x, y, g); }
void s_dr(__reg("a1") struct RastPort *rp, __reg("d0") LONG x, __reg("d1") LONG y, __reg("a6") struct GfxBase *g)
{ if (mine(rp)) rec('D', rp->cp_x, rp->cp_y, x, y, 0); o_dr(rp, x, y, g); }
void s_sp(__reg("a1") struct RastPort *rp, __reg("d0") ULONG pen, __reg("a6") struct GfxBase *g)
{ if (mine(rp)) curpen = pen; o_sp(rp, pen, g); }
void s_di(__reg("a0") struct RastPort *rp, __reg("a1") struct Image *im, __reg("d0") LONG l, __reg("d1") LONG t, __reg("d2") ULONG st, __reg("a2") struct DrawInfo *dri, __reg("a6") struct IntuitionBase *ib)
{ int m = mine(rp); if (m) { rec('I', l, t, im ? im->Width : 0, im ? im->Height : 0, 0); dep++; }
  o_di(rp, im, l, t, st, dri, ib); if (m) { dep--; rec('i', l, t, 0, 0, 0); } }

static void step(int ph, const char *name)
{ printf("phase %d %s\n", ph, name); }

int main(void)
{
    struct Window *a, *b; int i, p;
    static const char *pn[] = { "", "RefreshWindowFrame", "deactivate", "activate", "resize", "toback", "tofront" };
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
    if (!IntuitionBase || !GfxBase) return 20;
    b = OpenWindowTags(NULL, WA_Left, 60, WA_Top, 40, WA_Width, 200, WA_Height, 80, WA_Title, (ULONG)"bprobe B",
        WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, TAG_END);
    a = OpenWindowTags(NULL, WA_Left, 20, WA_Top, 20, WA_Width, 220, WA_Height, 90, WA_Title, (ULONG)"bprobe A",
        WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_SizeGadget, TRUE, WA_Zoom, (ULONG)"\0\0\0\0\0\x80\0\x40",
        WA_Activate, TRUE, TAG_END);
    if (!a || !b) { printf("open failed\n"); return 20; }
    Delay(50);
    tl = a->WLayer; wa = a;
    Forbid();
    o_rf = (rf_t)SetFunction((struct Library *)GfxBase, -306, (APTR)s_rf);
    o_mv = (mv_t)SetFunction((struct Library *)GfxBase, -240, (APTR)s_mv);
    o_dr = (mv_t)SetFunction((struct Library *)GfxBase, -246, (APTR)s_dr);
    o_sp = (sp_t)SetFunction((struct Library *)GfxBase, -342, (APTR)s_sp);
    o_di = (di_t)SetFunction((struct Library *)IntuitionBase, -618, (APTR)s_di);
    Permit();
    for (p = 1; p <= 6; p++) {
        phase = p; armed = 1;
        switch (p) {
        case 1: RefreshWindowFrame(a); break;
        case 2: ActivateWindow(b); break;
        case 3: ActivateWindow(a); break;
        case 4: ChangeWindowBox(a, 20, 20, 260, 110); break;
        case 5: WindowToBack(a); break;
        case 6: WindowToFront(a); break;
        }
        Delay(100); armed = 0;
        tl = a->WLayer; wa = a;
    }
    Forbid();
    SetFunction((struct Library *)GfxBase, -306, (APTR)o_rf);
    SetFunction((struct Library *)GfxBase, -240, (APTR)o_mv);
    SetFunction((struct Library *)GfxBase, -246, (APTR)o_dr);
    SetFunction((struct Library *)GfxBase, -342, (APTR)o_sp);
    SetFunction((struct Library *)IntuitionBase, -618, (APTR)o_di);
    Permit();
    printf("intuition V%d.%d win %dx%d ev %d\n", (int)IntuitionBase->LibNode.lib_Version, (int)IntuitionBase->LibNode.lib_Revision, (int)a->Width, (int)a->Height, nev);
    for (i = 0; i < nev; i++) {
        struct ev *e = &evs[i];
        if (i == 0 || evs[i-1].ph != e->ph) step(e->ph, pn[e->ph]);
        printf("%c%d p%d %d,%d %d,%d\n", e->t, (int)e->dep, (int)e->pen, (int)e->a, (int)e->b, (int)e->c, (int)e->d);
    }
    CloseWindow(a); CloseWindow(b);
    return 0;
}
