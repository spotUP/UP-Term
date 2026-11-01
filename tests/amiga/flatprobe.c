/* flatprobe -- FLAT-M1: dump the pens of two windows (one active, one inactive) on the Workbench screen as text, so a host
 * script can look for two-tone bevels without a picture (plan 2026-10-29-amarchy-flat-ui-patch.md, FLAT-M1).
 *   flatprobe [FOREVER]   open window A (inactive) and window B (active) with close / depth / zoom / size / drag gadgets and
 *                         GadTools gadgets (button, string, checkbox, cycle, listview, bevel box), then print
 *                         PENS <dri_Pens 0..11>, DEPTH <n>, per window WIN name left top w h, GAD kind x y w h (screen
 *                         coordinates) and one ROW line per pixel row (one char per pen: 0-9 a-v), and close both.
 *                         FOREVER keeps the windows open until Ctrl-C (for a menu / screenshot look). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/imageclass.h>
#include <graphics/gfxbase.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <workbench/workbench.h>
#include <proto/wb.h>
#include <exec/lists.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;
struct Library *WorkbenchBase;
long __stack = 16384;
const char flatprobe_stack_cookie[] = "$STACK: 16384";

#define NG 5
static struct Gadget *gl[2];
static void *vi;
static struct Screen *scr;
static struct DrawInfo *drawinfo;
static int custom;
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
    { struct TagItem t[14]; int i = 0;
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
      if (custom) { t[i].ti_Tag = WA_CustomScreen; t[i++].ti_Data = (ULONG)scr; }
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

static void dumpwin(struct Window *w, const char *name)
{
    static const char ch[] = "0123456789abcdefghijklmnopqrstuv";
    int x, y, k;
    char row[720];
    printf("WIN %s %ld %ld %ld %ld\n", name, (long)w->LeftEdge, (long)w->TopEdge, (long)w->Width, (long)w->Height);
    printf("BORDER left=%ld top=%ld right=%ld bottom=%ld\n", (long)w->BorderLeft, (long)w->BorderTop, (long)w->BorderRight, (long)w->BorderBottom);
    { struct Gadget *g; for (g = w->FirstGadget, k = 0; g; g = g->NextGadget, k++)
        printf("GAD %d type=%04x x=%d y=%d w=%d h=%d\n", k, g->GadgetType, w->LeftEdge + (g->Flags & GFLG_RELRIGHT ? w->Width + g->LeftEdge : g->LeftEdge),
               w->TopEdge + (g->Flags & GFLG_RELBOTTOM ? w->Height + g->TopEdge : g->TopEdge), g->Width + ((g->Flags & GFLG_RELWIDTH) ? w->Width : 0), g->Height + ((g->Flags & GFLG_RELHEIGHT) ? w->Height : 0)); }
    for (y = 0; y < w->Height; y++) {
        for (x = 0; x < w->Width && x < 700; x++) {
            long p = ReadPixel(&scr->RastPort, w->LeftEdge + x, w->TopEdge + y);
            row[x] = p >= 0 && p < 32 ? ch[p] : '?';
        }
        row[x] = 0;
        printf("ROW %s\n", row);
    }
}

static void dump(int n, const char *name)
{
    dumpwin(win[n], name);
}

static void region(const char *label, int x0, int y0, int w, int h)
{
    static const char ch[] = "0123456789abcdefghijklmnopqrstuv";
    char row[400];
    int x, y;
    printf("REGION %s %d %d %d %d\n", label, x0, y0, w, h);
    for (y = y0; y < y0 + h; y++) {
        for (x = 0; x < w && x < 399; x++) {
            long p = ReadPixel(&scr->RastPort, x0 + x, y);
            row[x] = p >= 0 && p < 32 ? ch[p] : '?';
        }
        row[x] = 0;
        printf("MROW %s\n", row);
    }
}

static struct Menu *mkmenu(void)
{
    static struct NewMenu nm[] = {
        { NM_TITLE, (STRPTR)"Project", 0, 0, 0, 0 },
        { NM_ITEM, (STRPTR)"Checked", 0, CHECKIT | CHECKED | MENUTOGGLE, 0, 0 },
        { NM_ITEM, NM_BARLABEL, 0, 0, 0, 0 },
        { NM_ITEM, (STRPTR)"Submenu", 0, 0, 0, 0 },
        { NM_SUB, (STRPTR)"One", 0, CHECKIT | CHECKED, 0, 0 },
        { NM_SUB, (STRPTR)"Two", 0, CHECKIT, 0, 0 },
        { NM_ITEM, (STRPTR)"Command", (STRPTR)"Q", 0, 0, 0 },
        { NM_ITEM, (STRPTR)"Mutual A", 0, CHECKIT | CHECKED, 2, 0 },
        { NM_ITEM, (STRPTR)"Mutual B", 0, CHECKIT, 1, 0 },
        { NM_TITLE, (STRPTR)"Edit", 0, 0, 0, 0 },
        { NM_ITEM, (STRPTR)"Copy", 0, 0, 0, 0 },
        { NM_END, 0, 0, 0, 0, 0 }
    };
    struct Menu *m = CreateMenusA(nm, NULL);
    if (m)
        LayoutMenusA(m, vi, NULL);
    return m;
}

/* a window with a bottom/right border scroller: prop gadgets with autoknob and sysiclass arrows (a Workbench drawer's pattern) */
static struct Window *mkscroll(int left)
{
    static struct Image *arrow[4];
    static struct Gadget prop[2], arr[4];
    static struct PropInfo pi[2];
    static struct Image knob[2];
    static const ULONG which[4] = { UPIMAGE, DOWNIMAGE, LEFTIMAGE, RIGHTIMAGE };
    struct Window *w;
    struct TagItem t[16];
    int i, k = 0;
    memset(prop, 0, sizeof prop); memset(arr, 0, sizeof arr); memset(pi, 0, sizeof pi); memset(knob, 0, sizeof knob);
    for (i = 0; i < 4; i++) {
        struct TagItem it[3];
        it[0].ti_Tag = SYSIA_DrawInfo; it[0].ti_Data = (ULONG)drawinfo;
        it[1].ti_Tag = SYSIA_Which; it[1].ti_Data = which[i];
        it[2].ti_Tag = TAG_DONE; it[2].ti_Data = 0;
        arrow[i] = (struct Image *)NewObjectA(NULL, (UBYTE *)"sysiclass", it);
    }
    for (i = 0; i < 4; i++) {
        arr[i].GadgetType = GTYP_REQGADGET; /* replaced below by border flags */
        arr[i].GadgetType = GTYP_BOOLGADGET;
        arr[i].Flags = GFLG_GADGIMAGE | GFLG_GADGHCOMP | GFLG_RELRIGHT | GFLG_RELBOTTOM;
        arr[i].Activation = GACT_RELVERIFY | (i < 2 ? GACT_RIGHTBORDER : GACT_BOTTOMBORDER);
        arr[i].GadgetRender = arrow[i];
        arr[i].Width = arrow[i] ? arrow[i]->Width : 16;
        arr[i].Height = arrow[i] ? arrow[i]->Height : 10;
        arr[i].GadgetID = 10 + i;
        if (i < 2) { arr[i].LeftEdge = -(arr[i].Width - 1); arr[i].TopEdge = -(arr[i].Height * (2 - i) + 8 - 1); }
        else { arr[i].LeftEdge = -(arr[i].Width * (4 - i) + 18 - 1); arr[i].TopEdge = -(arr[i].Height - 1); }
        arr[i].NextGadget = i < 3 ? &arr[i + 1] : &prop[0];
    }
    for (i = 0; i < 2; i++) {
        pi[i].Flags = AUTOKNOB | FREEVERT | PROPNEWLOOK | PROPBORDERLESS;
        pi[i].VertBody = 0x4000; pi[i].HorizBody = 0x4000;
        if (i == 0) pi[i].Flags = AUTOKNOB | FREEVERT | PROPNEWLOOK; else pi[i].Flags = AUTOKNOB | FREEHORIZ | PROPNEWLOOK;
        prop[i].GadgetType = GTYP_PROPGADGET;
        prop[i].GadgetRender = &knob[i];
        prop[i].SpecialInfo = &pi[i];
        prop[i].Activation = GACT_RELVERIFY | GACT_IMMEDIATE | (i == 0 ? GACT_RIGHTBORDER : GACT_BOTTOMBORDER);
        prop[i].GadgetID = 20 + i;
        if (i == 0) { prop[i].Flags = GFLG_RELRIGHT | GFLG_RELHEIGHT; prop[i].LeftEdge = -13; prop[i].TopEdge = 12; prop[i].Width = 10; prop[i].Height = -(12 + 30); }
        else { prop[i].Flags = GFLG_RELBOTTOM | GFLG_RELWIDTH; prop[i].LeftEdge = 4; prop[i].TopEdge = -7; prop[i].Width = -(4 + 70); prop[i].Height = 5; }
        prop[i].NextGadget = i == 0 ? &prop[1] : NULL;
    }
    t[k].ti_Tag = WA_Left; t[k++].ti_Data = left;
    t[k].ti_Tag = WA_Top; t[k++].ti_Data = 20;
    t[k].ti_Tag = WA_Width; t[k++].ti_Data = 220;
    t[k].ti_Tag = WA_Height; t[k++].ti_Data = 120;
    t[k].ti_Tag = WA_Title; t[k++].ti_Data = (ULONG)"scroller";
    t[k].ti_Tag = WA_CloseGadget; t[k++].ti_Data = 1;
    t[k].ti_Tag = WA_DepthGadget; t[k++].ti_Data = 1;
    t[k].ti_Tag = WA_SizeGadget; t[k++].ti_Data = 1;
    t[k].ti_Tag = WA_SizeBRight; t[k++].ti_Data = 1;
    t[k].ti_Tag = WA_DragBar; t[k++].ti_Data = 1;
    t[k].ti_Tag = WA_Gadgets; t[k++].ti_Data = (ULONG)&arr[0];
    t[k].ti_Tag = WA_IDCMP; t[k++].ti_Data = IDCMP_CLOSEWINDOW;
    if (custom) { t[k].ti_Tag = WA_CustomScreen; t[k++].ti_Data = (ULONG)scr; }
    t[k].ti_Tag = TAG_DONE; t[k].ti_Data = 0;
    w = OpenWindowTagList(NULL, t);
    return w;
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
    {   /* args: MODE [OWN FLAT|STOCK depth]; MODE BASIC | MENU | SCROLL | SCAN <secs> | FOREVER */
        static UWORD flatpens[] = { 0, 1, 2, 0, 1, 3, 0, 0, 2, 2, 0, 1, 0xFFFF };
        static UWORD stockpens[] = { 0, 1, 1, 2, 1, 3, 1, 0, 2, 1, 2, 1, 0xFFFF };
        struct TagItem st[8];
        int depth = 2, n = 0;
        if (argc > 2 && !strcmp(argv[2], "OWN")) {
            if (argc > 4)
                depth = atoi(argv[4]);
            st[n].ti_Tag = SA_Width; st[n++].ti_Data = 640;
            st[n].ti_Tag = SA_Height; st[n++].ti_Data = 256;
            st[n].ti_Tag = SA_Depth; st[n++].ti_Data = depth;
            st[n].ti_Tag = SA_DisplayID; st[n++].ti_Data = 0x8000;
            st[n].ti_Tag = SA_Title; st[n++].ti_Data = (ULONG)"flatprobe own screen";
            st[n].ti_Tag = SA_Pens; st[n++].ti_Data = (ULONG)(argc > 3 && !strcmp(argv[3], "STOCK") ? stockpens : flatpens);
            st[n].ti_Tag = TAG_DONE; st[n].ti_Data = 0;
            scr = OpenScreenTagList(NULL, st);
            custom = 1;
        } else
            scr = LockPubScreen(NULL);
    }
    di = scr ? GetScreenDrawInfo(scr) : NULL;
    drawinfo = di;
    vi = scr ? GetVisualInfoA(scr, NULL) : NULL;
    if (!di || !vi)
        return 10;
    lv.lh_Head = (struct Node *)&lv.lh_Tail; lv.lh_Tail = NULL; lv.lh_TailPred = (struct Node *)&lv.lh_Head;
    if (argc > 1 && !strcmp(argv[1], "SCAN")) {
        struct Window *w;
        Delay(50 * (argc > 2 ? atoi(argv[2]) : 5));
        printf("PENS"); for (i = 0; i < 12 && i < di->dri_NumPens; i++) printf(" %ld", (long)di->dri_Pens[i]);
        printf("\nDEPTH %ld\n", (long)scr->RastPort.BitMap->Depth);
        for (w = scr->FirstWindow; w; w = w->NextWindow)
            dumpwin(w, w->Title ? (const char *)w->Title : "(untitled)");
        FreeVisualInfo(vi); FreeScreenDrawInfo(scr, di); UnlockPubScreen(NULL, scr);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "DRAWER")) {   /* open a real Workbench drawer window (SYS:), then dump it; DRAWER [path] */
        struct Window *w;
        int tries;
        WorkbenchBase = OpenLibrary((STRPTR)"workbench.library", 36);
        OpenWorkbenchObjectA((STRPTR)(argc > 2 ? argv[2] : "SYS:Prefs"), NULL);
        Delay(50 * 6);
        printf("PENS"); for (i = 0; i < 12 && i < di->dri_NumPens; i++) printf(" %ld", (long)di->dri_Pens[i]);
        printf("\nDEPTH %ld\n", (long)scr->RastPort.BitMap->Depth);
        for (tries = 0, w = scr->FirstWindow; w; w = w->NextWindow, tries++)
            dumpwin(w, w->Title ? (const char *)w->Title : "(untitled)");
        FreeVisualInfo(vi); FreeScreenDrawInfo(scr, di); UnlockPubScreen(NULL, scr);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "SCROLL")) {
        struct Window *w = mkscroll(10);
        Delay(60);
        printf("PENS"); for (i = 0; i < 12 && i < di->dri_NumPens; i++) printf(" %ld", (long)di->dri_Pens[i]);
        printf("\nDEPTH %ld\n", (long)scr->RastPort.BitMap->Depth);
        if (w) { dumpwin(w, "scroller"); CloseWindow(w); } else printf("NOWIN\n");
        FreeVisualInfo(vi); FreeScreenDrawInfo(scr, di);
        if (custom) CloseScreen(scr); else UnlockPubScreen(NULL, scr);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "MENU")) {
        struct Window *w;
        struct Menu *m = mkmenu();
        struct TagItem t[10]; int k = 0;
        t[k].ti_Tag = WA_Left; t[k++].ti_Data = 300;
        t[k].ti_Tag = WA_Top; t[k++].ti_Data = 120;
        t[k].ti_Tag = WA_Width; t[k++].ti_Data = 200;
        t[k].ti_Tag = WA_Height; t[k++].ti_Data = 80;
        t[k].ti_Tag = WA_Title; t[k++].ti_Data = (ULONG)"menu host";
        t[k].ti_Tag = WA_Activate; t[k++].ti_Data = 1;
        t[k].ti_Tag = WA_IDCMP; t[k++].ti_Data = IDCMP_MENUPICK;
        if (custom) { t[k].ti_Tag = WA_CustomScreen; t[k++].ti_Data = (ULONG)scr; }
        t[k].ti_Tag = TAG_DONE; t[k].ti_Data = 0;
        w = OpenWindowTagList(NULL, t);
        if (!w || !m || !SetMenuStrip(w, m)) { printf("NOMENU w=%lx m=%lx\n", (unsigned long)w, (unsigned long)m); return 10; }
        printf("PENS"); for (i = 0; i < 12 && i < di->dri_NumPens; i++) printf(" %ld", (long)di->dri_Pens[i]);
        printf("\nDEPTH %ld\nREADY\n", (long)scr->RastPort.BitMap->Depth);
        fflush(stdout);
        Delay(50 * 7);
        region("menu1", 0, 0, 260, 100);
        Delay(50 * 6);
        region("menu2", 0, 0, 330, 100);
        Delay(50 * 6);
        ClearMenuStrip(w); CloseWindow(w); FreeMenus(m);
        FreeVisualInfo(vi); FreeScreenDrawInfo(scr, di);
        if (custom) CloseScreen(scr); else UnlockPubScreen(NULL, scr);
        return 0;
    }
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
        if (argc > 1 && !strcmp(argv[1], "FOREVER"))
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
    if (custom)
        CloseScreen(scr);
    else
        UnlockPubScreen(NULL, scr);
    return 0;
}
