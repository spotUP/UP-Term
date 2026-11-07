/* menu_theme -- see menu_theme.h */
#include <string.h>
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
    { 14, 0x00FFFFUL }, /* MT_SELTEXT: Bright Cyan */
    { 4, 0x0000EEUL },  /* MT_THEME_ACCENT: Blue */
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
    gt_roles r;
    gt_roles_of(t, &r);
    /* TE-2.2: an element of the theme model that has its own ref takes its role; the group value is the fallback. MT_DIM has no element. */
    rgb[MT_BG] = gt_elem_rgb(t, GTE_MENU_BACKGROUND, r.c[GR_SURFACE]) & 0xFFFFFFUL;
    rgb[MT_TEXT] = gt_elem_rgb(t, GTE_MENU_TEXT, r.c[GR_TEXT]) & 0xFFFFFFUL;
    rgb[MT_DIM] = r.dim & 0xFFFFFFUL;
    rgb[MT_SELTEXT] = gt_elem_rgb(t, GTE_MENU_SELECTED_TEXT, r.c[GR_SURFACE]) & 0xFFFFFFUL;
    rgb[MT_THEME_ACCENT] = gt_elem_rgb(t, GTE_MENU_HEADER, r.c[GR_ACCENT]) & 0xFFFFFFUL;
    rgb[MT_OK] = gt_elem_rgb(t, GTE_SUCCESS_TEXT, r.ok) & 0xFFFFFFUL;
    rgb[MT_FAIL] = gt_elem_rgb(t, GTE_FAILURE_TEXT, r.fail) & 0xFFFFFFUL;
}

int menu_theme_minimal(uc_u32 rgb[MT_ROLES])
{
    uc_u32 bg = rgb[MT_BG], fg = rgb[MT_TEXT];
    rgb[MT_BG] = rgb[MT_SELTEXT] = bg;
    rgb[MT_TEXT] = rgb[MT_DIM] = rgb[MT_THEME_ACCENT] = rgb[MT_OK] = rgb[MT_FAIL] = fg;
    return bg != fg;
}

void menu_theme_ink(uc_u32 out[MT_ROLES], const uc_u32 rgb[MT_ROLES], const uc_u32 pen_rgb[MT_ROLES], int direct,
                    int themed)
{
    int r;
    for (r = 0; r < MT_ROLES; r++)
        out[r] = direct && themed ? rgb[r] : pen_rgb[r];
}

/* Directory Opus's lister colours come from the theme directly, not from the menu roles (menu_theme.h). One
 * `set pens` line per key: a line with several keys read the later keys back wrong on the rig (2026-10-29). */
unsigned dopus_theme_commands(const gtheme *th, char *out, unsigned cap, int palette)
{
    static const char *const pens[] = { "dopus set pens icons 1 0 0 1 0 0\n", "dopus set pens files 1 0\n",
                                       "dopus set pens dirs 3 0\n", "dopus set pens selfiles 0 3\n",
                                       "dopus set pens seldirs 0 3\n", "dopus set pens devices 1 0\n",
                                       "dopus set pens assigns 1 0\n", "dopus set pens source 0 3\n",
                                       "dopus set pens dest 1 2\n", "dopus set pens gauge 3 0\n" };
    /* `set palette` and `set pens` only write the environment; an own screen loads its palette in display_open, which
     * `refresh all` reruns (dopus5 rexx_dopus.c:948, main_reset.c:40, display.c:267). `refresh lister` only redraws
     * (rexx_dopus.c:960): right for the Workbench screen, where a reopen would flicker every window. */
    const char *const refresh = palette ? "dopus refresh all\n" : "dopus refresh lister\n";
    static const char head[] = "dopus set palette";
    static const char hex[] = "0123456789abcdef";
    unsigned n = 0, i, k, need = 1;
    uc_u32 col[DOPUS_THEME_COLOURS];
    gt_roles r;
    if (cap)
        out[0] = 0;
    if (!th || !out)
        return 0;
    gt_roles_of(th, &r);    /* RL8: the lister's four colours are roles of the table */
    col[0] = r.c[GR_SURFACE];
    col[1] = r.c[GR_TEXT];
    col[2] = r.c[GR_MUTED];
    col[3] = r.c[GR_ACCENT];
    for (i = 0; i < DOPUS_THEME_COLOURS; i++)
        if (col[i] > 0xFFFFFFUL)
            return 0;
    for (i = 0; i < sizeof pens / sizeof pens[0]; i++)
        need += (unsigned)strlen(pens[i]);
    need += (unsigned)strlen(refresh);
    if (palette)
        need += sizeof head - 1 + DOPUS_THEME_COLOURS * 9 + 1;
    if (cap < need)
        return 0;
    if (palette) {
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
    }
    for (i = 0; i < sizeof pens / sizeof pens[0]; i++)
        for (k = 0; pens[i][k]; k++)
            out[n++] = pens[i][k];
    for (k = 0; refresh[k]; k++)
        out[n++] = refresh[k];
    out[n] = 0;
    return n;
}

/* does the `dopus screen` reply name the Workbench screen ("Workbench 800 600 4 ...")? */
static int dopus_on_workbench(const char *reply)
{
    static const char wb[] = "workbench";
    unsigned k;
    for (k = 0; wb[k]; k++) {
        char c = reply[k];
        if (c >= 'A' && c <= 'Z')
            c = (char)(c + 32);
        if (c != wb[k])
            return 0;
    }
    return 1;
}

/* case-insensitive equality of a reply with a path; a trailing newline or blank of the reply is ignored */
static int dopus_same_path(const char *reply, const char *path)
{
    unsigned k = 0;
    for (;; k++) {
        char a = reply[k], b = path[k];
        if (a >= 'A' && a <= 'Z')
            a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z')
            b = (char)(b + 32);
        if (!b) {
            while (reply[k] == ' ' || reply[k] == '\n' || reply[k] == '\r')
                k++;
            return reply[k] == 0;
        }
        if (a != b)
            return 0;
    }
}

/* line = a ... b ... c ... (NULL ends), truncated to cap-1 */
static void dopus_line(char *line, unsigned cap, const char *a, const char *b, const char *c, const char *d)
{
    const char *part[4];
    unsigned n = 0, i, k;
    part[0] = a; part[1] = b; part[2] = c; part[3] = d;
    for (i = 0; i < 4 && part[i]; i++)
        for (k = 0; part[i][k] && n + 1 < cap; k++)
            line[n++] = part[i][k];
    line[n] = 0;
}

/* B7: the listers still on upstream's default toolbar get the flat bank (DOPUS_TOOLBAR_FLAT). A lister's toolbar is
 * its own `toolbar_path` (dopus5 rexx_lister.c:613 query, :1269 set; `lister query all` lists the handles, `all` is
 * not allowed for set). Any other path, a user's own bank or one already switched, is left alone. A lister that cannot
 * open the new bank loses its toolbar (lister_toolbar.c lister_new_toolbar clears the path): the re-query catches that
 * and puts the old path back. Returns the number of lines handed to send. */
static int dopus_toolbar_flat(long (*send)(const char *port, const char *line, char *result, int cap, void *ud),
                              void *ud)
{
    static const char *const upstream[] = { "PROGDIR:Buttons/Toolbar", "DOpus5:Buttons/Toolbar" };
    char handles[160], line[160], path[160], hnd[24];
    unsigned i = 0, k, d;
    int sent = 1;
    handles[0] = 0;
    if (send(DOPUS_PORT, "lister query all", handles, (int)sizeof handles, ud) == -1)
        return sent;
    while (handles[i]) {
        while (handles[i] == ' ' || handles[i] == '\n' || handles[i] == '\r')
            i++;
        for (k = 0; handles[i] > ' ' && k < sizeof hnd - 1; i++)
            hnd[k++] = handles[i];
        hnd[k] = 0;
        if (!k)
            break;
        dopus_line(line, sizeof line, "lister query ", hnd, " toolbar", 0);
        path[0] = 0;
        sent++;
        if (send(DOPUS_PORT, line, path, (int)sizeof path, ud) == -1)
            return sent;
        for (d = 0; d < sizeof upstream / sizeof upstream[0] && !dopus_same_path(path, upstream[d]); d++)
            ;
        if (d == sizeof upstream / sizeof upstream[0])
            continue;
        dopus_line(line, sizeof line, "lister set ", hnd, " toolbar ", DOPUS_TOOLBAR_FLAT);
        sent++;
        if (send(DOPUS_PORT, line, 0, 0, ud) == -1)
            return sent;
        dopus_line(line, sizeof line, "lister query ", hnd, " toolbar", 0);
        path[0] = 0;
        sent++;
        if (send(DOPUS_PORT, line, path, (int)sizeof path, ud) == -1)
            return sent;
        if (!dopus_same_path(path, DOPUS_TOOLBAR_FLAT)) {
            dopus_line(line, sizeof line, "lister set ", hnd, " toolbar ", upstream[d]);
            sent++;
            if (send(DOPUS_PORT, line, 0, 0, ud) == -1)
                return sent;
        }
    }
    return sent;
}

int dopus_theme_send(const gtheme *th, long (*send)(const char *port, const char *line, char *result, int cap, void *ud),
                     void *ud)
{
    char cmds[512], line[192], reply[160];
    unsigned n, i = 0, k;
    int sent = 1, on_wb;
    if (!send || !th)
        return 0;
    /* a DOpus on the Workbench screen shares Workbench's palette: set palette would not reach it (it only recolours
     * Opus's own screen), and the Workbench pens are set by wb_pens_apply instead */
    if (send(DOPUS_PORT, "dopus screen", reply, (int)sizeof reply, ud) == -1)
        return sent;
    on_wb = dopus_on_workbench(reply);
    sent += dopus_toolbar_flat(send, ud);   /* before the refresh, the last line of dopus_theme_commands */
    n = dopus_theme_commands(th, cmds, sizeof cmds, !on_wb);
    while (i < n) {
        for (k = 0; i < n && cmds[i] != '\n'; i++)
            if (k < sizeof line - 1)
                line[k++] = cmds[i];
        i++;
        line[k] = 0;
        sent++;
        if (send(DOPUS_PORT, line, 0, 0, ud) == -1)
            break;
    }
    return sent;
}

int wb_pens_plan(const gtheme *th, uc_u32 rgb[WB_PENS])
{
    gt_roles r;
    if (!th)
        return 0;
    gt_roles_of(th, &r);    /* RL8: Workbench pens 0-3 are the surface, text, muted and accent roles */
    rgb[0] = r.c[GR_SURFACE];
    rgb[1] = r.c[GR_TEXT];
    rgb[2] = r.c[GR_MUTED];
    rgb[3] = r.c[GR_ACCENT];
    if (rgb[0] > 0xFFFFFFUL || rgb[1] > 0xFFFFFFUL || rgb[2] > 0xFFFFFFUL || rgb[3] > 0xFFFFFFUL)
        return 0;
    return gt_contrast(rgb[0], rgb[1]) >= GT_FLOOR_TEXT;
}
