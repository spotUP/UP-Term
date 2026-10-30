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

/* Directory Opus's lister colours come from the theme directly, not from the menu roles (menu_theme.h). One
 * `set pens` line per key: a line with several keys read the later keys back wrong on the rig (2026-10-29). */
unsigned dopus_theme_commands(const gtheme *th, char *out, unsigned cap, int palette)
{
    static const char *const pens[] = { "dopus set pens icons 1 0 0 1 0 0\n", "dopus set pens files 1 0\n",
                                       "dopus set pens dirs 3 0\n", "dopus set pens selfiles 0 3\n",
                                       "dopus set pens seldirs 0 3\n", "dopus set pens devices 1 0\n",
                                       "dopus set pens assigns 1 0\n", "dopus set pens source 0 3\n",
                                       "dopus set pens dest 1 2\n", "dopus set pens gauge 3 0\n",
                                       "dopus refresh lister\n" };
    static const char head[] = "dopus set palette";
    static const char hex[] = "0123456789abcdef";
    unsigned n = 0, i, k, need = 1;
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
    for (i = 0; i < sizeof pens / sizeof pens[0]; i++)
        need += (unsigned)strlen(pens[i]);
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

/* sRGB channel -> linear light, x100000 (the WCAG 2.x formula; generated, no libm on the Amiga) */
static const unsigned long wb_lin[256] = {
    0, 30, 61, 91, 121, 152, 182, 212, 243, 273, 304, 335, 368, 402, 439, 478,
    518, 561, 605, 651, 700, 750, 802, 857, 913, 972, 1033, 1096, 1161, 1229, 1298, 1370,
    1444, 1521, 1600, 1681, 1764, 1850, 1938, 2029, 2122, 2217, 2315, 2416, 2519, 2624, 2732, 2843,
    2956, 3071, 3190, 3310, 3434, 3560, 3689, 3820, 3955, 4092, 4231, 4374, 4519, 4667, 4817, 4971,
    5127, 5286, 5448, 5613, 5781, 5951, 6125, 6301, 6480, 6663, 6848, 7036, 7227, 7421, 7619, 7819,
    8022, 8228, 8438, 8650, 8866, 9084, 9306, 9531, 9759, 9990, 10224, 10462, 10702, 10946, 11193, 11444,
    11697, 11954, 12214, 12477, 12744, 13014, 13287, 13563, 13843, 14126, 14413, 14703, 14996, 15293, 15593, 15896,
    16203, 16513, 16827, 17144, 17465, 17789, 18116, 18447, 18782, 19120, 19462, 19807, 20156, 20508, 20864, 21223,
    21586, 21953, 22323, 22697, 23074, 23455, 23840, 24228, 24620, 25016, 25415, 25818, 26225, 26636, 27050, 27468,
    27889, 28315, 28744, 29177, 29614, 30054, 30499, 30947, 31399, 31855, 32314, 32778, 33245, 33716, 34191, 34670,
    35153, 35640, 36131, 36625, 37124, 37626, 38133, 38643, 39157, 39676, 40198, 40724, 41254, 41789, 42327, 42869,
    43415, 43966, 44520, 45079, 45641, 46208, 46778, 47353, 47932, 48515, 49102, 49693, 50289, 50888, 51492, 52100,
    52712, 53328, 53948, 54572, 55201, 55834, 56471, 57112, 57758, 58408, 59062, 59720, 60383, 61050, 61721, 62396,
    63076, 63760, 64448, 65141, 65837, 66539, 67244, 67954, 68669, 69387, 70110, 70838, 71569, 72306, 73046, 73791,
    74540, 75294, 76052, 76815, 77582, 78354, 79130, 79910, 80695, 81485, 82279, 83077, 83880, 84687, 85499, 86316,
    87137, 87962, 88792, 89627, 90466, 91310, 92158, 93011, 93869, 94731, 95597, 96469, 97345, 98225, 99110, 100000,
};

static unsigned long wb_luma(uc_u32 c)
{
    /* 0.2126 R + 0.7152 G + 0.0722 B, x1e6 */
    return (wb_lin[(c >> 16) & 255] * 2126UL + wb_lin[(c >> 8) & 255] * 7152UL + wb_lin[c & 255] * 722UL) / 1000UL;
}

unsigned wb_contrast_x100(uc_u32 a, uc_u32 b)
{
    unsigned long la = wb_luma(a), lb = wb_luma(b), hi = la > lb ? la : lb, lo = la > lb ? lb : la;
    return (unsigned)(((hi + 50000UL) * 100UL) / (lo + 50000UL));
}

int wb_pens_plan(const gtheme *th, uc_u32 rgb[WB_PENS])
{
    if (!th || th->bg > 0xFFFFFFUL || th->fg > 0xFFFFFFUL || th->dim > 0xFFFFFFUL || th->accent > 0xFFFFFFUL)
        return 0;
    rgb[0] = th->bg;
    rgb[1] = th->fg;
    rgb[2] = th->dim;
    rgb[3] = th->accent;
    return wb_contrast_x100(rgb[0], rgb[1]) >= WB_CONTRAST_MIN_X100;
}
