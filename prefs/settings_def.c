/* settings_def: the settings table (see settings_def.h). */
#include <stddef.h>
#include <string.h>

#include "settings_def.h"
#include "prefs_core.h"
#include "../amarchy/amconf_core.h"

#define FO(m) offsetof(prefs_fields, m)
#define FS(m) ((int)sizeof(((prefs_fields *)0)->m))
#define AO(k) (offsetof(am_prefs, val) + (size_t)(k) * UC_MAX_VALUE)
#define VALS(a) a, (int)(sizeof(a) / sizeof((a)[0]))
#define NOVALS 0, 0
#define NORANGE 0, 0

/* a check's file texts: off first, on second */
static const sd_value v_off_on[] = { { "off", "Off" }, { "on", "On" } };
static const sd_value v_ignore_scroll[] = { { "ignore", "Off" }, { "scroll", "On" } };
static const sd_value v_hide_show[] = { { "hide", "Off" }, { "show", "On" } };
static const sd_value v_background_extend[] = { { "background", "Off" }, { "extend", "On" } };
static const sd_value v_kc_common[] = { { "", "Off" }, { "C", "On" } };
static const sd_value v_kc_silent[] = { { "", "Off" }, { "S", "On" } };

/* the cycles, in the order of the gadget's choices */
static const sd_value v_cursor[] = { { "block", "Block" }, { "underline", "Underline" }, { "bar", "Bar" } };
static const sd_value v_bell[] = { { "none", "None" }, { "beep", "Beep" }, { "visual", "Visual" } };
static const sd_value v_meta[] = { { "amiga", "Left Amiga" }, { "alt", "Alt" } };
static const sd_value v_backspace[] = { { "del", "Delete" }, { "bs", "Backspace" } };
static const sd_value v_complete[] = { { "unix", "Unix" }, { "kingcon", "KingCON" } };
/* PREFS_KC_* order; the file text is the KingCON letters */
static const sd_value v_kc_match[] = { { "W", "Window" }, { "L", "List under the line" }, { "B", "Tab cycles" },
                                       { "LB", "List and Tab cycles" }, { "C", "Common part only" } };
static const sd_value v_screen[] = { { "workbench", "Workbench" }, { "own", "Own screen" },
                                     { "fullscreen", "Full screen" } };
static const sd_value v_clipboard[] = { { "write", "Set the clipboard" }, { "read-write", "Set and read the clipboard" },
                                        { "off", "Not use the clipboard" } };
/* prefs_colors_index order */
static const sd_value v_colors[] = { { "", "Not told" }, { "rgb", "Full colour (RGB)" }, { "256", "256 colours" },
                                     { "16", "16 colours" } };
static const sd_value v_icons[] = { { "generic", "Generic" }, { "amiga", "Amiga" }, { "none", "None" } };
static const sd_value v_termmenu[] = { { "text", "Text" }, { "graphical", "Graphical" } };
static const sd_value v_mode[] = { { "graphical", "Drawn" }, { "text", "Text" }, { "minimal", "Minimal" } };

#define PAL_HELP "One of the 16 palette colours. Click to pick one; empty: the built-in."

const sd_setting sd_settings[SDI_COUNT] = {
    /* General */
    { SDI_PROFILE, 0, 0, PG_PAGE_GENERAL, SD_PROFILE, "_Profile",
      "The profile being edited; each profile is one set of settings.",
      NOVALS, "", SD_UPTERM, SDF_NONE, 0, 0, NORANGE, PG_PROFILE, 0, 0, 1, 0 },
    { SDI_FONT, "font", 0, PG_PAGE_GENERAL, SD_FONT, "Fon_t",
      "The window font; empty: the screen's font. Click to choose.",
      NOVALS, "", SD_UPTERM, SDF_STR, FO(font), FS(font), NORANGE, PG_FONT, 0, 0, 1, 0 },
    { SDI_FALLBACK, "font-fallback", 0, PG_PAGE_GENERAL, SD_FONT, "_Fallback font",
      "Outline font for glyphs the font lacks. Click to choose.",
      NOVALS, "", SD_UPTERM, SDF_STR, FO(fallback), FS(fallback), NORANGE, PG_FALLBACK, 0, 0, 1, 0 },
    /* 0 is "No scrollback"; an absent key keeps 500 */
    { SDI_SCROLLBACK, "scrollback", 0, PG_PAGE_GENERAL, SD_INT, "Scrollbac_k",
      "Lines kept above the window, 1 to 99999 (500 when not set); 0: none.",
      NOVALS, "", SD_UPTERM, SDF_STR, FO(sb), FS(sb), 0, 99999, PG_SCROLLBACK, PG_SCROLLBACK_NONE, 0, 1, 0 },
    { SDI_CURSOR, "cursor", 0, PG_PAGE_GENERAL, SD_CYCLE, "Curso_r",
      "The shape of the cursor.",
      VALS(v_cursor), "block", SD_UPTERM, SDF_INT, FO(cursor), 0, NORANGE, PG_CURSOR, 0, 0, 1, 0 },
    { SDI_BLINK, "cursor-blink", 0, PG_PAGE_GENERAL, SD_CHECK, "Cursor bl_inks",
      "The cursor blinks.",
      VALS(v_off_on), "off", SD_UPTERM, SDF_INT, FO(blink), 0, NORANGE, PG_BLINK, 0, 0, 1, 0 },
    /* a colour, or the word "inverse" (the Inverse check) */
    { SDI_CURSOR_COLOUR, "cursor-color", 0, PG_PAGE_GENERAL, SD_COLOUR, "Cursor co_lour",
      "The cursor's colour, or Inverse. Click to pick one.",
      NOVALS, "", SD_UPTERM, SDF_STR, FO(curcol), FS(curcol), NORANGE,
      PG_CURSOR_COLOUR, PG_CURSOR_COLOUR_HEX, PG_CURSOR_INVERSE, 1, 0 },
    { SDI_BELL, "bell", 0, PG_PAGE_GENERAL, SD_CYCLE, "_Bell",
      "What the bell character does.",
      VALS(v_bell), "beep", SD_UPTERM, SDF_INT, FO(bell), 0, NORANGE, PG_BELL, 0, 0, 1, 0 },
    { SDI_BOLD, "bold-bright", 0, PG_PAGE_GENERAL, SD_CHECK, "Bold is brig_ht",
      "Bold text uses the bright colours.",
      VALS(v_off_on), "on", SD_UPTERM, SDF_INT, FO(bold), 0, NORANGE, PG_BOLD, 0, 0, 1, 0 },
    { SDI_SCROLLBAR, "scrollbar", 0, PG_PAGE_GENERAL, SD_CHECK, "Scroll b_ar",
      "Show the scroll bar in the window border.",
      VALS(v_hide_show), "show", SD_UPTERM, SDF_INT, FO(scrollbar), 0, NORANGE, PG_SCROLLBAR, 0, 0, 1, 0 },
    /* Colors */
    { SDI_FG, "fg", 0, PG_PAGE_COLORS, SD_COLOUR, "_Text colour",
      "The colour of text. Click to pick one.",
      NOVALS, "", SD_UPTERM, SDF_STR, FO(fg), FS(fg), NORANGE, PG_FG, PG_FG_HEX, 0, 1, 0 },
    { SDI_BG, "bg", 0, PG_PAGE_COLORS, SD_COLOUR, "_Background",
      "The window's background colour. Click to pick one.",
      NOVALS, "", SD_UPTERM, SDF_STR, FO(bg), FS(bg), NORANGE, PG_BG, PG_BG_HEX, 0, 1, 0 },
    { SDI_SELFG, "selection-fg", 0, PG_PAGE_COLORS, SD_COLOUR, "Selected te_xt",
      "The colour of selected text; empty swaps the colours. Click to pick one.",
      NOVALS, "", SD_UPTERM, SDF_STR, FO(selfg), FS(selfg), NORANGE, PG_SELFG, PG_SELFG_HEX, 0, 1, 0 },
    { SDI_SELBG, "selection-bg", 0, PG_PAGE_COLORS, SD_COLOUR, "Se_lection",
      "The colour behind selected text; empty swaps the colours. Click to pick one.",
      NOVALS, "", SD_UPTERM, SDF_STR, FO(selbg), FS(selbg), NORANGE, PG_SELBG, PG_SELBG_HEX, 0, 1, 0 },
    { SDI_THEME, 0, 0, PG_PAGE_COLORS, SD_THEME, "T_heme...",
      "Take the colours of a theme file.",
      NOVALS, "", SD_UPTERM, SDF_NONE, 0, 0, NORANGE, PG_THEME, 0, 0, 1, 0 },
    { SDI_DITHER, "dither", 0, PG_PAGE_COLORS, SD_CHECK, "_Dither on few colours",
      "On a screen of few colours, mix two colours for one that is missing.",
      VALS(v_off_on), "on", SD_UPTERM, SDF_INT, FO(dither), 0, NORANGE, PG_DITHER, 0, 0, 1, 0 },
    { SDI_OUTLINE_AA, "outline-antialias", 0, PG_PAGE_COLORS, SD_CHECK, "_Antialias outline glyphs",
      "Fallback font glyphs get soft edges in shades between text and background.",
      VALS(v_off_on), "on", SD_UPTERM, SDF_INT, FO(outline_aa), 0, NORANGE, PG_OUTLINE_AA, 0, 0, 1, 0 },
    { SDI_PADDING, "padding-color", 0, PG_PAGE_COLORS, SD_CHECK, "Padding _extends edges",
      "The strips beside the text take the colour of the edge cells.",
      VALS(v_background_extend), "background", SD_UPTERM, SDF_INT, FO(pad_extend), 0, NORANGE,
      PG_PADDING, 0, 0, 1, 0 },
    { SDI_TERM, "term", 0, PG_PAGE_COLORS, SD_STRING, "Ter_minal type",
      "TERM for programs vsh starts; empty: vtcon. /term sets it for one window.",
      NOVALS, "", SD_UPTERM, SDF_STR, FO(term), FS(term), NORANGE, PG_TERM, 0, 0, 1, 0 },
    { SDI_COLORS, "colors", 0, PG_PAGE_COLORS, SD_CYCLE, "Colour m_ode",
      "The colours programs are told they have. /colors sets it for one window.",
      VALS(v_colors), "", SD_UPTERM, SDF_STR, FO(colors), FS(colors), NORANGE, PG_COLORS, 0, 0, 1, 0 },
    /* Palette: the key holds all 16 entries */
    { SDI_PALETTE, "palette", 0, PG_PAGE_PALETTE, SD_PALETTE, "Palette",
      PAL_HELP,
      NOVALS, "", SD_UPTERM, SDF_STR, FO(pal), 16, NORANGE, PG_PAL, PG_PAL_HEX, 0, 16, 0 },
    /* Keyboard */
    { SDI_META, "meta", 0, PG_PAGE_KEYBOARD, SD_CYCLE, "Meta ke_y",
      "Which key acts as Meta.",
      VALS(v_meta), "alt", SD_UPTERM, SDF_INT, FO(meta_alt), 0, NORANGE, PG_META, 0, 0, 1, 0 },
    { SDI_AMIGAKEYS, "amiga-keys", 0, PG_PAGE_KEYBOARD, SD_CHECK, "AmigaShell _keys",
      "Shift with Left and Right sends Home and End.",
      VALS(v_off_on), "on", SD_UPTERM, SDF_INT, FO(amiga_keys), 0, NORANGE, PG_AMIGAKEYS, 0, 0, 1, 0 },
    { SDI_BACKSPACE, "backspace", 0, PG_PAGE_KEYBOARD, SD_CYCLE, "_Backspace key sends",
      "What the Backspace key sends to programs.",
      VALS(v_backspace), "del", SD_UPTERM, SDF_INT, FO(backspace_bs), 0, NORANGE, PG_BACKSPACE, 0, 0, 1, 0 },
    { SDI_COMPLETE, "completion", 0, PG_PAGE_KEYBOARD, SD_CYCLE, "_Tab completion",
      "How the Tab key completes names.",
      VALS(v_complete), "unix", SD_UPTERM, SDF_INT, FO(completion), 0, NORANGE, PG_COMPLETE, 0, 0, 1, 0 },
    /* the three KingCON rows share one key: its letters hold the match, the common part and the beeps */
    { SDI_KC_MATCH, "kingcon-mode", 0, PG_PAGE_KEYBOARD, SD_CYCLE, "Wh_en several names match",
      "What Tab does when several names match (KingCON completion).",
      VALS(v_kc_match), "W", SD_UPTERM, SDF_KC_MATCH, FO(kcmode), FS(kcmode), NORANGE, PG_KC_MATCH, 0, 0, 1, 0 },
    { SDI_KC_COMMON, "kingcon-mode", 0, PG_PAGE_KEYBOARD, SD_CHECK, "Complete the co_mmon part first",
      "Tab first completes what all matching names share.",
      VALS(v_kc_common), "", SD_UPTERM, SDF_KC_COMMON, FO(kcmode), FS(kcmode), NORANGE, PG_KC_COMMON, 0, 0, 1, 0 },
    { SDI_KC_SILENT, "kingcon-mode", 0, PG_PAGE_KEYBOARD, SD_CHECK, "No bee_ps",
      "KingCON completion never beeps.",
      VALS(v_kc_silent), "", SD_UPTERM, SDF_KC_SILENT, FO(kcmode), FS(kcmode), NORANGE, PG_KC_SILENT, 0, 0, 1, 0 },
    { SDI_KC_INFO, "kingcon-info", 0, PG_PAGE_KEYBOARD, SD_CHECK, ".info in _lists",
      "KingCON lists show .info files too.",
      VALS(v_hide_show), "hide", SD_UPTERM, SDF_INT, FO(kcinfo), 0, NORANGE, PG_KC_INFO, 0, 0, 1, 0 },
    { SDI_KC_CACHE, "kingcon-cache", 0, PG_PAGE_KEYBOARD, SD_CHECK, "_Directory cache",
      "KingCON keeps directory listings for the next Tab.",
      VALS(v_off_on), "on", SD_UPTERM, SDF_INT, FO(kccache), 0, NORANGE, PG_KC_CACHE, 0, 0, 1, 0 },
    { SDI_COPY, "copy-on-select", 0, PG_PAGE_KEYBOARD, SD_CHECK, "C_opy on select",
      "Selecting text copies it to the clipboard.",
      VALS(v_off_on), "off", SD_UPTERM, SDF_INT, FO(copy_sel), 0, NORANGE, PG_COPY, 0, 0, 1, 0 },
    { SDI_WHEEL, "wheel", 0, PG_PAGE_KEYBOARD, SD_CHECK, "_Wheel scrolls",
      "The mouse wheel scrolls the window.",
      VALS(v_ignore_scroll), "scroll", SD_UPTERM, SDF_INT, FO(wheel), 0, NORANGE, PG_WHEEL, 0, 0, 1, 0 },
    /* Advanced */
    { SDI_SCREEN, "screen", 0, PG_PAGE_ADVANCED, SD_CYCLE, "Scree_n",
      "Open on the Workbench, on a screen of its own, or on the whole screen.",
      VALS(v_screen), "workbench", SD_UPTERM, SDF_INT, FO(screen), 0, NORANGE, PG_SCREEN, 0, 0, 1, 0 },
    { SDI_SCREEN_MODE, "screen-mode", "screen-depth", PG_PAGE_ADVANCED, SD_SCREENMODE, "Screen _mode",
      "The display mode and depth of an own screen. Click to choose.",
      NOVALS, "", SD_UPTERM, SDF_STR, FO(screenmode), FS(screenmode), NORANGE, PG_SCREEN_MODE, 0, 0, 1, FO(screendepth) },
    { SDI_ASPECT, "font-aspect", 0, PG_PAGE_ADVANCED, SD_CHECK, "Font follows screen _aspect",
      "Swap the font for its pair that keeps its shape on this screen.",
      VALS(v_off_on), "on", SD_UPTERM, SDF_INT, FO(aspect), 0, NORANGE, PG_ASPECT, 0, 0, 1, 0 },
    { SDI_CLIPBOARD, "program-clipboard", 0, PG_PAGE_ADVANCED, SD_CYCLE, "_Programs may",
      "What programs may do with the clipboard (OSC 52).",
      VALS(v_clipboard), "write", SD_UPTERM, SDF_INT, FO(clipboard), 0, NORANGE, PG_CLIPBOARD, 0, 0, 1, 0 },
    { SDI_HLCAT, "highlight-cat", 0, PG_PAGE_ADVANCED, SD_CHECK, "_Highlight files shown with cat",
      "vsh's cat colours the file through hl.",
      VALS(v_off_on), "on", SD_UPTERM, SDF_INT, FO(hlcat), 0, NORANGE, PG_HLCAT, 0, 0, 1, 0 },
    { SDI_REFLOW, "reflow", 0, PG_PAGE_ADVANCED, SD_CHECK, "_Reflow on resize",
      "Re-wrap the lines when the window changes size.",
      VALS(v_off_on), "on", SD_UPTERM, SDF_INT, FO(reflow), 0, NORANGE, PG_REFLOW, 0, 0, 1, 0 },
    { SDI_LINK, "link-open", 0, PG_PAGE_ADVANCED, SD_STRING, "_Link command",
      "The command a Ctrl-clicked link runs; %s is the address.",
      NOVALS, "", SD_UPTERM, SDF_STR, FO(linkopen), FS(linkopen), NORANGE, PG_LINK, 0, 0, 1, 0 },
    /* Amarchy: the keys of ENVARC:Amarchy/prefs, in am_keys[] order of the Prefs page */
    { SDI_AM_MUSIC, "amarchy-music-drawers", 0, PG_PAGE_AMARCHY, SD_STRING, "_Music drawers",
      "More drawers, comma separated, listed under Music in the Amarchy menu.",
      NOVALS, "", SD_AMARCHY, SDF_AMSTR, AO(AM_MUSIC), UC_MAX_VALUE, NORANGE, PG_AM_MUSIC, 0, 0, 1, 0 },
    { SDI_AM_DEMO, "amarchy-demo-drawers", 0, PG_PAGE_AMARCHY, SD_STRING, "_Demo drawers",
      "More drawers, comma separated, listed under Demos in the Amarchy menu.",
      NOVALS, "", SD_AMARCHY, SDF_AMSTR, AO(AM_DEMO), UC_MAX_VALUE, NORANGE, PG_AM_DEMO, 0, 0, 1, 0 },
    { SDI_AM_HIDE, "amarchy-hide-drawers", 0, PG_PAGE_AMARCHY, SD_STRING, "_Hidden drawers",
      "Drawers, comma separated, the Amarchy menu leaves out of its lists.",
      NOVALS, "", SD_AMARCHY, SDF_AMSTR, AO(AM_HIDE), UC_MAX_VALUE, NORANGE, PG_AM_HIDE, 0, 0, 1, 0 },
    { SDI_AM_ICONS, "amarchy-app-icons", 0, PG_PAGE_AMARCHY, SD_CYCLE, "App _icons",
      "Generic: drawn glyphs. Amiga: .info icons of apps and games. None: no icons.",
      VALS(v_icons), "generic", SD_AMARCHY, SDF_AMSTR, AO(AM_ICONS), UC_MAX_VALUE, NORANGE,
      PG_AM_ICONS, 0, 0, 1, 0 },
    { SDI_AM_MODE, "menu-style", 0, PG_PAGE_AMARCHY, SD_CYCLE, "Menu mod_e",
      "Drawn: icons, border. Text: terminal menu. Minimal: text only, two colours.",
      VALS(v_mode), "graphical", SD_AMARCHY, SDF_AMSTR, AO(AM_STYLE), UC_MAX_VALUE, NORANGE,
      PG_AM_MODE, 0, 0, 1, 0 },
    { SDI_AM_TERMMENU, "menu-in-terminal", 0, PG_PAGE_AMARCHY, SD_CYCLE, "Menu in _terminal",
      "Menu over a UP-Term window: Text or Graphical. Full screen: always Text.",
      VALS(v_termmenu), "text", SD_AMARCHY, SDF_AMSTR, AO(AM_TERMMENU), UC_MAX_VALUE, NORANGE,
      PG_AM_TERMMENU, 0, 0, 1, 0 },
    { SDI_AM_GTHEME, "amarchy-gui-theme", 0, PG_PAGE_AMARCHY, SD_DYNLIST, "Me_nu theme",
      "Colours of the drawn menu: a shipped theme or one of your own.",
      NOVALS, "tokyo-night", SD_AMARCHY, SDF_AMSTR, AO(AM_GTHEME), UC_MAX_VALUE, NORANGE,
      PG_AM_GTHEME, 0, 0, 1, 0 },
    { SDI_AM_FONT, "amarchy-menu-font", 0, PG_PAGE_AMARCHY, SD_FONT, "Menu _font",
      "The Amarchy menu font; empty: the built-in choice. Click to choose.",
      NOVALS, "", SD_AMARCHY, SDF_AMSTR, AO(AM_FONT), UC_MAX_VALUE, NORANGE, PG_AM_FONT, 0, 0, 1, 0 },
};
const int sd_nsettings = SDI_COUNT;

/* The Amarchy keys no control of Prefs edits: they live in the file for the index (amarchy-index) and are
 * set by hand. They have no page, label or help; the rows above are all that Prefs shows. */
const sd_setting sd_hidden[SD_NHIDDEN] = {
    { -1, "amarchy-index-skip", 0, -1, SD_STRING, 0, 0, NOVALS, "", SD_AMARCHY, SDF_AMSTR, AO(AM_SKIP),
      UC_MAX_VALUE, NORANGE, 0, 0, 0, 1, 0 },
    { -1, "amarchy-index-volumes", 0, -1, SD_STRING, 0, 0, NOVALS, "", SD_AMARCHY, SDF_AMSTR, AO(AM_VOLUMES),
      UC_MAX_VALUE, NORANGE, 0, 0, 0, 1, 0 },
};

const sd_setting *sd_amarchy_key(int k)
{
    size_t off = AO(k);
    int i;
    for (i = 0; i < sd_nsettings; i++)
        if (sd_settings[i].store == SD_AMARCHY && sd_settings[i].off == off)
            return &sd_settings[i];
    for (i = 0; i < SD_NHIDDEN; i++)
        if (sd_hidden[i].off == off)
            return &sd_hidden[i];
    return 0;
}

static int sd_ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z')
            x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z')
            y = (char)(y - 'A' + 'a');
        if (x != y)
            return 0;
    }
    return *a == *b;
}

const sd_setting *sd_find_key(int store, const char *key)
{
    int i;
    for (i = 0; i < sd_nsettings; i++)
        if (sd_settings[i].store == store && sd_settings[i].key && !strcmp(sd_settings[i].key, key))
            return &sd_settings[i];
    return 0;
}

const sd_setting *sd_find_gadget(int id)
{
    int i, n;
    for (i = 0; i < sd_nsettings; i++) {
        const sd_setting *s = &sd_settings[i];
        for (n = 0; n < s->count; n++)
            if (id == s->gadget + n || (s->aux && id == s->aux + n) || (s->aux2 && id == s->aux2 + n))
                return s;
    }
    return 0;
}

int sd_value_index(const sd_setting *s, const char *file)
{
    int i;
    for (i = 0; i < s->nvalues; i++)
        if (sd_ieq(s->values[i].file, file))
            return i;
    return -1;
}

int sd_conf_index(const sd_setting *s, const upconf *c, const char *p)
{
    int x = sd_value_index(s, upconf_str(c, p, s->key, s->def));
    return x < 0 ? sd_def_index(s) : x;
}

int sd_conf_key_index(const char *key, const upconf *c, const char *p)
{
    const sd_setting *s = sd_find_key(SD_UPTERM, key);
    return s && s->nvalues ? sd_conf_index(s, c, p) : 0;
}

const int sd_write_order[] = {
    SDI_FALLBACK, SDI_SCREEN, SDI_SCREEN_MODE, SDI_SCROLLBACK, SDI_ASPECT, SDI_BACKSPACE, SDI_AMIGAKEYS,
    SDI_DITHER, SDI_OUTLINE_AA, SDI_PADDING, SDI_HLCAT, SDI_CLIPBOARD, SDI_LINK, SDI_TERM, SDI_COLORS,
    SDI_CURSOR, SDI_BLINK, SDI_BELL, SDI_BOLD, SDI_META, SDI_COPY, SDI_WHEEL, SDI_REFLOW, SDI_SCROLLBAR,
    SDI_COMPLETE, SDI_KC_MATCH, SDI_KC_INFO, SDI_KC_CACHE,
    /* the look */
    SDI_FONT, SDI_CURSOR_COLOUR, SDI_FG, SDI_BG, SDI_SELFG, SDI_SELBG, SDI_PALETTE
};
const int sd_nwrite = (int)(sizeof(sd_write_order) / sizeof(sd_write_order[0]));
const int sd_nstyle = 7;

int sd_def_index(const sd_setting *s)
{
    int i = sd_value_index(s, s->def);
    return i < 0 ? 0 : i;
}

int sd_index(const sd_setting *s, const void *store)
{
    const char *at = (const char *)store + s->off;
    prefs_kc k;
    int i;
    switch (s->ftype) {
    case SDF_INT:
        i = *(const int *)at;
        return i >= 0 && i < s->nvalues ? i : sd_def_index(s);
    case SDF_STR:
    case SDF_AMSTR:
        i = sd_value_index(s, at);
        return i < 0 ? sd_def_index(s) : i;
    case SDF_KC_MATCH:
        prefs_kc_decode(at, &k);
        return k.match;
    case SDF_KC_COMMON:
        prefs_kc_decode(at, &k);
        return k.common != 0;
    case SDF_KC_SILENT:
        prefs_kc_decode(at, &k);
        return k.silent != 0;
    }
    return 0;
}

void sd_set_index(const sd_setting *s, void *store, int index)
{
    char *at = (char *)store + s->off;
    prefs_kc k;
    if (index < 0 || index >= s->nvalues)
        return;
    switch (s->ftype) {
    case SDF_INT:
        *(int *)at = index;
        break;
    case SDF_STR:
    case SDF_AMSTR:
        strncpy(at, s->values[index].file, (size_t)s->size - 1);
        at[s->size - 1] = 0;
        break;
    case SDF_KC_MATCH:
    case SDF_KC_COMMON:
    case SDF_KC_SILENT:
        prefs_kc_decode(at, &k);
        if (s->ftype == SDF_KC_MATCH)
            k.match = index;
        else if (s->ftype == SDF_KC_COMMON)
            k.common = index;
        else
            k.silent = index;
        prefs_kc_encode(&k, at, s->size);
        break;
    }
}

char *sd_string(const sd_setting *s, void *store, int n)
{
    if (s->nvalues || (s->ftype != SDF_STR && s->ftype != SDF_AMSTR) || n < 0 || n >= s->count)
        return 0;
    return (char *)store + s->off + (size_t)n * (size_t)s->size;
}
