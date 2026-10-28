/* The font and pixel shape the Amarchy drawn menu uses on a screen (plan 2026-10-23-amarchy-menu-aspect.md).
 * Shared by amarchy-gui and tests/amiga/scrprobe, so the rig measures the font the menu really opens. */
#ifndef MENU_FONT_H
#define MENU_FONT_H

struct Screen;
struct TextFont;

/* The screen's pixel shape from its display mode: *rx, *ry = DisplayInfo.Resolution (0, 0 when the mode is
 * unknown). Returns 1 when the pixels are square or the shape is unknown. */
int menu_screen_aspect(struct Screen *scr, int *rx, int *ry);

/* The menu's font on this screen (render/fontpair.c fontpair_menu_choices: JetBrainsMono 16, then TopazPro 16 on
 * square pixels that fit 25 rows; topaz 8 on tall ones), the first that opens from memory or FONTS:. When none
 * does: the screen's own font. *opened is
 * 1 when the caller must CloseFont the result. */
/* One choice of fontpair_menu_choices as a bitmap font, from memory or FONTS: (no requester when missing);
 * 0 when it does not open at its size. The caller CloseFonts it. */
struct fontpair_choice;
struct TextFont *menu_font_open_choice(const struct fontpair_choice *ch);
struct TextFont *menu_font_open(struct Screen *scr, int square, const char *pref_name, int pref_size, int *opened);

#endif
