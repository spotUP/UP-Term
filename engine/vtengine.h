/* vtengine -- a terminal engine with three personalities.
 *
 * Bytes go in through vt_write(); the engine keeps a grid of Unicode cells
 * and tells the renderer what changed through callbacks (damage, scroll).
 * Replies the host must send back (DSR, DA, the Amiga window status report)
 * go out through the reply callback. Keys are encoded with vt_encode_key(),
 * because what a cursor key sends depends on the personality and on modes
 * the host set (DECCKM).
 *
 * Portable C89 without OS calls: it runs in the host test suites and on a
 * plain 68000 inside DCTelnet. Memory comes from VT_MALLOC / VT_FREE, which
 * an Amiga build points at AllocVec / FreeVec.
 */
#ifndef VTENGINE_H
#define VTENGINE_H

typedef unsigned char  vt_u8;
typedef unsigned short vt_u16;
typedef unsigned long  vt_u32;

enum vt_personality {
    VT_XTERM = 0,   /* what Unix ports expect; UTF-8 */
    VT_AMIGA = 1,   /* the ROM console.device dialect; Latin-1 */
    VT_PCANSI = 2   /* ANSI.SYS / BBS art; CP437 */
};

/* Colours in a cell. 0-255 index a palette: ANSI colours for xterm and
 * pcansi, screen pens for amiga. VT_COLOR_DEFAULT is the personality's
 * default; VT_COLOR_RGB | 0xRRGGBB is a direct colour (SGR 38;2), kept at
 * full 24 bits so a true-colour screen can show it exactly. */
typedef vt_u32 vt_color;
#define VT_COLOR_DEFAULT 0x0100
/* vt_resolve_colors only: the default background, so that it stays itself
 * when inverse swaps it into the foreground (the default foreground is
 * VT_COLOR_DEFAULT). */
#define VT_COLOR_DEFAULT_BG 0x0101
#define VT_COLOR_RGB     0x01000000UL
#define VT_RGB(r, g, b) (vt_color)(VT_COLOR_RGB | ((vt_u32)((r) & 0xFF) << 16) | \
                                    ((vt_u32)((g) & 0xFF) << 8) | (vt_u32)((b) & 0xFF))
#define VT_RGB_OF(c) ((vt_u32)(c) & 0xFFFFFFUL) /* 0xRRGGBB of a VT_COLOR_RGB colour */

#define VT_ATTR_BOLD      0x01
#define VT_ATTR_FAINT     0x02
#define VT_ATTR_ITALIC    0x04
#define VT_ATTR_UNDERLINE 0x08
#define VT_ATTR_BLINK     0x10
#define VT_ATTR_INVERSE   0x20
#define VT_ATTR_CONCEAL   0x40
#define VT_ATTR_STRIKE    0x80
#define VT_ATTR_OVERLINE  0x0100  /* SGR 53 */
#define VT_ATTR_RAPID     0x0200  /* SGR 6: with BLINK, the fast rate */
#define VT_ATTR_SUPER     0x0400  /* SGR 73 */
#define VT_ATTR_SUB       0x0800  /* SGR 74 */
#define VT_ATTR_FRAMED    0x1000  /* SGR 51 */
#define VT_ATTR_ENCIRCLED 0x2000  /* SGR 52 */
typedef vt_u16 vt_attr;

/* vt_cell.deco: the underline's style (with VT_ATTR_UNDERLINE set) and an
 * ideogram line (SGR 60-64), which ECMA-48 draws beside or over the text. */
#define VT_DECO_UL_MASK   0x07
#define VT_UL_SINGLE      1       /* SGR 4, 4:1 */
#define VT_UL_DOUBLE      2       /* SGR 21, 4:2 */
#define VT_UL_CURLY       3       /* 4:3 */
#define VT_UL_DOTTED      4       /* 4:4 */
#define VT_UL_DASHED      5       /* 4:5 */
#define VT_DECO_IDEO_SHIFT 3
#define VT_DECO_IDEO_MASK 0x38
#define VT_IDEO_UNDERLINE        1 /* SGR 60 */
#define VT_IDEO_DOUBLE_UNDERLINE 2 /* SGR 61 */
#define VT_IDEO_OVERLINE         3 /* SGR 62 */
#define VT_IDEO_DOUBLE_OVERLINE  4 /* SGR 63 */
#define VT_IDEO_STRESS           5 /* SGR 64 */

typedef struct vt_cell {
    vt_color fg, bg; /* see VT_COLOR_* */
    vt_u16 ch;      /* Unicode code point (BMP); 0x20 for blank */
    vt_attr attr;   /* VT_ATTR_* */
    vt_u8  width;   /* 1; 2 for the first cell of a wide glyph, 0 for its second */
    vt_u8  deco;    /* VT_DECO_*: underline style, ideogram line */
    vt_u8  ext;     /* 0, or 1 + an entry of the terminal's rare styles:
                     * underline colour and font (vt_cell_underline_color,
                     * vt_cell_font) */
    vt_u8  pad;
} vt_cell;

/* Window-level requests the engine does not own itself (amiga personality). */
enum vt_layout {
    VT_LAYOUT_PAGE_LENGTH = 1, /* CSI n t: text lines */
    VT_LAYOUT_LINE_LENGTH,     /* CSI n u: columns */
    VT_LAYOUT_LEFT_OFFSET,     /* CSI n x: pixels */
    VT_LAYOUT_TOP_OFFSET,      /* CSI n y: pixels */
    VT_LAYOUT_COLUMNS          /* DECCOLM (?3, allowed by ?40): 80 or 132 columns */
};

typedef struct vt_callbacks {
    /* Cells x0 <= x < x1, y0 <= y < y1 of the visible grid changed. */
    void (*damage)(void *user, int x0, int y0, int x1, int y1);
    /* Rows top <= y < bottom moved up by n rows (down when n < 0), all
     * columns. The renderer fills the rows it vacates with the DEFAULT
     * background (as ScrollRaster does with the background pen); the engine
     * damages them afterwards only when they hold something else (a BCE
     * erase colour). All scrolls of one vt_write arrive as one call. When
     * this is NULL the engine damages the whole region instead. */
    void (*scroll)(void *user, int top, int bottom, int n);
    void (*reply)(void *user, const vt_u8 *buf, long len);
    void (*bell)(void *user);
    void (*title)(void *user, const char *utf8);
    /* A setting that changes the window's layout (see vt_layout); value is
     * the parameter, -1 when the sequence reset it to the default. */
    void (*layout)(void *user, int which, int value);
    /* A program changed the palette (OSC 4 / 104) or the default colours
     * (OSC 10-12 / 110-112): the renderer's pens are out of date. */
    void (*colors)(void *user);
} vt_callbacks;

/* vt_modes() bits the host needs for input. */
#define VT_MODE_APP_CURSOR   0x0001 /* DECCKM */
#define VT_MODE_APP_KEYPAD   0x0002 /* DECKPAM */
#define VT_MODE_BRACKET_PASTE 0x0004
#define VT_MODE_MOUSE_X10    0x0008 /* ?9 */
#define VT_MODE_MOUSE_NORMAL 0x0010 /* ?1000 */
#define VT_MODE_MOUSE_BUTTON 0x0020 /* ?1002 */
#define VT_MODE_MOUSE_ANY    0x0040 /* ?1003 */
#define VT_MODE_MOUSE_SGR    0x0080 /* ?1006 */
#define VT_MODE_ALT_SCREEN   0x0100
#define VT_MODE_CURSOR_VISIBLE 0x0200
#define VT_MODE_NEWLINE      0x0400 /* LNM: LF also returns; Return sends CR LF */
#define VT_MODE_FOCUS        0x0800 /* ?1004 */
#define VT_MODE_SCREEN_REVERSE 0x1000 /* DECSCNM ?5: the whole screen in reverse video */
#define VT_MODE_CURSOR_BLINK 0x2000   /* ?12 (or a blinking DECSCUSR shape) */
#define VT_MODE_AUTOREPEAT   0x4000   /* ?8 DECARM: held keys repeat (default on) */
#define VT_MODE_REVERSE_WRAP 0x8000   /* ?45: BS at the left edge goes up a line */
#define VT_MODE_MOUSE_UTF8   0x10000  /* ?1005: mouse coordinates as UTF-8 */
#define VT_MODE_META_8BIT    0x20000  /* ?1034: Meta sets the 8th bit, no ESC prefix */
#define VT_MODE_SCHEME_UPDATES 0x40000 /* ?2031: report dark/light changes */
#define VT_MODE_APP_ESCAPE   0x80000  /* ?7727: the Escape key sends ESC O [ */

typedef struct vt_term vt_term;

vt_term *vt_new(int cols, int rows, int scrollback, const vt_callbacks *cb, void *user);
void     vt_free(vt_term *t);
void     vt_set_personality(vt_term *t, enum vt_personality p); /* also resets */
enum vt_personality vt_personality(const vt_term *t);
/* How the xterm personality reads bytes (and encodes typed characters):
 * UTF-8 (the default); Latin-1 with 8-bit C1 controls, as Amiga Unix ports
 * (ixemul/libnix) write; or CP437 for programs drawing for an IBM font
 * (BitchX's logo and prefixes). A lone $9B is the 8-bit CSI in all three. */
enum vt_charset { VT_CS_UTF8 = 0, VT_CS_LATIN1 = 1, VT_CS_CP437 = 2 };
void     vt_set_charset(vt_term *t, enum vt_charset cs);
/* ONLCR: a received LF also returns the carriage, as a Unix tty's output
 * does by default (the host's AmigaDOS programs end lines with a bare LF).
 * Unlike LNM (CSI 20 h) it does not change what Return sends. */
void     vt_set_onlcr(vt_term *t, int on);
/* Reflow on resize, as the ROM console's character-mapped units do: when
 * the width changes, vt_resize joins the rows a wrap linked into logical
 * lines and wraps them again at the new width, the cursor staying on its
 * character. Off by default (rows are cut or padded, as xterm does); a
 * setting of the host, so vt_reset leaves it. Scrollback lines and the
 * alternate screen are not reflowed. */
void     vt_set_reflow(vt_term *t, int on);
void     vt_reset(vt_term *t);  /* RIS */
void     vt_write(vt_term *t, const vt_u8 *buf, long len);
/* Frame-paced output: vt_feed changes the grid without telling the
 * renderer; vt_flush then sends everything changed since, as one batch
 * (all scrolls in between become one scroll call). vt_write = both. */
void     vt_feed(vt_term *t, const vt_u8 *buf, long len);
void     vt_flush(vt_term *t);
void     vt_resize(vt_term *t, int cols, int rows);

int      vt_cols(const vt_term *t);
int      vt_rows(const vt_term *t);
/* row >= 0: the visible grid; row < 0: scrollback, -1 the newest line.
 * NULL outside. *ncells gets how many cells the row holds: vt_cols() for the
 * grid, the width at the time for a scrollback line. */
const vt_cell *vt_row(const vt_term *t, int row, int *ncells);
int      vt_row_wrapped(const vt_term *t, int row); /* continues on the next row */
/* DEC line size of grid row `row` (ESC # 3/4/5/6): the row shows its first
 * half of the columns at twice the width, and for the height halves the
 * top or bottom half of glyphs twice as tall. */
#define VT_LINE_DOUBLE_WIDTH  1
#define VT_LINE_DOUBLE_TOP    2
#define VT_LINE_DOUBLE_BOTTOM 3
int      vt_row_size(const vt_term *t, int row);
int      vt_scrollback_lines(const vt_term *t);
/* Lines that have scrolled off the top of the primary screen since vt_new:
 * grid row y + vt_lines_scrolled() names a line for good (a selection
 * stays on its text while output scrolls). */
long     vt_lines_scrolled(const vt_term *t);
void     vt_cursor(const vt_term *t, int *x, int *y);
vt_u32   vt_modes(const vt_term *t);
const char *vt_title(const vt_term *t);
/* Amiga raw input event classes the host asked for (CSI n {), bit n. */
vt_u32   vt_raw_events(const vt_term *t);

/* The text of a selection from (ax, ay) to (bx, by) inclusive, in reading
 * order (rows below 0 are scrollback, as for vt_row). Trailing blanks of a
 * line are dropped and lines end with '\n', except a line that wrapped into
 * the next: selecting a wrapped paragraph gives it back as one line.
 * Written as UTF-8, NUL-terminated; returns the length (at most max - 1). */
long     vt_copy_text(const vt_term *t, int ax, int ay, int bx, int by, char *out, long max);

/* How many sequences were parsed but not acted on since vt_new, and the
 * distinct kinds (up to 16 are kept) with their counts, as text: "C ?12h"
 * a CSI, "E x" an ESC, "M 1005" a DEC mode, "S 58" an SGR value, "O 11" an
 * OSC, "D 0" / "X 0" a DCS / other string. Unused slots are NULL. */
long     vt_unhandled(const vt_term *t, const char **kinds, long *counts, int max);

/* The palette indices a cell draws with, for this personality: default
 * colours, bold-as-bright (pcansi, and xterm for colours 0-7), iCE blink,
 * inverse (the cell's, XOR the screen's DECSCNM) and conceal all resolved.
 * RGB colours pass through unchanged. */
void     vt_resolve_colors(const vt_term *t, const vt_cell *c, vt_color *fg, vt_color *bg);
/* The colour of the cell's underline (SGR 58), VT_COLOR_DEFAULT when it
 * follows the text; and its font, 0 primary, 1-9 SGR 11-19, 10 Fraktur (20). */
vt_color vt_cell_underline_color(const vt_term *t, const vt_cell *c);
int      vt_cell_font(const vt_term *t, const vt_cell *c);

/* Colours as programs query and set them (OSC 4, 10-12): 0xRRGGBB of
 * palette entry i (xterm's 16 + 6x6x6 cube + 24 greys, or what OSC 4 set),
 * and of the default text (0), background (1) and cursor (2) colours. The
 * host tells the engine the defaults it draws with; OSC 10-12 override. */
vt_u32   vt_palette_rgb(const vt_term *t, int i);
/* The xterm 256-colour index (16..255: the 6x6x6 cube or the grey ramp)
 * nearest to 0xRRGGBB, for screens that cannot show every colour. */
int      vt_rgb_to_256(vt_u32 rgb);
void     vt_set_default_colors(vt_term *t, vt_u32 fg, vt_u32 bg, vt_u32 cursor);
vt_u32   vt_default_color(const vt_term *t, int which);
/* The cell size in pixels, for the size reports programs ask for (CSI 14t,
 * 16t). */
void     vt_set_cell_pixels(vt_term *t, int w, int h);
/* DECSCUSR: 0/1 blinking block, 2 block, 3 blinking underline, 4 underline,
 * 5 blinking bar, 6 bar. */
int      vt_cursor_style(const vt_term *t);

/* The CP437 code points of bytes 0x80-0xFF (pcansi decodes with it). */
const vt_u16 *vt_cp437_table(void);

/* Keys. key is a Unicode character, or one of VT_KEY_*. */
enum vt_key {
    VT_KEY_UP = 0x110000, VT_KEY_DOWN, VT_KEY_RIGHT, VT_KEY_LEFT,
    VT_KEY_HOME, VT_KEY_END, VT_KEY_INSERT, VT_KEY_DELETE,
    VT_KEY_PAGE_UP, VT_KEY_PAGE_DOWN, VT_KEY_HELP,
    VT_KEY_F1, VT_KEY_F2, VT_KEY_F3, VT_KEY_F4, VT_KEY_F5, VT_KEY_F6,
    VT_KEY_F7, VT_KEY_F8, VT_KEY_F9, VT_KEY_F10, VT_KEY_F11, VT_KEY_F12,
    VT_KEY_RETURN, VT_KEY_BACKSPACE, VT_KEY_TAB, VT_KEY_ESCAPE,
    VT_KEY_KP_ENTER,
    /* numeric keypad: characters normally, SS3 codes in DECKPAM (xterm) */
    VT_KEY_KP_0, VT_KEY_KP_1, VT_KEY_KP_2, VT_KEY_KP_3, VT_KEY_KP_4,
    VT_KEY_KP_5, VT_KEY_KP_6, VT_KEY_KP_7, VT_KEY_KP_8, VT_KEY_KP_9,
    VT_KEY_KP_DOT, VT_KEY_KP_MINUS, VT_KEY_KP_PLUS, VT_KEY_KP_STAR, VT_KEY_KP_SLASH,
    VT_KEY_KP_LPAREN, VT_KEY_KP_RPAREN
};
#define VT_MOD_SHIFT 1
#define VT_MOD_ALT   2
#define VT_MOD_CTRL  4
/* Writes at most 32 bytes to out; returns the count (0: nothing to send). */
int      vt_encode_key(const vt_term *t, long key, int mods, vt_u8 *out);
/* Mouse reports, when the host asked for them (?9, ?1000, ?1002, ?1003,
 * with ?1006 for the SGR form). button: 0 left, 1 middle, 2 right,
 * 64/65 wheel up/down; kind: 0 press, 1 release, 2 motion. x, y are cell
 * coordinates from 0. Returns 0 when the current modes want no report. */
int      vt_encode_mouse(const vt_term *t, int button, int kind, int x, int y, int mods, vt_u8 *out);
/* Bracketed paste wrapper: writes the prefix or suffix (0 bytes when off). */
int      vt_encode_paste(const vt_term *t, int end, vt_u8 *out);

#endif
