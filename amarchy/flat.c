/* amarchy-flat: the resident flat-UI program (plan 2026-10-29-amarchy-flat-ui-patch.md FLAT-C0 / C1, OD-F8).
 *
 *   amarchy-flat            start: make the public state block "Amarchy.Flat", print its STATUS, wait for Ctrl-C
 *   amarchy-flat STATUS     print the STATUS line of the running program and return (flat=on|off|absent|held)
 *   amarchy-flat BOOT       the startup-sequence form (FLAT-C4): return at once, nothing resident, when neither
 *                           flat-system-look nor flat-window-gadgets is on in the Amarchy prefs; else start as above
 *
 * C1 swaps the h_SubEntry of sysiclass's dispatcher Hook (never h_Entry: one routine serves five classes) for a
 * pass-through trampoline that counts the call and tail-jumps to the saved routine, so every register and the
 * stack arrive unchanged (3.2 takes a0 / a2 / a1, 3.1 the C stack: one code path for both). No glyph is drawn yet
 * (C2). Ctrl-C removes the swap only if the slot is still ours; otherwise it reports still_chained and stays
 * inert. The swap model and the STATUS text are flat_core.c (host-tested).
 *
 * FLAT-D4 (runtime DOpus patch): when dopus5.library is resident and its version, revision and routine checksums are in
 * the table, the JMP operand of the DrawBox (LVO -696) and DrawFieldBox (-702) jump table entries is swapped with the
 * same flat_swap model for a routine that draws the flat outline while flat is on and the pens are flat (no shadow L), and
 * calls the saved routine otherwise. Any other build is inert (STATUS "dopus=<v.r> inert=..."). The path field is
 * not reachable by an LVO (button_class.c:900 calls draw_field_box inside the library) but its FieldBox body is hooked
 * (d4_fieldbody). Not reachable, so not patched: scroller knob and
 * arrows (dopusiclass IM_BBOX / arrows, image_class.c:261, :109), sort arrow (lister_title.c:972-987, inline in the
 * DirectoryOpus program). `amarchy-flat DOPUS` makes the running program look for DOpus again (Ctrl-D signal).
 *
 * FLAT-C4 kill switches (OD-F5, OD-F7; flat_core.h): the program starts INERT (no hook, no DOpus patch, STATUS
 * flat=held why=...) when ENVARC:Amarchy/flat-starting counts FLAT_MARKER_STRIKES starts (written before the swap
 * with the found count + 1, deleted 60 s later by a timer; two starts in a row that died early leave count 2, the
 * next start consumes it and stays inert; one early death is forgiven), when both mouse
 * buttons are down at start, or when VisualPrefs / Birdie runs or a foreign hook already sits on the sysiclass slot.
 * Return code: 0 done, 5 another copy is running, 10 no memory. */
#include <string.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <graphics/rastport.h>
#include <graphics/gfxbase.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
#include <intuition/screens.h>
#include <intuition/sghooks.h>
#include <utility/hooks.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include "../tty/qmark.h"
#include "flat_core.h"
#include "flat_shared.h"
#include "libpatch.h"
#include "flat_glyph.h"
#include "roundaa_core.h"
#include "menu_font.h"
#include "taskalive.h"
#include "poll_timer.h"
#include "amconf_core.h"
#include "amconf_dos.h"
#include "desk_core.h"
#include "iconpen_core.h"
#include "iconpen_dos.h"
#include "gthemes_core.h"
#include "gthemes_dos.h"
#include "iclabel_core.h"
#include "../render/aa_ink.h"
#include "../render/pen_match.h"
#include "stkprobe.h"
#include <graphics/view.h>
#include <intuition/intuitionbase.h>
#include <utility/tagitem.h>

static const char ver[] = "$VER: amarchy-flat 0.1 (30.10.2026) (UP-Term)";

/* RULES.md rule 6: the stack the program asks for, and the cookie the Shell and Workbench read. */
long __stack = STK_BYTES(16384);
const char amarchy_flat_stack_cookie[] = STK_COOKIE(16384);



struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
static flatshared *g_r;
/* FLAT-LOOP-BEAT: the main task names the step it enters (STATUS prints it; a loop stuck in a lock names that lock) */
#define STG(x) do { if (g_r) g_r->stage = (UWORD)(x); } while (0)
static int side_px(flatshared *r, struct Screen *scr);
static void aspect_update(flatshared *r, struct Screen *scr);

/* ---- FLAT-C2: the sysiclass dispatcher. It replaces the C1 pass-through: count the call, and for IM_DRAW of a
 * SYSIA_Which that flat_glyph.c knows, draw the flat glyph when the switch is on; every other call (other method,
 * other Which, switch off, held, box too small, OM_GET failing) goes to the saved routine with the arguments as
 * received. 3.2 passes hook / object / message in a0 / a2 / a1, 3.1 on the C stack (flat_conv), so there are two
 * entries over one body. The image is the object itself (struct Image: Width, Height); Which is read with GetAttr, never
 * from the instance. GetAttr(OM_GET) comes back through this dispatcher with a non-draw method: no recursion. */
typedef ULONG (*oldfn_reg)(__reg("a0") struct Hook *, __reg("a2") Object *, __reg("a1") Msg);
typedef ULONG (*oldfn_stk)(struct Hook *, Object *, Msg);

typedef void (*rectfn)(__reg("a1") struct RastPort *, __reg("d0") LONG, __reg("d1") LONG, __reg("d2") LONG, __reg("d3") LONG,
                       __reg("a6") struct Library *);

static void c2_fill(void *ctx, int x0, int y0, int x1, int y1, int pen)
{
    struct RastPort *rp = (struct RastPort *)ctx;
    SetAPen(rp, pen);
    RectFill(rp, x0, y0, x1, y1);
}

/* FLAT-BUTTON-ROUND: the pen already on the surface at (x, y), -1 off the layer. */
static int c2_read(void *ctx, int x, int y)
{
    return (int)ReadPixel((struct RastPort *)ctx, x, y);
}

/* RL19: the screen palette of the frame being drawn, read once per draw (raa_nearest asks for every pen of every corner pixel) */
#define RAA_PAL RAA_MAX_PENS
static unsigned long raa_pal[RAA_PAL];
static unsigned long c2_rgb(void *ctx, int pen)
{
    (void)ctx;
    return raa_pal[pen];
}

/* RL19: the pens a rastport's screen shows, into raa_pal; the count (0: none known, no antialiased corner) */
static int raa_load(struct RastPort *rp)
{
    struct Layer *lay = rp->Layer;
    struct Window *win = lay ? (struct Window *)lay->Window : NULL;
    ULONG rgb[3];
    int n, i;
    if (!win || !win->WScreen || !rp->BitMap || rp->BitMap->Depth > 8)
        return 0;
    n = 1 << rp->BitMap->Depth;
    if (n > RAA_PAL)
        n = RAA_PAL;
    for (i = 0; i < n; i++) {
        GetRGB32(win->WScreen->ViewPort.ColorMap, i, 1, rgb);
        raa_pal[i] = ((rgb[0] >> 24) << 16) | ((rgb[1] >> 24) << 8) | (rgb[2] >> 24);
    }
    return n;
}

/* FLAT-C7: a layer without a window is a screen's title bar: find the screen whose BarLayer it is and look through
 * Screen->FirstGadget (the depth gadget, type 0xc055) for the gadget that renders this image.
 * FLAT-HOOK-NO-IBASE: this runs on the drawing task, inside Intuition and layers calls that already hold locks, so it must
 * never take the Intuition lock (LockIBase here made Directory Opus' clock task and a lister task wait on each other: the
 * s71 freeze). The screen comes from the layer itself: a screen's layers hang off the Layer_Info embedded in struct Screen.
 * It counts only when the main task has published that screen (aspect_update's slot table), which proves the Layer_Info is
 * a screen's and not a program's own; the drawing task holds the layer lock, so that screen cannot close meanwhile. A
 * screen opened since the last update has no slot yet: its bar gadget draws stock until the next bar redraw. */
static int c7_screen_which(struct Layer *lay, Object *obj)
{
    struct Screen *sc;
    struct Gadget *g;
    int n, wh = -1;
    if (!g_r || !lay->LayerInfo)
        return -1;
    sc = (struct Screen *)((UBYTE *)lay->LayerInfo - offsetof(struct Screen, LayerInfo));
    if (!flat_aspect_known(g_r->aspect_slot, FLAT_ASPECT_SLOTS, sc) || sc->BarLayer != lay)
        return -1;
    for (g = sc->FirstGadget, n = 0; g && n < 64 && wh < 0; g = g->NextGadget, n++)
        wh = flat_gadget_which(g->GadgetType, g->GadgetRender, g->SelectRender, obj);
    return wh;
}


/* ---- FLAT-GLYPH-OS-SHAPES: the clone. The saved (system) dispatcher draws the real gadget image offscreen with a private
 * DrawInfo whose pen i is the marker pen i + 1; flat_clone_draw (flat_glyph.c) maps those markers to box / glyph and paints
 * them flat in the real RastPort. Any installed replacement of sysiclass underneath is cloned the same way. One cached
 * 8 bit offscreen bitmap, guarded by a semaphore (the dispatcher runs on every task that draws a window). */
typedef struct cloneenv {
    struct Hook *h;
    Object *obj;
    struct impDraw *d;
    int reg;
    ULONG size;                 /* the system image's SYSIA_Size: the private image must be made at the same size */
    ULONG which;                /* the system image's own SYSIA_Which (before flat_which_alias): the private image's key */
    int why;                    /* CLONE_WHY_*: set by clone_render when it gives up */
} cloneenv;
enum { CLONE_WHY_NONE = 0, CLONE_WHY_BUSY, CLONE_WHY_NODRI, CLONE_WHY_NOBM, CLONE_WHY_NOOBJ };

static struct SignalSemaphore cl_ss;
static struct BitMap *cl_bm;
static struct RastPort cl_rp;
static int cl_w, cl_h;
static UWORD cl_pens[FLAT_CLONE_MARKS];
static flat_clone_in cl_in;                         /* the call arguments, held under cl_ss (flat budget: 7 pushed arguments, not 18) */
/* S63-KS31-STACK: the clone runs on a private stack. It nests NewObjectA(sysiclass), AllocBitMap, the saved ROM dispatcher and a
 * ReadPixel loop; the drawing task can be any task (IPrefs had 1052 bytes free), so the call never runs on the caller's stack. */
#define CLONE_STACK_BYTES 16384
#define CLONE_STACK_FILL 0xA5
#define CLONE_STACK_SKIP 8     /* KS 3.1 writes 0xBAD1BAD3 into the first 4 bytes of a swapped-in stack (measured rig 97): not use, so the scan starts past it */
LONG flat_on_stack(LONG (*fn)(void *), void *arg, struct StackSwapStruct *sss);
#define CLONE_GUARD 16
static uint8_t *cl_base;                            /* the AllocVec block: CLONE_GUARD canary bytes, then the stack */
static uint8_t *cl_stk;                             /* allocated in main before the hooks install, freed after they are removed */
static struct StackSwapStruct cl_sss;               /* held under cl_ss */
static ULONG cl_hwp;                                /* first byte of cl_stk not yet seen written (high-water scan start), under cl_ss */
typedef struct { const flat_state *st; cloneenv *env; struct RastPort *rp; int rc; } cl_call;
static cl_call cl_arg;                              /* the stack function's argument, held under cl_ss */
static uint8_t cl_scratch[FLAT_CLONE_SCRATCH];     /* the grids: static (held under cl_ss), not on the stack of whatever task draws */
/* S32 CLONE-PRIVATE-IMAGE (flat_glyph.h): the private marker images. 3.2 sysiclass keeps the SYSIA_DrawInfo pointer and bakes
 * its pens at OM_NEW, so the marker DrawInfo is static (it outlives every private image) and dri_Depth is FLAT_CLONE_DEPTH. */
static struct DrawInfo cl_di;
static flat_clone_key cl_keys[FLAT_CLONE_OBJS];
static Object *cl_objs[FLAT_CLONE_OBJS];
static int cl_nobj;
static struct Task *cl_task;    /* the task inside the system dispatcher for the clone: its nested image draws go to the system */

static Object *clone_object(const struct DrawInfo *real, ULONG which, ULONG size, int w, int h)
{
    int i = flat_clone_slot(cl_keys, cl_nobj, FLAT_CLONE_OBJS, which, size, w, h);
    Object *o;
    if (i < 0)
        return NULL;
    if (i < cl_nobj)
        return cl_objs[i];
    if (!cl_nobj) {
        cl_di = *real;
        for (i = 0; i < FLAT_CLONE_MARKS; i++)
            cl_pens[i] = (UWORD)FLAT_CLONE_MARK(i);
        cl_di.dri_Pens = cl_pens;
        cl_di.dri_Depth = FLAT_CLONE_DEPTH;
        i = 0;
    }
    {
        struct TagItem t[6];
        t[0].ti_Tag = SYSIA_DrawInfo; t[0].ti_Data = (ULONG)&cl_di;
        t[1].ti_Tag = SYSIA_Which; t[1].ti_Data = which;
        t[2].ti_Tag = IA_Width; t[2].ti_Data = (ULONG)w;
        t[3].ti_Tag = IA_Height; t[3].ti_Data = (ULONG)h;
        t[4].ti_Tag = SYSIA_Size; t[4].ti_Data = size;
        t[5].ti_Tag = TAG_DONE; t[5].ti_Data = 0;
        o = (Object *)NewObjectA(NULL, (UBYTE *)"sysiclass", t);
    }
    if (!o)
        return NULL;
    cl_keys[i].which = which; cl_keys[i].size = size; cl_keys[i].w = w; cl_keys[i].h = h;
    cl_objs[i] = o;
    cl_nobj = i + 1;
    return o;
}

static int clone_ensure(int w, int h)
{
    if (cl_bm && w <= cl_w && h <= cl_h)
        return 1;
    if (w < cl_w) w = cl_w;
    if (h < cl_h) h = cl_h;
    if (GfxBase->LibNode.lib_Version < 39)
        return 0;
    {
        struct BitMap *bm = AllocBitMap((ULONG)w, (ULONG)h, 8, BMF_CLEAR, NULL);
        if (!bm)
            return 0;
        if (cl_bm)
            FreeBitMap(cl_bm);
        cl_bm = bm;
        cl_w = w;
        cl_h = h;
        InitRastPort(&cl_rp);
        cl_rp.BitMap = cl_bm;
    }
    return 1;
}

static void clone_free(void)
{
    while (cl_nobj > 0)
        DisposeObject(cl_objs[--cl_nobj]);
    if (cl_bm) {
        WaitBlit();
        FreeBitMap(cl_bm);
        cl_bm = NULL;
    }
}

static int clone_render(void *ctx, unsigned long ids, uint8_t *grid, int w, int h)
{
    cloneenv *e = (cloneenv *)ctx;
    struct Image *im = (struct Image *)e->obj;
    struct impDraw m = *e->d;
    Object *po;
    int x, y, ok = 0;
    (void)ids;
    if (!e->d->imp_DrInfo) {
        e->why = CLONE_WHY_NODRI;
        return 0;
    }
    if (!AttemptSemaphore(&cl_ss)) {
        e->why = CLONE_WHY_BUSY;
        return 0;
    }
    if (!clone_ensure(w, h))
        e->why = CLONE_WHY_NOBM;
    else if (cl_task = FindTask(NULL), !(po = clone_object(e->d->imp_DrInfo, e->which, e->size, im->Width, im->Height))) {
        cl_task = NULL;
        e->why = CLONE_WHY_NOOBJ;
    } else {
        /* the private image, baked with the marker pens, drawn by the system dispatcher with its column 0 at grid column 0:
         * 3.2 gives depth and zoom LeftEdge -1 (S32-CLONE-CLIP), and flat_clone_draw paints at the image's true origin */
        m.imp_RPort = &cl_rp;
        m.imp_Offset.X = flat_clone_offset(((struct Image *)po)->LeftEdge);
        m.imp_Offset.Y = flat_clone_offset(((struct Image *)po)->TopEdge);
        m.imp_DrInfo = &cl_di;
        SetRast(&cl_rp, 0);
        if (e->reg)
            ((oldfn_reg)g_r->sw.saved)(e->h, po, (Msg)&m);
        else
            ((oldfn_stk)g_r->sw.saved)(e->h, po, (Msg)&m);
        cl_task = NULL;
        WaitBlit();
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                LONG p = ReadPixel(&cl_rp, x, y);      /* offscreen bitmap, no layer: not the screen */
                grid[y * w + x] = (uint8_t)(p < 0 ? 0 : p);
            }
        ok = 1;
    }
    ReleaseSemaphore(&cl_ss);
    return ok;
}

/* FLAT-OUTER-EDGE ring fill: the saved (unpatched) RectFill, so the ring refill is not recoloured by the patch again. */
static void c2_ring_fill(void *ctx, int x0, int y0, int x1, int y1, int pen)
{
    struct RastPort *rp = (struct RastPort *)ctx;
    SetAPen(rp, (UBYTE)pen);
    ((rectfn)g_r->rsw.saved)(rp, x0, y0, x1, y1, (struct Library *)GfxBase);
}

/* TE-10.6b: pens per screen. g_pens holds the three/four pen numbers of each screen that has any (identity = screen pointer + that
 * slot's epoch, flat_pens_tab); the look block r->st stays global. A hook takes the pens of the screen of the window it draws in
 * (st_scr / st_rp, through g_draw below), never another screen's: a screen with no slot has no pens (0) and the pen functions fall back from that.
 * The Workbench slot also stores into r->st (the status line and the gate checks read it), so Workbench output is what it was.
 * No window (a menu or screen bar rastport): the Workbench pens in r->st, as before. g_wb_scr = the Workbench screen of the last pens_run
 * (only the main task writes it): the epoch of that screen is ipd_epoch(), of any other ipd_epoch_of_screen(). */
static flat_pens_tab g_pens;
static const void *g_wb_scr;

static unsigned scr_epoch(const void *scr)
{
    return scr && scr == g_wb_scr ? ipd_epoch() : ipd_epoch_of_screen(scr);
}

/* TE-10.6b budget fix (FLAT-C3): the hooks run on the stack of whichever task draws, so they copy no flat_state and call no
 * iconpen function. The main task keeps g_draw (flat_draw_tab: one flat_state per listed non-Workbench screen, written by draw_sync
 * under Forbid) and a hook only gets a pointer: the Workbench screen and no window = &r->st (live, as on main), a listed screen =
 * its slot, any other screen = the look with the four own pens 0. */
static flat_draw_tab g_draw;

static const flat_state *st_scr(flatshared *r, const struct Screen *scr)
{
    return flat_draw_state(&g_draw, &r->st, scr);
}

static const flat_state *st_rp(flatshared *r, const struct RastPort *rp)
{
    struct Window *w = rp && rp->Layer ? (struct Window *)rp->Layer->Window : NULL;
    return st_scr(r, w ? w->WScreen : NULL);
}

/* S32-ARROW-OUTLINE: the box fill ran over the window's outer ring where the gadget reaches the last column or row;
 * the ring belongs to flat, so refill the overlap with the edge pen through the saved RectFill. A function of its own, with the
 * rects handed to c2_ring_fill one by one, so no rect array is part of c2_draw's frame (flat budget). */
/* The ring of the window the image draws in: 0 = none to protect (no window, outline off, RectFill patch not saved, borderless). */
static int c2_ring_get(flatshared *r, struct impDraw *d, flat_ring *ring)
{
    struct Window *rw = d->imp_RPort->Layer ? (struct Window *)d->imp_RPort->Layer->Window : NULL;
    if (!rw || !r->st.pen_outline || !r->rsw.saved || (rw->Flags & WFLG_BORDERLESS))
        return 0;
    ring->w = rw->Width; ring->h = rw->Height; ring->px = side_px(r, rw->WScreen); ring->fill = c2_ring_fill;
    ring->pen = flat_edge_pen(st_scr(r, rw->WScreen), d->imp_RPort->BitMap ? d->imp_RPort->BitMap->Depth : 0, (rw->Flags & WFLG_WINDOWACTIVE) != 0);
    return 1;
}

static void c2_ring_refill(flatshared *r, struct impDraw *d, struct Image *im, int drawn)
{
    flat_ring ring;
    UBYTE fg = d->imp_RPort->FgPen;
    if (c2_ring_get(r, d, &ring) &&
        flat_ring_refill_after(drawn, &ring, im->LeftEdge + d->imp_Offset.X, im->TopEdge + d->imp_Offset.Y, im->Width, im->Height,
                               d->imp_RPort) > 0)
        SetAPen(d->imp_RPort, fg);
}

/* The pens one gadget image draws with (the pen of the frame it sits on, the knob and text pens, the title band rows), short so the
 * set is 10 bytes on the draw task's stack. */
typedef struct { short frame, knob, text, band, edge; } c2_pens;

static void c2_get_pens(flatshared *r, struct impDraw *d, struct Image *im, ULONG which, c2_pens *p)
{
    p->frame = -1; p->knob = -1; p->text = -1; p->band = 9999; p->edge = -1;
    /* FLAT-GADGET-BG: the pen of the frame the gadget sits on, so the box shows only the glyph lines. A window gadget
     * sits on the border (the background pen in both states: FLAT-SOLID-FILL); a screen bar gadget on
     * the bar block (V39+ DrawInfo has it). */
    if (d->imp_DrInfo) {
        struct Layer *fl = d->imp_RPort->Layer;
        if (fl && fl->Window) {
            int wdepth = d->imp_RPort->BitMap ? d->imp_RPort->BitMap->Depth : 0;
            int wactive = (((struct Window *)fl->Window)->Flags & WFLG_WINDOWACTIVE) != 0;
            const flat_state *ps = st_rp(r, d->imp_RPort);
            int band = flat_frame_pen(ps, wdepth, wactive);
            p->knob = flat_knob_state_pen(ps, wdepth, wactive);   /* S32-ARROW-KNOB-COLOUR: arrows and size draw in the scroller knob's pen */
            if (band >= 0 && flat_glyph_on_band(which)) {   /* the box is the title band (S32-INACTIVE-BAR: both states); the glyph takes the title text's pen */
                p->frame = band;
                p->band = flat_clone_band_rows(im->TopEdge + d->imp_Offset.Y, im->Height, ((struct Window *)fl->Window)->BorderTop);
                p->edge = d->imp_DrInfo->dri_Pens[BACKGROUNDPEN];
                p->text = r->st.pen_ptext;
            } else {
                p->frame = d->imp_DrInfo->dri_Pens[BACKGROUNDPEN];     /* FLAT-SOLID-FILL: the frame is the background */
                if (!wactive && flat_inactive_pen(ps, wdepth) >= 0)
                    p->text = flat_inactive_pen(ps, wdepth);       /* S32-INACTIVE-BAR: the arrows of an inactive window */
            }
        } else if (fl && d->imp_DrInfo->dri_Version >= 2)
            p->frame = d->imp_DrInfo->dri_Pens[BARBLOCKPEN];
    }
}

/* The record of a clone whose render put no pixel in the offscreen rastport (what the image was, for the diagnostics). Its own function
 * so its locals are not part of c2_clone's frame (flat budget). Called under cl_ss: it reads cl_scratch. */
static void c2_clone_note_empty(flatshared *r, struct impDraw *d, struct Image *im, ULONG which)
{
    int i, nz = 0;
    ULONG mk = 0;
    for (i = 0; i < im->Width * im->Height && i < FLAT_CLONE_MAX * FLAT_CLONE_MAX; i++)
        if (cl_scratch[i]) {
            nz++;
            mk |= 1UL << (cl_scratch[i] & 31);
        }
    r->cf_ids |= 1UL << (d->imp_State & 31);
    r->cf_lwhich = which; r->cf_lsize = ((ULONG)im->Width << 8) | (ULONG)im->Height; r->cf_lnz = (ULONG)nz; r->cf_lmarks = mk;
}

/* The counters of a failed clone draw, by the reason the environment recorded (flat budget: a function of its own). */
static void c2_clone_note_fail(flatshared *r, ULONG which, int got, int why)
{
    r->cfall++;                     /* no offscreen bitmap / busy / no DrawInfo / empty image: the block glyph below */
    if (!got || why == CLONE_WHY_BUSY)
        r->cf_busy++;
    else if (why == CLONE_WHY_NODRI)
        r->cf_nodri++;
    else if (why == CLONE_WHY_NOBM || why == CLONE_WHY_NOOBJ)
        r->cf_nobm++;               /* no offscreen bitmap, or no private marker image */
    else
        r->cf_empty++;              /* render ran, the system image put no pixel in the offscreen rastport */
    r->cf_mask |= 1UL << (which & 31);
}

/* The function that runs on the private stack (flat_on_stack): the whole clone draw. Under cl_ss. */
static LONG clone_on_private(void *a)
{
    cl_call *c = (cl_call *)a;
    c->rc = flat_clone_draw_in(c->st, &cl_in, clone_render, c->env, c2_fill, c->rp, cl_scratch);
    return 0;
}

/* Allocate the clone stack once, before the hooks install (main task). Filled with a pattern for the high-water mark. */
static void clone_stack_alloc(flatshared *r)
{
    cl_base = (uint8_t *)AllocVec(CLONE_STACK_BYTES + CLONE_GUARD, MEMF_PUBLIC);
    if (!cl_base)
        return;
    memset(cl_base, 0x5A, CLONE_GUARD);
    cl_stk = cl_base + CLONE_GUARD;
    memset(cl_stk, CLONE_STACK_FILL, CLONE_STACK_BYTES);
    cl_sss.stk_Lower = cl_stk;
    cl_sss.stk_Upper = (ULONG)(cl_stk + CLONE_STACK_BYTES);
    cl_sss.stk_Pointer = (APTR)(cl_stk + CLONE_STACK_BYTES);
    cl_hwp = CLONE_STACK_SKIP;
    r->cstk = CLONE_STACK_BYTES;
}

/* After the hooks are removed and cl_ss is held: nothing can be on the stack any more. */
static void clone_stack_free(void)
{
    if (cl_base) {
        FreeVec(cl_base);
        cl_base = NULL;
        cl_stk = NULL;
    }
}

/* Diagnostics, under cl_ss: the drawing task's free stack before the swap (least seen, and whose), the private stack high-water. */
static void clone_stack_note(flatshared *r, int after)
{
    if (!after) {
        struct Task *me = FindTask(NULL);
        ULONG free_b = (ULONG)((char *)&me - (char *)me->tc_SPLower);
        const char *nm = me->tc_Node.ln_Name ? me->tc_Node.ln_Name : "?";
        int i;
        if (r->cmin && free_b >= r->cmin)
            return;
        r->cmin = free_b;
        for (i = 0; i < 15 && nm[i]; i++)
            r->ctask[i] = nm[i];
        r->ctask[i] = 0;
        return;
    }
    cl_hwp = CLONE_STACK_SKIP;  /* rescan from the bottom every time: a later, deeper draw dirties bytes below the last stop */
    while (cl_hwp < CLONE_STACK_BYTES && cl_stk[cl_hwp] == CLONE_STACK_FILL)
        cl_hwp++;
    r->chw = CLONE_STACK_BYTES - cl_hwp;
    {   /* S63 probe: is byte 0 a stray write (a few bottom bytes) or real depth (the high-water ignoring the bottom 256)? */
        ULONG i, n = 0, g = 0;
        for (i = 0; i < 256; i++)
            n += cl_stk[i] == CLONE_STACK_FILL;
        for (i = 0; i < CLONE_GUARD; i++)
            g += cl_base[i] == 0x5A;
        for (i = 256; i < CLONE_STACK_BYTES && cl_stk[i] == CLONE_STACK_FILL; i++)
            ;
        r->cb256 = n; r->cguard = g; r->cdeep = CLONE_STACK_BYTES - i;
        r->cb8a = ((ULONG)cl_stk[0] << 24) | ((ULONG)cl_stk[1] << 16) | ((ULONG)cl_stk[2] << 8) | cl_stk[3];
        r->cb8b = ((ULONG)cl_stk[4] << 24) | ((ULONG)cl_stk[5] << 16) | ((ULONG)cl_stk[6] << 8) | cl_stk[7];
        r->chdr = *(ULONG *)(cl_base - 4);
    }
}

/* DOPUS-ARROW-WHICH: the arrow images made after flat started (flat_img_record holds the rule). Hooks run in many tasks, so the
 * table is touched under Forbid as the other shared tables are; the walk is FLAT_IMG_RECS (256) entries: each app scroller window holds 2-4 arrow images. A full table leaves the image ROM-drawn. */
static flat_img_rec img_tab[FLAT_IMG_RECS];
static int img_lookup(Object *obj, ULONG *size)
{
    int wh;
    Forbid();
    wh = flat_img_which(img_tab, FLAT_IMG_RECS, obj, size);
    Permit();
    return wh;
}

/* OM_NEW of sysiclass: read SYSIA_Which / SYSIA_Size from the taglist (TAG_MORE / IGNORE / SKIP followed) and record the new object. */
static void img_note_new(Object *obj, struct opSet *ops)
{
    const struct TagItem *t = ops->ops_AttrList;
    ULONG which = 0xFFFF, size = SYSISIZE_MEDRES, guard = 0;
    int ok = 0;
    for (; t && guard < 64; guard++) {
        if (t->ti_Tag == TAG_DONE) break;
        if (t->ti_Tag == TAG_MORE) { t = (const struct TagItem *)t->ti_Data; continue; }
        if (t->ti_Tag == TAG_SKIP) { t += t->ti_Data + 1; continue; }
        if (t->ti_Tag == SYSIA_Which) { which = t->ti_Data; ok = 1; }
        else if (t->ti_Tag == SYSIA_Size) size = t->ti_Data;
        else if (t->ti_Tag == SYSIA_DrawInfo && t->ti_Data == (ULONG)&cl_di) return;     /* flat's own private clone image */
        t++;
    }
    if (!ok)
        return;
    Forbid();
    flat_img_record(img_tab, FLAT_IMG_RECS, obj, which, size);
    Permit();
}

static void img_note_dispose(Object *obj)
{
    Forbid();
    flat_img_forget(img_tab, FLAT_IMG_RECS, obj);
    Permit();
}

/* The clone draw of one gadget image: 1 when it was drawn (the caller is done), 0 to fall back to the block glyph. Its own function
 * so the clone's environment is not part of c2_draw's frame (flat budget). */
static int c2_clone(struct Hook *hk, Object *obj, Msg msg, int reg, ULONG which, ULONG raw_which, const c2_pens *p)
{
    flatshared *r = g_r;
    struct impDraw *d = (struct impDraw *)msg;
    struct Image *im = (struct Image *)obj;
    cloneenv env;
    int rc, got;
    env.size = SYSISIZE_MEDRES;     /* the ROM default when SYSIA_Size is not given (and not gettable on every ROM) */
    if (GetAttr(SYSIA_Size, obj, &env.size) == 0)
        img_lookup(obj, &env.size);     /* 3.1: no OM_GET; the size recorded at OM_NEW */
    env.h = hk; env.obj = obj; env.d = d; env.reg = reg; env.which = raw_which; env.why = CLONE_WHY_NONE;
    got = AttemptSemaphore(&cl_ss);
    if (got) {                  /* cl_in is guarded by cl_ss like cl_scratch: off the draw task's stack */
        cl_in.which = which; cl_in.method = msg->MethodID; cl_in.ids = d->imp_State;
        cl_in.ox = im->LeftEdge + d->imp_Offset.X; cl_in.oy = im->TopEdge + d->imp_Offset.Y; cl_in.w = im->Width; cl_in.h = im->Height;
        cl_in.frame_pen = p->frame; cl_in.knob_pen = p->knob; cl_in.text_pen = p->text; cl_in.band_rows = p->band; cl_in.edge_pen = p->edge;
    }
    rc = FLAT_CLONE_FAIL;
    if (got && !cl_stk) {       /* no private stack: never run the clone on the caller's stack */
        r->cnostk++;
        env.why = CLONE_WHY_BUSY;
    } else if (got) {
        clone_stack_note(r, 0);
        cl_arg.st = st_rp(r, d->imp_RPort); cl_arg.env = &env; cl_arg.rp = d->imp_RPort; cl_arg.rc = FLAT_CLONE_FAIL;
        flat_on_stack(clone_on_private, &cl_arg, &cl_sss);
        clone_stack_note(r, 1);
        rc = cl_arg.rc;
    }
    if (got && rc == FLAT_CLONE_FAIL && env.why == CLONE_WHY_NONE)
        c2_clone_note_empty(r, d, im, which);
    if (got)
        ReleaseSemaphore(&cl_ss);
    if (rc == FLAT_CLONE_OK) {
        c2_ring_refill(r, d, im, 1);
        r->cdraws++;
        return 1;
    }
    if (rc == FLAT_CLONE_FAIL)
        c2_clone_note_fail(r, which, got, env.why);
    return 0;
}

static int c2_glyph(Object *obj, Msg msg, ULONG which, const c2_pens *p)
{
    flatshared *r = g_r;
    struct impDraw *d = (struct impDraw *)msg;
    struct Image *im = (struct Image *)obj;
    struct Layer *lay = d->imp_RPort ? d->imp_RPort->Layer : NULL;
    struct Window *w = lay ? (struct Window *)lay->Window : NULL;
    /* S69-ARROW-ASPECT: side from side_px of the drawing screen, the one per-screen aspect source (not dri_Resolution) */
    int side = w && w->WScreen ? side_px(r, w->WScreen) : d->imp_DrInfo && d->imp_DrInfo->dri_Screen ? side_px(r, d->imp_DrInfo->dri_Screen) : 1;
    int drawn = flat_glyph_draw(st_rp(r, d->imp_RPort), which, msg->MethodID, d->imp_State, im->LeftEdge + d->imp_Offset.X,
                           im->TopEdge + d->imp_Offset.Y, im->Width, im->Height, p->frame, p->knob, p->text, side,
                           c2_fill, d->imp_RPort);
    c2_ring_refill(r, d, im, drawn);    /* S71-DOPUS-BORDER-BREAK: the box fill covered the window outline; same refill as the clone path */
    return drawn;
}
#define NOWHICH 0xFFFFFFFFUL
/* Kickstart 3.1 sysiclass does not answer OM_GET: find the window gadget that renders this image. The layer
 * of the rastport belongs to the window being drawn; reading its gadget list from Intuition's own drawing
 * context needs no Forbid (the list only changes through Add/RemoveGList by the window's owner). NOWHICH when none does. */
static ULONG c2_which_31(Object *obj, struct impDraw *d)
{
    struct Layer *lay = d->imp_RPort->Layer;
    struct Window *w = lay ? (struct Window *)lay->Window : NULL;
    struct Gadget *g;
    int n = 0, wh = -1;
    /* system gadgets first: a zoom image recorded at OM_NEW (DOPUS-ZOOM-WHICH) is also a real WZOOM gadget on a normal window */
    for (g = w ? w->FirstGadget : NULL; g && n < 64 && wh < 0; g = g->NextGadget, n++)
        wh = flat_gadget_which(g->GadgetType, g->GadgetRender, g->SelectRender, obj);
    if (wh < 0)
        wh = img_lookup(obj, NULL);     /* DOPUS-ARROW-WHICH: an app-made arrow / zoom image, recorded at its OM_NEW */
    if (wh < 0 && lay && !w)
        wh = c7_screen_which(lay, obj);
    return wh < 0 ? NOWHICH : (ULONG)wh;
}

static int c2_draw(struct Hook *hk, Object *obj, Msg msg, int reg)
{
    flatshared *r = g_r;
    struct impDraw *d = (struct impDraw *)msg;
    struct Image *im = (struct Image *)obj;
    ULONG which = 0xFFFF, raw_which;
    c2_pens pens;
    r->st.calls++;
    if (msg->MethodID != FLAT_IM_DRAW || !r->st.enabled || r->st.held || !d->imp_RPort)
        return 0;
    if (cl_task && cl_task == FindTask(NULL))
        return 0;                   /* a nested image draw inside the clone's own system call: the system draws it */
    /* FLAT-OUTER-EDGE: the window's 1 px outer edge in the accent pen is NOT drawn here. Intuition repaints the four outer
     * border edges last in every frame redraw, as four 1 px RectFill calls through graphics.library, which erases anything
     * drawn from this hook. The ring is painted by the RectFill patch (flat_rf_c, below), which recolours exactly those fills. */
    if (GetAttr(SYSIA_Which, obj, &which) == 0 && (which = c2_which_31(obj, d)) == NOWHICH)
        return 0;
    raw_which = which;
    which = flat_which_alias(which);
    c2_get_pens(r, d, im, which, &pens);
    if (flat_clone_route(&r->st, which, msg->MethodID) && c2_clone(hk, obj, msg, reg, which, raw_which, &pens))
        return 1;
    return c2_glyph(obj, msg, which, &pens);
}

ULONG c2_disp_reg(__reg("a0") struct Hook *h, __reg("a2") Object *obj, __reg("a1") Msg msg)
{
    ULONG rc;
    if (c2_draw(h, obj, msg, 1))
        return TRUE;
    if (msg->MethodID == OM_DISPOSE)
        img_note_dispose(obj);
    rc = ((oldfn_reg)g_r->sw.saved)(h, obj, msg);
    if (msg->MethodID == OM_NEW && rc)
        img_note_new((Object *)rc, (struct opSet *)msg);
    return rc;
}

/* FLAT-UNLOAD-INFLIGHT: c2_disp_stk and c8_disp_stk are entered through the class gate (flat_dg.s) by a plain jsr, so the stack
 * holds [gate ret][caller ret][hook][obj][msg]: the leading caller_return parameter absorbs the caller's own return address
 * and is never read. The register-convention entries need nothing: their registers pass through the gate untouched. */
/* DOPUS-LOCK-ICONIFY: dopusiclass IM_DRAW of the title-bar iconify and lock images. type is the first field of the
 * instance data (BoopsiImageData.type, a short). 0 = not ours: the stock dispatcher draws. */
static int d5_draw(struct Hook *h, Object *obj, Msg msg)
{
    flatshared *r = g_r;
    struct impDraw *d = (struct impDraw *)msg;
    struct Image *im = (struct Image *)obj;
    c2_pens pens;
    int which = flat_dopus_image_which(*(short *)((UBYTE *)obj + ((struct IClass *)h)->cl_InstOffset));
    if (which < 0 || !r->st.enabled || r->st.held || !d->imp_RPort || !d->imp_DrInfo)
        return 0;
    c2_get_pens(r, d, im, (ULONG)which, &pens);
    if (!c2_glyph(obj, msg, (ULONG)which, &pens))
        return 0;
    r->d5draws++;
    return 1;
}

ULONG d5_disp_reg(__reg("a0") struct Hook *h, __reg("a2") Object *obj, __reg("a1") Msg msg)
{
    if (msg->MethodID == FLAT_IM_DRAW && obj && d5_draw(h, obj, msg))
        return TRUE;
    return ((oldfn_reg)g_r->d5sw.saved)(h, obj, msg);
}

ULONG c2_disp_stk(void *caller_return, struct Hook *h, Object *obj, Msg msg)
{
    ULONG rc;
    if (c2_draw(h, obj, msg, 0))
        return TRUE;
    if (msg->MethodID == OM_DISPOSE)
        img_note_dispose(obj);
    rc = ((oldfn_stk)g_r->sw.saved)(h, obj, msg);
    if (msg->MethodID == OM_NEW && rc)
        img_note_new((Object *)rc, (struct opSet *)msg);
    return rc;
}

/* ---- FLAT-C8: the frameiclass dispatcher. Under flat pens (shine == background) the stock bevel leaves its shadow half
 * as a one-sided right/bottom L; flat_frame_decide says draw flat, flat_frame_draw paints the box fill and a full 1 px outline instead. Every other
 * call goes to the saved routine with the arguments as received. */
/* BUTTON-ROLES: collect the window's button gadgets (render == select == frame, as flat_button_gadget) in gadget coordinates and let
 * flat_button_role decide for the one drawn through obj. Gadgets with relative flags keep their raw offsets (unresolved). */
static int role_of(struct RastPort *rp, Object *obj)
{
    flat_box sib[32], self;
    struct Layer *lay = rp->Layer;
    struct Window *win = lay ? (struct Window *)lay->Window : NULL;
    struct Gadget *g;
    int n = 0, found = 0, i;
    for (g = win ? win->FirstGadget : NULL, i = 0; g && i < 64 && n < 32; g = g->NextGadget, i++) {
        if (!g->GadgetRender || g->GadgetRender != g->SelectRender)
            continue;
        {   /* S56-CYCLE-PLAIN: only push buttons take part in the row; a cycle gadget keeps the plain pen_button fill */
            struct IntuiText *it = g->GadgetText;
            if (!flat_push_button(it != NULL, it ? it->LeftEdge : 0, it ? it->TopEdge : 0, g->Width, g->Height, g->Activation)) {
                if (g->GadgetRender == (APTR)obj)
                    return FLAT_ROLE_PLAIN;
                continue;
            }
        }
        sib[n].x = g->LeftEdge; sib[n].y = g->TopEdge; sib[n].w = g->Width; sib[n].h = g->Height;
        if (g->GadgetRender == (APTR)obj) { self = sib[n]; found = 1; }
        n++;
    }
    return found ? flat_button_role(&self, sib, n) : FLAT_ROLE_PRIMARY;
}

static int gadget_has_propinfo(struct Gadget *g);

/* S56-LOCALE-KNOB: 1 when obj is the knob frame of a SMARTKNOBIMAGE prop gadget in rp's window (flat_smart_knob holds the rule). */
static int smart_knob_of(struct RastPort *rp, Object *obj)
{
    struct Layer *lay = rp->Layer;
    struct Window *win = lay ? (struct Window *)lay->Window : NULL;
    struct Gadget *g;
    int i;
    for (g = win ? win->FirstGadget : NULL, i = 0; g && i < 64; g = g->NextGadget, i++) {
        struct Image *im = (struct Image *)g->GadgetRender;
        int prop = gadget_has_propinfo(g) && (g->GadgetType & GTYP_GTYPEMASK) == GTYP_PROPGADGET && (g->Flags & GFLG_GADGIMAGE) && im;
        if (prop && flat_smart_knob(1, ((struct PropInfo *)g->SpecialInfo)->Flags, im->NextImage, obj))
            return 1;
    }
    return 0;
}

/* the window gadget (render == select == obj) of rp's window, NULL when obj is no such gadget image */
static struct Gadget *gadget_of_image(struct RastPort *rp, Object *obj)
{
    struct Layer *lay = rp->Layer;
    struct Window *win = lay ? (struct Window *)lay->Window : NULL;
    struct Gadget *g;
    int n;
    for (g = win ? win->FirstGadget : NULL, n = 0; g && n < 64; g = g->NextGadget, n++)
        if (flat_button_gadget(g->GadgetRender, g->SelectRender, obj))
            return g;
    return NULL;
}

static int button_gadget_of(struct RastPort *rp, Object *obj)
{
    return gadget_of_image(rp, obj) != NULL;
}

/* 1 when obj is the image of a push button gadget (flat_push_button), as role_of tells a push button from a cycle */
static int push_gadget_of(struct RastPort *rp, Object *obj)
{
    struct Gadget *g = gadget_of_image(rp, obj);
    struct IntuiText *it = g ? g->GadgetText : NULL;
    return g && flat_push_button(it != NULL, it ? it->LeftEdge : 0, it ? it->TopEdge : 0, g->Width, g->Height, g->Activation);
}

/* The class-chain walk shared by the prop gadget test and the string input test: does boopsi gadget g's class or a superclass carry
 * cl_ID == id (depth bounded, NULL ids skipped)? Plain memory reads, no Intuition call, no allocation: safe inside a graphics hook. */
static int gadget_class_has(struct Gadget *g, const char *id, int cap)
{
    struct IClass *cl = OCLASS(g);
    int depth;
    for (depth = 0; cl && depth < cap; depth++, cl = cl->cl_Super) {
        const char *a = (const char *)cl->cl_ID, *b = id;
        if (a) {
            while (*a && *a == *b) { a++; b++; }
            if (*a == *b)
                return 1;
        }
    }
    return 0;
}

/* OWNER-2026-11-10 INPUT-GROUND: the string gadget of rp's window whose box the frame box x0,y0..x1,y1 borders (flat_string_frame),
 * NULL when there is none. Relative gadget edges are resolved against the window. */
static struct Gadget *string_gadget_at(struct RastPort *rp, int x0, int y0, int x1, int y1)
{
    struct Layer *lay = rp ? rp->Layer : NULL;
    struct Window *win = lay ? (struct Window *)lay->Window : NULL;
    struct Gadget *g;
    int n;
    for (g = win ? win->FirstGadget : NULL, n = 0; g && n < 64; g = g->NextGadget, n++) {
        struct StringInfo *si = (struct StringInfo *)g->SpecialInfo;
        int gx, gy, gw, gh;
        if (!flat_string_ext(g->GadgetType & GTYP_GTYPEMASK, g->Flags, g->Activation, si != NULL, si && si->Extension != NULL,
                             (g->GadgetType & GTYP_GTYPEMASK) == GTYP_CUSTOMGADGET && gadget_class_has(g, "strgclass", 8)))
            continue;
        gx = g->LeftEdge + ((g->Flags & GFLG_RELRIGHT) ? win->Width - 1 : 0);
        gy = g->TopEdge + ((g->Flags & GFLG_RELBOTTOM) ? win->Height - 1 : 0);
        gw = g->Width + ((g->Flags & GFLG_RELWIDTH) ? win->Width : 0);
        gh = g->Height + ((g->Flags & GFLG_RELHEIGHT) ? win->Height : 0);
        if (flat_string_frame(x0, y0, x1 - x0 + 1, y1 - y0 + 1, gx, gy, gw, gh))
            return g;
    }
    return NULL;
}

/* OWNER-2026-11-10 INPUT-GROUND: when the box x0,y0..x1,y1 is a string input's border, give the gadget the selection pen as its ground
 * (strgclass repaints its interior from StringExtend Pens[1] / ActivePens[1], so a fill here would be overpainted) and return that
 * pen; -1 when the box is no string input or the screen holds no such pen. Only two pen NUMBERS are written, no pointer: after the
 * patcher unloads the gadget still draws with a valid pen. */
static int string_ground(flatshared *r, struct RastPort *rp, int x0, int y0, int x1, int y1)
{
    struct Gadget *g = string_gadget_at(rp, x0, y0, x1, y1);
    struct Window *w = g ? (struct Window *)rp->Layer->Window : NULL;
    int pen = w ? flat_input_pen(st_scr(r, w->WScreen), rp->BitMap ? rp->BitMap->Depth : 0) : -1;
    if (pen >= 0) {
        struct StringExtend *ex = ((struct StringInfo *)g->SpecialInfo)->Extension;
        ex->Pens[1] = ex->ActivePens[1] = (UBYTE)pen;
        r->sground++;
    }
    return pen;
}

/* INPUT-GROUND: the input's whole box x0,y0..x1,y1 in its ground pen with the rounded corners of a flat button, no outline
 * (raa_ground_frame_draw: the same rounded fill and corner blend as the knobs and buttons). */
static void ground_box(flatshared *r, struct RastPort *rp, int x0, int y0, int x1, int y1, int pen)
{
    int blended;
    UBYTE fg = rp->FgPen, dm = rp->DrawMode;
    raa_ground_frame_draw(st_rp(r, rp), pen, x0, y0, x1 - x0 + 1, y1 - y0 + 1, c2_fill, c2_read, c2_rgb, raa_load(rp), rp, &blended);
    SetAPen(rp, fg);
    SetDrMd(rp, dm);
}

/* INSTALLER-BORDER-BUTTON (owner 2026-11-08): the Border-render push button of rp's window (flat_border_button) whose box is
 * exactly x0,y0..x1,y1, NULL when none. The Installer draws such a button as a pen-0 RectFill of the box, its label, then a
 * temporary frameiclass IM_DRAWFRAME over the same box; render != select, so the frame object never names the gadget and the
 * rectangle is the only link. GZZ windows are skipped (gadget and rastport coordinates differ there). *role gets the row role
 * among the window's border buttons (flat_button_role). */
static struct Task *bb_task;     /* the task inside the border-button fill: its own RectFills (c2_fill) go stock */

/* flat_border_button's box of g into *b, 1 when g is a Border-render push button */
static int border_box(struct Gadget *g, flat_box *b)
{
    if (!flat_border_button(g->GadgetType, g->Flags, g->GadgetRender, g->SelectRender, g->GadgetText != NULL, g->Width, g->Height))
        return 0;
    b->x = g->LeftEdge; b->y = g->TopEdge; b->w = g->Width; b->h = g->Height;
    return 1;
}

/* S62-FLAT-RF-TX-BUDGET: no sibling array on the drawing task's stack. Pass 1 finds the exact box among the first 32 border buttons
 * of the first 64 gadgets (the caps of the old array walk); pass 2 folds flat_button_role_add over the same buttons. */
static int border_role(struct Window *win, const flat_box *self)
{
    flat_box o;
    struct Gadget *g;
    int n = 0, i, seen = 0;
    for (g = win->FirstGadget, i = 0; g && i < 64 && n < 32; g = g->NextGadget, i++)
        if (border_box(g, &o)) {
            seen = flat_button_role_add(seen, self, &o);
            n++;
        }
    return flat_button_role_of(seen);
}

static struct Gadget *border_button_at(struct RastPort *rp, int x0, int y0, int x1, int y1, int *role)
{
    flat_box b;
    struct Layer *lay = rp ? rp->Layer : NULL;
    struct Window *win = lay ? (struct Window *)lay->Window : NULL;
    struct Gadget *g;
    int n = 0, i;
    if (!win || (win->Flags & WFLG_GIMMEZEROZERO))
        return NULL;
    for (g = win->FirstGadget, i = 0; g && i < 64 && n < 32; g = g->NextGadget, i++) {
        if (!border_box(g, &b))
            continue;
        if (flat_box_exact(&b, x0, y0, x1, y1)) {
            if (role)
                *role = border_role(win, &b);
            return g;
        }
        n++;
    }
    return NULL;
}

/* DOPUS-DIALOG-BUTTON-FILL (owner s70/s71): a DOpus push button draws its border through the bevel (d4_bevelbody), then fills its
 * interior (a RectFill of the box inset by one pixel), then its label. The bevel of a push button draws no outline here: it records
 * the box in the pending table dpt (flat_dpend_*, one entry per task, never refuses a put), and flat_rf_c turns the same task's next
 * matching interior fill into the rounded flat fill of the FULL box (bb_draw: the Installer border-button path). An entry is released
 * by its matching fill, by the same task's next bevel call that is not a push button (d4_flat_pens), by the same task's next push
 * button bevel (it reuses the entry) or, when the task never fills, by the oldest-first replacement; a stale entry of one task never
 * blocks another. Tables are touched under Forbid. Gadget under the box: custom, RELVERIFY, no TOGGLESELECT (flat_dopus_push_gadget). */
static flat_dpend_tab dpt;
static flat_dpend dpt_hit;          /* the entry the fill took, held by the bb_task claim (no struct on the deep stack path) */

/* 1 when the gadget of rp's window with exactly this box is a DOpus push button; *role gets its row role among the window's push buttons */
static int dopus_push_at(struct RastPort *rp, int x0, int y0, int x1, int y1, int *role)
{
    flat_box self, o;
    struct Layer *lay = rp ? rp->Layer : NULL;
    struct Window *win = lay ? (struct Window *)lay->Window : NULL;
    struct Gadget *g;
    int i, n = 0, found = 0, seen = 0;
    if (!win || (win->Flags & WFLG_GIMMEZEROZERO))
        return 0;
    self.x = x0; self.y = y0; self.w = x1 - x0 + 1; self.h = y1 - y0 + 1;
    for (g = win->FirstGadget, i = 0; g && i < 64; g = g->NextGadget, i++) {
        if (!flat_dopus_push_gadget(g->GadgetType, g->Activation) || g->Width < 3 || g->Height < 3)
            continue;
        o.x = g->LeftEdge; o.y = g->TopEdge; o.w = g->Width; o.h = g->Height;
        if (flat_box_exact(&o, x0, y0, x1, y1))
            found = 1;
        else if (n < 32) {
            seen = flat_button_role_add(seen, &self, &o);
            n++;
        }
    }
    *role = found ? flat_button_role_of(seen) : FLAT_ROLE_PRIMARY;
    return found;
}

/* The bevel of a DOpus push button: record it pending (1) or leave the outline path (0). Off the bevel hook's own frame. */
static int dopus_push_claim(struct RastPort *rp, int x0, int y0, int x1, int y1, LONG recessed)
{
    int role;
    const void *surf, *me;
    if (!rp->Layer || !dopus_push_at(rp, x0, y0, x1, y1, &role))
        return 0;
    surf = flat_tx_surface(rp->Layer, rp->BitMap);
    me = FindTask(NULL);
    Forbid();
    flat_dpend_put(&dpt, me, surf, x0, y0, x1 - x0 + 1, y1 - y0 + 1, (role << 8) | (recessed ? 1 : 0));
    Permit();
    return 1;
}

/* PREFS-NO-OUTLINES: the frame box ox,oy,w,h of an edges-only frame, classified by the window's gadget list (flat_fc_add / flat_fc_class,
 * pure, host tested), drawn without a coloured outline. TRACK, CELL and DISPLAY become a rounded ground box and are recorded so the
 * pen-0 content the app draws after the frame lands on it (flat_rf_c, flat_tx_c); GROUP draws nothing. 1 = handled, 0 = stock draw
 * (a frame bordering another gadget type, or no pen on this screen). Any frame draw first drops the records it overlaps. */
static int noline_frame(flatshared *r, struct RastPort *rp, int ox, int oy, int w, int h)
{
    struct Layer *lay = rp->Layer;
    struct Window *win = lay ? (struct Window *)lay->Window : NULL;
    const void *surf = flat_tx_surface(lay, rp->BitMap);
    struct Gadget *g;
    flat_fc fc;
    int n, cls, pen, depth = rp->BitMap ? rp->BitMap->Depth : 0, centre = -1;
    flat_gr_drop(&r->gr, surf, ox, oy, ox + w - 1, oy + h - 1);
    if (!win)
        return 0;
    memset(&fc, 0, sizeof fc);
    for (g = win->FirstGadget, n = 0; g && n < 64; g = g->NextGadget, n++) {
        struct StringInfo *si = (struct StringInfo *)g->SpecialInfo;
        flat_gadbox gb;
        int gt = g->GadgetType & GTYP_GTYPEMASK;
        gb.x = g->LeftEdge + ((g->Flags & GFLG_RELRIGHT) ? win->Width - 1 : 0);
        gb.y = g->TopEdge + ((g->Flags & GFLG_RELBOTTOM) ? win->Height - 1 : 0);
        gb.w = g->Width + ((g->Flags & GFLG_RELWIDTH) ? win->Width : 0);
        gb.h = g->Height + ((g->Flags & GFLG_RELHEIGHT) ? win->Height : 0);
        if (flat_string_ext(gt, g->Flags, g->Activation, si != NULL, si && si->Extension != NULL,
                            gt == GTYP_CUSTOMGADGET && gadget_class_has(g, "strgclass", 8)))
            gb.kind = FLAT_GK_STRING;
        else if (gt == GTYP_PROPGADGET && gadget_has_propinfo(g))
            gb.kind = FLAT_GK_PROP;
        else
            gb.kind = FLAT_GK_OTHER;
        flat_fc_add(&fc, ox, oy, w, h, &gb);
    }
    cls = flat_fc_class(&fc);
    if (cls == FLAT_FC_GROUP) {
        r->gbhits++;
        return 1;
    }
    if (cls != FLAT_FC_TRACK && cls != FLAT_FC_CELL && cls != FLAT_FC_DISPLAY)
        return 0;
    if (cls == FLAT_FC_CELL)
        centre = (int)ReadPixel(rp, ox + w / 2, oy + h / 2);
    pen = flat_noline_pen(st_scr(r, win->WScreen), cls, depth, centre);
    if (pen < 0)
        return 0;
    ground_box(r, rp, ox, oy, ox + w - 1, oy + h - 1, pen);
    flat_gr_add(&r->gr, surf, ox, oy, ox + w - 1, oy + h - 1, pen);
    r->gbhits++;
    return 1;
}

/* The frame draw of c8_skip (1 = drawn, 0 = leave it to the ROM frame). Runs on the private stack (S71-C8-PRIVATE-STACK): the
 * flat_state copy, the outline columns and the raa_* draw calls are far over the 384 byte budget of a hook on a foreign stack. */
static int c8_draw(Object *obj, Msg msg)
{
    flatshared *r = g_r;
    struct impDraw *d = (struct impDraw *)msg;
    {
        struct Image *im = (struct Image *)obj;
        ULONG eo = 1;
        /* Fill only when the frame says it is not edges-only (an unanswered OM_GET counts as edges-only: never erase content). */
        int fill;
        if (GetAttr(IA_EdgesOnly, obj, &eo) != 0)
            fill = flat_frame_fill(1, (int)eo, push_gadget_of(d->imp_RPort, obj));
        else      /* 3.1 frameiclass V40 does not answer OM_GET: a button or cycle gadget renders this image as render and select */
            fill = flat_frame_fill(0, 0, button_gadget_of(d->imp_RPort, obj));
        flat_state st = r->st;
        {   /* S57-BUTTON-ASPECT-BORDERS: the outline columns of this window's screen (slot table read, no Intuition call) */
            struct Layer *sl = d->imp_RPort->Layer;
            struct Window *sw = sl ? (struct Window *)sl->Window : NULL;
            const flat_state *ps = st_scr(r, sw ? sw->WScreen : NULL);
            st.side = (uint8_t)(sw ? side_px(r, sw->WScreen) : 1);
            st.pen_dim = ps->pen_dim; st.pen_knob = ps->pen_knob; st.pen_inbar = ps->pen_inbar; st.pen_sel = ps->pen_sel;
        }
        int ox = im->LeftEdge + d->imp_Offset.X, oy = im->TopEdge + d->imp_Offset.Y;
        int w, h;
        flat_frame_size(msg->MethodID == FLAT_IM_DRAWFRAME, d->imp_Dimensions.Width, d->imp_Dimensions.Height, im->Width, im->Height, &w, &h);
        if (border_button_at(d->imp_RPort, ox, oy, ox + w - 1, oy + h - 1, NULL)) {
            /* INSTALLER-BORDER-BUTTON: the rounded fill was painted at the erase (flat_rf_c) and the label is drawn; no ring */
            flat_tx_frame(&r->tx, flat_tx_surface(d->imp_RPort->Layer, d->imp_RPort->BitMap), FindTask(NULL), 0, 0, ox, oy, ox + w - 1, oy + h - 1);
            r->fflat++;
            return 1;
        }
        {
            int gp = string_ground(r, d->imp_RPort, ox, oy, ox + w - 1, oy + h - 1);
            if (gp >= 0) {               /* INPUT-GROUND: a string input has no outline: its whole box is the rounded ground */
                ground_box(r, d->imp_RPort, ox, oy, ox + w - 1, oy + h - 1, gp);
                return 1;
            }
        }
        if (smart_knob_of(d->imp_RPort, obj)) {      /* S56-LOCALE-KNOB: the listview scroller knob takes the Workbench knob pen */
            struct Window *kw = (struct Window *)d->imp_RPort->Layer->Window;
            if (!raa_knob_frame_draw(st_scr(r, kw->WScreen), d->imp_RPort->BitMap ? d->imp_RPort->BitMap->Depth : 0, (kw->Flags & WFLG_WINDOWACTIVE) != 0,
                                     ox, oy, w, h, c2_fill, c2_read, c2_rgb, raa_load(d->imp_RPort), d->imp_RPort, NULL))
                return 0;
            r->fflat++;
            return 1;
        }
        if (!fill && d->imp_State == 0 && noline_frame(r, d->imp_RPort, ox, oy, w, h))
            return 1;
        if (!raa_frame_draw(&st, (int)d->imp_State, ox, oy, w, h, fill, role_of(d->imp_RPort, obj), c2_fill, c2_read, c2_rgb,
                            fill ? raa_load(d->imp_RPort) : 0, d->imp_RPort, NULL))
            return 0;
        flat_gh_frame(&r->gh, flat_tx_surface(d->imp_RPort->Layer, d->imp_RPort->BitMap), FindTask(NULL), d->imp_State == 1, fill,
                      ox, oy, ox + w - 1, oy + h - 1);
        /* a pressed filled frame arms the Text patch for its label; any other frame draw on this rastport disarms */
        flat_tx_frame(&r->tx, flat_tx_surface(d->imp_RPort->Layer, d->imp_RPort->BitMap), FindTask(NULL),
                      flat_tx_arm(d->imp_State == 1, fill, role_of(d->imp_RPort, obj)), fill, ox, oy, ox + w - 1, oy + h - 1);
    }
    r->fflat++;
    return 1;
}

typedef struct { Object *obj; Msg msg; int rc; } c8_call;
static LONG c8_on_private(void *a);
/* The way into c8_on_private: always an indirect call (flat_on_stack's `jsr (a2)`, and the re-entry call below). The flat budget
 * follows direct calls only and skips calls through a register, so the draw's frames are charged to the private stack, where
 * they run, and not to the caller's 384 bytes. Not const, so the compiler cannot fold it back into a direct call. */
static LONG (*c8_run)(void *) = c8_on_private;

/* The function that runs on the private stack (flat_on_stack). */
static LONG c8_on_private(void *a)
{
    c8_call *c = (c8_call *)a;
    c->rc = c8_draw(c->obj, c->msg);
    return 0;
}

/* c8_draw on the private stack of the clone (cl_stk, cl_sss), shared with c2_clone, never on the caller's.
 * Re-entry: a frame drawn from inside a clone draw (the clone's NewObjectA/render path) already runs on cl_stk, and cl_ss is held by
 * this very task: the stack pointer is inside cl_stk, so c8_run calls the draw directly (no second StackSwap on cl_sss, no semaphore).
 * LOCK ORDER: cl_ss is only ever taken here with AttemptSemaphore, never waited for (as in c2_clone and clone_render). A hook that
 * never waits is no edge in a wait-for graph, so no cycle with a layer lock or any other task's lock can close on it (s71 freeze:
 * a lock cycle in a draw hook); the only waiter is main's ObtainSemaphore(&cl_ss) at shutdown, after the hooks are removed, and
 * nothing holding cl_ss waits for main. LockIBase is never taken. Busy (another task is drawing a clone or a frame) or no stack: 0,
 * the ROM draws its own frame, as c2_clone falls back to the block glyph. */
static int c8_draw_on(Object *obj, Msg msg)
{
    c8_call c;
    c.obj = obj; c.msg = msg; c.rc = 0;
    if (cl_stk && (uint8_t *)&c >= cl_stk && (uint8_t *)&c < cl_stk + CLONE_STACK_BYTES)
    {   /* already on cl_stk (cl_ss held by this task): no second swap */
        c8_run(&c);
        return c.rc;
    }
    if (!cl_stk) {              /* counters: plain ULONG increments like fcalls/fflat, in the shared block g_r */
        g_r->fnostk++;
        return 0;
    }
    if (!AttemptSemaphore(&cl_ss)) {
        g_r->fbusy++;
        return 0;
    }
    flat_on_stack(c8_run, &c, &cl_sss);
    ReleaseSemaphore(&cl_ss);
    return c.rc;
}

static int c8_skip(Object *obj, Msg msg)
{
    flatshared *r = g_r;
    struct impDraw *d = (struct impDraw *)msg;
    r->fcalls++;
    if (msg->MethodID != FLAT_IM_DRAW && msg->MethodID != FLAT_IM_DRAWFRAME)
        return 0;
    if (!d->imp_RPort || !d->imp_DrInfo)
        return 0;
    if (flat_frame_decide(&r->st, msg->MethodID, d->imp_DrInfo->dri_Pens[SHINEPEN], d->imp_DrInfo->dri_Pens[BACKGROUNDPEN]) != FLAT_DRAW_SKIP)
        return 0;
    return c8_draw_on(obj, msg);
}

ULONG c8_disp_reg(__reg("a0") struct Hook *h, __reg("a2") Object *obj, __reg("a1") Msg msg)
{
    if (c8_skip(obj, msg))
        return TRUE;
    return ((oldfn_reg)g_r->fsw.saved)(h, obj, msg);
}

ULONG c8_disp_stk(void *caller_return, struct Hook *h, Object *obj, Msg msg)
{
    if (c8_skip(obj, msg))
        return TRUE;
    return ((oldfn_stk)g_r->fsw.saved)(h, obj, msg);
}

/* INPUT-GROUND: strgclass GM_RENDER. Before the stock render the gadget's StringExtend ground pens become the input pen (two pen
 * numbers, no pointer: nothing dangles after unload). No allocation, no lock, no print. */
static void g9_ground(Object *obj, Msg msg)
{
    flatshared *r = g_r;
    struct gpRender *gp = (struct gpRender *)msg;
    struct Gadget *g = (struct Gadget *)obj;
    struct StringInfo *si;
    struct GadgetInfo *gi;
    int pen;
    if (msg->MethodID != FLAT_GM_RENDER || !obj || !gp->gpr_GInfo)
        return;
    gi = gp->gpr_GInfo;
    si = (struct StringInfo *)g->SpecialInfo;
    pen = flat_input_pen(st_scr(r, gi->gi_Screen), gi->gi_DrInfo ? gi->gi_DrInfo->dri_Depth : 0);
    if (flat_string_render_write(msg->MethodID, si != NULL, g->Flags, g->Activation, si && si->Extension != NULL, pen)) {
        int tpen = gi->gi_DrInfo ? flat_input_text_pen(gi->gi_DrInfo->dri_Pens[TEXTPEN], gi->gi_DrInfo->dri_Depth) : -1;
        si->Extension->Pens[1] = si->Extension->ActivePens[1] = (UBYTE)pen;
        if (tpen >= 0)          /* INPUT-TEXT: readable text on the ground, whatever pen the app chose (DOpus path: BLOCKPEN) */
            si->Extension->Pens[0] = si->Extension->ActivePens[0] = (UBYTE)tpen;
        r->sground++;
    }
}

ULONG g9_disp_reg(__reg("a0") struct Hook *h, __reg("a2") Object *obj, __reg("a1") Msg msg)
{
    g9_ground(obj, msg);
    return ((oldfn_reg)g_r->gsw.saved)(h, obj, msg);
}

/* ---- FLAT-D4: the replacement routines. They run on the caller's task with the caller's registers (a0 rp, a1 rect,
 * a2 info, d0 recessed). Flat on and flat pens: the flat outline. Otherwise the saved routine, same arguments. */
typedef void (*boxfn)(__reg("a0") struct RastPort *, __reg("a1") struct Rectangle *, __reg("a2") struct DrawInfo *,
                      __reg("d0") LONG, __reg("a6") struct Library *);
typedef void (*fieldfn)(__reg("a0") struct RastPort *, __reg("a1") struct Rectangle *, __reg("a2") struct DrawInfo *,
                        __reg("a6") struct Library *);

static uint32_t icl_rgb(struct ColorMap *cm, ULONG pen);

/* B10 (a): the colour of a pen on the screen behind rp, 0xRRGGBB, or -1 when it cannot be known (no layer or window:
 * an offscreen buffer; a pen past the colour map). */
static long d4_rgb(struct RastPort *rp, ULONG pen)
{
    struct Window *w = rp && rp->Layer ? (struct Window *)rp->Layer->Window : NULL;
    struct ColorMap *cm = w && w->WScreen ? w->WScreen->ViewPort.ColorMap : NULL;
    if (!cm || pen >= (ULONG)cm->Count)
        return -1;
    return (long)icl_rgb(cm, pen);
}

/* 0c DOPUS-ENV-UNSTYLED: under flat pens the stock bevel is replaced by the flat outline (flat_dopus_box_draw), with this
 * window's screen pens and outline columns (S57-BUTTON-ASPECT-BORDERS, as c8_skip); drawing nothing left every DOpus
 * button and field without a border (owner, DOpus Environment, rig 63). The owner reversed the border for fields on
 * 2026-11-10 (s71): FieldBox gets no ring either (flat_dopus_ring 0), like a raised DrawBox. */
static int d4_flat_pens(int el, struct RastPort *rp, ULONG shine, ULONG back, int x0, int y0, int x1, int y1, LONG recessed)
{
    flatshared *r = g_r;
    r->dcalls++;
    if (el == FLAT_EL_BEVEL)
        r->dbevel++;
    if (flat_dopus_decide(&r->st, el, recessed ? 1 : 0, (int)shine, (int)back,
                          shine == back ? -1 : d4_rgb(rp, shine), shine == back ? -1 : d4_rgb(rp, back)) != FLAT_DRAW_SKIP)
        return 0;
    if (el == FLAT_EL_BEVEL && rp) {
        if (dopus_push_claim(rp, x0, y0, x1, y1, recessed)) {
            r->dflat++;                 /* DOPUS-DIALOG-BUTTON-FILL: no outline, the interior fill draws the rounded button (flat_rf_c) */
            return 1;
        }
        if (dpt.n) {                    /* a bevel of this task that is no push button: its pending record is stale */
            const void *me = FindTask(NULL);
            Forbid();
            flat_dpend_drop(&dpt, me);
            Permit();
        }
    }
    if (rp) {
        struct Window *w = rp->Layer ? (struct Window *)rp->Layer->Window : NULL;
        flat_state st = *st_rp(r, rp);
        st.side = (uint8_t)(w ? side_px(r, w->WScreen) : 1);
        if (!flat_dopus_ring(el, recessed ? 1 : 0)) {
            /* DOPUS-BUTTON-NO-RING: the 1 px (side columns) border takes the pen of the box interior, so the button is one
             * flat block (SRCE / DEST) or bare content (OFF); the interior itself is DOpus's and is never touched here */
            int gp = el == FLAT_EL_FIELDBOX ? string_ground(r, rp, x0, y0, x1, y1) : -1;
            if (gp >= 0) {              /* INPUT-GROUND: no ring, no inner frame: the whole field box is the rounded ground */
                ground_box(r, rp, x0, y0, x1, y1, gp);
                r->dface++;
                r->dflat++;
                return 1;
            }
            LONG fp = gp >= 0 ? gp : ReadPixel(rp, x0 + st.side, y0 + 1);    /* INPUT-GROUND: the field's own ground, not the old pixel */

            if (fp < 0)
                r->dnopix++;
            st.pen_line = (uint8_t)flat_dopus_face_pen(fp, (int)back);
            r->dface++;
        }
        flat_dopus_box_draw(&st, x0, y0, x1, y1, c2_fill, rp);
    }
    r->dflat++;
    return 1;
}

static int d4_flat(int el, struct RastPort *rp, struct Rectangle *rect, struct DrawInfo *info, LONG recessed)
{
    if (!rect) {
        g_r->dcalls++;
        return 0;
    }
    return d4_flat_pens(el, rp, info->dri_Pens[SHINEPEN], info->dri_Pens[BACKGROUNDPEN], rect->MinX, rect->MinY,
                        rect->MaxX, rect->MaxY, recessed);
}

static void d4_drawbox(__reg("a0") struct RastPort *rp, __reg("a1") struct Rectangle *rect, __reg("a2") struct DrawInfo *info,
                       __reg("d0") LONG recessed)
{
    /* L_DrawBox declares BOOL recessed (a WORD): only the low word of d0 is defined (DOpus passes 0x22000000 for a raised
     * info-field frame). Our own decision reads the BOOL; the stock routine gets d0 untouched. */
    if (!d4_flat(FLAT_EL_DRAWBOX, rp, rect, info, flat_dopus_recessed(recessed)))
        ((boxfn)g_r->dsw[FLAT_EL_DRAWBOX].saved)(rp, rect, info, recessed, g_r->dlib);
}

static void d4_fieldbox(__reg("a0") struct RastPort *rp, __reg("a1") struct Rectangle *rect, __reg("a2") struct DrawInfo *info)
{
    if (!d4_flat(FLAT_EL_FIELDBOX, rp, rect, info, 0))
        ((fieldfn)g_r->dsw[FLAT_EL_FIELDBOX].saved)(rp, rect, info, g_r->dlib);
}

/* FLAT-UNLOAD-INFLIGHT: the body hooks are entered through the class gate template (flat_dg.s) by a plain jsr, so the stack
 * holds [gate ret][caller ret][args]: the two stack-convention handlers (d4_fieldbody, d4_bevelbody) take one leading
 * caller_return parameter that absorbs the caller's return address, as c2_disp_stk does. The register-convention ones
 * (d4_drawbox, d4_fieldbox) need nothing.
 * 0c: the FieldBox body, entered by JMP from its patched entry. Stack convention (rp, rect, info, GfxBase at 4/8/12/16(sp)),
 * the caller pops them; the saved path is the trampoline called with the same four arguments. */
typedef void (*fieldbodyfn)(struct RastPort *, struct Rectangle *, struct DrawInfo *, struct Library *);

static void d4_fieldbody(void *caller_return, struct RastPort *rp, struct Rectangle *rect, struct DrawInfo *info, struct Library *gfx)
{
    if (!d4_flat(FLAT_EL_FIELDBOX, rp, rect, info, 0))
        ((fieldbodyfn)g_r->dsw[FLAT_EL_FIELDBOX].saved)(rp, rect, info, gfx);
}

/* s66 0c buttons: dopus5's internal static bevel (5.104 0x166d8), entered by JMP from its patched entry. Stack convention
 * (unused, rp, pens, rect, recessed, thin at 4..24(sp)), the caller pops them. pens is the DrawInfo pen array (the stock
 * code reads pens[SHINEPEN] / pens[SHADOWPEN]); rect is four WORDs x, y, width, height (the stock code draws its top
 * line at y from x to x + width - 2 and its left line down to y + height - 1). dopusbuttongclass (Use / Save / Cancel)
 * and dopusiclass frame images draw here, never through DrawBox, so without this they kept pen 0 edges. */
typedef void (*bevelbodyfn)(ULONG, struct RastPort *, UWORD *, WORD *, ULONG, ULONG);

static void d4_bevelbody(void *caller_return, ULONG a, struct RastPort *rp, UWORD *pens, WORD *rect, ULONG recessed, ULONG thin)
{
    if (!pens || !rect || !d4_flat_pens(FLAT_EL_BEVEL, rp, pens[SHINEPEN], pens[BACKGROUNDPEN], rect[0], rect[1],
                                         rect[0] + rect[2] - 1, rect[1] + rect[3] - 1, (LONG)(recessed == 1)))
        ((bevelbodyfn)g_r->dsw[FLAT_EL_BEVEL].saved)(a, rp, pens, rect, recessed, thin);
}

static const LONG d4_lvo[FLAT_EL_COUNT] = { FLAT_LVO_DRAWBOX, FLAT_LVO_FIELDBOX, 0 };   /* the bevel has no LVO */

static int d4_installed(const flatshared *r)
{
    int el;
    for (el = 0; el < FLAT_EL_COUNT; el++)
        if (r->dsw[el].installed)
            return 1;
    return r->d5sw.installed;
}

/* ---- FLAT-BUTTON-FILL: graphics.library Text (LVO -60, Text(rp a1, string a0, count d0)). The frame is drawn first, then the
 * label in pen 1 with JAM1, also when pressed, so the dark pressed label needs this patch: while c8_skip has armed
 * {surface (layer, else bitmap: the label comes through another RastPort than the frame), task, box} for a pressed filled frame, a Text call on that rastport from that task with the pen position
 * inside the box runs with pen_ptext (flat_tx_text decides; the stub is flat_tx.s). Everything else: stock Text. ---- */
/* Every saved library routine (Text, RectFill, the dopus5 ones) is called with its library base in a6, as through its
 * LVO: the ROM code reads its base from a6, and these hooks use a6 as a scratch register and for FindTask (a6 = SysBase).
 * Without it, the stock Text ran with a6 = SysBase and jumped through a garbage base (s28 flat-ON hang). */
typedef void (*textfn)(__reg("a1") struct RastPort *, __reg("a0") CONST_STRPTR, __reg("d0") ULONG, __reg("a6") struct Library *);
/* FLAT-UNLOAD-INFLIGHT: the entry code of the Text and RectFill patches lives in a small AllocMem(MEMF_PUBLIC) block that is
 * NEVER freed, with the in-flight counter at its start. The reason: a caller that is inside the patch (a draw blocked on a
 * layer lock, say) comes back through the entry's count-out and final rts AFTER the handler returns, and by then unload may
 * have freed this program's segment; the old stub lived in that segment and the caller returned into freed code. The block
 * is a copy of the position-independent template in flat_rf.s / flat_tx.s (the jsr to the C handler is relocated by the
 * loader before the copy). Unload sets the block's closed flag FIRST (a late arrival counts in, sees it and runs the original
 * function: nothing of ours), restores the vector, then waits until the counter reads 0 (gate_wait); it never gives up. The
 * entry order is count in, then the closed test, so a caller that fetched the vector but has not yet counted in cannot slip
 * past the wait. The same gate (flat_dg.s) fronts the class dispatchers. Cost: about 70 bytes of public memory per load, deliberately left behind. */
typedef struct flat_gate {
    volatile LONG count;                /* callers inside the entry: addq.l #1 on entry, subq.l #1 before the final rts (single instructions, atomic on 68k) */
    volatile LONG closed;               /* unload sets it BEFORE it restores the vector: a caller that enters later counts in, sees it, and runs the original */
    ULONG orig;                         /* the function the vector held before our swap (flat_swap.saved): where a closed gate passes the call on */
    unsigned char code[1];              /* the copied template follows */
} flat_gate;

/* The template operands that gate_make patches in the copy (see the note at the end of flat_dg.s). */
#define GATE_M_COUNT   0x11111111UL
#define GATE_M_CLOSED  0x22222222UL
#define GATE_M_ORIG    0x33333333UL
#define GATE_M_HANDLER 0x44444444UL

/* Replace every even-offset occurrence of the magic word m in [code, code+len) by v; returns how many were replaced. */
static int gate_patch(unsigned char *code, ULONG len, ULONG m, ULONG v)
{
    ULONG o;
    int n = 0;
    for (o = 0; o + 4 <= len; o += 2) {
        if (code[o] == (unsigned char)(m >> 24) && code[o + 1] == (unsigned char)(m >> 16) && code[o + 2] == (unsigned char)(m >> 8) && code[o + 3] == (unsigned char)m) {
            code[o] = (unsigned char)(v >> 24); code[o + 1] = (unsigned char)(v >> 16); code[o + 2] = (unsigned char)(v >> 8); code[o + 3] = (unsigned char)v;
            n++;
        }
    }
    return n;
}

/* Copy the template [tpl, end) into a fresh block and point its operands at the block's own counter, closed flag and orig
 * word (and at handler, when the template calls one), flush the caches. NULL when out of memory or the template lacks an
 * operand. The gate starts open; gate_open sets orig after the swap. */
static flat_gate *gate_make(const void *tpl, const void *end, ULONG handler)
{
    ULONG len = (ULONG)((const unsigned char *)end - (const unsigned char *)tpl);
    flat_gate *g = (flat_gate *)AllocMem(sizeof(flat_gate) + len, MEMF_PUBLIC | MEMF_CLEAR);
    int ok;
    if (!g)
        return 0;
    CopyMem((APTR)tpl, g->code, len);
    ok = gate_patch(g->code, len, GATE_M_COUNT, (ULONG)&g->count) >= 2
      && gate_patch(g->code, len, GATE_M_CLOSED, (ULONG)&g->closed) == 1
      && gate_patch(g->code, len, GATE_M_ORIG, (ULONG)&g->orig) == 1
      && (!handler || gate_patch(g->code, len, GATE_M_HANDLER, handler) == 1);
    if (!ok) {
        FreeMem(g, sizeof(flat_gate) + len);    /* never entered: not yet reachable by anyone */
        return 0;
    }
    CacheClearU();                      /* the block is code just written */
    return g;
}

/* After the swap installed us: the original is what the swap saved, and the gate runs our handler. */
static void gate_open(flat_gate *g, uintptr_t orig)
{
    if (g) {
        g->orig = (ULONG)orig;
        g->closed = 0;
    }
}

/* Unload step 1, BEFORE the vector is restored: from now on every caller that reaches the gate runs the original. */
static void gate_close(flat_gate *g)
{
    if (g)
        g->closed = 1;
}

/* The restore was refused (a foreign hook sits on top): the gate keeps serving as it did. */
static void gate_reopen(flat_gate *g)
{
    if (g)
        g->closed = 0;
}

/* Unload step 3, after the vector is restored: wait for every caller that is inside the entry. No timeout, nothing is freed before it is 0. */
static void gate_wait(flat_gate *g)
{
    while (g && g->count > 0)
        Delay(1);
}

extern void flat_tx_tpl(void), flat_tx_tpl_end(void);
extern void flat_dg_tpl(void), flat_dg_tpl_end(void);   /* the dispatcher gate (flat_dg.s) */

/* ---- RL21 SELECTA-AA-ICONTEXT: antialiased Workbench and DOpus icon labels, inside the Text patch (no second patch).
 * A label call (icl_accept: ui-antialias on, JAM1/JAM2, box inside the window) reads its box
 * before (JAM1: the backdrop) and after the stock Text, builds the glyph mask and corner coverage (iclabel_core.c), and
 * writes the edge pixels back blended: ce_mix on a true-colour screen (cybergraphics ReadPixelArray/WritePixelArray),
 * a cached ObtainBestPen shade on a pen screen (ReadPixelArray8/WritePixelArray8). The buffers are allocated once
 * (icl_alloc, before tx_install) and used under icl_ss; a busy semaphore or a failed step draws stock Text (lskip).
 * They are not on the caller's stack: the Text caller's task owns the stack. Freed only after a clean tx_remove. */
static struct SignalSemaphore icl_ss;
static struct Library *icl_cgx;          /* cybergraphics, for a true-colour screen; 0 = pen screens only */
static uint32_t *icl_before, *icl_after, *icl_out;
static uint8_t *icl_mk, *icl_cv, *icl_p8;
static struct BitMap *icl_tbm;           /* the one-line bitmap ReadPixelArray8/WritePixelArray8 need */
static struct RastPort icl_trp;
static icl_pentab icl_pc;               /* TE-10.6b: one shade pen cache per screen slot, used under icl_ss */
static int icl_rel[ICL_CACHE];           /* pens icl_pens_release hands back (under icl_ss) */
static int icl_ready;

/* S57-ICL-PINK-SHADE: the ColorMap's free probe (ipd_pen_is_free, as ip_holder_verify), for icl_cache_verify and flat_pens_verify */
static int cm_pen_is_free(void *cm, int pen)
{
    return ipd_pen_is_free((struct ColorMap *)cm, pen);
}

/* TE-10.6b: a slot is open when its screen is still in the public screen list the caller has locked and its epoch is still the
 * slot's (the screen did not close and come back at the same address). */
static int scr_is_open(void *ctx, const void *scr, unsigned epoch)
{
    return ipd_screen_in(ctx, scr) != NULL && scr_epoch(scr) == epoch;
}

/* S57-ICL-PEN-RECOLOUR / TE-10.6b: unload gives each slot's shade pens back on the ColorMap of the screen that obtained them, under
 * the public screen list lock (a listed screen cannot close while it is held); a slot whose screen is gone or reopened is
 * forgotten without ReleasePen (dead PalExtra semaphore, s57 hang). Caller holds icl_ss. */
static void icl_pens_release(void)
{
    struct List *pl = LockPubScreenList();
    int i, j, n;
    icl_tab_reap(&icl_pc, scr_is_open, pl);
    for (i = 0; i < ICL_TAB_SLOTS; i++) {
        icl_pencache *c = &icl_pc.slot[i];
        struct Screen *scr = c->screen ? ipd_screen_in(pl, c->screen) : NULL;
        struct ColorMap *cm = scr ? scr->ViewPort.ColorMap : NULL;
        n = icl_cache_release(c, cm ? c->screen : NULL, c->epoch, icl_rel);
        for (j = 0; j < n; j++)
            ReleasePen(cm, (ULONG)icl_rel[j]);
    }
    UnlockPubScreenList();
    icl_tab_init(&icl_pc);
}

static void icl_free(void)
{
    icl_pens_release();                         /* the shade pens outlive label calls: give them back on the live map */
    if (icl_tbm) {
        icl_tbm->Depth = 8;
        FreeBitMap(icl_tbm);
        icl_tbm = 0;
    }
    if (icl_before) { FreeVec(icl_before); icl_before = 0; }
    if (icl_after) { FreeVec(icl_after); icl_after = 0; }
    if (icl_out) { FreeVec(icl_out); icl_out = 0; }
    if (icl_mk) { FreeVec(icl_mk); icl_mk = 0; }
    if (icl_cv) { FreeVec(icl_cv); icl_cv = 0; }
    if (icl_p8) { FreeVec(icl_p8); icl_p8 = 0; }
    if (icl_cgx) { CloseLibrary(icl_cgx); icl_cgx = 0; }
    icl_ready = 0;
}

static void icl_alloc(void)
{
    if (icl_ready)
        return;
    icl_tab_init(&icl_pc);
    icl_cgx = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 41);
    icl_before = (uint32_t *)AllocVec(ICL_MAXPX * 4, MEMF_PUBLIC);
    icl_after = (uint32_t *)AllocVec(ICL_MAXPX * 4, MEMF_PUBLIC);
    icl_out = (uint32_t *)AllocVec(ICL_MAXPX * 4, MEMF_PUBLIC);
    icl_mk = (uint8_t *)AllocVec(ICL_MAXPX, MEMF_PUBLIC);
    icl_cv = (uint8_t *)AllocVec(ICL_MAXPX, MEMF_PUBLIC);
    icl_p8 = (uint8_t *)AllocVec((ICL_MAXW + 16) * ICL_MAXH, MEMF_PUBLIC);
    icl_tbm = AllocBitMap(ICL_MAXW + 16, 1, 8, BMF_CLEAR, NULL);
    if (icl_before && icl_after && icl_out && icl_mk && icl_cv && icl_p8 && icl_tbm)
        icl_ready = 1;
    else
        icl_free();
}

static uint32_t icl_rgb(struct ColorMap *cm, ULONG pen)
{
    ULONG c[3];
    GetRGB32(cm, pen, 1, c);
    return ((c[0] >> 24) << 16) | ((c[1] >> 24) << 8) | (c[2] >> 24);
}

static long icl_dist(uint32_t want, uint32_t c)
{
    pm_target t;
    pm_aim(want, &t);
    return pm_dist(&t, c);
}

/* icl_pen_fn: the cached shade, else ObtainBestPen kept only when it shows the shade (icl_pen_choice) */
typedef struct { struct ColorMap *cm; icl_pencache *pc; } icl_cx;

static int icl_pen_for(void *u, int fgpen, int bgpen, int k)
{
    icl_cx *cx = (icl_cx *)u;
    struct ColorMap *cm = cx->cm;
    icl_pencache *pc = cx->pc;
    int n = aa_ink_shades_for_depth(8), pen, got, old;
    uint32_t fr = icl_rgb(cm, (ULONG)fgpen), br = icl_rgb(cm, (ULONG)bgpen), t;
    if ((pen = icl_cache_find(pc, fgpen, bgpen, k, fr, br)) >= 0)
        return pen;
    t = icl_pen_target(fr, br, k, n);
    /* no free slot: never obtain (a pen that cannot be kept would be released while drawn); nearest of fg/bg instead */
    got = !icl_cache_has_room(pc, fgpen, bgpen, k) ? -1 : (int)ObtainBestPen(cm, (t >> 16 & 0xFF) * 0x01010101UL, (t >> 8 & 0xFF) * 0x01010101UL, (t & 0xFF) * 0x01010101UL,
                             OBP_Precision, PRECISION_IMAGE, TAG_DONE);
    pen = got >= 0 ? icl_pen_choice(k, n, got, icl_dist(t, icl_rgb(cm, (ULONG)got)), icl_dist(t, fr), icl_dist(t, br), fgpen, bgpen)
                   : icl_pen_choice(k, n, -1, 0, 0, 0, fgpen, bgpen);
    if (got >= 0 && pen != got)
        ReleasePen(cm, (ULONG)got);
    if (!icl_cache_put(pc, fgpen, bgpen, k, fr, br, pen, got >= 0 && pen == got, &old)) {
        if (got >= 0 && pen == got)
            ReleasePen(cm, (ULONG)got);
    } else if (old >= 0)
        ReleasePen(cm, (ULONG)old);
    return pen;
}

static void icl_read(struct RastPort *rp, int rtg, int x, int y, int w, int h, uint32_t *dst)
{
    int i, n = w * h;
    if (rtg) {
        vr_cgx_read_pixels(icl_cgx, rp, (UWORD)x, (UWORD)y, dst, 0, 0, (UWORD)(w * 4), (UWORD)w, (UWORD)h, VR_RECTFMT_RGBA);
        for (i = 0; i < n; i++)
            dst[i] >>= 8;
    } else {
        int pw = ((w + 15) >> 4) << 4, xx, yy;
        ReadPixelArray8(rp, (UWORD)x, (UWORD)y, (UWORD)(x + w - 1), (UWORD)(y + h - 1), icl_p8, &icl_trp);
        for (yy = 0; yy < h; yy++)
            for (xx = 0; xx < w; xx++)
                dst[yy * w + xx] = icl_p8[yy * pw + xx];
    }
}

/* S62-FLAT-RF-TX-BUDGET: icl_label runs on the stack of whichever task calls Text; it is split so no frame holds more than its
 * own step (icl_box: the ink box; icl_run: draw, read, mask; icl_write: blend and write). */

/* The label's ink box b = x, y, w, h (TextExtent at the pen) when icl_accept takes it (ui-antialias on, the window box) */
static int icl_box(flatshared *r, struct RastPort *rp, struct Window *w, CONST_STRPTR str, ULONG count, int *b)
{
    struct TextExtent te;
    int il, it, ir, ib;
    TextExtent(rp, str, count, &te);
    b[0] = rp->cp_x + te.te_Extent.MinX;
    b[1] = rp->cp_y + te.te_Extent.MinY;
    b[2] = te.te_Extent.MaxX - te.te_Extent.MinX + 1;
    b[3] = te.te_Extent.MaxY - te.te_Extent.MinY + 1;
    if (w->Flags & WFLG_GIMMEZEROZERO) {
        il = 0; it = 0; ir = w->GZZWidth - 1; ib = w->GZZHeight - 1;
    } else {   /* the whole window box: titles are drawn in the border */
        il = 0; it = 0; ir = w->Width - 1; ib = w->Height - 1;
    }
    return icl_accept(r->st.aa, rp->DrawMode, rp->FgPen, rp->BgPen, b[0], b[1], b[2], b[3], il, it, ir, ib);
}

/* The blended edges of the label in box b, ink colour fg, written back over the stock glyphs */
static void icl_write(struct RastPort *rp, const int *b, int depth, uint32_t fg, icl_cx *icx)
{
    int n = b[2] * b[3], i;
    if (depth > 8) {
        if (icl_blend_rgb(icl_cv, icl_after, fg, n, icl_out) > 0) {
            for (i = 0; i < n; i++)
                icl_out[i] = (icl_out[i] << 8) | 0xFF;
            vr_cgx_write_pixels(icl_cgx, icl_out, 0, 0, (UWORD)(b[2] * 4), rp, (UWORD)b[0], (UWORD)b[1], (UWORD)b[2], (UWORD)b[3], VR_RECTFMT_RGBA);
        }
    } else {
        if (icl_blend_pens(icl_cv, icl_after, (int)fg, aa_ink_shades_for_depth(depth), icl_pen_for, icx, n, icl_out) > 0) {
            int pw = ((b[2] + 15) >> 4) << 4, xx, yy;
            for (yy = 0; yy < b[3]; yy++)
                for (xx = 0; xx < b[2]; xx++)
                    icl_p8[yy * pw + xx] = (uint8_t)icl_out[yy * b[2] + xx];
            WritePixelArray8(rp, (UWORD)b[0], (UWORD)b[1], (UWORD)(b[0] + b[2] - 1), (UWORD)(b[1] + b[3] - 1), icl_p8, &icl_trp);
        }
        /* The shade pens stay obtained (icl_pc, bound to this screen identity): a pen freed here would go to the next client,
         * which recolours it under the glyph pixels already drawn (red in the grey antialias). */
    }
}

/* Under icl_ss: stock Text in box b, the pixels before and after, the ink mask and coverage, then icl_write */
static void icl_run(flatshared *r, struct RastPort *rp, CONST_STRPTR str, ULONG count, const int *b, int depth, icl_cx *icx)
{
    int n = b[2] * b[3], rtg = depth > 8, jam2 = (rp->DrawMode & JAM2) != 0;
    uint32_t fg;
    if (!rtg) {
        icl_trp = *rp;
        icl_trp.Layer = 0;
        icl_trp.BitMap = icl_tbm;
        icl_tbm->Depth = (UBYTE)depth;
    }
    if (!jam2)
        icl_read(rp, rtg, b[0], b[1], b[2], b[3], icl_before);
    ((textfn)r->tsw.saved)(rp, str, count, (struct Library *)GfxBase);
    icl_read(rp, rtg, b[0], b[1], b[2], b[3], icl_after);
    if (rtg)
        icl_mask(jam2, 0, icl_before, icl_after, icl_rgb(icx->cm, rp->FgPen), icl_rgb(icx->cm, rp->BgPen), n, icl_mk);
    else
        icl_mask(jam2, 1, icl_before, icl_after, rp->FgPen, rp->BgPen, n, icl_mk);
    if (!icl_fg(icl_mk, icl_after, n, &fg) || !icl_coverage(icl_mk, b[2], b[3], icl_cv)) {
        r->lskip++;
        return;
    }
    icl_write(rp, b, depth, fg, icx);
    r->labels++;
}

/* 1 = the label was drawn here (stock Text inside, edges blended or not); 0 = the caller draws stock */
static int icl_label(flatshared *r, struct RastPort *rp, CONST_STRPTR str, ULONG count)
{
    struct Window *w;
    icl_cx icx;
    int b[4], depth;
    if (!icl_ready || !rp->Layer || !rp->Layer->Window || !rp->Font || !rp->BitMap || count == 0)
        return 0;
    w = (struct Window *)rp->Layer->Window;
    if (rp != w->RPort || !icl_box(r, rp, w, str, count, b))
        return 0;
    depth = (int)GetBitMapAttr(rp->BitMap, BMA_DEPTH);
    if (depth > 8 && !icl_cgx)
        return 0;
    if (!AttemptSemaphore(&icl_ss)) {
        r->lskip++;
        return 0;
    }
    icx.cm = w->WScreen->ViewPort.ColorMap;
    icx.pc = NULL;
    if (depth <= 8) {   /* TE-10.6b: this screen's own shade cache (a screen at a new epoch gets a fresh slot, the old one is reaped) */
        icx.pc = icl_tab_for(&icl_pc, w->WScreen, flat_draw_epoch(&g_draw, w->WScreen));
        if (!icx.pc) {  /* all slots taken by other screens: stock Text, never another screen's pens */
            r->lskip++;
            ReleaseSemaphore(&icl_ss);
            return 0;
        }
        /* S57-ICL-PINK-SHADE: same identity is not proof (a Workbench reopen at the same address keeps both): ask the ColorMap */
        icl_cache_verify(icx.pc, cm_pen_is_free, icx.cm);
    }
    icl_run(r, rp, str, count, b, depth, &icx);
    ReleaseSemaphore(&icl_ss);
    return 1;
}


/* FLAT-TITLE-ACCENT: the title span between the window's system gadgets, folded one gadget at a time (flat_title_span_add, the
 * same span as flat_title_span over the first 8 system gadgets) so no gadget array sits on the drawing task's stack. */
/* One system gadget as a flat_gadrect (window-relative, the relative-size flags kept) */
static void tx_gad(const struct Window *w, const struct Gadget *g, flat_gadrect *gr)
{
    gr->x = g->LeftEdge; gr->w = g->Width;
    gr->y = g->TopEdge + ((g->Flags & GFLG_RELBOTTOM) ? w->Height : 0);    /* sizing gadget sits at the bottom */
    gr->h = g->Height + ((g->Flags & GFLG_RELHEIGHT) ? w->Height : 0);
    gr->relright = (g->Flags & GFLG_RELRIGHT) != 0;
    gr->relwidth = (g->Flags & GFLG_RELWIDTH) != 0;
    gr->drag = (g->GadgetType & GTYP_SYSGADGET) && (g->GadgetType & GTYP_SYSTYPEMASK) == GTYP_WDRAGGING;
}

static void tx_span(struct Window *w, int *sx0, int *sx1)
{
    flat_gadrect gr;
    struct Gadget *g;
    int ng = 0, n = 0;
    *sx0 = 0;
    *sx1 = w->Width - 1;
    /* FLAT-TITLE-GADGETS: system gadgets and the application's title-bar gadgets (DOpus iconify / lock / hijacked zoom) */
    for (g = w->FirstGadget; g && n < 64 && ng < 8; g = g->NextGadget, n++) {
        tx_gad(w, g, &gr);
        if (flat_title_gadget(g->GadgetType, g->Activation, (w->Flags & WFLG_GIMMEZEROZERO) != 0, &gr, w->BorderTop)) {
            flat_title_span_add(&gr, w->Width, w->BorderTop, sx0, sx1);
            ng++;
        }
    }
}

/* The decided title (ft): the band, then the title's pens (flat_tx_c draws the text and puts the caller's pens back) */
static void tx_title_draw(flatshared *r, struct RastPort *rp, const flat_title *ft)
{
    UBYTE dm = rp->DrawMode;
    WORD cx = rp->cp_x, cy = rp->cp_y;
    r->ttitle++;
    if (ft->band_pen >= 0) {
        UWORD *pa = (UWORD *)rp->AreaPtrn;
        UBYTE pt = rp->AreaPtSz;
        rp->AreaPtrn = 0; rp->AreaPtSz = 0;
        SetDrMd(rp, JAM2);
        SetAPen(rp, (UBYTE)ft->band_pen);
        ((rectfn)r->rsw.saved)(rp, ft->bx0, ft->by0, ft->bx1, ft->by1, (struct Library *)GfxBase);
        rp->AreaPtrn = pa; rp->AreaPtSz = pt;
        SetDrMd(rp, dm);
        Move(rp, cx, cy);
    }
    if (ft->text_pen >= 0)
        SetAPen(rp, (UBYTE)ft->text_pen);
    if (ft->bg_pen >= 0)
        SetBPen(rp, (UBYTE)ft->bg_pen);
}

/* The title text in the accent band (flat_title_decide): band drawn and title pens set. Returns 1 when set. */
static int tx_title(flatshared *r, struct RastPort *rp, CONST_STRPTR str, ULONG count)
{
    struct Window *w = (struct Window *)rp->Layer->Window;
    flat_title ft;
    int sx0, sx1;
    tx_span(w, &sx0, &sx1);
    if (!flat_title_decide(st_rp(r, rp), rp->BitMap ? rp->BitMap->Depth : 0, (w->Flags & WFLG_WINDOWACTIVE) != 0, (const char *)w->Title,
                           w->Width, w->BorderTop, (const char *)str, (int)count, rp->cp_y - (rp->Font ? rp->Font->tf_Baseline : 0), sx0, sx1, &ft))
        return 0;
    tx_title_draw(r, rp, &ft);
    return 1;
}

/* A label pen from the frame tracker (flat_tx_pen), set as APen. Returns 1 when set. */
static int tx_armed(flatshared *r, struct RastPort *rp)
{
    int pen = flat_tx_pen(st_rp(r, rp), &r->tx, flat_tx_surface(rp->Layer, rp->BitMap), FindTask(NULL), rp->cp_x, rp->cp_y);
    if (pen < 0)
        return 0;
    r->thits++;
    SetAPen(rp, (UBYTE)pen);
    return 1;
}

/* S62-FLAT-RF-TX-BUDGET: flat_tx_c runs on the stack of whichever task calls Text; a dispatcher, each branch a helper.
 * The flat title and the armed label only set pens (their frames are gone before the text is drawn), then every text
 * goes through one step: the icon-label blend (RL21, ui-antialias), else stock Text. The caller's pens come back after. */
void flat_tx_c(struct RastPort *rp, CONST_STRPTR str, ULONG count)
{
    flatshared *r = g_r;
    int on = r->st.enabled && !r->st.held, set;
    UBYTE fg = rp->FgPen, bg = rp->BgPen;
    r->tcalls++;
    flat_gh_text(&r->gh, flat_tx_surface(rp->Layer, rp->BitMap), FindTask(NULL), rp->cp_x, rp->cp_y, (const char *)str, (int)count);
    set = (on && r->rsw.installed && rp->Layer && rp->Layer->Window && tx_title(r, rp, str, count))
          || (r->tx.armed && tx_armed(r, rp));
    if (on && (rp->DrawMode & JAM2) && rp->BgPen == r->st.pen_back && rp->Layer) {   /* PREFS-NO-OUTLINES: text on a ground box keeps its ground */
        const flat_grbox *gb = flat_gr_match(&r->st, &r->gr, flat_tx_surface(rp->Layer, rp->BitMap), rp->cp_x, rp->cp_y, rp->cp_x, rp->cp_y);
        if (gb) {
            SetBPen(rp, (UBYTE)gb->pen);
            set = 1;
            r->gbhits++;
        }
    }
    if (!(on && icl_label(r, rp, str, count)))
        ((textfn)r->tsw.saved)(rp, str, count, (struct Library *)GfxBase);
    if (set) {
        SetAPen(rp, fg);
        SetBPen(rp, bg);
    }
}

static int tx_install(flatshared *r)
{
    unsigned char *entry = (unsigned char *)GfxBase + FLAT_LVO_TEXT;
    int rc;
    if (!flat_jmp_target(entry))
        return -100;
    flat_swap_init(&r->tsw);
    if (!r->tgate)                      /* kept across a remove and install of this record; never freed (FLAT-UNLOAD-INFLIGHT) */
        r->tgate = gate_make((void *)flat_tx_tpl, (void *)flat_tx_tpl_end, 0);
    if (!r->tgate)
        return -101;
    g_r = r;
    Forbid();
    rc = flat_swap_install(&r->tsw, (uintptr_t *)(entry + 2), (uintptr_t)r->tgate->code);
    if (rc == FLAT_OK) {
        gate_open(r->tgate, r->tsw.saved);
        r->tslot = (uintptr_t *)(entry + 2);
        libpatch_done((struct Library *)GfxBase);
    }
    Permit();
    return rc;
}

static int tx_remove(flatshared *r)
{
    int rc = FLAT_NOT_INSTALLED;
    if (!r->tsw.installed)
        return rc;
    gate_close(r->tgate);                  /* FLAT-UNLOAD-INFLIGHT: from here a caller runs the original */
    Forbid();
    rc = flat_swap_remove(&r->tsw, r->tslot, &r->st);
    if (rc == FLAT_REMOVED) {
        libpatch_done((struct Library *)GfxBase);
    }
    Permit();
    if (rc != FLAT_REMOVED)
        gate_reopen(r->tgate);
    if (rc == FLAT_REMOVED) {
        gate_wait(r->tgate);            /* FLAT-UNLOAD-INFLIGHT: every Text caller inside the entry has left */
        r->tslot = 0;
        r->tx.armed = 0;
    }
    return rc;
}

/* ---- FLAT-OUTER-EDGE: graphics.library RectFill (LVO -306, RectFill(rp a1, xMin d0, yMin d1, xMax d2, yMax d3)).
 * Intuition fills each of a window's four outer border edges with one 1 px RectFill, last in every frame redraw (3.1 and
 * 3.2), so the ring cannot come from a class hook. The entry stub (flat_rf.s) passes the caller's return address; a fill
 * is recoloured to pen_outline only when ALL hold: enabled and not held, the rastport is a window's layer, the window
 * has a border, the rect is exactly one outer strip of the window (flat_outer_edge_match) and the caller lies in
 * Intuition's ROM code. Anything else runs the stock RectFill untouched. ---- */
extern void flat_rf_tpl(void), flat_rf_tpl_end(void);

/* S32-KNOB-NONE-FRESH: XCON's scroller is a boopsi gadget (GadgetType GTYP_CUSTOMGADGET) of propgclass or a subclass. Walk OCLASS(g)
 * and cl_Super comparing cl_ID with PROPGCLASS: plain memory reads, no Intuition call (this runs inside graphics.library), depth bounded. */
static int gadget_class_is_prop(struct Gadget *g)
{
    return gadget_class_has(g, PROPGCLASS, 16);
}

/* The one prop-gadget test for scroller_pen and the sring walk; flat_gadget_is_prop holds the rule. */
static int gadget_has_propinfo(struct Gadget *g)
{
    unsigned t = g->GadgetType & GTYP_GTYPEMASK;
    return flat_gadget_is_prop(t, t == GTYP_CUSTOMGADGET && gadget_class_is_prop(g), g->SpecialInfo != 0);
}

/* S56-XCON-KNOB-BOX: the knob rectangle Intuition draws for prop gadget g (container cx,cy,cw,ch): its AUTOKNOB Image box when it
 * has one, else the pot/body layout. k = kx, ky, kw, kh. 0 when the container is empty. */
static int prop_knob(struct Gadget *g, struct PropInfo *pi, int cx, int cy, int *k)
{
    struct Image *im = (struct Image *)g->GadgetRender;
    int img_ok = (pi->Flags & AUTOKNOB) && (g->Flags & GFLG_GADGIMAGE) && im;
    return flat_scroller_knob_img(cx, cy, pi->CWidth, pi->CHeight, img_ok, img_ok ? im->LeftEdge : 0, img_ok ? im->TopEdge : 0,
                                  img_ok ? im->Width : 0, img_ok ? im->Height : 0, (pi->Flags & FREEHORIZ) != 0, (pi->Flags & FREEVERT) != 0,
                                  pi->HorizPot, pi->VertPot, pi->HorizBody, pi->VertBody, &k[0], &k[1], &k[2], &k[3]);
}

/* FLAT-SCROLLER: the pen for one Intuition fill inside a window scroller, or -1. Walks the window's prop gadgets (no
 * Intuition call: this runs inside graphics.library, GetScreenDrawInfo here could deadlock) and asks flat_scroller_pen.
 * k gets the knob box (x, y, w, h) of the matching gadget; the caller takes the knob corner cuts from it one at a time
 * (flat_scroller_cut_at, S62-FLAT-RF-TX-BUDGET: no cut array on the drawing task's stack). */
/* S69-KNOB-ASPECT: the height of the window's horizontal-only prop gadget (its container rows), 0 when it has none. The one thickness
 * Intuition fixes per screen; a vertical knob is painted that many rows thick (flat_knob_cross). */
static int hprop_rows(struct Window *w)
{
    struct Gadget *g;
    int n;
    for (g = w->FirstGadget, n = 0; g && n < 64; g = g->NextGadget, n++)
        if (gadget_has_propinfo(g) && (((struct PropInfo *)g->SpecialInfo)->Flags & (FREEHORIZ | FREEVERT)) == FREEHORIZ)
            return ((struct PropInfo *)g->SpecialInfo)->CHeight;
    return 0;
}

/* S69-KNOB-ASPECT: narrow knob k (kx, ky, kw, kh) in place to what is painted and set r->knob_side to the screen's side; returns pen. A vertical-only knob
 * becomes the horizontal knob's visual thickness wide, centred (flat_knob_cross); every other knob is painted whole. The pen decision
 * has already used Intuition's full box, which is what the fills Intuition issues for the knob cover. */
static int knob_thin(flatshared *r, struct Window *w, struct PropInfo *pi, int *k, int pen)
{
    int off = 0, n;
    r->knob_side = side_px(r, w->WScreen);
    if ((pi->Flags & (FREEHORIZ | FREEVERT)) == FREEVERT) {
        n = flat_knob_cross(k[2], hprop_rows(w), r->knob_side, &off);
        k[0] += off;
        k[2] = n;
    }
    return pen;
}

/* FLAT-WALK-CAP-BUDGET: scroller_pen_of is split in three so the knob look-up (prop_knob, the deepest RectFill chain) runs under a
 * small frame: the box test and the pen decision each have their own frame, beside the look-up and not above it. That pays for the
 * walk counter in scroller_pen (4 bytes on this chain; make flat-budget). scroller_hit: 1 when fill box lies inside prop gadget g;
 * c = the container's top-left corner. */
static int scroller_hit(struct Window *w, struct Gadget *g, const LONG *box, int *c)
{
    struct PropInfo *pi = (struct PropInfo *)g->SpecialInfo;
    int gx, gy, gw, gh;
    gx = g->LeftEdge + ((g->Flags & GFLG_RELRIGHT) ? w->Width - 1 : 0);
    gy = g->TopEdge + ((g->Flags & GFLG_RELBOTTOM) ? w->Height - 1 : 0);
    gw = g->Width + ((g->Flags & GFLG_RELWIDTH) ? w->Width : 0);
    gh = g->Height + ((g->Flags & GFLG_RELHEIGHT) ? w->Height : 0);
    if (box[0] < gx || box[1] < gy || box[2] >= gx + gw || box[3] >= gy + gh)
        return 0;
    c[0] = gx + pi->LeftBorder; c[1] = gy + pi->TopBorder;
    return 1;
}

static int scroller_pen_at(flatshared *r, struct RastPort *rp, struct Window *w, struct PropInfo *pi, const int *c, const LONG *box, int *k)
{
    return knob_thin(r, w, pi, k, flat_scroller_pen_box(st_scr(r, w->WScreen), (rp->BitMap ? rp->BitMap->Depth : 0), (w->Flags & WFLG_WINDOWACTIVE) != 0,
                                                        c[0], c[1], pi->CWidth, pi->CHeight, k[0], k[1], k[2], k[3], (int)box[0], (int)box[1], (int)box[2], (int)box[3]));
}

static int scroller_pen_of(flatshared *r, struct RastPort *rp, struct Window *w, struct Gadget *g, const LONG *box, int *k)
{
    int c[2];
    if (!scroller_hit(w, g, box, c) || !prop_knob(g, (struct PropInfo *)g->SpecialInfo, c[0], c[1], k))
        return -1;
    return scroller_pen_at(r, rp, w, (struct PropInfo *)g->SpecialInfo, c, box, k);
}

static int scroller_pen(flatshared *r, struct RastPort *rp, struct Window *w, const LONG *box, int *k)
{
    struct Gadget *g;
    int pen, n;
    for (g = w->FirstGadget, n = 0; g && n < 64; g = g->NextGadget, n++)
        if (gadget_has_propinfo(g) && (pen = scroller_pen_of(r, rp, w, g, box, k)) >= 0)
            return pen;
    return -1;
}

/* FLAT-ASPECT-BORDERS: the column count of the left and right outer lines (flat_side_px of the screen's DisplayInfo Resolution,
 * the same lookup the menus use: menu_screen_aspect). The lookup runs on the main task (aspect_update, from pens_run, over every
 * open screen under LockIBase) into a per-screen slot table (flat_aspect_slot, one lookup per screen and mode ID); the draw hook
 * only reads its slot, so the DisplayInfo call and its frame stay off the draw-task stack (flat budget, and a GetScreenDrawInfo
 * there could wait on a lock the drawing task already holds). A screen opened since the last update draws one column. */
static int side_px(flatshared *r, struct Screen *scr)
{
    return flat_aspect_find(r->aspect_slot, FLAT_ASPECT_SLOTS, scr);
}

static void aspect_update(flatshared *r, struct Screen *wb)
{
    ULONG lock = LockIBase(0);
    struct Screen *sc;
    int n;
    for (sc = IntuitionBase->FirstScreen, n = 0; sc && n < FLAT_ASPECT_SLOTS; sc = sc->NextScreen, n++) {
        ULONG mode = GetVPModeID(&sc->ViewPort);
        if (flat_aspect_need(r->aspect_slot, FLAT_ASPECT_SLOTS, sc, mode)) {
            int rx, ry;
            menu_screen_aspect(sc, &rx, &ry);
            flat_aspect_put(r->aspect_slot, FLAT_ASPECT_SLOTS, sc, mode, flat_side_px(rx, ry));
            r->aspect_looks++;
        }
    }
    UnlockIBase(lock);
    if (wb)
        r->aspect_px = flat_aspect_find(r->aspect_slot, FLAT_ASPECT_SLOTS, wb);
}

/* S32-GHOST-MENU: Intuition's 3.2 menu ghost (flat_gm_pen) becomes the dim label pen: every non-background pixel of the rect
 * (label, shortcut, checkmark, separator line) is recoloured, background pixels stay (flat_gm_recolour). The background is the
 * ghost's own FgPen: the ROM erases text holes with the menu fill pen, so the fill pen IS the menu background. Items and the
 * separator bar (rp->Layer NULL, off-screen menu bitmap) are recoloured in place; a disabled title (screen bar layer, no Window)
 * goes through a scratch bitmap so the layer's clipping holds. Non-planar (RTG) bitmaps, a refused rect or no memory: stock. */
/* The disabled screen title: through a scratch bitmap, so the layer's clipping holds. The scratch RastPort is allocated, not a
 * local (S62-FLAT-RF-TX-BUDGET: 100 bytes off the drawing task's stack). -1 = refused or no memory. */
static long ghost_title(struct RastPort *rp, const LONG *b, int depth, int dim)
{
    int wd = (int)(b[2] - b[0] + 1), ht = (int)(b[3] - b[1] + 1);
    struct BitMap *tb;
    struct RastPort *trp;
    long n = -1;
    trp = (struct RastPort *)AllocVec(sizeof(struct RastPort), MEMF_PUBLIC);
    if (!trp)
        return -1;
    tb = AllocBitMap((ULONG)wd, (ULONG)ht, (ULONG)depth, BMF_CLEAR, NULL);
    if (tb) {
        InitRastPort(trp);
        trp->BitMap = tb;
        ClipBlit(rp, (WORD)b[0], (WORD)b[1], trp, 0, 0, (WORD)wd, (WORD)ht, 0xC0);
        WaitBlit();
        if (GetBitMapAttr(tb, BMA_FLAGS) & BMF_STANDARD)
            n = flat_gm_recolour((uint8_t *const *)tb->Planes, depth, tb->BytesPerRow, (int)GetBitMapAttr(tb, BMA_WIDTH), tb->Rows, 0, 0, wd - 1, ht - 1, rp->FgPen, dim);
        if (n >= 0)
            BltBitMapRastPort(tb, 0, 0, rp, (WORD)b[0], (WORD)b[1], (WORD)wd, (WORD)ht, 0xC0);
        WaitBlit();
        FreeBitMap(tb);
    }
    FreeVec(trp);
    return n;
}

static int ghost_menu(flatshared *r, struct RastPort *rp, const LONG *b)
{
    struct BitMap *bm = rp->BitMap;
    UWORD *pa = (UWORD *)rp->AreaPtrn;
    int depth, dim;
    long n;
    if (!bm || !pa || b[2] < b[0] || b[3] < b[1] || !(GetBitMapAttr(bm, BMA_FLAGS) & BMF_STANDARD))
        return 0;
    depth = bm->Depth;
    dim = flat_gm_pen(&r->st, depth, 1, 0, rp->AreaPtSz, pa[0], pa[1], rp->FgPen);
    if (dim < 0)
        return 0;
    if (!rp->Layer) {
        WaitBlit();                                       /* the item text may still be in the blitter */
        n = flat_gm_recolour((uint8_t *const *)bm->Planes, depth, bm->BytesPerRow, (int)GetBitMapAttr(bm, BMA_WIDTH), bm->Rows, (int)b[0], (int)b[1],
                             (int)b[2], (int)b[3], rp->FgPen, dim);
    } else
        n = ghost_title(rp, b, depth, dim);
    if (n < 0)
        return 0;
    r->mhits++;
    return 1;
}

/* S62-FLAT-RF-TX-BUDGET: flat_rf_c runs on the stack of whichever task calls RectFill (4 KB from a 3.1 Shell). It is a dispatcher;
 * every branch is a helper of its own with a small frame (tests/test_flat_budget.py, make flat-budget). b = x0, y0, x1, y1. */

/* INSTALLER-BORDER-BUTTON: the state the border-button fill draws with: the screen's look (st_scr) with the screen's outline columns.
 * One static copy, held by the one task inside the fill (bb_task, claimed under Forbid): a second task meanwhile draws stock. */
static flat_state bb_st;

/* Claims the border-button fill for this task and fills bb_st. Returns the frame ids and role as one value (BB_IDS, BB_ROLE), or -1:
 * not a border button's exact box, or another task is inside the fill. Off the frame draw's stack path. */
#define BB_IDS(v)  ((v) & 0xFF)
#define BB_ROLE(v) ((v) >> 8)
static void bb_setup(flatshared *r, struct RastPort *rp)
{
    struct Window *bw = (struct Window *)rp->Layer->Window;
    const flat_state *ps = st_scr(r, bw->WScreen);
    bb_st = r->st;
    bb_st.side = (uint8_t)side_px(r, bw->WScreen);
    bb_st.pen_dim = ps->pen_dim; bb_st.pen_knob = ps->pen_knob; bb_st.pen_inbar = ps->pen_inbar; bb_st.pen_sel = ps->pen_sel;
}

static int bb_take(flatshared *r, struct RastPort *rp, int val)
{
    struct Task *me = FindTask(NULL);
    int ok;
    Forbid();
    ok = bb_task == NULL;
    if (ok)
        bb_task = me;
    Permit();
    if (!ok)
        return -1;
    bb_setup(r, rp);
    return val;
}

static int bb_claim(flatshared *r, struct RastPort *rp, const LONG *b)
{
    int role = FLAT_ROLE_PRIMARY;
    struct Gadget *bg = border_button_at(rp, (int)b[0], (int)b[1], (int)b[2], (int)b[3], &role);
    if (!bg)
        return -1;
    return bb_take(r, rp, (role << 8) | flat_border_button_ids(bg->Flags));
}

/* DOPUS-DIALOG-BUTTON-FILL: the interior fill of the pending push button of this task on this surface (dpt): claims the fill and
 * returns its frame ids and role with the full box in bb_fb (flat_rf_c draws it, as for a border button), or -1. The entry is taken
 * and the bb_task claim made in one Forbid, so dpt_hit and bb_fb stay this task's; a busy bb_task leaves the entry for the next fill. */
static LONG bb_fb[4];            /* the full box of the pending DOpus button, held by the bb_task claim (no array on the deep stack path) */

static int dopus_fill(flatshared *r, struct RastPort *rp, const LONG *b)
{
    struct Task *me = FindTask(NULL);
    int ok;
    if (!rp->Layer)
        return -1;
    Forbid();
    ok = bb_task == NULL && flat_dpend_take(&dpt, me, flat_tx_surface(rp->Layer, rp->BitMap), (int)b[0], (int)b[1], (int)b[2], (int)b[3], &dpt_hit);
    if (ok)
        bb_task = me;
    Permit();
    if (!ok)
        return -1;
    bb_setup(r, rp);
    bb_fb[0] = dpt_hit.x; bb_fb[1] = dpt_hit.y; bb_fb[2] = dpt_hit.x + dpt_hit.w - 1; bb_fb[3] = dpt_hit.y + dpt_hit.h - 1;
    return dpt_hit.val;
}

/* The frame draw of the claimed border button; releases the claim. raa_frame_draw's two halves run one after the other here on
 * bb_job (static, held by the claim like bb_st), so this frame stays small and no raa_frame_draw frame sits under the corner
 * blend's nearest-pen search (the deepest RectFill path). */
static raa_job bb_job;
static int bb_draw(struct RastPort *rp, const LONG *b, int bb)
{
    UBYTE fg = rp->FgPen, dm = rp->DrawMode;
    int ok;
    bb_job.s = &bb_st; bb_job.fn = c2_fill; bb_job.rd = c2_read; bb_job.rgb = c2_rgb; bb_job.ctx = rp;
    bb_job.ids = BB_IDS(bb); bb_job.role = BB_ROLE(bb); bb_job.fill = 1; bb_job.npens = raa_load(rp);
    bb_job.ox = (int)b[0]; bb_job.oy = (int)b[1]; bb_job.w = (int)(b[2] - b[0] + 1); bb_job.h = (int)(b[3] - b[1] + 1);
    ok = raa_frame_box(&bb_job);
    if (ok == RAA_BOX_BLEND)
        raa_frame_corners(&bb_job, NULL);
    bb_task = NULL;
    SetAPen(rp, fg);
    SetDrMd(rp, dm);
    return ok;
}

/* The drawn border button: its box goes to the ghost and label trackers */
static void bb_done(flatshared *r, struct RastPort *rp, const LONG *b, int bb)
{
    int ids = BB_IDS(bb), role = BB_ROLE(bb);
    const void *surf = flat_tx_surface(rp->Layer, rp->BitMap);
    struct Task *me = FindTask(NULL);
    flat_gh_frame(&r->gh, surf, me, ids == 1, 1, (int)b[0], (int)b[1], (int)b[2], (int)b[3]);
    flat_tx_frame(&r->tx, surf, me, flat_tx_arm(ids == 1, 1, role), 1, (int)b[0], (int)b[1], (int)b[2], (int)b[3]);
    r->bhits++;
}

/* OWNER-2026-11-08: one pixel of a disabled button's corner square that keeps what it has: inside the corner radius rr, readable and
 * not fully covered by the rounded box (raa_cover < 255: bit (dy * 4 + dx) of part, raa_partial_mask of the box). (x, y) are
 * box-relative. */
static int dim_keeps(struct RastPort *rp, const flat_gbox *gb, int rr, unsigned part, int x, int y)
{
    int bw = gb->x1 - gb->x0 + 1, bh = gb->y1 - gb->y0 + 1;
    int dx = x < rr ? x : bw - 1 - x, dy = y < rr ? y : bh - 1 - y;
    if (dx >= rr || dy >= rr)
        return 0;
    return c2_read(rp, gb->x0 + x, gb->y0 + y) >= 0 && ((part >> (dy * 4 + dx)) & 1);
}

/* The dim fill is a rectangle; the button's rounded corner squares (radius raa_radius, as the frame draw) keep the pixels they have.
 * The corner rows are filled pixel by pixel around the kept ones, the rest in one fill: the same pixels as filling the whole box and
 * writing the kept ones back, without holding them (S62-FLAT-RF-TX-BUDGET: no keep[4][4][4] on the drawing task's stack). */
/* The dim fill's corner radius rr (raa_radius, clamped to half the box and to 4) in bits 16 and up, raa_partial_mask of the box in
 * bits 0..15. rf_disabled calls this, so the raa_cover chain does not sit under dim_fill's frame (S62-FLAT-RF-TX-BUDGET). */
static unsigned dim_corner(const flat_gbox *gb)
{
    int bw = gb->x1 - gb->x0 + 1, bh = gb->y1 - gb->y0 + 1, rr = raa_radius(bh);
    if (rr > bh / 2) rr = bh / 2;
    if (rr > bw / 2) rr = bw / 2;
    if (rr > 4) rr = 4;
    return ((unsigned)rr << 16) | raa_partial_mask(bw, bh, rr);
}

static void dim_fill(flatshared *r, struct RastPort *rp, const flat_gbox *gb, unsigned corner)
{
    int bw = gb->x1 - gb->x0 + 1, bh = gb->y1 - gb->y0 + 1, rr = (int)(corner >> 16), y, x;
    unsigned part = corner & 0xffffu;
    for (y = 0; y < bh; y++) {
        if (y >= rr && y < bh - rr) {               /* the rows between the corner squares: one fill */
            ((rectfn)r->rsw.saved)(rp, gb->x0, gb->y0 + y, gb->x1, gb->y1 - rr, (struct Library *)GfxBase);
            y = bh - rr - 1;
            continue;
        }
        for (x = 0; x < bw; x++) {
            if (x == rr && bw - rr > rr) {          /* the span between the two corner squares of this row */
                ((rectfn)r->rsw.saved)(rp, gb->x0 + rr, gb->y0 + y, gb->x1 - rr, gb->y0 + y, (struct Library *)GfxBase);
                x = bw - rr - 1;
                continue;
            }
            if (!dim_keeps(rp, gb, rr, part, x, y))
                ((rectfn)r->rsw.saved)(rp, gb->x0 + x, gb->y0 + y, gb->x0 + x, gb->y0 + y, (struct Library *)GfxBase);
        }
    }
}

/* FLAT-BUTTON-DISABLED-DIM: Intuition's dither over a disabled button: solid dim fill, label again in the dim label pen */
static int rf_disabled(flatshared *r, struct RastPort *rp, struct Window *w, const LONG *b)
{
    const flat_gbox *gb = flat_gh_match(&r->st, &r->gh, flat_tx_surface(rp->Layer, rp->BitMap), FindTask(NULL), rp->AreaPtSz, (int)b[0], (int)b[1],
                                        (int)b[2], (int)b[3]);
    int depth;
    UBYTE fg, dm, pt;
    UWORD *pa;
    WORD cx, cy;
    if (!gb)
        return 0;
    depth = rp->BitMap ? rp->BitMap->Depth : 0;
    fg = rp->FgPen; dm = rp->DrawMode; pt = rp->AreaPtSz;
    pa = (UWORD *)rp->AreaPtrn;
    cx = rp->cp_x; cy = rp->cp_y;
    r->ghits++;
    rp->AreaPtrn = 0; rp->AreaPtSz = 0;
    SetDrMd(rp, JAM2);
    SetAPen(rp, (UBYTE)flat_dim_pen(st_scr(r, w->WScreen), depth));
    dim_fill(r, rp, gb, dim_corner(gb));
    if (gb->str) {
        SetDrMd(rp, JAM1);
        SetAPen(rp, (UBYTE)flat_dimtext_pen(st_scr(r, w->WScreen), depth));
        Move(rp, gb->lx, gb->ly);
        ((textfn)r->tsw.saved)(rp, gb->str, gb->count, (struct Library *)GfxBase);
    }
    Move(rp, cx, cy);
    SetAPen(rp, fg);
    SetDrMd(rp, dm);
    rp->AreaPtrn = pa; rp->AreaPtSz = pt;
    return 1;
}

/* RL6 diagnostic: log the scroller fill b, its knob (the first prop gadget left/above of the fill whose knob resolves) and the cuts */
static void sring_note(flatshared *r, struct Window *w, const LONG *b, int ncuts)
{
    int *e = r->sring[r->sring_n++ & 15];
    struct Gadget *g2;
    int n2;
    e[0] = (int)b[0]; e[1] = (int)b[1]; e[2] = (int)b[2]; e[3] = (int)b[3]; e[4] = e[5] = e[6] = e[7] = -1; e[8] = ncuts;
    for (g2 = w->FirstGadget, n2 = 0; g2 && n2 < 64; g2 = g2->NextGadget, n2++) {
        struct PropInfo *p2;
        int gx2, gy2;
        if (!gadget_has_propinfo(g2))
            continue;
        p2 = (struct PropInfo *)g2->SpecialInfo;
        gx2 = g2->LeftEdge + ((g2->Flags & GFLG_RELRIGHT) ? w->Width - 1 : 0);
        gy2 = g2->TopEdge + ((g2->Flags & GFLG_RELBOTTOM) ? w->Height - 1 : 0);
        if (b[0] >= gx2 && b[1] >= gy2 && prop_knob(g2, p2, gx2 + p2->LeftBorder, gy2 + p2->TopBorder, &e[4]))
            break;
    }
}

/* Cut ci of knob k inside fill b (flat_scroller_cut_at). One call per cut from its own frame, so vbcc does not hoist the k and b
 * element addresses into stack temporaries of the scroller_cuts loop (S62-FLAT-RF-TX-BUDGET). */
static int scroller_cut(int side, const int *k, const LONG *b, int ci, int *xy)
{
    return flat_knob_cut_at(side, k[0], k[1], k[2], k[3], (int)b[0], (int)b[1], (int)b[2], (int)b[3], ci, xy);
}

/* FLAT-SLIDER-ROUND: the knob corner cuts (flat_scroller_cut_at, knob k inside the fill b) in index order; pen >= 0 fills each in
 * that pen, pen < 0 only counts them. Returns the count. */
static int scroller_cuts(flatshared *r, struct RastPort *rp, const int *k, const LONG *b, int pen)
{
    int xy[2], ci, n = 0;
    if (pen >= 0)
        SetAPen(rp, (UBYTE)pen);
    for (ci = 0; ci < FLAT_ROUND_MAX; ci++)
        if (scroller_cut(r->knob_side, k, b, ci, xy)) {
            n++;
            if (pen >= 0)
                ((rectfn)r->rsw.saved)(rp, xy[0], xy[1], xy[0], xy[1], (struct Library *)GfxBase);
        }
    return n;
}

/* S69-KNOB-ASPECT: fill b (wider than the painted knob k) in the ground pen, then the knob columns of b in sp. */
static void scroller_thin_fill(flatshared *r, struct RastPort *rp, const LONG *b, const int *k, int sp)
{
    LONG x0 = k[0] > b[0] ? k[0] : b[0], x1 = k[0] + k[2] - 1 < b[2] ? k[0] + k[2] - 1 : b[2];
    SetAPen(rp, (UBYTE)r->st.pen_back);
    ((rectfn)r->rsw.saved)(rp, b[0], b[1], b[2], b[3], (struct Library *)GfxBase);
    SetAPen(rp, (UBYTE)sp);
    ((rectfn)r->rsw.saved)(rp, x0, b[1], x1, b[3], (struct Library *)GfxBase);
}

/* FLAT-SCROLLER: a solid fill in the scroller pen sp (the stock track is a dither); FLAT-SLIDER-ROUND: the knob corners take the
 * track pen. Returns the knob corner cut count for the RL6 note. */
static int scroller_paint(flatshared *r, struct RastPort *rp, const LONG *b, const int *k, int sp)
{
    int ncuts = scroller_cuts(r, rp, k, b, -1);
    UBYTE fg = rp->FgPen, dm = rp->DrawMode, pt = rp->AreaPtSz;
    UWORD *pa = (UWORD *)rp->AreaPtrn;
    r->shits++;
    rp->AreaPtrn = 0; rp->AreaPtSz = 0;
    SetDrMd(rp, JAM2);
    if (sp != r->st.pen_back && (b[0] < k[0] || b[2] >= k[0] + k[2]))
        scroller_thin_fill(r, rp, b, k, sp);        /* S69-KNOB-ASPECT: Intuition's wider knob fill: ground, then the thinner knob */
    else {
        SetAPen(rp, (UBYTE)sp);
        ((rectfn)r->rsw.saved)(rp, b[0], b[1], b[2], b[3], (struct Library *)GfxBase);
    }
    if (ncuts > 0)
        scroller_cuts(r, rp, k, b, r->st.pen_back);
    SetAPen(rp, fg);
    SetDrMd(rp, dm);
    rp->AreaPtrn = pa; rp->AreaPtSz = pt;
    return ncuts;
}

/* The scroller look-up (scroller_pen, a deep chain) runs from this small frame; the paint runs after it returns. The RL6 note
 * (sring_note reaches prop_knob, a deep chain) runs from here after the paint, not under the paint's frame (S62-FLAT-RF-TX-BUDGET);
 * it only writes r->sring, so the drawing is unchanged. */
static int rf_scroller(flatshared *r, struct RastPort *rp, struct Window *w, const LONG *b)
{
    int k[4], sp = scroller_pen(r, rp, w, b, k);
    if (sp < 0)
        return 0;
    sring_note(r, w, b, scroller_paint(r, rp, b, k, sp));
    return 1;
}

/* FLAT-OUTER-EDGE: a fill over the window's outer ring takes the edge pen (S32-INACTIVE-BAR: the inactive ring) */
static int rf_edge(flatshared *r, struct RastPort *rp, struct Window *w, const LONG *b)
{
    int e[4];
    UBYTE fg;
    if (!flat_outer_edge_fill(w->Width, w->Height, (int)b[0], (int)b[1], (int)b[2], (int)b[3], side_px(r, w->WScreen), e))
        return 0;
    fg = rp->FgPen;
    r->rhits++;
    SetAPen(rp, (UBYTE)flat_edge_pen(st_scr(r, w->WScreen), rp->BitMap ? rp->BitMap->Depth : 0, (w->Flags & WFLG_WINDOWACTIVE) != 0));
    ((rectfn)r->rsw.saved)(rp, e[0], e[1], e[2], e[3], (struct Library *)GfxBase);
    SetAPen(rp, fg);
    return 1;
}

/* S57-LOCALE-LIST-NO-SELECT: FILLPEN is the accent; the sides stay dark */
static int rf_side(flatshared *r, struct RastPort *rp, struct Window *w, const LONG *b)
{
    int dk[4], sp2 = flat_side_fill_pen(st_scr(r, w->WScreen), rp->FgPen, w->Width, w->Height, w->BorderLeft, w->BorderTop, w->BorderRight,
                                        w->BorderBottom, (int)b[0], (int)b[1], (int)b[2], (int)b[3], dk);
    UBYTE fg;
    if (sp2 < 0)
        return 0;
    fg = rp->FgPen;
    r->rhits++;
    if (dk[1] > b[1])
        ((rectfn)r->rsw.saved)(rp, b[0], b[1], b[2], dk[1] - 1, (struct Library *)GfxBase);
    SetAPen(rp, (UBYTE)sp2);
    ((rectfn)r->rsw.saved)(rp, dk[0], dk[1], dk[2], dk[3], (struct Library *)GfxBase);
    SetAPen(rp, fg);
    return 1;
}

/* One band k of the rect b inside the record gb, in the record's pen (the pen is set by the caller). */
static void rf_gband(flatshared *r, struct RastPort *rp, const flat_grbox *gb, const LONG *b, int k)
{
    int o[4];
    if (flat_gr_clip(gb, raa_radius(gb->y1 - gb->y0 + 1), (int)b[0], (int)b[1], (int)b[2], (int)b[3], k, o))
        ((rectfn)r->rsw.saved)(rp, o[0], o[1], o[2], o[3], (struct Library *)GfxBase);
}

/* PREFS-NO-OUTLINES: a solid window-ground fill inside a recorded ground box (flat_gr_match) is drawn in the box's own pen, the
 * rounded corner squares the frame drew kept (flat_gr_clip). 1 = handled. */
static int rf_gbox(flatshared *r, struct RastPort *rp, const LONG *b)
{
    const flat_grbox *gb;
    UBYTE fg;
    int k;
    if (rp->AreaPtSz || rp->FgPen != r->st.pen_back || !rp->Layer)
        return 0;
    gb = flat_gr_match(&r->st, &r->gr, flat_tx_surface(rp->Layer, rp->BitMap), (int)b[0], (int)b[1], (int)b[2], (int)b[3]);
    if (!gb)
        return 0;
    fg = rp->FgPen;
    SetAPen(rp, (UBYTE)gb->pen);
    for (k = 0; k < 3; k++)
        rf_gband(r, rp, gb, b, k);
    SetAPen(rp, fg);
    r->gbhits++;
    return 1;
}

void flat_rf_c(struct RastPort *rp, LONG x0, LONG y0, LONG x1, LONG y1, ULONG ret)
{
    flatshared *r = g_r;
    struct Window *w = NULL;
    LONG b[4];
    int on, rom;
    r->rcalls++;
    on = r->st.enabled && !r->st.held && rp;
    rom = ret >= r->irom_lo && ret < r->irom_hi;
    b[0] = x0; b[1] = y0; b[2] = x1; b[3] = y1;
    if (on && rp->Layer)
        w = (struct Window *)rp->Layer->Window;
    if (on && rp->AreaPtSz && rom && !w && ghost_menu(r, rp, b))
        return;
    /* INSTALLER-BORDER-BUTTON (owner 2026-11-08): the erase of a Border-render push button (exact box, border_button_at) becomes the
     * rounded flat fill of the frame draw; its label comes next and the IM_DRAWFRAME after it draws no ring (c8_skip). No ROM gate:
     * the Installer erases from its own code; the exact gadget box is the gate. */
    if (on && !rp->AreaPtSz && dpt.n && bb_task != FindTask(NULL)) {
        int db = dopus_fill(r, rp, b);
        if (db >= 0 && bb_draw(rp, bb_fb, db)) {
            bb_done(r, rp, bb_fb, db);
            return;
        }
    }
    if (on && !rp->AreaPtSz && bb_task != FindTask(NULL)) {
        int bb = bb_claim(r, rp, b);
        if (bb >= 0 && bb_draw(rp, b, bb)) {
            bb_done(r, rp, b, bb);
            return;
        }
    }
    if (on && rf_gbox(r, rp, b))
        return;
    if (on && w && rom && (rf_disabled(r, rp, w, b) || rf_scroller(r, rp, w, b)))
        return;
    if (on && r->st.pen_outline && w && !(w->Flags & WFLG_BORDERLESS) && rom && (rf_edge(r, rp, w, b) || rf_side(r, rp, w, b)))
        return;
    ((rectfn)r->rsw.saved)(rp, x0, y0, x1, y1, (struct Library *)GfxBase);
}

static int rf_install(flatshared *r)
{
    struct Resident *rt = FindResident((STRPTR)"intuition.library");
    unsigned char *entry = (unsigned char *)GfxBase + FLAT_LVO_RECTFILL;
    int rc;
    if (!rt || !rt->rt_MatchTag || rt->rt_EndSkip <= (APTR)rt->rt_MatchTag || !flat_jmp_target(entry))
        return -100;
    r->irom_lo = (uintptr_t)rt->rt_MatchTag;
    r->irom_hi = (uintptr_t)rt->rt_EndSkip;
    flat_swap_init(&r->rsw);
    if (!r->rgate)                      /* FLAT-UNLOAD-INFLIGHT: see gate_make */
        r->rgate = gate_make((void *)flat_rf_tpl, (void *)flat_rf_tpl_end, 0);
    if (!r->rgate)
        return -101;
    g_r = r;
    Forbid();
    rc = flat_swap_install(&r->rsw, (uintptr_t *)(entry + 2), (uintptr_t)r->rgate->code);
    if (rc == FLAT_OK) {
        gate_open(r->rgate, r->rsw.saved);
        r->rslot = (uintptr_t *)(entry + 2);
        libpatch_done((struct Library *)GfxBase);
    }
    Permit();
    return rc;
}

static int rf_remove(flatshared *r)
{
    int rc = FLAT_NOT_INSTALLED;
    if (!r->rsw.installed)
        return rc;
    gate_close(r->rgate);                  /* FLAT-UNLOAD-INFLIGHT: from here a caller runs the original */
    Forbid();
    rc = flat_swap_remove(&r->rsw, r->rslot, &r->st);
    if (rc == FLAT_REMOVED) {
        libpatch_done((struct Library *)GfxBase);
    }
    Permit();
    if (rc != FLAT_REMOVED)
        gate_reopen(r->rgate);
    if (rc == FLAT_REMOVED) {
        gate_wait(r->rgate);            /* FLAT-UNLOAD-INFLIGHT: every RectFill caller inside the entry has left */
        r->rslot = 0;
    }
    return rc;
}

static flatshared *find_record(void)
{
    flatshared *r;
    Forbid();
    r = (flatshared *)FindSemaphore((STRPTR)FLAT_SEM_NAME);
    Permit();
    return r;
}

/* Append a pen number (0..255) to out at *n as decimal digits (no libc formatting in this program). */
static void put_pen(char *out, int *n, unsigned v)
{
    if (v >= 100)
        out[(*n)++] = (char)('0' + v / 100);
    if (v >= 10)
        out[(*n)++] = (char)('0' + (v / 10) % 10);
    out[(*n)++] = (char)('0' + v % 10);
}

static void put_num(char *out, int *n, unsigned long v)
{
    char t[12];
    int k = 0;
    do { t[k++] = (char)('0' + v % 10); v /= 10; } while (v && k < 11);
    while (k)
        out[(*n)++] = t[--k];
}

static void put_word(char *out, int *n, const char *w)
{
    while (*w)
        out[(*n)++] = *w++;
}

/* Print the STATUS line of the record in memory (a copy taken under Forbid; the layout is checked before use). */
/* FLAT-STUCK-DIAG: is t queued on semaphore s (exclusive or shared waiter, low bit masked)? */
static int sem_waits(struct SignalSemaphore *s, struct Task *t)
{
    struct SemaphoreRequest *q;
    if (!s) return 0;
    for (q = (struct SemaphoreRequest *)s->ss_WaitQueue.mlh_Head; q->sr_Link.mln_Succ;
         q = (struct SemaphoreRequest *)q->sr_Link.mln_Succ)
        if (((ULONG)q->sr_Waiter & ~1UL) == (ULONG)t) return 1;
    return 0;
}

/* FLAT-STUCK-DIAG: "<tag>+<hex offset>" (offset as unsigned 32-bit; a negative offset shows as ffff....) */
static void stuck_off(char *out, const char *tag, ULONG off)
{
    int i;
    strcpy(out, tag); strcat(out, "+");
    out += strlen(out);
    for (i = 28; i >= 0; i -= 4) *out++ = "0123456789abcdef"[(off >> i) & 15];
    *out = 0;
}

/* FLAT-STUCK-DIAG: find the semaphore t is queued on from the SemaphoreRequest exec left on t's stack (works for private
 * semaphores). Forbid held by the caller. Returns 0 when no node on the stack looks like a queued request. */
static struct SignalSemaphore *stuck_sem(struct Task *t)
{
    UBYTE *p, *end;
    ULONG sp = (ULONG)t->tc_SPReg;
    int i;
    if (sp >= (ULONG)t->tc_SPLower && sp < (ULONG)t->tc_SPUpper) end = (UBYTE *)t->tc_SPUpper;
    else end = (UBYTE *)(sp + 4096);
    for (p = (UBYTE *)(sp & ~1UL); p + 12 <= end; p += 2) {
        struct MinNode *c = (struct MinNode *)p, *w;
        if (((*(ULONG *)(p + 8)) & ~1UL) != (ULONG)t) continue;
        if (!c->mln_Succ || !c->mln_Pred || ((ULONG)c->mln_Succ & 1) || ((ULONG)c->mln_Pred & 1)) continue;
        if ((ULONG)c->mln_Succ < 0x400 || (ULONG)c->mln_Pred < 0x400) continue;
        if (c->mln_Pred->mln_Succ != c || c->mln_Succ->mln_Pred != c) continue;
        w = c;
        for (i = 0; i < 64 && w->mln_Pred; i++) w = w->mln_Pred;
        if (w->mln_Pred) continue;
        return (struct SignalSemaphore *)((UBYTE *)w - offsetof(struct SignalSemaphore, ss_WaitQueue));
    }
    return 0;
}

/* FLAT-STUCK-DIAG: name the semaphore the resident task waits on and follow the owner chain (up to 4 links). The public
 * list, each screen's PalExtra, LayerInfo and layer locks are searched first, then the stack scan above. Forbid only. */
static void print_stuck(struct Task *t)
{
    char what[40], own[32], nm[32];
    struct SignalSemaphore *hit = 0;
    struct Screen *sc;
    struct Node *n;
    ULONG st, sw, nest = 0, owner = 0;
    LONG cnt = 0;
    int link;
    struct Task *seen[4], *cur = t;
    what[0] = own[0] = 0;
    if (!t) return;
    for (link = 0; link < 4 && cur; link++) {
        int i, cyc = 0;
        struct Task *ot;
        what[0] = own[0] = nm[0] = 0; hit = 0; owner = nest = 0; cnt = 0;
        for (i = 0; i < link; i++) if (seen[i] == cur) cyc = 1;
        if (cyc) { PutStr((STRPTR)"cycle\n"); break; }
        seen[link] = cur;
        Forbid();
        st = cur->tc_State; sw = cur->tc_SigWait;
        if (cur->tc_Node.ln_Name) strncpy(nm, cur->tc_Node.ln_Name, 31);
        if (link > 0 && !(st == TS_WAIT && (sw & SIGF_SINGLE))) { Permit(); nm[31] = 0; break; }
        for (n = SysBase->SemaphoreList.lh_Head; n->ln_Succ && !hit; n = n->ln_Succ)
            if (sem_waits((struct SignalSemaphore *)n, cur)) { hit = (struct SignalSemaphore *)n; strncpy(what, n->ln_Name ? n->ln_Name : "?", 39); }
        for (sc = IntuitionBase->FirstScreen; sc && !hit; sc = sc->NextScreen) {
            struct ColorMap *cm = sc->ViewPort.ColorMap;
            struct Layer *l;
            if (cm && cm->PalExtra && sem_waits(&cm->PalExtra->pe_Semaphore, cur)) { hit = &cm->PalExtra->pe_Semaphore; strcpy(what, "palextra:"); }
            else if (sem_waits(&sc->LayerInfo.Lock, cur)) { hit = &sc->LayerInfo.Lock; strcpy(what, "layerinfo:"); }
            else for (l = sc->LayerInfo.top_layer; l && !hit; l = l->back)
                if (sem_waits(&l->Lock, cur)) { hit = &l->Lock; strcpy(what, "layer:"); }
            if (hit && sc->Title) strncat(what, (char *)sc->Title, 39 - strlen(what));
        }
        if (!hit && (cur->tc_State == TS_WAIT)) {
            hit = stuck_sem(cur);
            if (hit) {
                ULONG a = (ULONG)hit, b = (ULONG)IntuitionBase - IntuitionBase->LibNode.lib_NegSize;
                ULONG g = (ULONG)GfxBase - GfxBase->LibNode.lib_NegSize;
                if (a >= b && a < (ULONG)IntuitionBase + IntuitionBase->LibNode.lib_PosSize) stuck_off(what, "ibase", a - (ULONG)IntuitionBase);
                else if (a >= g && a < (ULONG)GfxBase + GfxBase->LibNode.lib_PosSize) stuck_off(what, "gfx", a - (ULONG)GfxBase);
                else strcpy(what, "private");
                if (hit->ss_Link.ln_Type != NT_SIGNALSEM) strcat(what, " badtype");
            }
        }
        ot = 0;
        if (hit) {
            owner = (ULONG)hit->ss_Owner; nest = (ULONG)hit->ss_NestCount; cnt = hit->ss_QueueCount; ot = hit->ss_Owner;
            if (ot && ot->tc_Node.ln_Name) strncpy(own, ot->tc_Node.ln_Name, 31);
        }
        Permit();
        what[39] = own[31] = nm[31] = 0;
        {
            ULONG a[11];
            a[0] = (ULONG)link + 1; a[1] = (ULONG)cur; a[2] = (ULONG)nm; a[3] = st; a[4] = sw; a[5] = (ULONG)hit;
            a[6] = (ULONG)what; a[7] = owner; a[8] = (ULONG)own; a[9] = nest; a[10] = (ULONG)cnt;
            VPrintf((STRPTR)"chain %ld task=%08lx %s state=%ld sigwait=%08lx sem=%08lx %s owner=%08lx %s nest=%ld queue=%ld\n", (LONG *)a);
        }
        if (link == 0) {
            ULONG a[7]; a[0] = st; a[1] = sw; a[2] = (ULONG)hit; a[3] = (ULONG)what; a[4] = owner; a[5] = (ULONG)own; a[6] = nest;
            VPrintf((STRPTR)"stuck state=%ld sigwait=%08lx sem=%08lx %s owner=%08lx %s nest=%ld\n", (LONG *)a);
        }
        cur = ot;
    }
}


static void print_status(void)
{
    flat_state copy;
    char line[512];              /* worst case: the input-ground line with its sentinels is about 330 */
    flatshared *r;
    int found = 0, dfound = 0, dreason = 0, dmask = 0, dinst = 0;
    unsigned dver = 0, drev = 0;
    unsigned long dcalls = 0, dflat = 0, fcalls = 0, fflat = 0, fbusy = 0, fnostk = 0;
    int finst = 0, rinst = 0;
    unsigned long rcalls = 0, rhits = 0, shits = 0, mhits = 0, bhits = 0, gbhits = 0, tcalls = 0, thits = 0, labels = 0, lskip = 0;
    int tinst = 0;
    unsigned long scmin = 0, schw = 0, scstk = 0, scno = 0, scb256 = 0, scb8a = 0, scb8b = 0, schdr = 0, scguard = 0, scdeep = 0;
    char sctask[16] = "";
    unsigned long cdraws = 0, cfall = 0, alooks = 0, cfb = 0, cfd = 0, cfm = 0, cfe = 0, cfk = 0, cfi = 0, clw = 0, cls = 0, cln = 0, clm = 0;
    int apx = 0, sring[16][9], sn = 0;
    uint32_t dsum[FLAT_EL_COUNT] = { 0, 0, 0 };
    unsigned long dbevel = 0, dface = 0, dnopix = 0, d5draws = 0;
    unsigned long lbeat = 0, lnref = 0;
    struct Task *ltask = 0;
    unsigned lstage = 0, lref = 0;
    unsigned long vground = 0, vgs = 0, vgslot = 0, vgcode = 0;
    memset(&copy, 0, sizeof copy);
    Forbid();
    r = (flatshared *)FindSemaphore((STRPTR)FLAT_SEM_NAME);
    if (r) {
        found = 1;
        copy = r->st;
        dfound = r->dfound; dver = r->dver; drev = r->drev; dreason = r->dreason; dmask = r->dmask;
        dinst = d4_installed(r); dcalls = r->dcalls; dflat = r->dflat; dbevel = r->dbevel; dface = r->dface; dnopix = r->dnopix; d5draws = r->d5draws;
        memcpy(dsum, r->dsum, sizeof dsum);
        finst = r->fsw.installed; fcalls = r->fcalls; fflat = r->fflat; fbusy = r->fbusy; fnostk = r->fnostk;
        rinst = r->rsw.installed; rcalls = r->rcalls; rhits = r->rhits; shits = r->shits; mhits = r->mhits; bhits = r->bhits; gbhits = r->gbhits;
        tinst = r->tsw.installed; tcalls = r->tcalls; thits = r->thits; labels = r->labels; lskip = r->lskip;
        scmin = r->cmin; schw = r->chw; scstk = r->cstk; scno = r->cnostk; memcpy(sctask, r->ctask, 16); sctask[15] = 0;
        scb256 = r->cb256; scb8a = r->cb8a; scb8b = r->cb8b; schdr = r->chdr; scguard = r->cguard; scdeep = r->cdeep;
        cdraws = r->cdraws; cfall = r->cfall;
        cfb = r->cf_busy; cfd = r->cf_nodri; cfm = r->cf_nobm; cfe = r->cf_empty; cfk = r->cf_mask;
        cfi = r->cf_ids; clw = r->cf_lwhich; cls = r->cf_lsize; cln = r->cf_lnz; clm = r->cf_lmarks;
        apx = r->aspect_px; alooks = r->aspect_looks;
        memcpy(sring, r->sring, sizeof sring); sn = (int)(r->sring_n > 16 ? 16 : r->sring_n);
        lbeat = r->beat; lstage = r->stage; lref = r->refuse; lnref = r->nrefuse;
        ltask = r->task;
        vground = r->sground; vgs = r->gsw.installed; vgslot = (unsigned long)r->gslot;
        vgcode = r->ggate ? (unsigned long)r->ggate->code : 0;
    }
    Permit();
    if (flat_status_line(&copy, sizeof copy, found, line, sizeof line)) {
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
    }
    if (found && flat_loop_line(lbeat, lstage, lref, lnref, line, sizeof line)) {     /* FLAT-LOOP-BEAT */
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
    }
    if (found && ltask != FindTask(0)) print_stuck(ltask);     /* FLAT-STUCK-DIAG */
    if (found && flat_check(&copy, sizeof copy) == FLAT_OK) {      /* FLAT-C9: the pens the desk apply / BOOT wrote (the rig reads them) */
        int n = 0;
        put_word(line, &n, "pens text=");   put_pen(line, &n, copy.pen_text);
        put_word(line, &n, " line=");       put_pen(line, &n, copy.pen_line);
        put_word(line, &n, " back=");       put_pen(line, &n, copy.pen_back);
        put_word(line, &n, " outline=");    put_pen(line, &n, copy.pen_outline);
        put_word(line, &n, " button=");     put_pen(line, &n, copy.pen_button);
        put_word(line, &n, " ptext=");      put_pen(line, &n, copy.pen_ptext);
        line[n] = 0;
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
    }
    if (found && flat_dopus_status_line(dfound, dver, drev, dreason, dmask, dinst, dcalls, dflat, dsum, line, sizeof line)) {
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
    }
    if (found && dinst) {                           /* s66: the bevel body's own sentinel (button / frame image calls) */
        int n = 0;
        put_word(line, &n, "dopus bevel calls=");   put_num(line, &n, dbevel);
        put_word(line, &n, " face=");               put_num(line, &n, dface);
        put_word(line, &n, " nopix=");              put_num(line, &n, dnopix);
        put_word(line, &n, " d5=");                 put_num(line, &n, d5draws);
        put_word(line, &n, " ground=");             put_num(line, &n, vground);
        put_word(line, &n, " gs=");                 put_num(line, &n, vgs ? 1 : 0);
        {   /* s71: slot address and gate code address, hex, to compare with tests/amiga/gadprobe */
            static const char hx[] = "0123456789abcdef";
            unsigned long v[2];
            int k, j;
            v[0] = vgslot;
            v[1] = vgcode;
            put_word(line, &n, " gslot=0x");
            for (k = 0; k < 2; k++) {
                for (j = 28; j >= 0; j -= 4) line[n++] = hx[(v[k] >> j) & 15];
                if (!k) put_word(line, &n, " gcode=0x");
            }
        }
        {   /* FLAT-BOOT-THEME-FIELDS: read from the record at STATUS time (copy, taken under Forbid), unlike the sentinels the strgclass hook writes */
            static const char hx3[] = "0123456789abcdef";
            int k;
            put_word(line, &n, " selrgb=");
            for (k = 0; k < 3; k++) {
                line[n++] = hx3[(copy.sel_rgb[k] >> 4) & 15];
                line[n++] = hx3[copy.sel_rgb[k] & 15];
            }
            put_word(line, &n, " pensel=");         put_num(line, &n, (unsigned long)copy.pen_sel);
        }
        line[n] = 0;
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
    }
    if (found) {                                    /* FLAT-OUTER-EDGE sentinels: calls rising = the stub runs, hits rising = ring painted */
        int n = 0;
        put_word(line, &n, "rectfill installed=");  put_word(line, &n, rinst ? "yes" : "no");
        put_word(line, &n, " calls=");              put_num(line, &n, rcalls);
        put_word(line, &n, " hits=");               put_num(line, &n, rhits);
        put_word(line, &n, " shits=");              put_num(line, &n, shits);
        put_word(line, &n, " mhits=");              put_num(line, &n, mhits);
        put_word(line, &n, " bhits=");              put_num(line, &n, bhits);
        put_word(line, &n, " gboxes=");             put_num(line, &n, gbhits);
        put_word(line, &n, " side=");               put_num(line, &n, (unsigned long)apx);
        put_word(line, &n, " looks=");              put_num(line, &n, alooks);
        line[n] = 0;
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
    }
    {                                               /* RL6: the last scroller fills: sfill x0 y0 x1 y1 knobx knoby knobw knobh cuts */
        int si, k;
        for (si = 0; found && si < sn; si++) {
            int n = 0, *e = sring[si];
            put_word(line, &n, "sfill");
            for (k = 0; k < 9; k++) { put_word(line, &n, " "); put_num(line, &n, (unsigned long)(e[k] < 0 ? 0 : e[k])); }
            line[n] = 0;
            PutStr((STRPTR)line);
            PutStr((STRPTR)"\n");
        }
    }
    if (found) {                                    /* FLAT-BUTTON-FILL sentinels: calls rising = the stub runs, hits rising = pressed label recoloured */
        int n = 0;
        put_word(line, &n, "text installed=");      put_word(line, &n, tinst ? "yes" : "no");
        put_word(line, &n, " calls=");              put_num(line, &n, tcalls);
        put_word(line, &n, " hits=");               put_num(line, &n, thits);
        put_word(line, &n, " labels=");             put_num(line, &n, labels);     /* RL21 sentinels */
        put_word(line, &n, " lskip=");              put_num(line, &n, lskip);
        put_word(line, &n, " cmin=");               put_num(line, &n, scmin);
        put_word(line, &n, " ctask=");              put_word(line, &n, sctask);
        put_word(line, &n, " chw=");                put_num(line, &n, schw);
        put_word(line, &n, " cstk=");               put_num(line, &n, scstk);
        put_word(line, &n, " cnostk=");             put_num(line, &n, scno);
        line[n] = 0;
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
        n = 0;
        put_word(line, &n, "cprobe fill256=");      put_num(line, &n, scb256);
        put_word(line, &n, " deep=");               put_num(line, &n, scdeep);
        put_word(line, &n, " guard=");              put_num(line, &n, scguard);
        put_word(line, &n, " hdr=");                put_num(line, &n, schdr);
        put_word(line, &n, " b0=");                 put_num(line, &n, scb8a);
        put_word(line, &n, " b4=");                 put_num(line, &n, scb8b);
        line[n] = 0;
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
    }
    if (found) {                                    /* FLAT-GLYPH-OS-SHAPES sentinels: draws rising = the clone path ran, fallback = block glyph used */
        int n = 0;
        put_word(line, &n, "clone draws=");         put_num(line, &n, cdraws);
        put_word(line, &n, " fallback=");           put_num(line, &n, cfall);
        line[n] = 0;
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
        n = 0;
        put_word(line, &n, "clone fallback busy="); put_num(line, &n, cfb);
        put_word(line, &n, " nodri=");              put_num(line, &n, cfd);
        put_word(line, &n, " nobm=");               put_num(line, &n, cfm);
        put_word(line, &n, " empty=");              put_num(line, &n, cfe);
        put_word(line, &n, " which=");              put_num(line, &n, cfk);
        line[n] = 0;
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
        n = 0;
        put_word(line, &n, "clone empty ids=");     put_num(line, &n, cfi);
        put_word(line, &n, " last which=");         put_num(line, &n, clw);
        put_word(line, &n, " size=");               put_num(line, &n, cls);
        put_word(line, &n, " nonzero=");            put_num(line, &n, cln);
        put_word(line, &n, " marks=");              put_num(line, &n, clm);
        line[n] = 0;
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
    }
    if (found) {                                    /* LUCIDE L7: the icon pens held on the Workbench screen */
        if (ipd_status_line(line, sizeof line)) {
            int n = (int)strlen(line);
            put_word(line, &n, " dim=");  put_num(line, &n, copy.pen_dim);
            put_word(line, &n, " knob=");  put_num(line, &n, copy.pen_knob);
            put_word(line, &n, " inbar=");  put_num(line, &n, copy.pen_inbar);
            line[n] = 0;
            PutStr((STRPTR)line);
            PutStr((STRPTR)"\n");
        }
    }
    if (found && flat_frame_status_line(finst, fcalls, fflat, fbusy, fnostk, line, sizeof line)) {
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
    }
}

static int is_status(const char *a)
{
    while (*a == ' ' || *a == '\t')
        a++;
    return (a[0] == 'S' || a[0] == 's') && (a[1] == 'T' || a[1] == 't') && (a[2] == 'A' || a[2] == 'a') &&
           (a[3] == 'T' || a[3] == 't') && (a[4] == 'U' || a[4] == 'u') && (a[5] == 'S' || a[5] == 's') &&
           (a[6] == 0 || a[6] == ' ' || a[6] == '\t' || a[6] == '\n');
}

static int is_dopus(const char *a)
{
    while (*a == ' ' || *a == '\t')
        a++;
    return (a[0] == 'D' || a[0] == 'd') && (a[1] == 'O' || a[1] == 'o') && (a[2] == 'P' || a[2] == 'p') &&
           (a[3] == 'U' || a[3] == 'u') && (a[4] == 'S' || a[4] == 's') && (a[5] == 0 || a[5] == ' ' || a[5] == '\t' || a[5] == '\n');
}

static int is_help(const char *a)
{
    while (*a == ' ' || *a == '\t')
        a++;
    return qmark_line(a) || (a[0] == '-' && ((a[1] == 'h' && (a[2] == 0 || a[2] == ' ' || a[2] == '\n')) || (a[1] == '-' && a[2] == 'h')));
}

/* The slot to swap: h_SubEntry of the class that every system image (SYSIA_Which 0..0x2C) shares. The class is found
 * through a throw-away sysiclass object. 0 when it cannot be made. */
static uintptr_t *class_slot(const char *name, struct TagItem *t, int entry)
{
    uintptr_t *slot = 0;
    Object *o = NewObjectA(NULL, (UBYTE *)name, t);
    if (o) {
        struct IClass *cl = OCLASS(o);      /* the class outlives the object: it is a public class */
        slot = entry ? (uintptr_t *)&cl->cl_Dispatcher.h_Entry : (uintptr_t *)&cl->cl_Dispatcher.h_SubEntry;
        DisposeObject(o);
    }
    return slot;
}

/* sysiclass / frameiclass need a DrawInfo for their throw-away object. */
static uintptr_t *draw_class_slot(int frame)
{
    struct TagItem t[3];
    struct Screen *scr = ipd_lock_wb_screen();
    struct DrawInfo *di = scr ? GetScreenDrawInfo(scr) : NULL;
    uintptr_t *slot = 0;
    if (di) {
        t[0].ti_Tag = SYSIA_DrawInfo; t[0].ti_Data = (ULONG)di;
        t[1].ti_Tag = frame ? IA_FrameType : SYSIA_Which;  t[1].ti_Data = frame ? FRAME_BUTTON : CLOSEIMAGE;
        t[2].ti_Tag = TAG_DONE;       t[2].ti_Data = 0;
        slot = class_slot(frame ? "frameiclass" : "sysiclass", t, 0);
    }
    if (di)
        FreeScreenDrawInfo(scr, di);
    if (scr)
        UnlockPubScreen(NULL, scr);
    return slot;
}

/* DOPUS-LOCK-ICONIFY: dopusiclass keeps its 68k dispatcher in h_Entry (register convention a0/a2/a1). */
static uintptr_t *dopusiclass_slot(void)
{
    struct TagItem t[4];
    t[0].ti_Tag = TAG_USER + 5; t[0].ti_Data = 0;       /* DIA_Type: any type builds an object */
    t[1].ti_Tag = IA_Width;     t[1].ti_Data = 8;
    t[2].ti_Tag = IA_Height;    t[2].ti_Data = 8;
    t[3].ti_Tag = TAG_DONE;     t[3].ti_Data = 0;
    return class_slot("dopusiclass", t, 1);
}

static uintptr_t *sysiclass_slot(void) { return draw_class_slot(0); }
static uintptr_t *frameiclass_slot(void) { return draw_class_slot(1); }

/* Install the pass-through under Forbid. Returns FLAT_OK or a negative flat_swap code / -100 (no class) / -101 (memory). */
static int hook_install(flatshared *r)
{
    uintptr_t *slot = sysiclass_slot();
    uintptr_t ours = flat_conv(IntuitionBase->LibNode.lib_Version) == FLAT_CONV_REG ? (uintptr_t)c2_disp_reg : (uintptr_t)c2_disp_stk;
    int rc;
    if (!slot)
        return FLAT_ERR_NOCLASS;
    if (!r->sgate)                      /* FLAT-UNLOAD-INFLIGHT: the never-freed gate in front of the dispatcher */
        r->sgate = gate_make((void *)flat_dg_tpl, (void *)flat_dg_tpl_end, (ULONG)ours);
    if (!r->sgate)
        return -101;                     /* memory */
    g_r = r;                            /* the dispatcher reads it from its first call on */
    Forbid();
    rc = flat_swap_install(&r->sw, slot, (uintptr_t)r->sgate->code);     /* r->sw.saved is the old routine before the first dispatcher call */
    if (rc == FLAT_OK)
        gate_open(r->sgate, r->sw.saved);
    Permit();
    if (rc != FLAT_OK)
        return rc;
    r->slot = slot;
    {   /* INPUT-GROUND: strgclass. Its absence leaves the sysiclass hook working. */
        uintptr_t *gs = class_slot("strgclass", NULL, 1);   /* h_Entry: the subclass reaches strgclass through it, h_SubEntry is never called */
        uintptr_t gours = (uintptr_t)g9_disp_reg;     /* h_Entry: register convention */
        if (gs && !r->ggate)
            r->ggate = gate_make((void *)flat_dg_tpl, (void *)flat_dg_tpl_end, (ULONG)gours);
        if (gs && r->ggate) {
            Forbid();
            rc = flat_swap_install(&r->gsw, gs, (uintptr_t)r->ggate->code);
            if (rc == FLAT_OK)
                gate_open(r->ggate, r->gsw.saved);
            Permit();
            if (rc == FLAT_OK)
                r->gslot = gs;
        }
    }
    {   /* FLAT-C8: the second class. Its absence leaves the sysiclass hook working: report it, do not undo C2. */
        uintptr_t *fs = frameiclass_slot();
        uintptr_t fours = flat_conv(IntuitionBase->LibNode.lib_Version) == FLAT_CONV_REG ? (uintptr_t)c8_disp_reg : (uintptr_t)c8_disp_stk;
        if (!fs)
            return FLAT_OK;
        if (!r->fgate)
            r->fgate = gate_make((void *)flat_dg_tpl, (void *)flat_dg_tpl_end, (ULONG)fours);
        if (!r->fgate)
            return FLAT_OK;             /* as an absent class: the sysiclass hook keeps working */
        Forbid();
        rc = flat_swap_install(&r->fsw, fs, (uintptr_t)r->fgate->code);
        if (rc == FLAT_OK)
            gate_open(r->fgate, r->fsw.saved);
        Permit();
        if (rc == FLAT_OK)
            r->fslot = fs;
    }
    return FLAT_OK;
}

/* Remove the swap if the slot is still ours (Forbid). The dispatcher stays reachable while a
 * foreign hook is chained on top of it, its code is still reachable and must stay. */
static int hook_remove(flatshared *r)
{
    int rc = FLAT_NOT_INSTALLED;
    if (!r->sw.installed)
        return rc;
    if (r->gsw.installed) {             /* INPUT-GROUND: the strgclass hook before all */
        gate_close(r->ggate);
        Forbid();
        rc = flat_swap_remove(&r->gsw, r->gslot, &r->st);
        Permit();
        if (rc == FLAT_STILL_CHAINED) {
            gate_reopen(r->ggate);
            return rc;
        }
        gate_wait(r->ggate);
        r->gslot = 0;
    }
    if (r->fsw.installed) {             /* FLAT-C8: the frameiclass hook first; chained on top = keep both */
        gate_close(r->fgate);           /* FLAT-UNLOAD-INFLIGHT: closed, restore, then wait */
        Forbid();
        rc = flat_swap_remove(&r->fsw, r->fslot, &r->st);
        Permit();
        if (rc == FLAT_STILL_CHAINED) {
            gate_reopen(r->fgate);
            return rc;
        }
        gate_wait(r->fgate);
        r->fslot = 0;
    }
    gate_close(r->sgate);
    Forbid();
    rc = flat_swap_remove(&r->sw, r->slot, &r->st);
    Permit();
    if (rc != FLAT_REMOVED)
        gate_reopen(r->sgate);
    if (rc == FLAT_REMOVED) {
        gate_wait(r->sgate);            /* every caller inside the dispatcher has left (it counts in before it looks at the flag) */
        r->slot = 0;
    }
    return rc;
}

/* ---- FLAT-D4: find dopus5.library among the resident libraries (never load it), gate it, swap the two entries ---- */
/* FLAT-LOOP-BEAT: a Ctrl-C refusal, kept for STATUS (the refusal line itself goes to a buffered stdout) */
static void refused(flatshared *r, unsigned why)
{
    r->refuse = (UWORD)why;
    r->nrefuse++;
}

static void dopus_unpatch(flatshared *r)
{
    int el, d5_removed = 0, removed[FLAT_EL_COUNT];
    Forbid();
    if (r->d5sw.installed) {
        gate_close(r->d5gate);          /* FLAT-UNLOAD-INFLIGHT: closed, restore, then wait */
        if (flat_swap_remove(&r->d5sw, r->d5slot, &r->st) == FLAT_REMOVED)     /* FLAT_STILL_CHAINED: installed stays set, the library stays open */
            d5_removed = 1;
        else
            gate_reopen(r->d5gate);
    }
    for (el = 0; el < FLAT_EL_COUNT; el++) {
        removed[el] = 0;
        if (r->dsw[el].installed) {
            int rc;
            gate_close(r->dgate[el]);   /* the gate passes a late caller to the original (for a body hook: the body, restored below in this same Forbid) */
            if (r->dbody[el])
                rc = flat_body_remove(&r->dsw[el], r->dbody[el], r->dorig[el], r->dplen[el], &r->st);
            else
                rc = flat_swap_remove(&r->dsw[el], r->dslot[el], &r->st);
            if (rc == FLAT_REMOVED)
                removed[el] = 1;
            else
                gate_reopen(r->dgate[el]);     /* refused (a foreign hook sits on top): the gate keeps serving as it did */
        }
    }
    if (r->dlib)
        libpatch_done(r->dlib);
    CacheClearU();
    Permit();
    if (d5_removed)
        gate_wait(r->d5gate);           /* every caller inside the dopusiclass replacement has left */
    for (el = 0; el < FLAT_EL_COUNT; el++)
        if (removed[el])
            gate_wait(r->dgate[el]);    /* every caller inside a D4 library entry replacement has left (it counts in before it looks at the flag) */
    if (!d4_installed(r) && r->dlib) {
        CloseLibrary(r->dlib);
        r->dlib = 0;
        r->dmask = 0;
    }
}

/* FLAT-UNLOAD-INFLIGHT: the gate of D4 element el for the handler ours. A gate keeps its handler in the copied code, so one is
 * reused only when the handler is the same (the LVO swap and the body hook of FieldBox use different handlers); otherwise a new
 * block is made and the old one, idle after its wait, stays behind like every gate. NULL: out of memory, element stays stock. */
static flat_gate *dgate_get(flatshared *r, int el, uintptr_t ours)
{
    if (!r->dgate[el] || r->dgate_h[el] != (ULONG)ours) {
        r->dgate[el] = gate_make((void *)flat_dg_tpl, (void *)flat_dg_tpl_end, (ULONG)ours);
        r->dgate_h[el] = r->dgate[el] ? (ULONG)ours : 0;
    }
    return r->dgate[el];
}

static void dopus_scan(flatshared *r)
{
    struct ExecBase *eb = *(struct ExecBase **)4;
    struct Library *lib;
    uint32_t sums[FLAT_EL_COUNT];
    int n, el, mask;
    const flat_dopus_build *tab = flat_dopus_table(&n);
    if (d4_installed(r))
        return;                         /* already patched */
    Forbid();
    lib = (struct Library *)FindName(&eb->LibList, (STRPTR)"dopus5.library");
    if (lib)
        lib = OpenLibrary((STRPTR)"dopus5.library", 0);     /* resident: no disk load; keeps it from expunging */
    Permit();
    r->dfound = lib != 0;
    if (!lib)
        return;
    r->dver = lib->lib_Version;
    r->drev = lib->lib_Revision;
    for (el = 0; el < FLAT_EL_COUNT; el++) {
        uint32_t t = d4_lvo[el] ? flat_jmp_target((const unsigned char *)lib + d4_lvo[el])
                                : flat_bevel_body(tab, n, r->dver, r->drev, flat_jmp_target((const unsigned char *)lib + FLAT_LVO_DRAWBOX));
        sums[el] = t ? flat_sum_reloc((const void *)t, FLAT_FUNC_BYTES) : 0;
        r->dsum[el] = sums[el];
    }
    mask = flat_dopus_gate(tab, n, r->dver, r->drev, sums, &r->dreason);
    if (!mask) {
        CloseLibrary(lib);
        return;
    }
    g_r = r;
    Forbid();
    for (el = 0; el < FLAT_EL_COUNT; el++) {
        unsigned char *entry = (unsigned char *)lib + (d4_lvo[el] ? d4_lvo[el] : FLAT_LVO_DRAWBOX);
        uint32_t t = flat_jmp_target(entry);
        if (el == FLAT_EL_BEVEL)
            t = flat_bevel_body(tab, n, r->dver, r->drev, t);
        if (!(mask & (1 << el)) || !t || flat_sum_reloc((const void *)t, FLAT_FUNC_BYTES) != sums[el]) {
            mask &= ~(1 << el);         /* changed since the gate (a foreign patch landed): leave it alone */
            continue;
        }
        flat_swap_init(&r->dsw[el]);
        r->dslot[el] = (uintptr_t *)(entry + 2);
        r->dbody[el] = 0;
        /* 0c: dopus5's own gadget classes jsr the BODY, never the jump table, so hook the body entry itself when its
         * prologue bytes are exactly the known ones; any other build keeps the LVO swap (fail closed) */
        {
            unsigned char *body = (unsigned char *)(el == FLAT_EL_FIELDBOX ? flat_fieldbox_body((const unsigned char *)t, t) : t);
            int plen = body ? flat_body_prologue(el, body) : 0;
            void *ours = el == FLAT_EL_DRAWBOX ? (void *)d4_drawbox : el == FLAT_EL_FIELDBOX ? (void *)d4_fieldbody : (void *)d4_bevelbody;
            if (plen) {
                flat_gate *g = dgate_get(r, el, (uintptr_t)ours);
                if (!g || flat_body_install(&r->dsw[el], body, plen, (unsigned char *)r->dtramp[el], r->dorig[el],
                                            (uintptr_t)g->code) != FLAT_OK)
                    mask &= ~(1 << el);
                else {
                    r->dbody[el] = body;
                    r->dplen[el] = plen;
                    gate_open(g, (uintptr_t)body);     /* a late caller of the closed gate runs the body, which unload has restored by then (the trampoline lives in the record) */
                }
                continue;
            }
        }
        if (el == FLAT_EL_BEVEL) {          /* no LVO slot to fall back on: an unknown prologue stays stock */
            mask &= ~(1 << el);
            continue;
        }
        {
            uintptr_t ours = el == FLAT_EL_DRAWBOX ? (uintptr_t)d4_drawbox : (uintptr_t)d4_fieldbox;
            flat_gate *g = dgate_get(r, el, ours);
            if (!g || flat_swap_install(&r->dsw[el], r->dslot[el], (uintptr_t)g->code) != FLAT_OK)
                mask &= ~(1 << el);
            else
                gate_open(g, r->dsw[el].saved);
        }
    }
    libpatch_done(lib);
    CacheClearU();                      /* the body entries are code just written */
    Permit();
    if (mask) {                         /* DOPUS-LOCK-ICONIFY: same gate, same lifetime as the D4 entries */
        uintptr_t *s5 = dopusiclass_slot();
        flat_swap_init(&r->d5sw);
        r->d5slot = s5;
        Forbid();
        if (!r->d5gate)
            r->d5gate = gate_make((void *)flat_dg_tpl, (void *)flat_dg_tpl_end, (ULONG)d5_disp_reg);
        if (!s5 || !r->d5gate || flat_swap_install(&r->d5sw, s5, (uintptr_t)r->d5gate->code) != FLAT_OK)
            r->d5slot = 0;              /* refused: dopusiclass stays stock */
        else
            gate_open(r->d5gate, r->d5sw.saved);
        Permit();
    }
    r->dmask = mask;
    if (mask)
        r->dlib = lib;
    else
        CloseLibrary(lib);
}

/* ---- LUCIDE L7 / F14: amarchy-flat is one OWNER of the shared icon pen holder (iconpen_dos.c; amarchy-icons APPLY is the other).
 * It holds the 16 pens for its whole run, flat on or off (ip_hold_wanted: a released pen heads the free list and the next shared
 * pen takes it) and gives its reference back on quit or when the kill switch is held; the pens go only with the last owner, so
 * unloading flat leaves applied icons themed. Nothing in the OS tells us the Workbench screen closed and reopened (no
 * screennotify exists in amarchy; wbreset_patch.c signals only clients with windows), so pens_sync runs at start, on Ctrl-D
 * (desk apply signals it after writing flat_state) and on a 1 s tick that compares the screen pointer and depth. ---- */
static void pens_sync(flatshared *r, int quit)
{
    int hold = ip_hold_wanted(quit, r->st.held != 0, GfxBase && GfxBase->LibNode.lib_Version >= 39);   /* not the flat switch */
    STG(FLAT_STG_PENS_SYNC);
    ipd_owner(IP_OWNER_FLAT, hold);     /* also syncs the pens with the owners and the screen */
}

/* Take the record out of the list and free it; 0 when someone holds it (never pull it from under a user). */
static int drop_record(flatshared *r)
{
    int ok = 0;
    Forbid();
    if (AttemptSemaphore(&r->ss)) {
        RemSemaphore(&r->ss);
        ReleaseSemaphore(&r->ss);
        ok = 1;
    }
    Permit();
    if (ok)
        FreeVec(r);
    return ok;
}

/* ---- FLAT-C4: the kill switches' inputs. The decision is flat_core.c (host-tested); here only the reads. ---- */
/* The marker's start count: 0 no file, else flat_marker_parse of its text (unreadable = 1). */
static int marker_count(void)
{
    char buf[32];
    LONG n;
    BPTR f = Open((STRPTR)FLAT_MARKER_PATH, MODE_OLDFILE);
    if (!f)
        return 0;
    n = Read(f, buf, (LONG)sizeof buf);
    Close(f);
    return flat_marker_parse(buf, n > 0 ? (unsigned)n : 0);
}

static int g_marker_prev;                       /* the count found at boot: this start writes g_marker_prev + 1 */

/* Write the marker and make the handler flush it to the disk: the swap must not start before it is safe. */
static void marker_write(int count)
{
    char txt[24];
    unsigned tl = flat_marker_format(count, txt);
    BPTR l = Lock((STRPTR)"ENVARC:Amarchy", ACCESS_READ), f;
    if (!l)
        l = CreateDir((STRPTR)"ENVARC:Amarchy");
    if (l)
        UnLock(l);
    f = Open((STRPTR)FLAT_MARKER_PATH, MODE_NEWFILE);
    if (f) {
        Write(f, (APTR)txt, (LONG)tl);
        Close(f);
    }
    l = Lock((STRPTR)"ENVARC:", ACCESS_READ);
    if (l) {
        DoPkt(((struct FileLock *)BADDR(l))->fl_Task, ACTION_FLUSH, 0, 0, 0, 0, 0);
        UnLock(l);
    }
}

static void marker_delete(void)
{
    DeleteFile((STRPTR)FLAT_MARKER_PATH);
}

/* A task or process named VisualPrefs / Birdie, as FLAT_HELD_*. */
static int scan_list(struct List *list)
{
    struct Node *n;
    int why = FLAT_HELD_NONE;
    for (n = list->lh_Head; !why && n->ln_Succ; n = n->ln_Succ)
        why = flat_other_task(n->ln_Name);
    return why;
}

static int other_patcher(uintptr_t *slot)
{
    struct Resident *rt;
    int why;
    Forbid();
    why = flat_other_task(SysBase->ThisTask->tc_Node.ln_Name);
    if (!why)
        why = scan_list(&SysBase->TaskReady);
    if (!why)
        why = scan_list(&SysBase->TaskWait);
    Permit();
    if (!why && slot && (rt = FindResident((STRPTR)"intuition.library")) != 0)
        why = flat_other_hook((unsigned long)*slot, (unsigned long)rt->rt_MatchTag, (unsigned long)rt->rt_EndSkip);
    return why;
}

/* Left button: CIA-A PRA bit 6, right button: POTGOR bit 10, both low while down. */
static int buttons(int *lmb, int *rmb)
{
    *lmb = !(*(volatile UBYTE *)0xBFE001 & 0x40);
    *rmb = !(*(volatile UWORD *)0xDFF016 & 0x0400);
    return 0;
}

static int is_boot(const char *a)
{
    while (*a == ' ' || *a == '\t')
        a++;
    return (a[0] == 'B' || a[0] == 'b') && (a[1] == 'O' || a[1] == 'o') && (a[2] == 'O' || a[2] == 'o') &&
           (a[3] == 'T' || a[3] == 't') && (a[4] == 0 || a[4] == ' ' || a[4] == '\t' || a[4] == '\n');
}

/* BOOT: is any flat feature switched on in the Amarchy prefs? */
static int boot_wanted(void)
{
    am_prefs *p;
    int on;
    p = amconf_borrow();
    on = amconf_flat_index(p, AM_FLAT) || amconf_flat_index(p, AM_FLATGAD);
    amconf_return(p);
    return on;
}

/* FLAT-BUTTON-DISABLED-DIM: the dim fill pen. The colour of the button fill pen on the Workbench screen scaled to DIM_PCT percent,
 * obtained as a shared pen (ObtainBestPen: an exact or nearest colour, no DrawInfo pen is recoloured) and held until the next refresh or
 * exit. Runs on the main task only: ObtainBestPen is never called inside the RectFill hook. Screens under 8 colours keep pen_dim 0
 * (flat_dim_pen then falls back to the plain button pen). */
/* S32-STALE-HELD-PENS: the pens (dim, knob, inbar, sel) of a screen are a flat_pens slot of g_pens (flat_core.c, TE-10.6b: one per screen, declared above the hooks): remembered with the screen and its ipd_epoch,
 * released only on that identity and forgotten, not released, on any other screen. KNOB-PEN: the scroller knob pen, from 16 colours
 * up (a 3 plane screen has no free pen: pen_knob stays 0 and flat_knob_pen answers DK_MUTED). INBAR-PEN (S32-INACTIVE-BAR): the
 * inactive dragbar / ring / arrow / knob pen, colour = the desk's inbar_rgb (theme `inactive`) when set, else derived from the screen. */
typedef struct { struct ColorMap *cm; } pen_cx;

static int fp_obtain(void *ctx, unsigned long c)
{
    pen_cx *x = ctx;
    return (int)ObtainBestPen(x->cm, ((c >> 16) & 0xff) * 0x01010101UL, ((c >> 8) & 0xff) * 0x01010101UL, (c & 0xff) * 0x01010101UL,
                              OBP_Precision, PRECISION_GUI, OBP_FailIfBad, FALSE, TAG_DONE);
}

static void fp_release(void *ctx, int pen)
{
    pen_cx *x = ctx;
    ReleasePen(x->cm, (ULONG)pen);
}

static unsigned long fp_rgb(void *ctx, int pen)
{
    pen_cx *x = ctx;
    ULONG a[3];
    GetRGB32(x->cm, (ULONG)pen, 1, a);
    return ((a[0] >> 24) << 16) | ((a[1] >> 24) << 8) | (a[2] >> 24);
}

/* TE-10.6b: forget (without ReleasePen: the ColorMap went with the screen) the pen slots of every screen no longer in the public screen
 * list or reopened at another epoch, under the list lock; icl_ss guards the icl table against the Text hook. */
static void pens_reap(void)
{
    struct List *pl;
    STG(FLAT_STG_REAP_LIST);
    pl = LockPubScreenList();
    flat_pens_tab_reap(&g_pens, scr_is_open, pl);
    if (icl_ready) {
        STG(FLAT_STG_REAP_ICL);
        ObtainSemaphore(&icl_ss);
        icl_tab_reap(&icl_pc, scr_is_open, pl);
        ReleaseSemaphore(&icl_ss);
    }
    UnlockPubScreenList();
}

/* TE-10.6b budget fix: main task. Collect (screen, epoch) of every open public screen except the Workbench under the list lock (the
 * epoch lookups are iconpen_dos calls, allowed here), then write g_draw under Forbid (the hooks run in other tasks). Sync points: the
 * end of every pens_run (start, CTRL_D refresh, 1 s watch) and the held branches that skip pens_run. */
static flat_draw_scr g_draw_list[FLAT_PENS_SLOTS];

static void draw_sync(flatshared *r)
{
    struct List *pl;
    struct Node *nd;
    int n = 0;
    unsigned wbep;
    STG(FLAT_STG_DRAW_SYNC);
    pl = LockPubScreenList();
    wbep = ipd_epoch();
    if (pl) {
        for (nd = pl->lh_Head; nd->ln_Succ && n < FLAT_PENS_SLOTS; nd = nd->ln_Succ) {
            const struct Screen *sc = ((struct PubScreenNode *)nd)->psn_Screen;
            if (!sc || sc == (const struct Screen *)g_wb_scr)
                continue;
            g_draw_list[n].screen = sc;
            g_draw_list[n].epoch = ipd_epoch_of_screen(sc);
            n++;
        }
    }
    UnlockPubScreenList();
    Forbid();
    flat_draw_sync(&g_draw, &r->st, &g_pens, g_draw_list, n, g_wb_scr, wbep);
    Permit();
}

/* mode 0: refresh (release on the identity, obtain again), 1: follow the screen (1 s watch), 2: release. Runs under the locked
 * Workbench screen so its ColorMap cannot close. TE-10.6b: the Workbench screen's slot of g_pens; its numbers are also stored into
 * r->st. Other screens obtain no pens here (their hooks draw with the stock fallbacks). */
static void pens_run(flatshared *r, int mode)
{
    struct Screen *scr;
    pen_cx x;
    flat_pen_ops o;
    const void *id;
    flat_pens *sl;
    unsigned ep;
    int depth = 0;
    STG(FLAT_STG_LOCK_WB);
    scr = ipd_lock_wb_screen();
    STG(FLAT_STG_PENS);
    id = scr;
    ep = ipd_epoch();
    x.cm = scr ? scr->ViewPort.ColorMap : NULL;
    o.ctx = &x;
    o.obtain = fp_obtain;
    o.release = fp_release;
    o.rgb = fp_rgb;
    if (scr && scr->RastPort.BitMap)
        depth = scr->RastPort.BitMap->Depth;
    if (!x.cm)
        id = NULL;
    g_wb_scr = id;
    aspect_update(r, scr);
    sl = flat_pens_tab_for(&g_pens, id, ep);
    if (!sl)
        sl = &g_pens.slot[0];       /* no screen, or no free slot: slot 0 only forgets or releases what it holds */
    if (mode == 0)
        flat_pens_refresh_on(sl, &o, &r->st, &r->st, id, ep, depth);
    else if (mode == 1) {
        flat_pens_verify(sl, &r->st, cm_pen_is_free, x.cm, id, ep);   /* S57-ICL-PINK-SHADE: same address reopen */
        flat_pens_follow_on(sl, &o, &r->st, &r->st, !r->st.held, id, ep, depth);
    } else
        flat_pens_release(sl, &o, &r->st, id, ep);
    if (scr)
        UnlockPubScreen(NULL, scr);
    draw_sync(r);
    if (mode == 1)
        pens_reap();
}

static void dim_refresh(flatshared *r)
{
    pens_run(r, 0);
}

static void dim_release(flatshared *r)
{
    pens_run(r, 2);
}

/* The hook and the two patches, the crash marker around them. A FLAT_ERR_NOCLASS result (the Workbench screen not
 * open yet: amarchy-flat BOOT runs before LoadWB) is retried from the 1 s screen watch (flat_install_due). */
static int install_all(flatshared *r, poll_timer *tm, int *marker_live)
{
    int rc;
    marker_write(g_marker_prev + 1);                             /* on disk before the swap, so a crash leaves it */
    *marker_live = 1;
    rc = hook_install(r);
    if (rc != FLAT_OK) {
        if (rc != FLAT_ERR_NOCLASS)
            PutStr((STRPTR)"amarchy-flat: sysiclass hook not installed\n");
        marker_delete();
        *marker_live = 0;
        return rc;
    }
    if (rf_install(r) != FLAT_OK)
        PutStr((STRPTR)"amarchy-flat: RectFill patch not installed\n");
    icl_alloc();
    if (tx_install(r) != FLAT_OK)
        PutStr((STRPTR)"amarchy-flat: Text patch not installed\n");
    pt_start(tm, (unsigned long)FLAT_MARKER_SECS * 1000000UL);
    return rc;
}

int main(void)
{
    const char *args = (const char *)GetArgStr();
    flatshared *r;
    poll_timer tm, pen_tm;
    int install_rc = FLAT_ERR_ARG;                      /* no install tried (held or no libraries) */
    int marker_live = 0;
    flat_state st0;                                     /* the prefs' state, read before the instance guard */
    (void)ver;
    stk_probe_init("AMFLT");
    InitSemaphore(&cl_ss);
    InitSemaphore(&icl_ss);
    if (is_help(args)) {
        PutStr((STRPTR)"amarchy-flat [STATUS]: the flat-UI resident program; prints flat=on|off|absent|held. Ctrl-C unloads it\n");
        return 0;
    }
    if (is_status(args)) {
        print_status();
        return 0;
    }
    if (is_dopus(args)) {                               /* ask the running program to look for DOpus again */
        flatshared *q = find_record();
        if (q && q->task)
            Signal(q->task, SIGBREAKF_CTRL_D);
        else
            PutStr((STRPTR)"amarchy-flat: not running\n");
        return q ? 0 : 5;
    }
    if (is_boot(args) && !boot_wanted())
        return 0;
    flat_init(&st0);
    {   /* FLAT-C9: the switch and pens from the prefs (the one mapping desk apply uses too). Read here, before the
           guard: a DOS read inside Forbid waits and breaks it, and a second starter (User-Startup's BOOT line and
           Amarchy's start gate run in the same second) then passed the guard too and patched twice (KS31-BOOT-CRASH) */
        static gtheme boot_theme;                   /* FLAT-BOOT-THEME-FIELDS: static, a theme is large for main's stack */
        am_prefs *bp;
        gt_dos_load_current(&boot_theme);           /* the saved theme (prefs AM_GTHEME, else the default); an unreadable file gives gt_load's built-in default */
        bp = amconf_borrow();
        desk_flat_full(amconf_flat_index(bp, AM_FLAT), amconf_flat_index(bp, AM_FLATGAD), amconf_flat_index(bp, AM_UIAA), &boot_theme, &st0);
        amconf_return(bp);
    }
    Forbid();
    r = (flatshared *)FindSemaphore((STRPTR)FLAT_SEM_NAME);
    if (r && task_alive(r->task, r->tname)) {
        Permit();
        PutStr((STRPTR)"amarchy-flat: already running\n");
        print_status();
        return 5;
    }
    if (r) {                                            /* a dead owner left its record: take it over, never leak it */
        r->task = 0;
        Permit();
        if (!drop_record(r)) {
            PutStr((STRPTR)"amarchy-flat: a stale record is held\n");
            return 5;
        }
        Forbid();
    }
    r = (flatshared *)AllocVec(sizeof(flatshared), MEMF_PUBLIC | MEMF_CLEAR);
    if (r) {
        struct Task *me = FindTask(0);
        int i;
        for (i = 0; FLAT_SEM_NAME[i] && i < (int)sizeof r->name - 1; i++)
            r->name[i] = FLAT_SEM_NAME[i];
        r->ss.ss_Link.ln_Name = r->name;
        r->st = st0;
        r->task = me;
        if (me->tc_Node.ln_Name) {
            for (i = 0; me->tc_Node.ln_Name[i] && i < (int)sizeof r->tname - 1; i++)
                r->tname[i] = me->tc_Node.ln_Name[i];
        }
        AddSemaphore(&r->ss);
    }
    Permit();
    if (!r) {
        PutStr((STRPTR)"amarchy-flat: out of memory\n");
        return 10;
    }
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 36);
    memset(&tm, 0, sizeof tm);   /* pt_stop reads busy: garbage there AbortIOs a random address (hung the rig) */
    pt_open(&tm);
    memset(&pen_tm, 0, sizeof pen_tm);
    pt_open(&pen_tm);
    clone_stack_alloc(r);                               /* S63-KS31-STACK: before any hook can run the clone */
    if (IntuitionBase && GfxBase) {
        int lmb, rmb, why;
        buttons(&lmb, &rmb);
        g_marker_prev = marker_count();
        why = flat_start_decide(g_marker_prev, lmb, rmb, other_patcher(sysiclass_slot()));
        if (why) {                                      /* a kill switch is set: stay resident, inert, say why */
            r->st.held = (uint16_t)why;
            if (why == FLAT_HELD_MARKER) {
                g_marker_prev = 0;
                marker_delete();                        /* one inert start per crash run, not for ever */
            }
        } else {
            install_rc = install_all(r, &tm, &marker_live);
        }
    }
    g_r = r;
    flat_pens_tab_init(&g_pens);
    flat_draw_init(&g_draw);
    pens_sync(r, 0);                                    /* the exclusive icon pens first: a shared pen (dim) must not sit on one */
    if (!r->st.held) {
        dopus_scan(r);
        dim_refresh(r);
    } else
        draw_sync(r);
    pt_start(&pen_tm, 1000000UL);                       /* the Workbench screen watch (pens_sync) */
    print_status();
    for (;;) {
        ULONG got;
        STG(FLAT_STG_WAIT);
        got = Wait(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | pt_sigmask(&tm) | pt_sigmask(&pen_tm));
        r->beat++;
        stk_probe();
        if (pt_fired(&pen_tm, got)) {                   /* Workbench closed / reopened / mode change */
            if (IntuitionBase && GfxBase && flat_install_due(r->st.held, install_rc == FLAT_OK, install_rc)) {
                STG(FLAT_STG_INSTALL);
                install_rc = install_all(r, &tm, &marker_live);
            }     /* S32-START-LOOK: LoadWB opened the screen */
            pens_sync(r, 0);
            pens_run(r, 1);                             /* S32-STALE-HELD-PENS: a new screen forgets the old pens and obtains on it (icon pens synced first) */
            pt_start(&pen_tm, 1000000UL);
        }
        if (marker_live && pt_fired(&tm, got)) {        /* 60 s without a crash: the start is safe */
            STG(FLAT_STG_MARKER);
            marker_delete();
            marker_live = 0;
        }
        if (got & SIGBREAKF_CTRL_D) {                   /* desk apply wrote flat_state, or amarchy-flat DOPUS */
            if (!r->st.held)
                dim_release(r);                         /* the dim pen is a shared pen: let go first so it cannot sit on an icon pen index */
            pens_sync(r, 0);
            if (!r->st.held) {
                STG(FLAT_STG_SCAN);
                dopus_scan(r);
                dim_refresh(r);
                STG(FLAT_STG_STATUS);
                print_status();
            } else
                draw_sync(r);
        }
        if (!(got & SIGBREAKF_CTRL_C))
            continue;
        STG(FLAT_STG_UNPATCH);
        dopus_unpatch(r);
        if (d4_installed(r)) {
            refused(r, FLAT_REF_DOPUS);
            PutStr((STRPTR)"amarchy-flat: a patch is still chained on a DOpus entry, not unloading\n");
            continue;
        }
        STG(FLAT_STG_TX_REMOVE);
        if (tx_remove(r) == FLAT_STILL_CHAINED) {
            refused(r, FLAT_REF_TEXT);
            PutStr((STRPTR)"amarchy-flat: a patch is still chained on Text, not unloading\n");
            continue;
        }
        STG(FLAT_STG_RF_REMOVE);
        if (rf_remove(r) == FLAT_STILL_CHAINED) {
            refused(r, FLAT_REF_RECTFILL);
            PutStr((STRPTR)"amarchy-flat: a patch is still chained on RectFill, not unloading\n");
            continue;
        }
        STG(FLAT_STG_HOOK_REMOVE);
        if (r->sw.installed && hook_remove(r) == FLAT_STILL_CHAINED) {
            refused(r, FLAT_REF_HOOK);
            PutStr((STRPTR)"amarchy-flat: a hook is still chained on top of ours, not unloading\n");
            continue;
        }
        STG(FLAT_STG_FREE);
        ObtainSemaphore(&cl_ss);
        clone_free();
        clone_stack_free();                             /* hooks are removed and cl_ss is ours: no task is on the stack */
        ReleaseSemaphore(&cl_ss);
        ObtainSemaphore(&icl_ss);                       /* RL21: Text is unchained, no label branch can be inside */
        icl_free();
        ReleaseSemaphore(&icl_ss);
        dim_release(r);
        pt_stop(&pen_tm);
        pens_sync(r, 1);                                /* unloading: give the icon pens back */
        if (IntuitionBase) {
            CloseLibrary((struct Library *)IntuitionBase);
            IntuitionBase = 0;
        }
        if (GfxBase) {
            CloseLibrary((struct Library *)GfxBase);
            GfxBase = 0;
        }
        if (drop_record(r)) {
            if (marker_live)
                marker_delete();
            pt_close(&tm);
            pt_close(&pen_tm);
            stk_probe();
            return 0;
        }
        refused(r, FLAT_REF_RECORD);
        PutStr((STRPTR)"amarchy-flat: the state block is in use, not unloading\n");
    }
}
