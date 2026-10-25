/* menu_theme -- see menu_theme.h */
#include "menu_theme.h"

/* role -> palette index, and that index's xterm colour. Default palette: black ground, light grey text, grey
 * header (5:1 on black), cyan accent, blue selected box (cyan on it reads at 8:1). */
static const struct {
    int idx;
    uc_u32 dflt;
} roles[MT_ROLES] = {
    { 0, 0x000000UL },  /* MT_BG:     Black */
    { 7, 0xE5E5E5UL },  /* MT_TEXT:   White */
    { 8, 0x7F7F7FUL },  /* MT_DIM:    Bright Black */
    { 14, 0x00FFFFUL }, /* MT_ACCENT: Bright Cyan */
    { 4, 0x0000EEUL }   /* MT_SELBOX: Blue */
};

int menu_theme_index(int role)
{
    return role >= 0 && role < MT_ROLES ? roles[role].idx : 0;
}

void menu_theme_roles_fb(const uc_u32 pal16[16], uc_u32 fg, uc_u32 bg, uc_u32 rgb[MT_ROLES])
{
    int i;
    for (i = 0; i < MT_ROLES; i++) {
        uc_u32 v = pal16 ? pal16[roles[i].idx] : 0;
        /* the terminal draws its ground and text from the upconf bg and fg keys when they are set, and
         * the menu follows it; the palette (then the xterm default) only answers for what is not set */
        if (i == MT_BG && (bg & 0x01000000UL))
            v = bg;
        else if (i == MT_TEXT && (fg & 0x01000000UL))
            v = fg;
        rgb[i] = (v & 0x01000000UL) ? (v & 0xFFFFFFUL) : roles[i].dflt;
    }
}

void menu_theme_roles(const uc_u32 pal16[16], uc_u32 rgb[MT_ROLES])
{
    menu_theme_roles_fb(pal16, 0, 0, rgb);
}
