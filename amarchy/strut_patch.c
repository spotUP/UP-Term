/* See strut_patch.h. The hook runs on the caller's task (any task, maybe a 4 KB Shell stack): no DOS calls, small
 * locals, no lock held across the original. It never writes the caller's NewWindow or tag list. */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <utility/tagitem.h>
#include <proto/intuition.h>
#include <string.h>
#include "strut_patch.h"
#include "ipatch.h"
#include "libpatch.h"
#include "win_core.h"
#include "bar_core.h"
#include "sentinel_dos.h"
#include "iconpen_dos.h"

extern struct IntuitionBase *IntuitionBase;

#define DESK_MAX 4
#define REFLOW_MAX 32           /* bordered windows moved out of the strip, kept for BAR OFF */
#define STRUT_INJECT_ZOOM 0     /* probe Z0 (s28-evidence/bar-strip-reserve.md): the default alternate box is 60x40 at the window top-left, never in the strip */

typedef struct strut_pub {
    struct SignalSemaphore ss;
    char name[20];
    const struct Screen *scr;   /* only compared, never dereferenced */
    int sw, sh, title, strip;   /* screen size, title bar height, strip height; strip 0 = nothing published */
    long busy;
    long opens, clamped, zoom_set;
    int nd;
    struct { struct Window *w; int h; } d[DESK_MAX];
    int nr;
    struct { struct Window *w; win_box old, set; } r[REFLOW_MAX];
} strut_pub;

static strut_pub *g_pub;
static ipatch g_pw, g_pt;       /* OpenWindow / OpenWindowTagList */
static long g_last[5];

typedef struct req {
    const struct Screen *scr;   /* cust or pub screen given; 0 = by name / default */
    const char *name;
    int wbench;                 /* NewWindow type WBENCHSCREEN */
    long left, top, width, height, minh;
    long flags;
    int inner;                  /* height is WA_InnerHeight */
    WORD *zoom;
} req;

typedef struct Window *(*openfn)(__reg("a0") struct NewWindow *, __reg("a1") struct TagItem *, __reg("a6") struct Library *);

static struct TagItem *tag_next(struct TagItem **p)
{
    struct TagItem *t;
    while ((t = *p) != 0) {
        switch (t->ti_Tag) {
        case TAG_DONE: return 0;
        case TAG_IGNORE: *p = t + 1; break;
        case TAG_MORE: *p = (struct TagItem *)t->ti_Data; break;
        case TAG_SKIP: *p = t + 1 + t->ti_Data; break;
        default: *p = t + 1; return t;
        }
    }
    return 0;
}

static void set_flag(long *f, long bit, ULONG on)
{
    if (on)
        *f |= bit;
    else
        *f &= ~bit;
}

static void parse(req *r, const struct NewWindow *nw, struct TagItem *tags)
{
    struct TagItem *t;
    memset(r, 0, sizeof *r);
    if (nw) {
        r->left = nw->LeftEdge; r->top = nw->TopEdge; r->width = nw->Width; r->height = nw->Height;
        r->minh = nw->MinHeight; r->flags = nw->Flags;
        if ((nw->Type & SCREENTYPE) == CUSTOMSCREEN)
            r->scr = nw->Screen;
        else if ((nw->Type & SCREENTYPE) == WBENCHSCREEN)
            r->wbench = 1;
    }
    while ((t = tag_next(&tags)) != 0)
        switch (t->ti_Tag) {
        case WA_Left: r->left = (long)t->ti_Data; break;
        case WA_Top: r->top = (long)t->ti_Data; break;
        case WA_Width: r->width = (long)t->ti_Data; break;
        case WA_Height: r->height = (long)t->ti_Data; r->inner = 0; break;
        case WA_InnerHeight: r->height = (long)t->ti_Data; r->inner = 1; break;
        case WA_MinHeight: r->minh = (long)t->ti_Data; break;
        case WA_Flags: r->flags = (long)t->ti_Data; break;
        case WA_Backdrop: set_flag(&r->flags, WFLG_BACKDROP, t->ti_Data); break;
        case WA_Borderless: set_flag(&r->flags, WFLG_BORDERLESS, t->ti_Data); break;
        case WA_DragBar: set_flag(&r->flags, WFLG_DRAGBAR, t->ti_Data); break;
        case WA_SizeGadget: set_flag(&r->flags, WFLG_SIZEGADGET, t->ti_Data); break;
        case WA_CustomScreen: case WA_PubScreen: r->scr = (const struct Screen *)t->ti_Data; break;
        case WA_PubScreenName: r->name = (const char *)t->ti_Data; break;
        case WA_Zoom: r->zoom = (WORD *)t->ti_Data; break;
        default: break;
        }
}

/* 1 when the request opens on the bar's screen. A name or default is resolved by locking the public screen. */
static int on_bar_screen(const strut_pub *p, const req *r)
{
    struct Screen *s;
    int same;
    if (r->scr)
        return r->scr == p->scr;
    /* The Workbench by flag or name is looked up, never opened (BOOT-31-WB-DEADLOCK). Another name or the default screen is the
     * caller's own OpenWindow target: the system opens it for that caller anyway, so locking it here adds no new open. */
    if (r->wbench || (r->name && !strcmp(r->name, "Workbench")))
        s = ipd_lock_wb_screen();
    else
        s = LockPubScreen((STRPTR)r->name);
    if (!s)
        return 0;
    same = s == p->scr;
    UnlockPubScreen(0, s);
    return same;
}

typedef struct plan {
    win_strut_cls cls;
    win_box box;
    win_screen scr;
    WORD zw[4];
    int zoom;                   /* 1: give WA_Zoom = zw */
    int change;                 /* 1: the box (top / height) differs */
} plan;

static int make_plan(const strut_pub *p, const req *r, plan *pl)
{
    win_strut_req q;
    memset(&q, 0, sizeof q);
    pl->scr.width = p->sw; pl->scr.height = p->sh; pl->scr.title = p->title; pl->scr.bottom = p->strip;
    q.box.left = (int)r->left; q.box.top = (int)r->top; q.box.width = (int)r->width;
    q.box.height = (int)(r->inner ? r->height + p->title + 2 : r->height);   /* inner: a border estimate; MoveWindow fixes the rest */
    q.box.minh = (int)r->minh;
    q.backdrop = (r->flags & WFLG_BACKDROP) != 0;
    q.borderless = (r->flags & WFLG_BORDERLESS) != 0;
    q.dragbar = (r->flags & WFLG_DRAGBAR) != 0;
    q.sizegad = (r->flags & WFLG_SIZEGADGET) != 0;
    q.desktop = bar_is_desktop((unsigned long)r->flags, (int)r->left, (int)(r->left + r->width - 1), p->sw);
    pl->box = q.box;
    pl->cls = win_strut_class(&q, &pl->scr);
    pl->change = win_strut_clamp(pl->cls, &pl->box, &pl->scr);
    pl->zoom = 0;
    if (pl->cls == WSC_BORDERED && (q.sizegad || r->zoom)) {
        win_box z;
        if (r->zoom) {
            z.left = r->zoom[0]; z.top = r->zoom[1]; z.width = r->zoom[2]; z.height = r->zoom[3];
        } else if (STRUT_INJECT_ZOOM) {
            win_usable(&pl->scr, &z);
            z.minh = 1;
        } else
            return pl->change;
        z.minh = 1;
        if (!r->zoom || win_strut_clamp(WSC_BORDERED, &z, &pl->scr)) {
            pl->zw[0] = (WORD)z.left; pl->zw[1] = (WORD)z.top; pl->zw[2] = (WORD)z.width; pl->zw[3] = (WORD)z.height;
            pl->zoom = 1;
        }
    }
    return pl->change || pl->zoom;
}

/* After the open: the border estimate of an InnerHeight request (or an unknown box) may leave the window in the strip. */
static void after_open(const strut_pub *p, const plan *pl, struct Window *w)
{
    int ub = p->sh - p->strip, nt;
    if (!w || pl->cls != WSC_BORDERED || w->TopEdge + w->Height <= ub)
        return;
    nt = ub - w->Height;
    if (nt < p->title)
        nt = p->title;
    if (nt < w->TopEdge) {
        MoveWindow(w, 0, nt - w->TopEdge);
        g_pub->clamped++;
    }
}

static struct TagItem *edited_tags(struct TagItem *tags, const plan *pl, const req *r)
{
    struct TagItem *src = tags, *t, *out, *o;
    int n = 0, top = 0, height = 0, zoom = 0;
    while (tag_next(&src))
        n++;
    out = (struct TagItem *)AllocVec((ULONG)(n + 4) * sizeof *out, MEMF_PUBLIC);
    if (!out)
        return 0;
    o = out;
    src = tags;
    while ((t = tag_next(&src)) != 0) {
        *o = *t;
        if (t->ti_Tag == WA_Top) { o->ti_Data = (ULONG)pl->box.top; top = 1; }
        else if (t->ti_Tag == WA_Height && !r->inner) { o->ti_Data = (ULONG)pl->box.height; height = 1; }
        else if (t->ti_Tag == WA_Zoom && pl->zoom) { o->ti_Data = (ULONG)pl->zw; zoom = 1; }
        o++;
    }
    if (!top && pl->change) { o->ti_Tag = WA_Top; o->ti_Data = (ULONG)pl->box.top; o++; }
    if (!height && pl->change && !r->inner) { o->ti_Tag = WA_Height; o->ti_Data = (ULONG)pl->box.height; o++; }
    if (!zoom && pl->zoom) { o->ti_Tag = WA_Zoom; o->ti_Data = (ULONG)pl->zw; o++; }
    o->ti_Tag = TAG_DONE;
    return out;
}

static struct Window *tags_hook(__reg("a0") struct NewWindow *nw, __reg("a1") struct TagItem *tags, __reg("a6") struct Library *ib)
{
    strut_pub *p = g_pub;
    struct Window *w;
    req r;
    plan pl;
    struct TagItem *mine = 0;
    int act = 0, planned = 0;
    Forbid();
    p->busy++;
    Permit();
    parse(&r, nw, tags);
    if (p->strip > 0 && on_bar_screen(p, &r)) {
        p->opens++;
        act = make_plan(p, &r, &pl);
        planned = 1;
        if (act)
            mine = edited_tags(tags, &pl, &r);
    }
    w = ((openfn)ipatch_saved(&g_pt))(nw, mine ? mine : tags, ib);
    if (mine) {
        FreeVec(mine);
        p->clamped += pl.change != 0;
        p->zoom_set += pl.zoom != 0;
    }
    if (planned && w && r.inner)
        after_open(p, &pl, w);
    Forbid();
    p->busy--;
    Permit();
    return w;
}

static struct Window *plain_hook(__reg("a0") struct NewWindow *nw, __reg("a1") struct TagItem *tags, __reg("a6") struct Library *ib)
{
    strut_pub *p = g_pub;
    struct Window *w;
    struct NewWindow copy;
    req r;
    plan pl;
    Forbid();
    p->busy++;
    Permit();
    parse(&r, nw, 0);
    if (nw && p->strip > 0 && on_bar_screen(p, &r)) {
        p->opens++;
        if (make_plan(p, &r, &pl) && pl.change) {
            copy = *nw;
            copy.TopEdge = (WORD)pl.box.top;
            copy.Height = (WORD)pl.box.height;
            p->clamped++;
            nw = &copy;
        }
    }
    w = ((openfn)ipatch_saved(&g_pw))(nw, tags, ib);
    Forbid();
    p->busy--;
    Permit();
    return w;
}

int strut_patch_install(void)
{
    int rc1, rc2;
    if (!IntuitionBase)
        return -1;
    rc1 = ipatch_alloc(&g_pw, STRUT_LVO_OPENWINDOW);
    if (rc1)
        return rc1;
    rc1 = ipatch_alloc(&g_pt, STRUT_LVO_OPENWINDOWTAGLIST);
    if (rc1) {
        ipatch_free(&g_pw, 0);
        return rc1;
    }
    Forbid();
    rc1 = FindSemaphore((STRPTR)STRUT_SEM_NAME) != 0;
    Permit();
    g_pub = rc1 ? 0 : (strut_pub *)AllocMem(sizeof *g_pub, MEMF_PUBLIC | MEMF_CLEAR);
    if (!g_pub) {
        ipatch_free(&g_pw, 0);
        ipatch_free(&g_pt, 0);
        return rc1 ? -3 : -4;
    }
    strcpy(g_pub->name, STRUT_SEM_NAME);
    g_pub->ss.ss_Link.ln_Name = g_pub->name;
    AddSemaphore(&g_pub->ss);
    Forbid();
    rc1 = ipatch_arm(&g_pw, (void *)plain_hook);
    rc2 = ipatch_arm(&g_pt, (void *)tags_hook);
    libpatch_done((struct Library *)IntuitionBase);
    Permit();
    if (rc1 != FLAT_OK || rc2 != FLAT_OK) {
        strut_patch_remove();
        return -5;
    }
    am_sentinel("AMARCHY_STRUT_INSTALLED", 1);
    am_sentinel("AMARCHY_STRUT_OPENS", 0);          /* the counters start at 0 for this run */
    am_sentinel("AMARCHY_STRUT_CLAMPED", 0);
    am_sentinel("AMARCHY_STRUT_ZOOM", 0);
    am_sentinel("AMARCHY_STRUT_DESK", 0);
    return 0;
}

void strut_patch_remove(void)
{
    int rc1, rc2, i;
    if (!g_pub)
        return;
    strut_clear();
    Forbid();
    ipatch_passthru(&g_pw);
    ipatch_passthru(&g_pt);
    rc1 = ipatch_unarm(&g_pw);
    rc2 = ipatch_unarm(&g_pt);
    libpatch_done((struct Library *)IntuitionBase);
    RemSemaphore(&g_pub->ss);
    Permit();
    Delay(5);
    for (i = 0; i < 300 && g_pub->busy; i++)
        Delay(2);
    am_sentinel("AMARCHY_STRUT_INSTALLED", 0);
    if (g_pub->busy)
        return;                     /* a caller is still inside: leak rather than free under it */
    ipatch_free(&g_pw, rc1);
    ipatch_free(&g_pt, rc2);
    FreeMem(g_pub, sizeof *g_pub);
    g_pub = 0;
}

/* The desktop windows of scr (W2): the Workbench root window and a backdrop desktop, never Amarchy's own. */
void strut_publish(const struct Screen *scr, int height)
{
    struct Window *w, *list[DESK_MAX];
    int n = 0, i, ub;
    if (!g_pub || !scr || height <= 0)
        return;
    Forbid();
    g_pub->sw = scr->Width; g_pub->sh = scr->Height; g_pub->title = scr->BarHeight + 1; g_pub->strip = height;
    g_pub->scr = scr;
    Permit();
    ub = g_pub->sh - height;
    {
        ULONG il = LockIBase(0);
        for (w = scr->FirstWindow; w && n < DESK_MAX; w = w->NextWindow)
            if (!(w->Title && win_is_amarchy((const char *)w->Title)) &&
                bar_is_desktop((unsigned long)w->Flags, w->LeftEdge, w->LeftEdge + w->Width - 1, scr->Width) &&
                w->TopEdge + w->Height > ub && w->Height < g_pub->sh)
                list[n++] = w;
        UnlockIBase(il);
    }
    for (i = 0; i < n; i++) {
        int oh = list[i]->Height;
        ChangeWindowBox(list[i], list[i]->LeftEdge, list[i]->TopEdge, list[i]->Width, ub - list[i]->TopEdge);
        g_pub->d[g_pub->nd].w = list[i];
        g_pub->d[g_pub->nd].h = oh;
        g_pub->nd++;
    }
    /* Bordered windows already open (before the bar, across BAR OFF / ON, after a reset) reflow by the open-time rule. */
    n = 0;
    {
        struct Window *lst[REFLOW_MAX];
        win_screen ws;
        ws.width = g_pub->sw; ws.height = g_pub->sh; ws.title = g_pub->title; ws.bottom = height;
        {
            ULONG il = LockIBase(0);
            for (w = scr->FirstWindow; w && n < REFLOW_MAX; w = w->NextWindow)
                if (!(w->Title && win_is_amarchy((const char *)w->Title)) &&
                    !bar_is_desktop((unsigned long)w->Flags, w->LeftEdge, w->LeftEdge + w->Width - 1, scr->Width) &&
                    w->TopEdge + w->Height > ub)
                    lst[n++] = w;
            UnlockIBase(il);
        }
        for (i = 0; i < n; i++) {
            win_strut_req q;
            win_box b;
            memset(&q, 0, sizeof q);
            q.box.left = lst[i]->LeftEdge; q.box.top = lst[i]->TopEdge;
            q.box.width = lst[i]->Width; q.box.height = lst[i]->Height;
            q.box.minh = lst[i]->MinHeight;
            q.backdrop = (lst[i]->Flags & WFLG_BACKDROP) != 0;
            q.borderless = (lst[i]->Flags & WFLG_BORDERLESS) != 0;
            q.dragbar = (lst[i]->Flags & WFLG_DRAGBAR) != 0;
            q.sizegad = (lst[i]->Flags & WFLG_SIZEGADGET) != 0;
            b = q.box;
            if (win_strut_reflow(&q, &b, &ws) && g_pub->nr < REFLOW_MAX) {
                g_pub->r[g_pub->nr].w = lst[i];
                g_pub->r[g_pub->nr].old = q.box;
                g_pub->r[g_pub->nr].set = b;
                g_pub->nr++;
                ChangeWindowBox(lst[i], b.left, b.top, b.width, b.height);
                g_pub->clamped++;
            }
        }
    }
}

void strut_clear(void)
{
    int i;
    const struct Screen *rscr = 0;
    struct Window *w;
    if (!g_pub || !g_pub->strip)
        return;
    {
        const struct Screen *scr = g_pub->scr;
        rscr = scr;
        Forbid();
        g_pub->strip = 0;                   /* the hook passes everything from now on */
        g_pub->scr = 0;
        Permit();
        for (i = 0; i < g_pub->nd; i++) {
            ULONG il = LockIBase(0);
            for (w = scr->FirstWindow; w && w != g_pub->d[i].w; w = w->NextWindow)
                ;
            if (w)
                ChangeWindowBox(w, w->LeftEdge, w->TopEdge, w->Width, g_pub->d[i].h);
            UnlockIBase(il);
        }
    }
    g_pub->nd = 0;
    {
        const struct Screen *scr = rscr;
        for (i = 0; i < g_pub->nr; i++) {
            /* Find the window and decide under the lock; move it after
             * UnlockIBase, the same order strut_publish uses. */
            int go = 0;
            win_box out;
            ULONG il = LockIBase(0);
            for (w = scr->FirstWindow; w && w != g_pub->r[i].w; w = w->NextWindow)
                ;
            if (w) {
                win_box now;
                memset(&now, 0, sizeof now);
                now.left = w->LeftEdge; now.top = w->TopEdge; now.width = w->Width; now.height = w->Height;
                go = win_strut_restore(&now, &g_pub->r[i].set, &g_pub->r[i].old, &out);
            }
            UnlockIBase(il);
            if (go)
                ChangeWindowBox(w, out.left, out.top, out.width, out.height);
        }
        g_pub->nr = 0;
    }
}

void strut_counts(long *opens, long *clamped, long *zoom)
{
    *opens = g_pub ? g_pub->opens : 0;
    *clamped = g_pub ? g_pub->clamped : 0;
    *zoom = g_pub ? g_pub->zoom_set : 0;
}

void strut_sentinels(void)
{
    long v[5];
    if (!g_pub)
        return;
    v[0] = g_pub->opens; v[1] = g_pub->clamped; v[2] = g_pub->zoom_set; v[3] = g_pub->nd; v[4] = 0;
    if (v[0] != g_last[0]) am_sentinel("AMARCHY_STRUT_OPENS", v[0]);
    if (v[1] != g_last[1]) am_sentinel("AMARCHY_STRUT_CLAMPED", v[1]);
    if (v[2] != g_last[2]) am_sentinel("AMARCHY_STRUT_ZOOM", v[2]);
    if (v[3] != g_last[3]) am_sentinel("AMARCHY_STRUT_DESK", v[3]);
    memcpy(g_last, v, sizeof v);
}
