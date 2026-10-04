/* sbar_gad: the scroll bar in a window's right border; see sbar_gad.h.
 * The layout is the one Workbench windows use: under the title bar the
 * knob's track, then the up and the down arrow, then the sizing gadget,
 * all as wide as the sizing gadget. */
#include "sbar_gad.h"
#include <string.h>
#include <intuition/gadgetclass.h>
#include <intuition/imageclass.h>
#include <intuition/icclass.h>
#include <intuition/screens.h>
#include <utility/tagitem.h>
#include <proto/intuition.h>

int sbar_gad_fits(const struct Window *win)
{
    return win && (win->Flags & WFLG_SIZEGADGET) && (win->Flags & WFLG_SIZEBRIGHT) &&
           !(win->Flags & WFLG_BORDERLESS);
}

/* a system image of the screen's size class */
static Object *sys_image(struct DrawInfo *dri, ULONG which, ULONG size)
{
    struct TagItem t[4];
    t[0].ti_Tag = SYSIA_DrawInfo; t[0].ti_Data = (ULONG)dri;
    t[1].ti_Tag = SYSIA_Which;    t[1].ti_Data = which;
    t[2].ti_Tag = SYSIA_Size;     t[2].ti_Data = size;
    t[3].ti_Tag = TAG_DONE;       t[3].ti_Data = 0;
    return NewObjectA(0, (UBYTE *)SYSICLASS, t);
}

static ULONG attr(Object *o, ULONG id)
{
    ULONG v = 0;
    GetAttr(id, o, &v);
    return v;
}

static struct Gadget *arrow(Object *img, struct Gadget *prev, ULONG id, LONG relright, LONG relbottom,
                            ULONG w, ULONG h)
{
    struct TagItem t[10];
    t[0].ti_Tag = GA_Image;       t[0].ti_Data = (ULONG)img;
    t[1].ti_Tag = GA_RelRight;    t[1].ti_Data = (ULONG)relright;
    t[2].ti_Tag = GA_RelBottom;   t[2].ti_Data = (ULONG)relbottom;
    t[3].ti_Tag = GA_Width;       t[3].ti_Data = w;
    t[4].ti_Tag = GA_Height;      t[4].ti_Data = h;
    t[5].ti_Tag = GA_RightBorder; t[5].ti_Data = TRUE;
    t[6].ti_Tag = GA_Previous;    t[6].ti_Data = (ULONG)prev;
    t[7].ti_Tag = GA_ID;          t[7].ti_Data = id;
    t[8].ti_Tag = ICA_TARGET;     t[8].ti_Data = (ULONG)ICTARGET_IDCMP;
    t[9].ti_Tag = TAG_DONE;       t[9].ti_Data = 0;
    return (struct Gadget *)NewObjectA(0, (UBYTE *)BUTTONGCLASS, t);
}

static void free_parts(sbar_gad *g, struct Screen *scr)
{
    if (g->down)
        DisposeObject((Object *)g->down);
    if (g->up)
        DisposeObject((Object *)g->up);
    if (g->prop)
        DisposeObject((Object *)g->prop);
    if (g->down_img)
        DisposeObject(g->down_img);
    if (g->up_img)
        DisposeObject(g->up_img);
    if (g->dri)
        FreeScreenDrawInfo(scr, g->dri);
    memset(g, 0, sizeof(*g));
}

int sbar_gad_open(sbar_gad *g, struct Window *win)
{
    struct Screen *scr;
    Object *size_img;
    ULONG size, sw, sh, uh, dh;
    struct TagItem t[16];
    if (g->win)
        return 1;
    memset(g, 0, sizeof(*g));
    if (!sbar_gad_fits(win))
        return 0;
    scr = win->WScreen;
    if (!(g->dri = GetScreenDrawInfo(scr)))
        return 0;
    size = (scr->Flags & SCREENHIRES) ? SYSISIZE_MEDRES : SYSISIZE_LOWRES;
    /* the sizing gadget's size: the border's width, the arrows' column */
    size_img = sys_image(g->dri, SIZEIMAGE, size);
    g->up_img = sys_image(g->dri, UPIMAGE, size);
    g->down_img = sys_image(g->dri, DOWNIMAGE, size);
    if (!size_img || !g->up_img || !g->down_img) {
        if (size_img)
            DisposeObject(size_img);
        free_parts(g, scr);
        return 0;
    }
    sw = attr(size_img, IA_Width);
    sh = attr(size_img, IA_Height);
    DisposeObject(size_img);
    uh = attr(g->up_img, IA_Height);
    dh = attr(g->down_img, IA_Height);
    if (sw < 10 || (WORD)sw > win->BorderRight) {
        free_parts(g, scr); /* a border narrower than the gadgets: text would be under them */
        return 0;
    }
    /* the track: 4 pixels in from either side of the border, from under
     * the title bar to the up arrow */
    t[0].ti_Tag = GA_ID;          t[0].ti_Data = SBAR_GID_PROP;
    t[1].ti_Tag = GA_RelRight;    t[1].ti_Data = (ULONG)(5 - (LONG)sw);
    t[2].ti_Tag = GA_Top;         t[2].ti_Data = (ULONG)(win->BorderTop + 1);
    t[3].ti_Tag = GA_Width;       t[3].ti_Data = sw - 8;
    t[4].ti_Tag = GA_RelHeight;   t[4].ti_Data = (ULONG)(-(LONG)(win->BorderTop + sh + uh + dh + 2));
    t[5].ti_Tag = GA_RightBorder; t[5].ti_Data = TRUE;
    t[6].ti_Tag = ICA_TARGET;     t[6].ti_Data = (ULONG)ICTARGET_IDCMP;
    t[7].ti_Tag = PGA_Freedom;    t[7].ti_Data = FREEVERT;
    t[8].ti_Tag = PGA_NewLook;    t[8].ti_Data = TRUE;
    t[9].ti_Tag = PGA_Borderless; t[9].ti_Data = TRUE;
    t[10].ti_Tag = PGA_Total;     t[10].ti_Data = 1;
    t[11].ti_Tag = PGA_Visible;   t[11].ti_Data = 1;
    t[12].ti_Tag = PGA_Top;       t[12].ti_Data = 0;
    t[13].ti_Tag = TAG_DONE;      t[13].ti_Data = 0;
    g->prop = (struct Gadget *)NewObjectA(0, (UBYTE *)PROPGCLASS, t);
    if (g->prop)
        g->up = arrow(g->up_img, g->prop, SBAR_GID_UP, 1 - (LONG)sw, 1 - (LONG)(sh + dh + uh), sw, uh);
    if (g->up)
        g->down = arrow(g->down_img, g->up, SBAR_GID_DOWN, 1 - (LONG)sw, 1 - (LONG)(sh + dh), sw, dh);
    if (!g->down) {
        free_parts(g, scr);
        return 0;
    }
    /* the knob reports to our IDCMP port */
    if (!(win->IDCMPFlags & IDCMP_IDCMPUPDATE) && !ModifyIDCMP(win, win->IDCMPFlags | IDCMP_IDCMPUPDATE)) {
        free_parts(g, scr);
        return 0;
    }
    AddGList(win, g->prop, (UWORD)~0, 3, 0);
    RefreshGList(g->prop, win, 0, 3);
    g->win = win;
    return 1;
}

void sbar_gad_close(sbar_gad *g)
{
    struct Window *win = g->win;
    if (!win)
        return;
    RemoveGList(win, g->prop, 3);
    free_parts(g, win->WScreen);
    /* the border where they were, as the window draws it */
    RefreshWindowFrame(win);
}

void sbar_gad_set(sbar_gad *g, const sbar_knob *k)
{
    struct TagItem t[4];
    if (!g->win || (g->shown_valid && sbar_equal(&g->shown, k)))
        return;
    t[0].ti_Tag = PGA_Total;   t[0].ti_Data = k->total;
    t[1].ti_Tag = PGA_Visible; t[1].ti_Data = k->visible;
    t[2].ti_Tag = PGA_Top;     t[2].ti_Data = k->top;
    t[3].ti_Tag = TAG_DONE;    t[3].ti_Data = 0;
    SetGadgetAttrsA(g->prop, g->win, 0, t);
    g->shown = *k;
    g->shown_valid = 1;
}

int sbar_gad_event(const struct IntuiMessage *im, ULONG *id, ULONG *top)
{
    const struct TagItem *t = (const struct TagItem *)im->IAddress;
    int have_id = 0;
    *id = 0;
    *top = (ULONG)~0;
    if (im->Class != IDCMP_IDCMPUPDATE)
        return 0;
    /* GetTagData would need utility.library: the list read by hand */
    while (t) {
        if (t->ti_Tag == TAG_DONE)
            break;
        if (t->ti_Tag == TAG_MORE) {
            t = (const struct TagItem *)t->ti_Data;
            continue;
        }
        if (t->ti_Tag == TAG_SKIP) {
            t += 1 + t->ti_Data;
            continue;
        }
        if (t->ti_Tag == GA_ID) {
            *id = t->ti_Data;
            have_id = 1;
        } else if (t->ti_Tag == PGA_Top)
            *top = t->ti_Data;
        t++;
    }
    return have_id && (*id == SBAR_GID_PROP || *id == SBAR_GID_UP || *id == SBAR_GID_DOWN);
}
