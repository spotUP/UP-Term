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

/* The roles that are palette colours of the Directory Opus lister (colour n is role n): the first five. MT_OK and
 * MT_FAIL come after them, so no existing pen number moves, and they are never sent to Directory Opus. */
enum {
    MT_PALETTE_ROLES = MT_SELBOX + 1
};

/* The role the border is drawn in: the selected row's box role, so outline and selection are one colour (gui.c
 * draw_border reads this; tests/test_keymap.c asserts it equals MT_SELBOX). */
#define MT_BORDER_ROLE MT_SELBOX

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

/* The two Directory Opus ARexx commands that make its lister colours the menu's roles. Syntax read from upstream
 * dopus5 v5.104 source/Program/rexx_dopus.c: RXCMD_PALETTE (set: from line 587) takes "0xRRGGBB" words, 8 bits a
 * channel, one per colour from colour 0 (at most 16; the first MT_PALETTE_ROLES); RXCMD_PENS (set: from line 616) takes keyword groups
 * (files, dirs, selfiles, seldirs, ...) each followed by a foreground and a background pen number, which are the
 * palette colour numbers (rexx_dopus_get_pen, line 994). Colour n is role n (MT_BG is colour 0 ... MT_SELBOX 4; MT_OK and MT_FAIL are not sent), so
 * the pens read: files text on ground, directories accent on ground, the selected two on the selection box.
 * Writes two lines, "dopus set palette ..." and "dopus set pens ...", each ending in a newline, and a NUL.
 * Returns the length written (without the NUL), or 0 when rgb is NULL, a colour is above 0xFFFFFF, or cap is too
 * small (out[0] is then 0 when cap is at least 1). */
unsigned menu_theme_dopus_commands(const uc_u32 rgb[MT_ROLES], char *out, unsigned cap);

#endif
