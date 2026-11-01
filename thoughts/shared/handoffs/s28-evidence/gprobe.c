/* gprobe -- scratch probe (not committed): FLAT-BUTTON-FILL route. Opens a window with GadTools gadgets of every kind
   plus DrawBevelBox, and logs (a) every frameiclass IM_DRAW / IM_DRAWFRAME call (h_SubEntry wrap, both conventions),
   (b) every graphics Text() and RectFill() on the window's layer, in call order. Positions go to RAM:gprobe.pos
   right after the open; the log is printed when the run ends (RUNSECS seconds). Build: vc ... -lgadtools via amiga.lib. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
#include <intuition/screens.h>
#include <graphics/rastport.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <stdio.h>
#include <string.h>

static ULONG __stack = 32768;
static const char stackcookie[] = "$STACK:32768";
#define RUNSECS 40

struct IntuitionBase *IntuitionBase; struct GfxBase *GfxBase; struct Library *GadToolsBase;
struct ev { UBYTE t; UBYTE mine; WORD a, b, c, d, e, f, g, h; ULONG o; char s[12]; };
#define NEV 1500
static struct ev evs[NEV];
static volatile int nev, armed;
static struct Window *win; static ULONG *slot;

static void rec(UBYTE t, WORD a, WORD b, WORD c, WORD d, WORD e, WORD f, WORD g, WORD h, ULONG o, const char *s, int n)
{
    if (nev < NEV) { struct ev *x = &evs[nev++]; int i;
        x->t = t; x->a = a; x->b = b; x->c = c; x->d = d; x->e = e; x->f = f; x->g = g; x->h = h; x->o = o;
        for (i = 0; i < 11; i++) x->s[i] = (s && i < n && s[i] >= 32) ? s[i] : (s && i < n ? '?' : 0);
        x->s[11] = 0; }
}
static int mine(struct RastPort *rp) { return armed && rp && rp->Layer && win && rp->Layer == win->WLayer; }

/* frameiclass wrap */
static ULONG (*saved)(void);
static void logframe(Object *obj, Msg msg)
{
    struct impDraw *d = (struct impDraw *)msg;
    struct Image *im = (struct Image *)obj;
    ULONG eo = 99, ft = 99, rc = 99;
    LONG ge, gf, gr;
    if (!armed || (msg->MethodID != IM_DRAW && msg->MethodID != IM_DRAWFRAME)) return;
    ge = GetAttr(IA_EdgesOnly, obj, &eo); gf = GetAttr(IA_FrameType, obj, &ft); gr = GetAttr(IA_Recessed, obj, &rc);
    rec(msg->MethodID == IM_DRAW ? 'F' : 'W', (WORD)d->imp_State, d->imp_Offset.X + im->LeftEdge, d->imp_Offset.Y + im->TopEdge,
        im->Width, im->Height, (WORD)(ge ? (WORD)eo : -1), (WORD)(gf ? (WORD)ft : -1),
        (WORD)(gr ? (WORD)rc : -1), (ULONG)obj, 0, 0);
    evs[nev - 1].mine = d->imp_RPort && d->imp_RPort->Layer == win->WLayer;
    /* imp_RPort FgPen */
    evs[nev - 1].s[0] = d->imp_RPort ? d->imp_RPort->FgPen : 99;
    evs[nev - 1].s[1] = 1;
}
typedef ULONG (*fn_reg)(__reg("a0") struct Hook *, __reg("a2") Object *, __reg("a1") Msg);
typedef ULONG (*fn_stk)(struct Hook *, Object *, Msg);
ULONG w_reg(__reg("a0") struct Hook *h, __reg("a2") Object *o, __reg("a1") Msg m) { logframe(o, m); return ((fn_reg)saved)(h, o, m); }
ULONG w_stk(struct Hook *h, Object *o, Msg m) { logframe(o, m); return ((fn_stk)saved)(h, o, m); }

/* graphics hooks */
typedef void (*tx_t)(__reg("a1") struct RastPort *, __reg("a0") CONST_STRPTR, __reg("d0") ULONG, __reg("a6") struct GfxBase *);
typedef void (*rf_t)(__reg("a1") struct RastPort *, __reg("d0") LONG, __reg("d1") LONG, __reg("d2") LONG, __reg("d3") LONG, __reg("a6") struct GfxBase *);
static tx_t o_tx; static rf_t o_rf;
void s_tx(__reg("a1") struct RastPort *rp, __reg("a0") CONST_STRPTR s, __reg("d0") ULONG n, __reg("a6") struct GfxBase *g)
{ if (mine(rp)) { rec('T', rp->FgPen, rp->BgPen, rp->DrawMode, n, rp->cp_x, rp->cp_y, 0, 0, 0, (const char *)s, n); }
  o_tx(rp, s, n, g); }
void s_rf(__reg("a1") struct RastPort *rp, __reg("d0") LONG x0, __reg("d1") LONG y0, __reg("d2") LONG x1, __reg("d3") LONG y1, __reg("a6") struct GfxBase *g)
{ if (mine(rp)) rec('R', x0, y0, x1, y1, rp->FgPen, 0, 0, 0, 0, 0, 0); o_rf(rp, x0, y0, x1, y1, g); }

static STRPTR cyc[] = { (STRPTR)"One", (STRPTR)"Two", NULL };
static STRPTR mxl[] = { (STRPTR)"Aaa", (STRPTR)"Bbb", NULL };
static struct List lst; static struct Node n1, n2, n3;

int main(void)
{
    struct Screen *scr; APTR vi; struct Gadget *gl = NULL, *g, *gad[8]; struct NewGadget ng; int i, t;
    UBYTE *name[8]; FILE *f; struct IntuiMessage *m; ULONG **dummy; struct Image *probe; struct IClass *cl;
    static const char *kn[] = { "BUTTON1", "BUTTON2", "STRING", "CHECKBOX", "CYCLE", "MX", "LISTVIEW", "SLIDER" };
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 36);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 36);
    GadToolsBase = OpenLibrary("gadtools.library", 37);
    if (!IntuitionBase || !GfxBase || !GadToolsBase) return 20;
    scr = LockPubScreen(NULL); vi = GetVisualInfoA(scr, NULL);
    lst.lh_Head = (struct Node *)&lst.lh_Tail; lst.lh_Tail = NULL; lst.lh_TailPred = (struct Node *)&lst.lh_Head;
    n1.ln_Name = "Item one"; n2.ln_Name = "Item two"; n3.ln_Name = "Item three";
    AddTail(&lst, &n1); AddTail(&lst, &n2); AddTail(&lst, &n3);
    g = CreateContext(&gl);
    memset(&ng, 0, sizeof ng); ng.ng_VisualInfo = vi; ng.ng_TextAttr = scr->Font;
    ng.ng_LeftEdge = 10; ng.ng_TopEdge = 20; ng.ng_Width = 100; ng.ng_Height = 16; ng.ng_GadgetText = (UBYTE *)"Button"; ng.ng_GadgetID = 1;
    gad[0] = g = CreateGadget(BUTTON_KIND, g, &ng, TAG_END);
    ng.ng_TopEdge = 44; ng.ng_GadgetText = (UBYTE *)"Press"; ng.ng_GadgetID = 2;
    gad[1] = g = CreateGadget(BUTTON_KIND, g, &ng, TAG_END);
    ng.ng_LeftEdge = 130; ng.ng_TopEdge = 20; ng.ng_Width = 120; ng.ng_GadgetText = NULL; ng.ng_GadgetID = 3;
    gad[2] = g = CreateGadget(STRING_KIND, g, &ng, GTST_String, "text", GTST_MaxChars, 20, TAG_END);
    ng.ng_TopEdge = 44; ng.ng_Width = 26; ng.ng_GadgetID = 4; ng.ng_GadgetText = (UBYTE *)"Chk";
    gad[3] = g = CreateGadget(CHECKBOX_KIND, g, &ng, TAG_END);
    ng.ng_LeftEdge = 260; ng.ng_TopEdge = 20; ng.ng_Width = 100; ng.ng_GadgetID = 5; ng.ng_GadgetText = NULL;
    gad[4] = g = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, cyc, TAG_END);
    ng.ng_TopEdge = 44; ng.ng_Width = 17; ng.ng_Height = 9; ng.ng_GadgetID = 6;
    gad[5] = g = CreateGadget(MX_KIND, g, &ng, GTMX_Labels, mxl, TAG_END);
    ng.ng_LeftEdge = 10; ng.ng_TopEdge = 80; ng.ng_Width = 140; ng.ng_Height = 50; ng.ng_GadgetID = 7;
    gad[6] = g = CreateGadget(LISTVIEW_KIND, g, &ng, GTLV_Labels, &lst, TAG_END);
    ng.ng_LeftEdge = 260; ng.ng_TopEdge = 80; ng.ng_Width = 100; ng.ng_Height = 14; ng.ng_GadgetID = 8;
    gad[7] = g = CreateGadget(SLIDER_KIND, g, &ng, GTSL_Min, 0, GTSL_Max, 10, TAG_END);
    for (i = 0; i < 8; i++) if (!gad[i]) { printf("gadget %d (%s) failed\n", i, kn[i]); return 20; }
    win = OpenWindowTags(NULL, WA_Left, 20, WA_Top, 20, WA_Width, 400, WA_Height, 190, WA_Title, (ULONG)"gprobe",
        WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE, WA_SmartRefresh, TRUE,
        WA_Gadgets, (ULONG)gl, WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_GADGETDOWN | IDCMP_GADGETUP | IDCMP_REFRESHWINDOW | IDCMP_MOUSEBUTTONS | BUTTONIDCMP | LISTVIEWIDCMP | CYCLEIDCMP | MXIDCMP | SLIDERIDCMP | STRINGIDCMP | CHECKBOXIDCMP,
        TAG_END);
    UnlockPubScreen(NULL, scr);
    if (!win) { printf("open failed\n"); return 20; }
    GT_RefreshWindow(win, NULL);
    f = fopen("RAM:gprobe.pos", "w");
    if (f) {
        fprintf(f, "WIN %d %d %d %d bt=%d bl=%d V%d\n", win->LeftEdge, win->TopEdge, win->Width, win->Height, win->BorderTop, win->BorderLeft, (int)IntuitionBase->LibNode.lib_Version);
        for (i = 0; i < 8; i++)
            fprintf(f, "GAD %s id=%d addr=%08lx box=%d,%d,%d,%d render=%08lx select=%08lx flags=%04x text=%08lx\n", kn[i], (int)gad[i]->GadgetID,
                    (unsigned long)gad[i], gad[i]->LeftEdge, gad[i]->TopEdge, gad[i]->Width, gad[i]->Height,
                    (unsigned long)gad[i]->GadgetRender, (unsigned long)gad[i]->SelectRender, (unsigned)gad[i]->Flags, (unsigned long)gad[i]->GadgetText);
        fclose(f);
    }
    /* find frameiclass slot */
    {   struct TagItem tg[3]; struct DrawInfo *di; Object *o;
        scr = LockPubScreen(NULL); di = GetScreenDrawInfo(scr);
        tg[0].ti_Tag = IA_FrameType; tg[0].ti_Data = FRAME_BUTTON; tg[1].ti_Tag = TAG_DONE;
        o = NewObjectA(NULL, (UBYTE *)"frameiclass", tg);
        if (o) { cl = OCLASS(o); slot = (ULONG *)&cl->cl_Dispatcher.h_SubEntry; DisposeObject(o); }
        FreeScreenDrawInfo(scr, di); UnlockPubScreen(NULL, scr);
        if (!slot) { printf("no frameiclass\n"); return 20; }
        Forbid();
        saved = (ULONG (*)(void))*slot;
        *slot = (ULONG)(IntuitionBase->LibNode.lib_Version < 44 ? (void *)w_stk : (void *)w_reg);
        o_tx = (tx_t)SetFunction((struct Library *)GfxBase, -60, (APTR)s_tx);
        o_rf = (rf_t)SetFunction((struct Library *)GfxBase, -306, (APTR)s_rf);
        Permit();
        /* phase 1: a forced redraw of all gadgets + a bevel box with the window's own rp */
        armed = 1;
        rec('M', 1, 0, 0, 0, 0, 0, 0, 0, 0, "REFRESH", 7);
        RefreshGList(gl, win, NULL, -1);
        GT_RefreshWindow(win, NULL);
        {   struct TagItem bt[2]; bt[0].ti_Tag = GT_VisualInfo; bt[0].ti_Data = (ULONG)vi; bt[1].ti_Tag = TAG_DONE;
            rec('M', 2, 0, 0, 0, 0, 0, 0, 0, 0, "BEVEL-UP", 8);
            DrawBevelBoxA(win->RPort, 200, 140, 80, 30, bt);
            rec('M', 3, 0, 0, 0, 0, 0, 0, 0, 0, "BEVEL-REC", 9);
            bt[0].ti_Tag = GTBB_Recessed; bt[0].ti_Data = TRUE; bt[1].ti_Tag = GT_VisualInfo; bt[1].ti_Data = (ULONG)vi;
            { struct TagItem b3[3]; b3[0] = bt[0]; b3[1] = bt[1]; b3[2].ti_Tag = TAG_DONE; DrawBevelBoxA(win->RPort, 300, 140, 80, 30, b3); }
            rec('M', 4, 0, 0, 0, 0, 0, 0, 0, 0, "END-STATIC", 10);
        }
    }
    for (t = 0; t < RUNSECS * 10; t++) {
        while ((m = GT_GetIMsg(win->UserPort))) {
            ULONG cls = m->Class; UWORD code = m->Code; struct Gadget *ag = (struct Gadget *)m->IAddress;
            if (cls == IDCMP_GADGETDOWN || cls == IDCMP_GADGETUP)
                rec('M', 10 + (cls == IDCMP_GADGETUP), ag->GadgetID, 0, 0, 0, 0, 0, 0, 0, cls == IDCMP_GADGETDOWN ? "GADGETDOWN" : "GADGETUP", 10);
            else if (cls == IDCMP_MOUSEBUTTONS)
                rec('M', 20, code, 0, 0, 0, 0, 0, 0, 0, "MOUSEBTN", 8);
            GT_ReplyIMsg(m);
            if (cls == IDCMP_REFRESHWINDOW) { GT_BeginRefresh(win); GT_EndRefresh(win, TRUE); }
            if (cls == IDCMP_CLOSEWINDOW) t = RUNSECS * 10;
        }
        Delay(5);
    }
    armed = 0;
    Forbid();
    *slot = (ULONG)saved;
    SetFunction((struct Library *)GfxBase, -60, (APTR)o_tx);
    SetFunction((struct Library *)GfxBase, -306, (APTR)o_rf);
    Permit();
    printf("intuition V%d nev %d\n", (int)IntuitionBase->LibNode.lib_Version, nev);
    for (i = 0; i < nev; i++) {
        struct ev *e = &evs[i];
        switch (e->t) {
        case 'F': case 'W':
            printf("%c obj=%08lx state=%d box=%d,%d %dx%d eo=%d ft=%d rec=%d mine=%d fg=%d\n", e->t, e->o, e->a, e->b, e->c, e->d, e->e, e->f, e->g, e->h, e->mine, e->s[0]); break;
        case 'T': printf("T fg=%d bg=%d drmd=%d n=%d xy=%d,%d \"%s\"\n", e->a, e->b, e->c, e->d, e->e, e->f, e->s); break;
        case 'R': printf("R %d,%d %d,%d fg=%d\n", e->a, e->b, e->c, e->d, e->e); break;
        default: printf("M %s a=%d b=%d\n", e->s, e->a, e->b); break;
        }
    }
    CloseWindow(win); FreeGadgets(gl); FreeVisualInfo(vi);
    return 0;
}
