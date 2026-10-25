/* prefs_ui: what UP-Term Prefs' window is made of, as data and pure
 * functions with no OS calls, so the host tests look at it (tests/test_prefs.c):
 *
 *   - the gadget table: every gadget of the editor window with its stable id,
 *     page, kind, label (the `_` marks the keyboard shortcut), one-line help
 *     and place in the page's grid;
 *   - the layout: the grid to pixel boxes from the font's height and label
 *     widths, with topaz 8 when the screen's font does not fit the screen;
 *   - which gadget lies under a point (gadget help), which one a key reaches.
 *
 * prefs/upprefs.c builds GadTools gadgets from the table and holds the
 * window; the rig reads the ids from this header (tools/rig/prefs_rig.py).
 */
#ifndef PREFS_UI_H
#define PREFS_UI_H

/* ---- gadget ids: stable, the rig names them ------------------------------ */
enum {
    /* in every page */
    PG_PAGE = 1, PG_SAVE = 2, PG_USE = 3, PG_CANCEL = 4, PG_STATUS = 5,
    /* General */
    PG_PROFILE = 10, PG_PROFILE_NEW = 11, PG_PROFILE_RENAME = 12, PG_PROFILE_DELETE = 13,
    PG_FONT = 14, PG_FONT_CHOOSE = 15, PG_FONT_CLEAR = 16,
    PG_FALLBACK = 17, PG_FALLBACK_CHOOSE = 18, PG_FALLBACK_CLEAR = 19,
    PG_SCROLLBACK = 20, PG_SCROLLBACK_NONE = 21,
    PG_CURSOR = 22, PG_BLINK = 23,
    PG_CURSOR_COLOUR = 24, PG_CURSOR_COLOUR_HEX = 25, PG_CURSOR_INVERSE = 26,
    PG_BELL = 27, PG_BOLD = 28, PG_WHEEL = 29, PG_COPY = 30, PG_SCROLLBAR = 31, PG_REFLOW = 32,
    /* Colors */
    PG_FG = 40, PG_FG_HEX = 41, PG_BG = 42, PG_BG_HEX = 43,
    PG_SELFG = 44, PG_SELFG_HEX = 45, PG_SELBG = 46, PG_SELBG_HEX = 47,
    PG_THEME = 48, PG_DITHER = 49, PG_PADDING = 50, PG_OUTLINE_AA = 51,
    /* Palette: PG_PAL + i the swatch of entry i, PG_PAL_HEX + i its hex field */
    PG_PAL = 60, PG_PAL_HEX = 80,
    /* Keyboard */
    PG_META = 100, PG_AMIGAKEYS = 101, PG_BACKSPACE = 102, PG_COMPLETE = 103,
    PG_KC_MATCH = 104, PG_KC_COMMON = 105, PG_KC_SILENT = 106, PG_KC_INFO = 107, PG_KC_CACHE = 108,
    /* Advanced */
    PG_SCREEN = 120, PG_SCREEN_MODE = 121, PG_SCREEN_CHOOSE = 122, PG_SCREEN_WORKBENCH = 123,
    PG_ASPECT = 124, PG_CLIPBOARD = 125, PG_HLCAT = 126, PG_LINK = 127, PG_LINK_DEFAULT = 128,
    PG_TERM = 129, PG_COLORS = 130,
    /* Amarchy */
    PG_AM_MUSIC = 140, PG_AM_DEMO = 141, PG_AM_HIDE = 142,
    PG_AM_FONT = 143, PG_AM_FONT_CHOOSE = 144, PG_AM_FONT_CLEAR = 145,
    /* the colour window (PX12) and the name window (PX11) */
    PC_WHEEL = 150, PC_BRIGHTNESS = 151, PC_RED = 152, PC_GREEN = 153, PC_BLUE = 154,
    PC_HEX = 155, PC_SWATCH = 156, PC_OK = 157, PC_CANCEL = 158,
    PN_NAME = 170, PN_OK = 171, PN_CANCEL = 172
};

enum { PG_PAGE_GENERAL, PG_PAGE_COLORS, PG_PAGE_PALETTE, PG_PAGE_KEYBOARD, PG_PAGE_ADVANCED,
       PG_PAGE_AMARCHY, PG_NPAGES };
#define PG_COMMON (-1)   /* in every page */

/* the controls; a swatch is a gadget the program draws */
enum { PGK_STRING, PGK_INTEGER, PGK_BUTTON, PGK_CYCLE, PGK_CHECK, PGK_TEXT, PGK_SWATCH };

/* when a gadget is greyed out */
enum {
    PGD_NONE,
    PGD_NOT_KINGCON,       /* Tab completion is not KingCON */
    PGD_NOT_OWN_SCREEN,    /* Screen is Workbench */
    PGD_SCROLLBACK_NONE,   /* "No scrollback" is on */
    PGD_CURSOR_INVERSE     /* the cursor colour is Inverse */
};

typedef struct pg_item {
    int id;
    int page;          /* PG_PAGE_* or PG_COMMON */
    int kind;          /* PGK_* */
    const char *label; /* the text; "_" before the shortcut letter; 0: none */
    const char *help;  /* one line for the status row */
    int row, col;      /* the grid: row 0.., slot 0..PG_COLS-1 */
    int chars;         /* the control's width in characters (string, text, cycle, integer) */
    int disabled;      /* PGD_* */
} pg_item;

#define PG_COLS 4
#define PG_ROW_STATUS 100   /* the common rows below the page */
#define PG_ROW_BUTTONS 101
#define PG_ROW_TOP 102      /* the page switch above it */

extern const pg_item pg_items[];
extern const int pg_nitems;
const pg_item *pg_find(int id);
/* the label the gadget shows: the table's, or a palette entry's ANSI name */
const char *pg_label(const pg_item *it);

/* "_Font" -> 'f' (lower case), 0 when the label has no shortcut */
int pg_shortcut(const char *label);
/* the label without its "_" ("_Font" -> "Font") into out (cap bytes) */
void pg_plain_label(const char *label, char *out, int cap);
/* The gadget a key reaches on `page`: the page's own or a common one with
 * that shortcut (any case); 0 when none. */
const pg_item *pg_for_key(int page, int key);
/* The page the command line asks for: the keyword AMARCHY (any case, blanks around) opens the Amarchy page;
 * anything else, and no arguments, the first page. */
int pg_page_for_args(const char *args);

/* ---- layout ------------------------------------------------------------------ */
typedef struct pl_box { int x, y, w, h; } pl_box;

/* The font the layout measures with: height of a line, width of a digit
 * (the unit of `chars`) and a function giving the pixel width of a label. */
typedef struct pl_font {
    int w, h;
    int (*text_w)(void *ctx, const char *s);
    void *ctx;
} pl_font;

#define PL_MAX_IDS 160
typedef struct pl_result {
    pl_box box[PL_MAX_IDS];  /* by gadget id (the control; the label lies left of it) */
    int label_w[PL_MAX_IDS]; /* the label's width reserved left of the control */
    int inner_w, inner_h;    /* the window's inner size */
    int fallback;            /* 1: topaz 8 was used, the screen font did not fit */
    int fits;                /* the result fits avail_w x avail_h */
    int font_h, font_w;      /* the font the boxes were made for */
    int pitch;               /* the row pitch */
} pl_result;

/* Lay the whole window out. avail_w x avail_h is the room inside the
 * window's borders. With `screen_font` the boxes are for it; when they do
 * not fit, again for topaz 8 (8 x 8, fixed width: pl_result.fallback). */
void pl_layout(const pl_font *screen_font, int avail_w, int avail_h, pl_result *out);

/* The gadget id of the page's (and the common) gadgets under (x, y), inner
 * coordinates; 0 when none. */
int pl_hit(const pl_result *r, int page, int x, int y);

#endif
