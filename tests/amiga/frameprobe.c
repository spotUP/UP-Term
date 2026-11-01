/* frameprobe -- FLAT-C8 Q2: does frameiclass's dispatcher run for GadTools bevels? (plan 2026-10-29-amarchy-flat-ui-patch.md)
 *   frameprobe   swap frameiclass's dispatcher h_SubEntry (when set, else h_Entry) for a counting trampoline that tail-calls
 *                the saved routine, open a GadTools window (button, string, checkbox, cycle, listview, DrawBevelBox), refresh
 *                it and open a second window to deactivate it, then print COUNT lines (per MethodID) and the first calls
 *                (method, state, size), and put the slot back. 3.2 register convention only (a0 / a2 / a1). */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/imageclass.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;
long __stack = 16384;
const char frameprobe_stack_cookie[] = "$STACK: 16384";

static unsigned long g_saved;
static long g_total, g_draw, g_frame, g_other;
static long g_n;
static unsigned long g_log[12][4];
typedef ULONG (*oldfn_reg)(__reg("a0") struct Hook *, __reg("a2") Object *, __reg("a1") Msg);

ULONG count_disp(__reg("a0") struct Hook *h, __reg("a2") Object *obj, __reg("a1") Msg msg)
{
    ULONG m = msg->MethodID;
    g_total++;
    if (m == IM_DRAW) g_draw++; else if (m == IM_DRAWFRAME) g_frame++; else g_other++;
    if ((m == IM_DRAW || m == IM_DRAWFRAME) && g_n < 12) {
        struct Image *im = (struct Image *)obj;
        g_log[g_n][0] = m; g_log[g_n][1] = ((struct impDraw *)msg)->imp_State;
        g_log[g_n][2] = im->Width; g_log[g_n][3] = im->Height;
        g_n++;
    }
    return ((oldfn_reg)g_saved)(h, obj, msg);
}

static struct List lv;
static const char *cyc[] = { "One", "Two", NULL };

static struct Window *mkwin(struct Screen *scr, void *vi, int left, struct Gadget **gl)
{
    struct NewGadget ng;
    struct Gadget *g;
    struct TagItem t[10];
    int i = 0;
    *gl = NULL;
    g = CreateContext(gl);
    memset(&ng, 0, sizeof ng);
    ng.ng_VisualInfo = vi; ng.ng_TextAttr = scr->Font;
    ng.ng_LeftEdge = 8; ng.ng_TopEdge = 18; ng.ng_Width = 80; ng.ng_Height = 14; ng.ng_GadgetText = (UBYTE *)"Button"; ng.ng_GadgetID = 1;
    g = CreateGadgetA(BUTTON_KIND, g, &ng, NULL);
    ng.ng_TopEdge = 36; ng.ng_GadgetText = NULL; ng.ng_GadgetID = 2;
    g = CreateGadgetA(STRING_KIND, g, &ng, NULL);
    ng.ng_TopEdge = 54; ng.ng_Width = 26; ng.ng_GadgetID = 3;
    g = CreateGadgetA(CHECKBOX_KIND, g, &ng, NULL);
    { struct TagItem c[2]; c[0].ti_Tag = GTCY_Labels; c[0].ti_Data = (ULONG)cyc; c[1].ti_Tag = TAG_DONE; c[1].ti_Data = 0;
      ng.ng_TopEdge = 72; ng.ng_Width = 80; ng.ng_GadgetID = 4;
      g = CreateGadgetA(CYCLE_KIND, g, &ng, c); }
    { struct TagItem l[2]; l[0].ti_Tag = GTLV_Labels; l[0].ti_Data = (ULONG)&lv; l[1].ti_Tag = TAG_DONE; l[1].ti_Data = 0;
      ng.ng_LeftEdge = 100; ng.ng_TopEdge = 18; ng.ng_Width = 90; ng.ng_Height = 50; ng.ng_GadgetID = 5;
      g = CreateGadgetA(LISTVIEW_KIND, g, &ng, l); }
    t[i].ti_Tag = WA_Left; t[i++].ti_Data = left;
    t[i].ti_Tag = WA_Top; t[i++].ti_Data = 30;
    t[i].ti_Tag = WA_Width; t[i++].ti_Data = 220;
    t[i].ti_Tag = WA_Height; t[i++].ti_Data = 120;
    t[i].ti_Tag = WA_Title; t[i++].ti_Data = (ULONG)"frameprobe";
    t[i].ti_Tag = WA_Gadgets; t[i++].ti_Data = (ULONG)*gl;
    t[i].ti_Tag = WA_IDCMP; t[i++].ti_Data = IDCMP_CLOSEWINDOW | BUTTONIDCMP;
    t[i].ti_Tag = TAG_DONE; t[i].ti_Data = 0;
    return OpenWindowTagList(NULL, t);
}

int main(int argc, char **argv)
{
    struct Screen *scr;
    struct DrawInfo *di;
    void *vi;
    Object *o;
    struct IClass *cl;
    struct Window *w1, *w2;
    struct Gadget *gl1, *gl2;
    unsigned long *slot;
    struct TagItem t[3];
    int i;
    if (argc < 0 || (argv && argv[0] == NULL))
        return 5;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 36);
    GadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 36);
    if (!IntuitionBase || !GfxBase || !GadToolsBase)
        return 20;
    scr = LockPubScreen(NULL);
    di = scr ? GetScreenDrawInfo(scr) : NULL;
    vi = scr ? GetVisualInfoA(scr, NULL) : NULL;
    if (!di || !vi)
        return 10;
    lv.lh_Head = (struct Node *)&lv.lh_Tail; lv.lh_Tail = NULL; lv.lh_TailPred = (struct Node *)&lv.lh_Head;
    t[0].ti_Tag = SYSIA_DrawInfo; t[0].ti_Data = (ULONG)di;
    t[1].ti_Tag = IA_FrameType; t[1].ti_Data = FRAME_BUTTON;
    t[2].ti_Tag = TAG_DONE; t[2].ti_Data = 0;
    o = NewObjectA(NULL, (UBYTE *)"frameiclass", t);
    if (!o) { printf("NOFRAMECLASS\n"); return 10; }
    cl = OCLASS(o);
    DisposeObject(o);
    printf("INTUITION %ld.%ld frameiclass=%08lx entry=%08lx sub=%08lx\n", (long)IntuitionBase->LibNode.lib_Version,
           (long)IntuitionBase->LibNode.lib_Revision, (unsigned long)cl, (unsigned long)cl->cl_Dispatcher.h_Entry,
           (unsigned long)cl->cl_Dispatcher.h_SubEntry);
    slot = cl->cl_Dispatcher.h_SubEntry ? (unsigned long *)&cl->cl_Dispatcher.h_SubEntry : (unsigned long *)&cl->cl_Dispatcher.h_Entry;
    printf("SLOT %s\n", cl->cl_Dispatcher.h_SubEntry ? "h_SubEntry" : "h_Entry");
    Forbid(); g_saved = *slot; *slot = (unsigned long)count_disp; Permit();
    w1 = mkwin(scr, vi, 10, &gl1);
    if (w1) {
        struct TagItem bt[2];
        GT_RefreshWindow(w1, NULL);
        bt[0].ti_Tag = GT_VisualInfo; bt[0].ti_Data = (ULONG)vi; bt[1].ti_Tag = TAG_DONE; bt[1].ti_Data = 0;
        DrawBevelBoxA(w1->RPort, 100, 76, 90, 30, bt);
        printf("COUNT after open: total=%ld draw=%ld drawframe=%ld other=%ld\n", g_total, g_draw, g_frame, g_other);
        w2 = mkwin(scr, vi, 250, &gl2);
        Delay(50);
        printf("COUNT after second window (w1 inactive): total=%ld draw=%ld drawframe=%ld other=%ld\n", g_total, g_draw, g_frame, g_other);
        ActivateWindow(w1); Delay(25);
        RefreshGList(gl1, w1, NULL, -1); GT_RefreshWindow(w1, NULL); Delay(25);
        printf("COUNT after refresh: total=%ld draw=%ld drawframe=%ld other=%ld\n", g_total, g_draw, g_frame, g_other);
        if (w2) CloseWindow(w2);
        CloseWindow(w1);
    } else
        printf("NOWIN\n");
    for (i = 0; i < g_n; i++)
        printf("CALL %d method=%lx state=%lu w=%lu h=%lu\n", i, g_log[i][0], g_log[i][1], g_log[i][2], g_log[i][3]);
    Forbid(); if (*slot == (unsigned long)count_disp) *slot = g_saved; Permit();
    FreeGadgets(gl1);
    FreeVisualInfo(vi); FreeScreenDrawInfo(scr, di); UnlockPubScreen(NULL, scr);
    return 0;
}
