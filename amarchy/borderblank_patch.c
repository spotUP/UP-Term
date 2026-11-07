/* See borderblank_patch.h. The hook runs on the screen opener's task (any task, maybe a 4 KB Shell stack): no DOS
 * calls, small locals. The blank happens after the original returns, so no lock is held across the original. */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <graphics/videocontrol.h>
#include <graphics/view.h>
#include <utility/tagitem.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <string.h>
#include "borderblank_patch.h"
#include "borderblank_core.h"
#include "ipatch.h"
#include "libpatch.h"
#include "sentinel_dos.h"

extern struct IntuitionBase *IntuitionBase;

typedef struct bb_pub {
    struct SignalSemaphore ss;  /* named BB_SEM_NAME: a second Amarchy does not patch twice */
    char name[24];
    struct GfxBase *gfx;
    long blanked;               /* screens blanked over the run, for the sentinel */
    bb_table t;                 /* guarded by Forbid */
} bb_pub;

static bb_pub *g_pub;
static ipatch g_po, g_pt;       /* OpenScreen / OpenScreenTagList */

typedef struct Screen *(*openfn)(__reg("a0") struct NewScreen *, __reg("a1") struct TagItem *, __reg("a6") struct Library *);

/* 1 when scr is still in intuition's screen list. Call under LockIBase. */
static int scr_alive(const void *scr)
{
    struct Screen *s;
    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
        if ((const void *)s == scr)
            return 1;
    return 0;
}

/* Drops the entries of screens that have closed since, so the table holds only open screens. */
static void prune(bb_pub *p)
{
    int i;
    ULONG il = LockIBase(0);
    Forbid();
    for (i = p->t.n - 1; i >= 0; i--)
        if (!scr_alive(p->t.scr[i]))
            bb_drop(&p->t, p->t.scr[i]);
    Permit();
    UnlockIBase(il);
}

/* Blanks scr's border when it is native and nobody blanked it yet. 1 = changed (the caller remakes the display). */
static int blank_one(bb_pub *p, struct Screen *scr)
{
    struct GfxBase *GfxBase = p->gfx;
    struct TagItem get[2], set[2];
    int known;
    if (!scr || !scr->ViewPort.ColorMap || !bb_native_mode((unsigned long)GetVPModeID(&scr->ViewPort)))
        return 0;
    Forbid();
    known = bb_has(&p->t, scr);
    Permit();
    if (known)
        return 0;               /* OpenScreen may reach OpenScreenTagList: the inner hook did it */
    get[0].ti_Tag = VTAG_BORDERBLANK_GET; get[0].ti_Data = 0;
    get[1].ti_Tag = TAG_DONE;
    VideoControl(scr->ViewPort.ColorMap, get);
    if (get[0].ti_Data)
        return 0;               /* the program blanked it itself: Off must leave it so */
    set[0].ti_Tag = VTAG_BORDERBLANK_SET; set[0].ti_Data = 0;
    set[1].ti_Tag = TAG_DONE;
    if (VideoControl(scr->ViewPort.ColorMap, set))
        return 0;               /* nonzero = a tag the chipset or graphics does not know */
    Forbid();
    if (p->t.n == BB_MAX)
        Permit(), prune(p), Forbid();
    bb_add(&p->t, scr);
    p->blanked++;
    Permit();
    MakeScreen(scr);
    return 1;
}

static struct Screen *after_open(bb_pub *p, struct Screen *scr)
{
    if (scr && blank_one(p, scr))
        RethinkDisplay();
    return scr;
}

static struct Screen *tags_hook(__reg("a0") struct NewScreen *ns, __reg("a1") struct TagItem *tags, __reg("a6") struct Library *ib)
{
    bb_pub *p = g_pub;
    struct Screen *s;
    s = after_open(p, ((openfn)ipatch_saved(&g_pt))(ns, tags, ib));
    ipatch_leave(&g_pt);
    return s;
}

static struct Screen *plain_hook(__reg("a0") struct NewScreen *ns, __reg("a1") struct TagItem *tags, __reg("a6") struct Library *ib)
{
    bb_pub *p = g_pub;
    struct Screen *s;
    s = after_open(p, ((openfn)ipatch_saved(&g_po))(ns, tags, ib));
    ipatch_leave(&g_po);
    return s;
}

/* Blanks every open native screen. Collects under LockIBase, blanks after (VideoControl / MakeScreen outside it). */
static void blank_open_screens(bb_pub *p)
{
    struct Screen *list[BB_MAX], *s;
    int n = 0, i, changed = 0;
    ULONG il = LockIBase(0);
    for (s = IntuitionBase->FirstScreen; s && n < BB_MAX; s = s->NextScreen)
        list[n++] = s;
    UnlockIBase(il);
    for (i = 0; i < n; i++) {
        il = LockIBase(0);
        s = scr_alive(list[i]) ? list[i] : 0;
        UnlockIBase(il);
        changed |= blank_one(p, s);
    }
    if (changed)
        RethinkDisplay();
}

int borderblank_on(void)
{
    return g_pub != 0;
}

int borderblank_install(void)
{
    int rc1, rc2;
    struct GfxBase *gfx;
    if (g_pub)
        return 0;
    if (!IntuitionBase)
        return -1;
    gfx = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    if (!gfx)
        return -1;
    rc1 = ipatch_alloc(&g_po, BB_LVO_OPENSCREEN);
    if (rc1) {
        CloseLibrary((struct Library *)gfx);
        return rc1;
    }
    rc1 = ipatch_alloc(&g_pt, BB_LVO_OPENSCREENTAGLIST);
    if (rc1) {
        ipatch_forget(&g_po);
        CloseLibrary((struct Library *)gfx);
        return rc1;
    }
    Forbid();
    rc1 = FindSemaphore((STRPTR)BB_SEM_NAME) != 0;
    Permit();
    g_pub = rc1 ? 0 : (bb_pub *)AllocMem(sizeof *g_pub, MEMF_PUBLIC | MEMF_CLEAR);
    if (!g_pub) {
        ipatch_forget(&g_po);
        ipatch_forget(&g_pt);
        CloseLibrary((struct Library *)gfx);
        return rc1 ? -3 : -4;
    }
    g_pub->gfx = gfx;
    bb_init(&g_pub->t);
    strcpy(g_pub->name, BB_SEM_NAME);
    g_pub->ss.ss_Link.ln_Name = g_pub->name;
    AddSemaphore(&g_pub->ss);
    Forbid();
    rc1 = ipatch_arm(&g_po, (void *)plain_hook);
    rc2 = ipatch_arm(&g_pt, (void *)tags_hook);
    libpatch_done((struct Library *)IntuitionBase);
    Permit();
    if (rc1 != FLAT_OK || rc2 != FLAT_OK) {
        borderblank_remove();
        return -5;
    }
    blank_open_screens(g_pub);
    am_sentinel("AMARCHY_BORDERBLANK_INSTALLED", 1);
    am_sentinel("AMARCHY_BORDERBLANK_SCREENS", g_pub->blanked);
    return 0;
}

/* Clears the border blank on every recorded screen that is still open. */
static void clear_recorded(bb_pub *p)
{
    struct GfxBase *GfxBase = p->gfx;
    struct TagItem clr[2];
    const void *list[BB_MAX];
    int n, i, changed = 0;
    prune(p);
    Forbid();
    n = p->t.n;
    memcpy(list, p->t.scr, sizeof list);
    bb_init(&p->t);
    Permit();
    for (i = 0; i < n; i++) {
        ULONG il = LockIBase(0);
        int alive = scr_alive(list[i]);
        UnlockIBase(il);
        if (!alive)
            continue;
        clr[0].ti_Tag = VTAG_BORDERBLANK_CLR; clr[0].ti_Data = 0;
        clr[1].ti_Tag = TAG_DONE;
        VideoControl(((struct Screen *)list[i])->ViewPort.ColorMap, clr);
        MakeScreen((struct Screen *)list[i]);
        changed = 1;
    }
    if (changed)
        RethinkDisplay();
}

void borderblank_remove(void)
{
    int rc1, rc2, i;
    if (!g_pub)
        return;
    Forbid();
    ipatch_passthru(&g_po);
    ipatch_passthru(&g_pt);
    rc1 = ipatch_unarm(&g_po);
    rc2 = ipatch_unarm(&g_pt);
    libpatch_done((struct Library *)IntuitionBase);
    RemSemaphore(&g_pub->ss);
    Permit();
    Delay(5);
    for (i = 0; ipatch_busy(&g_po) + ipatch_busy(&g_pt); i++) {
        if (i && i % 50 == 0)          /* ~10 s a round: a hook that never returns stays visible on the rig */
            am_sentinel("AMARCHY_BORDERBLANK_BUSY_WAIT", i / 5);
        Delay(10);                      /* a caller is inside: stay resident, a Guru under a live hook is worse */
    }
    clear_recorded(g_pub);
    am_sentinel("AMARCHY_BORDERBLANK_INSTALLED", 0);
    ipatch_forget(&g_po);
    ipatch_forget(&g_pt);
    CloseLibrary((struct Library *)g_pub->gfx);
    FreeMem(g_pub, sizeof *g_pub);
    g_pub = 0;
}
