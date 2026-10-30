/* flatprobe -- FLAT-M1: dump the pens of two windows (one active, one inactive) on the Workbench screen as text, so a host
 * script can look for two-tone bevels without a picture (plan 2026-10-29-amarchy-flat-ui-patch.md, FLAT-M1).
 *   flatprobe [FOREVER]   open window A (inactive) and window B (active) with close / depth / zoom / size / drag gadgets and
 *                         GadTools gadgets (button, string, checkbox, cycle, listview, bevel box), then print
 *                         PENS <dri_Pens 0..11>, DEPTH <n>, per window WIN name left top w h, GAD kind x y w h (screen
 *                         coordinates) and one ROW line per pixel row (one char per pen: 0-9 a-v), and close both.
 *                         FOREVER keeps the windows open until Ctrl-C (for a menu / screenshot look). */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <exec/lists.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;
long __stack = 16384;
const char flatprobe_stack_cookie[] = "$STACK: 16384";

#define NG 5
static struct Gadget *gl[2];
static void *vi;
static struct Screen *scr;
static const char *cyc[] = { "One", "Two", NULL };
static struct List lv;
static struct Window *win[2];
static char gk[2][NG][12];
static int gx[2][NG + 1], gy[2][NG + 1], gw[2][NG + 1], gh[2][NG + 1];

static struct Window *mkwin(int n, int left, const char *title)
{
    struct NewGadget ng;
    struct Gadget *g;
    struct Window *w;
    int k;
    gl[n] = NULL;
    g = CreateContext(&gl[n]);
    memset(&ng, 0, sizeof ng);
    ng.ng_VisualInfo = vi;
    ng.ng_TextAttr = scr->Font;
    ng.ng_LeftEdge = 8; ng.ng_TopEdge = 18; ng.ng_Width = 80; ng.ng_Height = 14; ng.ng_GadgetText = (UBYTE *)"Button"; ng.ng_GadgetID = 1;
    g = CreateGadgetA(BUTTON_KIND, g, &ng, NULL);
    ng.ng_TopEdge = 36; ng.ng_GadgetText = NULL; ng.ng_GadgetID = 2;
    g = CreateGadgetA(STRING_KIND, g, &ng, NULL);
    ng.ng_TopEdge = 54; ng.ng_Width = 26; ng.ng_GadgetID = 3;
    g = CreateGadgetA(CHECKBOX_KIND, g, &ng, NULL);
    { struct TagItem t[2]; t[0].ti_Tag = GTCY_Labels; t[0].ti_Data = (ULONG)cyc; t[1].ti_Tag = TAG_DONE; t[1].ti_Data = 0;
      ng.ng_TopEdge = 72; ng.ng_Width = 80; ng.ng_GadgetID = 4;
      g = CreateGadgetA(CYCLE_KIND, g, &ng, t); }
    { struct TagItem t[2]; t[0].ti_Tag = GTLV_Labels; t[0].ti_Data = (ULONG)&lv; t[1].ti_Tag = TAG_DONE; t[1].ti_Data = 0;
      ng.ng_LeftEdge = 100; ng.ng_TopEdge = 18; ng.ng_Width = 90; ng.ng_Height = 50; ng.ng_GadgetID = 5;
      g = CreateGadgetA(LISTVIEW_KIND, g, &ng, t); }
    { struct TagItem t[12]; int i = 0;
      t[i].ti_Tag = WA_Left; t[i++].ti_Data = left;
      t[i].ti_Tag = WA_Top; t[i++].ti_Data = 30;
      t[i].ti_Tag = WA_Width; t[i++].ti_Data = 220;
      t[i].ti_Tag = WA_Height; t[i++].ti_Data = 120;
      t[i].ti_Tag = WA_Title; t[i++].ti_Data = (ULONG)title;
      t[i].ti_Tag = WA_CloseGadget; t[i++].ti_Data = 1;
      t[i].ti_Tag = WA_DepthGadget; t[i++].ti_Data = 1;
      t[i].ti_Tag = WA_SizeGadget; t[i++].ti_Data = 1;
      t[i].ti_Tag = WA_DragBar; t[i++].ti_Data = 1;
      t[i].ti_Tag = WA_Gadgets; t[i++].ti_Data = (ULONG)gl[n];
      t[i].ti_Tag = WA_IDCMP; t[i++].ti_Data = IDCMP_CLOSEWINDOW | BUTTONIDCMP;
      t[i].ti_Tag = TAG_DONE; t[i].ti_Data = 0;
      w = OpenWindowTagList(NULL, t); }
    if (!w)
        return NULL;
    GT_RefreshWindow(w, NULL);
    {   struct TagItem bt[2]; bt[0].ti_Tag = GT_VisualInfo; bt[0].ti_Data = (ULONG)vi; bt[1].ti_Tag = TAG_DONE; bt[1].ti_Data = 0;
        DrawBevelBoxA(w->RPort, 100, 76, 90, 30, bt); }
    k = 0;
    for (g = w->FirstGadget; g && k < NG + 1; g = g->NextGadget, k++) {
        gx[n][k] = w->LeftEdge + g->LeftEdge; gy[n][k] = w->TopEdge + g->TopEdge; gw[n][k] = g->Width; gh[n][k] = g->Height;
    }
    gw[n][NG] = 90; gh[n][NG] = 30; gx[n][NG] = w->LeftEdge + 100; gy[n][NG] = w->TopEdge + 76;
    return w;
}

static void dump(int n, const char *name)
{
    static const char ch[] = "0123456789abcdefghijklmnopqrstuv";
    struct Window *w = win[n];
    int x, y, k;
    char row[260];
    printf("WIN %s %ld %ld %ld %ld\n", name, (long)w->LeftEdge, (long)w->TopEdge, (long)w->Width, (long)w->Height);
    printf("BORDER left=%ld top=%ld right=%ld bottom=%ld\n", (long)w->BorderLeft, (long)w->BorderTop, (long)w->BorderRight, (long)w->BorderBottom);
    { struct Gadget *g; for (g = w->FirstGadget, k = 0; g; g = g->NextGadget, k++)
        printf("GAD %d type=%04x x=%d y=%d w=%d h=%d\n", k, g->GadgetType, w->LeftEdge + (g->Flags & GFLG_RELRIGHT ? w->Width + g->LeftEdge : g->LeftEdge),
               w->TopEdge + (g->Flags & GFLG_RELBOTTOM ? w->Height + g->TopEdge : g->TopEdge), g->Width + ((g->Flags & GFLG_RELWIDTH) ? w->Width : 0), g->Height + ((g->Flags & GFLG_RELHEIGHT) ? w->Height : 0)); }
    for (y = 0; y < w->Height; y++) {
        for (x = 0; x < w->Width && x < 255; x++) {
            long p = ReadPixel(&scr->RastPort, w->LeftEdge + x, w->TopEdge + y);
            row[x] = p >= 0 && p < 32 ? ch[p] : '?';
        }
        row[x] = 0;
        printf("ROW %s\n", row);
    }
}

int main(int argc, char **argv)
{
    struct DrawInfo *di;
    int i;
    if (argv && argv[0] == NULL)
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
    printf("PENS");
    for (i = 0; i < 12 && i < di->dri_NumPens; i++)
        printf(" %ld", (long)di->dri_Pens[i]);
    printf("\nSCRBAR h=%ld\n", (long)scr->BarHeight);
    { int y, x; static const char c2[] = "0123456789abcdefghijklmnopqrstuv"; char r[90];
      for (y = 0; y <= scr->BarHeight; y++) { for (x = 0; x < 80; x++) { long p = ReadPixel(&scr->RastPort, x, y); r[x] = p >= 0 && p < 32 ? c2[p] : '?'; } r[80] = 0; printf("SROW %s\n", r);
        for (x = 0; x < 80; x++) { long p = ReadPixel(&scr->RastPort, scr->Width - 80 + x, y); r[x] = p >= 0 && p < 32 ? c2[p] : '?'; } r[80] = 0; printf("SROWR %s\n", r); } }
    printf("\nDEPTH %ld\n", (long)scr->RastPort.BitMap->Depth);
    win[0] = mkwin(0, 10, "inactive");
    win[1] = mkwin(1, 250, "active");
    if (win[0] && win[1]) {
        ActivateWindow(win[1]);
        Delay(60);
        dump(0, "inactive");
        dump(1, "active");
        if (argc > 1)
            Wait(SIGBREAKF_CTRL_C);
    } else
        printf("NOWIN\n");
    for (i = 0; i < 2; i++)
        if (win[i]) {
            CloseWindow(win[i]);
            FreeGadgets(gl[i]);
        }
    FreeVisualInfo(vi);
    FreeScreenDrawInfo(scr, di);
    UnlockPubScreen(NULL, scr);
    return 0;
}
