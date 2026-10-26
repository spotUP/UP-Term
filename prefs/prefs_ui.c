/* prefs_ui: the editor window as data and pure functions (see prefs_ui.h). */
#include <stddef.h>
#include <string.h>

#include "prefs_ui.h"
#include "prefs_core.h"

#define PAL_SWATCH_HELP "Click to pick this palette colour."
#define PAL_HEX_HELP "This palette colour as six hex digits, RRGGBB; empty: the built-in."

const pg_item pg_items[] = {
    { PG_PAGE, PG_COMMON, PGK_CYCLE, "Pa_ge",
      "Switch between the pages of settings.",
      PG_ROW_TOP, 0, 12, PGD_NONE },
    { PG_STATUS, PG_COMMON, PGK_TEXT, NULL,
      "What the last action did.",
      PG_ROW_STATUS, 0, 0, PGD_NONE },
    { PG_SAVE, PG_COMMON, PGK_BUTTON, "_Save",
      "Write the profile to ENVARC: and ENV:, kept across a reboot.",
      PG_ROW_BUTTONS, 0, 0, PGD_NONE },
    { PG_USE, PG_COMMON, PGK_BUTTON, "_Use",
      "Write the profile to ENV: only, until the next reboot.",
      PG_ROW_BUTTONS, 1, 0, PGD_NONE },
    { PG_CANCEL, PG_COMMON, PGK_BUTTON, "_Cancel",
      "Close without writing anything.",
      PG_ROW_BUTTONS, 2, 0, PGD_NONE },
    { PG_PROFILE, PG_PAGE_GENERAL, PGK_CYCLE, "_Profile",
      "The profile being edited; each profile is one set of settings.",
      0, 0, 12, PGD_NONE },
    { PG_PROFILE_NEW, PG_PAGE_GENERAL, PGK_BUTTON, "_New...",
      "Start a new profile with the built-in values.",
      0, 1, 0, PGD_NONE },
    { PG_PROFILE_RENAME, PG_PAGE_GENERAL, PGK_BUTTON, "R_ename...",
      "Give this profile another name.",
      0, 2, 0, PGD_NONE },
    { PG_PROFILE_DELETE, PG_PAGE_GENERAL, PGK_BUTTON, "_Delete",
      "Remove this profile from the file (asks first).",
      0, 3, 0, PGD_NONE },
    { PG_FONT, PG_PAGE_GENERAL, PGK_TEXT, "Fon_t",
      "The window font; empty: the screen's font. Click to choose.",
      1, 0, 14, PGD_NONE },
    { PG_FONT_CHOOSE, PG_PAGE_GENERAL, PGK_BUTTON, "Choose...",
      "Choose a fixed-width font in the font requester.",
      1, 1, 0, PGD_NONE },
    { PG_FONT_CLEAR, PG_PAGE_GENERAL, PGK_BUTTON, "Clear",
      "Use the screen's font again.",
      1, 2, 0, PGD_NONE },
    { PG_FALLBACK, PG_PAGE_GENERAL, PGK_TEXT, "_Fallback font",
      "Outline font for glyphs the font lacks. Click to choose.",
      2, 0, 14, PGD_NONE },
    { PG_FALLBACK_CHOOSE, PG_PAGE_GENERAL, PGK_BUTTON, "Choose...",
      "Choose an outline font (.otag) from FONTS:.",
      2, 1, 0, PGD_NONE },
    { PG_FALLBACK_CLEAR, PG_PAGE_GENERAL, PGK_BUTTON, "Clear",
      "Use no fallback font.",
      2, 2, 0, PGD_NONE },
    { PG_SCROLLBACK, PG_PAGE_GENERAL, PGK_INTEGER, "Scrollbac_k",
      "Lines kept above the window, 1 to 99999 (500 when not set).",
      3, 0, 7, PGD_SCROLLBACK_NONE },
    { PG_SCROLLBACK_NONE, PG_PAGE_GENERAL, PGK_CHECK, "N_o scrollback",
      "Keep no lines above the window.",
      3, 1, 0, PGD_NONE },
    { PG_CURSOR, PG_PAGE_GENERAL, PGK_CYCLE, "Curso_r",
      "The shape of the cursor.",
      4, 0, 10, PGD_NONE },
    { PG_BLINK, PG_PAGE_GENERAL, PGK_CHECK, "Cursor bl_inks",
      "The cursor blinks.",
      4, 1, 0, PGD_NONE },
    { PG_CURSOR_COLOUR, PG_PAGE_GENERAL, PGK_SWATCH, "Cursor co_lour",
      "The cursor's colour. Click to pick one.",
      5, 0, 0, PGD_CURSOR_INVERSE },
    { PG_CURSOR_COLOUR_HEX, PG_PAGE_GENERAL, PGK_STRING, NULL,
      "The cursor's colour as six hex digits, RRGGBB.",
      5, 1, 8, PGD_CURSOR_INVERSE },
    { PG_CURSOR_INVERSE, PG_PAGE_GENERAL, PGK_CHECK, "In_verse",
      "The cursor swaps the colours of the cell under it.",
      5, 2, 0, PGD_NONE },
    { PG_BELL, PG_PAGE_GENERAL, PGK_CYCLE, "_Bell",
      "What the bell character does.",
      6, 0, 10, PGD_NONE },
    { PG_BOLD, PG_PAGE_GENERAL, PGK_CHECK, "Bold is brig_ht",
      "Bold text uses the bright colours.",
      6, 1, 0, PGD_NONE },
    { PG_SCROLLBAR, PG_PAGE_GENERAL, PGK_CHECK, "Scroll b_ar",
      "Show the scroll bar in the window border.",
      7, 1, 0, PGD_NONE },
    { PG_FG, PG_PAGE_COLORS, PGK_SWATCH, "_Text colour",
      "The colour of text. Click to pick one.",
      0, 0, 0, PGD_NONE },
    { PG_FG_HEX, PG_PAGE_COLORS, PGK_STRING, NULL,
      "The text colour as six hex digits, RRGGBB.",
      0, 1, 8, PGD_NONE },
    { PG_BG, PG_PAGE_COLORS, PGK_SWATCH, "_Background",
      "The window's background colour. Click to pick one.",
      0, 2, 0, PGD_NONE },
    { PG_BG_HEX, PG_PAGE_COLORS, PGK_STRING, NULL,
      "The background colour as six hex digits, RRGGBB.",
      0, 3, 8, PGD_NONE },
    { PG_SELFG, PG_PAGE_COLORS, PGK_SWATCH, "Selected te_xt",
      "The colour of selected text; empty swaps the colours. Click to pick one.",
      1, 0, 0, PGD_NONE },
    { PG_SELFG_HEX, PG_PAGE_COLORS, PGK_STRING, NULL,
      "The selected text colour as six hex digits, RRGGBB.",
      1, 1, 8, PGD_NONE },
    { PG_SELBG, PG_PAGE_COLORS, PGK_SWATCH, "Se_lection",
      "The colour behind selected text; empty swaps the colours. Click to pick one.",
      1, 2, 0, PGD_NONE },
    { PG_SELBG_HEX, PG_PAGE_COLORS, PGK_STRING, NULL,
      "The selection colour as six hex digits, RRGGBB.",
      1, 3, 8, PGD_NONE },
    { PG_THEME, PG_PAGE_COLORS, PGK_BUTTON, "T_heme...",
      "Take the colours of a theme file.",
      2, 0, 0, PGD_NONE },
    { PG_DITHER, PG_PAGE_COLORS, PGK_CHECK, "_Dither on few colours",
      "On a screen of few colours, mix two colours for one that is missing.",
      3, 0, 0, PGD_NONE },
    { PG_OUTLINE_AA, PG_PAGE_COLORS, PGK_CHECK, "_Antialias outline glyphs",
      "Fallback font glyphs get soft edges in shades between text and background.",
      7, 0, 0, PGD_NONE },
    { PG_PADDING, PG_PAGE_COLORS, PGK_CHECK, "Padding _extends edges",
      "The strips beside the text take the colour of the edge cells.",
      4, 0, 0, PGD_NONE },
    { PG_TERM, PG_PAGE_COLORS, PGK_STRING, "Ter_minal type",
      "TERM for programs vsh starts; empty: vtcon. /term sets it for one window.",
      5, 0, 20, PGD_NONE },
    { PG_COLORS, PG_PAGE_COLORS, PGK_CYCLE, "Colour m_ode",
      "The colours programs are told they have. /colors sets it for one window.",
      6, 0, 19, PGD_NONE },
    { PG_META, PG_PAGE_KEYBOARD, PGK_CYCLE, "Meta ke_y",
      "Which key acts as Meta.",
      0, 0, 12, PGD_NONE },
    { PG_AMIGAKEYS, PG_PAGE_KEYBOARD, PGK_CHECK, "AmigaShell _keys",
      "Shift with Left and Right sends Home and End.",
      0, 1, 0, PGD_NONE },
    { PG_BACKSPACE, PG_PAGE_KEYBOARD, PGK_CYCLE, "_Backspace key sends",
      "What the Backspace key sends to programs.",
      1, 0, 10, PGD_NONE },
    { PG_COMPLETE, PG_PAGE_KEYBOARD, PGK_CYCLE, "_Tab completion",
      "How the Tab key completes names.",
      2, 0, 10, PGD_NONE },
    { PG_KC_MATCH, PG_PAGE_KEYBOARD, PGK_CYCLE, "Wh_en several names match",
      "What Tab does when several names match (KingCON completion).",
      3, 0, 19, PGD_NOT_KINGCON },
    { PG_KC_COMMON, PG_PAGE_KEYBOARD, PGK_CHECK, "Complete the co_mmon part first",
      "Tab first completes what all matching names share.",
      4, 0, 0, PGD_NOT_KINGCON },
    { PG_KC_SILENT, PG_PAGE_KEYBOARD, PGK_CHECK, "No bee_ps",
      "KingCON completion never beeps.",
      4, 1, 0, PGD_NOT_KINGCON },
    { PG_KC_INFO, PG_PAGE_KEYBOARD, PGK_CHECK, ".info in _lists",
      "KingCON lists show .info files too.",
      5, 0, 0, PGD_NOT_KINGCON },
    { PG_KC_CACHE, PG_PAGE_KEYBOARD, PGK_CHECK, "_Directory cache",
      "KingCON keeps directory listings for the next Tab.",
      5, 1, 0, PGD_NOT_KINGCON },
    { PG_COPY, PG_PAGE_KEYBOARD, PGK_CHECK, "C_opy on select",
      "Selecting text copies it to the clipboard.",
      6, 0, 0, PGD_NONE },
    { PG_WHEEL, PG_PAGE_KEYBOARD, PGK_CHECK, "_Wheel scrolls",
      "The mouse wheel scrolls the window.",
      6, 1, 0, PGD_NONE },
    { PG_SCREEN, PG_PAGE_ADVANCED, PGK_CYCLE, "Scree_n",
      "Open on the Workbench, on a screen of its own, or on the whole screen.",
      0, 0, 12, PGD_NONE },
    { PG_SCREEN_MODE, PG_PAGE_ADVANCED, PGK_TEXT, "Screen _mode",
      "The display mode and depth of an own screen. Click to choose.",
      1, 0, 28, PGD_NOT_OWN_SCREEN },
    { PG_SCREEN_CHOOSE, PG_PAGE_ADVANCED, PGK_BUTTON, "Choose...",
      "Choose the display mode and depth in the screen mode requester.",
      1, 1, 0, PGD_NOT_OWN_SCREEN },
    { PG_SCREEN_WORKBENCH, PG_PAGE_ADVANCED, PGK_BUTTON, "Use Workbench's",
      "Use the Workbench's display mode and depth.",
      1, 2, 0, PGD_NOT_OWN_SCREEN },
    { PG_ASPECT, PG_PAGE_ADVANCED, PGK_CHECK, "Font follows screen _aspect",
      "Swap the font for its pair that keeps its shape on this screen.",
      2, 0, 0, PGD_NONE },
    { PG_CLIPBOARD, PG_PAGE_ADVANCED, PGK_CYCLE, "_Programs may",
      "What programs may do with the clipboard (OSC 52).",
      3, 0, 26, PGD_NONE },
    { PG_HLCAT, PG_PAGE_ADVANCED, PGK_CHECK, "_Highlight files shown with cat",
      "vsh's cat colours the file through hl.",
      4, 0, 0, PGD_NONE },
    { PG_REFLOW, PG_PAGE_ADVANCED, PGK_CHECK, "_Reflow on resize",
      "Re-wrap the lines when the window changes size.",
      5, 0, 0, PGD_NONE },
    { PG_LINK, PG_PAGE_ADVANCED, PGK_STRING, "_Link command",
      "The command a Ctrl-clicked link runs; %s is the address.",
      6, 0, 32, PGD_NONE },
    { PG_LINK_DEFAULT, PG_PAGE_ADVANCED, PGK_BUTTON, "Default",
      "Use OpenURL for links.",
      6, 1, 0, PGD_NONE },
    { PG_AM_MUSIC, PG_PAGE_AMARCHY, PGK_STRING, "_Music drawers",
      "More drawers, comma separated, listed under Music in the Amarchy menu.",
      0, 0, 32, PGD_NONE },
    { PG_AM_DEMO, PG_PAGE_AMARCHY, PGK_STRING, "_Demo drawers",
      "More drawers, comma separated, listed under Demos in the Amarchy menu.",
      1, 0, 32, PGD_NONE },
    { PG_AM_HIDE, PG_PAGE_AMARCHY, PGK_STRING, "_Hidden drawers",
      "Drawers, comma separated, the Amarchy menu leaves out of its lists.",
      2, 0, 32, PGD_NONE },
    { PG_AM_ICONS, PG_PAGE_AMARCHY, PGK_CYCLE, "App _icons",
      "Generic: drawn glyphs. Amiga: .info icons of apps and games. None: no icons.",
      3, 0, 7, PGD_NONE },
    { PG_AM_MODE, PG_PAGE_AMARCHY, PGK_CYCLE, "Menu mod_e",
      "Drawn: icons, border. Text: terminal menu. Minimal: text only, two colours.",
      5, 0, 7, PGD_NONE },
    { PG_AM_TERMMENU, PG_PAGE_AMARCHY, PGK_CYCLE, "Menu in _terminal",
      "Menu over a UP-Term window: Text or Graphical. Full screen: always Text.",
      6, 0, 9, PGD_NONE },
    { PG_AM_FONT, PG_PAGE_AMARCHY, PGK_TEXT, "Menu _font",
      "The Amarchy menu font; empty: the built-in choice. Click to choose.",
      4, 0, 14, PGD_NONE },
    { PG_AM_FONT_CHOOSE, PG_PAGE_AMARCHY, PGK_BUTTON, "Choose...",
      "Choose a fixed-width font for the Amarchy menu in the font requester.",
      4, 1, 0, PGD_NONE },
    { PG_AM_FONT_CLEAR, PG_PAGE_AMARCHY, PGK_BUTTON, "Clear",
      "Use the built-in menu font again.",
      4, 2, 0, PGD_NONE },
    { PG_PAL + 0, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 0, 0, 0, PGD_NONE },
    { PG_PAL_HEX + 0, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 0, 1, 8, PGD_NONE },
    { PG_PAL + 1, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 1, 0, 0, PGD_NONE },
    { PG_PAL_HEX + 1, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 1, 1, 8, PGD_NONE },
    { PG_PAL + 2, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 2, 0, 0, PGD_NONE },
    { PG_PAL_HEX + 2, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 2, 1, 8, PGD_NONE },
    { PG_PAL + 3, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 3, 0, 0, PGD_NONE },
    { PG_PAL_HEX + 3, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 3, 1, 8, PGD_NONE },
    { PG_PAL + 4, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 4, 0, 0, PGD_NONE },
    { PG_PAL_HEX + 4, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 4, 1, 8, PGD_NONE },
    { PG_PAL + 5, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 5, 0, 0, PGD_NONE },
    { PG_PAL_HEX + 5, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 5, 1, 8, PGD_NONE },
    { PG_PAL + 6, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 6, 0, 0, PGD_NONE },
    { PG_PAL_HEX + 6, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 6, 1, 8, PGD_NONE },
    { PG_PAL + 7, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 7, 0, 0, PGD_NONE },
    { PG_PAL_HEX + 7, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 7, 1, 8, PGD_NONE },
    { PG_PAL + 8, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 0, 2, 0, PGD_NONE },
    { PG_PAL_HEX + 8, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 0, 3, 8, PGD_NONE },
    { PG_PAL + 9, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 1, 2, 0, PGD_NONE },
    { PG_PAL_HEX + 9, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 1, 3, 8, PGD_NONE },
    { PG_PAL + 10, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 2, 2, 0, PGD_NONE },
    { PG_PAL_HEX + 10, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 2, 3, 8, PGD_NONE },
    { PG_PAL + 11, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 3, 2, 0, PGD_NONE },
    { PG_PAL_HEX + 11, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 3, 3, 8, PGD_NONE },
    { PG_PAL + 12, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 4, 2, 0, PGD_NONE },
    { PG_PAL_HEX + 12, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 4, 3, 8, PGD_NONE },
    { PG_PAL + 13, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 5, 2, 0, PGD_NONE },
    { PG_PAL_HEX + 13, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 5, 3, 8, PGD_NONE },
    { PG_PAL + 14, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 6, 2, 0, PGD_NONE },
    { PG_PAL_HEX + 14, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 6, 3, 8, PGD_NONE },
    { PG_PAL + 15, PG_PAGE_PALETTE, PGK_SWATCH, NULL, PAL_SWATCH_HELP, 7, 2, 0, PGD_NONE },
    { PG_PAL_HEX + 15, PG_PAGE_PALETTE, PGK_STRING, NULL, PAL_HEX_HELP, 7, 3, 8, PGD_NONE },
};
const int pg_nitems = (int)(sizeof(pg_items) / sizeof(pg_items[0]));

const pg_item *pg_find(int id)
{
    int i;
    for (i = 0; i < pg_nitems; i++)
        if (pg_items[i].id == id)
            return &pg_items[i];
    return NULL;
}

const char *pg_label(const pg_item *it)
{
    if (it->id >= PG_PAL && it->id < PG_PAL + 16)
        return prefs_pal_name(it->id - PG_PAL);
    return it->label;
}

int pg_shortcut(const char *label)
{
    if (!label)
        return 0;
    for (; *label; label++)
        if (*label == '_' && label[1]) {
            int c = (unsigned char)label[1];
            return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
        }
    return 0;
}

void pg_plain_label(const char *label, char *out, int cap)
{
    int n = 0;
    if (cap < 1)
        return;
    for (; label && *label && n < cap - 1; label++)
        if (*label != '_')
            out[n++] = *label;
    out[n] = 0;
}

const pg_item *pg_for_key(int page, int key)
{
    int i;
    if (key >= 'A' && key <= 'Z')
        key = key - 'A' + 'a';
    for (i = 0; i < pg_nitems; i++)
        if ((pg_items[i].page == page || pg_items[i].page == PG_COMMON) &&
            pg_shortcut(pg_label(&pg_items[i])) == key)
            return &pg_items[i];
    return NULL;
}

/* ---- layout -------------------------------------------------------------------- */

#define GAP_LABEL 6      /* between a label and its control */
#define GAP_SLOT 8       /* between two slots of a row */
#define CHECK_W 26
#define STRING_PAD 8     /* a string gadget's bevel and caret room */
#define CYCLE_PAD 26     /* a cycle gadget's arrow */
#define BUTTON_PAD 16    /* a button's room around its text */

/* topaz 8, the fallback: 8 x 8, fixed width */
static int topaz_w(void *ctx, const char *s)
{
    (void)ctx;
    return 8 * (int)strlen(s);
}

static int plain_w(const pl_font *f, const char *label)
{
    char buf[80];
    if (!label)
        return 0;
    pg_plain_label(label, buf, sizeof(buf));
    return f->text_w(f->ctx, buf);
}

/* the label left of the control (strings, cycles, swatches, text boxes) */
static int left_label_w(const pl_font *f, const pg_item *it)
{
    if (!pg_label(it) || it->kind == PGK_BUTTON || it->kind == PGK_CHECK)
        return 0;
    return plain_w(f, pg_label(it)) + GAP_LABEL;
}

/* the label right of a check box */
static int right_label_w(const pl_font *f, const pg_item *it)
{
    if (it->kind != PGK_CHECK || !it->label)
        return 0;
    return GAP_LABEL + plain_w(f, it->label);
}

static int ctl_w(const pl_font *f, const pg_item *it)
{
    switch (it->kind) {
    case PGK_STRING:
    case PGK_INTEGER:
    case PGK_TEXT:
        return it->chars * f->w + STRING_PAD;
    case PGK_CYCLE:
        return it->chars * f->w + CYCLE_PAD;
    case PGK_BUTTON:
        return plain_w(f, it->label) + BUTTON_PAD;
    case PGK_CHECK:
        return CHECK_W;
    default:
        return 6 * f->w; /* a swatch */
    }
}

static int is_common_row(int row)
{
    return row >= PG_ROW_STATUS;
}

static void layout_with(const pl_font *f, pl_result *out)
{
    int page, c, i, maxrows = 0, pagew = 0, h, pitch, y0;
    int status_y, buttons_y, bw[3];
    memset(out, 0, sizeof(*out));
    h = f->h + 4;
    pitch = h + 2;
    out->font_h = f->h;
    out->font_w = f->w;
    out->pitch = pitch;
    for (i = 0; i < pg_nitems; i++)
        if (!is_common_row(pg_items[i].row) && pg_items[i].row + 1 > maxrows)
            maxrows = pg_items[i].row + 1;
    y0 = pitch + 4; /* below the page switch */
    for (page = 0; page < PG_NPAGES; page++) {
        int lw[PG_COLS], sw[PG_COLS], x[PG_COLS], total = 0;
        for (c = 0; c < PG_COLS; c++)
            lw[c] = sw[c] = 0;
        for (i = 0; i < pg_nitems; i++) {
            const pg_item *it = &pg_items[i];
            if (it->page != page)
                continue;
            if (left_label_w(f, it) > lw[it->col])
                lw[it->col] = left_label_w(f, it);
        }
        for (i = 0; i < pg_nitems; i++) {
            const pg_item *it = &pg_items[i];
            int w;
            if (it->page != page)
                continue;
            w = (it->kind == PGK_CHECK ? 0 : lw[it->col]) + ctl_w(f, it) + right_label_w(f, it);
            if (w > sw[it->col])
                sw[it->col] = w;
        }
        for (c = 0; c < PG_COLS; c++) {
            x[c] = total;
            total += sw[c];
            if (sw[c])
                total += GAP_SLOT;
        }
        if (total)
            total -= GAP_SLOT;
        if (total > pagew)
            pagew = total;
        for (i = 0; i < pg_nitems; i++) {
            const pg_item *it = &pg_items[i];
            pl_box *b;
            if (it->page != page)
                continue;
            b = &out->box[it->id];
            b->x = x[it->col] + (it->kind == PGK_CHECK ? 0 : lw[it->col]);
            b->y = y0 + it->row * pitch;
            b->w = ctl_w(f, it);
            b->h = it->kind == PGK_CHECK ? (f->h + 3 > 11 ? f->h + 3 : 11) : h;
            out->label_w[it->id] = left_label_w(f, it);
        }
    }
    /* the common rows */
    status_y = y0 + maxrows * pitch + 4;
    buttons_y = status_y + pitch + 2;
    for (c = 0; c < 3; c++)
        bw[c] = 0;
    for (i = 0; i < pg_nitems; i++) {
        const pg_item *it = &pg_items[i];
        if (it->page != PG_COMMON)
            continue;
        if (it->row == PG_ROW_BUTTONS && ctl_w(f, it) > bw[it->col])
            bw[it->col] = ctl_w(f, it);
    }
    for (c = 1; c < 3; c++)
        if (bw[c] > bw[0])
            bw[0] = bw[c];
    if (bw[0] < 80)
        bw[0] = 80;
    {
        const pg_item *pgi = pg_find(PG_PAGE);
        int lab = left_label_w(f, pgi);
        out->box[PG_PAGE].x = lab;
        out->box[PG_PAGE].y = 0;
        out->box[PG_PAGE].w = ctl_w(f, pgi);
        out->box[PG_PAGE].h = h;
        out->label_w[PG_PAGE] = lab;
        if (lab + ctl_w(f, pgi) > pagew)
            pagew = lab + ctl_w(f, pgi);
    }
    if (3 * bw[0] + 2 * GAP_SLOT > pagew)
        pagew = 3 * bw[0] + 2 * GAP_SLOT;
    out->inner_w = pagew;
    out->box[PG_STATUS].x = 0;
    out->box[PG_STATUS].y = status_y;
    out->box[PG_STATUS].w = pagew;
    out->box[PG_STATUS].h = h;
    for (c = 0; c < 3; c++) {
        static const int ids[3] = { PG_SAVE, PG_USE, PG_CANCEL };
        pl_box *b = &out->box[ids[c]];
        b->w = bw[0];
        b->h = h;
        b->y = buttons_y;
        b->x = c == 0 ? 0 : c == 1 ? (pagew - bw[0]) / 2 : pagew - bw[0];
    }
    out->inner_h = buttons_y + h;
}

void pl_layout(const pl_font *screen_font, int avail_w, int avail_h, pl_result *out)
{
    pl_font topaz;
    layout_with(screen_font, out);
    out->fits = out->inner_w <= avail_w && out->inner_h <= avail_h;
    if (out->fits)
        return;
    topaz.w = 8;
    topaz.h = 8;
    topaz.text_w = topaz_w;
    topaz.ctx = NULL;
    layout_with(&topaz, out);
    out->fallback = 1;
    out->fits = out->inner_w <= avail_w && out->inner_h <= avail_h;
}

int pl_hit(const pl_result *r, int page, int x, int y)
{
    int i;
    for (i = 0; i < pg_nitems; i++) {
        const pg_item *it = &pg_items[i];
        const pl_box *b;
        if (it->page != page && it->page != PG_COMMON)
            continue;
        b = &r->box[it->id];
        if (x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h)
            return it->id;
    }
    return 0;
}

int pg_page_for_args(const char *args)
{
    static const char kw[] = "AMARCHY";
    int i = 0;
    if (!args)
        return PG_PAGE_GENERAL;
    while (args[i] == ' ' || args[i] == '\t')
        i++;
    {
        int k = 0;
        while (kw[k] && args[i + k] && (args[i + k] & ~0x20) == kw[k])
            k++;
        if (!kw[k]) {
            int e = i + k;
            while (args[e] == ' ' || args[e] == '\t' || args[e] == '\n' || args[e] == '\r')
                e++;
            if (!args[e])
                return PG_PAGE_AMARCHY;
        }
    }
    return PG_PAGE_GENERAL;
}
