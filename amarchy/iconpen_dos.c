/* iconpen_dos: see iconpen_dos.h. The OS calls of the shared icon pen holder, moved out of amarchy/flat.c (L7). */
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <exec/libraries.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include "iconpen_dos.h"
#include "amconf_core.h"
#include "amconf_dos.h"
#include "gthemes_core.h"

extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;

typedef struct ipd_rec {
    struct SignalSemaphore ss;   /* first: AddSemaphore's node is the allocation's start */
    char name[20];
    ip_shared s;
} ipd_rec;

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

/* Bring the held pens in line with the owners and the Workbench screen as it is now. The public screen list stays locked for
 * the whole sync, so the screen cannot close under the colour map. The caller holds r->ss. */
static void pens_sync(ipd_rec *r)
{
    pen_ctx c;
    ip_ops o;
    struct List *pl;
    struct Screen *sc;
    int act, depth = 0;
    int hold = ip_shared_hold(&r->s, GfxBase && GfxBase->LibNode.lib_Version >= 39);
    o.obtain = pen_obtain;
    o.release = pen_release;
    o.ctx = &c;
    pl = LockPubScreenList();
    sc = wb_screen(pl);
    if (sc)
        depth = ip_screen_depth(sc->BitMap.Depth, (sc->ViewPort.Modes & EXTRA_HALFBRITE) != 0);
    c.cur = sc ? sc->ViewPort.ColorMap : 0;
    c.old = (sc && r->s.h.screen == (const void *)sc) ? c.cur : 0;   /* old pens are released only on the screen they belong to */
    act = ip_sync_decide(&r->s.h, hold, sc, depth);
    if (act == IP_SYNC_RELEASE) {
        ip_holder_release(&r->s.h, &o);
    } else if (act == IP_SYNC_OBTAIN) {
        gtheme th;
        uint8_t rgb[IP_PENS][3];
        am_prefs ap;
        amconf_load(&ap);
        gt_load(pen_theme_read, 0, ap.val[AM_GTHEME][0] ? ap.val[AM_GTHEME] : GT_DEFAULT, &th);
        ip_colours(&th, rgb);
        ip_holder_obtain(&r->s.h, &o, sc, depth, rgb);   /* releases the old ones first */
    }
    UnlockPubScreenList();
}

/* The record, created when `create`; 0 when absent (or no memory). */
static ipd_rec *find_rec(int create)
{
    ipd_rec *r;
    Forbid();
    r = (ipd_rec *)FindSemaphore((STRPTR)IPD_SEM_NAME);
    if (!r && create) {
        r = (ipd_rec *)AllocVec(sizeof *r, MEMF_PUBLIC | MEMF_CLEAR);
        if (r) {
            int i;
            for (i = 0; IPD_SEM_NAME[i] && i < (int)sizeof r->name - 1; i++)
                r->name[i] = IPD_SEM_NAME[i];
            r->ss.ss_Link.ln_Name = r->name;
            ip_shared_init(&r->s);
            AddSemaphore(&r->ss);
        }
    }
    Permit();
    return r;
}

int ipd_owner(unsigned owner, int on)
{
    ipd_rec *r = find_rec(on);
    int refs = 0;
    if (!r)
        return 0;
    ObtainSemaphore(&r->ss);
    refs = ip_shared_set(&r->s, owner, on);
    pens_sync(r);
    if (refs == 0) {                    /* last owner gone: the record goes too (pens_sync just released the pens) */
        int ok = 0;
        ReleaseSemaphore(&r->ss);
        Forbid();
        if (AttemptSemaphore(&r->ss)) {
            RemSemaphore(&r->ss);
            ReleaseSemaphore(&r->ss);
            ok = 1;
        }
        Permit();
        if (ok)
            FreeVec(r);
        return 0;
    }
    ReleaseSemaphore(&r->ss);
    return refs;
}

void ipd_resync(void)
{
    ipd_rec *r = find_rec(0);
    if (!r)
        return;
    ObtainSemaphore(&r->ss);
    pens_sync(r);
    ReleaseSemaphore(&r->ss);
}

int ipd_status_line(char *out, size_t cap)
{
    ip_shared s;
    ipd_rec *r;
    int n, w;
    ip_shared_init(&s);
    Forbid();
    r = (ipd_rec *)FindSemaphore((STRPTR)IPD_SEM_NAME);
    if (r)
        s = r->s;
    Permit();
    n = ip_status_line(&s.h, out, cap);
    if (n <= 0)
        return 0;
    w = snprintf(out + n, cap - (size_t)n, " refs=%d", ip_shared_refs(&s));
    if (w <= 0 || (size_t)(n + w) >= cap)
        return 0;
    return n + w;
}
