/* menu_theme -- which palette colour is which role of the drawn Amarchy menu. One table (menu_theme.c); the
 * palette is the user's upconf one (upconf_palette_parse: 0x01RRGGBB per remapped index, 0 = not remapped). A
 * colour the user did not remap is the xterm one the renderer draws (engine vt_palette_rgb; the table keeps
 * those values so this module needs no engine, tests/test_keymap.c holds the two equal). The terminal's own
 * upconf keys fg and bg take over MT_TEXT and MT_BG when set, so the menu matches the terminal it opens from.
 * No OS calls. */
#ifndef MENU_THEME_H
#define MENU_THEME_H

#include "../config/upconf.h"
#include "gthemes_core.h"

enum {
    MT_BG,     /* the menu's background */
    MT_TEXT,   /* row text */
    MT_DIM,    /* the header's dim text */
    MT_ACCENT, /* the selected row's text */
    MT_SELBOX, /* the selected row's box and the border */
    MT_OK,     /* the job view's Done footer (green-ish) */
    MT_FAIL,   /* the job view's Failed footer (red-ish) */
    MT_ROLES
};

/* The role the border is drawn in: the selected row's box role, so outline and selection are one colour (gui.c
 * draw_border reads this; tests/test_keymap.c asserts it equals MT_SELBOX). */
#define MT_BORDER_ROLE MT_SELBOX

/* The role the drawn menu's header text is drawn in: the theme's accent, so the header visibly follows a theme
 * switch (owner 2026-10-28; the dim role was a near-grey in every theme). gui.c draw_header reads this;
 * tests/test_gthemes.c asserts it carries each shipped theme's accent. */
#define MT_HEAD_ROLE MT_SELBOX

/* Where a role's pen goes when the screen has no pen left for it (a few-colour screen): MT_OK and MT_FAIL draw in
 * the row text's pen (MT_TEXT; MT_ACCENT in the drawn menu is the background colour, invisible on the footer's
 * ground). The five core roles keep their own fallback (background pen 0, else pen 1); -1 for them. */
int menu_theme_fallback_role(int role);

/* The palette index that gives a role its colour (0..15) */
int menu_theme_index(int role);

/* rgb[role] = 0xRRGGBB for every role: the remapped palette colour, else the role's default */
void menu_theme_roles(const uc_u32 pal16[16], uc_u32 rgb[MT_ROLES]);

/* menu_theme_roles with the terminal's fg and bg: each is 0x01RRGGBB when the upconf key is set, 0 when not.
 * A set bg gives MT_BG, a set fg gives MT_TEXT; every other role, and an unset one, comes from the palette. */
void menu_theme_roles_fb(const uc_u32 pal16[16], uc_u32 fg, uc_u32 bg, uc_u32 rgb[MT_ROLES]);

/* The drawn menu's roles from an Omarchy GUI theme (gthemes_core.h): BG = background, TEXT = foreground, DIM =
 * dark_foreground, ACCENT (the selected row's text) = background, SELBOX (box and border) = accent, OK = green, FAIL = red
 * (the theme file's own green and red keys; a missing key gives accent and foreground, gt_parse). */
void menu_theme_roles_gui(const gtheme *t, uc_u32 rgb[MT_ROLES]);

/* The minimal menu's two colours: rgb[] from menu_theme_roles_fb is rewritten so only the background and the
 * foreground (MT_TEXT) remain: MT_BG and MT_ACCENT = background, MT_TEXT, MT_DIM, MT_SELBOX, MT_OK and MT_FAIL = foreground
 * (the selected row is the swap: foreground box, background text). Returns 0 when the two colours are equal: the
 * selection would be invisible, and the caller falls back to the screen's own pens 1 and 0. */
int menu_theme_minimal(uc_u32 rgb[MT_ROLES]);

/* The colour each role draws in (out): on a true-colour screen (direct, cybergraphics) the theme's own rgb[], since
 * every colour is drawn as itself; otherwise, or when `themed` is 0 (minimal mode's two equal colours), the colour of
 * the pen the role got (pen_rgb[]). A screen whose pens are all taken (Directory Opus's 24-bit screen) gives only
 * the nearest of its few colours, and the menu drew in greys (rig 7, 2026-10-28). */
void menu_theme_ink(uc_u32 out[MT_ROLES], const uc_u32 rgb[MT_ROLES], const uc_u32 pen_rgb[MT_ROLES], int direct,
                    int themed);

/* The number of lister palette colours dopus_theme_commands sends: colour 0 = theme bg, 1 = fg, 2 = dim, 3 = accent. */
#define DOPUS_THEME_COLOURS 4

/* The Directory Opus ARexx lines that make its lister colours the Amarchy theme's. Syntax read from upstream
 * dopus5 v5.104 source/Program/rexx_dopus.c: RXCMD_PALETTE (set: from line 587) takes "0xRRGGBB" words, 8 bits a
 * channel, one per colour from colour 0 (at most 16); RXCMD_PENS (set: from line 616) takes keyword groups
 * (files, dirs, selfiles, seldirs, ...) each followed by a foreground and a background pen number, which are the
 * palette colour numbers (rexx_dopus_get_pen, line 994). The palette is the theme itself, not the menu roles (under
 * menu_theme_roles_gui the accent ROLE is the background, which drew directory names invisibly; owner 2026-10-29).
 * Pens: files fg on bg (1 0), directories accent on bg (3 0), selected files and directories bg on accent (0 3),
 * one `set pens` line per key, then "dopus refresh lister".
 * With palette != 0 the first line is "dopus set palette ..." (only meaningful when Opus has its own screen: on the
 * Workbench screen the palette is Workbench's, see wb_pens_plan). Every line ends in a newline; then a NUL.
 * Returns the length written (without the NUL), or 0 when th is NULL, a colour is above 0xFFFFFF, or cap is too
 * small (out[0] is then 0 when cap is at least 1). */
unsigned dopus_theme_commands(const gtheme *th, char *out, unsigned cap, int palette);

/* Directory Opus's ARexx port */
#define DOPUS_PORT "DOPUS.1"

/* Asks DOPUS_PORT "dopus screen" first (send(port, line, result, cap, ud); result NULL = no reply wanted), then sends
 * dopus_theme_commands' lines one at a time (line has no newline), the palette line only when the reply does not
 * start with "Workbench". send returns -1 when the port is absent (Directory Opus not running: normal, not an
 * error), and sending stops at that line. Returns the number of lines handed to send (including the screen query and
 * a refused one); 0 when th or send is NULL. */
int dopus_theme_send(const gtheme *th, long (*send)(const char *port, const char *line, char *result, int cap, void *ud),
                     void *ud);

/* Workbench pens 0-3 take the theme's bg, fg, dim, accent (Directory Opus on the Workbench screen shares them). */
#define WB_PENS 4
/* The refusal threshold: WCAG 2.x contrast ratio of pen 0 against pen 1, x100. 450 = 4.5:1, the AA level for body text. */
#define WB_CONTRAST_MIN_X100 450
/* WCAG contrast ratio of two 0xRRGGBB colours, x100 (21:1 = 2100; 1:1 = 100). Integer maths, no libm. */
unsigned wb_contrast_x100(uc_u32 a, uc_u32 b);
/* Fills rgb[0..3] with the theme's pens 0-3. Returns 1 when the pens may be set, 0 when th is NULL, a colour is above
 * 0xFFFFFF, or pen 0 / pen 1 contrast is below WB_CONTRAST_MIN_X100 (a Workbench nobody can read: rgb is still filled). */
int wb_pens_plan(const gtheme *th, uc_u32 rgb[WB_PENS]);

#endif
