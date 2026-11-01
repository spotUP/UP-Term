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
void ip_colours(const gtheme *t, uint8_t rgb[IP_PENS][3]);

/* The screen pen that icon slot (0..15) is drawn with at screen depth `depth` (EHB counts as 5):
 * depth >= 6: IP_FIRST + slot (held); depth 4 and 5: slots 8..15 are pens 8..15 (held), slots 0..7 fold; depth 2 and 3: every
 * slot folds. A fold lands on the theme role pen (DK_BG..DK_MUTED, 0..5; 0..3 on depth 2) whose colour is nearest the
 * slot's colour (ties: the lower pen), never on DK_GREEN / DK_RED. */
int ip_fold(const gtheme *t, int slot, int depth);

/* ---- the pen holder (L7) ---- */
typedef struct ip_ops {
    int (*obtain)(void *ctx, int pen, unsigned r, unsigned g, unsigned b);   /* the pen number, or -1 when it is taken */
    void (*release)(void *ctx, int pen);
    void *ctx;
} ip_ops;

typedef struct ip_holder {
    const void *screen;          /* identity of the screen the pens were obtained on */
    int depth;
    int held;                    /* pens held now */
    uint8_t have[IP_PENS];       /* have[i] = 1 when slot desk_pen_plan.icon_slot + i is held */
    int slot0, first, count;     /* the plan the pens were obtained under */
} ip_holder;

void ip_holder_init(ip_holder *h);
/* Release what is held, then obtain the pens of desk_pen_plan(depth) for rgb. Returns the pens held. A taken pen is skipped
 * (the applier then folds that slot). */
int ip_holder_obtain(ip_holder *h, const ip_ops *o, const void *screen, int depth, const uint8_t rgb[IP_PENS][3]);
/* 1 when the screen or its depth is not the one the pens were obtained on (the re-obtain trigger) */
int ip_holder_stale(const ip_holder *h, const void *screen, int depth);
/* Release every held pen (flat off, program exit). Returns the number released. */
int ip_holder_release(ip_holder *h, const ip_ops *o);
/* "pens-held N"; returns the length, 0 when cap is too small */
int ip_status_line(const ip_holder *h, char *out, size_t cap);

/* ---- the Amiga glue's decisions (amarchy/flat.c pens_sync) ---- */
enum { IP_SYNC_NONE, IP_SYNC_OBTAIN, IP_SYNC_RELEASE };
/* What a sync does: flat off or no Workbench screen -> release what is held (RELEASE only while pens are tied to a screen);
 * flat on and the screen or its depth is not the one the pens were obtained on (first run, Workbench closed and reopened,
 * mode change) -> obtain; else nothing. */
int ip_sync_decide(const ip_holder *h, int enabled, const void *screen, int depth);
/* ObtainPen takes 32-bit colour gun values: an 8-bit gun c becomes c * 0x01010101 */
unsigned long ip_scale32(unsigned c);
/* the depth ip_fold / desk_pen_plan_for take: the bitmap's planes, EHB (6 planes) counts as 5 */
int ip_screen_depth(int planes, int ehb);

#endif
