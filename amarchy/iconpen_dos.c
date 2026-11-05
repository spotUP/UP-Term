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
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include "iconpen_dos.h"
#include "amconf_core.h"
#include "amconf_dos.h"
#include "gthemes_core.h"
#include "desk_core.h"

extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;

typedef struct ipd_rec {
    struct SignalSemaphore ss;   /* first: AddSemaphore's node is the allocation's start */
    char name[20];
    ip_shared s;
} ipd_rec;

/* cur: the Workbench screen's colour map now (0 = Workbench closed); vp its viewport; h the holder, sc the screen now: a held pen
 * is released only while h->screen is sc (else its screen is gone and the pens went with it; ip_holder_verify forgets the pens
 * of a screen reopened at the same address before any release) */
/* t/flat/mask: the theme, read from file before the public screen list is locked (FLAT-BOOT-FREEZE, see pens_theme_cached). */
typedef struct pen_ctx {
    struct ColorMap *cur; struct ViewPort *vp; const ip_holder *h; const void *sc;
    gtheme t; int flat; unsigned mask; int have;
} pen_ctx;

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
    if (c->cur && c->h->screen == c->sc)
        ReleasePen(c->cur, (ULONG)pen);
}

/* The ColorMap's own answer: an exclusive obtain of a pen anyone holds fails; one that succeeds is handed straight back. */
int ipd_pen_is_free(struct ColorMap *cm, int pen)
{
    if (!cm)
        return 0;
    if ((LONG)ObtainPen(cm, (ULONG)pen, 0, 0, 0, PENF_EXCLUSIVE | PENF_NO_SETCOLOR) != (LONG)pen)
        return 0;
    ReleasePen(cm, (ULONG)pen);
    return 1;
}

static int pen_is_free(void *ctx, int pen)
{
    return ipd_pen_is_free(((pen_ctx *)ctx)->cur, pen);
}

static void pen_set_rgb(void *ctx, int pen, uc_u32 rgb)
{
    pen_ctx *c = (pen_ctx *)ctx;
    if (c->vp)
        SetRGB32(c->vp, (ULONG)pen, ip_scale32((unsigned)(rgb >> 16)), ip_scale32((unsigned)(rgb >> 8)), ip_scale32((unsigned)rgb));
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

/* The node called `name` in the public screen list (the caller holds its lock). 0 when no such screen is listed.
 * Names compare with strcmp (case-sensitive), as Intuition's own public screen names are. */
static struct PubScreenNode *pub_node_in(struct List *pl, const char *name)
{
    struct PubScreenNode *n;
    for (n = (struct PubScreenNode *)pl->lh_Head; n->psn_Node.ln_Succ; n = (struct PubScreenNode *)n->psn_Node.ln_Succ)
        if (n->psn_Node.ln_Name && !strcmp(n->psn_Node.ln_Name, name))
            return n;
    return 0;
}

/* The public screen `name`, found in the public screen list (the caller holds its lock). 0 when it is not listed.
 * For readers under the list lock: the screen exists, public or not. It does NOT say LockPubScreen will find it. */
struct Screen *ipd_pub_screen_in(struct List *pl, const char *name)
{
    struct PubScreenNode *n = name && *name ? pub_node_in(pl, name) : 0;
    return n ? n->psn_Screen : 0;
}

struct Screen *ipd_wb_screen_in(struct List *pl)
{
    return ipd_pub_screen_in(pl, "Workbench");
}

#if PSNF_PRIVATE != IP_PSN_PRIVATE
#error IP_PSN_PRIVATE must equal PSNF_PRIVATE from intuition/screens.h
#endif

/* BOOT-31-WB-DEADLOCK: LockPubScreen("Workbench") on a closed Workbench makes Intuition reopen it (OS 3.1), which deadlocks
 * against IPrefs closing it for the boot RTG switch. This is the only place that asks for a public screen lock by name
 * (tests/test_wb_lock_guard.py): it reads the node in the list first and calls LockPubScreen only when Intuition will find
 * it (the list lock is held, so it cannot change between).
 * S57-FREEZE-2IN9: "listed with a screen" is not enough. While LoadWB opens the Workbench the node is listed but private;
 * Intuition counts it as absent and takes the same reopen path, which waits for the opening task while that task waits
 * for the list lock held here (2 boots in 9 froze). ip_pub_lockable is Intuition's own test; a screen that is still
 * opening reads as "not open yet", and the callers ask again on their next tick. 0 for a null or empty name. */
struct Screen *ipd_lock_pub_screen(const char *name)
{
    struct List *pl;
    struct PubScreenNode *n;
    struct Screen *sc = 0;
    if (!name || !*name)
        return 0;
    pl = LockPubScreenList();
    n = pub_node_in(pl, name);
    if (ip_pub_lockable(n != 0, n && n->psn_Screen, n ? n->psn_Flags : 0))
        sc = LockPubScreen((STRPTR)name);
    UnlockPubScreenList();
    return sc;
}

struct Screen *ipd_lock_wb_screen(void)
{
    return ipd_lock_pub_screen("Workbench");
}

/* The theme and desk prefs, read from file (ENV:, ENVARC:, the theme file). Never call it with the public screen list locked. */
static void pens_theme(void *ctx, gtheme *t, int *flat, unsigned *mask)
{
    am_prefs ap;
    struct Process *me = (struct Process *)FindTask(0);
    APTR oldwin = me->pr_WindowPtr;
    (void)ctx;
    me->pr_WindowPtr = (APTR)-1;        /* FLAT-BOOT-FREEZE: UP-Term: not assigned yet at boot is an answer, not a requester */
    amconf_load(&ap);
    gt_load(pen_theme_read, 0, ap.val[AM_GTHEME][0] ? ap.val[AM_GTHEME] : GT_DEFAULT, t);
    me->pr_WindowPtr = oldwin;
    *flat = amconf_desk_flat_pens(&ap);
    *mask = amconf_desk_mask(&ap);
}

/* FLAT-BOOT-FREEZE: the theme op ip_sync / ip_recolour call with the public screen list locked. It hands back the theme
 * pens_theme read before the lock and touches no file: a DOS read there can put up a requester (volume not ready at boot,
 * when amarchy-flat syncs the first second after LoadWB opens the Workbench), the requester's window needs the public screen
 * list, and the system stops. */
static int pens_theme_ready(void *ctx)
{
    return ((const pen_ctx *)ctx)->have;
}

static void pens_theme_cached(void *ctx, gtheme *t, int *flat, unsigned *mask)
{
    const pen_ctx *c = (const pen_ctx *)ctx;
    *t = c->t;
    *flat = c->flat;
    *mask = c->mask;
}

void ipd_icon_rgb(uint8_t rgb[IP_PENS][3])
{
    gtheme t;
    int flat;
    unsigned mask;
    pens_theme(0, &t, &flat, &mask);
    ip_colours(&t, ip_ground(&t, flat, mask), rgb);
}

/* Bring the held pens in line with the owners and the Workbench screen as it is now. The public screen list stays locked for
 * the whole sync, so the screen cannot close under the colour map; the theme is read only when pens are to be obtained (ip_sync NEED_THEME), with the list unlocked. The caller holds r->ss. */
static void pens_sync(ipd_rec *r)
{
    pen_ctx c;
    ip_ops o;
    struct List *pl;
    struct Screen *sc;
    int depth = 0, pass, act;
    int hold = ip_shared_hold(&r->s, GfxBase && GfxBase->LibNode.lib_Version >= 39);
    o.obtain = pen_obtain;
    o.release = pen_release;
    o.is_free = pen_is_free;
    o.theme = pens_theme_cached;
    o.set_rgb = pen_set_rgb;
    o.theme_ready = pens_theme_ready;
    o.ctx = &c;
    c.have = 0;
    for (pass = 0; pass < 2; pass++) {
        pl = LockPubScreenList();
        sc = ipd_wb_screen_in(pl);
        depth = 0;
        if (sc)
            depth = ip_screen_depth(sc->BitMap.Depth, (sc->ViewPort.Modes & EXTRA_HALFBRITE) != 0);
        c.cur = sc ? sc->ViewPort.ColorMap : 0;
        c.vp = sc ? &sc->ViewPort : 0;
        c.h = &r->s.h;
        c.sc = sc;
        act = ip_sync(&r->s.h, &o, hold, sc, depth, sc ? sc->BitMap.Depth : 0);
        if (act == IP_SYNC_NEED_THEME) {    /* pens must be obtained: read the theme with the list unlocked, then once more */
            UnlockPubScreenList();
            pens_theme(0, &c.t, &c.flat, &c.mask);
            c.have = 1;
            continue;
        }
        if (r->s.h.new_identity)
            r->s.epoch++;                   /* S32-STALE-HELD-PENS: a new screen (or none): pens other clients hold on the old one are gone; S32-PEN-EPOCH-LEAK: not on a release or recheck on the same screen */
        UnlockPubScreenList();
        break;
    }
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

unsigned ipd_epoch(void)
{
    unsigned e = 0;
    ipd_rec *r;
    Forbid();
    r = (ipd_rec *)FindSemaphore((STRPTR)IPD_SEM_NAME);
    if (r)
        e = r->s.epoch;
    Permit();
    return e;
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

/* The sync of pens_sync, but only the colours: the screen is the one the pens are held on or nothing happens. The screen list stays
 * locked so the ColorMap cannot close under SetRGB32. */
void ipd_recolour(void)
{
    ipd_rec *r = find_rec(0);
    pen_ctx c;
    ip_ops o;
    struct List *pl;
    struct Screen *sc;
    if (!r)
        return;
    ObtainSemaphore(&r->ss);
    o.obtain = pen_obtain;
    o.release = pen_release;
    o.is_free = pen_is_free;
    o.theme = pens_theme_cached;
    o.set_rgb = pen_set_rgb;
    o.ctx = &c;
    o.theme_ready = 0;
    pens_theme(0, &c.t, &c.flat, &c.mask);
    pl = LockPubScreenList();
    sc = ipd_wb_screen_in(pl);
    if (sc && sc == r->s.h.screen) {
        c.cur = sc->ViewPort.ColorMap;
        c.vp = &sc->ViewPort;
        c.h = &r->s.h;
        c.sc = sc;
        ip_recolour(&r->s.h, &o);
    }
    UnlockPubScreenList();
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
