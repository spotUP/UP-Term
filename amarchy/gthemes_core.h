/* gthemes_core: the Omarchy colour themes of the drawn Amarchy menu (plan 2026-10-26-amarchy-gui-themes.md). A theme is
 * one file NAME.theme holding an Omarchy colors.toml verbatim (`key = "#rrggbb"`, `mode = "dark"`), read by the upconf
 * reader (quotes and `#` are tolerated by upconf_hex). Six keys drive the menu: background, foreground,
 * dark_foreground, accent, green, red; the other keys of the file are kept and ignored, so a copied Omarchy file works. The
 * shipped drawer is UP-Term:Amarchy/Themes, the user's ENVARC:Amarchy/themes; a user file of the same name wins.
 * No OS calls: the file read comes in as a callback (the DOS side is gthemes_dos.c). */
#ifndef GTHEMES_CORE_H
#define GTHEMES_CORE_H

#include "../config/upconf.h"

#define GT_NAME 40
#define GT_MAX 64
#define GT_TITLE 64
#define GT_SHIPPED_DIR "UP-Term:Amarchy/Themes"
#define GT_USER_DIR "ENVARC:Amarchy/themes"
#define GT_EXT ".theme"
#define GT_DEFAULT "tokyo-night"

typedef struct gtheme {
    char name[GT_NAME];
    uc_u32 bg, fg, dim, accent; /* 0xRRGGBB: background, foreground, dark_foreground, accent */
    uc_u32 ok, fail;            /* the job view's Done and Failed footers: green and red; no key = accent, foreground */
    int light;                  /* mode = "light" */
    /* desktop theme (plan 2026-10-29-amarchy-p1a-native-theme.md, R1.5): the keys of the file that tint the Workbench.
     * A missing key is derived (OD5): dark = bg; darker = dark_background, else bg at 80 % brightness; lighter = bg
     * moved a tenth toward fg; selection = lighter; muted = dim. */
    uc_u32 dark_bg, darker_bg, lighter_bg, selection, muted;
    uc_u32 inactive;        /* inactive= : the dragbar, gadget boxes, ring, arrows and knob of an inactive window (S32-INACTIVE-BAR); derived: gt_mix(bg, fg, 45) */
    char backdrop[GT_NAME]; /* backdrop= : the drawer name of the theme's backdrops, "" = the theme name */
    char font[GT_NAME];     /* font= : a font file name such as helvetica.font, "" = keep the user's font */
    char wbtitle[GT_TITLE]; /* wbtitle= : a Workbench title format; carried, the title stays stock (OD11) */
} gtheme;

/* RL8 FLAT-COLOUR-OVERHAUL: THE role table. Every Amarchy UI colour is one of these six roles of a gtheme, made by the one
 * derivation gt_roles_of, the same rule for every theme; the desktop pens (DK_*), the menu roles (MT_*), the icon slots (IP_*) and
 * the flat glyph roles (FLAT_ROLE_*) are views of it, never their own picks from the gtheme.
 *   GR_BACKGROUND  the desktop backdrop, the darkest tone (gt_darker)
 *   GR_SURFACE     windows, panels, the menu and the bar ground (background)
 *   GR_TEXT        text and active glyphs (foreground)
 *   GR_MUTED       secondary text: file names, dim labels, the menu's dim header (muted; dark_foreground when the file has none)
 *   GR_ACCENT      the one highlight: selected row box, border, outline, header (accent)
 *   GR_SELECTION   raised fills: buttons, scroller knob (selection, pushed off the surface by gt_selection)
 *   GR_INACTIVE    S32-INACTIVE-BAR: the bar and frame tone of an inactive window (inactive; background moved GT_INACTIVE_PCT toward the foreground when the file has none)
 * ok and fail are not UI roles: the two status tones of the job view's Done / Failed footers (green, red). */
enum { GR_BACKGROUND, GR_SURFACE, GR_TEXT, GR_MUTED, GR_ACCENT, GR_SELECTION, GR_INACTIVE, GR_ROLES };
#define GT_INACTIVE_PCT 45
/* a moved pct percent of the way toward b, per channel (0xRRGGBB) */
uc_u32 gt_mix(uc_u32 a, uc_u32 b, int pct);
typedef struct gt_roles {
    uc_u32 c[GR_ROLES]; /* 0xRRGGBB per GR_* */
    uc_u32 ok, fail;    /* status tones */
} gt_roles;
void gt_roles_of(const gtheme *t, gt_roles *r);

/* the colour maths of the derivation (moved from desk_core, R1.18a): brightness 0..255, a contrast ratio times 100 (100 = equal) */
int gt_luma(uc_u32 rgb);
int gt_contrast(uc_u32 a, uc_u32 b);
#define GT_FLOOR_TEXT 220 /* text on a ground: measured, the weakest shipped pair is rose-pine at 233 */
#define GT_STEP_DARK 108  /* the darker tone against the background */
#define GT_FLOOR_SEL 150  /* the selection (button fill, scroller knob) against the surface: shipped light themes sit at 110-130 */
#define GT_BLACK_LIFT 0x222222UL /* the backdrop over a black ground: dark grey, lighter than black */
/* 1 when the ground is black on a 12-bit screen: all channels below 16 */
int gt_ground_black(const gtheme *t);
/* GR_BACKGROUND (OD5, DESK-BACKDROP-DARKER): the theme's darker_background when it is darker than the background and keeps its
 * distance, else the background at 80 % or less, in every case strictly darker after the 12-bit nibble cut so the backdrop pen never
 * equals the ground pen. A ground that is black on the screen keeps its colour and the backdrop is GT_BLACK_LIFT. */
uc_u32 gt_darker(const gtheme *t);
/* GR_SELECTION: t->selection pushed darker (light ground) or lighter (dark ground) in 10 % steps until its contrast against the
 * surface is >= GT_FLOOR_SEL, on the 24-bit and on the 12-bit screen */
uc_u32 gt_selection(const gtheme *t);

/* The compiled-in theme (tokyo-night, the default): a bare install without the data drawer still draws. */
void gt_builtin(gtheme *out);

/* Parse the text of a colors.toml (len bytes) into out; a missing or unreadable key keeps the built-in value. */
void gt_parse(const char *buf, long len, gtheme *out);

/* The terminal colours of a theme (Omarchy recolours every terminal with the theme): the text of one UP-Term profile section
 * (fg, bg, cursor-color, the 16 palette entries) made from the colors.toml text, ready for the console's theme_apply. Returns
 * the length, 0 when it does not fit cap. */
int gt_term_text(const char *buf, long len, const char *name, char *out, int cap);

/* The file the console handler's tabs watch: a written theme recolours every open tab (vtcon_handler.c watch_worker). */
#define GT_TERM_FILE "ENV:up-term/theme-active"

/* Every channel of an 0xRRGGBB scaled to pct percent (0..100); the one brightness scaler of the desktop theme. */
uc_u32 gt_scale(uc_u32 rgb, int pct);

/* name -> the file path in a drawer ("dir/name.theme"; a dir ending in ':' or '/' gets no extra slash). 0 when too long. */
int gt_path(const char *dir, const char *name, char *out, int cap);

/* "tokyo-night.theme" -> "tokyo-night" (extension compared without case); 0 when the file is not a theme file. */
int gt_name_of_file(const char *file, char *out, int cap);

typedef struct gt_list {
    int n;
    char name[GT_MAX][GT_NAME];
    char user[GT_MAX]; /* 1: the user's drawer holds this name (it wins over the shipped file) */
} gt_list;

/* Add one file name found in a drawer. Sorted by name without case, one row per name, the user's file marks the row. */
void gt_list_add(gt_list *l, const char *file, int from_user);

typedef long (*gt_read_fn)(void *ctx, const char *path, char *buf, long cap); /* length, -1 missing */

/* Load a theme by name: the user's file, else the shipped one, else (unknown or unreadable name) the built-in default. */
void gt_load(gt_read_fn rd, void *ctx, const char *name, gtheme *out);

/* gt_term_text of the theme NAME as gt_load finds it (the user's file, else the shipped one); 0 when there is no such theme. */
int gt_load_term(gt_read_fn rd, void *ctx, const char *name, char *out, int cap);

/* One drawer's file names, given one by one to emit (the DOS side walks the drawer; a host test fakes it). */
typedef void (*gt_emit_fn)(void *ectx, const char *file);
typedef void (*gt_dir_fn)(void *ctx, const char *dir, gt_emit_fn emit, void *ectx);

/* The theme list of both drawers: shipped first, then the user's (whose file wins and marks the row). */
void gt_scan(gt_dir_fn dir, void *ctx, gt_list *l);

/* The menu row label: the name, with " (current)" appended when it is the saved theme (compared without case). */
void gt_label(const char *name, const char *current, char *out, int cap);

/* 1 when name can be saved as the current theme: non-empty, fits GT_NAME, no path characters ('/' or ':'). */
int gt_name_ok(const char *name);

/* F3-THEME-BOOT: the startup write of GT_TERM_FILE. Calls send(name) once with the configured theme (AM_GTHEME) or GT_DEFAULT when
 * it is empty or not a usable name; returns how many times send ran (1). */
int gt_boot_term(const char *configured, void (*send)(const char *name));

/* A theme row's path in the menu index ("gtheme:<name>"); gt_path_name gives the name back, 0 when the path is not one. */
#define GT_ROW_PREFIX "gtheme:"
int gt_path_name(const char *path, char *out, int cap);

/* The theme the open menu draws while the cursor walks Style > Theme (owner, 2026-10-28: preview on cursor move):
 * the theme of the selected row (row_path "gtheme:<name>"), else the saved one (current), so leaving the list or
 * moving onto another row restores it. Returns 1 and writes the name to out when it differs from drawn (compared
 * without case), 0 when the menu already draws it. row_path may be 0. */
int gt_preview_pick(const char *row_path, const char *current, const char *drawn, char *out, int cap);

/* THEME-PREVIEW-SHELL (owner, 2026-10-29): the picker's preview also recolours the open consoles and agent tabs. gt_preview_pick, and
 * when it picks a theme send(name) runs (the bridge that Apply uses: gt_dos_term_send, GT_TERM_FILE) on every highlight move, no
 * throttle; leaving the list or cancelling picks the saved theme, so send restores it. Returns what gt_preview_pick returned. */
int gt_preview_term(const char *row_path, const char *current, const char *drawn, char *out, int cap,
                    void (*send)(const char *name));

/* THEME-PREVIEW-LIVE-A1200: a picker that keeps no pens of its own (the text menu amarchy-menu, drawn in the console it
 * recolours) holds only drawn, the theme the consoles show now. gt_preview_term with drawn as both the compare and the
 * output: the highlight's theme, or current on any other row / at close (Esc restores the theme active before the picker
 * opened). Start drawn as current when the menu opens, so opening it writes nothing. Returns what gt_preview_term did. */
int gt_preview_step(const char *row_path, const char *current, char *drawn, int cap, void (*send)(const char *name));


/* Why a theme save failed, as the text the menu shows. step: GT_SAVE_NAME (the name check), or a prefs_save_at result
 * (PREFS_SAVE_NODIR, PREFS_INSTALL_WRITE / BACKUP / PLACE / STRANDED); failed: the PREFS_T_ target; err: the DOS error
 * (0: unknown). Writes e.g. "the theme could not be saved: write ENV:Amarchy/prefs, error 202". */
#define GT_SAVE_NAME (-200)
void gt_save_text(int step, int failed, long err, char *out, int cap);

#endif
