/* desk_core: the pure half of the Amarchy native desktop theme (plan 2026-10-29-amarchy-p1a-native-theme.md). A gtheme
 * becomes the bytes of the Workbench preference files: palette.prefs (PALT), wbpattern.prefs (PTRN), pointer.prefs
 * (PNTR colour tables), font.prefs (FONT pens and name), plus the contrast guard, the target table and the revert
 * bookkeeping. No OS calls: buffers in, buffers out. The file layouts come from the probe note
 * thoughts/shared/research/2026-10-29-amarchy-p1a-prefs-probe.md and the goldens in tests/data/desk/. */
#ifndef DESK_CORE_H
#define DESK_CORE_H

#include "gthemes_core.h"
#include "flat_core.h"

/* the eight palette roles of the flat mapping (OD13): the pen index of a role is its enum value.
 * FILENAME-PEN: Workbench draws the list-view file names with the fixed DrawInfo pen 1, so pen 1 holds the muted text colour
 * (DK_MUTED) and follows every theme; Amarchy's own dark tone (DK_DARKER: BLOCKPEN, BARTRIMPEN, the label backdrop) lives on pen 5. */
enum { DK_BG, DK_MUTED, DK_FG, DK_ACCENT, DK_SELECTION, DK_DARKER, DK_GREEN, DK_RED, DK_ROLES };

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
/* FLAT-COLOUR-OVERHAUL: the selection role colour: t->selection pushed darker (light ground) or lighter (dark ground) until its contrast
 * against the surface is >= 1.5 */
uc_u32 desk_selection(const gtheme *t);

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
 * button = the flat button fill (DK_SELECTION), backdrop = the Workbench backdrop tone (FLAT-COLOUR-OVERHAUL: its own pen DK_RED, nothing draws with it; a WBPattern tile is 3 planes, so its pen is below 8 but it is the SCREEN pen 2^depth-8+pen
 * when depth > 3 (desk_backdrop_screen_pen: pen 255 on 8 planes), tile pen == screen pen only at depth <= 3; on a screen below 3 planes no pen is free and it is DK_DARKER, the same tone). DK_MUTED keeps the theme muted text colour. The icon pens are the range icon_first .. icon_first + icon_count - 1 that amarchy-flat holds for icon
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
 * compared) reopens it; colour values alone load live. ncur <= 0 (no DrawInfo): unknown, 1.
 * depth = bitplanes of the live screen: the live pens are the map folded to 1 << depth colours (desk_pen_fold, FILENAME-PEN), so the
 * map is folded the same way before comparing; depth <= 0: unknown, no folding. */
int desk_pens_reset(const unsigned short *cur, int ncur, int flat, int depth);

/* DESK-31: 1 once the Workbench reset after a Palette USE finished. before = screen address before the write, now = the address
 * the lock gives now (0 while the screen is closed in the middle of the reset), live/nlive = its DrawInfo pens. Done = the screen
 * is open and (it is a new address, 3.1, or its pens equal the desk pens, 3.2 resets in place with the same address). */
/* Has IPrefs finished with every prefs file written so far (DESK-31)? A stock editor's USE closes ENV:Sys/<x>.prefs, the
 * file notification signals IPrefs at once, and IPrefs resets the Workbench screen from it (3.1 Font: two resets, 0.3-0.8 s
 * after the editor exits). It is done when it is back in the Wait() it sat in before the apply (state TS_WAIT, wait mask
 * idle_mask, idle_mask != 0) with none of those signals pending. A Delay, DOS packet or requester inside the reset waits on
 * other signals, a pending notification shows in recvd. No IPrefs task (have = 0): nothing to wait for. */
/* RL15 AMARCHY-WBPREFS-INUSE: does a stock editor run (USE) need the IPrefs idle wait before anything else writes? Workbench and
 * WBPattern: IPrefs opens ENV:Sys/Workbench.prefs about 6.7 s after their USE, a second Workbench write in between fails "in use" */
int desk_editor_waits_iprefs(const char *editor);
int desk_iprefs_idle(int have, int waiting, unsigned long sigwait, unsigned long recvd, unsigned long idle_mask);
#define DESK_IPREFS_POLL_TICKS 1
int desk_reset_done(const void *before, const void *now, const unsigned short *live, int nlive, int flat, int depth);

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
/* the theme colour of role pen 0..7 (desk_role); the backdrop pen is the theme dark (desk_darker) whenever flat is on */
uc_u32 desk_pen_colour(const gtheme *t, int pen, int flat_backdrop);
/* FLAT-PENS47-RESET: what a NEW Workbench screen needs on its live role pens. flat = amconf_desk_flat_pens, mask = amconf_desk_mask (the
 * desktop-theme palette also writes the theme). When either puts the theme in the palette: the pens (desk_live_role_pens at depth) in
 * pens and their colours (desk_pen_colour; the backdrop pen is the dark one whenever flat is on) in rgb,
 * returns how many. Else 0: the screen keeps what IPrefs and graphics give it. desk_dos_apply sets them once on the screen open at
 * apply time; the Workbench reopen that follows makes a fresh screen with graphics-default pens 4-7, so the screen follow path
 * (amarchy iconpen_dos pens_sync, IP_SYNC_OBTAIN) sets them again on every new screen. */
int desk_live_follow(const gtheme *t, int flat, unsigned mask, int depth, int pens[4], uc_u32 rgb[4]);
/* FLAT-PEN5-MAGENTA: cur (a palette.prefs, pens already set) with every entry that reaches a role pen at depth recoloured to
 * desk_pen_colour; the entries that reach other pens (252-255 on a deep screen) keep the user's colours. 0 on a bad file. */
long desk_palette_theme(const unsigned char *cur, long curlen, const gtheme *t, int depth, int flat_backdrop, unsigned char *out, long cap);
/* FLAT-PENS-LIVE: cur (a palette.prefs of curlen bytes) with only the pen arrays replaced by pens; the colours stay. Returns curlen,
 * 0 when cur is no palette.prefs or does not fit cap */
int desk_backdrop_pen(void);
/* the backdrop pen of a screen of depth planes (the plan's) */
int desk_backdrop_pen_at(int depth);
/* FLAT-VIS-1: the screen pen the backdrop tile pen is drawn in (tile pens 4-7 land on 2^depth-8+pen: 255 on 8 planes, = the tile pen at depth <= 3) */
int desk_backdrop_screen_pen(int depth);
long desk_palette_rgb(const unsigned char *cur, long curlen, int pen, int depth);
long desk_palette_set_rgb(const unsigned char *cur, long curlen, int pen, int depth, uc_u32 c, unsigned char *out, long cap);
/* FILENAME-PEN: a pen at or above the screen's colour count folds to DK_BG (the one fold rule of the pen maps and the backdrop fallback) */
int desk_pen_fold(int pen, int ncolors);
long desk_palette_pens(const unsigned char *cur, long curlen, const unsigned char pens[DK_SLOTS], unsigned char *out, long cap);
/* FLAT-PENS-STALE-FILE: 1 when the palette file bytes cur differ from a write of pens (either pen table, any slot), or are no palette file */
int desk_palette_pens_stale(const unsigned char *cur, long curlen, const unsigned char pens[DK_SLOTS]);
/* FLAT-PENS-STALE-FILE part 2 (owner 2026-11-02, check at start too): 1 when Amarchy's start must rewrite palette.prefs: flat-system-look on
 * and the file bytes cur (curlen > 0, else no file was read) do not hold pens. Flat off, no file, or a current file: 0, nothing is written. */
int desk_flat_start_refresh(int flat, const unsigned char *cur, long curlen, const unsigned char pens[DK_SLOTS]);
/* FLAT-PENS-LIVE: the pen write flat-system-look (flat 0/1) asks for. pens gets desk_pens_for(flat). Returns 1 to write it: flat on and
 * the live pens (live/nlive, the Workbench DrawInfo) differ from the flat map; flat off and the live pens ARE the flat map (restore
 * the stock roles); else 0. depth = bitplanes of the live screen, see desk_pens_reset. */
int desk_pens_sync(int flat, const unsigned short *live, int nlive, unsigned char pens[DK_SLOTS], int depth);

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
/* icon label text mode = fp_SpecialDrawMode of the WBFONT chunk (prefs/font.h; values from the 2026-10-29 prefs probe, OD10) */
enum { DK_ICONTEXT_PLAIN = 0, DK_ICONTEXT_OUTLINE = 1, DK_ICONTEXT_SHADOW = 2 };
long desk_font_pens(unsigned char *buf, long len, int text_mode);

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
/* FP-2 (R1.10, OD14): the (ENV: file, ENVARC: file) pairs of the targets in mask (icontrol excluded: it writes both copies itself), plus the
 * Workbench.prefs pair when wbprefs. Kept = copy ENV to ENVARC; the originals are saved from and restored to the same pairs. Returns the count. */
#define DESK_WBPREFS_ENV "ENV:Sys/Workbench.prefs"
#define DESK_WBPREFS_ARC "ENVARC:Sys/Workbench.prefs"
int desk_persist_pairs(unsigned mask, int wbprefs, const char **env, const char **arc, int cap);
/* OD14 off: the targets whose originals are saved (bit t of saved) and that the current desktop mask no longer applies; icontrol is never in it */
unsigned desk_off_mask(unsigned saved, unsigned desk_mask);
/* the copy name of a prefs file inside dir: dir "/" + file with ':' and '/' turned into '-'. 0 when out (cap bytes) is too small */
/* FP-2: after "off" ran the stock editors and IPrefs/flat code had their say, the ENV: file must equal the original byte for byte:
 * nothing to do, copy the original over it, or delete it (the original did not exist) */
enum { DESK_FIX_NONE, DESK_FIX_COPY, DESK_FIX_DELETE };
int desk_env_fix(int want_exists, int got_exists, int equal);
/* DESK-STALE-ORIG: the fix runs until the ENV: file is right, not once. Opening ENV:Sys/pointer.prefs while IPrefs still reads it after the
 * editor's USE fails ("object is in use"), and that failed copy left the editor's rewrite in ENV:. Each round re-reads the state (want: the
 * copy exists; got: the ENV: file exists; equal), acts, and pauses before every retry. Returns the actions taken (0: already right), -1 when
 * the file is still wrong after tries actions. */
typedef struct desk_fix_ops {
    int (*want)(void *ud);
    int (*got)(void *ud);
    int (*equal)(void *ud);
    void (*copy)(void *ud);
    void (*del)(void *ud);
    void (*pause)(void *ud);
} desk_fix_ops;
int desk_env_fix_run(const desk_fix_ops *ops, void *ud, int tries);
int desk_copy_name(const char *dir, const char *file, char *out, int cap);
/* the files to restore for mask, in restore order (reverse of the write order); returns the count */
int desk_restore(unsigned mask, const char **list, int cap);
/* the pending marker: written before the first write, removed after the last. Its text names the targets, so a
 * crashed run restores what it had touched even when the switches changed since. Returns the length. */
long desk_marker_make(unsigned mask, char *out, long cap);
/* FP-3 (R1.9): the files whose T:amarchy-revert snapshot copies the on-disk marker ENVARC:Amarchy/pending-revert keeps, so a reboot (T: is gone) can still
 * revert an unconfirmed apply: the desk_snapshot list of mask, plus ENV:Sys/Workbench.prefs when wbprefs (the border/gauge write). Returns the count. */
int desk_pending_files(unsigned mask, int wbprefs, const char **list, int cap);

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
/* AM-ICON-NOBORDER-OFF: the 3.2 editor's stock Workbench.prefs (border shown, gauge shown), the settings an absent original stands for. Deleting
 * ENV:Sys/Workbench.prefs does not tell workbench.library (the icons stayed frameless after flat off, rig 56 s31); a restore to "absent" USEs this file
 * through SYS:Prefs/Workbench first, then deletes. Returns the size written (84), 0 when cap is too small. */
long desk_wb_stock(unsigned char *out, long cap);
long desk_wbborder(int flat, const unsigned char *cur, long curlen, int saved, int *save_out, unsigned char *out, long cap);
/* WB-VOLUME-GAUGE-OFF: the Workbench volume gauge is the BOOL wbe_DisableVolumeGauge (WBNC body offset 38, NDK 3.2 prefs/workbench.h). Same contract as
 * desk_wbborder without the created-file case: flat 1 sets it (saved value kept in *save_out), flat 0 puts the saved value back, every other byte kept.
 * desk_wbgauge_value: 1 disabled, 0 shown, -1 no such field (no file, 3.1, short chunk, which are left unchanged). Returns the size written, 0 for nothing. */
int desk_wbgauge_value(const unsigned char *cur, long curlen);
long desk_wbgauge(int flat, const unsigned char *cur, long curlen, int saved, int *save_out, unsigned char *out, long cap);
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


/* FP-1 (R1.8): the confirm state machine, pure. After an apply the user has DESK_CONFIRM_SECS to Keep it; the clock is the
 * commodity tick (whole seconds). Keep = DC_KEEP, Revert or the timeout = a revert. The mask is what the snapshot holds. */
#define DESK_CONFIRM_SECS 15
typedef struct { int pending; unsigned mask; long deadline; } desk_confirm;
enum { DC_WAIT, DC_KEEP, DC_REVERT_USER, DC_REVERT_TIMEOUT };
void desk_confirm_init(desk_confirm *c);
/* an apply happened at now: pending, the 15 s start over, the masks of applies still unconfirmed add up (the first snapshot stands) */
void desk_confirm_begin(desk_confirm *c, long now, unsigned mask);
/* the tick: DC_REVERT_TIMEOUT once when the deadline passed (pending ends), else DC_WAIT */
int desk_confirm_tick(desk_confirm *c, long now);
/* the user's answer: DC_KEEP or DC_REVERT_USER (pending ends); DC_WAIT when nothing was pending */
int desk_confirm_answer(desk_confirm *c, int keep);
/* whole seconds left (0 when not pending or past) */
long desk_confirm_left(const desk_confirm *c, long now);

/* S31-START-APPLY (rig 55, 2026-11-02): which desk files Amarchy's start must rewrite so ENV: matches the prefs it starts with. Before this only
 * a theme pick (desk_dos_apply) wrote them, so a fresh 3.2 system with desktop-theme / flat-system-look on kept the stock backdrop pattern, the
 * icon frame and the volume gauge. DESK_START_WB: flat on and desk_wbborder or desk_wbgauge would change the Workbench.prefs bytes wb (wblen < 0:
 * none read, the created-file case; wbok 0: no 3.2 Workbench editor, never). DESK_START_PATTERN: DT_WBPATTERN in mask and desk_wbpattern(th,
 * depth) differs from the ENV: bytes pat (patlen < 0: none). 0 when ENV: already matches: a second start writes nothing. */
#define DESK_START_WB 1u
#define DESK_START_PATTERN 2u
/* S31-START-PALETTE (rig 96, 2026-11-02): desktop-theme and desktop-palette on, the start wrote the flat pens and the theme backdrop but
 * never the theme colours: dk_flat_pens leaves the colours to the palette part of a theme pick (dk_apply) when DT_PALETTE is in the mask,
 * and the start runs no dk_apply, so the Workbench kept the stock grey/orange palette under the theme pattern. DESK_START_PALETTE:
 * DT_PALETTE in mask and the theme colours on base (the palette with the pens set, dk_palette_base) differ from the ENV: palette.prefs
 * bytes cur (curlen < 0: none). desk_start_palette writes the palette to use into out and returns its length, 0 when nothing to write. */
#define DESK_START_PALETTE 4u
long desk_start_palette(unsigned mask, int flat, const gtheme *th, int depth, const unsigned char *base, long baselen,
                        const unsigned char *cur, long curlen, unsigned char *out, long cap);
/* S31-START-PARTS: pointer, font and icontrol of the theme, also only applied by dk_apply before. DESK_START_POINTER / FONT: desk_start_prefs
 * (target DT_POINTER or DT_FONT) returns the length of the file to write into out, 0 when nothing to write. DESK_START_ICONTROL:
 * desk_start_icontrol returns DK_IC_WRITE / DK_IC_RESTORE / DK_IC_NONE (the thin border size, as dk_apply decides it). */
#define DESK_START_POINTER 8u
#define DESK_START_FONT 16u
#define DESK_START_ICONTROL 32u
long desk_start_prefs(int target, unsigned mask, const gtheme *th, int text_mode, desk_font_exists_fn exists, void *ctx,
                      const unsigned char *cur, long curlen, unsigned char *out, long cap);
int desk_start_icontrol(unsigned mask, const unsigned char *cur, long curlen, int have_snap, int *snapshot);
/* 1 when two wbpattern.prefs show the same pens (the editor stores an all-pen-0 chunk with depth 1) */
int desk_wbpattern_same(const unsigned char *a, long alen, const unsigned char *b, long blen);
unsigned desk_start_plan(unsigned mask, int flat, int wbok, const unsigned char *wb, long wblen, int saved, int gsaved,
                         const gtheme *th, int depth, const unsigned char *pat, long patlen);

#endif
