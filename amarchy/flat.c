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
 * same flat_swap model for a routine that draws nothing while flat is on and the pens are flat (no shadow L), and
 * calls the saved routine otherwise. Any other build is inert (STATUS "dopus=<v.r> inert=..."). Not reachable by an
 * LVO, so not patched: path field (button_class.c:900 calls draw_field_box inside the library), scroller knob and
 * arrows (dopusiclass IM_BBOX / arrows, image_class.c:261, :109), sort arrow (lister_title.c:972-987, inline in the
 * DirectoryOpus program). `amarchy-flat DOPUS` makes the running program look for DOpus again (Ctrl-D signal).
 *
 * FLAT-C4 kill switches (OD-F5, OD-F7; flat_core.h): the program starts INERT (no hook, no DOpus patch, STATUS
 * flat=held why=...) when ENVARC:Amarchy/flat-starting exists (written before the swap, deleted 60 s later by a
 * timer; a start that crashed the machine leaves it, the next start consumes it and stays inert), when both mouse
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
#include <intuition/screens.h>
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
#include "taskalive.h"
#include "poll_timer.h"
#include "amconf_core.h"
#include "amconf_dos.h"
#include "desk_core.h"
#include "iconpen_core.h"
#include "gthemes_core.h"
#include <graphics/view.h>

static const char ver[] = "$VER: amarchy-flat 0.1 (30.10.2026) (UP-Term)";

/* RULES.md rule 6: the stack the program asks for, and the cookie the Shell and Workbench read. */
long __stack = 16384;
const char amarchy_flat_stack_cookie[] = "$STACK: 16384";



struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
static flatshared *g_r;

/* ---- FLAT-C2: the sysiclass dispatcher. It replaces the C1 pass-through: count the call, and for IM_DRAW of a
 * SYSIA_Which that flat_glyph.c knows, draw the flat glyph when the switch is on; every other call (other method,
 * other Which, switch off, held, box too small, OM_GET failing) goes to the saved routine with the arguments as
 * received. 3.2 passes hook / object / message in a0 / a2 / a1, 3.1 on the C stack (flat_conv), so there are two
 * entries over one body. The image is the object itself (struct Image: Width, Height); Which is read with GetAttr, never
 * from the instance. GetAttr(OM_GET) comes back through this dispatcher with a non-draw method: no recursion. */
typedef ULONG (*oldfn_reg)(__reg("a0") struct Hook *, __reg("a2") Object *, __reg("a1") Msg);
typedef ULONG (*oldfn_stk)(struct Hook *, Object *, Msg);

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

/* FLAT-C7: a layer without a window is a screen's title bar: find the screen whose BarLayer it is and look through
 * Screen->FirstGadget (the depth gadget, type 0xc055) for the gadget that renders this image. */
static int c7_screen_which(struct Layer *lay, Object *obj)
{
    ULONG lock = LockIBase(0);
    struct Screen *sc;
    struct Gadget *g;
    int n, wh = -1;
    for (sc = IntuitionBase->FirstScreen; sc && wh < 0; sc = sc->NextScreen)
        if (sc->BarLayer == lay)
            for (g = sc->FirstGadget, n = 0; g && n < 64 && wh < 0; g = g->NextGadget, n++)
                wh = flat_gadget_which(g->GadgetType, g->GadgetRender, g->SelectRender, obj);
    UnlockIBase(lock);
    return wh;
}

static int c2_draw(Object *obj, Msg msg)
{
    flatshared *r = g_r;
    struct impDraw *d = (struct impDraw *)msg;
    struct Image *im = (struct Image *)obj;
    ULONG which = 0xFFFF;
    int frame_pen = -1;
    r->st.calls++;
    if (msg->MethodID != FLAT_IM_DRAW || !r->st.enabled || r->st.held || !d->imp_RPort)
        return 0;
    /* FLAT-OUTER-EDGE: the window's 1 px outer edge in the accent pen is NOT drawn here. Intuition repaints the four outer
     * border edges last in every frame redraw, as four 1 px RectFill calls through graphics.library, which erases anything
     * drawn from this hook. The ring is painted by the RectFill patch (flat_rf_c, below), which recolours exactly those fills. */
    if (GetAttr(SYSIA_Which, obj, &which) == 0) {
        /* Kickstart 3.1 sysiclass does not answer OM_GET: find the window gadget that renders this image. The layer
         * of the rastport belongs to the window being drawn; reading its gadget list from Intuition's own drawing
         * context needs no Forbid (the list only changes through Add/RemoveGList by the window's owner). */
        struct Layer *lay = d->imp_RPort->Layer;
        struct Window *w = lay ? (struct Window *)lay->Window : NULL;
        struct Gadget *g;
        int n = 0, wh = -1;
        for (g = w ? w->FirstGadget : NULL; g && n < 64 && wh < 0; g = g->NextGadget, n++)
            wh = flat_gadget_which(g->GadgetType, g->GadgetRender, g->SelectRender, obj);
        if (wh < 0 && lay && !w)
            wh = c7_screen_which(lay, obj);
        if (wh < 0)
            return 0;
        which = (ULONG)wh;
    }
    which = flat_which_alias(which);
    /* FLAT-GADGET-BG: the pen of the frame the gadget sits on, so the box shows only the glyph lines. A window gadget
     * sits on the border (the background pen in both states: FLAT-SOLID-FILL); a screen bar gadget on
     * the bar block (V39+ DrawInfo has it). */
    if (d->imp_DrInfo) {
        struct Layer *fl = d->imp_RPort->Layer;
        if (fl && fl->Window) {
            frame_pen = d->imp_DrInfo->dri_Pens[BACKGROUNDPEN];     /* FLAT-SOLID-FILL: both states, the frame is the background */
        } else if (fl && d->imp_DrInfo->dri_Version >= 2)
            frame_pen = d->imp_DrInfo->dri_Pens[BARBLOCKPEN];
    }
    return flat_glyph_draw(&r->st, which, msg->MethodID, d->imp_State, im->LeftEdge + d->imp_Offset.X,
                           im->TopEdge + d->imp_Offset.Y, im->Width, im->Height, frame_pen, c2_fill, d->imp_RPort);
}

ULONG c2_disp_reg(__reg("a0") struct Hook *h, __reg("a2") Object *obj, __reg("a1") Msg msg)
{
    if (c2_draw(obj, msg))
        return TRUE;
    return ((oldfn_reg)g_r->sw.saved)(h, obj, msg);
}

ULONG c2_disp_stk(struct Hook *h, Object *obj, Msg msg)
{
    if (c2_draw(obj, msg))
        return TRUE;
    return ((oldfn_stk)g_r->sw.saved)(h, obj, msg);
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
        sib[n].x = g->LeftEdge; sib[n].y = g->TopEdge; sib[n].w = g->Width; sib[n].h = g->Height;
        if (g->GadgetRender == (APTR)obj) { self = sib[n]; found = 1; }
        n++;
    }
    return found ? flat_button_role(&self, sib, n) : FLAT_ROLE_PRIMARY;
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
    {
        struct Image *im = (struct Image *)obj;
        ULONG eo = 1;
        /* Fill only when the frame says it is not edges-only (an unanswered OM_GET counts as edges-only: never erase content). */
        int fill;
        if (GetAttr(IA_EdgesOnly, obj, &eo) != 0)
            fill = eo == 0;
        else {      /* 3.1 frameiclass V40 does not answer OM_GET: a button or cycle gadget renders this image as render and select */
            struct Layer *lay = d->imp_RPort->Layer;
            struct Window *win = lay ? (struct Window *)lay->Window : NULL;
            struct Gadget *g;
            int n;
            fill = 0;
            for (g = win ? win->FirstGadget : NULL, n = 0; g && n < 64 && !fill; g = g->NextGadget, n++)
                fill = flat_button_gadget(g->GadgetRender, g->SelectRender, obj);
        }
        flat_state st = r->st;
        int ox = im->LeftEdge + d->imp_Offset.X, oy = im->TopEdge + d->imp_Offset.Y;
        int w, h;
        flat_frame_size(msg->MethodID == FLAT_IM_DRAWFRAME, d->imp_Dimensions.Width, d->imp_Dimensions.Height, im->Width, im->Height, &w, &h);
        /* FLAT-BUTTON-FILL: the fill pen must exist on this screen (a 4 colour screen has no pen 4) */
        st.pen_button = (UBYTE)flat_button_pen(&r->st, d->imp_RPort->BitMap ? d->imp_RPort->BitMap->Depth : 0);
        if (!flat_frame_draw_round(&st, (int)d->imp_State, ox, oy, w, h, fill, role_of(d->imp_RPort, obj), c2_fill, c2_read, d->imp_RPort))
            return 0;
        flat_gh_frame(&r->gh, flat_tx_surface(d->imp_RPort->Layer, d->imp_RPort->BitMap), FindTask(NULL), d->imp_State == 1, fill,
                      fill && role_of(d->imp_RPort, obj) == FLAT_ROLE_SECONDARY, ox, oy, ox + w - 1, oy + h - 1);
        /* a pressed filled frame arms the Text patch for its label; any other frame draw on this rastport disarms */
        flat_tx_frame(&r->tx, flat_tx_surface(d->imp_RPort->Layer, d->imp_RPort->BitMap), FindTask(NULL),
                      d->imp_State == 1, fill, ox, oy, ox + w - 1, oy + h - 1);
    }
    r->fflat++;
    return 1;
}

ULONG c8_disp_reg(__reg("a0") struct Hook *h, __reg("a2") Object *obj, __reg("a1") Msg msg)
{
    if (c8_skip(obj, msg))
        return TRUE;
    return ((oldfn_reg)g_r->fsw.saved)(h, obj, msg);
}

ULONG c8_disp_stk(struct Hook *h, Object *obj, Msg msg)
{
    if (c8_skip(obj, msg))
        return TRUE;
    return ((oldfn_stk)g_r->fsw.saved)(h, obj, msg);
}

/* ---- FLAT-D4: the replacement routines. They run on the caller's task with the caller's registers (a0 rp, a1 rect,
 * a2 info, d0 recessed). Flat on and flat pens: draw nothing. Otherwise the saved routine, same arguments. */
typedef void (*boxfn)(__reg("a0") struct RastPort *, __reg("a1") struct Rectangle *, __reg("a2") struct DrawInfo *,
                      __reg("d0") LONG, __reg("a6") struct Library *);
typedef void (*fieldfn)(__reg("a0") struct RastPort *, __reg("a1") struct Rectangle *, __reg("a2") struct DrawInfo *,
                        __reg("a6") struct Library *);

static int d4_flat(int el, struct DrawInfo *info, LONG recessed)
{
    flatshared *r = g_r;
    r->dcalls++;
    if (flat_dopus_decide(&r->st, el, recessed ? 1 : 0, info->dri_Pens[SHINEPEN], info->dri_Pens[BACKGROUNDPEN]) != FLAT_DRAW_SKIP)
        return 0;
    r->dflat++;
    return 1;
}

static void d4_drawbox(__reg("a0") struct RastPort *rp, __reg("a1") struct Rectangle *rect, __reg("a2") struct DrawInfo *info,
                       __reg("d0") LONG recessed)
{
    if (!d4_flat(FLAT_EL_DRAWBOX, info, recessed))
        ((boxfn)g_r->dsw[FLAT_EL_DRAWBOX].saved)(rp, rect, info, recessed, g_r->dlib);
}

static void d4_fieldbox(__reg("a0") struct RastPort *rp, __reg("a1") struct Rectangle *rect, __reg("a2") struct DrawInfo *info)
{
    if (!d4_flat(FLAT_EL_FIELDBOX, info, 0))
        ((fieldfn)g_r->dsw[FLAT_EL_FIELDBOX].saved)(rp, rect, info, g_r->dlib);
}

static const LONG d4_lvo[FLAT_EL_COUNT] = { FLAT_LVO_DRAWBOX, FLAT_LVO_FIELDBOX };

/* ---- FLAT-BUTTON-FILL: graphics.library Text (LVO -60, Text(rp a1, string a0, count d0)). The frame is drawn first, then the
 * label in pen 1 with JAM1, also when pressed, so the dark pressed label needs this patch: while c8_skip has armed
 * {surface (layer, else bitmap: the label comes through another RastPort than the frame), task, box} for a pressed filled frame, a Text call on that rastport from that task with the pen position
 * inside the box runs with pen_ptext (flat_tx_text decides; the stub is flat_tx.s). Everything else: stock Text. ---- */
/* Every saved library routine (Text, RectFill, the dopus5 ones) is called with its library base in a6, as through its
 * LVO: the ROM code reads its base from a6, and these hooks use a6 as a scratch register and for FindTask (a6 = SysBase).
 * Without it, the stock Text ran with a6 = SysBase and jumped through a garbage base (s28 flat-ON hang). */
typedef void (*rectfn)(__reg("a1") struct RastPort *, __reg("d0") LONG, __reg("d1") LONG, __reg("d2") LONG, __reg("d3") LONG,
                       __reg("a6") struct Library *);
typedef void (*textfn)(__reg("a1") struct RastPort *, __reg("a0") CONST_STRPTR, __reg("d0") ULONG, __reg("a6") struct Library *);
extern void flat_tx_entry(void);

void flat_tx_c(struct RastPort *rp, CONST_STRPTR str, ULONG count)
{
    flatshared *r = g_r;
    r->tcalls++;
    int pen;
    flat_gh_text(&r->gh, flat_tx_surface(rp->Layer, rp->BitMap), FindTask(NULL), rp->cp_x, rp->cp_y, (const char *)str, (int)count);
    if (r->st.enabled && !r->st.held && r->rsw.installed && rp->Layer && rp->Layer->Window) {      /* FLAT-TITLE-ACCENT */
        struct Window *w = (struct Window *)rp->Layer->Window;
        flat_title ft;
        if (flat_title_decide(&r->st, rp->BitMap ? rp->BitMap->Depth : 0, (w->Flags & WFLG_WINDOWACTIVE) != 0, (const char *)w->Title,
                              w->Width, w->BorderTop, (const char *)str, (int)count, rp->cp_y - (rp->Font ? rp->Font->tf_Baseline : 0), &ft)) {
            UBYTE fg = rp->FgPen, bg = rp->BgPen, dm = rp->DrawMode;
            WORD cx = rp->cp_x, cy = rp->cp_y;
            r->ttitle++;
            if (ft.band_pen >= 0) {
                UWORD *pa = (UWORD *)rp->AreaPtrn;
                UBYTE pt = rp->AreaPtSz;
                rp->AreaPtrn = 0; rp->AreaPtSz = 0;
                SetDrMd(rp, JAM2);
                SetAPen(rp, (UBYTE)ft.band_pen);
                ((rectfn)r->rsw.saved)(rp, ft.bx0, ft.by0, ft.bx1, ft.by1, (struct Library *)GfxBase);
                rp->AreaPtrn = pa; rp->AreaPtSz = pt;
                SetDrMd(rp, dm);
                Move(rp, cx, cy);
            }
            if (ft.text_pen >= 0)
                SetAPen(rp, (UBYTE)ft.text_pen);
            if (ft.bg_pen >= 0)
                SetBPen(rp, (UBYTE)ft.bg_pen);
            ((textfn)r->tsw.saved)(rp, str, count, (struct Library *)GfxBase);
            SetAPen(rp, fg);
            SetBPen(rp, bg);
            return;
        }
    }
    if (r->tx.armed && (pen = flat_tx_pen(&r->st, &r->tx, flat_tx_surface(rp->Layer, rp->BitMap), FindTask(NULL),
                                           rp->cp_x, rp->cp_y)) >= 0) {
        UBYTE fg = rp->FgPen;
        r->thits++;
        SetAPen(rp, (UBYTE)pen);
        ((textfn)r->tsw.saved)(rp, str, count, (struct Library *)GfxBase);
        SetAPen(rp, fg);
        return;
    }
    ((textfn)r->tsw.saved)(rp, str, count, (struct Library *)GfxBase);
}

static int tx_install(flatshared *r)
{
    unsigned char *entry = (unsigned char *)GfxBase + FLAT_LVO_TEXT;
    int rc;
    if (!flat_jmp_target(entry))
        return -100;
    flat_swap_init(&r->tsw);
    g_r = r;
    Forbid();
    rc = flat_swap_install(&r->tsw, (uintptr_t *)(entry + 2), (uintptr_t)flat_tx_entry);
    if (rc == FLAT_OK) {
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
    Forbid();
    rc = flat_swap_remove(&r->tsw, r->tslot, &r->st);
    if (rc == FLAT_REMOVED) {
        libpatch_done((struct Library *)GfxBase);
    }
    Permit();
    if (rc == FLAT_REMOVED) {
        Delay(5);
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
extern void flat_rf_entry(void);

/* FLAT-SCROLLER: the pen for one Intuition fill inside a window scroller, or -1. Walks the window's prop gadgets (no
 * Intuition call: this runs inside graphics.library, GetScreenDrawInfo here could deadlock) and asks flat_scroller_pen. */
static int scroller_pen(flatshared *r, struct RastPort *rp, struct Window *w, LONG x0, LONG y0, LONG x1, LONG y1)
{
    struct Gadget *g;
    for (g = w->FirstGadget; g; g = g->NextGadget) {
        struct PropInfo *pi;
        int gx, gy, gw, gh, cx, cy, cw, ch, pen;
        if ((g->GadgetType & GTYP_GTYPEMASK) != GTYP_PROPGADGET || !(pi = (struct PropInfo *)g->SpecialInfo))
            continue;
        gx = g->LeftEdge + ((g->Flags & GFLG_RELRIGHT) ? w->Width - 1 : 0);
        gy = g->TopEdge + ((g->Flags & GFLG_RELBOTTOM) ? w->Height - 1 : 0);
        gw = g->Width + ((g->Flags & GFLG_RELWIDTH) ? w->Width : 0);
        gh = g->Height + ((g->Flags & GFLG_RELHEIGHT) ? w->Height : 0);
        if (x0 < gx || y0 < gy || x1 >= gx + gw || y1 >= gy + gh)
            continue;
        cx = gx + pi->LeftBorder; cy = gy + pi->TopBorder; cw = pi->CWidth; ch = pi->CHeight;
        pen = flat_scroller_pen(&r->st, (rp->BitMap ? rp->BitMap->Depth : 0), cx, cy, cw, ch, (pi->Flags & FREEHORIZ) != 0, (pi->Flags & FREEVERT) != 0,
                                pi->HorizPot, pi->VertPot, pi->HorizBody, pi->VertBody, (int)x0, (int)y0, (int)x1, (int)y1);
        if (pen >= 0)
            return pen;
    }
    return -1;
}

void flat_rf_c(struct RastPort *rp, LONG x0, LONG y0, LONG x1, LONG y1, ULONG ret)
{
    flatshared *r = g_r;
    struct Window *w;
    r->rcalls++;
    if (r->st.enabled && !r->st.held && rp && rp->Layer && (w = (struct Window *)rp->Layer->Window) != 0 && ret >= r->irom_lo &&
        ret < r->irom_hi) {
        int sp;
        const flat_gbox *gb = flat_gh_match(&r->st, &r->gh, flat_tx_surface(rp->Layer, rp->BitMap), FindTask(NULL), rp->AreaPtSz, (int)x0, (int)y0, (int)x1, (int)y1);
        if (gb) {          /* FLAT-BUTTON-DISABLED-DIM: Intuition's dither over a disabled button: solid dim fill, label again in the dim label pen */
            int depth = rp->BitMap ? rp->BitMap->Depth : 0;
            UBYTE fg = rp->FgPen, dm = rp->DrawMode, pt = rp->AreaPtSz;
            UWORD *pa = (UWORD *)rp->AreaPtrn;
            WORD cx = rp->cp_x, cy = rp->cp_y;
            r->ghits++;
            rp->AreaPtrn = 0; rp->AreaPtSz = 0;
            SetDrMd(rp, JAM2);
            SetAPen(rp, (UBYTE)flat_dim_pen(&r->st, depth));
            ((rectfn)r->rsw.saved)(rp, gb->x0, gb->y0, gb->x1, gb->y1, (struct Library *)GfxBase);
            if (gb->str) {
                SetDrMd(rp, JAM1);
                SetAPen(rp, (UBYTE)flat_dimtext_pen(&r->st, depth));
                Move(rp, gb->lx, gb->ly);
                ((textfn)r->tsw.saved)(rp, gb->str, gb->count, (struct Library *)GfxBase);
            }
            Move(rp, cx, cy);
            SetAPen(rp, fg);
            SetDrMd(rp, dm);
            rp->AreaPtrn = pa; rp->AreaPtSz = pt;
            return;
        }
        sp = scroller_pen(r, rp, w, x0, y0, x1, y1);
        if (sp >= 0) {
            UBYTE fg = rp->FgPen, dm = rp->DrawMode, pt = rp->AreaPtSz;
            UWORD *pa = (UWORD *)rp->AreaPtrn;
            r->shits++;
            rp->AreaPtrn = 0; rp->AreaPtSz = 0;               /* a solid fill: the stock track is a dither */
            SetDrMd(rp, JAM2);
            SetAPen(rp, sp);
            ((rectfn)r->rsw.saved)(rp, x0, y0, x1, y1, (struct Library *)GfxBase);
            SetAPen(rp, fg);
            SetDrMd(rp, dm);
            rp->AreaPtrn = pa; rp->AreaPtSz = pt;
            return;
        }
    }
    if (r->st.enabled && !r->st.held && r->st.pen_outline && rp && rp->Layer && (w = (struct Window *)rp->Layer->Window) != 0 &&
        !(w->Flags & WFLG_BORDERLESS) && ret >= r->irom_lo && ret < r->irom_hi &&
        flat_outer_edge_match(w->Width, w->Height, (int)x0, (int)y0, (int)x1, (int)y1)) {
        UBYTE fg = rp->FgPen;
        r->rhits++;
        SetAPen(rp, r->st.pen_outline);
        ((rectfn)r->rsw.saved)(rp, x0, y0, x1, y1, (struct Library *)GfxBase);
        SetAPen(rp, fg);
        return;
    }
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
    g_r = r;
    Forbid();
    rc = flat_swap_install(&r->rsw, (uintptr_t *)(entry + 2), (uintptr_t)flat_rf_entry);
    if (rc == FLAT_OK) {
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
    Forbid();
    rc = flat_swap_remove(&r->rsw, r->rslot, &r->st);
    if (rc == FLAT_REMOVED) {
        libpatch_done((struct Library *)GfxBase);
    }
    Permit();
    if (rc == FLAT_REMOVED) {
        Delay(5);                       /* a caller that already entered the stub finishes its fill */
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
static void print_status(void)
{
    flat_state copy;
    char line[96];
    flatshared *r;
    int found = 0, dfound = 0, dreason = 0, dmask = 0, dinst = 0;
    unsigned dver = 0, drev = 0;
    unsigned long dcalls = 0, dflat = 0, fcalls = 0, fflat = 0;
    int finst = 0, rinst = 0;
    unsigned long rcalls = 0, rhits = 0, shits = 0, tcalls = 0, thits = 0;
    int tinst = 0;
    ip_holder held;
    uint32_t dsum[FLAT_EL_COUNT] = { 0, 0 };
    memset(&copy, 0, sizeof copy);
    memset(&held, 0, sizeof held);
    Forbid();
    r = (flatshared *)FindSemaphore((STRPTR)FLAT_SEM_NAME);
    if (r) {
        found = 1;
        copy = r->st;
        dfound = r->dfound; dver = r->dver; drev = r->drev; dreason = r->dreason; dmask = r->dmask;
        dinst = r->dsw[0].installed || r->dsw[1].installed; dcalls = r->dcalls; dflat = r->dflat;
        dsum[0] = r->dsum[0]; dsum[1] = r->dsum[1];
        finst = r->fsw.installed; fcalls = r->fcalls; fflat = r->fflat;
        rinst = r->rsw.installed; rcalls = r->rcalls; rhits = r->rhits; shits = r->shits;
        tinst = r->tsw.installed; tcalls = r->tcalls; thits = r->thits;
        held = r->ih;
    }
    Permit();
    if (flat_status_line(&copy, sizeof copy, found, line, sizeof line)) {
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
    }
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
    if (found) {                                    /* FLAT-OUTER-EDGE sentinels: calls rising = the stub runs, hits rising = ring painted */
        int n = 0;
        put_word(line, &n, "rectfill installed=");  put_word(line, &n, rinst ? "yes" : "no");
        put_word(line, &n, " calls=");              put_num(line, &n, rcalls);
        put_word(line, &n, " hits=");               put_num(line, &n, rhits);
        put_word(line, &n, " shits=");              put_num(line, &n, shits);
        line[n] = 0;
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
    }
    if (found) {                                    /* FLAT-BUTTON-FILL sentinels: calls rising = the stub runs, hits rising = pressed label recoloured */
        int n = 0;
        put_word(line, &n, "text installed=");      put_word(line, &n, tinst ? "yes" : "no");
        put_word(line, &n, " calls=");              put_num(line, &n, tcalls);
        put_word(line, &n, " hits=");               put_num(line, &n, thits);
        line[n] = 0;
        PutStr((STRPTR)line);
        PutStr((STRPTR)"\n");
    }
    if (found) {                                    /* LUCIDE L7: the icon pens held on the Workbench screen */
        if (ip_status_line(&held, line, sizeof line)) {
            int n = (int)strlen(line);
            put_word(line, &n, " dim=");  put_num(line, &n, copy.pen_dim);
            line[n] = 0;
            PutStr((STRPTR)line);
            PutStr((STRPTR)"\n");
        }
    }
    if (found && flat_frame_status_line(finst, fcalls, fflat, line, sizeof line)) {
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
static uintptr_t *class_slot(int frame)
{
    struct TagItem t[4];
    struct Screen *scr = LockPubScreen(NULL);
    struct DrawInfo *di = scr ? GetScreenDrawInfo(scr) : NULL;
    uintptr_t *slot = 0;
    if (di) {
        Object *o;
        t[0].ti_Tag = SYSIA_DrawInfo; t[0].ti_Data = (ULONG)di;
        t[1].ti_Tag = frame ? IA_FrameType : SYSIA_Which;  t[1].ti_Data = frame ? FRAME_BUTTON : CLOSEIMAGE;
        t[2].ti_Tag = TAG_DONE;       t[2].ti_Data = 0;
        o = NewObjectA(NULL, (UBYTE *)(frame ? "frameiclass" : "sysiclass"), t);
        if (o) {
            struct IClass *cl = OCLASS(o);
            slot = (uintptr_t *)&cl->cl_Dispatcher.h_SubEntry;     /* the class outlives the object: it is a public class */
            DisposeObject(o);
        }
    }
    if (di)
        FreeScreenDrawInfo(scr, di);
    if (scr)
        UnlockPubScreen(NULL, scr);
    return slot;
}

static uintptr_t *sysiclass_slot(void) { return class_slot(0); }
static uintptr_t *frameiclass_slot(void) { return class_slot(1); }

/* Install the pass-through under Forbid. Returns FLAT_OK or a negative flat_swap code / -100 (no class) / -101 (memory). */
static int hook_install(flatshared *r)
{
    uintptr_t *slot = sysiclass_slot();
    uintptr_t ours = flat_conv(IntuitionBase->LibNode.lib_Version) == FLAT_CONV_REG ? (uintptr_t)c2_disp_reg : (uintptr_t)c2_disp_stk;
    int rc;
    if (!slot)
        return -100;
    g_r = r;                            /* the dispatcher reads it from its first call on */
    Forbid();
    rc = flat_swap_install(&r->sw, slot, ours);     /* r->sw.saved is the old routine before the first dispatcher call */
    Permit();
    if (rc != FLAT_OK)
        return rc;
    r->slot = slot;
    {   /* FLAT-C8: the second class. Its absence leaves the sysiclass hook working: report it, do not undo C2. */
        uintptr_t *fs = frameiclass_slot();
        uintptr_t fours = flat_conv(IntuitionBase->LibNode.lib_Version) == FLAT_CONV_REG ? (uintptr_t)c8_disp_reg : (uintptr_t)c8_disp_stk;
        if (!fs)
            return FLAT_OK;
        Forbid();
        rc = flat_swap_install(&r->fsw, fs, fours);
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
    if (r->fsw.installed) {             /* FLAT-C8: the frameiclass hook first; chained on top = keep both */
        Forbid();
        rc = flat_swap_remove(&r->fsw, r->fslot, &r->st);
        Permit();
        if (rc == FLAT_STILL_CHAINED)
            return rc;
        r->fslot = 0;
    }
    Forbid();
    rc = flat_swap_remove(&r->sw, r->slot, &r->st);
    Permit();
    if (rc == FLAT_REMOVED) {
        Delay(5);                       /* a caller that already entered the dispatcher finishes its draw */
        r->slot = 0;
    }
    return rc;
}

/* ---- FLAT-D4: find dopus5.library among the resident libraries (never load it), gate it, swap the two entries ---- */
static void dopus_unpatch(flatshared *r)
{
    int el;
    Forbid();
    for (el = 0; el < FLAT_EL_COUNT; el++)
        if (r->dsw[el].installed)
            flat_swap_remove(&r->dsw[el], r->dslot[el], &r->st);
    if (r->dlib)
        libpatch_done(r->dlib);
    CacheClearU();
    Permit();
    if (!r->dsw[0].installed && !r->dsw[1].installed && r->dlib) {
        Delay(2);                       /* a caller that already entered the replacement leaves it */
        CloseLibrary(r->dlib);
        r->dlib = 0;
        r->dmask = 0;
    }
}

static void dopus_scan(flatshared *r)
{
    struct ExecBase *eb = *(struct ExecBase **)4;
    struct Library *lib;
    uint32_t sums[FLAT_EL_COUNT];
    int n, el, mask;
    const flat_dopus_build *tab = flat_dopus_table(&n);
    if (r->dsw[0].installed || r->dsw[1].installed)
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
        uint32_t t = flat_jmp_target((const unsigned char *)lib + d4_lvo[el]);
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
        unsigned char *entry = (unsigned char *)lib + d4_lvo[el];
        uint32_t t = flat_jmp_target(entry);
        if (!(mask & (1 << el)) || !t || flat_sum_reloc((const void *)t, FLAT_FUNC_BYTES) != sums[el]) {
            mask &= ~(1 << el);         /* changed since the gate (a foreign patch landed): leave it alone */
            continue;
        }
        flat_swap_init(&r->dsw[el]);
        r->dslot[el] = (uintptr_t *)(entry + 2);
        if (flat_swap_install(&r->dsw[el], r->dslot[el], (uintptr_t)(el == FLAT_EL_DRAWBOX ? (void *)d4_drawbox : (void *)d4_fieldbox)) != FLAT_OK)
            mask &= ~(1 << el);
    }
    libpatch_done(lib);
    Permit();
    r->dmask = mask;
    if (mask)
        r->dlib = lib;
    else
        CloseLibrary(lib);
}

/* ---- LUCIDE L7: the pen holder. amarchy-flat holds the 16 icon pens (iconpen_core.c: ip_holder_*) on the Workbench screen
 * for its whole run, flat on or off (ip_hold_wanted: a released pen heads the free list and the next shared pen takes it). ObtainPen(PENF_EXCLUSIVE) takes the exact pen number or fails (-1); the decisions are iconpen_core.c
 * (ip_sync_decide, host-tested), here only the OS calls. Nothing in the OS tells us the Workbench screen closed and reopened
 * (no screennotify exists in amarchy; wbreset_patch.c signals only clients with windows), so pens_sync runs at start, on
 * Ctrl-D (desk apply signals it after writing flat_state) and on a 1 s tick that compares the screen pointer and depth. ---- */
typedef struct pen_ctx { struct ColorMap *cur, *old; } pen_ctx;   /* cur: obtain on; old: release on (0 = its screen is gone) */

static int pen_obtain(void *ctx, int pen, unsigned r, unsigned g, unsigned b)
{
    pen_ctx *c = (pen_ctx *)ctx;
    if (!c->cur)
        return -1;
    return (int)ObtainPen(c->cur, (ULONG)pen, ip_scale32(r), ip_scale32(g), ip_scale32(b), PENF_EXCLUSIVE);
}

static void pen_release(void *ctx, int pen)
{
    pen_ctx *c = (pen_ctx *)ctx;
    if (c->old)                         /* 0 = the screen the pens were obtained on is gone; they went with it */
        ReleasePen(c->old, (ULONG)pen);
}

static long pen_theme_read(void *ctx, const char *path, char *buf, long cap)
{
    BPTR f = Open((STRPTR)path, MODE_OLDFILE);
    LONG got;
    (void)ctx;
    if (!f)
        return -1;
    got = Read(f, buf, cap);
    Close(f);
    return got < 0 ? 0 : got;
}

/* The Workbench screen, found in the public screen list (the caller holds its lock). LockPubScreen("Workbench") is never used:
 * it opens a closed Workbench. 0 when the Workbench is closed. */
static struct Screen *wb_screen(struct List *pl)
{
    struct PubScreenNode *n;
    for (n = (struct PubScreenNode *)pl->lh_Head; n->psn_Node.ln_Succ; n = (struct PubScreenNode *)n->psn_Node.ln_Succ)
        if (n->psn_Node.ln_Name && !strcmp(n->psn_Node.ln_Name, "Workbench"))
            return n->psn_Screen;
    return 0;
}

/* Bring the held pens in line with: flat on and the Workbench screen as it is now. The public screen list stays locked for
 * the whole sync, so the screen cannot close under the colour map. */
static void pens_sync(flatshared *r, int quit)
{
    pen_ctx c;
    ip_ops o;
    struct List *pl;
    struct Screen *sc;
    int act, depth = 0;
    int hold = ip_hold_wanted(quit, r->st.held != 0, GfxBase && GfxBase->LibNode.lib_Version >= 39);   /* not the flat switch */
    o.obtain = pen_obtain;
    o.release = pen_release;
    o.ctx = &c;
    pl = LockPubScreenList();
    sc = wb_screen(pl);
    if (sc)
        depth = ip_screen_depth(sc->BitMap.Depth, (sc->ViewPort.Modes & EXTRA_HALFBRITE) != 0);
    c.cur = sc ? sc->ViewPort.ColorMap : 0;
    c.old = (sc && r->ih.screen == (const void *)sc) ? c.cur : 0;   /* old pens are released only on the screen they belong to */
    act = ip_sync_decide(&r->ih, hold, sc, depth);
    if (act == IP_SYNC_RELEASE) {
        ip_holder_release(&r->ih, &o);
    } else if (act == IP_SYNC_OBTAIN) {
        gtheme th;
        uint8_t rgb[IP_PENS][3];
        am_prefs ap;
        amconf_load(&ap);
        gt_load(pen_theme_read, 0, ap.val[AM_GTHEME][0] ? ap.val[AM_GTHEME] : GT_DEFAULT, &th);
        ip_colours(&th, rgb);
        ip_holder_obtain(&r->ih, &o, sc, depth, rgb);   /* releases the old ones first */
    }
    UnlockPubScreenList();
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
static int marker_exists(void)
{
    BPTR l = Lock((STRPTR)FLAT_MARKER_PATH, ACCESS_READ);
    if (l)
        UnLock(l);
    return l != 0;
}

/* Write the marker and make the handler flush it to the disk: the swap must not start before it is safe. */
static void marker_write(void)
{
    BPTR l = Lock((STRPTR)"ENVARC:Amarchy", ACCESS_READ), f;
    if (!l)
        l = CreateDir((STRPTR)"ENVARC:Amarchy");
    if (l)
        UnLock(l);
    f = Open((STRPTR)FLAT_MARKER_PATH, MODE_NEWFILE);
    if (f) {
        Write(f, (APTR)FLAT_MARKER_HEAD, (LONG)strlen(FLAT_MARKER_HEAD));
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
    am_prefs p;
    amconf_load(&p);
    return amconf_flat_index(&p, AM_FLAT) || amconf_flat_index(&p, AM_FLATGAD);
}

/* FLAT-BUTTON-DISABLED-DIM: the dim fill pen. The colour of the button fill pen on the Workbench screen scaled to DIM_PCT percent,
 * obtained as a shared pen (ObtainBestPen: an exact or nearest colour, no DrawInfo pen is recoloured) and held until the next refresh or
 * exit. Runs on the main task only: ObtainBestPen is never called inside the RectFill hook. Screens under 8 colours keep pen_dim 0
 * (flat_dim_pen then falls back to the plain button pen). */
#define DIM_PCT 60
static LONG g_dim_pen = -1;

static void dim_refresh(flatshared *r)
{
    struct Screen *scr = LockPubScreen(NULL);
    if (scr) {
        struct ColorMap *cm = scr->ViewPort.ColorMap;
        if (g_dim_pen >= 0) {
            ReleasePen(cm, (ULONG)g_dim_pen);
            g_dim_pen = -1;
            r->st.pen_dim = 0;
        }
        if (cm && scr->RastPort.BitMap && scr->RastPort.BitMap->Depth >= 3) {
            ULONG rgb[3];
            uc_u32 c;
            GetRGB32(cm, (ULONG)flat_button_pen(&r->st, scr->RastPort.BitMap->Depth), 1, rgb);
            c = gt_scale(((rgb[0] >> 24) << 16) | ((rgb[1] >> 24) << 8) | (rgb[2] >> 24), DIM_PCT);
            g_dim_pen = ObtainBestPen(cm, ((c >> 16) & 0xff) * 0x01010101UL, ((c >> 8) & 0xff) * 0x01010101UL, (c & 0xff) * 0x01010101UL,
                                      OBP_Precision, PRECISION_GUI, OBP_FailIfBad, FALSE, TAG_DONE);
            if (g_dim_pen >= 0)
                r->st.pen_dim = (UBYTE)g_dim_pen;
        }
        UnlockPubScreen(NULL, scr);
    }
}

static void dim_release(flatshared *r)
{
    struct Screen *scr = g_dim_pen >= 0 ? LockPubScreen(NULL) : NULL;
    r->st.pen_dim = 0;
    if (scr) {
        ReleasePen(scr->ViewPort.ColorMap, (ULONG)g_dim_pen);
        UnlockPubScreen(NULL, scr);
    }
    g_dim_pen = -1;
}

int main(void)
{
    const char *args = (const char *)GetArgStr();
    flatshared *r;
    poll_timer tm, pen_tm;
    int marker_live = 0;
    (void)ver;
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
        flat_init(&r->st);
        {   /* FLAT-C9: the switch and pens from the prefs (the one mapping desk apply uses too) */
            am_prefs bp;
            amconf_load(&bp);
            desk_flat_state(amconf_flat_index(&bp, AM_FLAT), amconf_flat_index(&bp, AM_FLATGAD), &r->st);
        }
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
    if (IntuitionBase && GfxBase) {
        int lmb, rmb, why;
        buttons(&lmb, &rmb);
        why = flat_start_decide(marker_exists(), lmb, rmb, other_patcher(sysiclass_slot()));
        if (why) {                                      /* a kill switch is set: stay resident, inert, say why */
            r->st.held = (uint16_t)why;
            if (why == FLAT_HELD_MARKER)
                marker_delete();                        /* one inert start per crash, not for ever */
        } else {
            int rc;
            marker_write();                             /* on disk before the swap, so a crash leaves it */
            marker_live = 1;
            rc = hook_install(r);
            if (rc != FLAT_OK) {
                PutStr((STRPTR)"amarchy-flat: sysiclass hook not installed\n");
                marker_delete();
                marker_live = 0;
            } else {
                if (rf_install(r) != FLAT_OK)
                    PutStr((STRPTR)"amarchy-flat: RectFill patch not installed\n");
                if (tx_install(r) != FLAT_OK)
                    PutStr((STRPTR)"amarchy-flat: Text patch not installed\n");
                pt_start(&tm, (unsigned long)FLAT_MARKER_SECS * 1000000UL);
            }
        }
    }
    g_r = r;
    ip_holder_init(&r->ih);
    pens_sync(r, 0);                                    /* the exclusive icon pens first: a shared pen (dim) must not sit on one */
    if (!r->st.held) {
        dopus_scan(r);
        dim_refresh(r);
    }
    pt_start(&pen_tm, 1000000UL);                       /* the Workbench screen watch (pens_sync) */
    print_status();
    for (;;) {
        ULONG got = Wait(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | pt_sigmask(&tm) | pt_sigmask(&pen_tm));
        if (pt_fired(&pen_tm, got)) {                   /* Workbench closed / reopened / mode change */
            pens_sync(r, 0);
            pt_start(&pen_tm, 1000000UL);
        }
        if (marker_live && pt_fired(&tm, got)) {        /* 60 s without a crash: the start is safe */
            marker_delete();
            marker_live = 0;
        }
        if (got & SIGBREAKF_CTRL_D) {                   /* desk apply wrote flat_state, or amarchy-flat DOPUS */
            if (!r->st.held)
                dim_release(r);                         /* the dim pen is a shared pen: let go first so it cannot sit on an icon pen index */
            pens_sync(r, 0);
            if (!r->st.held) {
                dopus_scan(r);
                dim_refresh(r);
                print_status();
            }
        }
        if (!(got & SIGBREAKF_CTRL_C))
            continue;
        dopus_unpatch(r);
        if (r->dsw[0].installed || r->dsw[1].installed) {
            PutStr((STRPTR)"amarchy-flat: a patch is still chained on a DOpus entry, not unloading\n");
            continue;
        }
        if (tx_remove(r) == FLAT_STILL_CHAINED) {
            PutStr((STRPTR)"amarchy-flat: a patch is still chained on Text, not unloading\n");
            continue;
        }
        if (rf_remove(r) == FLAT_STILL_CHAINED) {
            PutStr((STRPTR)"amarchy-flat: a patch is still chained on RectFill, not unloading\n");
            continue;
        }
        if (r->sw.installed && hook_remove(r) == FLAT_STILL_CHAINED) {
            PutStr((STRPTR)"amarchy-flat: a hook is still chained on top of ours, not unloading\n");
            continue;
        }
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
            return 0;
        }
        PutStr((STRPTR)"amarchy-flat: the state block is in use, not unloading\n");
    }
}
