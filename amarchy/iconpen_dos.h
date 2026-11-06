/* iconpen_dos: the OS half of the shared icon pen holder (plan 2026-11-01-amarchy-filled-icon-set.md F14, H1).
 * One public record ("Amarchy.IconPens.5" semaphore, ip_shared) lives in memory for as long as an owner wants the 16 icon pens.
 * ObtainPen pens belong to the screen's colour map, not to a task, so the record outlives the amarchy-icons process that
 * created it; amarchy-flat's 1 s tick re-obtains them when the Workbench screen is reopened. The decisions are
 * iconpen_core.c (host-tested). The program defines struct IntuitionBase *IntuitionBase and struct GfxBase *GfxBase and opens
 * both before calling ipd_owner. */
#ifndef ICONPEN_DOS_H
#define ICONPEN_DOS_H

#include <stddef.h>
#include <stdint.h>
#include <exec/semaphores.h>
#include "iconpen_core.h"
#include "../handler/pubscr.h"

/* The name carries the record layout: ".5" adds screen_applies; ".4" was the record before it; ".3" adds the plan pens to the holder (TE-10.5b); ".2" was the per-screen table (TE-10.4). A binary still running from before an update looks
 * for the old name and never reads this layout. 18 characters, fits ipd_rec.name[20]. */
#define IPD_SEM_NAME "Amarchy.IconPens.5"

/* The public record. Public so tests/amiga/iconpens.c reads the real layout; nobody else touches it. Everything big lives here
 * (AllocVec in find_rec), never on a stack. */
typedef struct ipd_stheme {      /* the theme of one slot, read from file with the list unlocked; valid for one sync (have) */
    gtheme t;
    int flat;
    unsigned mask;
    int have;
    char name[GT_NAME];          /* the theme the screen rules give this slot's screen */
} ipd_stheme;

typedef struct ipd_rec {
    struct SignalSemaphore ss;   /* first: AddSemaphore's node is the allocation's start */
    char name[20];
    ip_screens t;                /* slot 0 = Workbench; t.slot[0].sh is the ip_shared of the single-screen days */
    ip_snap snap;                /* the public screen list as the last sync saw it */
    ip_snap cur;                 /* work: the list now */
    ip_diff diff;                /* work: snap -> cur */
    gt_screen_rules rules;       /* ENV:Amarchy/screen-themes + the screen-themes key */
    char main_theme[GT_NAME];    /* the main theme name (AM_GTHEME), read with the rules */
    int rules_dirty;             /* reread rules and main theme at the next sync (starts 1; ipd_rules_changed) */
    ipd_stheme st[IP_SCREENS];
    unsigned long screen_applies; /* TE-10.5b2b sentinel: +1 each time the plan op returned a plan for a slot >= 1 (the sync may run in any program) */
} ipd_rec;

struct List;
struct Screen;
struct ColorMap;
/* S57-ICL-PINK-SHADE: the ColorMap's own answer whether `pen` is free on it (an exclusive obtain of a pen anyone holds fails; one that
 * succeeds is handed straight back). The probe ip_holder_verify uses, shared with the icl shade cache and flat_pens. 0 for no map.
 * PalExtra is taken inside ObtainPen/ReleasePen only: a leaf lock, no new order. */
int ipd_pen_is_free(struct ColorMap *cm, int pen);
/* The Workbench screen found in the public screen list the caller has locked (LockPubScreenList); 0 when it is closed.
 * The screen may still be private (LoadWB is opening it): read it under the list lock, never LockPubScreen it from here. */
struct Screen *ipd_wb_screen_in(struct List *pl);
/* The same for any public screen name (strcmp); 0 for a null or empty name or a screen that is not listed. */
struct Screen *ipd_pub_screen_in(struct List *pl, const char *name);
/* The Workbench screen, locked (the caller UnlockPubScreen(0, scr)), or 0 when the Workbench is closed or still opening
 * (ask again later). NEVER call LockPubScreen("Workbench") elsewhere: Intuition reopens a Workbench it does not find, and
 * that deadlocks against IPrefs closing it (BOOT-31-WB-DEADLOCK) or LoadWB opening it (S57-FREEZE-2IN9). */
struct Screen *ipd_lock_wb_screen(void);
/* ipd_lock_default_pub_screen() lives in handler/pubscr.h (included above). */

/* Add (on 1) or drop (on 0) `owner`, then bring the held pens in line with the owners and the Workbench screen as it is now.
 * Returns the reference count after. The record is freed with the last owner. */
int ipd_owner(unsigned owner, int on);
/* The 16 icon slot colours of the current theme (ip_colours on ip_ground): the palette amarchy-icons bakes into the FORM ICON. */
void ipd_icon_rgb(uint8_t rgb[16][3]);
/* S32-STALE-HELD-PENS: the screen epoch, changed by every sync that obtained or released the icon pens (Workbench closed, reopened, mode change); 0 without a record. */
unsigned ipd_epoch(void);
/* TE-10.4: the epoch of the pens on the public screen `name` (no case): "Workbench" gives ipd_epoch(); 0 for a screen with no slot or without a record. */
unsigned ipd_epoch_of(const char *name);
/* TE-10.4: reads ENV:Amarchy/screen-themes and the screen-themes key into *out (default "workbench" and no rules when the file is
 * missing). DOS I/O: never with the public screen list locked. */
void ipd_screen_rules_load(gt_screen_rules *out);
/* TE-10.7: the public name of the screen `scr` copied into out (cap bytes), "" when it is no public screen. Takes the public screen
 * list lock itself and unlocks before it returns. */
void ipd_pub_screen_name_of(struct Screen *scr, char *out, int cap);
/* TE-10.7: the theme a window Amarchy draws itself (the drawn menu, the theme windows) takes on `scr`: the screen rules' theme for
 * that screen, else the main theme. DOS I/O: never with the public screen list locked. */
void ipd_screen_theme_name(struct Screen *scr, char *out, int cap);
/* TE-10.7: calls of ipd_screen_theme_name (reachability counter). */
extern unsigned long ipd_screen_theme_asks;
/* TE-10.4: the rules (or the main theme) changed: the next sync rereads them. Does nothing without a record. */
void ipd_rules_changed(void);
/* Re-sync without changing owners (screen reopened, theme changed). */
void ipd_resync(void);
/* A theme switch: rewrite the held pens 32..47 with SetRGB32 from the new theme (ip_recolour). Does nothing when no record exists. */
void ipd_recolour(void);
/* S57-PENS47-UNOWNED: write a live role pen (4-7) through the holder: 1 written (the holder owns it), 0 not ours or not on the
 * Workbench screen the pens are held on (skipped), -1 no holder runs (the caller writes it directly, as before) */
int ipd_role_write(int pen, uc_u32 rgb);
/* "pens-held N refs=M screens=K applies=A" (K = other screens holding pens, A = plans applied to them) from the record; no record reads "pens-held 0 refs=0 screens=0 applies=0". Needs no library. Returns the length. */
int ipd_status_line(char *out, size_t cap);

#endif
