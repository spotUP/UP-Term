/* iconpen_core: the pure half of the Lucide icon pens (plan 2026-10-31-amarchy-lucide-icons.md section 3, rows L5, L7).
 * The 16 icon colours a theme gives (stored in the icon files as pens 32..47), the fold of those pens onto a shallow
 * screen, and the pen holder model that amarchy-flat drives with ObtainPen / ReleasePen. No OS calls: the holder takes
 * its obtain / release as callbacks, so tests/test_iconpen.c drives the same code the resident program runs. */
#ifndef ICONPEN_CORE_H
#define ICONPEN_CORE_H

#include <stddef.h>
#include <stdint.h>
#include "gthemes_core.h"
#include "../handler/pubscr_core.h"
#include "desk_core.h"

#define IP_PENS 16
#define IP_FIRST 32   /* the stored pen of icon slot 0 */

/* the icon slots (stored pen = IP_FIRST + slot); the table of section 3 */
enum {
    IP_FG33, IP_FG100, IP_SELFG100, IP_ACCENT100, IP_ACCENT33, IP_FG66, IP_SELFG66, IP_ACCENT66,
    IP_MUTED33, IP_MUTED100, IP_SELFG33, IP_SELBG, IP_SPARE12, IP_MUTED66, IP_SPARE14, IP_SPARE15
};

/* The colour of every slot. FG, ACCENT and MUTED levels blend from the theme background toward the role (33 / 66 / 100 %),
 * SELFG levels blend from the selection colour toward the foreground (the selected image sits on a SELBG plate), SELBG is
 * the selection colour, the spare slots are the background. */
void ip_colours(const gtheme *t, uc_u32 ground, uint8_t rgb[IP_PENS][3]);
/* ICON-AA-WRONG-BG: the colour the icons are drawn ON, the Workbench backdrop pen's colour (desk_pen_colour: the darker tone when flat
 * is on, else the muted role). `ground` of ip_colours: the AA ramp (FG/ACCENT/MUTED 33/66 %) must start at it, or the edge
 * pixels form a fringe against the real backdrop. flat / mask as desk_live_follow takes them. */
uc_u32 ip_ground(const gtheme *t, int flat, unsigned mask);

/* The screen pen that icon slot (0..15) is drawn with at screen depth `depth` (EHB counts as 5):
 * depth >= 6: IP_FIRST + slot (held); depth 4 and 5: slots 8..15 are pens 8..15 (held), slots 0..7 fold; depth 2 and 3: every
 * slot folds. A fold lands on the theme role pen (DK_BG..DK_MUTED, 0..5; 0..3 on depth 2) whose colour is nearest the
 * slot's colour (ties: the lower pen), never on DK_GREEN / DK_BACKDROP. */
int ip_fold(const gtheme *t, int slot, int depth);

/* ---- the pen holder (L7) ---- */
typedef struct ip_ops {
    int (*obtain)(void *ctx, int pen, unsigned r, unsigned g, unsigned b);   /* the pen number, or -1 when it is taken */
    void (*release)(void *ctx, int pen);
    void *ctx;
    /* The rest are used by ip_sync only (the screen follow path); 0 = absent. */
    /* 1 when `pen` is allocated to nobody on the CURRENT Workbench screen's ColorMap (FLAT-PENS47-RESET): the ColorMap is the
     * one record of who holds a pen, so the holder checks its belief against it (ip_holder_verify) */
    int (*is_free)(void *ctx, int pen);
    /* load the theme and the switches desk_live_follow takes (amconf_desk_flat_pens, amconf_desk_mask) for an obtain */
    void (*theme)(void *ctx, gtheme *t, int *flat, unsigned *mask);
    /* FLAT-BOOT-FREEZE: optional, 0 = always ready. Asked once ip_sync has decided on an obtain, before anything changes: 0 means
     * the theme is not loaded yet, ip_sync returns IP_SYNC_NEED_THEME and the caller loads it (outside any lock) and syncs again */
    int (*theme_ready)(void *ctx);
    /* set one live role pen of the current screen to rgb (0xRRGGBB) */
    void (*set_rgb)(void *ctx, int pen, uc_u32 rgb);
    /* TE-10.5b2: optional, 0 = absent (the plan is empty). Used by ip_sync_plan only: the pen plan (gt_screen_apply_plan) of the screen
     * the select callback chose, written into out[0..cap); returns the entries written. */
    int (*plan)(void *ctx, int depth, gt_plan_pen *out, int cap);
} ip_ops;

typedef struct ip_holder {
    const void *screen;          /* identity of the screen the pens were obtained on */
    int depth;
    int held;                    /* pens held now */
    uint8_t have[IP_PENS];       /* have[i] = 1 when slot desk_pen_plan.icon_slot + i is held */
    int slot0, first, count;     /* the plan the pens were obtained under */
    uint8_t rgb[IP_PENS][3];     /* the colours the held pens carry now (ip_recolour compares against them) */
    int recheck;                 /* ip_holder_verify: a missing pen is free now; the next sync re-obtains */
    int new_identity;            /* the last ip_sync changed the screen identity (ip_sync_new_identity); survives ip_holder_init */
    /* S57-PENS47-UNOWNED: the live role pens (desk_live_role_pens: 4-7 on a screen deeper than 3 planes) the sync writes the theme
     * onto are obtained exclusive like the icon pens, held and released with them (rhave[i] = 1 when rpen[i] is held) */
    int rcount;
    int rpen[4];
    uint8_t rhave[4];
    uc_u32 rrgb[4];              /* the colour each role pen carries now (ip_recolour compares against it) */
    /* TE-10.5b: the exclusive pens of the screen's pen plan (gt_screen_apply_plan), held and released with the others. Only HELD
     * entries are stored (pcount of them); capacity = the most entries the plan can return (one per element, GTE_COUNT) */
    int pcount;
    short ppen[GTE_COUNT];
    uc_u32 prgb[GTE_COUNT];      /* the colour each plan pen carries now (ip_plan_sync recolours when the plan changes it) */
} ip_holder;

void ip_holder_init(ip_holder *h);
/* Release what is held, then obtain the pens of desk_pen_plan(depth) for rgb. Returns the pens held. A taken pen is skipped
 * (the applier then folds that slot). */
int ip_holder_obtain(ip_holder *h, const ip_ops *o, const void *screen, int depth, const uint8_t rgb[IP_PENS][3]);
/* 1 when the screen or its depth is not the one the pens were obtained on, or ip_holder_verify found pens to re-obtain (the
 * re-obtain trigger) */
int ip_holder_stale(const ip_holder *h, const void *screen, int depth);
/* FLAT-PENS47-RESET cause 2: the screen ADDRESS is not a screen identity. A Workbench reopen can allocate the new Screen at the
 * freed one's address (rig 14, 3.2, depth 8), and address + depth then say "same screen" while the 16 pens went with the old
 * ColorMap. The identity check therefore asks the ColorMap (o->is_free) on the screen the holder believes it holds on:
 * - a pen the holder holds is free there: the screen was reopened. Nothing is held on it, so the holder forgets its pens
 *   without ReleasePen (releasing a pen we never obtained on this ColorMap would corrupt its free list) and returns 1;
 * - a pen the holder could not get is free now: the holder is marked stale (re-obtain, releasing its pens normally), returns 1;
 * - the same address at another depth is a reopened screen too: forget, returns 1.
 * Else 0 (also with no o->is_free, another screen, or nothing obtained). */
int ip_holder_verify(ip_holder *h, const ip_ops *o, const void *screen, int depth);
/* S57-ICL-PINK-SHADE: the same question for pens kept outside an ip_holder (the icl shade cache, flat_pens), which trust
 * "same screen + same ipd epoch" and so miss a Workbench reopen at the same address before the next pens_sync. 1 when any of the
 * n pens we believe we hold is free on the ColorMap (is_free(ctx, pen)): the screen was reopened, the pens went with the old map and
 * the caller forgets them WITHOUT ReleasePen. 0 when all are held, n is 0 or is_free is 0. */
int ip_any_free(int (*is_free)(void *ctx, int pen), void *ctx, const int *pens, int n);
/* ICON-AA-WRONG-BG / ICONPEN-THEME-SWITCH-RECOLOUR: a theme switch on the same screen. Re-reads the theme (o->theme), and every held pen
 * whose colour changed is rewritten with o->set_rgb (SetRGB32); no release, no obtain. Returns the pens rewritten, 0 when nothing is
 * held or the colours are the same. */
int ip_recolour(ip_holder *h, const ip_ops *o);
/* S57-PENS47-UNOWNED: 1 when `pen` is a role pen the holder holds on its screen (the caller may write it; rrgb takes `rgb`), else 0: the
 * pen is not ours and must not be written */
int ip_role_claim(ip_holder *h, int pen, uc_u32 rgb);
/* Release every held pen (flat off, program exit). Returns the number released. */
int ip_holder_release(ip_holder *h, const ip_ops *o);
/* TE-10.5b: make the held plan pens match plan[0..n) (n = 0 releases them all). An exclusive entry not held is obtained (a refused
 * pen is skipped and retried next sync); a held one whose colour changed is recoloured with o->set_rgb; a held one no longer
 * exclusive or absent is released. exclusive = 0 entries do nothing. A pen the holder already holds as icon or role pen is skipped.
 * Returns the plan pens held after the call. */
int ip_plan_sync(ip_holder *h, const ip_ops *o, const gt_plan_pen *plan, int n);
/* TE-10.5b2: ORs the pens of the holder's own plan (ppen[0..pcount)) into a 256-bit free mask (pen p = bit p & 31 of word
 * p >> 5), so a re-plan can offer the pens the holder already holds instead of releasing and re-obtaining them. */
void ip_holder_plan_mask(const ip_holder *h, uc_u32 mask[GT_MASK_WORDS]);
/* TE-10.5b2b: the free mask of one screen for gt_screen_apply_plan. mask is CLEARED, then gets bit p (word p >> 5, bit p & 31) for every
 * pen p < (1 << depth) (at most 256) that o->is_free reports free on the screen's ColorMap (no is_free = none free), plus the pens the
 * holder already holds for its plan (ip_holder_plan_mask), so a re-plan can keep them. No bit at or above 1 << depth. */
void ip_plan_free_mask(const ip_holder *h, const ip_ops *o, int depth, uc_u32 mask[GT_MASK_WORDS]);
/* "pens-held N"; returns the length, 0 when cap is too small */
int ip_status_line(const ip_holder *h, char *out, size_t cap);

/* ---- the Amiga glue's decisions (amarchy/flat.c pens_sync) ---- */
enum { IP_SYNC_NONE, IP_SYNC_OBTAIN, IP_SYNC_RELEASE, IP_SYNC_NEED_THEME };
/* What a sync does: `hold` 0 (program quitting, kill switch held, graphics < V39) or no Workbench screen -> release what
 * is held (RELEASE only while pens are tied to a screen); `hold` 1 and the screen or its depth is not the one the pens were
 * obtained on (first run, Workbench closed and reopened, mode change) -> obtain; else nothing. */
int ip_sync_decide(const ip_holder *h, int hold, const void *screen, int depth);
/* One sync of the screen follow path (amarchy/iconpen_dos.c pens_sync; the caller holds the public screen list):
 * ip_holder_verify, ip_sync_decide, then RELEASE: ip_holder_release; OBTAIN (a new Workbench screen or depth): o->theme,
 * ip_colours + ip_holder_obtain, then the live role pens the new screen needs (desk_live_follow at `planes`, FLAT-PENS47-RESET
 * cause 1: a reopened screen starts with graphics-default pens 4-7) through o->set_rgb. Returns the action taken. */
/* S32-PEN-EPOCH-LEAK: whether a sync changed the identity of the screen the pens live on (the epoch other clients compare).
 * old_screen = the holder's screen before the sync, new_screen = the Workbench screen now, act = the ip_sync result, lost = 1
 * when ip_holder_verify found our exclusive pens freed on the same address (a reopen: the ColorMap was rebuilt). 1 when the
 * screen pointer changed or went away, or act is OBTAIN with the same pointer and lost; 0 for NONE, a RELEASE with the screen
 * still open (hold switched off) and a recheck OBTAIN on the same screen (those keep the clients' pens valid). */
int ip_sync_new_identity(const void *old_screen, const void *new_screen, int act, int lost);
int ip_sync(ip_holder *h, const ip_ops *o, int hold, const void *screen, int depth, int planes);
/* TE-10.5b2: the sync of a screen other than the Workbench (slots >= 1 of ip_screens_sync). Such a holder holds ONLY the pens of
 * the theme's pen plan (o->plan, exclusive entries): never the 16 icon pens, never role pens 4-7 on its own (h->count, h->held and
 * h->rcount stay 0). Same IP_SYNC_* codes as ip_sync: ip_holder_verify (a plan pen the ColorMap reports free = the screen was
 * reopened at the same address: forget, no release), ip_sync_decide, then RELEASE = ip_holder_release; OBTAIN = IP_SYNC_NEED_THEME
 * with nothing changed while o->theme_ready says 0, else o->plan and ip_plan_sync, and the identity is recorded like ip_sync does
 * (h->screen, h->depth, h->new_identity). A tick where nothing is stale calls no op but the is_free checks of verify.
 * Re-plan request: set h->recheck = 1 on a held plan holder (the DOS half does it on a theme or rule change); the next sync re-runs
 * the plan: a changed colour is rewritten, a pen no longer planned exclusive is released, a missing one is obtained. A plan pen the
 * ColorMap refused is skipped and leaves h->recheck = 1, so the next sync retries it. */
int ip_sync_plan(ip_holder *h, const ip_ops *o, int hold, const void *screen, int depth);
/* Whether amarchy-flat holds the icon pens: for its whole run, whatever the flat switch says (L7 cause 2). graphics puts a
 * released pen at the HEAD of the PalExtra free list (rig 10, 3.2: after flat off the free order read 47 46 .. 32 244 ..),
 * so the next ObtainBestPen of any client (the dim pen, Amarchy's own theme pens, colour icons) took 47, then 46, and
 * flat on again found them shared. The flat switch is deliberately not an input. */
int ip_hold_wanted(int quit, int held, int gfx_v39);
/* ---- the shared holder (plan 2026-11-01-amarchy-filled-icon-set.md F14, H1) ----
 * The pens are held for as long as ANY owner wants them, not for as long as amarchy-flat runs: icons that name pens 32..47 are
 * on disk after amarchy-icons APPLY exits, flat on or off. Each owner is one bit, so a second APPLY or a second RESTORE counts
 * once (the reference count is the number of owners). The pens are released when the last owner lets go. */
#define IP_OWNER_FLAT 1u    /* amarchy-flat, while it runs and no other patcher holds the kill switch */
#define IP_OWNER_ICONS 2u   /* amarchy-icons APPLY, until RESTORE */
#define IP_OWNER_MAIN 4u    /* Amarchy (main), while it runs and the icons are applied: the holder record lives in RAM, so after a reboot nobody else holds the pens the .info files on disk name */

/* What Amarchy main does at start and on every 1 s tick: `applied` = the amarchy-icons manifest exists, `owned` = main holds
 * IP_OWNER_MAIN now. TAKE: become an owner; DROP: RESTORE ran, give the reference up; SYNC: re-take after a Workbench reopen
 * (ipd_resync); NONE: nothing to do. */
enum { IP_MAIN_NONE, IP_MAIN_TAKE, IP_MAIN_DROP, IP_MAIN_SYNC };
int ip_main_step(int applied, int owned);

typedef struct ip_shared {
    unsigned owners;
    ip_holder h;
    unsigned epoch;     /* S32-STALE-HELD-PENS: counts the syncs that obtained or released (a new Workbench screen); other clients compare it to know their pens on the old screen are gone */
} ip_shared;

void ip_shared_init(ip_shared *s);
/* Add (on 1) or drop (on 0) one owner; returns the reference count after. Idempotent per owner. */
int ip_shared_set(ip_shared *s, unsigned owner, int on);
int ip_shared_refs(const ip_shared *s);
/* The `hold` input of ip_sync_decide: some owner is left and graphics is V39 or newer. */
int ip_shared_hold(const ip_shared *s, int gfx_v39);

/* ObtainPen takes 32-bit colour gun values: an 8-bit gun c becomes c * 0x01010101 */
unsigned long ip_scale32(unsigned c);
/* the depth ip_fold / desk_pen_plan_for take: the bitmap's planes, EHB (6 planes) counts as 5 */
int ip_screen_depth(int planes, int ehb);


/* ---- TE-10.3: the pen holder per public screen (design s58 te10-design.md 2.2 and 5) ----
 * Slot 0 is always "Workbench" and is the ip_shared the Workbench path already uses; the diff never changes it. A slot of
 * another public screen holds its own ip_shared. Public screen names compare CASE-INSENSITIVELY (ASCII), as Intuition's
 * LockPubScreen does. Limit: IP_SCREENS slots (the Workbench plus 32 rules plus spare); a full table refuses an add and stays
 * unchanged, the caller then simply has no slot for that screen. */
#define IP_PSN_NAME 140      /* MAXPUBSCREENNAME (139) + 1; the OS constant is not visible on the host */
#define IP_SCREENS 40

typedef struct ip_screen_slot {
    char name[IP_PSN_NAME];
    ip_shared sh;
    unsigned epoch;          /* bumped when the screen of this name is reopened (new identity); slot 0 keeps sh.epoch */
    int owed;                /* TE-10.5b2: a same-address reopen was forgotten but its sync answered NEED_THEME; the epoch bump is owed */
} ip_screen_slot;

typedef struct ip_screens {
    int count;
    ip_screen_slot slot[IP_SCREENS];
} ip_screens;

/* count 1, slot 0 "Workbench" with a fresh ip_shared */
void ip_screens_init(ip_screens *t);
/* slot index of `name` (case-insensitive), -1 when absent */
int ip_screens_find(const ip_screens *t, const char *name);
/* index of the new slot (or of the existing one); -1 when the table is full or the name is empty or longer than 139 */
int ip_screens_add(ip_screens *t, const char *name);
/* removes the slot and shifts the rest down; 1 done, 0 absent, -1 slot 0 (refused) */
int ip_screens_drop(ip_screens *t, const char *name);

/* one public screen of the list snapshot taken under LockPubScreenList */
typedef struct ip_snap_entry {
    char name[IP_PSN_NAME];
    const void *screen;
    unsigned flags;          /* psn_Flags; IP_PSN_PRIVATE */
    int depth;
    int ehb;
} ip_snap_entry;

typedef struct ip_snap {
    int count;
    ip_snap_entry e[IP_SCREENS];
} ip_snap;

enum { IP_DIFF_SAME, IP_DIFF_APPEARED, IP_DIFF_GONE, IP_DIFF_REOPENED };

typedef struct ip_diff_entry {
    char name[IP_PSN_NAME];
    int kind;
    unsigned flags;          /* of the new entry (of the old one for GONE) */
    const void *screen;      /* identity of the new entry (of the old one for GONE) */
    int depth;
} ip_diff_entry;

typedef struct ip_diff {
    int count;
    ip_diff_entry e[2 * IP_SCREENS];
} ip_diff;

/* Pure. Per name (case-insensitive): in new only APPEARED, in old only GONE, in both with another screen pointer or depth
 * REOPENED, else SAME. Order: the new snapshot's names, then the GONE ones. */
void ip_screens_diff(const ip_snap *old_s, const ip_snap *new_s, ip_diff *out);
/* Folds a diff into the table. Slot 0 is never touched. The rules, in order:
 *   GONE              forgets the slot's pens WITHOUT any release (the ColorMap went with the screen) and keeps the slot,
 *                     private node or not. No epoch change: the reopen bumps it.
 *   PRIVATE, listed   keeps its slot as it is (no forget, no epoch) and adds none: "not open yet", never "gone".
 *   public, no slot   adds a slot, whatever the diff kind. A screen is listed PRIVATE first and made public later on the
 *                     same address, so its first public sighting is SAME, not APPEARED.
 *   public, slot      the holder's own identity decides when it has one (pens obtained on a screen): the same screen and
 *                     depth changes nothing (no leak when a diff calls a held screen APPEARED), another one forgets the pens
 *                     without a release and bumps ONLY that slot's epoch (a reopen is seen also across a private phase).
 *                     With no identity the diff kind decides: SAME changes nothing, APPEARED and REOPENED bump the epoch.
 * Returns the number of slots changed. */
int ip_screens_apply(ip_screens *t, const ip_diff *d);

/* ---- TE-10.4: the per-screen sync ----
 * The public screen list snapshot is taken by the OS half; this is its pure part. */
/* Appends one screen to a snapshot (names longer than 139 or an empty name, and a full snapshot, are refused). depth is
 * ip_screen_depth's answer, ehb the screen's half-brite mode. 1 added, 0 refused. */
int ip_snap_add(ip_snap *s, const char *name, const void *screen, unsigned flags, int depth, int ehb);

/* Called by ip_screens_sync before slot `slot`'s ip_sync: the caller points the ops' context at that slot's screen (`e`, 0 when
 * the screen is not in the snapshot: only slot 0, the Workbench), its holder and its theme. */
typedef void (*ip_slot_ctx_fn)(void *ctx, int slot, const ip_snap_entry *e);

/* One ip_sync per slot over the table `t` and the snapshot `snap` the table was just brought in line with (ip_screens_diff +
 * ip_screens_apply). hold[i] is slot i's `hold` input. Slot 0 (the Workbench) is synced as ever, also with no screen (closed).
 * Another slot is synced only when its screen is in the snapshot and PUBLIC: a private node is "not open yet", it gets no
 * pen call, and a slot whose screen left the snapshot was already forgotten by ip_screens_apply (never a release). Slot 0's
 * epoch (sh.epoch) goes up when its identity changed; the other slots' epochs are ip_screens_apply's. need[i] (optional, one
 * int per slot) is 1 for a slot that answered IP_SYNC_NEED_THEME, else 0. Returns the number of slots that need a theme. */
int ip_screens_sync(ip_screens *t, const ip_snap *snap, const ip_ops *o, const int *hold, ip_slot_ctx_fn select, void *ctx, int *need);
/* The slots other than Workbench that hold pens now (the "screens=<n>" of the status line). */
/* TE-10.5b2b: ask every slot >= 1 for a re-plan at its next sync (recheck = 1 on its holder; slot 0 is untouched). The DOS half calls
 * it when the screen rules or the main theme were reread. A holder that holds nothing yet obtains at its next sync anyway (decide
 * says OBTAIN for hold 1); with hold 0 and nothing held decide says NONE and the flag is harmless; a slot whose screen is not open is
 * skipped by ip_screens_sync and keeps the flag until it opens. */
void ip_screens_recheck_plans(ip_screens *t);
int ip_screens_held(const ip_screens *t);
/* The epoch of the slot named `name` (no case); slot 0's is its ip_shared epoch; 0 when there is no such slot. */
unsigned ip_screens_epoch(const ip_screens *t, const char *name);
/* TE-10.6b: the same for the slot whose holder holds its pens on `screen` (the pointer, for a draw hook that has no name); 0 when no
 * slot holds pens on that screen or screen is 0. */
unsigned ip_screens_epoch_of_screen(const ip_screens *t, const void *screen);

#endif
