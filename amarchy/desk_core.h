/* desk_core: the pure half of the Amarchy native desktop theme (plan 2026-10-29-amarchy-p1a-native-theme.md). A gtheme
 * becomes the bytes of the Workbench preference files: palette.prefs (PALT), wbpattern.prefs (PTRN), pointer.prefs
 * (PNTR colour tables), font.prefs (FONT pens and name), plus the contrast guard, the target table and the revert
 * bookkeeping. No OS calls: buffers in, buffers out. The file layouts come from the probe note
 * thoughts/shared/research/2026-10-29-amarchy-p1a-prefs-probe.md and the goldens in tests/data/desk/. */
#ifndef DESK_CORE_H
#define DESK_CORE_H

#include "gthemes_core.h"
#include "flat_core.h"

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
 * its distance, else the background at 80 % or less, in every case strictly darker after the 12-bit nibble cut so the
 * backdrop pen never equals the ground pen (DESK-BACKDROP-DARKER). A ground that is black on the screen (desk_ground_black)
 * keeps its colour and the backdrop is dark grey, lighter than black. */
uc_u32 desk_darker(const gtheme *t);

/* 1 when the ground is black on a 12-bit screen: all channels below 16 */
int desk_ground_black(const gtheme *t);

/* R1.19: the pen index of every DrawInfo slot (OD13) into pens[DK_SLOTS] */
void desk_pens(unsigned char pens[DK_SLOTS]);

/* FLAT-P1-1 (OD-F4 a), FLAT-SOLID-FILL: the flat map. The stock map with SHINEPEN, SHADOWPEN and FILLPEN = the surface pen
 * (DK_BG) and FILLTEXTPEN = DK_FG, so bevels vanish and every window is one solid rectangle. A theme switch while flat stays
 * on or off keeps the pen indices (no Workbench reset); switching flat itself changes them, which desk_pens_reset detects.
 * Every index stays below 4, so the same map serves a 4-colour screen without aliasing. */
void desk_pens_flat(unsigned char pens[DK_SLOTS]);
/* FLAT-SOLID-FILL: pen_text (active glyph), pen_line (inactive glyph), pen_back of the flat_state block under the flat map */
void desk_flat_glyph_pens(unsigned char *text, unsigned char *line, unsigned char *back);

/* FLAT-C9: the one mapping from the prefs (flat = amarchy flat-system-look, gadgets = flat-window-gadgets, both 0/1) and the theme's
 * pen map to the flat_state block of "Amarchy.Flat": enabled = either switch, pen_text/pen_line/pen_back = desk_flat_glyph_pens,
 * pen_outline = the theme accent pen (DK_ACCENT), pen_button = DK_SELECTION, pen_ptext = DK_BG. Only these fields are written; version, size, held, still_chained and calls
 * belong to the resident program and are left alone. desk_flat_state_calls counts calls (the reachability sentinel). */
void desk_flat_state(int flat, int gadgets, flat_state *s);
extern unsigned long desk_flat_state_calls;
/* The icon applier's state: 0 originals (flat off), 1 Filled applied, 2 Outline applied. The desk re-runs amarchy-icons when it changes,
 * so a switch of the icon style while flat is on applies the other set at once. */
int desk_icons_state(int on, int outline);

/* Pen plan (lucide icons L6): which screen pens the desktop uses beyond the DrawInfo pens, per screen depth (EHB counts as 5).
 * button = the flat button fill (DK_SELECTION), backdrop = the Workbench backdrop tone (DK_MUTED; WBPattern tiles are 3 planes,
 * so both are below 8). The icon pens are the range icon_first .. icon_first + icon_count - 1 that amarchy-flat holds for icon
 * slots icon_slot ..: depth >= 6 pens 32..47 (slots 0..15), depth 4 and 5 pens 8..15 (slots 8..15), depth 2 and 3 none. The
 * range never overlaps button, backdrop or the eight role pens 0..7. */
typedef struct desk_pen_plan {
    int button, backdrop;
    int icon_first, icon_count, icon_slot;
} desk_pen_plan;
void desk_pen_plan_for(int depth, desk_pen_plan *p);

/* FLAT-P1-3: the pen map the palette write carries: desk_pens_flat when flat-system-look is on (flat), else desk_pens */
void desk_pens_for(int flat, unsigned char pens[DK_SLOTS]);

/* R1.17: 1 when a Palette USE of the pens desk_pens_for(flat) gives makes IPrefs reset the Workbench screen. DrawInfo dri_Pens are fixed
 * at screen open, so only a pen array that differs from the live screen's (cur, ncur = dri_NumPens, the first ncur slots are
 * compared) reopens it; colour values alone load live. ncur <= 0 (no DrawInfo): unknown, 1. */
int desk_pens_reset(const unsigned short *cur, int ncur, int flat);

/* DESK-31: 1 once the Workbench reset after a Palette USE finished. before = screen address before the write, now = the address
 * the lock gives now (0 while the screen is closed in the middle of the reset), live/nlive = its DrawInfo pens. Done = the screen
 * is open and (it is a new address, 3.1, or its pens equal the desk pens, 3.2 resets in place with the same address). */
/* Has IPrefs finished with every prefs file written so far (DESK-31)? A stock editor's USE closes ENV:Sys/<x>.prefs, the
 * file notification signals IPrefs at once, and IPrefs resets the Workbench screen from it (3.1 Font: two resets, 0.3-0.8 s
 * after the editor exits). It is done when it is back in the Wait() it sat in before the apply (state TS_WAIT, wait mask
 * idle_mask, idle_mask != 0) with none of those signals pending. A Delay, DOS packet or requester inside the reset waits on
 * other signals, a pending notification shows in recvd. No IPrefs task (have = 0): nothing to wait for. */
int desk_iprefs_idle(int have, int waiting, unsigned long sigwait, unsigned long recvd, unsigned long idle_mask);
#define DESK_IPREFS_POLL_TICKS 1
int desk_reset_done(const void *before, const void *now, const unsigned short *live, int nlive, int flat);

/* R1.2: the palette.prefs of the theme with ncolors (4 or 8) colours and the pen map pens (desk_pens or desk_pens_flat);
 * returns DK_PALETTE_SIZE, 0 on a bad argument */
long desk_palette(const gtheme *t, int ncolors, const unsigned char pens[DK_SLOTS], unsigned char *out, long cap);

/* FLAT-PENS-LIVE: the palette.prefs of the colours now on the screen (rgb4 = ncolors 4-bit 0x0RGB entries) with the pen map pens */
long desk_palette_live(const unsigned short *rgb4, int nlive, int depth, const unsigned char pens[DK_SLOTS], unsigned char *out, long cap);
/* FLAT-PEN5-MAGENTA: the one entry-to-pen mapping of palette.prefs. desk_palette_pen = the screen pen ColorSpec entry (0-7) sets at
 * depth (IPrefs: 0-3 -> 0-3, 4-7 -> 2^depth-8+entry; -1 none); desk_palette_entry_of = the inverse (-1: no entry reaches that pen).
 * desk_palette_live above takes the live colours of pens 0..nlive-1 (4-bit 0x0RGB) and fills each entry from the pen it sets. */
int desk_palette_pen(int entry, int depth);
int desk_palette_entry_of(int pen, int depth);
/* the role pens 0..7 no palette.prefs entry reaches at depth (4-7 on a screen deeper than 3): they are set live (SetRGB32). Returns
 * how many, the pens in out */
int desk_live_role_pens(int depth, int out[4]);
/* FLAT-LIVEBASE-SAVED: the colour of role pen at depth under the saved palette.prefs (saved/savedlen, may be 0): its entry's colour,
 * else the graphics default of pens 4-7. The restore baseline of the live pens; never read from the live screen. */
uc_u32 desk_live_baseline(const unsigned char *saved, long savedlen, int pen, int depth);
/* the theme colour of role pen 0..7 (desk_role); the backdrop pen is the theme dark (desk_darker) when flat owns the backdrop */
uc_u32 desk_pen_colour(const gtheme *t, int pen, int own_backdrop);
/* FLAT-PENS47-RESET: what a NEW Workbench screen needs on its live role pens. flat = amconf_desk_flat_pens, mask = amconf_desk_mask (the
 * desktop-theme palette also writes the theme). When either puts the theme in the palette: the pens (desk_live_role_pens at depth) in
 * pens and their colours (desk_pen_colour; the backdrop pen is the dark one when flat owns the backdrop, no DT_WBPATTERN) in rgb,
 * returns how many. Else 0: the screen keeps what IPrefs and graphics give it. desk_dos_apply sets them once on the screen open at
 * apply time; the Workbench reopen that follows makes a fresh screen with graphics-default pens 4-7, so the screen follow path
 * (amarchy iconpen_dos pens_sync, IP_SYNC_OBTAIN) sets them again on every new screen. */
int desk_live_follow(const gtheme *t, int flat, unsigned mask, int depth, int pens[4], uc_u32 rgb[4]);
/* FLAT-PEN5-MAGENTA: cur (a palette.prefs, pens already set) with every entry that reaches a role pen at depth recoloured to
 * desk_pen_colour; the entries that reach other pens (252-255 on a deep screen) keep the user's colours. 0 on a bad file. */
long desk_palette_theme(const unsigned char *cur, long curlen, const gtheme *t, int depth, int own_backdrop, unsigned char *out, long cap);
/* FLAT-PENS-LIVE: cur (a palette.prefs of curlen bytes) with only the pen arrays replaced by pens; the colours stay. Returns curlen,
 * 0 when cur is no palette.prefs or does not fit cap */
int desk_backdrop_pen(void);
long desk_palette_rgb(const unsigned char *cur, long curlen, int pen, int depth);
long desk_palette_set_rgb(const unsigned char *cur, long curlen, int pen, int depth, uc_u32 c, unsigned char *out, long cap);
long desk_palette_pens(const unsigned char *cur, long curlen, const unsigned char pens[DK_SLOTS], unsigned char *out, long cap);
/* FLAT-PENS-LIVE: the pen write flat-system-look (flat 0/1) asks for. pens gets desk_pens_for(flat). Returns 1 to write it: flat on and
 * the live pens (live/nlive, the Workbench DrawInfo) differ from the flat map; flat off and the live pens ARE the flat map (restore
 * the stock roles); else 0. */
int desk_pens_sync(int flat, const unsigned short *live, int nlive, unsigned char pens[DK_SLOTS]);

/* R1.3, R1.20: the wbpattern.prefs (ROOT, DRAWER, SCREEN chunks). depth = the screen depth (capped at 3); the root is
 * one solid darker tone, no dots. prev/prevlen = the file now on disk (or 0): its SCREEN chunk is kept.
 * Returns the size, 0 when cap is too small. */
long desk_wbpattern(const gtheme *t, int depth, const unsigned char *prev, long prevlen,
                    unsigned char *out, long cap);

/* R1.21a: write the theme's colours into the RGB table of every PNTR chunk of buf in place (triple 1 foreground,
 * 2 background, 3 accent); bitmaps stay. Returns the chunk count, -1 when buf is no pointer.prefs. */
long desk_pointer(const gtheme *t, unsigned char *buf, long len);

/* R1.22a: the WBFONT chunk of font.prefs gets the label pen, the backdrop pen, JAM1 and special mode 1 (OD10). Returns
 * the number of chunks changed (0 or 1), -1 when buf is no font.prefs. */
long desk_font_pens(unsigned char *buf, long len);

/* FLAT-P1-2: the icontrol.prefs for the window border and title size. cur/curlen = the file now on disk (or 0): every chunk and
 * every ICTL field is kept (IEXC exception chunks byte for byte); ICB_VERSIONED, ic_Version 2, ic_VersionMagic 0 and
 * ic_GUIGeometry[4] are set, a pre-V2 ICTL grows to DK_ICTL_SIZE with zero fields. No file: a new one with defaults.
 * Returns the size, 0 when cap is too small or cur is no prefs file. */
#define DK_ICTL_SIZE 38L
long desk_icontrol(const unsigned char *cur, long curlen, const unsigned char geometry[4], unsigned char *out, long cap);
/* the geometry bytes of the border and title size: thin = 0 stock (all zero), 1 thin (byte 1 = -1: title bar one pixel
 * lower than stock; measured FLAT-R1, research note section 9; nothing thins the other borders) */
void desk_geometry(int thin, unsigned char geometry[4]);

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
enum { DT_PALETTE, DT_WBPATTERN, DT_POINTER, DT_FONT, DT_ICONTROL, DT_COUNT };
typedef struct {
    const char *name;    /* palette, wbpattern, pointer, font, icontrol (the screen title stays stock, OD11: no target) */
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
/* Palette editor gate (DESK-31-CRASH): on Kickstart 40.68 (Palette 40.2, IPrefs 40.7) any write of ENV:Sys/palette.prefs
 * freezes the guest a few seconds later (stock file too); the 47.x editor applies it. 1 when the editor with this file image
 * may be run on a palette.prefs (major >= 47); 0 below 47 or unknown. The caller then leaves the file alone and sets the
 * colours live (wb_pens_apply). */
int desk_palette_editor_ok(const unsigned char *bin, long len);
/* the targets to write on a Workbench whose Palette editor is (palette_ok) or is not usable: without it the palette bit goes. When a
 * wbpattern write remains (either way), *ws_reset is 1 -- IPrefs (40.7 and 47.x) makes Intuition ask for a Workbench reset on it ("Please close all
 * windows, except drawers"), so the caller closes its windows around that write and waits for the new screen. The icontrol bit stays only with palette_ok (3.2 reads the
 * geometry, 3.1 ignores it) and sets *ws_reset too: a geometry change reopens the Workbench screen */
unsigned desk_gate_mask(unsigned mask, int palette_ok, int *ws_reset);
/* FLAT-P1-2 icontrol action. thin = the border size switch is thin (and the desktop theme on); cur = the current icontrol.prefs bytes (curlen <= 0: none);
 * have_snap = T:amarchy-revert holds a copy. Thin and not yet written: DK_IC_WRITE and *snapshot = 1 (copy the file first). Thin already written: DK_IC_NONE
 * (no second Workbench reset; the snapshot is not overwritten by our own file). Stock after a thin write with a snapshot: DK_IC_RESTORE (copy the snapshot back, ENVARC: and ENV:). Else DK_IC_NONE. */
enum { DK_IC_NONE, DK_IC_WRITE, DK_IC_RESTORE };
int desk_icontrol_decide(int thin, const unsigned char *cur, long curlen, int have_snap, int *snapshot);
/* AM-ICON-NOBORDER: the Workbench icon border is the BOOL wbp_Borderless of the WBNC chunk in ENV:Sys/Workbench.prefs (NDK 3.2
 * prefs/workbench.h:27-37; the 3.2 Prefs/Workbench editor "Border Size" cycle, "No Border"). desk_wbborder_value: 1 no border, 0 border, -1 no WBNC chunk
 * (3.1 has none). desk_wbborder: the file flat-system-look wants (flat 1 = no border, 0 = the saved value back). saved = the value kept when we changed it
 * (-1 none). Returns the size written to out, 0 when nothing changes. *save_out: the value to keep (0/1, or DESK_WB_CREATED), -2 forget the saved one, -1 as is,
 * No file at all: flat writes the editor's 84-byte stock file with the
 * border off and keeps DESK_WB_CREATED; off writes only the border word back (border 0) into the current file, every other byte kept; the file is never deleted. */
#define DESK_WB_CREATED 2
int desk_wbborder_value(const unsigned char *cur, long curlen);
long desk_wbborder(int flat, const unsigned char *cur, long curlen, int saved, int *save_out, unsigned char *out, long cap);
/* the mask a marker names; 0 when buf is no marker */
unsigned desk_marker_parse(const char *buf, long len);

/* DESK-PREVIEW-PALETTE (owner 2026-10-30): while the theme picker's highlight moves, the Workbench COLOURS (pens 0..npens-1 of the
 * Workbench screen's ViewPort, the pens desk_palette writes) show the highlighted theme; nothing else of the desktop changes until Enter.
 * ops.get/set read and write one pen (0xRRGGBB); npens is 4 or 8 (the screen's colours, capped). The first preview saves the pens
 * it overwrites; desk_preview(p, 0) (Esc, leaving the list, cleanup) puts them back. A theme the contrast guard refuses (desk_check)
 * changes nothing. Returns 1 when pens were written, 0 otherwise. */
typedef struct desk_pv {
    void *ctx;
    int npens;
    void (*get)(void *ctx, int pen, uc_u32 *rgb);
    void (*set)(void *ctx, int pen, uc_u32 rgb);
    uc_u32 saved[DK_ROLES];
    int have;
} desk_pv;
int desk_preview(desk_pv *p, const gtheme *t);

#endif
