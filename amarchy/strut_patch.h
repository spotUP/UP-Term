/* BAR-STRIP-RESERVE (design: thoughts/shared/research/2026-11-01-bar-strip-reserve-design.md): the commodity's
 * patch of intuition OpenWindowTagList (LVO -606) and OpenWindow (LVO -204). While the bar publishes its strip, a
 * window opened on the bar's screen gets a box out of the strip (win_strut_class / win_strut_clamp, host-tested);
 * the Workbench root window is shortened at publish and grown back at clear. Amiga only. */
#ifndef AMARCHY_STRUT_PATCH_H
#define AMARCHY_STRUT_PATCH_H

#define STRUT_SEM_NAME "Amarchy.Strut"
#define STRUT_LVO_OPENWINDOW (-204)
#define STRUT_LVO_OPENWINDOWTAGLIST (-606)

struct Screen;

/* 0 = installed; the patch stays off (and windows open as before) on any error. */
int strut_patch_install(void);
void strut_patch_remove(void);

/* The bar is open: its strip is the last height rows of scr. Shortens the desktop windows above it. No patch: no-op. */
void strut_publish(const struct Screen *scr, int height);
/* The bar is closing: the desktop windows grow back and the patch passes every open through. */
void strut_clear(void);
/* Writes the counters (opens, clamped, zoom, desktop) as AMARCHY_STRUT_* sentinels when they changed. */
void strut_sentinels(void);
/* The same counters for BAR STATUS (strut_opens strut_clamped strut_zoom); all 0 without the patch. */
void strut_counts(long *opens, long *clamped, long *zoom);

#endif
