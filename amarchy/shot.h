/* shot: screenshots for Amarchy (P4 R4.3-R4.5). Reads a rectangle of a screen into an IFF ILBM file through
 * shot_core, then puts the finished file on clipboard unit 0 (decision D3). Chipset and palette screens only
 * (depth 1..8); a deeper screen is refused with SHOT_DEEP until the owner decides how to store RTG truecolour
 * (thoughts/shared/research/2026-10-29-p4-probe.md, open question Q1). */
#ifndef SHOT_H
#define SHOT_H

struct Screen;

#define SHOT_OK     0
#define SHOT_NOSCR  1   /* no screen / no target window */
#define SHOT_DEEP   2   /* deeper than 8 bits: not decided */
#define SHOT_IO     3   /* directory, file or memory failure */
#define SHOT_BAD    4   /* empty rectangle */

/* Capture screen rectangle (x, y, w, h; clipped to the screen) to a new file in dir (created when missing),
 * the name in path (cap bytes). clip: also put it on the clipboard. Returns SHOT_*. */
int shot_take(struct Screen *sc, long x, long y, long w, long h, const char *dir, int clip, char *path, long cap);

/* The three modes. Each counts one shot on success (shot_count) and keeps the last path (shot_last). */
int shot_screen(const char *dir);           /* the front screen, whole */
int shot_window(const char *dir);           /* the active window's rectangle (win_target_rect), overlaps included */
int shot_region(const char *dir);           /* opens the rubber-band window; the shot happens in shot_region_serve */

long shot_count(void);
const char *shot_last(void);
int shot_last_rc(void);
const char *shot_default_dir(void);         /* RAM:Shots (decision D1) */

/* The region window's signal bit mask (0 when none is open) and its IDCMP handler: release captures, Esc cancels. */
unsigned long shot_region_sigmask(void);
void shot_region_serve(void);
int shot_region_open_p(void);

#endif
