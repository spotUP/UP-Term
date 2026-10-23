/* menu_theme -- which palette colour is which role of the drawn Amarchy menu. One table (menu_theme.c); the
 * palette is the user's upconf one (upconf_palette_parse: 0x01RRGGBB per remapped index, 0 = not remapped). A
 * colour the user did not remap is the xterm one the renderer draws (engine vt_palette_rgb; the table keeps
 * those values so this module needs no engine, tests/test_keymap.c holds the two equal). No OS calls. */
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

#endif
