/* pubscreen: a named custom PUBLIC screen with one window of GadTools gadgets at fixed coordinates and a menu strip, a rig
 * helper for the per-screen theming proof (TE-9.6). Not part of the release dist or kit.
 *
 *   pubscreen NAME/A,DEPTH/N,TITLE/K,SECONDS/N
 *
 * Opens the public screen NAME (depth DEPTH, default 4, a hires screen cloned from the Workbench where V39 allows, the standard
 * 3D pen list so the DrawInfo is new-look), one window "TITLE" (default NAME) with drag bar, depth, close and size gadgets and
 * these GadTools gadgets, in WINDOW coordinates (the ready line prints them in SCREEN coordinates = window left/top + these):
 *   button    20,20  100x14   "Button"
 *   string   120,40  150x14   "text"      (label left of the box)
 *   scroller 120,60  150x12   horizontal, with arrows
 *   checkbox 120,80  (GadTools size)
 *   cycle    120,100 120x14   Alpha / Beta / Gamma
 * and a one-item menu "Project / Quit". The window is 300x150 at 40,30.
 * Prints one "pubscreen ready ..." line, then waits for CTRL-C, the close gadget or SECONDS. Exit: 0 ok, 5 screen still has
 * visitors, 20 failure. */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>

#ifndef SA_LikeWorkbench
#define SA_LikeWorkbench (SA_Dummy + 0x28)
#endif

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;
long __stack = 16384;
const char pubscreen_stack_cookie[] = "$STACK: 16384";
static const char ver[] = "$VER: pubscreen 1.0 (6.11.2026) (UP-Term)";

#define WL 40
#define WT 30
#define WW 300
#define WH 150

static struct NewMenu newmenu[] = {
    {NM_TITLE, (STRPTR)"Project", 0, 0, 0, 0},
    {NM_ITEM, (STRPTR)"Quit", (STRPTR)"Q", 0, 0, 0},
    {NM_END, 0, 0, 0, 0, 0}
};
static STRPTR cyclelabels[] = {(STRPTR)"Alpha", (STRPTR)"Beta", (STRPTR)"Gamma", 0};

static void box(const char *n, struct Gadget *g, struct Window *w)
{
    printf(" %s=%ld,%ld,%ld,%ld", n, (long)(w->LeftEdge + g->LeftEdge), (long)(w->TopEdge + g->TopEdge), (long)g->Width, (long)g->Height);
}

static struct Gadget *mk(ULONG kind, struct Gadget *prev, void *vi, struct TextAttr *ta, int l, int t, int w, int h, const char *text,
                         int id, struct TagItem *tags)
{
    struct NewGadget ng;
    memset(&ng, 0, sizeof ng);
    ng.ng_VisualInfo = vi; ng.ng_TextAttr = ta;
    ng.ng_LeftEdge = l; ng.ng_TopEdge = t; ng.ng_Width = w; ng.ng_Height = h;
    ng.ng_GadgetText = (UBYTE *)text; ng.ng_GadgetID = id;
    return CreateGadgetA(kind, prev, &ng, tags);
}

int main(int argc, char **argv)
{
    static UWORD pens[] = {(UWORD)~0};
    struct RDArgs *rda;
    LONG args[4] = {0, 0, 0, 0};
    const char *name, *title;
    long depth = 4, secs = 0, ticks = 0, i;
    struct Screen *scr = NULL;
    struct Window *win = NULL;
    struct Gadget *gl = NULL, *g, *gb, *gs, *gc, *gk, *gy;
    struct Menu *menus = NULL;
    void *vi = NULL;
    struct TagItem t[16];
    int n, done = 0, rc = 20;
    if (argc < 1 || (argv && argv[0] == NULL))
        return 5;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    GadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 39);
    if (!IntuitionBase || !GfxBase || !GadToolsBase) {
        printf("pubscreen: need intuition, graphics and gadtools V39\n");
        goto out;
    }
    rda = ReadArgs((STRPTR)"NAME/A,DEPTH/N,TITLE/K,SECONDS/N", args, NULL);
    if (!rda) {
        printf("pubscreen: usage: pubscreen NAME/A,DEPTH/N,TITLE/K,SECONDS/N\n");
        goto out;
    }
    name = (const char *)args[0];
    if (args[1]) depth = *(LONG *)args[1];
    title = args[2] ? (const char *)args[2] : name;
    if (args[3]) secs = *(LONG *)args[3];
    if (depth < 1 || depth > 8) {
        printf("pubscreen: DEPTH must be 1..8\n");
        FreeArgs(rda);
        goto out;
    }
    n = 0;
    t[n].ti_Tag = SA_LikeWorkbench; t[n++].ti_Data = TRUE;
    t[n].ti_Tag = SA_Depth; t[n++].ti_Data = depth;
    t[n].ti_Tag = SA_Title; t[n++].ti_Data = (ULONG)title;
    t[n].ti_Tag = SA_PubName; t[n++].ti_Data = (ULONG)name;
    t[n].ti_Tag = SA_Type; t[n++].ti_Data = PUBLICSCREEN;
    t[n].ti_Tag = SA_Pens; t[n++].ti_Data = (ULONG)pens;
    t[n].ti_Tag = TAG_DONE;
    scr = OpenScreenTagList(NULL, t);
    if (!scr) { /* fallback: a plain hires screen */
        n = 0;
        t[n].ti_Tag = SA_Width; t[n++].ti_Data = 640;
        t[n].ti_Tag = SA_Height; t[n++].ti_Data = 256;
        t[n].ti_Tag = SA_DisplayID; t[n++].ti_Data = 0x00008000; /* HIRES_KEY */
        t[n].ti_Tag = SA_Depth; t[n++].ti_Data = depth;
        t[n].ti_Tag = SA_Title; t[n++].ti_Data = (ULONG)title;
        t[n].ti_Tag = SA_PubName; t[n++].ti_Data = (ULONG)name;
        t[n].ti_Tag = SA_Type; t[n++].ti_Data = PUBLICSCREEN;
        t[n].ti_Tag = SA_Pens; t[n++].ti_Data = (ULONG)pens;
        t[n].ti_Tag = TAG_DONE;
        scr = OpenScreenTagList(NULL, t);
    }
    FreeArgs(rda); /* name/title are copied by Intuition; the strings are not needed any more */
    if (!scr) {
        printf("pubscreen: cannot open screen (name already taken?)\n");
        goto out;
    }
    PubScreenStatus(scr, 0);
    vi = GetVisualInfoA(scr, NULL);
    if (!vi) { printf("pubscreen: no visual info\n"); goto out; }
    g = CreateContext(&gl);
    if (!g) { printf("pubscreen: no gadget context\n"); goto out; }
    n = 0; t[n].ti_Tag = TAG_DONE;
    gb = g = mk(BUTTON_KIND, g, vi, scr->Font, 20, 20, 100, 14, "Button", 1, t);
    n = 0;
    t[n].ti_Tag = GTST_String; t[n++].ti_Data = (ULONG)"text";
    t[n].ti_Tag = GTST_MaxChars; t[n++].ti_Data = 40;
    t[n].ti_Tag = TAG_DONE;
    gs = g = mk(STRING_KIND, g, vi, scr->Font, 120, 40, 150, 14, "String", 2, t);
    n = 0;
    t[n].ti_Tag = PGA_Freedom; t[n++].ti_Data = LORIENT_HORIZ;
    t[n].ti_Tag = GTSC_Total; t[n++].ti_Data = 10;
    t[n].ti_Tag = GTSC_Visible; t[n++].ti_Data = 3;
    t[n].ti_Tag = GTSC_Arrows; t[n++].ti_Data = 12;
    t[n].ti_Tag = TAG_DONE;
    gc = g = mk(SCROLLER_KIND, g, vi, scr->Font, 120, 60, 150, 12, "Scroller", 3, t);
    n = 0; t[n].ti_Tag = TAG_DONE;
    gk = g = mk(CHECKBOX_KIND, g, vi, scr->Font, 120, 80, 26, 11, "Check", 4, t);
    n = 0;
    t[n].ti_Tag = GTCY_Labels; t[n++].ti_Data = (ULONG)cyclelabels;
    t[n].ti_Tag = TAG_DONE;
    gy = g = mk(CYCLE_KIND, g, vi, scr->Font, 120, 100, 120, 14, "Cycle", 5, t);
    if (!g) { printf("pubscreen: cannot create gadgets\n"); goto out; }
    menus = CreateMenusA(newmenu, NULL);
    if (!menus || !LayoutMenusA(menus, vi, NULL)) { printf("pubscreen: cannot create menus\n"); goto out; }
    n = 0;
    t[n].ti_Tag = WA_Left; t[n++].ti_Data = WL;
    t[n].ti_Tag = WA_Top; t[n++].ti_Data = WT;
    t[n].ti_Tag = WA_Width; t[n++].ti_Data = WW;
    t[n].ti_Tag = WA_Height; t[n++].ti_Data = WH;
    t[n].ti_Tag = WA_Title; t[n++].ti_Data = (ULONG)title;
    t[n].ti_Tag = WA_PubScreen; t[n++].ti_Data = (ULONG)scr;
    t[n].ti_Tag = WA_DragBar; t[n++].ti_Data = TRUE;
    t[n].ti_Tag = WA_DepthGadget; t[n++].ti_Data = TRUE;
    t[n].ti_Tag = WA_CloseGadget; t[n++].ti_Data = TRUE;
    t[n].ti_Tag = WA_SizeGadget; t[n++].ti_Data = TRUE;
    t[n].ti_Tag = WA_Activate; t[n++].ti_Data = TRUE;
    t[n].ti_Tag = WA_SimpleRefresh; t[n++].ti_Data = TRUE;
    t[n].ti_Tag = WA_Gadgets; t[n++].ti_Data = (ULONG)gl;
    t[n].ti_Tag = WA_IDCMP; t[n++].ti_Data = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_MENUPICK | BUTTONIDCMP | STRINGIDCMP |
                                             SCROLLERIDCMP | CHECKBOXIDCMP | CYCLEIDCMP;
    t[n].ti_Tag = TAG_DONE;
    win = OpenWindowTagList(NULL, t);
    if (!win) { printf("pubscreen: cannot open window\n"); goto out; }
    GT_RefreshWindow(win, NULL);
    if (!SetMenuStrip(win, menus)) { printf("pubscreen: cannot set menu strip\n"); CloseWindow(win); win = NULL; goto out; }
    printf("pubscreen ready name=%s screen=0x%lx window=0x%lx depth=%ld win=%ld,%ld,%ld,%ld", name, (ULONG)scr, (ULONG)win,
           (long)scr->RastPort.BitMap->Depth, (long)win->LeftEdge, (long)win->TopEdge, (long)win->Width, (long)win->Height);
    box("button", gb, win); box("string", gs, win); box("scroller", gc, win); box("checkbox", gk, win); box("cycle", gy, win);
    printf("\n");
    fflush(stdout);
    rc = 0;
    while (!done) {
        struct IntuiMessage *m;
        Delay(5);
        ticks++;
        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) break;
        if (secs && ticks >= secs * 10) break;
        while ((m = GT_GetIMsg(win->UserPort)) != NULL) {
            ULONG cls = m->Class;
            if (cls == IDCMP_CLOSEWINDOW) done = 1;
            else if (cls == IDCMP_MENUPICK && m->Code != MENUNULL) done = 1;
            GT_ReplyIMsg(m);
            if (cls == IDCMP_REFRESHWINDOW) {
                GT_BeginRefresh(win);
                GT_EndRefresh(win, TRUE);
            }
        }
    }
out:
    if (win) {
        ClearMenuStrip(win);
        CloseWindow(win);
    }
    if (gl) FreeGadgets(gl);
    if (menus) FreeMenus(menus);
    if (vi) FreeVisualInfo(vi);
    if (scr) {
        for (i = 0; !CloseScreen(scr); i++) {
            if (i >= 20) {
                printf("pubscreen: screen still has visitors\n");
                rc = 5;
                break;
            }
            Delay(25);
        }
    }
    if (GadToolsBase) CloseLibrary(GadToolsBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
