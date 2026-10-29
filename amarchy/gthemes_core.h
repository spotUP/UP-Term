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
    char backdrop[GT_NAME]; /* backdrop= : the drawer name of the theme's backdrops, "" = the theme name */
    char font[GT_NAME];     /* font= : a font file name such as helvetica.font, "" = keep the user's font */
    char wbtitle[GT_TITLE]; /* wbtitle= : a Workbench title format; carried, the title stays stock (OD11) */
} gtheme;

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


/* Why a theme save failed, as the text the menu shows. step: GT_SAVE_NAME (the name check), or a prefs_save_at result
 * (PREFS_SAVE_NODIR, PREFS_INSTALL_WRITE / BACKUP / PLACE / STRANDED); failed: the PREFS_T_ target; err: the DOS error
 * (0: unknown). Writes e.g. "the theme could not be saved: write ENV:Amarchy/prefs, error 202". */
#define GT_SAVE_NAME (-200)
void gt_save_text(int step, int failed, long err, char *out, int cap);

#endif
