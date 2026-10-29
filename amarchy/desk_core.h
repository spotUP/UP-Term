/* desk_core: the pure half of the Amarchy native desktop theme (plan 2026-10-29-amarchy-p1a-native-theme.md). A gtheme
 * becomes the bytes of the Workbench preference files: palette.prefs (PALT), wbpattern.prefs (PTRN), pointer.prefs
 * (PNTR colour tables), font.prefs (FONT pens and name), plus the contrast guard, the target table and the revert
 * bookkeeping. No OS calls: buffers in, buffers out. The file layouts come from the probe note
 * thoughts/shared/research/2026-10-29-amarchy-p1a-prefs-probe.md and the goldens in tests/data/desk/. */
#ifndef DESK_CORE_H
#define DESK_CORE_H

#include "gthemes_core.h"

/* the eight palette roles of the flat mapping (OD13): the pen index of a role is its enum value */
enum { DK_BG, DK_DARKER, DK_FG, DK_ACCENT, DK_SELECTION, DK_MUTED, DK_GREEN, DK_RED, DK_ROLES };

/* the DrawInfo slots of pap_4ColorPens / pap_8ColorPens, in the order of the file */
enum {
    DK_DETAIL, DK_BLOCK, DK_TEXT, DK_SHINE, DK_SHADOW, DK_FILL, DK_FILLTEXT, DK_BACKGROUND,
    DK_HIGHLIGHTTEXT, DK_BARDETAIL, DK_BARBLOCK, DK_BARTRIM, DK_EXTRA, DK_SLOTS
};

#define DK_PALETTE_SIZE 434L /* a palette.prefs: FORM + PRHD + PALT of 400 bytes */
#define DK_WBPATTERN_SIZE 410L /* three PTRN chunks of depth 3 */

/* brightness 0..255 and a contrast ratio times 100 (100 = equal) from it; integer only */
int desk_luma(uc_u32 rgb);
int desk_contrast(uc_u32 a, uc_u32 b);

/* the colour of a palette role (0xRRGGBB) */
uc_u32 desk_role(const gtheme *t, int role);

/* R1.18a: the darker tone (OD5): the theme's darker_background when it is darker than the background and keeps
 * its distance, else the background at 80 %. Pure black stays black (nothing is darker). */
uc_u32 desk_darker(const gtheme *t);

/* R1.19: the pen index of every DrawInfo slot (OD13) into pens[DK_SLOTS] */
void desk_pens(unsigned char pens[DK_SLOTS]);

/* R1.17: 1 when a Palette USE of the pens desk_pens gives makes IPrefs reset the Workbench screen. DrawInfo dri_Pens are fixed
 * at screen open, so only a pen array that differs from the live screen's (cur, ncur = dri_NumPens, the first ncur slots are
 * compared) reopens it; colour values alone load live. ncur <= 0 (no DrawInfo): unknown, 1. */
int desk_pens_reset(const unsigned short *cur, int ncur);

/* R1.2: the palette.prefs of the theme with ncolors (4 or 8) colours; returns DK_PALETTE_SIZE, 0 on a bad argument */
long desk_palette(const gtheme *t, int ncolors, unsigned char *out, long cap);

/* R1.3, R1.20: the wbpattern.prefs (ROOT, DRAWER, SCREEN chunks). depth = the screen depth (capped at 3), tiled = 1
 * puts the sparse dot grid on the root. prev/prevlen = the file now on disk (or 0): its SCREEN chunk is kept.
 * Returns the size, 0 when cap is too small. */
long desk_wbpattern(const gtheme *t, int depth, int tiled, const unsigned char *prev, long prevlen,
                    unsigned char *out, long cap);

/* R1.21a: write the theme's colours into the RGB table of every PNTR chunk of buf in place (triple 1 foreground,
 * 2 background, 3 accent); bitmaps stay. Returns the chunk count, -1 when buf is no pointer.prefs. */
long desk_pointer(const gtheme *t, unsigned char *buf, long len);

/* R1.22a: the WBFONT chunk of font.prefs gets the label pen, the backdrop pen, JAM1 and special mode 1 (OD10). Returns
 * the number of chunks changed (0 or 1), -1 when buf is no font.prefs. */
long desk_font_pens(unsigned char *buf, long len);

/* R1.4: the theme's font= names the WBFONT of font.prefs, only when exists(ctx, name) says the font is there. Returns
 * 1 when the file changed, 0 when the key is empty or unknown (file untouched), -1 when buf is no font.prefs. */
typedef int (*desk_font_exists_fn)(void *ctx, const char *name);
long desk_font_name(const gtheme *t, unsigned char *buf, long len, desk_font_exists_fn exists, void *ctx);

/* the font.prefs of a machine that has none (518 bytes); 0 when cap is too small */
long desk_font_default(unsigned char *out, long cap);

/* R1.6: 0 when the theme passes the contrast guard, else 1 and a refusal text in why (cap bytes) */
int desk_check(const gtheme *t, char *why, int cap);

/* R1.6: the apply decision for the amconf desktop mask (bit t = target t): 0 when every switch is off (nothing is read
 * or written), -1 when the theme is refused (why gets the text), else the mask to apply */
int desk_plan(unsigned mask, const gtheme *t, char *why, int cap);

/* D-TGT: the targets; each has its own amconf switch (OD2) and the files it writes */
enum { DT_PALETTE, DT_WBPATTERN, DT_POINTER, DT_FONT, DT_COUNT };
typedef struct {
    const char *name;    /* palette, wbpattern, pointer, font (the screen title stays stock, OD11: no target) */
    const char *key;     /* the amconf key of its switch */
    const char *files[2]; /* the files it writes, 0 = none */
} desk_target;
const desk_target *desk_target_get(int target);
int desk_target_by_name(const char *name); /* -1 when unknown */

/* R1.16a: the files to snapshot for the enabled targets (bit t of mask = target t) in write order; returns the count */
int desk_snapshot(unsigned mask, const char **list, int cap);
/* the files to restore for mask, in restore order (reverse of the write order); returns the count */
int desk_restore(unsigned mask, const char **list, int cap);
/* the pending marker: written before the first write, removed after the last. Its text names the targets, so a
 * crashed run restores what it had touched even when the switches changed since. Returns the length. */
long desk_marker_make(unsigned mask, char *out, long cap);

/* Pointer editor version gate (R1.17): the 3.1 editor (40.x) reset-loops on the 3.2 pointer.prefs this module writes
 * (PRHD + 41 PNTR); the 3.2 editor (47.x) applies it. Reads the major version from the "$VER: <name> N.M" string inside
 * the editor's file image; -1 when no such string. */
int desk_ver_major(const unsigned char *bin, long len);
/* 1 when the editor with this file image may be fed the pointer prefs (major >= 47); 0 below 47 or unknown */
int desk_pointer_editor_ok(const unsigned char *bin, long len);
/* the mask a marker names; 0 when buf is no marker */
unsigned desk_marker_parse(const char *buf, long len);

#endif
