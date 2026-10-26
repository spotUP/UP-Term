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

int menu_theme_minimal(uc_u32 rgb[MT_ROLES])
{
    uc_u32 bg = rgb[MT_BG], fg = rgb[MT_TEXT];
    rgb[MT_BG] = rgb[MT_ACCENT] = bg;
    rgb[MT_TEXT] = rgb[MT_DIM] = rgb[MT_SELBOX] = fg;
    return bg != fg;
}

/* Directory Opus's own pen numbers for the roles are the palette colour numbers (menu_theme.h) */
unsigned menu_theme_dopus_commands(const uc_u32 rgb[MT_ROLES], char *out, unsigned cap)
{
    static const char head[] = "dopus set palette";
    static const char pens[] = "dopus set pens files 1 0 dirs 3 0 selfiles 1 4 seldirs 3 4\n";
    static const char hex[] = "0123456789abcdef";
    unsigned n = 0, i, k;
    if (cap)
        out[0] = 0;
    if (!rgb || !out)
        return 0;
    for (i = 0; i < MT_ROLES; i++)
        if (rgb[i] > 0xFFFFFFUL)
            return 0;
    /* head, five " 0xRRGGBB", newline, pens (its own newline), NUL */
    if (cap < sizeof head - 1 + MT_ROLES * 9 + 1 + sizeof pens)
        return 0;
    for (k = 0; head[k]; k++)
        out[n++] = head[k];
    for (i = 0; i < MT_ROLES; i++) {
        int sh;
        out[n++] = ' ';
        out[n++] = '0';
        out[n++] = 'x';
        for (sh = 20; sh >= 0; sh -= 4)
            out[n++] = hex[(rgb[i] >> sh) & 15];
    }
    out[n++] = '\n';
    for (k = 0; pens[k]; k++)
        out[n++] = pens[k];
    out[n] = 0;
    return n;
}
