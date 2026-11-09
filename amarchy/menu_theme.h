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
    MT_BG,           /* the menu's background (GR_SURFACE) */
    MT_TEXT,         /* row text (GR_TEXT) */
    MT_DIM,          /* dim text (GR_MUTED) */
    MT_SELTEXT,      /* the selected row's text: the background colour on the accent box (GR_SURFACE; was MT_ACCENT) */
    MT_THEME_ACCENT, /* the selected row's box, the border and the header (GR_ACCENT; was MT_SELBOX) */
    MT_OK,           /* the job view's Done footer (status tone ok) */
    MT_FAIL,         /* the job view's Failed footer (status tone fail) */
    MT_ROLES
};

/* The role the border is drawn in: the selected row's box role, so outline and selection are one colour (gui.c
 * draw_border reads this; tests/test_keymap.c asserts it equals MT_THEME_ACCENT). */
#define MT_BORDER_ROLE MT_THEME_ACCENT

/* The role the drawn menu's header text is drawn in: the theme's accent, so the header visibly follows a theme
 * switch (owner 2026-10-28; the dim role was a near-grey in every theme). gui.c draw_header reads this;
 * tests/test_gthemes.c asserts it carries each shipped theme's accent. */
#define MT_HEAD_ROLE MT_THEME_ACCENT

/* Where a role's pen goes when the screen has no pen left for it (a few-colour screen): MT_OK and MT_FAIL draw in
 * the row text's pen (MT_TEXT; MT_SELTEXT in the drawn menu is the background colour, invisible on the footer's
 * ground). The five core roles keep their own fallback (background pen 0, else pen 1); -1 for them. */
int menu_theme_fallback_role(int role);

/* The palette index that gives a role its colour (0..15) */
int menu_theme_index(int role);

/* rgb[role] = 0xRRGGBB for every role: the remapped palette colour, else the role's default */
void menu_theme_roles(const uc_u32 pal16[16], uc_u32 rgb[MT_ROLES]);

/* menu_theme_roles with the terminal's fg and bg: each is 0x01RRGGBB when the upconf key is set, 0 when not.
 * A set bg gives MT_BG, a set fg gives MT_TEXT; every other role, and an unset one, comes from the palette. */
void menu_theme_roles_fb(const uc_u32 pal16[16], uc_u32 fg, uc_u32 bg, uc_u32 rgb[MT_ROLES]);

/* The drawn menu's roles from an Omarchy GUI theme: a view of the role table (gt_roles_of, gthemes_core.h). BG = GR_SURFACE,
 * TEXT = GR_TEXT, DIM = GR_MUTED (RL8: the theme's muted, was dark_foreground), SELTEXT = GR_SURFACE, THEME_ACCENT = GR_ACCENT,
 * OK and FAIL = the status tones (green, red; a missing key gives accent and foreground, gt_parse). */
void menu_theme_roles_gui(const gtheme *t, uc_u32 rgb[MT_ROLES]);

/* The minimal menu's two colours: rgb[] from menu_theme_roles_fb is rewritten so only the background and the
 * foreground (MT_TEXT) remain: MT_BG and MT_SELTEXT = background, MT_TEXT, MT_DIM, MT_THEME_ACCENT, MT_OK and MT_FAIL = foreground
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
 * Pens (FLAT-D2, the one mapping; DOpus palette 0 bg, 1 fg, 2 dim, 3 accent): icons, files, devices, assigns fg on bg (1 0),
 * directories, the gauge and the source lister's info bar accent on bg (3 0), selected files and directories bg on accent (0 3),
 * the destination lister's info bar dim on bg (2 0). DOPUS-LISTER-LOOK (2026-11-09): the info bars are text on the surface,
 * never a filled bar, as in every other flat window, where the only filled accent bar is the active title band (a source bar
 * of bg on accent stacked a second accent band under it); the 10 keys icons files dirs selfiles seldirs devices assigns source dest gauge, one
 * `set pens` line per key (never `user`: that is DOpus's own user-pen count), then "dopus refresh lister".
 * With palette != 0 the first line is "dopus set palette ..." (only meaningful when Opus has its own screen: on the
 * Workbench screen the palette is Workbench's, see wb_pens_plan). Every line ends in a newline; then a NUL.
 * Returns the length written (without the NUL), or 0 when th is NULL, a colour is above 0xFFFFFF, or cap is too
 * small (out[0] is then 0 when cap is at least 1). */
unsigned dopus_theme_commands(const gtheme *th, char *out, unsigned cap, int palette);

/* Directory Opus's ARexx port */
#define DOPUS_PORT "DOPUS.1"
/* The flat toolbar banks the up-dopus add-on installs, one per family and size set (dist/dopus-toolbar.txt, tools/gen_dopus_toolbar.py):
 * DOPUS_TOOLBAR_FLAT followed by the set name ("filled-24", "outline-24t", see dopustb_core.h dt_set_name). A bank holds its brushes'
 * paths, so neither the size nor the family can change without another bank. DOPUS_TOOLBAR_SET is the set used when the screen is
 * not known (filled, the default family). */
#define DOPUS_TOOLBAR_FLAT "DOpus5:Buttons/Amarchy-toolbar-"
#define DOPUS_TOOLBAR_SET "filled-24"

/* The Amiga side of the toolbar set and blend (amarchy/dopustb_dos.c); dopus_theme_send takes NULL for none (default set, brushes
 * left as they are). screen: font height and pixel aspect of the DOpus screen (returns 0 when it cannot be read: set 24, square).
 * recolour: writes the theme's blend (dt_cmap) into the brushes of the named set; returns how many files it changed.
 * outline (DT-9): 1 when icon-style is outline (the Lucide family), 0 for filled (Phosphor); NULL = filled, the default. */
typedef struct {
    int (*screen)(unsigned *font_h, int *rx, int *ry, void *ud);
    int (*recolour)(const char *set, const unsigned char rgb[8][3], void *ud);
    int (*outline)(void *ud);
    void *ud;
} dopus_tb_ops;

/* Asks DOPUS_PORT "dopus screen" first (send(port, line, result, cap, ud); result NULL = no reply wanted), then switches every lister whose
 * `lister query <handle> toolbar` is upstream's default (PROGDIR:Buttons/Toolbar or DOpus5:Buttons/Toolbar) or one of our banks
 * to the bank of the set tb picks (DOPUS_TOOLBAR_FLAT + set) with `lister set <handle> toolbar` (a lister that loses its toolbar by it
 * gets the old path back). Our bank already on a lister is set again only when the brushes were recoloured (DT-4: that is the
 * refresh that makes Opus read them again; rig check pending),
 * then sends dopus_theme_commands' lines one at a time (line has no newline), the palette line only when the reply does not
 * start with "Workbench". send returns -1 when the port is absent (Directory Opus not running: normal, not an
 * error), and sending stops at that line. Returns the number of lines handed to send (including the screen query and
 * a refused one); 0 when th or send is NULL. */
int dopus_theme_send(const gtheme *th, long (*send)(const char *port, const char *line, char *result, int cap, void *ud),
                     void *ud, const dopus_tb_ops *tb);

/* Workbench pens 0-3 take the theme's bg, fg, dim, accent (Directory Opus on the Workbench screen shares them). */
#define WB_PENS 4
/* The refusal threshold is GT_FLOOR_TEXT (gthemes_core.h): the text-on-ground contrast floor of the role derivation. */
/* Fills rgb[0..3] with the theme's pens 0-3. Returns 1 when the pens may be set, 0 when th is NULL, a colour is above
 * 0xFFFFFF, or pen 0 / pen 1 gt_contrast is below GT_FLOOR_TEXT (a Workbench nobody can read: rgb is still filled). */
int wb_pens_plan(const gtheme *th, uc_u32 rgb[WB_PENS]);

#endif
