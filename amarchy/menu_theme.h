/* menu_theme -- which palette colour is which role of the drawn Amarchy menu. One table (menu_theme.c); the
 * palette is the user's upconf one (upconf_palette_parse: 0x01RRGGBB per remapped index, 0 = not remapped). A
 * colour the user did not remap is the xterm one the renderer draws (engine vt_palette_rgb; the table keeps
 * those values so this module needs no engine, tests/test_keymap.c holds the two equal). The terminal's own
 * upconf keys fg and bg take over MT_TEXT and MT_BG when set, so the menu matches the terminal it opens from.
 * No OS calls. */
#ifndef MENU_THEME_H
#define MENU_THEME_H

#include "../config/upconf.h"

enum {
    MT_BG,     /* the menu's background */
    MT_TEXT,   /* row text */
    MT_DIM,    /* the header's dim text */
    MT_ACCENT, /* the border and the selected row's text */
    MT_SELBOX, /* the selected row's box */
    MT_ROLES
};

/* The palette index that gives a role its colour (0..15) */
int menu_theme_index(int role);

/* rgb[role] = 0xRRGGBB for every role: the remapped palette colour, else the role's default */
void menu_theme_roles(const uc_u32 pal16[16], uc_u32 rgb[MT_ROLES]);

/* menu_theme_roles with the terminal's fg and bg: each is 0x01RRGGBB when the upconf key is set, 0 when not.
 * A set bg gives MT_BG, a set fg gives MT_TEXT; every other role, and an unset one, comes from the palette. */
void menu_theme_roles_fb(const uc_u32 pal16[16], uc_u32 fg, uc_u32 bg, uc_u32 rgb[MT_ROLES]);

/* The minimal menu's two colours: rgb[] from menu_theme_roles_fb is rewritten so only the background and the
 * foreground (MT_TEXT) remain: MT_BG and MT_ACCENT = background, MT_TEXT, MT_DIM and MT_SELBOX = foreground
 * (the selected row is the swap: foreground box, background text). Returns 0 when the two colours are equal: the
 * selection would be invisible, and the caller falls back to the screen's own pens 1 and 0. */
int menu_theme_minimal(uc_u32 rgb[MT_ROLES]);

#endif
