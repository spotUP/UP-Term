/* pick_list_gt: the picker window (see pick_list.h): a GadTools list, OK and Cancel. Amiga only. */
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include "pick_list.h"

#define PK_LIST 1
#define PK_OK 2
#define PK_CANCEL 3
#define MARGIN_X 8      /* the same inner margins as the Prefs window */
#define MARGIN_Y 6

int pk_run(struct Screen *scr, struct Window *parent, void *vi, struct TextAttr *ta, int fw, int fh,
           const char *title, const char *const *text, int n, int sel)
{
    struct List list;
    struct Node *nodes;
    struct Gadget *glist = 0, *g;
    struct NewGadget ng;
    struct TagItem t[10], wt[12];
    struct Requester rq;
    struct Window *win;
    struct IntuiMessage *im;
    int i, rows = pk_rows(n), maxlen = 12, bh = fh + 4, lw, lh, bw = 10 * fw, tw, done = 0, blocked = 0;
    int result = -1, wn = 0;
    ULONG lastsec = 0, lastmic = 0;
    WORD ox = (WORD)(scr->WBorLeft + MARGIN_X), oy = (WORD)(scr->WBorTop + scr->Font->ta_YSize + 1 + MARGIN_Y);
    if (n < 1)
        return -1;
    nodes = (struct Node *)AllocVec((ULONG)n * sizeof(struct Node), MEMF_PUBLIC | MEMF_CLEAR);
    if (!nodes)
        return -1;
    list.lh_Head = (struct Node *)&list.lh_Tail;       /* NewList, without linking amiga.lib */
    list.lh_Tail = 0;
    list.lh_TailPred = (struct Node *)&list.lh_Head;
    list.lh_Type = NT_USER;
    for (i = 0; i < n; i++) {
        int len = (int)strlen(text[i]);
        nodes[i].ln_Name = (char *)text[i];
        AddTail(&list, &nodes[i]);
        if (len > maxlen)
            maxlen = len;
    }
    sel = pk_initial(n, sel);
    /* wide enough for the longest entry (plus the scroll bar), never wider than the screen */
    lw = (maxlen + 4) * fw;
    tw = 2 * bw + 2 * fw;
    if (lw < tw)
        lw = tw;
    if (lw > scr->Width - 2 * (MARGIN_X + scr->WBorLeft))
        lw = scr->Width - 2 * (MARGIN_X + scr->WBorLeft);
    lh = rows * fh + 4;
    if (!CreateContext(&glist)) {
        FreeVec(nodes);
        return -1;
    }
    ng.ng_TextAttr = ta;
    ng.ng_VisualInfo = vi;
    ng.ng_UserData = 0;
    ng.ng_Flags = 0;
    ng.ng_LeftEdge = ox;
    ng.ng_TopEdge = oy;
    ng.ng_Width = (WORD)lw;
    ng.ng_Height = (WORD)lh;
    ng.ng_GadgetText = 0;
    ng.ng_GadgetID = PK_LIST;
    t[0].ti_Tag = GTLV_Labels;       t[0].ti_Data = (ULONG)&list;
    t[1].ti_Tag = GTLV_ShowSelected; t[1].ti_Data = 0;
    t[2].ti_Tag = GTLV_Selected;     t[2].ti_Data = (ULONG)sel;
    t[3].ti_Tag = GTLV_Top;          t[3].ti_Data = (ULONG)pk_top(n, rows, sel);
    t[4].ti_Tag = GTLV_ScrollWidth;  t[4].ti_Data = (ULONG)(fw * 2 + 6);
    t[5].ti_Tag = TAG_DONE;          t[5].ti_Data = 0;
    g = CreateGadgetA(LISTVIEW_KIND, glist, &ng, t);
    ng.ng_TopEdge = (WORD)(oy + lh + 8);
    ng.ng_Height = (WORD)bh;
    ng.ng_Width = (WORD)bw;
    ng.ng_GadgetText = (STRPTR)"OK";
    ng.ng_GadgetID = PK_OK;
    ng.ng_Flags = PLACETEXT_IN;
    if (g)
        g = CreateGadgetA(BUTTON_KIND, g, &ng, 0);
    ng.ng_LeftEdge = (WORD)(ox + lw - bw);
    ng.ng_GadgetText = (STRPTR)"Cancel";
    ng.ng_GadgetID = PK_CANCEL;
    if (g)
        g = CreateGadgetA(BUTTON_KIND, g, &ng, 0);
    if (!g) {
        FreeGadgets(glist);
        FreeVec(nodes);
        return -1;
    }
    wt[wn].ti_Tag = WA_Left;        wt[wn++].ti_Data = (ULONG)((scr->Width - (lw + 2 * MARGIN_X)) / 2);
    wt[wn].ti_Tag = WA_Top;         wt[wn++].ti_Data = (ULONG)((scr->Height - (lh + bh + 8 + 2 * MARGIN_Y)) / 3);
    wt[wn].ti_Tag = WA_InnerWidth;  wt[wn++].ti_Data = (ULONG)(lw + 2 * MARGIN_X);
    wt[wn].ti_Tag = WA_InnerHeight; wt[wn++].ti_Data = (ULONG)(lh + bh + 8 + 2 * MARGIN_Y);
    wt[wn].ti_Tag = WA_IDCMP;       wt[wn++].ti_Data = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | LISTVIEWIDCMP |
                                                       BUTTONIDCMP | IDCMP_VANILLAKEY;
    wt[wn].ti_Tag = WA_Flags;       wt[wn++].ti_Data = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET |
                                                       WFLG_ACTIVATE | WFLG_SMART_REFRESH;
    wt[wn].ti_Tag = WA_Title;       wt[wn++].ti_Data = (ULONG)title;
    wt[wn].ti_Tag = WA_PubScreen;   wt[wn++].ti_Data = (ULONG)scr;
    wt[wn].ti_Tag = WA_Gadgets;     wt[wn++].ti_Data = (ULONG)glist;
    wt[wn].ti_Tag = TAG_DONE;       wt[wn].ti_Data = 0;
    win = OpenWindowTagList(0, wt);
    if (!win) {
        FreeGadgets(glist);
        FreeVec(nodes);
        return -1;
    }
    InitRequester(&rq);
    blocked = Request(&rq, parent) ? 1 : 0;
    GT_RefreshWindow(win, 0);
    while (!done) {
        Wait(1UL << win->UserPort->mp_SigBit);
        while (!done && (im = GT_GetIMsg(win->UserPort))) {
            ULONG cls = im->Class, sec = im->Seconds, mic = im->Micros;
            UWORD code = im->Code;
            struct Gadget *ig = (struct Gadget *)im->IAddress;
            GT_ReplyIMsg(im);
            if (cls == IDCMP_CLOSEWINDOW)
                done = 1;
            else if (cls == IDCMP_REFRESHWINDOW) {
                GT_BeginRefresh(win);
                GT_EndRefresh(win, TRUE);
            } else if (cls == IDCMP_VANILLAKEY) {
                if (code == 0x1b)
                    done = 1;
                else if (code == '\r') {
                    result = pk_result(n, sel, 1);
                    done = 1;
                }
            } else if (cls == IDCMP_GADGETUP && ig) {
                if (ig->GadgetID == PK_CANCEL)
                    done = 1;
                else if (ig->GadgetID == PK_OK) {
                    result = pk_result(n, sel, 1);
                    done = 1;
                } else if (ig->GadgetID == PK_LIST) {
                    int again = (int)code == sel && DoubleClick(lastsec, lastmic, sec, mic);
                    sel = pk_initial(n, (int)code);
                    lastsec = sec;
                    lastmic = mic;
                    if (again) {
                        result = pk_result(n, sel, 1);
                        done = 1;
                    }
                }
            }
        }
    }
    CloseWindow(win);
    if (blocked)
        EndRequest(&rq, parent);
    FreeGadgets(glist);
    FreeVec(nodes);
    return result;
}
