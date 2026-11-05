/* Lucide icon pens, pure half (amarchy/iconpen_core.c, desk_pen_plan_for; plan 2026-10-31-amarchy-lucide-icons.md L5, L6, L7). */
#include <stdio.h>
#include <string.h>
#include "../amarchy/iconpen_core.h"
#include "harness.h"

static void theme(gtheme *t)
{
    const char *s = "name = ipt\nbackground = #101820\nforeground = #e0e8f0\ndark_foreground = #607080\naccent = #ff8800\n"
                    "selection = #304860\nmuted = #8090a0\n";
    gt_parse(s, (long)strlen(s), t);
}

static void test_colours_and_fold(void)
{
    gtheme t;
    uint8_t rgb[IP_PENS][3];
    int slot, depth;
    theme(&t);
    ip_colours(&t, desk_role(&t, DK_BG), rgb);
    CHECK(rgb[IP_FG100][0] == 0xe0 && rgb[IP_FG100][1] == 0xe8 && rgb[IP_FG100][2] == 0xf0);
    CHECK(rgb[IP_ACCENT100][0] == 0xff && rgb[IP_ACCENT100][2] == 0x00);
    CHECK(rgb[IP_SELBG][0] == 0x30 && rgb[IP_SELBG][1] == 0x48 && rgb[IP_SELBG][2] == 0x60);
    CHECK(rgb[IP_MUTED100][0] == 0x80);
    CHECK(rgb[IP_SPARE12][0] == 0x10 && rgb[IP_SPARE15][2] == 0x20);
    CHECK(rgb[IP_FG33][0] > 0x10 && rgb[IP_FG33][0] < rgb[IP_FG66][0] && rgb[IP_FG66][0] < rgb[IP_FG100][0]);
    /* fold: full roles land on their own pen, no fold ever lands on green or red */
    CHECK_INT(ip_fold(&t, IP_FG100, 3), DK_FG);
    CHECK_INT(ip_fold(&t, IP_ACCENT100, 3), DK_ACCENT);
    CHECK_INT(ip_fold(&t, IP_SELBG, 3), DK_SELECTION);
    CHECK_INT(ip_fold(&t, IP_MUTED100, 3), DK_MUTED);
    for (depth = 2; depth <= 8; depth++)
        for (slot = 0; slot < IP_PENS; slot++) {
            int p = ip_fold(&t, slot, depth);
            CHECK(p != DK_GREEN && p != DK_BACKDROP);
            if (depth == 2) CHECK(p >= 0 && p <= DK_ACCENT);
            if (depth >= 6) CHECK_INT(p, IP_FIRST + slot);
            if (depth == 4 || depth == 5) CHECK(slot >= 8 ? p == slot : (p >= 0 && p <= DK_DARKER));
        }
}

static void test_plan_no_overlap(void)
{
    unsigned char pens[DK_SLOTS];
    int depth, i, f, k;
    CHECK_INT(DK_SELECTION, 4);
    CHECK_INT(DK_MUTED, 1); /* FILENAME-PEN: Workbench draws list-view file names with fixed pen 1 */
    CHECK_INT(DK_DARKER, 5);
    for (depth = 2; depth <= 8; depth++) {
        desk_pen_plan p;
        desk_pen_plan_for(depth, &p);
        CHECK_INT(p.button, 4);
        CHECK_INT(p.backdrop, depth >= 3 ? DK_BACKDROP : DK_BG); /* FLAT-COLOUR-OVERHAUL: own pen at 3+ planes; FILENAME-PEN: below that pen 5 folds to DK_BG */
        CHECK_INT(desk_backdrop_pen_at(depth), p.backdrop); /* L6: the backdrop pen (WBPattern tile, palette write) is the plan's */
        CHECK(p.button != p.backdrop);
        CHECK(p.icon_first + p.icon_count <= (1 << depth));
        for (i = 0; i < p.icon_count; i++) {
            int pen = p.icon_first + i;
            CHECK(pen != p.button && pen != p.backdrop && pen >= 8);
            for (f = 0; f <= 1; f++) {
                desk_pens_for(f, pens);
                for (k = 0; k < DK_SLOTS; k++) CHECK(pens[k] != pen);
            }
        }
    }
}

typedef struct fake { int taken[64]; int mine[64]; int releases; } fake;
static int f_obtain(void *c, int pen, unsigned r, unsigned g, unsigned b)
{
    fake *f = c;
    (void)r; (void)g; (void)b;
    if (f->taken[pen] || f->mine[pen]) return -1;
    f->mine[pen] = 1;
    return pen;
}
static void f_release(void *c, int pen)
{
    fake *f = c;
    f->releases++;
    f->mine[pen] = 0;
}

static void test_holder(void)
{
    gtheme t;
    uint8_t rgb[IP_PENS][3];
    fake f;
    ip_ops o;
    ip_holder h;
    char line[32];
    int s1, s2, i, left = 0;
    theme(&t);
    ip_colours(&t, desk_role(&t, DK_BG), rgb);
    memset(&f, 0, sizeof f);
    o.obtain = f_obtain; o.release = f_release; o.ctx = &f;
    ip_holder_init(&h);
    CHECK_INT(ip_holder_obtain(&h, &o, &s1, 6, rgb), 16);
    CHECK(ip_status_line(&h, line, sizeof line) > 0);
    CHECK_STR(line, "pens-held 16");
    CHECK(!ip_holder_stale(&h, &s1, 6));
    CHECK(ip_holder_stale(&h, &s2, 6));
    CHECK(ip_holder_stale(&h, &s1, 4));
    /* the screen closes and reopens: re-obtain releases the old pens first, holds 16 again */
    CHECK_INT(ip_holder_obtain(&h, &o, &s2, 6, rgb), 16);
    CHECK_INT(f.releases, 16);
    /* a taken pen is skipped, not held */
    ip_holder_release(&h, &o);
    f.taken[40] = 1;
    CHECK_INT(ip_holder_obtain(&h, &o, &s1, 6, rgb), 15);
    ip_status_line(&h, line, sizeof line);
    CHECK_STR(line, "pens-held 15 missing 40");   /* names the pen another user holds (rig 10: the dim pen on 46/47) */
    /* flat off releases everything */
    CHECK_INT(ip_holder_release(&h, &o), 15);
    ip_status_line(&h, line, sizeof line);
    CHECK_STR(line, "pens-held 0");
    for (i = 0; i < 64; i++) left += f.mine[i];
    CHECK_INT(left, 0);
    /* depth 5 holds pens 8..15 only; depth 3 holds none */
    CHECK_INT(ip_holder_obtain(&h, &o, &s1, 5, rgb), 8);
    CHECK(f.mine[8] && f.mine[15] && !f.mine[32]);
    CHECK_INT(ip_holder_obtain(&h, &o, &s1, 3, rgb), 0);
}

static void test_sync(void)
{
    ip_holder h;
    ip_ops o;
    uint8_t rgb[IP_PENS][3];
    int s1, s2;
    memset(rgb, 0, sizeof rgb);
    memset(&o, 0, sizeof o);
    ip_holder_init(&h);
    CHECK_INT(ip_sync_decide(&h, 1, &s1, 6), IP_SYNC_OBTAIN);       /* boot, flat on */
    CHECK_INT(ip_sync_decide(&h, 0, &s1, 6), IP_SYNC_NONE);         /* flat off, nothing held */
    CHECK_INT(ip_sync_decide(&h, 1, NULL, 6), IP_SYNC_NONE);        /* no Workbench screen yet */
    CHECK_INT(ip_scale32(0xff), 0xffffffffUL);
    CHECK_INT(ip_scale32(0x80), 0x80808080UL);
    CHECK_INT(ip_screen_depth(6, 1), 5);
    CHECK_INT(ip_screen_depth(6, 0), 6);
    CHECK_INT(ip_screen_depth(8, 0), 8);
    /* hold on s1 (depth 3 holds none but still ties to the screen) */
    h.screen = &s1; h.depth = 6; h.held = 16;
    CHECK_INT(ip_sync_decide(&h, 1, &s1, 6), IP_SYNC_NONE);         /* same screen: no re-obtain */
    CHECK_INT(ip_sync_decide(&h, 1, &s2, 6), IP_SYNC_OBTAIN);       /* Workbench reopened: new screen */
    CHECK_INT(ip_sync_decide(&h, 1, &s1, 8), IP_SYNC_OBTAIN);       /* depth changed */
    CHECK_INT(ip_sync_decide(&h, 1, NULL, 6), IP_SYNC_RELEASE);     /* Workbench closed: drop */
    CHECK_INT(ip_sync_decide(&h, 0, &s1, 6), IP_SYNC_RELEASE);      /* quitting / kill switch held */
    /* S32-PEN-EPOCH-LEAK: the epoch moves only when the screen identity does */
    CHECK_INT(ip_sync_new_identity(&s1, &s1, IP_SYNC_RELEASE, 0), 0);   /* hold off, screen still open */
    CHECK_INT(ip_sync_new_identity(&s1, &s1, IP_SYNC_OBTAIN, 0), 0);    /* recheck on the same screen */
    CHECK_INT(ip_sync_new_identity(&s1, &s1, IP_SYNC_NONE, 0), 0);
    CHECK_INT(ip_sync_new_identity(&s1, &s2, IP_SYNC_OBTAIN, 0), 1);    /* screen pointer changed */
    CHECK_INT(ip_sync_new_identity(&s1, NULL, IP_SYNC_RELEASE, 0), 1);  /* screen gone */
    CHECK_INT(ip_sync_new_identity(&s1, &s1, IP_SYNC_OBTAIN, 1), 1);    /* same-address reopen: pens lost */
    CHECK_INT(ip_hold_wanted(0, 0, 1), 1);                          /* running: hold, whatever the flat switch says */
    CHECK_INT(ip_hold_wanted(1, 0, 1), 0);                          /* quitting */
    CHECK_INT(ip_hold_wanted(0, 1, 1), 0);                          /* kill switch held */
    CHECK_INT(ip_hold_wanted(0, 0, 0), 0);                          /* graphics < V39 */
    /* after a real release the holder ties to no screen, so flat on again obtains again (regression: it stayed on the old screen) */
    {
        fake f;
        memset(&f, 0, sizeof f);
        o.obtain = f_obtain; o.release = f_release; o.ctx = &f;
        ip_holder_init(&h);
        CHECK_INT(ip_holder_obtain(&h, &o, &s1, 6, rgb), 16);
        CHECK_INT(ip_holder_release(&h, &o), 16);
        CHECK_INT(ip_sync_decide(&h, 1, &s1, 6), IP_SYNC_OBTAIN);
    }
}

/* ---- L7 cause 2: the graphics PalExtra free list as measured on rig 10 (3.2, depth 8): free pens are handed out from the
 * head (boot order 251 250 .. top down), ReleasePen puts the pen back at the HEAD (after flat off: 47 46 .. 32 244 ..),
 * ObtainPen(n, exclusive) takes n only while n is free, ObtainBestPen of a new colour takes the head. ---- */
typedef struct palx { int list[256]; int n; } palx;

static void px_boot(palx *p)
{
    int i;
    p->n = 0;
    for (i = 251; i >= 4; i--)
        if (i < 17 || i > 19)
            p->list[p->n++] = i;
}

static int px_take(palx *p, int pen)
{
    int i;
    for (i = 0; i < p->n; i++)
        if (p->list[i] == pen) {
            memmove(&p->list[i], &p->list[i + 1], (size_t)(p->n - i - 1) * sizeof p->list[0]);
            p->n--;
            return pen;
        }
    return -1;
}

static int px_best(palx *p) { return p->n ? px_take(p, p->list[0]) : -1; }   /* a new shared colour */

static int px_obtain(void *ctx, int pen, unsigned r, unsigned g, unsigned b) { (void)r; (void)g; (void)b; return px_take((palx *)ctx, pen); }

static void px_release(void *ctx, int pen)
{
    palx *p = (palx *)ctx;
    memmove(&p->list[1], &p->list[0], (size_t)p->n * sizeof p->list[0]);
    p->list[0] = pen;
    p->n++;
}

/* pens_sync of amarchy/flat.c: the same two decisions in the same order (the flat switch is not an input) */
static void px_sync(ip_holder *h, const ip_ops *o, int quit, const void *scr, const uint8_t rgb[IP_PENS][3])
{
    int act = ip_sync_decide(h, ip_hold_wanted(quit, 0, 1), scr, 8);
    if (act == IP_SYNC_RELEASE)
        ip_holder_release(h, o);
    else if (act == IP_SYNC_OBTAIN)
        ip_holder_obtain(h, o, scr, 8, rgb);
}

static void test_free_list_model(void)
{
    palx p;
    ip_holder h;
    ip_ops o;
    uint8_t rgb[IP_PENS][3];
    int scr, dim, i, k;
    memset(rgb, 0, sizeof rgb);
    px_boot(&p);
    o.obtain = px_obtain; o.release = px_release; o.ctx = &p;
    ip_holder_init(&h);
    px_sync(&h, &o, 0, &scr, rgb);                      /* boot, flat on */
    CHECK_INT(h.held, 16);
    for (k = 0; k < 2; k++) {                           /* flat off, on, off, on: the 1 s tick and Ctrl-D syncs */
        px_sync(&h, &o, 0, &scr, rgb);                  /* flat off */
        dim = px_best(&p);                              /* dim_refresh and Amarchy's theme pens while off */
        CHECK_INT(dim >= IP_FIRST && dim < IP_FIRST + IP_PENS, 0);
        for (i = 0; i < 3; i++) {
            int sh = px_best(&p);
            CHECK_INT(sh >= IP_FIRST && sh < IP_FIRST + IP_PENS, 0);
        }
        px_sync(&h, &o, 0, &scr, rgb);                  /* flat on again */
        CHECK_INT(h.held, 16);
    }
    px_sync(&h, &o, 1, &scr, rgb);                      /* quit: every pen back */
    CHECK_INT(h.held, 0);
    CHECK_INT(px_take(&p, IP_FIRST + 15), IP_FIRST + 15);
}

/* F14 (H1) symptom: icons keep theme pens when flat stops. amarchy-icons APPLY takes a reference and exits; amarchy-flat takes
 * another while it runs; flat stopping (or the kill switch) must not release the pens while the icons are applied. Drives the
 * same decisions ipd_owner runs in amarchy/iconpen_dos.c (ip_shared_set -> ip_shared_hold -> ip_sync_decide -> obtain/release). */
static void sh_sync(ip_shared *s, const ip_ops *o, const void *scr, const uint8_t rgb[IP_PENS][3])
{
    int act = ip_sync_decide(&s->h, ip_shared_hold(s, 1), scr, 8);
    if (act == IP_SYNC_RELEASE)
        ip_holder_release(&s->h, o);
    else if (act == IP_SYNC_OBTAIN)
        ip_holder_obtain(&s->h, o, scr, 8, rgb);
}

static int sh_owner(ip_shared *s, const ip_ops *o, const void *scr, const uint8_t rgb[IP_PENS][3], unsigned owner, int on)
{
    int refs = ip_shared_set(s, owner, on);
    sh_sync(s, o, scr, rgb);
    return refs;
}

static void test_icons_keep_pens_when_flat_stops(void)
{
    palx p;
    ip_shared s;
    ip_ops o;
    uint8_t rgb[IP_PENS][3];
    int scr;
    memset(rgb, 0, sizeof rgb);
    px_boot(&p);
    o.obtain = px_obtain; o.release = px_release; o.ctx = &p;
    ip_shared_init(&s);
    CHECK_INT(sh_owner(&s, &o, &scr, rgb, IP_OWNER_ICONS, 1), 1);       /* APPLY with flat OFF and not running */
    CHECK_INT(s.h.held, 16);
    CHECK_INT(sh_owner(&s, &o, &scr, rgb, IP_OWNER_ICONS, 1), 1);       /* a second APPLY counts once */
    CHECK_INT(sh_owner(&s, &o, &scr, rgb, IP_OWNER_FLAT, 1), 2);        /* amarchy-flat starts */
    CHECK_INT(s.h.held, 16);
    CHECK_INT(sh_owner(&s, &o, &scr, rgb, IP_OWNER_FLAT, 0), 1);        /* flat stops: the icons stay themed */
    CHECK_INT(s.h.held, 16);
    CHECK_INT(px_take(&p, IP_FIRST), -1);                               /* pen 32 is still ours, not free for a shared pen */
    CHECK_INT(sh_owner(&s, &o, &scr, rgb, IP_OWNER_FLAT, 0), 1);        /* a second stop does not underflow */
    CHECK_INT(s.h.held, 16);
    CHECK_INT(sh_owner(&s, &o, &scr, rgb, IP_OWNER_ICONS, 0), 0);       /* RESTORE: the last reference releases */
    CHECK_INT(s.h.held, 0);
    CHECK_INT(px_take(&p, IP_FIRST + 15), IP_FIRST + 15);   /* freed: a probe can take it ... */
    px_boot(&p);                                            /* ... then a fresh palette for the next order */
    /* flat alone first, then APPLY, then RESTORE: flat keeps the pens */
    CHECK_INT(sh_owner(&s, &o, &scr, rgb, IP_OWNER_FLAT, 1), 1);
    CHECK_INT(sh_owner(&s, &o, &scr, rgb, IP_OWNER_ICONS, 1), 2);
    CHECK_INT(sh_owner(&s, &o, &scr, rgb, IP_OWNER_ICONS, 0), 1);
    CHECK_INT(s.h.held, 16);
    CHECK_INT(ip_shared_hold(&s, 0), 0);                                /* graphics < V39 never holds */
}

/* ICONPEN-MAIN-BOOT symptom: flat OFF, icons applied, reboot. The holder record is RAM-only, so nobody holds pens 32-47 though the
 * Amarchy .info files name them. Amarchy main, started at boot, takes IP_OWNER_MAIN while the manifest exists (ip_main_step drives
 * it, as iconpens_follow in amarchy/amarchy.c does), re-takes after a Workbench reopen, drops on RESTORE and on exit. */
static void px_gone(void *ctx, int pen)   /* iconpen_dos pen_release with old = 0: the old screen is gone, its pens went with it */
{
    (void)ctx; (void)pen;
}

static int main_follow(ip_shared *s, const ip_ops *o, const void *scr, const uint8_t rgb[IP_PENS][3], int applied, int *own, int *synced)
{
    int step = ip_main_step(applied, *own);
    if (step == IP_MAIN_TAKE) { sh_owner(s, o, scr, rgb, IP_OWNER_MAIN, 1); *own = 1; }
    else if (step == IP_MAIN_DROP) { sh_owner(s, o, scr, rgb, IP_OWNER_MAIN, 0); *own = 0; }
    else if (step == IP_MAIN_SYNC) { sh_sync(s, o, scr, rgb); (*synced)++; }
    return step;
}

static void test_main_takes_pens_at_boot(void)
{
    palx p;
    ip_shared s;
    ip_ops o;
    uint8_t rgb[IP_PENS][3];
    ip_ops o2;
    int scr, scr2, own = 0, synced = 0;
    memset(rgb, 0, sizeof rgb);
    px_boot(&p);
    o.obtain = px_obtain; o.release = px_release; o.ctx = &p;
    ip_shared_init(&s);                                                 /* fresh boot: no record, no owner */
    CHECK_INT(main_follow(&s, &o, &scr, rgb, 0, &own, &synced), IP_MAIN_NONE);   /* icons original: main holds nothing */
    CHECK_INT(s.h.held, 0);
    CHECK_INT(main_follow(&s, &o, &scr, rgb, 1, &own, &synced), IP_MAIN_TAKE);   /* icons applied: main takes the pens at start */
    CHECK_INT(s.h.held, 16);
    CHECK_INT(ip_shared_refs(&s), 1);
    CHECK_INT(px_take(&p, IP_FIRST), -1);
    CHECK_INT(main_follow(&s, &o, &scr, rgb, 1, &own, &synced), IP_MAIN_SYNC);   /* the tick: nothing changes while the screen stays */
    CHECK_INT(s.h.held, 16);
    px_boot(&p);                                                        /* Workbench reopened: new screen, new colour map */
    o2 = o; o2.release = px_gone;
    CHECK_INT(main_follow(&s, &o2, &scr2, rgb, 1, &own, &synced), IP_MAIN_SYNC);
    CHECK_INT(s.h.screen == (const void *)&scr2, 1);                    /* re-taken on the new screen */
    CHECK_INT(s.h.held, 16);
    CHECK_INT(px_take(&p, IP_FIRST), -1);
    CHECK_INT(main_follow(&s, &o, &scr2, rgb, 0, &own, &synced), IP_MAIN_DROP);  /* RESTORE ran while main runs */
    CHECK_INT(s.h.held, 0);
    CHECK_INT(ip_shared_refs(&s), 0);
    CHECK_INT(main_follow(&s, &o, &scr2, rgb, 1, &own, &synced), IP_MAIN_TAKE);
    sh_owner(&s, &o, &scr2, rgb, IP_OWNER_ICONS, 1);                    /* another owner joins: main's exit keeps the pens */
    sh_owner(&s, &o, &scr2, rgb, IP_OWNER_MAIN, 0);
    CHECK_INT(s.h.held, 16);
    sh_owner(&s, &o, &scr2, rgb, IP_OWNER_ICONS, 0);
    CHECK_INT(s.h.held, 0);
    CHECK_INT(synced >= 2, 1);
}

/* FLAT-PENS47-RESET (s28 evidence flat-pens47-hunt.md, rig 14, 3.2, depth 8): the Workbench reopen that desk_dos_apply runs
 * allocated the new Screen at the freed one's address. The holder keyed its screen on address + depth, so ip_sync_decide said
 * NONE: the 16 icon pens stayed missing on the new ColorMap ("pens-held 0 missing 32 .. 47") and pens 4-7 kept the graphics
 * defaults 0000ff ff00ff 00ffff ffffff. This drives ip_sync (the whole of amarchy/iconpen_dos.c pens_sync) against a model
 * ColorMap whose allocation state is the truth, like graphics' PalExtra. */
typedef struct rcm {
    int alloc[256];              /* 1 = allocated (by anyone) */
    unsigned long rgb[256];
    int bad_release;             /* ReleasePen of a pen that is not allocated: corrupts a real free list */
    int sets;                    /* set_rgb calls */
} rcm;

static void rcm_open(rcm *c)          /* a fresh screen: nothing allocated, graphics-default pens 4-7 */
{
    static const unsigned long def[4] = { 0x0000ffUL, 0xff00ffUL, 0x00ffffUL, 0xffffffUL };
    int i;
    memset(c->alloc, 0, sizeof c->alloc);
    for (i = 0; i < 4; i++)
        c->rgb[4 + i] = def[i];
}
static int rcm_obtain(void *ctx, int pen, unsigned r, unsigned g, unsigned b)
{
    rcm *c = (rcm *)ctx;
    if (c->alloc[pen])
        return -1;
    c->alloc[pen] = 1;
    c->rgb[pen] = ((unsigned long)r << 16) | ((unsigned long)g << 8) | b;
    return pen;
}
static void rcm_release(void *ctx, int pen)
{
    rcm *c = (rcm *)ctx;
    if (!c->alloc[pen])
        c->bad_release++;
    c->alloc[pen] = 0;
}
static int rcm_is_free(void *ctx, int pen) { return !((rcm *)ctx)->alloc[pen]; }
static int rcm_other;            /* 1: the theme the user switched to */
static void rcm_theme(void *ctx, gtheme *t, int *flat, unsigned *mask)
{
    static const char *b = "name = ipb\nbackground = #f0e8d8\nforeground = #202830\ndark_foreground = #706050\naccent = #0050c0\n"
                           "selection = #c8b898\nmuted = #908070\n";
    (void)ctx;
    if (rcm_other)
        gt_parse(b, (long)strlen(b), t);
    else
        theme(t);
    *flat = 1;                   /* flat on, flat owns the backdrop */
    *mask = 0;
}
static void rcm_set(void *ctx, int pen, uc_u32 rgb)
{
    rcm *c = (rcm *)ctx;
    c->rgb[pen] = rgb;
    c->sets++;
}

static void check_theme_pens47(const rcm *c)
{
    gtheme t;
    int i;
    theme(&t);
    for (i = 0; i < 4; i++)
        CHECK_INT((long)c->rgb[4 + i], (long)desk_pen_colour(&t, 4 + i, 1));
    CHECK(c->rgb[5] != 0xff00ffUL);
}

static void test_reopen_same_address(void)
{
    static rcm c;
    ip_holder h;
    ip_ops o;
    int scr;                     /* one address for both screens */
    memset(&c, 0, sizeof c);
    memset(&o, 0, sizeof o);
    o.obtain = rcm_obtain; o.release = rcm_release; o.is_free = rcm_is_free; o.theme = rcm_theme; o.set_rgb = rcm_set;
    o.ctx = &c;
    ip_holder_init(&h);
    rcm_open(&c);
    CHECK_INT(ip_sync(&h, &o, 1, &scr, 8, 8), IP_SYNC_OBTAIN);        /* boot */
    CHECK_INT(h.held, 16);
    check_theme_pens47(&c);
    c.sets = 0;
    CHECK_INT(ip_sync(&h, &o, 1, &scr, 8, 8), IP_SYNC_NONE);          /* same screen, pens still ours: nothing */
    CHECK_INT(c.sets, 0);
    rcm_open(&c);                                                     /* Workbench reopen at the SAME address */
    CHECK_INT(ip_sync(&h, &o, 1, &scr, 8, 8), IP_SYNC_OBTAIN);        /* the ColorMap says the pens are gone */
    CHECK_INT(h.held, 16);
    CHECK_INT(c.alloc[IP_FIRST] + c.alloc[IP_FIRST + 15], 2);
    check_theme_pens47(&c);                                           /* cause 1: the new screen gets the theme on 4-7 */
    CHECK_INT(c.bad_release, 0);                                      /* never released on a ColorMap it did not obtain on */
    rcm_open(&c);                                                     /* same address, other depth */
    CHECK_INT(ip_sync(&h, &o, 1, &scr, 4, 4), IP_SYNC_OBTAIN);
    CHECK_INT(c.bad_release, 0);
    /* a pen another program held at obtain time and has let go since: re-obtained on the same screen (released normally) */
    rcm_open(&c);
    ip_holder_init(&h);
    c.alloc[IP_FIRST + 8] = 1;
    CHECK_INT(ip_sync(&h, &o, 1, &scr, 8, 8), IP_SYNC_OBTAIN);
    CHECK_INT(h.held, 15);
    CHECK_INT(ip_sync(&h, &o, 1, &scr, 8, 8), IP_SYNC_NONE);          /* still taken: no churn */
    c.alloc[IP_FIRST + 8] = 0;
    CHECK_INT(ip_sync(&h, &o, 1, &scr, 8, 8), IP_SYNC_OBTAIN);
    CHECK_INT(h.held, 16);
    CHECK_INT(c.bad_release, 0);
    CHECK_INT(ip_sync(&h, &o, 0, &scr, 8, 8), IP_SYNC_RELEASE);       /* last owner gone */
    CHECK_INT(h.held, 0);
    CHECK_INT(c.alloc[IP_FIRST], 0);
    CHECK_INT(c.bad_release, 0);
}

/* FLAT-BOOT-FREEZE: the theme is asked for only when pens are to be obtained; not loaded yet -> NEED_THEME and nothing changes */
static int tr_have;
static int tr_ready(void *ctx) { (void)ctx; return tr_have; }
static void test_theme_ready(void)
{
    static rcm c;
    ip_holder h;
    ip_ops o;
    int scr;
    memset(&c, 0, sizeof c);
    memset(&o, 0, sizeof o);
    o.obtain = rcm_obtain; o.release = rcm_release; o.is_free = rcm_is_free; o.theme = rcm_theme; o.set_rgb = rcm_set;
    o.theme_ready = tr_ready;
    o.ctx = &c;
    ip_holder_init(&h);
    rcm_open(&c);
    tr_have = 0;
    CHECK_INT(ip_sync(&h, &o, 1, &scr, 8, 8), IP_SYNC_NEED_THEME);    /* boot, theme not read: nothing obtained */
    CHECK_INT(h.held, 0);
    CHECK_INT(c.sets, 0);
    CHECK_INT(h.new_identity, 0);
    tr_have = 1;                                                      /* the caller read it */
    CHECK_INT(ip_sync(&h, &o, 1, &scr, 8, 8), IP_SYNC_OBTAIN);
    CHECK_INT(h.held, 16);
    CHECK_INT(h.new_identity, 1);
    /* steady state: holder current, the readiness is not even asked, so no read is ever needed */
    tr_have = 0;
    CHECK_INT(ip_sync(&h, &o, 1, &scr, 8, 8), IP_SYNC_NONE);
    CHECK_INT(ip_sync(&h, &o, 0, &scr, 8, 8), IP_SYNC_RELEASE);      /* release needs no theme either */
    /* same-address reopen: verify forgets the pens on the first call; the retry reports the same new identity */
    tr_have = 1;
    ip_holder_init(&h);
    rcm_open(&c);
    CHECK_INT(ip_sync(&h, &o, 1, &scr, 8, 8), IP_SYNC_OBTAIN);
    rcm_open(&c);
    tr_have = 0;
    CHECK_INT(ip_sync(&h, &o, 1, &scr, 8, 8), IP_SYNC_NEED_THEME);
    tr_have = 1;
    CHECK_INT(ip_sync(&h, &o, 1, &scr, 8, 8), IP_SYNC_OBTAIN);
    CHECK_INT(h.new_identity, 1);
    CHECK_INT(c.bad_release, 0);
}

/* ICON-AA-WRONG-BG + ICONPEN-THEME-SWITCH-RECOLOUR: the ramp starts at the backdrop, and a theme switch on the same screen rewrites 32-47 */
static void test_theme_switch_recolour(void)
{
    static rcm c;
    ip_holder h;
    ip_ops o;
    gtheme a, b;
    int scr, i;
    memset(&c, 0, sizeof c);
    memset(&o, 0, sizeof o);
    o.obtain = rcm_obtain; o.release = rcm_release; o.is_free = rcm_is_free; o.theme = rcm_theme; o.set_rgb = rcm_set;
    o.ctx = &c;
    ip_holder_init(&h);
    rcm_open(&c);
    rcm_other = 0;
    theme(&a);
    rcm_other = 1;
    rcm_theme(&c, &b, &i, (unsigned *)&scr);
    rcm_other = 0;
    CHECK_INT((long)ip_ground(&a, 1, 1u << DT_WBPATTERN), (long)gt_darker(&a)); /* PEN255: the desktop-theme tile keeps the dark backdrop */
    CHECK_INT((long)ip_ground(&a, 1, 1u << DT_WBPATTERN), (long)ip_ground(&a, 1, 0));
    CHECK(ip_ground(&a, 1, 0) != desk_role(&a, DK_BG));              /* the backdrop is not the panel tone */
    CHECK_INT(ip_sync(&h, &o, 1, &scr, 8, 8), IP_SYNC_OBTAIN);
    /* theme A: the spare slots (the ramp's zero point) are the backdrop RGB, not pen 0 */
    CHECK_INT((long)c.rgb[IP_FIRST + IP_SPARE12], (long)ip_ground(&a, 1, 0));
    CHECK(c.rgb[IP_FIRST + IP_SPARE12] != desk_role(&a, DK_BG));
    c.sets = 0;
    CHECK_INT(ip_recolour(&h, &o), 0);                                /* same theme: no poke */
    CHECK_INT(c.sets, 0);
    rcm_other = 1;                                                    /* switch to theme B, same screen */
    CHECK_INT(ip_sync(&h, &o, 1, &scr, 8, 8), IP_SYNC_NONE);          /* the holder alone sees no change */
    CHECK_INT(ip_recolour(&h, &o), 16);
    CHECK_INT(c.sets, 16);                                            /* sentinel: every pen 32-47 rewritten */
    CHECK_INT((long)c.rgb[IP_FIRST + IP_SPARE12], (long)ip_ground(&b, 1, 0));
    CHECK_INT((long)c.rgb[IP_FIRST + IP_SELBG], (long)gt_selection(&b));
    CHECK_INT(c.bad_release, 0);
    CHECK_INT(h.held, 16);
    CHECK_INT(ip_recolour(&h, &o), 0);                                /* and only once */
    rcm_other = 0;
}

/* OWNER-ICON-COLOURS-INVERTED (s31 evidence icon-colours-inverted.md, rig 61, 3.2, depth 8, flat on, vantablack): RAM:iconpens read
 * pens 32-47 as 555555 bebebe bebebe 8d8d8d 454545 898989 868686 696969 3f3f3f 7a7a7a 505050 1a1a1a 222222 5c5c5c 222222 222222.
 * bebebe was read as the matte-black foreground and the pens as stale. They are vantablack's own: its #ffffff foreground becomes
 * GT_DARK_WHITE_FG (bebebe) under the owner rule, and its black ground lifts the backdrop to GT_BLACK_LIFT (222222). This pins the
 * rig numbers to the theme, so the held pens are proved current and the inverted look has another cause. */
static void test_icon_colours_inverted_pens_are_vantablack(void)
{
    static const char *vb = "mode = \"dark\"\naccent = \"#8d8d8d\"\nselection = \"#1a1a1a\"\nmuted = \"#7a7a7a\"\n"
                            "background = \"#000000\"\ndark_background = \"#090909\"\ndarker_background = \"#070707\"\n"
                            "lighter_background = \"#1a1a1a\"\nforeground = \"#ffffff\"\ndark_foreground = \"#505050\"\n"
                            "light_foreground = \"#ececec\"\nbright_foreground = \"#ffffff\"\n";
    static const unsigned long rig[16] = { 0x555555UL, 0xbebebeUL, 0xbebebeUL, 0x8d8d8dUL, 0x454545UL, 0x898989UL, 0x868686UL,
                                           0x696969UL, 0x3f3f3fUL, 0x7a7a7aUL, 0x505050UL, 0x1a1a1aUL, 0x222222UL, 0x5c5c5cUL,
                                           0x222222UL, 0x222222UL };
    gtheme t;
    uint8_t rgb[IP_PENS][3];
    desk_pen_plan p;
    int i;
    memset(&t, 0, sizeof t);
    gt_parse(vb, (long)strlen(vb), &t);
    desk_pen_plan_for(8, &p);
    CHECK_INT(p.icon_first, 32);
    CHECK_INT(p.icon_count, 16);
    ip_colours(&t, ip_ground(&t, 1, 0), rgb);
    for (i = 0; i < p.icon_count; i++) {
        const uint8_t *c = rgb[p.icon_slot + i];
        CHECK_INT((long)(((unsigned long)c[0] << 16) | ((unsigned long)c[1] << 8) | c[2]), (long)rig[i]);
    }
}

void suite_iconpen(void)
{
    test_icon_colours_inverted_pens_are_vantablack();
    test_colours_and_fold();
    test_plan_no_overlap();
    test_holder();
    test_free_list_model();
    test_sync();
    test_theme_ready();
    test_icons_keep_pens_when_flat_stops();
    test_main_takes_pens_at_boot();
    test_reopen_same_address();
    test_theme_switch_recolour();
}
