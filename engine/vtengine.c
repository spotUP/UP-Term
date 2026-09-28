/* vtengine -- see vtengine.h. The parser is Paul Williams' DEC-compatible
 * state machine (vt100.net/emu/dec_ansi_parser); the dispatch tables below
 * it are per personality, because the Amiga console and xterm give the same
 * CSI finals different meanings (the conformance matrix in
 * thoughts/shared/research/2026-09-28_console-conformance-matrix.md). */
#include "vtengine.h"
#include <string.h>

#if defined(VT_AMIGA_EXEC_ALLOC)
/* code without a C startup (the handler): exec memory, no libc heap */
#include <exec/memory.h>
#include <proto/exec.h>
#define VT_MALLOC(n) AllocVec((ULONG)(n), MEMF_ANY)
#define VT_FREE(p) FreeVec(p)
#elif !defined(VT_MALLOC)
#include <stdlib.h>
#define VT_MALLOC(n) malloc(n)
#define VT_FREE(p) free(p)
#endif

#define VT_MAX_PARAMS 16
#define VT_STR_MAX 256
#define VT_PARAM_MAX 65535L

/* Parser states. */
enum {
    S_GROUND, S_ESC, S_ESC_INT, S_CSI_ENTRY, S_CSI_PARAM, S_CSI_INT,
    S_CSI_IGNORE, S_OSC, S_STRING /* DCS, SOS, PM, APC: consumed, ignored */
};

/* A grid line: one allocation, cells follow the header. */
typedef struct vt_line {
    vt_u16 cap;      /* cells allocated */
    vt_u16 n;        /* cells in use (== cols for grid lines) */
    vt_u8 wrapped;   /* the text continues on the next line */
    vt_u8 pad;
    vt_cell c[1];
} vt_line;

typedef struct vt_saved {
    int x, y, wrap_pending, origin;
    vt_u16 fg, bg;
    vt_u8 attr;
    vt_u8 charset[4];
    int gl;
} vt_saved;

struct vt_term {
    vt_callbacks cb;
    void *user;
    enum vt_personality pers;
    int cols, rows;

    vt_line **scr, **pri, **alt;
    vt_line **sb;          /* scrollback ring */
    long scrolled;         /* lines scrolled off the primary screen's top */
    int sb_cap, sb_len, sb_head; /* head: next slot to write */

    int cx, cy, wrap_pending;
    vt_u16 fg, bg;
    vt_u8 attr;
    int top, bot;          /* scroll region rows [top, bot) */
    vt_u8 *tabs;
    int tabs_cap;

    vt_u32 modes;
    int autowrap, origin, insert;
    vt_u8 charset[4];      /* 'B' ASCII, '0' DEC graphics, 'A' UK */
    int gl, single_shift;
    vt_saved sav, sav_1049;
    vt_u16 last_ch;

    /* amiga personality */
    vt_u32 raw_events;
    vt_u16 amiga_bg;       /* global background pen (SGR >n) */
    vt_u16 amiga_dfg, amiga_dbg; /* SGR 0 / 39 / 49 defaults (CSI SP s, V39) */
    vt_u8 amiga_dattr;
    int amiga_msb;         /* SO: 20-7F display as A0-FF (matrix 2.1, C-SO) */
    int scroll_enabled;    /* CSI >1h / >1l */

    /* parser */
    int state;
    long params[VT_MAX_PARAMS];
    vt_u8 sub[VT_MAX_PARAMS];  /* 1: this parameter followed a ':' */
    int np, have;
    vt_u8 priv, inter;
    int ninter;
    int csi8;                  /* this CSI came as the raw byte $9B (see csi_xterm) */
    int raw_c1;                /* feed(): the C1 code point is a raw 8-bit byte */
    vt_u8 str_kind;            /* ']' OSC, 'P' DCS, 'X' SOS, '^' PM, '_' APC */
    int str_esc;
    char str[VT_STR_MAX];
    int str_len;

    /* UTF-8 decoder (xterm) */
    vt_u32 u_cp;
    int u_need;
    int utf8;                  /* the charset is UTF-8 */
    int cp437;                 /* the charset is CP437 (xterm personality) */
    int onlcr;                 /* LF also returns (vt_set_onlcr) */

    /* per-row dirty spans, flushed at the end of each write */
    short *dx0, *dx1;
    int dirty;
    /* A scroll the renderer has not been told about yet: the pixels are
     * pend_n rows behind the grid in [pend_top, pend_bot) (negative: down).
     * All scrolls of one write become one blit; the dirty spans move with
     * the rows, so after flush() the pixels equal the grid. */
    int pend_n, pend_top, pend_bot;

    char title[VT_STR_MAX];

    /* sequences parsed but not acted on (vt_unhandled): the terminfo test
     * requires none, so a capability the engine lacks cannot hide */
    long unhandled;
    char unhandled_seen[16][16];   /* distinct kinds, with counts */
    long unhandled_count[16];
    int unhandled_kinds;
};

static void note_text(vt_term *t, const char *d)
{
    int i;
    t->unhandled++;
    for (i = 0; i < t->unhandled_kinds; i++)
        if (!strcmp(t->unhandled_seen[i], d)) {
            t->unhandled_count[i]++;
            return;
        }
    if (t->unhandled_kinds < 16) {
        strcpy(t->unhandled_seen[t->unhandled_kinds], d);
        t->unhandled_count[t->unhandled_kinds++] = 1;
    }
}

static int put_utf8(vt_u8 *o, long c);

static void note_value(vt_term *t, char kind, long v)
{
    char d[16];
    int n = 0, k = 0;
    char tmp[12];
    d[n++] = kind;
    d[n++] = ' ';
    if (v < 0)
        v = 0;
    if (!v)
        tmp[k++] = '0';
    while (v && k < 10) {
        tmp[k++] = (char)('0' + v % 10);
        v /= 10;
    }
    while (k)
        d[n++] = tmp[--k];
    d[n] = 0;
    note_text(t, d);
}

/* Record a sequence the engine parsed and ignored: "CSI ? 12 h" style. */
static void note_unhandled(vt_term *t, char kind, vt_u8 final)
{
    char d[16];
    int n = 0;
    long p = t->np ? t->params[0] : -1;
    d[n++] = kind;
    d[n++] = ' ';
    if (t->priv)
        d[n++] = (char)t->priv;
    if (p >= 0) {
        char tmp[8];
        int k = 0;
        if (!p)
            tmp[k++] = '0';
        while (p && k < 6) {
            tmp[k++] = (char)('0' + p % 10);
            p /= 10;
        }
        while (k)
            d[n++] = tmp[--k];
    }
    if (t->inter)
        d[n++] = (char)t->inter;
    d[n++] = (char)final;
    d[n] = 0;
    note_text(t, d);
}

/* ---- small helpers ---------------------------------------------------- */

static int clampi(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static vt_line *line_new(int cap)
{
    vt_line *l;
    if (cap < 1)
        cap = 1;
    l = (vt_line *)VT_MALLOC(sizeof(vt_line) + (cap - 1) * sizeof(vt_cell));
    if (l) {
        l->cap = (vt_u16)cap;
        l->n = 0;
        l->wrapped = 0;
        l->pad = 0;
    }
    return l;
}

static void blank_cell(const vt_term *t, vt_cell *c)
{
    c->ch = ' ';
    c->fg = VT_COLOR_DEFAULT;
    /* BCE: erased cells take the current background. The Amiga console
     * fills vacated areas with the global background colour instead (RKM
     * Devices, SGR implementation notes). */
    c->bg = (t->pers == VT_AMIGA) ? VT_COLOR_DEFAULT : t->bg;
    c->attr = 0;
    c->width = 1;
}

static void cells_blank(const vt_term *t, vt_cell *c, int n)
{
    vt_cell b;
    int i;
    blank_cell(t, &b);
    for (i = 0; i < n; i++)
        c[i] = b;
}

static void line_clear(const vt_term *t, vt_line *l, int n)
{
    cells_blank(t, l->c, n);
    l->n = (vt_u16)n;
    l->wrapped = 0;
}

static void mark(vt_term *t, int x0, int y, int x1)
{
    if (y < 0 || y >= t->rows)
        return;
    x0 = clampi(x0, 0, t->cols);
    x1 = clampi(x1, 0, t->cols);
    if (x0 >= x1)
        return;
    if (t->dx0[y] > x0)
        t->dx0[y] = (short)x0;
    if (t->dx1[y] < x1)
        t->dx1[y] = (short)x1;
    t->dirty = 1;
}

static void mark_rows(vt_term *t, int y0, int y1)
{
    int y;
    for (y = y0; y < y1; y++)
        mark(t, 0, y, t->cols);
}

/* Tell the renderer about everything marked, one call per run of rows with
 * the same span. */
static void flush(vt_term *t)
{
    int y, y0;
    if (t->pend_n) {
        int n = t->pend_n;
        t->pend_n = 0;
        if (t->cb.scroll)
            t->cb.scroll(t->user, t->pend_top, t->pend_bot, n);
    }
    if (!t->dirty)
        return;
    t->dirty = 0;
    y = 0;
    while (y < t->rows) {
        if (t->dx0[y] >= t->dx1[y]) {
            y++;
            continue;
        }
        y0 = y;
        while (y + 1 < t->rows && t->dx0[y + 1] == t->dx0[y0] && t->dx1[y + 1] == t->dx1[y0])
            y++;
        if (t->cb.damage)
            t->cb.damage(t->user, t->dx0[y0], y0, t->dx1[y0], y + 1);
        for (; y0 <= y; y0++) {
            t->dx0[y0] = (short)t->cols;
            t->dx1[y0] = 0;
        }
        y++;
    }
}

static void reply(vt_term *t, const char *s, int n)
{
    if (t->cb.reply)
        t->cb.reply(t->user, (const vt_u8 *)s, n);
}

static int fmt_uint(char *buf, int n, long v)
{
    char d[12];
    int k = 0;
    if (v < 0)
        v = 0;
    if (!v)
        d[k++] = '0';
    while (v) {
        d[k++] = (char)('0' + v % 10);
        v /= 10;
    }
    while (k)
        buf[n++] = d[--k];
    return n;
}

/* The CSI introducer a reply uses: the Amiga console answers with the 8-bit
 * $9B, and so does the xterm personality to a request that came 8-bit;
 * everything else gets ESC [. */
static int put_csi(const vt_term *t, char *buf)
{
    /* an 8-bit request is answered 8-bit too (ixemul parses 9B ... r) */
    if (t->pers == VT_AMIGA || t->csi8) {
        buf[0] = (char)0x9B;
        return 1;
    }
    buf[0] = 0x1B;
    buf[1] = '[';
    return 2;
}

/* ---- character tables --------------------------------------------------- */

static const vt_u16 cp437_hi[128] = {
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
    0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
    0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
    0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
    0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
    0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
    0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
    0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
    0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4,
    0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
    0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248,
    0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
};

/* DEC special graphics, 0x5F-0x7E. */
static const vt_u16 dec_graphics[32] = {
    0x00A0, 0x25C6, 0x2592, 0x2409, 0x240C, 0x240D, 0x240A, 0x00B0,
    0x00B1, 0x2424, 0x240B, 0x2518, 0x2510, 0x250C, 0x2514, 0x253C,
    0x23BA, 0x23BB, 0x2500, 0x23BC, 0x23BD, 0x251C, 0x2524, 0x2534,
    0x252C, 0x2502, 0x2264, 0x2265, 0x03C0, 0x2260, 0x00A3, 0x00B7
};

const vt_u16 *vt_cp437_table(void)
{
    return cp437_hi;
}

/* Display width: 0 for combining marks, 2 for East Asian wide and
 * fullwidth, 1 otherwise. Ranges as in Markus Kuhn's wcwidth, reduced to
 * the BMP blocks a terminal meets. */
static int cp_width(vt_u32 c)
{
    if (c < 0x300)
        return 1;
    if ((c >= 0x0300 && c <= 0x036F) || (c >= 0x0483 && c <= 0x0489) ||
        (c >= 0x0591 && c <= 0x05BD) || (c >= 0x0610 && c <= 0x061A) ||
        (c >= 0x064B && c <= 0x065F) || (c >= 0x0E31 && c <= 0x0E3A && c != 0x0E32 && c != 0x0E33) ||
        (c >= 0x1AB0 && c <= 0x1AFF) || (c >= 0x1DC0 && c <= 0x1DFF) ||
        (c >= 0x200B && c <= 0x200F) || (c >= 0x20D0 && c <= 0x20FF) ||
        (c >= 0xFE00 && c <= 0xFE0F) || (c >= 0xFE20 && c <= 0xFE2F) || c == 0xFEFF)
        return 0;
    if ((c >= 0x1100 && c <= 0x115F) || c == 0x2329 || c == 0x232A ||
        (c >= 0x2E80 && c <= 0xA4CF && c != 0x303F) || (c >= 0xAC00 && c <= 0xD7A3) ||
        (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFE30 && c <= 0xFE4F) ||
        (c >= 0xFF00 && c <= 0xFF60) || (c >= 0xFFE0 && c <= 0xFFE6))
        return 2;
    return 1;
}

/* ---- grid operations ---------------------------------------------------- */

static vt_cell *cell_at(vt_term *t, int x, int y)
{
    return &t->scr[y]->c[x];
}

/* Overwriting either half of a wide glyph blanks the other half. */
static void unwide(vt_term *t, int x, int y)
{
    vt_cell *c = t->scr[y]->c;
    if (c[x].width == 0 && x > 0) {
        blank_cell(t, &c[x - 1]);
        mark(t, x - 1, y, x);
    }
    if (c[x].width == 2 && x + 1 < t->cols) {
        blank_cell(t, &c[x + 1]);
        mark(t, x + 1, y, x + 2);
    }
}

/* The renderer's scroll fills the rows it vacates with the default
 * background (the contract in vtengine.h), so blank rows in that colour
 * need no drawing: only a BCE erase in another colour does. */
static int vacated_default(const vt_term *t)
{
    vt_cell b;
    blank_cell(t, &b);
    return b.bg == VT_COLOR_DEFAULT;
}

/* Before the grid moves: a pending scroll of another region or direction
 * is settled first, while the grid still matches it. */
static void pend_prepare(vt_term *t, int top, int bot, int n)
{
    if (t->cb.scroll && t->pend_n &&
        (t->pend_top != top || t->pend_bot != bot || (t->pend_n > 0) != (n > 0)))
        flush(t);
}

/* The rows [top, bot) moved by n (up when n > 0): move their dirty spans
 * with them and remember the pixels owe that scroll (see pend_n). */
static void pend_scroll(vt_term *t, int top, int bot, int n)
{
    int y, h = bot - top;
    if (!t->cb.scroll) {
        mark_rows(t, top, bot); /* no blitting renderer: redraw the region */
        return;
    }
    if (n > 0) {
        for (y = top; y < bot - n; y++) {
            t->dx0[y] = t->dx0[y + n];
            t->dx1[y] = t->dx1[y + n];
        }
        for (y = bot - n; y < bot; y++) {
            t->dx0[y] = (short)t->cols;
            t->dx1[y] = 0;
        }
        if (!vacated_default(t))
            mark_rows(t, bot - n, bot);
    } else {
        for (y = bot - 1; y >= top - n; y--) {
            t->dx0[y] = t->dx0[y + n];
            t->dx1[y] = t->dx1[y + n];
        }
        for (y = top; y < top - n; y++) {
            t->dx0[y] = (short)t->cols;
            t->dx1[y] = 0;
        }
        if (!vacated_default(t))
            mark_rows(t, top, top - n);
    }
    t->pend_top = top;
    t->pend_bot = bot;
    t->pend_n += n;
    if (t->pend_n >= h || -t->pend_n >= h) {
        t->pend_n = 0; /* everything moved out: redraw instead of blitting */
        mark_rows(t, top, bot);
    }
}

static void sb_push(vt_term *t, vt_line *l)
{
    if (!t->sb_cap) {
        VT_FREE(l);
        return;
    }
    if (t->sb_len == t->sb_cap)
        VT_FREE(t->sb[t->sb_head]);
    else
        t->sb_len++;
    t->sb[t->sb_head] = l;
    t->sb_head = (t->sb_head + 1) % t->sb_cap;
}

/* Rows [top, bot) move up by n; n blank rows enter at the bottom. Lines
 * leaving the top of the primary screen go to the scrollback. */
static void scroll_up(vt_term *t, int top, int bot, int n)
{
    int i, h = bot - top;
    vt_line *l;
    if (n <= 0 || h <= 0)
        return;
    if (n > h)
        n = h;
    pend_prepare(t, top, bot, n);
    if (top == 0 && t->scr == t->pri)
        t->scrolled += n;
    for (i = 0; i < n; i++) {
        l = t->scr[top];
        memmove(&t->scr[top], &t->scr[top + 1], (h - 1) * sizeof(vt_line *));
        if (top == 0 && t->scr == t->pri && t->pers != VT_AMIGA && t->sb_cap) {
            /* Into the scrollback. A full ring hands back its oldest line
             * to become the new blank one, so steady scrolling allocates
             * nothing. */
            vt_line *blank = 0;
            if (t->sb_len == t->sb_cap && t->sb[t->sb_head]->cap >= t->cols) {
                blank = t->sb[t->sb_head];
            } else {
                blank = line_new(t->cols);
                if (blank) {
                    if (t->sb_len == t->sb_cap)
                        VT_FREE(t->sb[t->sb_head]);
                    else
                        t->sb_len++;
                }
            }
            if (blank) { /* no memory: the line is dropped, not saved */
                t->sb[t->sb_head] = l;
                t->sb_head = (t->sb_head + 1) % t->sb_cap;
                l = blank;
            }
        }
        line_clear(t, l, t->cols);
        t->scr[bot - 1] = l;
    }
    pend_scroll(t, top, bot, n);
}

static void scroll_down(vt_term *t, int top, int bot, int n)
{
    int i, h = bot - top;
    vt_line *l;
    if (n <= 0 || h <= 0)
        return;
    if (n > h)
        n = h;
    pend_prepare(t, top, bot, -n);
    for (i = 0; i < n; i++) {
        l = t->scr[bot - 1];
        memmove(&t->scr[top + 1], &t->scr[top], (h - 1) * sizeof(vt_line *));
        line_clear(t, l, t->cols);
        t->scr[top] = l;
    }
    pend_scroll(t, top, bot, -n);
}

static void erase_cells(vt_term *t, int y, int x0, int x1)
{
    x0 = clampi(x0, 0, t->cols);
    x1 = clampi(x1, 0, t->cols);
    if (x0 >= x1)
        return;
    unwide(t, x0, y);
    if (x1 < t->cols)
        unwide(t, x1 - 1, y);
    cells_blank(t, &t->scr[y]->c[x0], x1 - x0);
    if (x1 == t->cols)
        t->scr[y]->wrapped = 0;
    mark(t, x0, y, x1);
}

static void erase_rows(vt_term *t, int y0, int y1)
{
    int y;
    for (y = y0; y < y1; y++)
        erase_cells(t, y, 0, t->cols);
}

static void home_limits(vt_term *t, int *y0, int *y1)
{
    if (t->origin) {
        *y0 = t->top;
        *y1 = t->bot - 1;
    } else {
        *y0 = 0;
        *y1 = t->rows - 1;
    }
}

static void move_to(vt_term *t, int x, int y)
{
    int y0, y1;
    home_limits(t, &y0, &y1);
    t->cx = clampi(x, 0, t->cols - 1);
    t->cy = clampi(y, y0, y1);
    t->wrap_pending = 0;
}

/* Relative vertical motion stops at the scroll margins when it starts
 * inside them (xterm), at the screen edge otherwise. */
static void move_rel_y(vt_term *t, int dy)
{
    int y = t->cy + dy;
    if (t->cy >= t->top && t->cy < t->bot)
        y = clampi(y, t->top, t->bot - 1);
    else
        y = clampi(y, 0, t->rows - 1);
    t->cy = y;
    t->wrap_pending = 0;
}

static void index_down(vt_term *t)
{
    if (t->cy == t->bot - 1) {
        if (t->pers != VT_AMIGA || t->scroll_enabled)
            scroll_up(t, t->top, t->bot, 1);
    } else if (t->cy < t->rows - 1) {
        t->cy++;
    }
}

static void index_up(vt_term *t)
{
    if (t->cy == t->top)
        scroll_down(t, t->top, t->bot, 1);
    else if (t->cy > 0)
        t->cy--;
}

static void tab_reset(vt_term *t)
{
    int i;
    for (i = 0; i < t->tabs_cap; i++)
        t->tabs[i] = (vt_u8)(i && (i % 8) == 0);
}

static void tab_forward(vt_term *t, int n)
{
    while (n-- > 0) {
        int x = t->cx + 1;
        while (x < t->cols - 1 && !t->tabs[x])
            x++;
        t->cx = clampi(x, 0, t->cols - 1);
    }
    t->wrap_pending = 0;
}

static void tab_back(vt_term *t, int n)
{
    while (n-- > 0) {
        int x = t->cx - 1;
        while (x > 0 && !t->tabs[x])
            x--;
        t->cx = clampi(x, 0, t->cols - 1);
    }
    t->wrap_pending = 0;
}

static void insert_chars(vt_term *t, int n)
{
    vt_cell *c = t->scr[t->cy]->c;
    int x = t->cx, k;
    n = clampi(n, 1, t->cols - x);
    unwide(t, x, t->cy);
    k = t->cols - x - n;
    if (k > 0)
        memmove(&c[x + n], &c[x], k * sizeof(vt_cell));
    cells_blank(t, &c[x], n);
    if (c[t->cols - 1].width == 2)
        blank_cell(t, &c[t->cols - 1]);
    t->wrap_pending = 0;
    mark(t, x, t->cy, t->cols);
}

static void delete_chars(vt_term *t, int n)
{
    vt_cell *c = t->scr[t->cy]->c;
    int x = t->cx, k;
    n = clampi(n, 1, t->cols - x);
    unwide(t, x, t->cy);
    if (x + n < t->cols)
        unwide(t, x + n, t->cy);
    k = t->cols - x - n;
    if (k > 0)
        memmove(&c[x], &c[x + n], k * sizeof(vt_cell));
    cells_blank(t, &c[t->cols - n], n);
    t->wrap_pending = 0;
    mark(t, x, t->cy, t->cols);
}

static void insert_lines(vt_term *t, int n)
{
    if (t->cy < t->top || t->cy >= t->bot)
        return;
    scroll_down(t, t->cy, t->bot, n);
    t->cx = 0;
    t->wrap_pending = 0;
}

static void delete_lines(vt_term *t, int n)
{
    if (t->cy < t->top || t->cy >= t->bot)
        return;
    scroll_up(t, t->cy, t->bot, n);
    t->cx = 0;
    t->wrap_pending = 0;
}

/* ---- state ------------------------------------------------------------- */

static void save_cursor(vt_term *t, vt_saved *s)
{
    s->x = t->cx;
    s->y = t->cy;
    s->wrap_pending = t->wrap_pending;
    s->origin = t->origin;
    s->fg = t->fg;
    s->bg = t->bg;
    s->attr = t->attr;
    memcpy(s->charset, t->charset, 4);
    s->gl = t->gl;
}

static void restore_cursor(vt_term *t, const vt_saved *s)
{
    t->origin = s->origin;
    t->fg = s->fg;
    t->bg = s->bg;
    t->attr = s->attr;
    memcpy(t->charset, s->charset, 4);
    t->gl = s->gl;
    t->cx = clampi(s->x, 0, t->cols - 1);
    t->cy = clampi(s->y, 0, t->rows - 1);
    t->wrap_pending = 0; /* DECRC lands on the cell, like libvterm (quirk-wrap-then-decsc-decrc) */
}

static void sgr_reset(vt_term *t)
{
    if (t->pers == VT_AMIGA) {
        t->fg = t->amiga_dfg;
        t->bg = t->amiga_dbg;
        t->attr = t->amiga_dattr;
        return;
    }
    t->fg = VT_COLOR_DEFAULT;
    t->bg = VT_COLOR_DEFAULT;
    t->attr = 0;
}

static void soft_reset(vt_term *t)
{
    t->insert = 0;
    t->origin = 0;
    t->autowrap = 1;
    t->modes &= ~(vt_u32)(VT_MODE_APP_CURSOR | VT_MODE_APP_KEYPAD);
    t->modes |= VT_MODE_CURSOR_VISIBLE;
    t->charset[0] = t->charset[1] = t->charset[2] = t->charset[3] = 'B';
    t->gl = 0;
    t->single_shift = 0;
    t->top = 0;
    t->bot = t->rows;
    sgr_reset(t);
    t->sav.x = t->sav.y = 0;
    t->sav.wrap_pending = 0;
    t->sav.origin = 0;
    t->sav.fg = t->sav.bg = VT_COLOR_DEFAULT;
    t->sav.attr = 0;
    memcpy(t->sav.charset, t->charset, 4);
    t->sav.gl = 0;
}

static void set_alt(vt_term *t, int on, int clear)
{
    if (on && t->scr != t->alt) {
        t->scr = t->alt;
        if (clear)
            erase_rows(t, 0, t->rows);
        t->modes |= VT_MODE_ALT_SCREEN;
        mark_rows(t, 0, t->rows);
    } else if (!on && t->scr == t->alt) {
        if (clear)
            erase_rows(t, 0, t->rows);
        t->scr = t->pri;
        t->modes &= ~(vt_u32)VT_MODE_ALT_SCREEN;
        mark_rows(t, 0, t->rows);
    }
}

/* ---- output of characters ------------------------------------------------ */

static void put_char(vt_term *t, vt_u32 cp)
{
    int w;
    vt_cell *c;
    vt_u8 cs;

    cs = t->charset[t->single_shift ? t->single_shift : t->gl];
    t->single_shift = 0;
    if (cs == '0' && cp >= 0x5F && cp <= 0x7E)
        cp = dec_graphics[cp - 0x5F];
    else if (cs == 'A' && cp == '#')
        cp = 0xA3;
    if (cp > 0xFFFF)
        cp = 0xFFFD; /* cells hold the BMP */

    w = cp_width(cp);
    if (w == 0)
        return; /* combining marks: not composed (one glyph per cell) */
    if (w == 2 && t->cols < 2)
        w = 1;

    if (t->wrap_pending && t->autowrap) {
        t->scr[t->cy]->wrapped = 1;
        t->cx = 0;
        index_down(t);
    }
    t->wrap_pending = 0;
    if (w == 2 && t->cx == t->cols - 1) {
        if (t->autowrap) {
            erase_cells(t, t->cy, t->cx, t->cols);
            t->scr[t->cy]->wrapped = 1;
            t->cx = 0;
            index_down(t);
        } else {
            t->cx = t->cols - 2;
        }
    }
    if (t->insert)
        insert_chars(t, w);

    unwide(t, t->cx, t->cy);
    if (w == 2)
        unwide(t, t->cx + 1, t->cy);
    c = cell_at(t, t->cx, t->cy);
    c->ch = (vt_u16)cp;
    c->fg = t->fg;
    c->bg = t->bg;
    c->attr = t->attr;
    c->width = (vt_u8)w;
    if (w == 2) {
        c[1] = c[0];
        c[1].ch = ' ';
        c[1].width = 0;
    }
    mark(t, t->cx, t->cy, t->cx + w);
    t->last_ch = (vt_u16)cp;

    if (t->cx + w >= t->cols) {
        t->cx = t->cols - 1;
        t->wrap_pending = 1;
    } else {
        t->cx += w;
    }
}

/* ---- controls ------------------------------------------------------------ */

static void clear_screen_home(vt_term *t)
{
    erase_rows(t, 0, t->rows);
    t->cx = t->cy = 0;
    t->wrap_pending = 0;
}

static void exec_c0(vt_term *t, vt_u32 c)
{
    switch (c) {
    case 0x07:
        if (t->cb.bell)
            t->cb.bell(t->user);
        break;
    case 0x08:
        if (t->cx > 0)
            t->cx--;
        t->wrap_pending = 0;
        break;
    case 0x09:
        tab_forward(t, 1);
        break;
    case 0x0B:
        if (t->pers == VT_AMIGA) { /* VT: cursor up (RKM) */
            if (t->cy > 0)
                t->cy--;
            t->wrap_pending = 0;
            break;
        }
        /* fall through: VT is LF elsewhere */
    case 0x0A:
        t->wrap_pending = 0;
        index_down(t);
        if ((t->modes & VT_MODE_NEWLINE) || t->onlcr)
            t->cx = 0;
        break;
    case 0x0C:
        if (t->pers == VT_XTERM) { /* FF is LF on a VT100 */
            t->wrap_pending = 0;
            index_down(t);
            if (t->modes & VT_MODE_NEWLINE)
                t->cx = 0;
        } else {
            clear_screen_home(t); /* the Amiga console and ANSI.SYS clear */
        }
        break;
    case 0x0D:
        t->cx = 0;
        t->wrap_pending = 0;
        break;
    case 0x0E:
        if (t->pers == VT_XTERM)
            t->gl = 1;
        else if (t->pers == VT_AMIGA)
            t->amiga_msb = 1;
        break;
    case 0x0F:
        if (t->pers == VT_XTERM)
            t->gl = 0;
        else if (t->pers == VT_AMIGA)
            t->amiga_msb = 0;
        break;
    default:
        break;
    }
}

static void exec_c1(vt_term *t, vt_u32 c)
{
    switch (c) {
    case 0x84: /* IND */
        index_down(t);
        break;
    case 0x85: /* NEL */
        t->cx = 0;
        t->wrap_pending = 0;
        index_down(t);
        break;
    case 0x88: /* HTS */
        if (t->cx < t->tabs_cap)
            t->tabs[t->cx] = 1;
        break;
    case 0x8D: /* RI */
        t->wrap_pending = 0;
        index_up(t);
        break;
    case 0x8E: /* SS2 */
        t->single_shift = 2;
        break;
    case 0x8F: /* SS3 */
        t->single_shift = 3;
        break;
    default:
        break;
    }
}

/* ---- ESC dispatch --------------------------------------------------------- */

static void esc_dispatch(vt_term *t, vt_u8 final)
{
    if (t->inter) {
        switch (t->inter) {
        case '(':
        case ')':
        case '*':
        case '+':
            if (final == '0' || final == 'B' || final == 'A')
                t->charset[t->inter - '('] = final;
            else
                t->charset[t->inter - '('] = 'B';
            break;
        case '#':
            if (final == '8') { /* DECALN */
                int x, y;
                t->top = 0;
                t->bot = t->rows;
                for (y = 0; y < t->rows; y++) {
                    for (x = 0; x < t->cols; x++) {
                        vt_cell *c = cell_at(t, x, y);
                        blank_cell(t, c);
                        c->ch = 'E';
                    }
                    t->scr[y]->wrapped = 0;
                }
                mark_rows(t, 0, t->rows);
                move_to(t, 0, 0);
            } else {
                note_unhandled(t, 'E', final);
            }
            break;
        default:
            note_unhandled(t, 'E', final);
            break;
        }
        return;
    }
    switch (final) {
    case '7':
        save_cursor(t, &t->sav);
        break;
    case '8':
        restore_cursor(t, &t->sav);
        break;
    case 'D':
        exec_c1(t, 0x84);
        break;
    case 'E':
        exec_c1(t, 0x85);
        break;
    case 'H':
        exec_c1(t, 0x88);
        break;
    case 'M':
        exec_c1(t, 0x8D);
        break;
    case 'N':
        exec_c1(t, 0x8E);
        break;
    case 'O':
        exec_c1(t, 0x8F);
        break;
    case 'c':
        vt_reset(t);
        break;
    case '=':
        t->modes |= VT_MODE_APP_KEYPAD;
        break;
    case '>':
        t->modes &= ~(vt_u32)VT_MODE_APP_KEYPAD;
        break;
    case '\\':
        break; /* ST with nothing open */
    default:
        note_unhandled(t, 'E', final);
        break;
    }
}

/* ---- CSI dispatch ---------------------------------------------------------- */

static long param(const vt_term *t, int i, long def)
{
    if (i >= t->np || t->params[i] == 0)
        return def;
    return t->params[i];
}

static long param0(const vt_term *t, int i)
{
    if (i >= t->np)
        return 0;
    return t->params[i];
}

static vt_u16 ext_colour(vt_term *t, int *i)
{
    /* 38;5;n / 38;2;r;g;b and the colon forms 38:5:n, 38:2:[cs]:r:g:b */
    int k = *i;
    long mode = param0(t, k + 1);
    int colon = (k + 1 < t->np) && t->sub[k + 1];
    if (mode == 5) {
        *i = k + 2;
        return (vt_u16)(param0(t, k + 2) & 0xFF);
    }
    if (mode == 2) {
        int base = k + 2;
        long r, g, b;
        if (colon) {
            /* count the sub-parameters that follow 38:2 */
            int n = 0;
            while (base + n < t->np && t->sub[base + n])
                n++;
            if (n >= 4)
                base++; /* colour space id present */
        }
        r = param0(t, base);
        g = param0(t, base + 1);
        b = param0(t, base + 2);
        *i = base + 2;
        return VT_RGB(clampi((int)r, 0, 255), clampi((int)g, 0, 255), clampi((int)b, 0, 255));
    }
    *i = k + 1;
    return VT_COLOR_DEFAULT;
}

static void sgr(vt_term *t)
{
    int i;
    if (t->np == 0) {
        sgr_reset(t);
        return;
    }
    if (t->pers == VT_PCANSI) {
        for (i = 0; i < t->np; i++)
            if (t->sub[i])
                return; /* DCTelnet ignores the whole SGR on a ':' form */
    }
    for (i = 0; i < t->np; i++) {
        long p = t->params[i];
        if (t->sub[i] == 2) { /* amiga '>n' item */
            t->amiga_bg = (vt_u16)(p & 0xFF);
            mark_rows(t, 0, t->rows);
            continue;
        }
        if (t->sub[i])
            continue; /* a stray sub-parameter */
        if (t->pers == VT_PCANSI && (p == 2 || p == 21)) {
            t->attr &= ~VT_ATTR_BOLD; /* DCTelnet: intensity off */
            continue;
        }
        if (t->pers == VT_PCANSI && p >= 90 && p <= 97) {
            t->fg = (vt_u16)(p - 90); /* and bold, so a later 30-37 stays bright */
            t->attr |= VT_ATTR_BOLD;
            continue;
        }
        if (t->pers == VT_PCANSI && p >= 100 && p <= 107) {
            t->bg = (vt_u16)(p - 100);
            t->attr |= VT_ATTR_BLINK;
            continue;
        }
        if (t->pers == VT_PCANSI && (p == 38 || p == 48)) {
            /* consumed with no effect: 16 colours only */
            long mode = param0(t, i + 1);
            i += mode == 5 ? 2 : mode == 2 ? 4 : 1;
            continue;
        }
        if (p == 0) {
            sgr_reset(t);
        } else if (p == 1) {
            t->attr |= VT_ATTR_BOLD;
        } else if (p == 2) {
            t->attr |= VT_ATTR_FAINT;
        } else if (p == 3) {
            t->attr |= VT_ATTR_ITALIC;
        } else if (p == 4) {
            if (i + 1 < t->np && t->sub[i + 1]) {
                if (t->params[i + 1] == 0)
                    t->attr &= ~VT_ATTR_UNDERLINE;
                else
                    t->attr |= VT_ATTR_UNDERLINE;
                i++;
            } else {
                t->attr |= VT_ATTR_UNDERLINE;
            }
        } else if (p == 5 || p == 6) {
            t->attr |= VT_ATTR_BLINK;
        } else if (p == 7) {
            t->attr |= VT_ATTR_INVERSE;
        } else if (p == 8) {
            t->attr |= VT_ATTR_CONCEAL;
        } else if (p == 9) {
            t->attr |= VT_ATTR_STRIKE;
        } else if (p == 21) {
            t->attr |= VT_ATTR_UNDERLINE;
        } else if (p == 22) {
            t->attr &= ~(VT_ATTR_BOLD | VT_ATTR_FAINT);
        } else if (p == 23) {
            t->attr &= ~VT_ATTR_ITALIC;
        } else if (p == 24) {
            t->attr &= ~VT_ATTR_UNDERLINE;
        } else if (p == 25) {
            t->attr &= ~VT_ATTR_BLINK;
        } else if (p == 27) {
            t->attr &= ~VT_ATTR_INVERSE;
        } else if (p == 28) {
            t->attr &= ~VT_ATTR_CONCEAL;
        } else if (p == 29) {
            t->attr &= ~VT_ATTR_STRIKE;
        } else if (p >= 30 && p <= 37) {
            t->fg = (vt_u16)(p - 30);
        } else if (p == 38) {
            t->fg = ext_colour(t, &i);
        } else if (p == 39) {
            t->fg = t->pers == VT_AMIGA ? t->amiga_dfg : VT_COLOR_DEFAULT;
        } else if (p >= 40 && p <= 47) {
            t->bg = (vt_u16)(p - 40);
        } else if (p == 48) {
            t->bg = ext_colour(t, &i);
        } else if (p == 49) {
            t->bg = t->pers == VT_AMIGA ? t->amiga_dbg : VT_COLOR_DEFAULT;
        } else if (p >= 90 && p <= 97 && t->pers != VT_AMIGA) {
            t->fg = (vt_u16)(p - 90 + 8);
        } else if (p >= 100 && p <= 107 && t->pers != VT_AMIGA) {
            t->bg = (vt_u16)(p - 100 + 8);
        } else {
            note_value(t, 'S', p);
        }
    }
}

static void set_mode(vt_term *t, int on)
{
    int i;
    for (i = 0; i < t->np; i++) {
        long p = t->params[i];
        if (t->priv == '?') {
            switch (p) {
            case 1:
                if (on)
                    t->modes |= VT_MODE_APP_CURSOR;
                else
                    t->modes &= ~(vt_u32)VT_MODE_APP_CURSOR;
                break;
            case 3: /* DECCOLM: the window sets the width (xterm without allowColumns) */
            case 4: /* DECSCLM: smooth scrolling, nothing to do */
                break;
            case 5: /* DECSCNM */
                if (!on != !(t->modes & VT_MODE_SCREEN_REVERSE)) {
                    t->modes ^= VT_MODE_SCREEN_REVERSE;
                    mark_rows(t, 0, t->rows);
                }
                break;
            case 6:
                t->origin = on;
                move_to(t, 0, t->origin ? t->top : 0);
                break;
            case 7:
                t->autowrap = on;
                if (!on)
                    t->wrap_pending = 0;
                break;
            case 25:
                if (on)
                    t->modes |= VT_MODE_CURSOR_VISIBLE;
                else
                    t->modes &= ~(vt_u32)VT_MODE_CURSOR_VISIBLE;
                break;
            case 9:
            case 1000:
            case 1002:
            case 1003:
            case 1004:
            case 1006:
            case 2004: {
                vt_u32 bit = p == 9 ? VT_MODE_MOUSE_X10 : p == 1000 ? VT_MODE_MOUSE_NORMAL
                           : p == 1002 ? VT_MODE_MOUSE_BUTTON : p == 1003 ? VT_MODE_MOUSE_ANY
                           : p == 1004 ? VT_MODE_FOCUS : p == 1006 ? VT_MODE_MOUSE_SGR
                           : VT_MODE_BRACKET_PASTE;
                if (on)
                    t->modes |= bit;
                else
                    t->modes &= ~bit;
                break;
            }
            case 66:
                if (on)
                    t->modes |= VT_MODE_APP_KEYPAD;
                else
                    t->modes &= ~(vt_u32)VT_MODE_APP_KEYPAD;
                break;
            case 47:
                set_alt(t, on, 0);
                break;
            case 1047:
                set_alt(t, on, !on);
                break;
            case 1048:
                if (on)
                    save_cursor(t, &t->sav_1049);
                else
                    restore_cursor(t, &t->sav_1049);
                break;
            case 1049:
                if (on) {
                    save_cursor(t, &t->sav_1049);
                    set_alt(t, 1, 1);
                } else {
                    set_alt(t, 0, 0);
                    restore_cursor(t, &t->sav_1049);
                }
                break;
            default:
                note_value(t, 'M', p); /* a DEC private mode we do not have */
                break;
            }
        } else if (t->priv == '>' && t->pers == VT_AMIGA) {
            if (p == 1)
                t->scroll_enabled = on; /* CSI >1h / >1l */
        } else if (!t->priv) {
            if (p == 4)
                t->insert = on;
            else if (p == 20) {
                if (on)
                    t->modes |= VT_MODE_NEWLINE;
                else
                    t->modes &= ~(vt_u32)VT_MODE_NEWLINE;
            }
        }
    }
}

static void report_cursor(vt_term *t, int dec)
{
    char b[24];
    int n = put_csi(t, b);
    int y = t->cy - (t->origin ? t->top : 0);
    if (dec)
        b[n++] = '?';
    n = fmt_uint(b, n, y + 1);
    b[n++] = ';';
    n = fmt_uint(b, n, t->cx + 1);
    b[n++] = 'R';
    reply(t, b, n);
}

/* CSI finals every personality shares (ECMA-48 editing and motion). Returns
 * 0 when the final is not one of them. */
static int csi_common(vt_term *t, vt_u8 final)
{
    long n = param(t, 0, 1);
    switch (final) {
    case '@':
        insert_chars(t, (int)n);
        return 1;
    case 'A':
        move_rel_y(t, -(int)n);
        return 1;
    case 'B':
    case 'e':
        move_rel_y(t, (int)n);
        return 1;
    case 'C':
    case 'a':
        t->cx = clampi(t->cx + (int)n, 0, t->cols - 1);
        t->wrap_pending = 0;
        return 1;
    case 'D':
        t->cx = clampi(t->cx - (int)n, 0, t->cols - 1);
        t->wrap_pending = 0;
        return 1;
    case 'E':
        t->cx = 0;
        move_rel_y(t, (int)n);
        return 1;
    case 'F':
        t->cx = 0;
        move_rel_y(t, -(int)n);
        return 1;
    case 'G':
    case '`':
        t->cx = clampi((int)n - 1, 0, t->cols - 1);
        t->wrap_pending = 0;
        return 1;
    case 'H':
    case 'f':
        move_to(t, (int)param(t, 1, 1) - 1, (int)n - 1 + (t->origin ? t->top : 0));
        return 1;
    case 'I':
        tab_forward(t, (int)n);
        return 1;
    case 'J': {
        long m = param0(t, 0);
        if (m == 0) {
            erase_cells(t, t->cy, t->cx, t->cols);
            erase_rows(t, t->cy + 1, t->rows);
        } else if (m == 1) {
            erase_rows(t, 0, t->cy);
            erase_cells(t, t->cy, 0, t->cx + 1);
        } else if (m == 2 || m == 3) {
            erase_rows(t, 0, t->rows);
            if (m == 3 && t->pers == VT_XTERM) {
                while (t->sb_len) {
                    t->sb_head = (t->sb_head + t->sb_cap - 1) % t->sb_cap;
                    VT_FREE(t->sb[t->sb_head]);
                    t->sb_len--;
                }
            }
            if (t->pers == VT_PCANSI) { /* ANSI.SYS homes the cursor */
                t->cx = t->cy = 0;
            }
        }
        t->wrap_pending = 0;
        return 1;
    }
    case 'K': {
        long m = param0(t, 0);
        if (m == 0)
            erase_cells(t, t->cy, t->cx, t->cols);
        else if (m == 1)
            erase_cells(t, t->cy, 0, t->cx + 1);
        else if (m == 2)
            erase_cells(t, t->cy, 0, t->cols);
        t->wrap_pending = 0;
        return 1;
    }
    case 'L':
        insert_lines(t, (int)n);
        return 1;
    case 'M':
        delete_lines(t, (int)n);
        return 1;
    case 'P':
        delete_chars(t, (int)n);
        return 1;
    case 'S':
        scroll_up(t, t->top, t->bot, (int)n);
        return 1;
    case 'T':
        if (t->np <= 1)
            scroll_down(t, t->top, t->bot, (int)n);
        return 1;
    case 'X': {
        int x1 = t->cx + (int)n;
        erase_cells(t, t->cy, t->cx, x1);
        t->wrap_pending = 0;
        return 1;
    }
    case 'Z':
        tab_back(t, (int)n);
        return 1;
    case 'b': {
        long k;
        if (!t->last_ch)
            return 1;
        for (k = 0; k < n && k < 65535L; k++)
            put_char(t, t->last_ch);
        return 1;
    }
    case 'd':
        move_to(t, t->cx, (int)n - 1 + (t->origin ? t->top : 0));
        return 1;
    case 'g': {
        long m = param0(t, 0);
        if (m == 0 && t->cx < t->tabs_cap)
            t->tabs[t->cx] = 0;
        else if (m == 3)
            memset(t->tabs, 0, t->tabs_cap);
        return 1;
    }
    case 'h':
        set_mode(t, 1);
        return 1;
    case 'l':
        set_mode(t, 0);
        return 1;
    case 'm':
        sgr(t);
        return 1;
    case 'n':
        if (param0(t, 0) == 5) {
            char b[8];
            int k = put_csi(t, b);
            b[k++] = '0';
            b[k++] = 'n';
            reply(t, b, k);
        } else if (param0(t, 0) == 6) {
            report_cursor(t, 0);
        }
        return 1;
    default:
        return 0;
    }
}

static void csi_amiga(vt_term *t, vt_u8 final);

/* The Amiga console's private sequences whose bytes mean something else in
 * xterm (matrix 3.1): t u x y { } SP p SP q, and SGR with a '>' item. */
static int amiga_collision(const vt_term *t, vt_u8 final)
{
    int i;
    if (t->inter == ' ')
        return final == 'p' || final == 'q';
    if (t->inter)
        return 0;
    if (!t->priv && (final == 't' || final == 'u' || final == 'x' || final == 'y' ||
                     final == '{' || final == '}'))
        return 1;
    if (final == 'm') {
        if (t->priv == '>')
            return 1;
        for (i = 0; i < t->np; i++)
            if (t->sub[i] == 2)
                return 1;
    }
    return 0;
}

static void csi_xterm(vt_term *t, vt_u8 final)
{
    /* One window, both dialects: Unix programs write the 7-bit ESC [, the
     * Amiga ones (and ixemul's size probe) the 8-bit $9B. So a sequence
     * opened with $9B takes the Amiga meaning where the two collide. */
    if (t->csi8 && amiga_collision(t, final)) {
        csi_amiga(t, final);
        return;
    }
    if (t->inter == '!' && final == 'p') { /* DECSTR */
        soft_reset(t);
        return;
    }
    if (t->inter) {
        note_unhandled(t, 'C', final); /* DECSCUSR, DECRQM, ...: parsed and ignored */
        return;
    }
    if (t->priv == '?') {
        if (final == 'h' || final == 'l')
            set_mode(t, final == 'h');
        else if (final == 'n' && param0(t, 0) == 6)
            report_cursor(t, 1);
        else if (final == 'J' || final == 'K')
            csi_common(t, final); /* DECSED / DECSEL: no protected cells */
        else
            note_unhandled(t, 'C', final);
        return;
    }
    if (t->priv == '>') {
        if (final == 'c' && param0(t, 0) == 0)
            reply(t, "\033[>1;10;0c", 10); /* DA2: a VT220, firmware 10 */
        else
            note_unhandled(t, 'C', final);
        return;
    }
    if (t->priv) {
        note_unhandled(t, 'C', final);
        return;
    }
    switch (final) {
    case 'c':
        if (param0(t, 0) == 0)
            reply(t, "\033[?62;22c", 9); /* DA1: VT220 with ANSI colour */
        return;
    case 'r': { /* DECSTBM */
        int top = (int)param(t, 0, 1) - 1;
        int bot = (int)param(t, 1, t->rows);
        if (bot > t->rows)
            bot = t->rows;
        if (top < bot - 1) {
            t->top = top;
            t->bot = bot;
            move_to(t, 0, t->origin ? t->top : 0);
        }
        return;
    }
    case 's':
        save_cursor(t, &t->sav);
        return;
    case 'u':
        restore_cursor(t, &t->sav);
        return;
    case 't':
        if (param0(t, 0) == 18) {
            char b[32];
            int n = put_csi(t, b);
            b[n++] = '8';
            b[n++] = ';';
            n = fmt_uint(b, n, t->rows);
            b[n++] = ';';
            n = fmt_uint(b, n, t->cols);
            b[n++] = 't';
            reply(t, b, n);
        } else {
            note_unhandled(t, 'C', final);
        }
        return;
    default:
        if (!csi_common(t, final))
            note_unhandled(t, 'C', final);
        return;
    }
}

static void csi_pcansi(vt_term *t, vt_u8 final)
{
    if (t->inter)
        return;
    if (t->priv == '?') {
        if ((final == 'h' || final == 'l'))
            set_mode(t, final == 'h');
        return;
    }
    if (t->priv)
        return;
    switch (final) {
    case 's':
        save_cursor(t, &t->sav);
        return;
    case 'u':
        restore_cursor(t, &t->sav);
        return;
    case 'r': /* DECSTBM, as ibmcon 1.4-fixed does */
        csi_xterm(t, final);
        return;
    default:
        csi_common(t, final);
        return;
    }
}

static void csi_amiga(vt_term *t, vt_u8 final)
{
    if (t->inter == ' ') {
        if (final == 'p') { /* cursor rendition: 0 invisible, else visible */
            if (t->np && t->params[0] == 0)
                t->modes &= ~(vt_u32)VT_MODE_CURSOR_VISIBLE;
            else
                t->modes |= VT_MODE_CURSOR_VISIBLE;
        } else if (final == 's') { /* aSDSS: current pens become the defaults */
            t->amiga_dfg = t->fg;
            t->amiga_dbg = t->bg;
            t->amiga_dattr = t->attr;
        } else if (final == 'q') { /* window status request */
            char b[40];
            int n = put_csi(t, b);
            b[n++] = '1';
            b[n++] = ';';
            b[n++] = '1';
            b[n++] = ';';
            n = fmt_uint(b, n, t->rows);
            b[n++] = ';';
            n = fmt_uint(b, n, t->cols);
            b[n++] = ' ';
            b[n++] = 'r';
            reply(t, b, n);
        }
        return;
    }
    if (t->inter)
        return;
    if (t->priv == '>') {
        if (final == 'm') { /* SGR >n: global background pen */
            t->amiga_bg = (vt_u16)(param0(t, 0) & 0xFF);
            mark_rows(t, 0, t->rows);
        } else if (final == 'h' || final == 'l') {
            set_mode(t, final == 'h');
        }
        return;
    }
    if (t->priv == '?') {
        if (final == 'h' || final == 'l')
            set_mode(t, final == 'h');
        return;
    }
    if (t->priv)
        return;
    switch (final) {
    case 't':
    case 'u':
    case 'x':
    case 'y':
        if (t->cb.layout) {
            int which = final == 't' ? VT_LAYOUT_PAGE_LENGTH : final == 'u' ? VT_LAYOUT_LINE_LENGTH
                      : final == 'x' ? VT_LAYOUT_LEFT_OFFSET : VT_LAYOUT_TOP_OFFSET;
            t->cb.layout(t->user, which, t->np ? (int)t->params[0] : -1);
        }
        return;
    case '{':
    case '}': {
        int i;
        for (i = 0; i < t->np; i++) {
            long e = t->params[i];
            if (e >= 0 && e < 32) {
                if (final == '{')
                    t->raw_events |= (vt_u32)1 << e;
                else
                    t->raw_events &= ~((vt_u32)1 << e);
            }
        }
        return;
    }
    case 'W': { /* CTC: 0 set tab here, 2 clear tab here, 5 clear all */
        long m = param0(t, 0);
        if (m == 0 && t->cx < t->tabs_cap)
            t->tabs[t->cx] = 1;
        else if (m == 2 && t->cx < t->tabs_cap)
            t->tabs[t->cx] = 0;
        else if (m == 5)
            memset(t->tabs, 0, t->tabs_cap);
        return;
    }
    case 'c':
    case 'r':
    case 's':
        return; /* not console.device sequences */
    default:
        csi_common(t, final);
        return;
    }
}

static void csi_dispatch(vt_term *t, vt_u8 final)
{
    if (t->pers == VT_AMIGA)
        csi_amiga(t, final);
    else if (t->pers == VT_PCANSI)
        csi_pcansi(t, final);
    else
        csi_xterm(t, final);
}

/* ---- OSC ----------------------------------------------------------------- */

static void osc_dispatch(vt_term *t)
{
    int i = 0;
    long cmd = 0;
    t->str[t->str_len] = 0;
    while (i < t->str_len && t->str[i] >= '0' && t->str[i] <= '9')
        cmd = cmd * 10 + (t->str[i++] - '0');
    if (i >= t->str_len || t->str[i] != ';')
        return;
    i++;
    if (cmd == 1)
        return; /* icon name: no icon to name */
    if (cmd != 0 && cmd != 2) {
        note_value(t, 'O', cmd);
        return;
    }
    {
        int n = t->str_len - i;
        memcpy(t->title, t->str + i, n);
        t->title[n] = 0;
        if (t->cb.title)
            t->cb.title(t->user, t->title);
    }
}

/* ---- parser ---------------------------------------------------------------- */

static void clear_params(vt_term *t)
{
    t->np = 0;
    t->have = 0;
    t->params[0] = 0;
    t->sub[0] = 0;
    t->priv = 0;
    t->inter = 0;
    t->ninter = 0;
}

static void enter_string(vt_term *t, vt_u8 kind)
{
    t->state = kind == ']' ? S_OSC : S_STRING;
    t->str_kind = kind;
    t->str_len = 0;
    t->str_esc = 0;
}

static void end_string(vt_term *t)
{
    if (t->state == S_OSC)
        osc_dispatch(t);
    else
        note_value(t, t->str_kind == 'P' ? 'D' : 'X', 0);
    t->state = S_GROUND;
}

static void feed(vt_term *t, vt_u32 c)
{
    /* Anywhere transitions. */
    if (c == 0x18 || c == 0x1A) { /* CAN, SUB */
        t->state = S_GROUND;
        return;
    }
    if (t->state == S_OSC || t->state == S_STRING) {
        if (t->str_esc) {
            t->str_esc = 0;
            if (c == '\\') {
                end_string(t);
                return;
            }
            end_string(t); /* ESC ends the string and starts a new ESC */
            t->state = S_ESC;
            clear_params(t);
            /* re-dispatch c as the byte after ESC */
        } else if (c == 0x1B) {
            t->str_esc = 1;
            return;
        } else if (c == 0x07 && t->state == S_OSC) {
            end_string(t);
            return;
        } else if (c == 0x9C) {
            end_string(t);
            return;
        } else {
            if (t->state == S_OSC && c >= 0x20 && t->str_len < VT_STR_MAX - 1) {
                if (c < 0x80)
                    t->str[t->str_len++] = (char)c;
                else if (c < 0x800 && t->str_len < VT_STR_MAX - 2) {
                    t->str[t->str_len++] = (char)(0xC0 | (c >> 6));
                    t->str[t->str_len++] = (char)(0x80 | (c & 0x3F));
                } else if (t->str_len < VT_STR_MAX - 3) {
                    t->str[t->str_len++] = (char)(0xE0 | (c >> 12));
                    t->str[t->str_len++] = (char)(0x80 | ((c >> 6) & 0x3F));
                    t->str[t->str_len++] = (char)(0x80 | (c & 0x3F));
                }
            }
            return;
        }
    }
    if (c == 0x1B) {
        t->state = S_ESC;
        clear_params(t);
        return;
    }
    /* C1 arrive as code points: the raw 8-bit bytes for amiga (Latin-1) and
     * pcansi ($9B only, the rest is CP437), UTF-8 C2 80..C2 9F for xterm. */
    if (c >= 0x80 && c <= 0x9F) {
        switch (c) {
        case 0x9B:
            clear_params(t);
            t->csi8 = t->raw_c1;
            t->state = S_CSI_ENTRY;
            return;
        case 0x9D:
            enter_string(t, ']');
            return;
        case 0x90:
            enter_string(t, 'P');
            return;
        case 0x98:
        case 0x9E:
        case 0x9F:
            enter_string(t, 'X');
            return;
        case 0x9C:
            t->state = S_GROUND;
            return;
        default:
            t->state = S_GROUND;
            exec_c1(t, c);
            return;
        }
    }
    if (c < 0x20) {
        exec_c0(t, c); /* C0 controls execute in every state */
        return;
    }

    switch (t->state) {
    case S_GROUND:
        if (c == 0x7F) {
            /* DEL is a glyph on the Amiga console and in CP437 (matrix C-DEL). */
            if (t->pers == VT_AMIGA)
                put_char(t, 0x7F);
            else if (t->pers == VT_PCANSI)
                put_char(t, 0x2302);
            return;
        }
        if (t->amiga_msb && c >= 0x20 && c < 0x80)
            c |= 0x80;
        put_char(t, c);
        return;

    case S_ESC:
        if (c >= 0x20 && c <= 0x2F) {
            t->inter = (vt_u8)c;
            t->ninter = 1;
            t->state = S_ESC_INT;
            return;
        }
        if (c == '[') {
            clear_params(t);
            t->csi8 = 0;
            t->state = S_CSI_ENTRY;
            return;
        }
        if (c == ']' || c == 'P' || c == 'X' || c == '^' || c == '_') {
            enter_string(t, (vt_u8)c);
            return;
        }
        t->state = S_GROUND;
        if (c >= 0x30 && c <= 0x7E)
            esc_dispatch(t, (vt_u8)c);
        return;

    case S_ESC_INT:
        if (c >= 0x20 && c <= 0x2F) {
            t->ninter++;
            return;
        }
        t->state = S_GROUND;
        if (c >= 0x30 && c <= 0x7E && t->ninter == 1)
            esc_dispatch(t, (vt_u8)c);
        return;

    case S_CSI_ENTRY:
    case S_CSI_PARAM:
        if (c >= '0' && c <= '9') {
            if (t->np == 0) {
                t->np = 1;
                t->params[0] = 0;
                t->sub[0] = 0;
            }
            if (t->params[t->np - 1] < VT_PARAM_MAX / 10)
                t->params[t->np - 1] = t->params[t->np - 1] * 10 + (long)(c - '0');
            else
                t->params[t->np - 1] = VT_PARAM_MAX;
            t->state = S_CSI_PARAM;
            return;
        }
        if (c == ';' || c == ':') {
            if (t->np == 0) {
                t->np = 1;
                t->params[0] = 0;
                t->sub[0] = 0;
            }
            if (t->np < VT_MAX_PARAMS) {
                t->params[t->np] = 0;
                t->sub[t->np] = (vt_u8)(c == ':');
                t->np++;
            }
            t->state = S_CSI_PARAM;
            return;
        }
        if (c >= 0x3C && c <= 0x3F) { /* < = > ? */
            if (t->state == S_CSI_ENTRY) {
                t->priv = (vt_u8)c;
            } else if (c == '>' && (t->pers == VT_AMIGA || t->csi8) && t->np &&
                       t->params[t->np - 1] == 0) {
                /* CSI 1;33;40;>0m: a '>' item after ';' is the global
                 * background colour (matrix 4.1). */
                t->sub[t->np - 1] = 2;
            } else {
                t->state = S_CSI_IGNORE;
            }
            return;
        }
        if (c >= 0x20 && c <= 0x2F) {
            t->inter = (vt_u8)c;
            t->ninter = 1;
            t->state = S_CSI_INT;
            return;
        }
        if (c >= 0x40 && c <= 0x7E) {
            t->state = S_GROUND;
            csi_dispatch(t, (vt_u8)c);
            return;
        }
        return; /* DEL */

    case S_CSI_INT:
        if (c >= 0x20 && c <= 0x2F) {
            t->ninter++;
            return;
        }
        if (c >= 0x40 && c <= 0x7E) {
            t->state = S_GROUND;
            if (t->ninter == 1)
                csi_dispatch(t, (vt_u8)c);
            return;
        }
        if (c >= 0x30 && c <= 0x3F)
            t->state = S_CSI_IGNORE;
        return;

    case S_CSI_IGNORE:
        if (c >= 0x40 && c <= 0x7E)
            t->state = S_GROUND;
        return;

    default:
        t->state = S_GROUND;
        return;
    }
}

/* Bytes to code points, per personality. */
static void decode(vt_term *t, vt_u8 b)
{
    if (t->pers == VT_AMIGA) {
        t->raw_c1 = 1;
        feed(t, b); /* Latin-1: the byte is the code point */
        t->raw_c1 = 0;
        return;
    }
    if (t->pers == VT_PCANSI) {
        if (b >= 0x80 && b != 0x9B)
            feed(t, cp437_hi[b - 0x80]);
        else
            feed(t, b);
        return;
    }
    if (t->cp437) {
        if (b == 0x9B) {
            t->raw_c1 = 1;
            feed(t, 0x9B);
            t->raw_c1 = 0;
        } else if (b >= 0x80) {
            feed(t, cp437_hi[b - 0x80]);
        } else {
            feed(t, b);
        }
        return;
    }
    if (!t->utf8) {
        /* Latin-1 xterm: bytes are code points, 80-9F are C1 */
        t->raw_c1 = b >= 0x80 && b <= 0x9F;
        feed(t, b);
        t->raw_c1 = 0;
        return;
    }
    /* UTF-8 */
    if (t->u_need) {
        if ((b & 0xC0) == 0x80) {
            t->u_cp = (t->u_cp << 6) | (b & 0x3F);
            if (--t->u_need == 0) {
                vt_u32 c = t->u_cp;
                if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
                    c = 0xFFFD;
                feed(t, c);
            }
            return;
        }
        t->u_need = 0;
        feed(t, 0xFFFD); /* truncated sequence; b starts afresh */
    }
    if (b < 0x80) {
        feed(t, b);
    } else if (b == 0x9B) {
        /* A lone $9B is not UTF-8. Amiga programs and ixemul send it as
         * the 8-bit CSI (ixemul asks the window size with 9B 20 71), so it
         * is taken as one, marked raw for csi_xterm. */
        t->raw_c1 = 1;
        feed(t, 0x9B);
        t->raw_c1 = 0;
    } else if ((b & 0xE0) == 0xC0 && b >= 0xC2) {
        t->u_cp = b & 0x1F;
        t->u_need = 1;
    } else if ((b & 0xF0) == 0xE0) {
        t->u_cp = b & 0x0F;
        t->u_need = 2;
    } else if ((b & 0xF8) == 0xF0 && b <= 0xF4) {
        t->u_cp = b & 0x07;
        t->u_need = 3;
    } else {
        feed(t, 0xFFFD);
    }
}

/* ---- public API -------------------------------------------------------------- */

static int alloc_screen(vt_line ***scr, int rows, int cols, const vt_term *t)
{
    int y;
    *scr = (vt_line **)VT_MALLOC(rows * sizeof(vt_line *));
    if (!*scr)
        return 0;
    for (y = 0; y < rows; y++) {
        (*scr)[y] = line_new(cols);
        if (!(*scr)[y])
            return 0;
        line_clear(t, (*scr)[y], cols);
    }
    return 1;
}

static void free_screen(vt_line **scr, int rows)
{
    int y;
    if (!scr)
        return;
    for (y = 0; y < rows; y++)
        if (scr[y])
            VT_FREE(scr[y]);
    VT_FREE(scr);
}

vt_term *vt_new(int cols, int rows, int scrollback, const vt_callbacks *cb, void *user)
{
    vt_term *t;
    int y;
    if (cols < 1 || rows < 1)
        return 0;
    t = (vt_term *)VT_MALLOC(sizeof(vt_term));
    if (!t)
        return 0;
    memset(t, 0, sizeof(*t));
    if (cb)
        t->cb = *cb;
    t->user = user;
    t->cols = cols;
    t->rows = rows;
    t->pers = VT_XTERM;
    t->tabs_cap = cols;
    t->tabs = (vt_u8 *)VT_MALLOC(cols);
    t->dx0 = (short *)VT_MALLOC(rows * sizeof(short));
    t->dx1 = (short *)VT_MALLOC(rows * sizeof(short));
    t->utf8 = 1;
    t->sb_cap = scrollback > 0 ? scrollback : 0;
    if (t->sb_cap)
        t->sb = (vt_line **)VT_MALLOC(t->sb_cap * sizeof(vt_line *));
    if (!t->tabs || !t->dx0 || !t->dx1 || (t->sb_cap && !t->sb) ||
        !alloc_screen(&t->pri, rows, cols, t) || !alloc_screen(&t->alt, rows, cols, t)) {
        vt_free(t);
        return 0;
    }
    for (y = 0; y < rows; y++) {
        t->dx0[y] = (short)cols;
        t->dx1[y] = 0;
    }
    t->scr = t->pri;
    vt_reset(t);
    return t;
}

void vt_free(vt_term *t)
{
    if (!t)
        return;
    free_screen(t->pri, t->rows);
    free_screen(t->alt, t->rows);
    while (t->sb_len) {
        t->sb_head = (t->sb_head + t->sb_cap - 1) % t->sb_cap;
        VT_FREE(t->sb[t->sb_head]);
        t->sb_len--;
    }
    if (t->sb)
        VT_FREE(t->sb);
    if (t->tabs)
        VT_FREE(t->tabs);
    if (t->dx0)
        VT_FREE(t->dx0);
    if (t->dx1)
        VT_FREE(t->dx1);
    VT_FREE(t);
}

void vt_reset(vt_term *t)
{
    t->scr = t->pri;
    t->modes = VT_MODE_CURSOR_VISIBLE;
    if (t->pers == VT_AMIGA)
        t->modes |= VT_MODE_NEWLINE; /* the console's LF starts a new line */
    t->amiga_dfg = VT_COLOR_DEFAULT;
    t->amiga_dbg = VT_COLOR_DEFAULT;
    t->amiga_dattr = 0;
    t->amiga_msb = 0;
    soft_reset(t);
    t->sav_1049 = t->sav;
    t->raw_events = 0;
    t->amiga_bg = 0;
    t->scroll_enabled = 1;
    t->state = S_GROUND;
    t->u_need = 0;
    t->last_ch = 0;
    t->title[0] = 0;
    tab_reset(t);
    set_alt(t, 0, 0);
    clear_screen_home(t);
    {
        vt_line **keep = t->scr;
        t->scr = t->alt;
        erase_rows(t, 0, t->rows);
        t->scr = keep;
    }
    mark_rows(t, 0, t->rows);
    flush(t);
}

void vt_set_personality(vt_term *t, enum vt_personality p)
{
    t->pers = p;
    vt_reset(t);
}

enum vt_personality vt_personality(const vt_term *t)
{
    return t->pers;
}

void vt_set_onlcr(vt_term *t, int on)
{
    t->onlcr = on != 0;
}

void vt_set_charset(vt_term *t, enum vt_charset cs)
{
    t->utf8 = cs == VT_CS_UTF8;
    t->cp437 = cs == VT_CS_CP437;
    t->u_need = 0;
}

/* Plain printable ASCII in the ground state, the bulk of all output, goes
 * straight into the row: no decoding, one dirty mark per run. Anything that
 * needs put_char's care (the last column and wrapping, wide cells being
 * overwritten, character sets, insert mode) takes the general path. */
static long put_ascii_run(vt_term *t, const vt_u8 *b, long n)
{
    vt_cell *c;
    long k, room;
    if (t->wrap_pending || t->cx >= t->cols - 1)
        return 0;
    room = t->cols - 1 - t->cx;
    if (n > room)
        n = room;
    c = &t->scr[t->cy]->c[t->cx];
    for (k = 0; k < n; k++) {
        if (c[k].width != 1 || (t->cx + k + 1 < t->cols && c[k + 1].width == 0))
            break; /* a wide glyph here: put_char unwides it */
        c[k].ch = b[k];
        c[k].fg = t->fg;
        c[k].bg = t->bg;
        c[k].attr = t->attr;
    }
    if (k) {
        mark(t, t->cx, t->cy, t->cx + (int)k);
        t->cx += (int)k;
        t->last_ch = b[k - 1];
    }
    return k;
}

void vt_write(vt_term *t, const vt_u8 *buf, long len)
{
    long i = 0;
    while (i < len) {
        vt_u8 b = buf[i];
        if (b >= 0x20 && b < 0x7F && t->state == S_GROUND && !t->u_need && !t->insert &&
            !t->single_shift && t->charset[t->gl] == 'B' && !t->amiga_msb) {
            long j = i + 1, k;
            while (j < len && buf[j] >= 0x20 && buf[j] < 0x7F)
                j++;
            k = put_ascii_run(t, buf + i, j - i);
            if (k) {
                i += k;
                continue;
            }
        }
        decode(t, b);
        i++;
    }
    flush(t);
}

static int resize_screen(vt_term *t, vt_line ***scrp, int cols, int rows, int is_pri, int *cy)
{
    vt_line **old = *scrp, **nw;
    int y, drop_top = 0, oy;
    nw = (vt_line **)VT_MALLOC(rows * sizeof(vt_line *));
    if (!nw)
        return 0;
    if (rows < t->rows) {
        /* Drop blank rows below the cursor first, then rows off the top. */
        int excess = t->rows - rows;
        int below = t->rows - 1 - *cy;
        int from_bottom = excess < below ? excess : below;
        drop_top = excess - from_bottom;
        for (y = t->rows - from_bottom; y < t->rows; y++)
            VT_FREE(old[y]);
        for (y = 0; y < drop_top; y++) {
            if (is_pri)
                sb_push(t, old[y]);
            else
                VT_FREE(old[y]);
        }
        *cy -= drop_top;
    }
    for (y = 0; y < rows; y++) {
        oy = y + drop_top;
        if (oy < t->rows && (rows >= t->rows || y < rows)) {
            vt_line *l = old[oy];
            if (l->cap < cols) {
                vt_line *nl = line_new(cols);
                if (!nl) {
                    VT_FREE(nw);
                    return 0;
                }
                memcpy(nl->c, l->c, l->n * sizeof(vt_cell));
                nl->n = l->n;
                nl->wrapped = l->wrapped;
                VT_FREE(l);
                l = nl;
            }
            if (cols > l->n)
                cells_blank(t, &l->c[l->n], cols - l->n);
            l->n = (vt_u16)cols;
            if (l->c[cols - 1].width == 2)
                blank_cell(t, &l->c[cols - 1]);
            nw[y] = l;
        } else {
            nw[y] = line_new(cols);
            if (!nw[y]) {
                VT_FREE(nw);
                return 0;
            }
            line_clear(t, nw[y], cols);
        }
    }
    VT_FREE(old);
    *scrp = nw;
    return 1;
}

void vt_resize(vt_term *t, int cols, int rows)
{
    int y, cy, alt_active, i;
    short *d0, *d1;
    vt_u8 *tabs;
    if (cols < 1 || rows < 1 || (cols == t->cols && rows == t->rows))
        return;
    flush(t);
    d0 = (short *)VT_MALLOC(rows * sizeof(short));
    d1 = (short *)VT_MALLOC(rows * sizeof(short));
    if (!d0 || !d1) {
        if (d0)
            VT_FREE(d0);
        if (d1)
            VT_FREE(d1);
        return;
    }
    if (cols > t->tabs_cap) {
        tabs = (vt_u8 *)VT_MALLOC(cols);
        if (!tabs) {
            VT_FREE(d0);
            VT_FREE(d1);
            return;
        }
        memcpy(tabs, t->tabs, t->tabs_cap);
        for (i = t->tabs_cap; i < cols; i++)
            tabs[i] = (vt_u8)((i % 8) == 0);
        VT_FREE(t->tabs);
        t->tabs = tabs;
        t->tabs_cap = cols;
    }
    alt_active = t->scr == t->alt;
    cy = alt_active ? t->sav_1049.y : t->cy;
    if (!alt_active)
        cy = t->cy;
    {
        int pcy = alt_active ? t->sav_1049.y : t->cy;
        int acy = alt_active ? t->cy : 0;
        resize_screen(t, &t->pri, cols, rows, 1, &pcy);
        resize_screen(t, &t->alt, cols, rows, 0, &acy);
        if (alt_active) {
            t->sav_1049.y = clampi(pcy, 0, rows - 1);
            cy = acy;
        } else {
            cy = pcy;
        }
    }
    t->scr = alt_active ? t->alt : t->pri;
    VT_FREE(t->dx0);
    VT_FREE(t->dx1);
    t->dx0 = d0;
    t->dx1 = d1;
    t->cols = cols;
    t->rows = rows;
    for (y = 0; y < rows; y++) {
        t->dx0[y] = (short)cols;
        t->dx1[y] = 0;
    }
    t->top = 0;
    t->bot = rows;
    t->cx = clampi(t->cx, 0, cols - 1);
    t->cy = clampi(cy, 0, rows - 1);
    t->wrap_pending = 0;
    mark_rows(t, 0, rows);
    flush(t);
}

int vt_cols(const vt_term *t)
{
    return t->cols;
}

int vt_rows(const vt_term *t)
{
    return t->rows;
}

const vt_cell *vt_row(const vt_term *t, int row, int *ncells)
{
    vt_line *l;
    if (row >= 0) {
        if (row >= t->rows)
            return 0;
        l = t->scr[row];
    } else {
        if (-row > t->sb_len)
            return 0;
        l = t->sb[(t->sb_head + t->sb_cap + row) % t->sb_cap];
    }
    if (ncells)
        *ncells = row >= 0 ? t->cols : l->n;
    return l->c;
}

int vt_row_wrapped(const vt_term *t, int row)
{
    if (row >= 0)
        return row < t->rows ? t->scr[row]->wrapped : 0;
    if (-row > t->sb_len)
        return 0;
    return t->sb[(t->sb_head + t->sb_cap + row) % t->sb_cap]->wrapped;
}

int vt_scrollback_lines(const vt_term *t)
{
    return t->sb_len;
}

long vt_lines_scrolled(const vt_term *t)
{
    return t->scrolled;
}

void vt_cursor(const vt_term *t, int *x, int *y)
{
    if (x)
        *x = t->cx;
    if (y)
        *y = t->cy;
}

vt_u32 vt_modes(const vt_term *t)
{
    return t->modes;
}

const char *vt_title(const vt_term *t)
{
    return t->title;
}

vt_u32 vt_raw_events(const vt_term *t)
{
    return t->raw_events;
}

long vt_copy_text(const vt_term *t, int ax, int ay, int bx, int by, char *out, long max)
{
    long len = 0;
    int y;
    if (max < 1)
        return 0;
    if (ay > by || (ay == by && ax > bx)) {
        int tx = ax, ty = ay;
        ax = bx;
        ay = by;
        bx = tx;
        by = ty;
    }
    for (y = ay; y <= by; y++) {
        int n, x, x0, x1, end;
        const vt_cell *c = vt_row(t, y, &n);
        if (!c)
            continue;
        x0 = y == ay ? ax : 0;
        x1 = y == by ? bx : n - 1;
        if (x1 > n - 1)
            x1 = n - 1;
        end = x1;
        if (!(y < by && vt_row_wrapped(t, y)))
            while (end >= x0 && c[end].ch == ' ')
                end--; /* trailing blanks of a line that ends here */
        for (x = x0; x <= end; x++) {
            vt_u8 u[4];
            int k, m;
            if (c[x].width == 0)
                continue;
            m = put_utf8(u, c[x].ch);
            if (len + m >= max)
                goto done;
            for (k = 0; k < m; k++)
                out[len++] = (char)u[k];
        }
        if (y < by && !vt_row_wrapped(t, y)) {
            if (len + 1 >= max)
                goto done;
            out[len++] = '\n';
        }
    }
done:
    out[len] = 0;
    return len;
}

long vt_unhandled(const vt_term *t, const char **kinds, long *counts, int max)
{
    int i;
    for (i = 0; i < t->unhandled_kinds && i < max; i++) {
        kinds[i] = t->unhandled_seen[i];
        counts[i] = t->unhandled_count[i];
    }
    for (; i < max; i++) {
        kinds[i] = 0;
        counts[i] = 0;
    }
    return t->unhandled;
}

void vt_resolve_colors(const vt_term *t, const vt_cell *c, vt_u16 *fg, vt_u16 *bg)
{
    vt_u16 f = c->fg, b = c->bg, tmp;
    switch (t->pers) {
    case VT_AMIGA:
        /* Pens are screen pens; bold is a font style, not a colour. */
        if (f == VT_COLOR_DEFAULT)
            f = 1;
        if (b == VT_COLOR_DEFAULT)
            b = t->amiga_bg;
        break;
    case VT_PCANSI:
        if (f == VT_COLOR_DEFAULT)
            f = 7;
        if (b == VT_COLOR_DEFAULT)
            b = 0;
        if ((c->attr & VT_ATTR_BOLD) && f < 8)
            f = (vt_u16)(f + 8);
        if ((c->attr & VT_ATTR_BLINK) && b < 8)
            b = (vt_u16)(b + 8); /* iCE colours */
        break;
    default:
        if ((c->attr & VT_ATTR_BOLD) && f < 8)
            f = (vt_u16)(f + 8);
        if (b == VT_COLOR_DEFAULT)
            b = VT_COLOR_DEFAULT_BG;
        break;
    }
    if (!(c->attr & VT_ATTR_INVERSE) != !(t->modes & VT_MODE_SCREEN_REVERSE)) {
        tmp = f;
        f = b;
        b = tmp;
    }
    if (c->attr & VT_ATTR_CONCEAL)
        f = b;
    *fg = f;
    *bg = b;
}

/* ---- keys ----------------------------------------------------------------- */

static int put_utf8(vt_u8 *o, long c)
{
    if (c < 0x80) {
        o[0] = (vt_u8)c;
        return 1;
    }
    if (c < 0x800) {
        o[0] = (vt_u8)(0xC0 | (c >> 6));
        o[1] = (vt_u8)(0x80 | (c & 0x3F));
        return 2;
    }
    if (c < 0x10000) {
        o[0] = (vt_u8)(0xE0 | (c >> 12));
        o[1] = (vt_u8)(0x80 | ((c >> 6) & 0x3F));
        o[2] = (vt_u8)(0x80 | (c & 0x3F));
        return 3;
    }
    o[0] = (vt_u8)(0xF0 | (c >> 18));
    o[1] = (vt_u8)(0x80 | ((c >> 12) & 0x3F));
    o[2] = (vt_u8)(0x80 | ((c >> 6) & 0x3F));
    o[3] = (vt_u8)(0x80 | (c & 0x3F));
    return 4;
}

static int cp437_encode(long c)
{
    int i;
    if (c < 0x80)
        return (int)c;
    for (i = 0; i < 128; i++)
        if (cp437_hi[i] == c)
            return 0x80 + i;
    return '?';
}

/* xterm: CSI 1 ; m final (or SS3 final unmodified in application mode). */
static int xterm_cursor(vt_u8 *o, vt_u8 final, int mods, int app)
{
    int n = 0;
    if (mods) {
        o[n++] = 0x1B;
        o[n++] = '[';
        o[n++] = '1';
        o[n++] = ';';
        o[n++] = (vt_u8)('1' + mods);
    } else {
        o[n++] = 0x1B;
        o[n++] = (vt_u8)(app ? 'O' : '[');
    }
    o[n++] = final;
    return n;
}

static int xterm_tilde(vt_u8 *o, int code, int mods)
{
    char b[16];
    int n = 0, i;
    b[n++] = 0x1B;
    b[n++] = '[';
    n = fmt_uint(b, n, code);
    if (mods) {
        b[n++] = ';';
        b[n++] = (char)('1' + mods);
    }
    b[n++] = '~';
    for (i = 0; i < n; i++)
        o[i] = (vt_u8)b[i];
    return n;
}

static int amiga_key(vt_u8 *o, long key, int mods)
{
    /* RKM Devices: special key reports use the 8-bit CSI. */
    static const char *const fk[10] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
    int n = 0, sh = (mods & VT_MOD_SHIFT) != 0;
    o[n++] = 0x9B;
    switch (key) {
    case VT_KEY_UP:
        o[n++] = (vt_u8)(sh ? 'T' : 'A');
        return n;
    case VT_KEY_DOWN:
        o[n++] = (vt_u8)(sh ? 'S' : 'B');
        return n;
    case VT_KEY_RIGHT:
        if (sh) {
            o[n++] = ' ';
            o[n++] = '@';
        } else {
            o[n++] = 'C';
        }
        return n;
    case VT_KEY_LEFT:
        if (sh) {
            o[n++] = ' ';
            o[n++] = 'A';
        } else {
            o[n++] = 'D';
        }
        return n;
    case VT_KEY_HELP:
        o[n++] = '?';
        o[n++] = '~';
        return n;
    default:
        break;
    }
    if (key >= VT_KEY_F1 && key <= VT_KEY_F10) {
        const char *d = fk[key - VT_KEY_F1];
        if (sh)
            o[n++] = '1';
        o[n++] = (vt_u8)d[0];
        o[n++] = '~';
        return n;
    }
    {
        /* 101-key keyboards (matrix 5.3): F11 20~, F12 21~ (shifted 30/31),
         * Insert 40~, PgUp 41~, PgDn 42~, Home 44~, End 45~ (shifted +10). */
        int code = key == VT_KEY_F11 ? 20 : key == VT_KEY_F12 ? 21 : key == VT_KEY_INSERT ? 40
                 : key == VT_KEY_PAGE_UP ? 41 : key == VT_KEY_PAGE_DOWN ? 42
                 : key == VT_KEY_HOME ? 44 : key == VT_KEY_END ? 45 : 0;
        if (code) {
            if (sh)
                code += 10;
            o[n++] = (vt_u8)('0' + code / 10);
            o[n++] = (vt_u8)('0' + code % 10);
            o[n++] = '~';
            return n;
        }
    }
    return 0;
}

int vt_encode_key(const vt_term *t, long key, int mods, vt_u8 *out)
{
    int n = 0;
    int app = (t->modes & VT_MODE_APP_CURSOR) != 0;

    if (key < 0x110000) { /* a character */
        long c = key;
        if (mods & VT_MOD_CTRL) {
            if (c >= 'a' && c <= 'z')
                c -= 0x60;
            else if (c >= '@' && c <= '_')
                c -= 0x40;
            else if (c == ' ')
                c = 0;
            else if (c == '?')
                c = 0x7F;
        }
        if ((mods & VT_MOD_ALT) && t->pers != VT_AMIGA)
            out[n++] = 0x1B;
        if (t->pers == VT_XTERM && t->utf8)
            n += put_utf8(out + n, c);
        else if (t->pers == VT_XTERM && t->cp437)
            out[n++] = (vt_u8)cp437_encode(c);
        else if (t->pers == VT_PCANSI)
            out[n++] = (vt_u8)cp437_encode(c);
        else
            out[n++] = (vt_u8)(c > 0xFF ? '?' : c);
        return n;
    }

    switch (key) {
    case VT_KEY_RETURN:
        out[n++] = '\r';
        if (t->pers != VT_AMIGA && (t->modes & VT_MODE_NEWLINE))
            out[n++] = '\n';
        return n;
    case VT_KEY_KP_ENTER:
        if (t->pers == VT_XTERM && (t->modes & VT_MODE_APP_KEYPAD)) {
            out[n++] = 0x1B;
            out[n++] = 'O';
            out[n++] = 'M';
            return n;
        }
        out[n++] = '\r';
        return n;
    case VT_KEY_BACKSPACE:
        out[n++] = (vt_u8)(t->pers == VT_XTERM ? 0x7F : 0x08);
        return n;
    case VT_KEY_TAB:
        if ((mods & VT_MOD_SHIFT) && t->pers == VT_AMIGA) {
            out[n++] = 0x9B; /* the usa keymap's Shift+Tab */
            out[n++] = 'Z';
            return n;
        }
        if (mods & VT_MOD_SHIFT) {
            out[n++] = 0x1B;
            out[n++] = '[';
            out[n++] = 'Z';
            return n;
        }
        out[n++] = '\t';
        return n;
    case VT_KEY_ESCAPE:
        out[n++] = 0x1B;
        return n;
    default:
        break;
    }

    if (key >= VT_KEY_KP_0 && key <= VT_KEY_KP_RPAREN) {
        static const char plain[] = "0123456789.-+*/()";
        static const char ss3[] = "pqrstuvwxynmkjo";
        int k = (int)(key - VT_KEY_KP_0);
        if (t->pers == VT_XTERM && (t->modes & VT_MODE_APP_KEYPAD) && k < 15) {
            out[n++] = 0x1B;
            out[n++] = 'O';
            out[n++] = (vt_u8)ss3[k];
            return n;
        }
        out[n++] = (vt_u8)plain[k];
        return n;
    }

    if (t->pers == VT_AMIGA) {
        if (key == VT_KEY_DELETE) {
            out[n++] = 0x7F;
            return n;
        }
        return amiga_key(out, key, mods);
    }

    if (t->pers == VT_PCANSI)
        mods = 0, app = 0; /* BBS software knows the plain forms only */

    switch (key) {
    case VT_KEY_UP:
        return xterm_cursor(out, 'A', mods, app);
    case VT_KEY_DOWN:
        return xterm_cursor(out, 'B', mods, app);
    case VT_KEY_RIGHT:
        return xterm_cursor(out, 'C', mods, app);
    case VT_KEY_LEFT:
        return xterm_cursor(out, 'D', mods, app);
    case VT_KEY_HOME:
        return xterm_cursor(out, 'H', mods, app);
    case VT_KEY_END:
        return xterm_cursor(out, 'F', mods, app);
    case VT_KEY_INSERT:
        return xterm_tilde(out, 2, mods);
    case VT_KEY_DELETE:
        if (t->pers == VT_PCANSI) {
            out[n++] = 0x7F;
            return n;
        }
        return xterm_tilde(out, 3, mods);
    case VT_KEY_PAGE_UP:
        return xterm_tilde(out, 5, mods);
    case VT_KEY_PAGE_DOWN:
        return xterm_tilde(out, 6, mods);
    case VT_KEY_HELP:
        return xterm_tilde(out, 28, mods);
    case VT_KEY_F1:
    case VT_KEY_F2:
    case VT_KEY_F3:
    case VT_KEY_F4:
        if (mods) {
            n = xterm_cursor(out, (vt_u8)('P' + (key - VT_KEY_F1)), mods, 0);
            return n;
        }
        out[n++] = 0x1B;
        out[n++] = 'O';
        out[n++] = (vt_u8)('P' + (key - VT_KEY_F1));
        return n;
    default:
        break;
    }
    if (key >= VT_KEY_F5 && key <= VT_KEY_F12) {
        static const int code[8] = { 15, 17, 18, 19, 20, 21, 23, 24 };
        return xterm_tilde(out, code[key - VT_KEY_F5], mods);
    }
    return 0;
}

int vt_encode_mouse(const vt_term *t, int button, int kind, int x, int y, int mods, vt_u8 *out)
{
    vt_u32 m = t->modes;
    int cb, n = 0, i;
    char b[32];
    if (t->pers != VT_XTERM)
        return 0;
    if (!(m & (VT_MODE_MOUSE_X10 | VT_MODE_MOUSE_NORMAL | VT_MODE_MOUSE_BUTTON | VT_MODE_MOUSE_ANY)))
        return 0;
    if (kind == 2 && !(m & VT_MODE_MOUSE_ANY) && !(m & VT_MODE_MOUSE_BUTTON))
        return 0;
    if (kind == 1 && (m & VT_MODE_MOUSE_X10) && !(m & (VT_MODE_MOUSE_NORMAL | VT_MODE_MOUSE_BUTTON |
                                                      VT_MODE_MOUSE_ANY)))
        return 0; /* X10 reports presses only */
    if (button >= 64 && kind != 0)
        return 0; /* the wheel has no release */
    cb = button;
    if (kind == 1 && !(m & VT_MODE_MOUSE_SGR))
        cb = 3; /* the legacy form cannot say which button went up */
    if (kind == 2)
        cb += 32;
    if (!(m & VT_MODE_MOUSE_X10) || (m & (VT_MODE_MOUSE_NORMAL | VT_MODE_MOUSE_BUTTON | VT_MODE_MOUSE_ANY))) {
        if (mods & VT_MOD_SHIFT)
            cb += 4;
        if (mods & VT_MOD_ALT)
            cb += 8;
        if (mods & VT_MOD_CTRL)
            cb += 16;
    }
    b[n++] = 0x1B;
    b[n++] = '[';
    if (m & VT_MODE_MOUSE_SGR) {
        b[n++] = '<';
        n = fmt_uint(b, n, cb);
        b[n++] = ';';
        n = fmt_uint(b, n, x + 1);
        b[n++] = ';';
        n = fmt_uint(b, n, y + 1);
        b[n++] = (char)(kind == 1 ? 'm' : 'M');
    } else {
        if (x > 222 || y > 222)
            return 0; /* beyond what one byte can carry */
        b[n++] = 'M';
        b[n++] = (char)(32 + cb);
        b[n++] = (char)(33 + x);
        b[n++] = (char)(33 + y);
    }
    for (i = 0; i < n; i++)
        out[i] = (vt_u8)b[i];
    return n;
}

int vt_encode_paste(const vt_term *t, int end, vt_u8 *out)
{
    const char *s = end ? "\033[201~" : "\033[200~";
    int i;
    if (!(t->modes & VT_MODE_BRACKET_PASTE) || t->pers != VT_XTERM)
        return 0;
    for (i = 0; s[i]; i++)
        out[i] = (vt_u8)s[i];
    return i;
}
