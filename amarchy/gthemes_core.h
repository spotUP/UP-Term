/* gthemes_core: the Omarchy colour themes of the drawn Amarchy menu (plan 2026-10-26-amarchy-gui-themes.md). A theme is
 * one file NAME.theme holding an Omarchy colors.toml verbatim (`key = "#rrggbb"`, `mode = "dark"`), read by the upconf
 * reader (quotes and `#` are tolerated by upconf_hex). Four keys drive the menu: background, foreground,
 * dark_foreground, accent; the other keys of the file are kept and ignored, so a copied Omarchy file works. The
 * shipped drawer is UP-Term:Amarchy/Themes, the user's ENVARC:Amarchy/themes; a user file of the same name wins.
 * No OS calls: the file read comes in as a callback (the DOS side is gthemes_dos.c). */
#ifndef GTHEMES_CORE_H
#define GTHEMES_CORE_H

#include "../config/upconf.h"

#define GT_NAME 40
#define GT_MAX 64
#define GT_SHIPPED_DIR "UP-Term:Amarchy/Themes"
#define GT_USER_DIR "ENVARC:Amarchy/themes"
#define GT_EXT ".theme"
#define GT_DEFAULT "tokyo-night"

typedef struct gtheme {
    char name[GT_NAME];
    uc_u32 bg, fg, dim, accent; /* 0xRRGGBB: background, foreground, dark_foreground, accent */
    int light;                  /* mode = "light" */
} gtheme;

/* The compiled-in theme (tokyo-night, the default): a bare install without the data drawer still draws. */
void gt_builtin(gtheme *out);

/* Parse the text of a colors.toml (len bytes) into out; a missing or unreadable key keeps the built-in value. */
void gt_parse(const char *buf, long len, gtheme *out);

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

/* The menu row label: the name, with " (current)" appended when it is the saved theme (compared without case). */
void gt_label(const char *name, const char *current, char *out, int cap);

#endif
