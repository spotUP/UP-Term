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
#include "../handler/pubscr.h"
#include "amconf_core.h"
#include "amconf_dos.h"
#include "gthemes_core.h"
#include "desk_core.h"

extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;

/* cur: the Workbench screen's colour map now (0 = Workbench closed); vp its viewport; h the holder, sc the screen now: a held pen
 * is released only while h->screen is sc (else its screen is gone and the pens went with it; ip_holder_verify forgets the pens
 * of a screen reopened at the same address before any release) */
/* t/flat/mask: the theme, read from file before the public screen list is locked (FLAT-BOOT-FREEZE, see pens_theme_cached). */
typedef struct pen_ctx {
    ipd_rec *r;
    struct ColorMap *cur; struct ViewPort *vp; const ip_holder *h; const void *sc;
    gtheme t; int flat; unsigned mask; int have;
    int slot;                       /* TE-10.5b2b: the slot pens_select chose (the plan op counts applies for slots >= 1) */
    int takeover;                   /* the chosen slot's rule: Take Over Palette */
    int take[IP_SCREENS];           /* the rule's Take Over Palette per slot, set by pens_sync's hold loop */
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

/* The public screen `name`, found in the public screen list (the caller holds its lock). 0 when it is not listed.
 * For readers under the list lock: the screen exists, public or not. It does NOT say LockPubScreen will find it. */
struct Screen *ipd_pub_screen_in(struct List *pl, const char *name)
{
    struct PubScreenNode *n = name && *name ? pubscr_node_in(pl, name) : 0;
    return n ? n->psn_Screen : 0;
}

/* TE-10.6b: the Screen at pointer `scr` when it is in the locked public screen list, else 0. */
struct Screen *ipd_screen_in(struct List *pl, const void *scr)
{
    struct Node *n;
    if (!pl || !scr)
        return 0;
    for (n = pl->lh_Head; n->ln_Succ; n = n->ln_Succ)
        if (((struct PubScreenNode *)n)->psn_Screen == (const struct Screen *)scr)
            return ((struct PubScreenNode *)n)->psn_Screen;
    return 0;
}

struct Screen *ipd_wb_screen_in(struct List *pl)
{
    return ipd_pub_screen_in(pl, "Workbench");
}

struct Screen *ipd_lock_wb_screen(void)
{
    return ipd_lock_pub_screen("Workbench");
}

/* The theme and desk prefs, read from file (ENV:, ENVARC:, the theme file). Never call it with the public screen list locked. */
static void pens_theme_named(const char *name, gtheme *t, int *flat, unsigned *mask, int model)
{
    am_prefs ap;
    struct Process *me = (struct Process *)FindTask(0);
    APTR oldwin = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;        /* FLAT-BOOT-FREEZE: UP-Term: not assigned yet at boot is an answer, not a requester */
    amconf_load(&ap);
    if (model)                          /* TE-10.5b2b: the pen plan reads the theme's model (heap memory of THIS process) */
        gt_load_model(pen_theme_read, 0, name && *name ? name : ap.val[AM_GTHEME][0] ? ap.val[AM_GTHEME] : GT_DEFAULT, t);
    else
        gt_load(pen_theme_read, 0, name && *name ? name : ap.val[AM_GTHEME][0] ? ap.val[AM_GTHEME] : GT_DEFAULT, t);
    me->pr_WindowPtr = oldwin;
    *flat = amconf_desk_flat_pens(&ap);
    *mask = amconf_desk_mask(&ap);
}

/* name: the theme of a screen rule; 0 = the main theme (AM_GTHEME) */
static void pens_theme(void *ctx, gtheme *t, int *flat, unsigned *mask)
{
    (void)ctx;
    pens_theme_named(0, t, flat, mask, 0);
}

/* TE-10.4: the screen rules and the main theme name, from file. The caller has the public screen list unlocked. */
static void rules_read(gt_screen_rules *out, char *main_theme)
{
    am_prefs ap;
    struct Process *me = (struct Process *)FindTask(0);
    APTR oldwin = me->pr_WindowPtr;
    long len = 0;
    char *buf = (char *)AllocVec(8192, MEMF_PUBLIC);
    me->pr_WindowPtr = (APTR)-1;        /* a volume not there is an answer, not a requester */
    if (buf) {
        len = pen_theme_read(0, "ENV:Amarchy/screen-themes", buf, 8192);
        if (len < 0)
            len = 0;
    }
    gt_screen_rules_parse(buf ? buf : "", len, out);
    amconf_load(&ap);
    me->pr_WindowPtr = oldwin;
    if (buf)
        FreeVec(buf);
    out->enabled = amconf_flat_index(&ap, AM_SCREENTHEMES);   /* "on" (the default) or off */
    strncpy(main_theme, ap.val[AM_GTHEME][0] ? ap.val[AM_GTHEME] : GT_DEFAULT, GT_NAME - 1);
    main_theme[GT_NAME - 1] = 0;
}

void ipd_screen_rules_load(gt_screen_rules *out)
{
    char main_theme[GT_NAME];
    rules_read(out, main_theme);
}

void ipd_pub_screen_name_of(struct Screen *scr, char *out, int cap)
{
    struct List *pl;
    struct Node *nd;
    if (cap <= 0)
        return;
    out[0] = 0;
    if (!scr)
        return;
    pl = LockPubScreenList();
    for (nd = pl ? pl->lh_Head : 0; nd && nd->ln_Succ; nd = nd->ln_Succ) {
        if (((struct PubScreenNode *)nd)->psn_Screen == scr) {
            strncpy(out, nd->ln_Name ? nd->ln_Name : "", (size_t)cap - 1);
            out[cap - 1] = 0;
            break;
        }
    }
    UnlockPubScreenList();
}

unsigned long ipd_screen_theme_asks;

void ipd_screen_theme_name(struct Screen *scr, char *out, int cap)
{
    char name[IP_PSN_NAME], main_theme[GT_NAME];   /* a public screen name is up to 139 characters: a shorter buffer cuts it and the rule no longer matches */
    static gt_screen_rules rules;   /* about 6 KB: off the stack, the Amarchy main program runs its theme windows on 16384 bytes */
    ipd_screen_theme_asks++;
    ipd_pub_screen_name_of(scr, name, (int)sizeof name);   /* the list is unlocked again: the DOS reads below can put up a requester */
    rules_read(&rules, main_theme);   /* the rules and the main theme (AM_GTHEME) in one read, as the icon pens take them */
    gt_screen_draw_theme(&rules, name, main_theme, out, cap);
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

/* The public screen list as a snapshot (the caller holds its lock): name, screen, flags, depth, half-brite. */
static void take_snap(ip_snap *out, struct List *pl)
{
    struct Node *nd;
    out->count = 0;
    for (nd = pl ? pl->lh_Head : 0; nd && nd->ln_Succ; nd = nd->ln_Succ) {
        struct PubScreenNode *psn = (struct PubScreenNode *)nd;
        struct Screen *sc = psn->psn_Screen;
        int ehb = sc && (sc->ViewPort.Modes & EXTRA_HALFBRITE) != 0;
        ip_snap_add(out, nd->ln_Name ? nd->ln_Name : "", sc, (unsigned)psn->psn_Flags,
                    sc ? ip_screen_depth(sc->BitMap.Depth, ehb) : 0, ehb);
    }
}

/* TE-10.5b2b: the plan op ip_sync_plan calls with the public screen list locked: no DOS call, no allocation, no wait. The pens of the
 * chosen slot's theme plan on its screen: every pen the ColorMap reports free plus the pens this holder already holds. 0 entries when
 * the slot has no model (the theme was not loaded with one). The sentinel counts a plan for a slot >= 1. */
static int pens_plan(void *ctx, int depth, gt_plan_pen *out, int cap)
{
    pen_ctx *c = (pen_ctx *)ctx;
    ip_ops fo;
    uc_u32 mask[GT_MASK_WORDS];
    int n;
    memset(&fo, 0, sizeof fo);
    fo.is_free = pen_is_free;
    fo.ctx = c;
    ip_plan_free_mask(c->h, &fo, depth, mask);
    n = gt_screen_apply_plan(c->t.model, depth, mask, c->takeover, out, cap);
    if (n > 0 && c->slot >= 1)
        c->r->screen_applies++;
    return n;
}

/* ip_screens_sync's select: point the ops' context at slot i's screen, holder and theme */
static void pens_select(void *ctx, int slot, const ip_snap_entry *e)
{
    pen_ctx *c = (pen_ctx *)ctx;
    struct Screen *sc = e ? (struct Screen *)e->screen : 0;
    const ipd_stheme *st = &c->r->st[slot];
    c->cur = sc ? sc->ViewPort.ColorMap : 0;
    c->vp = sc ? &sc->ViewPort : 0;
    c->h = &c->r->t.slot[slot].sh.h;
    c->sc = sc;
    c->t = st->t;
    c->flat = st->flat;
    c->mask = st->mask;
    c->have = st->have;
    c->slot = slot;
    c->takeover = slot > 0 ? c->take[slot] : 0;
}

/* Passes of one sync: the first, plus one per round of theme reads. A pass that finds slots needing a theme reads ALL of them
 * with the list unlocked, so the next pass finishes unless the screen list changed meanwhile; three passes bound a list that
 * changes on every pass (the loop is a for over a constant). A slot left unsynced is picked up by the next 1 s tick: the
 * holder, not the diff, says an obtain is due. */
#define IPD_PASSES 3

/* Bring the held pens in line with the owners and every public screen the rules theme, as they are now. Under ONE public screen
 * list lock: snapshot, diff, apply, then ip_sync per slot (ip_screens_sync). The theme files and the rules are read only with the
 * list unlocked. A tick where nothing changed reads nothing from DOS. The caller holds r->ss (lock order: the holder semaphore
 * first, the list lock second). No LockPubScreen, no Wait/Delay under the list lock. */
static void pens_sync(ipd_rec *r)
{
    pen_ctx c;
    ip_ops o;
    int hold[IP_SCREENS], need[IP_SCREENS], pass, i, owner;
    int v39 = GfxBase && GfxBase->LibNode.lib_Version >= 39;
    memset(&o, 0, sizeof o);
    o.plan = pens_plan;
    o.obtain = pen_obtain;
    o.release = pen_release;
    o.is_free = pen_is_free;
    o.theme = pens_theme_cached;
    o.set_rgb = pen_set_rgb;
    o.theme_ready = pens_theme_ready;
    o.ctx = &c;
    memset(&c, 0, sizeof c);
    c.r = r;
    if (r->rules_dirty) {                   /* cleared first: a change during the read marks it again */
        r->rules_dirty = 0;
        rules_read(&r->rules, r->main_theme);
        ip_screens_recheck_plans(&r->t);    /* a rule or theme change: the themed screens re-plan at this sync */
    }
    for (i = 0; i < IP_SCREENS; i++) {
        r->st[i].have = 0;
        r->st[i].t.model = 0;               /* a pointer found here is from a broken earlier call (maybe another process): never freed */
    }
    for (pass = 0; pass < IPD_PASSES; pass++) {
        struct List *pl = LockPubScreenList();
        int n;
        take_snap(&r->cur, pl);
        ip_screens_diff(&r->snap, &r->cur, &r->diff);
        ip_screens_apply(&r->t, &r->diff);
        r->snap = r->cur;
        owner = ip_shared_hold(&r->t.slot[0].sh, v39);
        for (i = 0; i < r->t.count; i++) {
            int takeover = 0;
            hold[i] = owner && gt_screen_theme_of(&r->rules, i == 0 ? "Workbench" : r->t.slot[i].name, r->main_theme,
                                                  r->st[i].name, GT_NAME, &takeover);
            c.take[i] = i > 0 && takeover;
        }
        n = ip_screens_sync(&r->t, &r->snap, &o, hold, pens_select, &c, need);
        UnlockPubScreenList();
        if (n == 0)
            break;
        for (i = 0; i < r->t.count; i++)    /* pens must be obtained: read those themes with the list unlocked */
            if (need[i]) {
                gt_free(&r->st[i].t);           /* read twice in one call: our own model from the pass before */
                pens_theme_named(i == 0 ? 0 : r->st[i].name, &r->st[i].t, &r->st[i].flat, &r->st[i].mask, i > 0);
                r->st[i].have = 1;
            }
    }
    for (i = 0; i < IP_SCREENS; i++)        /* the models are this process's heap: none stays in the shared record */
        gt_free(&r->st[i].t);
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
            ip_screens_init(&r->t);
            r->rules_dirty = 1;
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
    refs = ip_shared_set(&r->t.slot[0].sh, owner, on);
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
        e = r->t.slot[0].sh.epoch;
    Permit();
    return e;
}

/* TE-10.6b: the epoch of the pens held for the screen at pointer `scr`; 0 for an unknown screen. */
unsigned ipd_epoch_of_screen(const void *scr)
{
    unsigned e = 0;
    ipd_rec *r;
    Forbid();
    r = (ipd_rec *)FindSemaphore((STRPTR)IPD_SEM_NAME);
    if (r)
        e = ip_screens_epoch_of_screen(&r->t, scr);
    Permit();
    return e;
}

unsigned ipd_epoch_of(const char *name)
{
    unsigned e = 0;
    ipd_rec *r;
    Forbid();
    r = (ipd_rec *)FindSemaphore((STRPTR)IPD_SEM_NAME);
    if (r)
        e = ip_screens_epoch(&r->t, name);
    Permit();
    return e;
}

void ipd_rules_changed(void)
{
    ipd_rec *r = find_rec(0);
    if (r)
        r->rules_dirty = 1;
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
    memset(&o, 0, sizeof o);            /* plan = 0: the recolour never plans */
    memset(&c, 0, sizeof c);
    ObtainSemaphore(&r->ss);
    r->rules_dirty = 1;                 /* the main theme may have changed: the rules' answers with it */
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
    if (sc && sc == r->t.slot[0].sh.h.screen) {
        c.cur = sc->ViewPort.ColorMap;
        c.vp = &sc->ViewPort;
        c.h = &r->t.slot[0].sh.h;
        c.sc = sc;
        ip_recolour(&r->t.slot[0].sh.h, &o);
    }
    UnlockPubScreenList();
    ReleaseSemaphore(&r->ss);
}

int ipd_role_write(int pen, uc_u32 rgb)
{
    ipd_rec *r = find_rec(0);
    struct List *pl;
    struct Screen *sc;
    int ok = 0;
    if (!r)
        return -1;
    ObtainSemaphore(&r->ss);
    pl = LockPubScreenList();
    sc = ipd_wb_screen_in(pl);
    if (sc && sc == r->t.slot[0].sh.h.screen && ip_role_claim(&r->t.slot[0].sh.h, pen, rgb)) {
        SetRGB32(&sc->ViewPort, (ULONG)pen, ip_scale32((unsigned)(rgb >> 16)), ip_scale32((unsigned)(rgb >> 8)), ip_scale32((unsigned)rgb));
        ok = 1;
    }
    UnlockPubScreenList();
    ReleaseSemaphore(&r->ss);
    return ok;
}

int ipd_status_line(char *out, size_t cap)
{
    ip_shared s;
    ipd_rec *r;
    int n, w, screens = 0;
    unsigned long applies = 0;
    ip_shared_init(&s);
    Forbid();
    r = (ipd_rec *)FindSemaphore((STRPTR)IPD_SEM_NAME);
    if (r) {
        s = r->t.slot[0].sh;
        screens = ip_screens_held(&r->t);
        applies = r->screen_applies;
    }
    Permit();
    n = ip_status_line(&s.h, out, cap);
    if (n <= 0)
        return 0;
    w = snprintf(out + n, cap - (size_t)n, " refs=%d screens=%d applies=%lu", ip_shared_refs(&s), screens, applies);
    if (w <= 0 || (size_t)(n + w) >= cap)
        return 0;
    return n + w;
}
