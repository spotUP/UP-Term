/* iconpen_core: the pure half of the Lucide icon pens (plan 2026-10-31-amarchy-lucide-icons.md section 3, rows L5, L7).
 * The 16 icon colours a theme gives (stored in the icon files as pens 32..47), the fold of those pens onto a shallow
 * screen, and the pen holder model that amarchy-flat drives with ObtainPen / ReleasePen. No OS calls: the holder takes
 * its obtain / release as callbacks, so tests/test_iconpen.c drives the same code the resident program runs. */
#ifndef ICONPEN_CORE_H
#define ICONPEN_CORE_H

#include <stddef.h>
#include <stdint.h>
#include "gthemes_core.h"
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
/* Release every held pen (flat off, program exit). Returns the number released. */
int ip_holder_release(ip_holder *h, const ip_ops *o);
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

/* S57-FREEZE-2IN9: the psn_Flags bit of a public screen node that is not public (yet): intuition's PSNF_PRIVATE. */
#define IP_PSN_PRIVATE 0x0001
/* Whether LockPubScreen(name) will FIND the public screen (any name, the Workbench included), from its node read under the public screen list lock:
 * the node is listed, has a screen and is not private. While LoadWB opens the Workbench the node is listed but still
 * private; Intuition then treats it as absent and takes its "reopen the Workbench" path, which waits for the task
 * that is opening it. A caller that holds the list lock must not make that call: the opening task needs the list
 * lock to make the screen public, so both wait for ever (evidence: s57-evidence/freeze-9boot-rig145.md). */
int ip_pub_lockable(int listed, int has_screen, unsigned flags);

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
} ip_diff_entry;

typedef struct ip_diff {
    int count;
    ip_diff_entry e[2 * IP_SCREENS];
} ip_diff;

/* Pure. Per name (case-insensitive): in new only APPEARED, in old only GONE, in both with another screen pointer or depth
 * REOPENED, else SAME. Order: the new snapshot's names, then the GONE ones. */
void ip_screens_diff(const ip_snap *old_s, const ip_snap *new_s, ip_diff *out);
/* Folds a diff into the table. Slot 0 is never touched. APPEARED adds a slot (a PRIVATE node adds none; a name that already
 * has a slot counts as a reopen). GONE forgets the slot's pens WITHOUT any release (the ColorMap went with the screen) and
 * keeps the slot. REOPENED forgets the pens the same way and bumps ONLY that slot's epoch. A PRIVATE node keeps its slot as it
 * is (no forget, no epoch). Returns the number of slots changed. */
int ip_screens_apply(ip_screens *t, const ip_diff *d);

#endif
