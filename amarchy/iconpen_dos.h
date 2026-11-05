/* iconpen_dos: the OS half of the shared icon pen holder (plan 2026-11-01-amarchy-filled-icon-set.md F14, H1).
 * One public record ("Amarchy.IconPens" semaphore, ip_shared) lives in memory for as long as an owner wants the 16 icon pens.
 * ObtainPen pens belong to the screen's colour map, not to a task, so the record outlives the amarchy-icons process that
 * created it; amarchy-flat's 1 s tick re-obtains them when the Workbench screen is reopened. The decisions are
 * iconpen_core.c (host-tested). The program defines struct IntuitionBase *IntuitionBase and struct GfxBase *GfxBase and opens
 * both before calling ipd_owner. */
#ifndef ICONPEN_DOS_H
#define ICONPEN_DOS_H

#include <stddef.h>
#include <stdint.h>
#include "iconpen_core.h"

#define IPD_SEM_NAME "Amarchy.IconPens"

struct List;
struct Screen;
struct ColorMap;
/* S57-ICL-PINK-SHADE: the ColorMap's own answer whether `pen` is free on it (an exclusive obtain of a pen anyone holds fails; one that
 * succeeds is handed straight back). The probe ip_holder_verify uses, shared with the icl shade cache and flat_pens. 0 for no map.
 * PalExtra is taken inside ObtainPen/ReleasePen only: a leaf lock, no new order. */
int ipd_pen_is_free(struct ColorMap *cm, int pen);
/* The Workbench screen found in the public screen list the caller has locked (LockPubScreenList); 0 when it is closed. */
struct Screen *ipd_wb_screen_in(struct List *pl);
/* The Workbench screen, locked (the caller UnlockPubScreen(0, scr)), or 0 when the Workbench is closed. NEVER call
 * LockPubScreen("Workbench") elsewhere: on OS 3.1 it reopens a closed Workbench and deadlocks against IPrefs (BOOT-31-WB-DEADLOCK). */
struct Screen *ipd_lock_wb_screen(void);

/* Add (on 1) or drop (on 0) `owner`, then bring the held pens in line with the owners and the Workbench screen as it is now.
 * Returns the reference count after. The record is freed with the last owner. */
int ipd_owner(unsigned owner, int on);
/* The 16 icon slot colours of the current theme (ip_colours on ip_ground): the palette amarchy-icons bakes into the FORM ICON. */
void ipd_icon_rgb(uint8_t rgb[16][3]);
/* S32-STALE-HELD-PENS: the screen epoch, changed by every sync that obtained or released the icon pens (Workbench closed, reopened, mode change); 0 without a record. */
unsigned ipd_epoch(void);
/* Re-sync without changing owners (screen reopened, theme changed). */
void ipd_resync(void);
/* A theme switch: rewrite the held pens 32..47 with SetRGB32 from the new theme (ip_recolour). Does nothing when no record exists. */
void ipd_recolour(void);
/* S57-PENS47-UNOWNED: write a live role pen (4-7) through the holder: 1 written (the holder owns it), 0 not ours or not on the
 * Workbench screen the pens are held on (skipped), -1 no holder runs (the caller writes it directly, as before) */
int ipd_role_write(int pen, uc_u32 rgb);
/* "pens-held N refs=M" from the record; no record reads "pens-held 0 refs=0". Needs no library. Returns the length. */
int ipd_status_line(char *out, size_t cap);

#endif
