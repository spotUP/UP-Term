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
    { 4, 0x0000EEUL },  /* MT_SELBOX: Blue */
    { 10, 0x00FF00UL }, /* MT_OK:     Bright Green */
    { 9, 0xFF0000UL }   /* MT_FAIL:   Bright Red */
};

int menu_theme_fallback_role(int role)
{
    return role == MT_OK || role == MT_FAIL ? MT_TEXT : -1;
}

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

void menu_theme_roles_gui(const gtheme *t, uc_u32 rgb[MT_ROLES])
{
    rgb[MT_BG] = t->bg & 0xFFFFFFUL;
    rgb[MT_TEXT] = t->fg & 0xFFFFFFUL;
    rgb[MT_DIM] = t->dim & 0xFFFFFFUL;
    rgb[MT_ACCENT] = t->bg & 0xFFFFFFUL;
    rgb[MT_SELBOX] = t->accent & 0xFFFFFFUL;
    rgb[MT_OK] = t->ok & 0xFFFFFFUL;
    rgb[MT_FAIL] = t->fail & 0xFFFFFFUL;
}

int menu_theme_minimal(uc_u32 rgb[MT_ROLES])
{
    uc_u32 bg = rgb[MT_BG], fg = rgb[MT_TEXT];
    rgb[MT_BG] = rgb[MT_ACCENT] = bg;
    rgb[MT_TEXT] = rgb[MT_DIM] = rgb[MT_SELBOX] = rgb[MT_OK] = rgb[MT_FAIL] = fg;
    return bg != fg;
}

void menu_theme_ink(uc_u32 out[MT_ROLES], const uc_u32 rgb[MT_ROLES], const uc_u32 pen_rgb[MT_ROLES], int direct,
                    int themed)
{
    int r;
    for (r = 0; r < MT_ROLES; r++)
        out[r] = direct && themed ? rgb[r] : pen_rgb[r];
}

/* Directory Opus's lister palette comes from the theme directly, not from the menu roles (menu_theme.h) */
unsigned dopus_theme_commands(const gtheme *th, char *out, unsigned cap)
{
    static const char head[] = "dopus set palette";
    static const char pens[] = "dopus set pens files 1 0 dirs 3 0 selfiles 0 3 seldirs 0 3\n";
    static const char hex[] = "0123456789abcdef";
    unsigned n = 0, i, k;
    uc_u32 col[DOPUS_THEME_COLOURS];
    if (cap)
        out[0] = 0;
    if (!th || !out)
        return 0;
    col[0] = th->bg;
    col[1] = th->fg;
    col[2] = th->dim;
    col[3] = th->accent;
    for (i = 0; i < DOPUS_THEME_COLOURS; i++)
        if (col[i] > 0xFFFFFFUL)
            return 0;
    /* head, four " 0xRRGGBB", newline, pens (its own newline), NUL */
    if (cap < sizeof head - 1 + DOPUS_THEME_COLOURS * 9 + 1 + sizeof pens)
        return 0;
    for (k = 0; head[k]; k++)
        out[n++] = head[k];
    for (i = 0; i < DOPUS_THEME_COLOURS; i++) {
        int sh;
        out[n++] = ' ';
        out[n++] = '0';
        out[n++] = 'x';
        for (sh = 20; sh >= 0; sh -= 4)
            out[n++] = hex[(col[i] >> sh) & 15];
    }
    out[n++] = '\n';
    for (k = 0; pens[k]; k++)
        out[n++] = pens[k];
    out[n] = 0;
    return n;
}

int dopus_theme_send(const gtheme *th, long (*send)(const char *port, const char *line, void *ud), void *ud)
{
    char cmds[256], line[192];
    unsigned n, i = 0, k;
    int sent = 0;
    if (!send)
        return 0;
    n = dopus_theme_commands(th, cmds, sizeof cmds);
    while (i < n) {
        for (k = 0; i < n && cmds[i] != '\n'; i++)
            if (k < sizeof line - 1)
                line[k++] = cmds[i];
        i++;
        line[k] = 0;
        sent++;
        if (send(DOPUS_PORT, line, ud) == -1)
            break;
    }
    return sent;
}
