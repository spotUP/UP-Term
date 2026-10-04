/* vtengine -- see vtengine.h. The parser is Paul Williams' DEC-compatible
 * state machine (vt100.net/emu/dec_ansi_parser); the dispatch tables below
 * it are per personality, because the Amiga console and xterm give the same
 * CSI finals different meanings (the conformance matrix in
 * thoughts/shared/research/2026-09-28_console-conformance-matrix.md). */
#include "vtengine.h"
#include "vtwidth.h"
#include <stddef.h>
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
    vt_u8 dbl;       /* DEC line size: 0 single, VT_LINE_DOUBLE_WIDTH, _TOP, _BOTTOM */
    vt_u16 used;     /* cells [used, n) are still the default blank the last
                      * line_clear wrote: clearing again need not touch them.
                      * mark() -- every change to a grid cell passes it --
                      * raises it; a line whose cells are not known (new, or
                      * cleared in a colour) has used == cap. (S1: a scroll
                      * cleared 80 cells a line, 0.69 ms on a 14 MHz 68020.) */
    short dx0, dx1;  /* the row's cells [dx0, dx1) changed since the last flush
                      * (empty when dx0 >= dx1). Kept in the line: a scroll
                      * moves the lines, and their spans with them. */
    vt_cell c[1];
} vt_line;

typedef struct vt_saved {
    int x, y, wrap_pending, origin;
    vt_color fg, bg, ul;
    vt_attr attr;
    vt_u8 deco, font;
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
    vt_color fg, bg;
    vt_attr attr;
    vt_u8 deco;            /* VT_DECO_* for the next cells */
    vt_color ul;           /* SGR 58 underline colour, VT_COLOR_DEFAULT = the text's */
    vt_u8 font;            /* SGR 10-20 */
    vt_u8 ext;             /* the rare-style entry of ul + font, 0 = none (style_index) */
    struct { vt_color ul; vt_u8 font; } styles[255];
    int n_styles;
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
    vt_color amiga_bg;       /* global background pen (SGR >n) */
    vt_color amiga_dfg, amiga_dbg; /* SGR 0 / 39 / 49 defaults (CSI SP s, V39) */
    vt_attr amiga_dattr;
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
    int reflow;                /* vt_resize re-wraps wrapped lines (vt_set_reflow) */
    int tab_end;               /* amiga: the last byte was a TAB that ended at the last column */
    int bold_bright;           /* xterm: SGR 1 takes the bright 8-15 (vt_set_bold_bright) */
    /* reflow: rows a resize pushed off the top of the primary screen, oldest
     * first, all t->cols wide. The next reflow lays them out again with the
     * screen (the ROM console brings them back on a grow); output that
     * scrolls makes them history (ovf_drop). */
    vt_line **ovf;
    int novf, ovf_cap;

    /* per-row dirty spans, flushed at the end of each write */
    int dirty;
    int full_y0, full_y1;      /* rows damaged whole since the last flush (damage_rows) */
    /* A scroll the renderer has not been told about yet: the pixels are
     * pend_n rows behind the grid in [pend_top, pend_bot) (negative: down).
     * All scrolls of one write become one blit; the dirty spans move with
     * the rows, so after flush() the pixels equal the grid. */
    int pend_n, pend_top, pend_bot;

    char title[VT_STR_MAX];
    char title_stack[4][VT_STR_MAX]; /* CSI 22 t / 23 t */
    int n_titles;
    vt_u8 str_bel;             /* the string ended with BEL: answer with BEL */
    vt_u32 pal_set[256];       /* OSC 4: 0x01RRGGBB, 0 = the default entry */
    vt_u32 dflt[3];            /* the host's default text, background, cursor */
    vt_u32 dflt_set[3];        /* OSC 10-12: 0x01RRGGBB, 0 = the host's */
    int cell_w, cell_h;        /* pixels, for CSI 14t / 16t */
    vt_u8 cursor_style;        /* DECSCUSR */
    vt_u8 allow_cols;          /* ?40: DECCOLM may change the width */
    vt_u8 mok;                 /* modifyOtherKeys level, CSI > 4 ; n m */
    vt_u8 scheme;              /* the last dark (1) / light (2) scheme reported */

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
        l->used = (vt_u16)cap;
        l->dx0 = 0x7FFF;
        l->dx1 = 0;
        l->n = 0;
        l->wrapped = 0;
        l->dbl = 0;
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
    c->deco = 0;
    c->ext = 0;
    c->pad = 0;
    if (t->pers == VT_PCANSI) {
        /* ANSI.SYS erases with the whole attribute byte: an erased cell
         * is a space in the current colours, so blink (the iCE bright
         * background) and inverse (fg and bg swapped) count too. */
        c->attr = (vt_attr)(t->attr & VT_ATTR_BLINK);
        if (t->attr & VT_ATTR_INVERSE) {
            /* the foreground is visible only when swapped in; otherwise
             * the blank stays canonical (vacated_default) */
            c->fg = t->fg;
            c->attr |= (vt_attr)(t->attr & (VT_ATTR_BOLD | VT_ATTR_INVERSE));
        }
    }
    c->width = 1;
}

#ifdef VT_ASM
long vt_asm_put_run(vt_cell *c, const vt_u8 *b, long n, const vt_cell *proto);
void vt_asm_fill(vt_cell *c, long n, const vt_cell *proto);
void vt_asm_cells_move(vt_cell *dst, const vt_cell *src, long n);
void vt_asm_rows_up(struct vt_line **p, long k);
void vt_asm_rows_down(struct vt_line **p, long k);
/* vtengine_68k.s knows vt_cell by its offsets: a negative array size here
 * when they change */
typedef char vt_asm_layout[(sizeof(vt_cell) == 16 && offsetof(vt_cell, ch) == 8 && offsetof(vt_cell, attr) == 10 &&
                            offsetof(vt_cell, width) == 12 && offsetof(vt_cell, pad) == 15) ? 1 : -1];
#endif

static void cells_blank(const vt_term *t, vt_cell *c, int n)
{
    vt_cell b;
    blank_cell(t, &b);
#ifdef VT_ASM
    vt_asm_fill(c, n, &b);
#else
    {
        int i;
        for (i = 0; i < n; i++)
            c[i] = b;
    }
#endif
}

static int vacated_default(const vt_term *t);

static void line_clear(const vt_term *t, vt_line *l, int n)
{
    if (l->n == n && l->used < n && vacated_default(t)) {
        /* the same width as its last clear, and the blank is the default
         * one: only the cells written since need it */
#ifdef VT_CHECK_USED
        int i;
        vt_cell b;
        blank_cell(t, &b);
        for (i = l->used; i < n; i++)
            if (l->c[i].ch != b.ch || l->c[i].fg != b.fg || l->c[i].bg != b.bg || l->c[i].attr != b.attr ||
                l->c[i].width != b.width || l->c[i].deco != b.deco || l->c[i].ext != b.ext)
                abort(); /* a cell changed without mark(): host tests only */
#endif
        cells_blank(t, l->c, l->used);
    } else {
        cells_blank(t, l->c, n);
    }
    l->used = (vt_u16)(vacated_default(t) ? 0 : n);
    l->n = (vt_u16)n;
    l->wrapped = 0;
    l->dbl = 0;
}

/* The columns row y holds: half on a double-width or -height line. */
static int row_cols(const vt_term *t, int y)
{
    return y >= 0 && y < t->rows && t->scr[y]->dbl ? (t->cols + 1) / 2 : t->cols;
}

static void mark(vt_term *t, int x0, int y, int x1)
{
    if (y < 0 || y >= t->rows)
        return;
    x0 = clampi(x0, 0, t->cols);
    x1 = clampi(x1, 0, t->cols);
    if (x0 >= x1)
        return;
    {
        vt_line *l = t->scr[y];
        if (l->dx0 > x0)
            l->dx0 = (short)x0;
        if (l->dx1 < x1)
            l->dx1 = (short)x1;
        if (l->used < x1)
            l->used = (vt_u16)x1;
    }
    t->dirty = 1;
}

/* Rows [y0, y1) are to be drawn again, their cells unchanged (a scroll's
 * bookkeeping): damage only, the lines' `used` stays. full_y0 / full_y1
 * remember the widest such range since the last flush: a scroll inside it
 * has nothing to track, every row there is drawn whole anyway. */
static void damage_rows(vt_term *t, int y0, int y1)
{
    int y;
    if (y0 < 0)
        y0 = 0;
    if (y1 > t->rows)
        y1 = t->rows;
    if (y0 >= y1)
        return;
    for (y = y0; y < y1; y++) {
        t->scr[y]->dx0 = 0;
        t->scr[y]->dx1 = (short)t->cols;
    }
    if (t->full_y0 >= t->full_y1 || (y0 <= t->full_y0 && y1 >= t->full_y1)) {
        t->full_y0 = y0;
        t->full_y1 = y1;
    }
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
    t->full_y0 = t->full_y1 = 0;
    y = 0;
    while (y < t->rows) {
        if (t->scr[y]->dx0 >= t->scr[y]->dx1) {
            y++;
            continue;
        }
        y0 = y;
        while (y + 1 < t->rows && t->scr[y + 1]->dx0 == t->scr[y0]->dx0 && t->scr[y + 1]->dx1 == t->scr[y0]->dx1)
            y++;
        if (t->cb.damage)
            t->cb.damage(t->user, t->scr[y0]->dx0, y0, t->scr[y0]->dx1, y + 1);
        for (; y0 <= y; y0++) {
            t->scr[y0]->dx0 = 0x7FFF;
            t->scr[y0]->dx1 = 0;
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
    return b.bg == VT_COLOR_DEFAULT && b.fg == VT_COLOR_DEFAULT && !b.attr;
}

static void damage_rows(vt_term *t, int y0, int y1);

/* Before the grid moves: a pending scroll of another region or direction
 * cannot be added to. Its rows are drawn again from the grid at the flush
 * instead of being moved on screen -- no drawing in the middle of a
 * write: an insert-line / delete-line pair was two blits of its own each
 * time (conbench insdel-line, 9.5 ms a pair on the stock rig; S1). */
static void pend_prepare(vt_term *t, int top, int bot, int n)
{
    if (t->cb.scroll && t->pend_n &&
        (t->pend_top != top || t->pend_bot != bot || (t->pend_n > 0) != (n > 0))) {
        t->pend_n = 0;
        damage_rows(t, t->pend_top, t->pend_bot);
    }
}

/* The rows [top, bot) moved by n (up when n > 0): move their dirty spans
 * with them and remember the pixels owe that scroll (see pend_n). */
static void pend_scroll(vt_term *t, int top, int bot, int n)
{
    int h = bot - top;
    if (!t->cb.scroll) {
        damage_rows(t, top, bot); /* no blitting renderer: redraw the region */
        return;
    }
    if (!t->pend_n && top >= t->full_y0 && bot <= t->full_y1) {
        /* the region is drawn whole at the flush: nothing moves on screen.
         * The lines that came in are clean (scroll_up / scroll_down), and
         * their rows are drawn whole like the others. */
        if (n > 0)
            damage_rows(t, bot - n < top ? top : bot - n, bot);
        else
            damage_rows(t, top, top - n > bot ? bot : top - n);
        return;
    }
    /* the rows' dirty spans went with their lines (vt_line.dx0), the rows
     * that came in start clean (scroll_up / scroll_down) */
    if (n > 0) {
        if (!vacated_default(t))
            damage_rows(t, bot - n, bot);
    } else {
        if (!vacated_default(t))
            damage_rows(t, top, top - n);
    }
    t->pend_top = top;
    t->pend_bot = bot;
    t->pend_n += n;
    if (t->pend_n >= h || -t->pend_n >= h) {
        t->pend_n = 0; /* everything moved out: redraw instead of blitting */
        damage_rows(t, top, bot);
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

/* The rows a resize pushed out stop being part of the screen: into the
 * scrollback where the personality keeps one (amiga keeps none, as the
 * console), else gone. */
static void ovf_drop(vt_term *t)
{
    int i;
    for (i = 0; i < t->novf; i++) {
        if (t->pers != VT_AMIGA)
            sb_push(t, t->ovf[i]);
        else
            VT_FREE(t->ovf[i]);
    }
    t->novf = 0;
}

#define VT_OVF_MAX 500 /* rows kept above the screen for a later grow */

/* A row a reflow pushed off the top, kept for the next one. */
static void ovf_push(vt_term *t, vt_line *l)
{
    if (t->novf == t->ovf_cap) {
        int cap = t->ovf_cap ? t->ovf_cap * 2 : 16;
        vt_line **n;
        if (cap > VT_OVF_MAX)
            cap = VT_OVF_MAX;
        if (t->novf == cap) { /* full: the oldest becomes history */
            if (t->pers != VT_AMIGA)
                sb_push(t, t->ovf[0]);
            else
                VT_FREE(t->ovf[0]);
            memmove(t->ovf, t->ovf + 1, (t->novf - 1) * sizeof(vt_line *));
            t->novf--;
        } else {
            n = (vt_line **)VT_MALLOC(cap * sizeof(vt_line *));
            if (!n) {
                sb_push(t, l);
                return;
            }
            if (t->novf)
                memcpy(n, t->ovf, t->novf * sizeof(vt_line *));
            if (t->ovf)
                VT_FREE(t->ovf);
            t->ovf = n;
            t->ovf_cap = cap;
        }
    }
    t->ovf[t->novf++] = l;
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
    if (top == 0 && t->scr == t->pri) {
        t->scrolled += n;
        ovf_drop(t); /* above the rows scrolling out: history first */
    }
    for (i = 0; i < n; i++) {
        vt_line **p = &t->scr[top];
        l = *p;
#ifdef VT_ASM
        vt_asm_rows_up(p, h - 1);
#else
        {
            int k;
            for (k = h - 1; k > 0; k--, p++)
                p[0] = p[1];
        }
#endif
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
        l->dx0 = 0x7FFF;
        l->dx1 = 0;
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
        vt_line **p = &t->scr[bot - 1];
        l = *p;
#ifdef VT_ASM
        vt_asm_rows_down(p, h - 1);
#else
        {
            int k;
            for (k = h - 1; k > 0; k--, p--)
                p[0] = p[-1];
        }
#endif
        line_clear(t, l, t->cols);
        l->dx0 = 0x7FFF;
        l->dx1 = 0;
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
    /* erased to the end of the row with the default blank: from x0 on
     * the row is unused again (see vt_line.used) */
    if (x1 == t->cols && t->scr[y]->used > x0 && vacated_default(t))
        t->scr[y]->used = (vt_u16)x0;
}

static void erase_rows(vt_term *t, int y0, int y1)
{
    int y;
    for (y = y0; y < y1; y++) {
        erase_cells(t, y, 0, t->cols);
        t->scr[y]->dbl = 0; /* an erased line is single size again (VT100) */
    }
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
    t->cy = clampi(y, y0, y1);
    t->cx = clampi(x, 0, row_cols(t, t->cy) - 1);
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

/* The Amiga console moves the cursor as one line of text through the
 * window (measured on the ROM console, tests/probes): back past column 1
 * into the row above, forward past the edge into the rows below, stopping
 * at the top and bottom rows without scrolling. */
static void amiga_move_linear(vt_term *t, long d)
{
    long pos = (long)t->cy * t->cols + t->cx + d;
    long row = pos >= 0 ? pos / t->cols : -((-pos + t->cols - 1) / t->cols);
    long col = pos - row * t->cols;
    if (row < 0)
        row = 0;
    if (row > t->rows - 1)
        row = t->rows - 1;
    t->cy = (int)row;
    t->cx = (int)col;
    t->wrap_pending = 0;
}

static void tab_forward(vt_term *t, int n)
{
    while (n-- > 0) {
        int x = t->cx + 1;
        if (t->pers == VT_AMIGA && t->cx >= t->cols - 1 && t->tab_end) {
            /* the ROM console: a tab at the last column goes on to the
             * next line's first tab stop -- when a tab took the cursor
             * there; placed there (a CUP past the edge) the first tab
             * stays (3.2.3 and 3.1: CSI 1;75H TAB TAB in 61 columns ends
             * at 2;9, in 80 columns too) */
            t->cx = 0;
            index_down(t);
            x = 1;
        }
        while (x < row_cols(t, t->cy) - 1 && !t->tabs[x])
            x++;
        t->cx = clampi(x, 0, row_cols(t, t->cy) - 1);
        t->tab_end = t->cx >= t->cols - 1;
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

/* n cells from src to dst, overlapping or not */
static void cells_move(vt_cell *dst, const vt_cell *src, int n)
{
    if (n <= 0)
        return;
#ifdef VT_ASM
    vt_asm_cells_move(dst, src, n);
#else
    memmove(dst, src, (size_t)n * sizeof(vt_cell));
#endif
}

/* The default blank (not the current erase colour): what the cells past a
 * line's `used` hold. */
static void default_cells(vt_cell *c, int n)
{
    vt_cell b;
    int i;
    b.ch = ' ';
    b.fg = VT_COLOR_DEFAULT;
    b.bg = VT_COLOR_DEFAULT;
    b.attr = 0;
    b.width = 1;
    b.deco = 0;
    b.ext = 0;
    b.pad = 0;
    for (i = 0; i < n; i++)
        c[i] = b;
}

/* Insert and delete character move the row's tail. Only its cells in use
 * (vt_line.used) need moving and drawing: past them the row is default
 * blanks before and after. (conbench insdel-char: the whole tail moved
 * through memmove and was drawn again, 4 ms an operation on the stock
 * rig; S1.) */
static void insert_chars(vt_term *t, int n)
{
    vt_line *l = t->scr[t->cy];
    vt_cell *c = l->c;
    int x = t->cx, k, u, end;
    n = clampi(n, 1, t->cols - x);
    unwide(t, x, t->cy);
    t->wrap_pending = 0;
    u = l->used < t->cols ? l->used : t->cols;
    if (u <= x) {
        /* default blanks from the cursor on: only an erase colour shows */
        if (!vacated_default(t)) {
            cells_blank(t, &c[x], n);
            mark(t, x, t->cy, x + n);
        }
        return;
    }
    k = (u < t->cols - n ? u : t->cols - n) - x; /* the cells in use that stay on the row */
    cells_move(&c[x + n], &c[x], k);
    cells_blank(t, &c[x], n);
    if (c[t->cols - 1].width == 2)
        blank_cell(t, &c[t->cols - 1]);
    end = u + n < t->cols ? u + n : t->cols;
    mark(t, x, t->cy, end);
}

static void delete_chars(vt_term *t, int n)
{
    vt_line *l = t->scr[t->cy];
    vt_cell *c = l->c;
    int x = t->cx, k, u, from, dflt = vacated_default(t);
    n = clampi(n, 1, t->cols - x);
    unwide(t, x, t->cy);
    if (x + n < t->cols)
        unwide(t, x + n, t->cy);
    t->wrap_pending = 0;
    u = l->used < t->cols ? l->used : t->cols;
    if (u > x) {
        k = u - x - n; /* the cells in use right of the deleted ones */
        cells_move(&c[x], &c[x + n], k);
        from = x + (k > 0 ? k : 0);
        default_cells(&c[from], u - from); /* default blanks came in behind them */
        mark(t, x, t->cy, u);
        if (dflt)
            l->used = (vt_u16)from; /* unused again from there */
    }
    if (!dflt) {
        cells_blank(t, &c[t->cols - n], n); /* the row's end in the erase colour */
        mark(t, t->cols - n, t->cy, t->cols);
    }
}

static void insert_lines(vt_term *t, int n)
{
    if (t->cy < t->top || t->cy >= t->bot)
        return;
    scroll_down(t, t->cy, t->bot, n);
    if (t->pers != VT_AMIGA)
        t->cx = 0; /* xterm homes the column; the ROM console keeps it */
    t->wrap_pending = 0;
}

static void delete_lines(vt_term *t, int n)
{
    if (t->cy < t->top || t->cy >= t->bot)
        return;
    scroll_up(t, t->cy, t->bot, n);
    if (t->pers != VT_AMIGA)
        t->cx = 0;
    t->wrap_pending = 0;
}

/* ---- state ------------------------------------------------------------- */

/* ---- rare styles: underline colour and font ---------------------------------
 * Few cells have them, so a cell holds a one-byte index into this table
 * instead of the values. A full table is swept: entries no cell uses any
 * more are dropped and the cells renumbered. */

/* Lines first..first+n-1 of a ring of cap (the grid: first 0, cap n). */
static void sweep_lines(vt_line **lines, int first, int n, int cap, const vt_u8 *remap, vt_u8 *used)
{
    int y, x;
    for (y = 0; y < n; y++) {
        vt_line *l = lines ? lines[(first + y) % cap] : 0;
        if (!l)
            continue;
        for (x = 0; x < l->n; x++) {
            if (!l->c[x].ext)
                continue;
            if (used)
                used[l->c[x].ext] = 1;
            else
                l->c[x].ext = remap[l->c[x].ext];
        }
    }
}

static void sweep_styles(vt_term *t)
{
    vt_u8 used[256], remap[256];
    int i, k = 0;
    memset(used, 0, sizeof(used));
    sweep_lines(t->pri, 0, t->rows, t->rows, 0, used);
    if (t->alt)
        sweep_lines(t->alt, 0, t->rows, t->rows, 0, used);
    if (t->sb_cap)
        sweep_lines(t->sb, t->sb_head + t->sb_cap - t->sb_len, t->sb_len, t->sb_cap, 0, used);
    remap[0] = 0;
    for (i = 1; i <= t->n_styles; i++) {
        remap[i] = 0;
        if (used[i]) {
            t->styles[k] = t->styles[i - 1];
            remap[i] = (vt_u8)++k;
        }
    }
    t->n_styles = k;
    sweep_lines(t->pri, 0, t->rows, t->rows, remap, 0);
    if (t->alt)
        sweep_lines(t->alt, 0, t->rows, t->rows, remap, 0);
    if (t->sb_cap)
        sweep_lines(t->sb, t->sb_head + t->sb_cap - t->sb_len, t->sb_len, t->sb_cap, remap, 0);
}

/* The entry for the current underline colour and font (0: both default). */
static vt_u8 style_index(vt_term *t)
{
    int i, pass;
    if (t->ul == VT_COLOR_DEFAULT && !t->font)
        return 0;
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < t->n_styles; i++)
            if (t->styles[i].ul == t->ul && t->styles[i].font == t->font)
                return (vt_u8)(i + 1);
        if (t->n_styles < 255) {
            t->styles[t->n_styles].ul = t->ul;
            t->styles[t->n_styles].font = t->font;
            return (vt_u8)++t->n_styles;
        }
        sweep_styles(t);
    }
    return 0; /* 255 distinct styles on screen at once: drawn plain */
}

vt_color vt_cell_underline_color(const vt_term *t, const vt_cell *c)
{
    return c->ext && c->ext <= t->n_styles ? t->styles[c->ext - 1].ul : VT_COLOR_DEFAULT;
}

int vt_cell_font(const vt_term *t, const vt_cell *c)
{
    return c->ext && c->ext <= t->n_styles ? t->styles[c->ext - 1].font : 0;
}

static void save_cursor(vt_term *t, vt_saved *s)
{
    s->x = t->cx;
    s->y = t->cy;
    s->wrap_pending = t->wrap_pending;
    s->origin = t->origin;
    s->fg = t->fg;
    s->bg = t->bg;
    s->attr = t->attr;
    s->deco = t->deco;
    s->ul = t->ul;
    s->font = t->font;
    memcpy(s->charset, t->charset, 4);
    s->gl = t->gl;
}

static void restore_cursor(vt_term *t, const vt_saved *s)
{
    t->origin = s->origin;
    t->fg = s->fg;
    t->bg = s->bg;
    t->attr = s->attr;
    t->deco = s->deco;
    t->ul = s->ul;
    t->font = s->font;
    t->ext = style_index(t);
    memcpy(t->charset, s->charset, 4);
    t->gl = s->gl;
    t->cx = clampi(s->x, 0, t->cols - 1);
    t->cy = clampi(s->y, 0, t->rows - 1);
    t->wrap_pending = 0; /* DECRC lands on the cell, like libvterm (quirk-wrap-then-decsc-decrc) */
}

static void sgr_reset(vt_term *t)
{
    t->deco = 0;
    t->ul = VT_COLOR_DEFAULT;
    t->font = 0;
    t->ext = 0;
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
    t->sav.fg = t->sav.bg = t->sav.ul = VT_COLOR_DEFAULT;
    t->sav.attr = 0;
    t->sav.deco = t->sav.font = 0;
    memcpy(t->sav.charset, t->charset, 4);
    t->sav.gl = 0;
}

static int alloc_screen(vt_line ***scr, int rows, int cols, const vt_term *t);
static void free_screen(vt_line **scr, int rows);

static void set_alt(vt_term *t, int on, int clear)
{
    if (on && !t->alt) {
        /* made on first use: a terminal whose programs never switch (the
         * amiga personality, a console.device unit) never pays for it --
         * 16 bytes a cell, 32 KB at 80 x 25 (plan DV4) */
        if (!alloc_screen(&t->alt, t->rows, t->cols, t)) {
            free_screen(t->alt, t->rows);
            t->alt = 0;
            return; /* no memory: the primary screen stays */
        }
    }
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
    int w, lc;
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

    w = vt_char_width(cp);
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
    lc = row_cols(t, t->cy);
    if (w == 2 && t->cx == lc - 1) {
        if (t->autowrap) {
            erase_cells(t, t->cy, t->cx, t->cols);
            t->scr[t->cy]->wrapped = 1;
            t->cx = 0;
            index_down(t);
        } else {
            t->cx = lc - 2;
        }
        lc = row_cols(t, t->cy);
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
    c->deco = t->deco;
    c->ext = t->ext;
    c->width = (vt_u8)w;
    if (w == 2) {
        c[1] = c[0];
        c[1].ch = ' ';
        c[1].width = 0;
    }
    mark(t, t->cx, t->cy, t->cx + w);
    t->last_ch = (vt_u16)cp;

    if (t->cx + w >= lc) {
        t->cx = lc - 1;
        if (t->pers == VT_AMIGA && t->autowrap) {
            /* the ROM console wraps at once, no deferred wrap */
            t->scr[t->cy]->wrapped = 1;
            t->cx = 0;
            index_down(t);
        } else {
            t->wrap_pending = 1;
        }
    } else {
        t->cx += w;
    }
}

/* ---- controls ------------------------------------------------------------ */

static void clear_screen_home(vt_term *t)
{
    if (t->scr == t->pri)
        ovf_drop(t); /* a cleared screen has nothing above it to bring back */
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
        if (t->pers == VT_AMIGA) {
            amiga_move_linear(t, -1); /* the ROM: back into the row above */
            break;
        }
        if (t->cx > 0) {
            t->cx--;
        } else if ((t->modes & VT_MODE_REVERSE_WRAP) && t->autowrap && t->cy > t->top) {
            t->cy--; /* ?45: back to the end of the line above */
            t->cx = t->cols - 1;
        }
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
        if (t->pers == VT_PCANSI)
            break; /* not a motion for ANSI.SYS art (DOS prints it as a
                      glyph; DCTelnet's term-engine ignores it) */
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
        case '%':
            if (t->pers == VT_XTERM && final == 'G') {
                t->utf8 = 1; /* ISO 2022 DOCS: UTF-8 */
                t->cp437 = 0;
            } else if (t->pers == VT_XTERM && final == '@') {
                t->utf8 = 0; /* back to the 8-bit set */
            } else {
                note_unhandled(t, 'E', final);
            }
            break;
        case '#':
            if (final >= '3' && final <= '6') {
                /* DECDHL top / bottom, DECSWL, DECDWL: the line's size */
                vt_line *l = t->scr[t->cy];
                l->dbl = (vt_u8)(final == '3' ? VT_LINE_DOUBLE_TOP : final == '4' ? VT_LINE_DOUBLE_BOTTOM
                               : final == '6' ? VT_LINE_DOUBLE_WIDTH : 0);
                if (t->cx > row_cols(t, t->cy) - 1)
                    t->cx = row_cols(t, t->cy) - 1;
                mark_rows(t, t->cy, t->cy + 1);
            } else if (final == '8') { /* DECALN */
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
                    t->scr[y]->dbl = 0;
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
        if (t->pers != VT_AMIGA) /* the ROM console has no DECSC/DECRC (probed) */
            save_cursor(t, &t->sav);
        break;
    case '8':
        if (t->pers != VT_AMIGA)
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

static vt_color ext_colour(vt_term *t, int *i)
{
    /* 38;5;n / 38;2;r;g;b and the colon forms 38:5:n, 38:2:[cs]:r:g:b */
    int k = *i;
    long mode = param0(t, k + 1);
    int colon = (k + 1 < t->np) && t->sub[k + 1];
    if (mode == 5) {
        *i = k + 2;
        return (vt_color)(param0(t, k + 2) & 0xFF);
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
            t->amiga_bg = (vt_color)(p & 0xFF);
            mark_rows(t, 0, t->rows);
            continue;
        }
        if (t->sub[i])
            continue; /* a stray sub-parameter */
        if (t->pers != VT_PCANSI && p >= 30 && p <= 48 && p != 39) {
            /* the colours first: they are what an SGR mostly holds, and
             * they sat at the end of the chain below (pcansi has its own
             * 38 / 48 just below) */
            if (p <= 37)
                t->fg = (vt_color)(p - 30);
            else if (p == 38)
                t->fg = ext_colour(t, &i);
            else if (p <= 47)
                t->bg = (vt_color)(p - 40);
            else
                t->bg = ext_colour(t, &i);
            continue;
        }
        if (t->pers == VT_PCANSI && (p == 2 || p == 21)) {
            t->attr &= ~VT_ATTR_BOLD; /* DCTelnet: intensity off */
            continue;
        }
        if (t->pers == VT_PCANSI && p >= 90 && p <= 97) {
            t->fg = (vt_color)(p - 90); /* and bold, so a later 30-37 stays bright */
            t->attr |= VT_ATTR_BOLD;
            continue;
        }
        if (t->pers == VT_PCANSI && p >= 100 && p <= 107) {
            t->bg = (vt_color)(p - 100);
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
            long style = VT_UL_SINGLE;
            if (i + 1 < t->np && t->sub[i + 1]) {
                style = t->params[++i]; /* 4:0 none, 4:1-4:5 the styles */
                if (style > VT_UL_DASHED)
                    style = VT_UL_SINGLE;
            }
            t->deco = (vt_u8)((t->deco & ~VT_DECO_UL_MASK) | style);
            if (style)
                t->attr |= VT_ATTR_UNDERLINE;
            else
                t->attr &= ~VT_ATTR_UNDERLINE;
        } else if (p == 5) {
            t->attr = (vt_attr)((t->attr | VT_ATTR_BLINK) & ~VT_ATTR_RAPID);
        } else if (p == 6) {
            t->attr |= VT_ATTR_BLINK | (t->pers == VT_AMIGA ? 0 : VT_ATTR_RAPID);
        } else if (p == 7) {
            t->attr |= VT_ATTR_INVERSE;
        } else if (p == 8) {
            t->attr |= VT_ATTR_CONCEAL;
        } else if (p == 9) {
            t->attr |= VT_ATTR_STRIKE;
        } else if (p == 21) {
            t->attr |= VT_ATTR_UNDERLINE;
            t->deco = (vt_u8)((t->deco & ~VT_DECO_UL_MASK) | VT_UL_DOUBLE);
        } else if (p == 22) {
            t->attr &= ~(VT_ATTR_BOLD | VT_ATTR_FAINT);
        } else if (p == 23) {
            t->attr &= ~VT_ATTR_ITALIC;
            if (t->font == 10)
                t->font = 0; /* not italic, not Fraktur */
        } else if (p == 24) {
            t->attr &= ~VT_ATTR_UNDERLINE;
            t->deco &= ~VT_DECO_UL_MASK;
        } else if (p == 25) {
            t->attr &= ~(VT_ATTR_BLINK | VT_ATTR_RAPID);
        } else if (p == 27) {
            t->attr &= ~VT_ATTR_INVERSE;
        } else if (p == 28) {
            t->attr &= ~VT_ATTR_CONCEAL;
        } else if (p == 29) {
            t->attr &= ~VT_ATTR_STRIKE;
        } else if (t->pers != VT_AMIGA && p >= 10 && p <= 20) {
            t->font = (vt_u8)(p - 10); /* 10 primary, 11-19 alternative, 20 Fraktur */
        } else if (t->pers != VT_AMIGA && (p == 26 || p == 50)) {
            /* proportional spacing on / off: a character cell grid has none */
        } else if (t->pers != VT_AMIGA && p == 51) {
            t->attr = (vt_attr)((t->attr | VT_ATTR_FRAMED) & ~VT_ATTR_ENCIRCLED);
        } else if (t->pers != VT_AMIGA && p == 52) {
            t->attr = (vt_attr)((t->attr | VT_ATTR_ENCIRCLED) & ~VT_ATTR_FRAMED);
        } else if (t->pers != VT_AMIGA && p == 53) {
            t->attr |= VT_ATTR_OVERLINE;
        } else if (t->pers != VT_AMIGA && p == 54) {
            t->attr &= ~(VT_ATTR_FRAMED | VT_ATTR_ENCIRCLED);
        } else if (t->pers != VT_AMIGA && p == 55) {
            t->attr &= ~VT_ATTR_OVERLINE;
        } else if (t->pers != VT_AMIGA && p == 58) {
            t->ul = ext_colour(t, &i);
        } else if (t->pers != VT_AMIGA && p == 59) {
            t->ul = VT_COLOR_DEFAULT;
        } else if (t->pers != VT_AMIGA && p >= 60 && p <= 64) {
            t->deco = (vt_u8)((t->deco & ~VT_DECO_IDEO_MASK) | ((p - 59) << VT_DECO_IDEO_SHIFT));
        } else if (t->pers != VT_AMIGA && p == 65) {
            t->deco &= ~VT_DECO_IDEO_MASK;
        } else if (t->pers != VT_AMIGA && p == 73) {
            t->attr = (vt_attr)((t->attr | VT_ATTR_SUPER) & ~VT_ATTR_SUB);
        } else if (t->pers != VT_AMIGA && p == 74) {
            t->attr = (vt_attr)((t->attr | VT_ATTR_SUB) & ~VT_ATTR_SUPER);
        } else if (t->pers != VT_AMIGA && p == 75) {
            t->attr &= ~(VT_ATTR_SUPER | VT_ATTR_SUB);
        } else if (p >= 30 && p <= 37) {
            t->fg = (vt_color)(p - 30);
        } else if (p == 38) {
            t->fg = ext_colour(t, &i);
        } else if (p == 39) {
            t->fg = t->pers == VT_AMIGA ? t->amiga_dfg : VT_COLOR_DEFAULT;
        } else if (p >= 40 && p <= 47) {
            t->bg = (vt_color)(p - 40);
        } else if (p == 48) {
            t->bg = ext_colour(t, &i);
        } else if (p == 49) {
            t->bg = t->pers == VT_AMIGA ? t->amiga_dbg : VT_COLOR_DEFAULT;
        } else if (p >= 90 && p <= 97 && t->pers != VT_AMIGA) {
            t->fg = (vt_color)(p - 90 + 8);
        } else if (p >= 100 && p <= 107 && t->pers != VT_AMIGA) {
            t->bg = (vt_color)(p - 100 + 8);
        } else {
            note_value(t, 'S', p);
        }
    }
    t->ext = style_index(t);
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
            case 3: /* DECCOLM: 132 / 80 columns when ?40 allows it, as xterm */
                if (t->allow_cols) {
                    if (t->cb.layout)
                        t->cb.layout(t->user, VT_LAYOUT_COLUMNS, on ? 132 : 80);
                    t->top = 0; /* and, as a VT100 does, a clear screen */
                    t->bot = t->rows;
                    clear_screen_home(t);
                }
                break;
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
            case 2026:
                if (on)
                    t->modes |= VT_MODE_SYNC;
                else
                    t->modes &= ~(vt_u32)VT_MODE_SYNC;
                break;
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
            case 8: case 12: case 45: case 1005: case 1034: case 2031: case 7727: {
                vt_u32 bit = p == 8 ? VT_MODE_AUTOREPEAT : p == 12 ? VT_MODE_CURSOR_BLINK
                           : p == 45 ? VT_MODE_REVERSE_WRAP : p == 1005 ? VT_MODE_MOUSE_UTF8
                           : p == 1034 ? VT_MODE_META_8BIT : p == 2031 ? VT_MODE_SCHEME_UPDATES
                           : VT_MODE_APP_ESCAPE;
                if (on)
                    t->modes |= bit;
                else
                    t->modes &= ~bit;
                if (p == 12)
                    mark(t, t->cx, t->cy, t->cx + 1); /* the cursor's look changed */
                break;
            }
            case 40:
                t->allow_cols = (vt_u8)on;
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
        if (t->pers == VT_AMIGA) {
            amiga_move_linear(t, n);
            return 1;
        }
        t->cx = clampi(t->cx + (int)n, 0, row_cols(t, t->cy) - 1);
        t->wrap_pending = 0;
        return 1;
    case 'D':
        if (t->pers == VT_AMIGA) {
            amiga_move_linear(t, -n);
            return 1;
        }
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
        t->cx = clampi((int)n - 1, 0, row_cols(t, t->cy) - 1);
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
            if (t->scr == t->pri)
                ovf_drop(t);
            erase_rows(t, 0, t->rows);
            if (m == 3 && t->pers == VT_XTERM)
                vt_clear_scrollback(t);
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
    case 'T': /* more parameters: xterm's mouse highlight; SD for ANSI art */
        if (t->np <= 1 || t->pers == VT_PCANSI)
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

/* CSI Ps t (xterm window operations): the reports, and the title stack.
 * Moving, resizing and raising the window are the user's, not a program's. */
static void window_op(vt_term *t)
{
    long op = param0(t, 0);
    char b[40];
    int n, a = 0, c = 0, code = 0;
    switch (op) {
    case 14: /* text area in pixels */
        code = 4;
        a = t->rows * t->cell_h;
        c = t->cols * t->cell_w;
        break;
    case 16: /* character cell in pixels */
        code = 6;
        a = t->cell_h;
        c = t->cell_w;
        break;
    case 18: /* text area in characters */
    case 19: /* the screen: the same, the window is the terminal */
        code = op == 18 ? 8 : 9;
        a = t->rows;
        c = t->cols;
        break;
    case 22: /* push the title */
        if (param0(t, 1) != 1) {
            if (t->n_titles == 4) {
                memmove(t->title_stack[0], t->title_stack[1], 3 * VT_STR_MAX);
                t->n_titles = 3;
            }
            memcpy(t->title_stack[t->n_titles++], t->title, VT_STR_MAX);
        }
        return;
    case 23: /* pop it */
        if (param0(t, 1) != 1 && t->n_titles) {
            memcpy(t->title, t->title_stack[--t->n_titles], VT_STR_MAX);
            if (t->cb.title)
                t->cb.title(t->user, t->title);
        }
        return;
    default:
        note_unhandled(t, 'C', 't');
        return;
    }
    if ((op == 14 || op == 16) && !t->cell_w)
        return; /* the host has not said */
    n = put_csi(t, b);
    n = fmt_uint(b, n, code);
    b[n++] = ';';
    n = fmt_uint(b, n, a);
    b[n++] = ';';
    n = fmt_uint(b, n, c);
    b[n++] = 't';
    reply(t, b, n);
}

/* DECRQM: CSI [?] Ps $ p -> CSI [?] Ps ; Pm $ y, Pm 1 set, 2 reset,
 * 0 not recognised, 4 permanently reset. */
static void report_mode(vt_term *t)
{
    long m = param0(t, 0);
    int v = 0;
    char b[32];
    int n;
    if (t->priv == '?') {
        vt_u32 bit = 0;
        switch (m) {
        case 1: bit = VT_MODE_APP_CURSOR; break;
        case 5: bit = VT_MODE_SCREEN_REVERSE; break;
        case 9: bit = VT_MODE_MOUSE_X10; break;
        case 25: bit = VT_MODE_CURSOR_VISIBLE; break;
        case 47: case 1047: case 1049: bit = VT_MODE_ALT_SCREEN; break;
        case 1000: bit = VT_MODE_MOUSE_NORMAL; break;
        case 1002: bit = VT_MODE_MOUSE_BUTTON; break;
        case 1003: bit = VT_MODE_MOUSE_ANY; break;
        case 1004: bit = VT_MODE_FOCUS; break;
        case 1006: bit = VT_MODE_MOUSE_SGR; break;
        case 2004: bit = VT_MODE_BRACKET_PASTE; break;
        case 2026: bit = VT_MODE_SYNC; break;
        case 6: v = t->origin ? 1 : 2; break;
        case 7: v = t->autowrap ? 1 : 2; break;
        case 12: bit = VT_MODE_CURSOR_BLINK; break;
        case 8: bit = VT_MODE_AUTOREPEAT; break;
        case 45: bit = VT_MODE_REVERSE_WRAP; break;
        case 1005: bit = VT_MODE_MOUSE_UTF8; break;
        case 1034: bit = VT_MODE_META_8BIT; break;
        case 2031: bit = VT_MODE_SCHEME_UPDATES; break;
        case 7727: bit = VT_MODE_APP_ESCAPE; break;
        case 40: v = t->allow_cols ? 1 : 2; break;
        case 3: v = t->cols == 132 ? 1 : 2; break;
        default: break;
        }
        if (bit)
            v = (t->modes & bit) ? 1 : 2;
    } else {
        if (m == 4)
            v = t->insert ? 1 : 2;
        else if (m == 20)
            v = (t->modes & VT_MODE_NEWLINE) ? 1 : 2;
    }
    n = put_csi(t, b);
    if (t->priv == '?')
        b[n++] = '?';
    n = fmt_uint(b, n, m);
    b[n++] = ';';
    n = fmt_uint(b, n, v);
    b[n++] = '$';
    b[n++] = 'y';
    reply(t, b, n);
}

static int scheme_of(const vt_term *t);

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
    if (t->inter == ' ' && final == 'q') { /* DECSCUSR */
        long st = param0(t, 0);
        t->cursor_style = (vt_u8)(st <= 6 ? st : 0);
        return;
    }
    if (t->inter == '$' && final == 'p') { /* DECRQM */
        report_mode(t);
        return;
    }
    if (t->inter) {
        note_unhandled(t, 'C', final);
        return;
    }
    if (t->priv == '?') {
        if (final == 'h' || final == 'l')
            set_mode(t, final == 'h');
        else if (final == 'n' && param0(t, 0) == 6)
            report_cursor(t, 1);
        else if (final == 'n' && param0(t, 0) == 996)
            reply(t, scheme_of(t) == 1 ? "\033[?997;1n" : "\033[?997;2n", 9);
        else if (final == 'm' && param0(t, 0) == 4) { /* XTQMODKEYS */
            char b[16];
            int n = put_csi(t, b);
            b[n++] = '>';
            b[n++] = '4';
            b[n++] = ';';
            b[n++] = (char)('0' + t->mok);
            b[n++] = 'm';
            reply(t, b, n);
        }
        else if (final == 'J' || final == 'K')
            csi_common(t, final); /* DECSED / DECSEL: no protected cells */
        else
            note_unhandled(t, 'C', final);
        return;
    }
    if (t->priv == '>') {
        if (final == 'q' && param0(t, 0) == 0) {
            reply(t, "\033P>|vtcon 1.0\033\\", 15); /* XTVERSION */
            return;
        }
        if (final == 'm' && param0(t, 0) == 4) { /* modifyOtherKeys */
            long v = t->np > 1 ? param0(t, 1) : 0;
            t->mok = (vt_u8)(v > 2 ? 2 : v);
            return;
        }
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
        window_op(t);
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
    case 's': /* ANSI.SYS SCP/RCP: the position only, colours stay */
        t->sav.x = t->cx;
        t->sav.y = t->cy;
        return;
    case 'u':
        t->cx = clampi(t->sav.x, 0, t->cols - 1);
        t->cy = clampi(t->sav.y, 0, t->rows - 1);
        t->wrap_pending = 0;
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
            t->amiga_bg = (vt_color)(param0(t, 0) & 0xFF);
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
    case 'G': /* no CHA on the ROM console (probed) */
    case '`':
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

/* The string terminator a reply uses: BEL for a request that ended with
 * BEL, else ST (ESC \). */
static int put_st(const vt_term *t, char *b, int n)
{
    if (t->str_bel) {
        b[n++] = 0x07;
    } else {
        b[n++] = 0x1B;
        b[n++] = '\\';
    }
    return n;
}

static int hexval(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* An X colour spec: rgb:r/g/b (1-4 hex digits each) or #rgb, #rrggbb,
 * #rrrgggbbb, #rrrrggggbbbb. 0x01RRGGBB, 0 when it is not one. */
static vt_u32 parse_color(const char *s, int n)
{
    vt_u32 c[3];
    int i, k, d;
    if (n > 4 && !memcmp(s, "rgb:", 4)) {
        s += 4;
        n -= 4;
        for (k = 0; k < 3; k++) {
            vt_u32 v = 0, max = 0;
            for (i = 0; i < n && s[i] != '/'; i++) {
                if ((d = hexval(s[i])) < 0 || i >= 4)
                    return 0;
                v = v * 16 + (vt_u32)d;
                max = max * 16 + 15;
            }
            if (!i)
                return 0;
            c[k] = v * 255 / max;
            s += i + (i < n);
            n -= i + (i < n);
        }
        return 0x01000000UL | (c[0] << 16) | (c[1] << 8) | c[2];
    }
    if (n >= 4 && s[0] == '#' && (n - 1) % 3 == 0 && n - 1 <= 12) {
        int w = (n - 1) / 3;
        for (k = 0; k < 3; k++) {
            vt_u32 v = 0;
            for (i = 0; i < w; i++) {
                if ((d = hexval(s[1 + k * w + i])) < 0)
                    return 0;
                v = v * 16 + (vt_u32)d;
            }
            c[k] = w == 1 ? v * 17 : v >> ((w - 2) * 4);
        }
        return 0x01000000UL | (c[0] << 16) | (c[1] << 8) | c[2];
    }
    return 0;
}

/* OSC Ps;[index;]rgb:rrrr/gggg/bbbb, the form xterm answers in. */
static void reply_color(vt_term *t, long cmd, int index, vt_u32 rgb)
{
    static const char hex[] = "0123456789abcdef";
    char b[48];
    int n = 0, k;
    b[n++] = 0x1B;
    b[n++] = ']';
    n = fmt_uint(b, n, cmd);
    b[n++] = ';';
    if (index >= 0) {
        n = fmt_uint(b, n, index);
        b[n++] = ';';
    }
    memcpy(b + n, "rgb:", 4);
    n += 4;
    for (k = 2; k >= 0; k--) {
        int v = (int)((rgb >> (k * 8)) & 0xFF);
        b[n++] = hex[v >> 4];
        b[n++] = hex[v & 15];
        b[n++] = hex[v >> 4];
        b[n++] = hex[v & 15];
        if (k)
            b[n++] = '/';
    }
    n = put_st(t, b, n);
    reply(t, b, n);
}

/* 1 dark, 2 light: the background's brightness. */
static int scheme_of(const vt_term *t)
{
    vt_u32 bg = vt_default_color(t, 1);
    long lum = (long)((bg >> 16) & 0xFF) * 3 + (long)((bg >> 8) & 0xFF) * 6 + (long)(bg & 0xFF);
    return lum < 1280 ? 1 : 2;
}

/* ?2031: a program asked to hear when dark and light swap. */
static void scheme_check(vt_term *t)
{
    int s = scheme_of(t);
    if (s != t->scheme) {
        t->scheme = (vt_u8)s;
        if (t->modes & VT_MODE_SCHEME_UPDATES)
            reply(t, s == 1 ? "\033[?997;1n" : "\033[?997;2n", 9);
    }
}

static void colors_changed(vt_term *t)
{
    mark_rows(t, 0, t->rows);
    if (t->cb.colors)
        t->cb.colors(t->user);
    scheme_check(t);
}

/* The next ';'-separated item of the OSC string from *i: its length. */
static int osc_item(const vt_term *t, int *i, const char **item)
{
    int k = *i;
    *item = t->str + k;
    while (k < t->str_len && t->str[k] != ';')
        k++;
    k -= *i;
    *i += k + (*i + k < t->str_len);
    return k;
}

static void osc_dispatch(vt_term *t)
{
    int i = 0, changed = 0;
    long cmd = 0;
    t->str[t->str_len] = 0;
    while (i < t->str_len && t->str[i] >= '0' && t->str[i] <= '9')
        cmd = cmd * 10 + (t->str[i++] - '0');
    if (i < t->str_len && t->str[i] != ';')
        return;
    i++;
    if (cmd == 104 || (cmd >= 110 && cmd <= 112)) { /* resets */
        const char *it;
        int n, any = 0;
        if (cmd != 104) {
            t->dflt_set[cmd - 110] = 0;
        } else {
            while (i < t->str_len && (n = osc_item(t, &i, &it)) >= 0) {
                long k = 0;
                int j;
                for (j = 0; j < n && it[j] >= '0' && it[j] <= '9'; j++)
                    k = k * 10 + (it[j] - '0');
                if (j && k < 256)
                    t->pal_set[k] = 0;
                any = 1;
                if (!n)
                    break;
            }
            if (!any)
                memset(t->pal_set, 0, sizeof(t->pal_set));
        }
        colors_changed(t);
        return;
    }
    if (i > t->str_len)
        return;
    if (cmd == 1)
        return; /* icon name: no icon to name */
    if (cmd == 4) {
        /* 4;index;spec[;index;spec...]: ? queries, a colour sets */
        const char *it, *spec;
        int n, m;
        while (i < t->str_len) {
            long k = 0;
            int j;
            n = osc_item(t, &i, &it);
            m = osc_item(t, &i, &spec);
            for (j = 0; j < n && it[j] >= '0' && it[j] <= '9'; j++)
                k = k * 10 + (it[j] - '0');
            if (!j || k > 255)
                break;
            if (m == 1 && spec[0] == '?') {
                reply_color(t, 4, (int)k, vt_palette_rgb(t, (int)k));
            } else {
                vt_u32 c = parse_color(spec, m);
                if (c) {
                    t->pal_set[k] = c;
                    changed = 1;
                }
            }
        }
        if (changed)
            colors_changed(t);
        return;
    }
    if (cmd >= 10 && cmd <= 12) {
        /* 10;fg[;bg[;cursor]]: each item the next of the three */
        const char *spec;
        int m;
        long which = cmd;
        while (i < t->str_len && which <= 12) {
            m = osc_item(t, &i, &spec);
            if (m == 1 && spec[0] == '?') {
                reply_color(t, which, -1, vt_default_color(t, (int)(which - 10)));
            } else {
                vt_u32 c = parse_color(spec, m);
                if (c) {
                    t->dflt_set[which - 10] = c;
                    changed = 1;
                }
            }
            which++;
        }
        if (changed)
            colors_changed(t);
        return;
    }
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

/* ---- DCS ------------------------------------------------------------------ */

/* The current SGR as parameters (DECRQSS answers with it). */
static int put_sgr(const vt_term *t, char *b, int n)
{
    static const struct { vt_attr a; char p; } plain[] = {
        { VT_ATTR_BOLD, '1' }, { VT_ATTR_FAINT, '2' }, { VT_ATTR_ITALIC, '3' },
        { VT_ATTR_INVERSE, '7' }, { VT_ATTR_CONCEAL, '8' }, { VT_ATTR_STRIKE, '9' }
    };
    int k, w;
    b[n++] = '0';
    for (k = 0; k < (int)(sizeof(plain) / sizeof(plain[0])); k++)
        if (t->attr & plain[k].a) {
            b[n++] = ';';
            b[n++] = plain[k].p;
        }
    if (t->attr & VT_ATTR_UNDERLINE) {
        b[n++] = ';';
        b[n++] = '4';
        if ((t->deco & VT_DECO_UL_MASK) > VT_UL_SINGLE) {
            b[n++] = ':';
            b[n++] = (char)('0' + (t->deco & VT_DECO_UL_MASK));
        }
    }
    if (t->attr & VT_ATTR_BLINK) {
        b[n++] = ';';
        b[n++] = t->attr & VT_ATTR_RAPID ? '6' : '5';
    }
    if (t->attr & VT_ATTR_OVERLINE) {
        memcpy(b + n, ";53", 3);
        n += 3;
    }
    for (w = 0; w < 2; w++) {
        vt_color c = w ? t->bg : t->fg;
        if (c == VT_COLOR_DEFAULT)
            continue;
        b[n++] = ';';
        if (c & VT_COLOR_RGB) {
            n = fmt_uint(b, n, w ? 48 : 38);
            memcpy(b + n, ";2;", 3);
            n += 3;
            n = fmt_uint(b, n, (long)((c >> 16) & 0xFF));
            b[n++] = ';';
            n = fmt_uint(b, n, (long)((c >> 8) & 0xFF));
            b[n++] = ';';
            n = fmt_uint(b, n, (long)(c & 0xFF));
        } else if (c < 8) {
            n = fmt_uint(b, n, (long)(c + (w ? 40 : 30)));
        } else if (c < 16) {
            n = fmt_uint(b, n, (long)(c - 8 + (w ? 100 : 90)));
        } else {
            n = fmt_uint(b, n, w ? 48 : 38);
            memcpy(b + n, ";5;", 3);
            n += 3;
            n = fmt_uint(b, n, (long)c);
        }
    }
    return n;
}

/* DECRQSS (DCS $ q Pt ST): the setting Pt names, as DCS 1 $ r ... ST;
 * DCS 0 $ r ST for one this terminal does not have. */
static void decrqss(vt_term *t, const char *pt, int len)
{
    char b[160];
    int n = 0;
    b[n++] = 0x1B;
    b[n++] = 'P';
    b[n++] = '1';
    b[n++] = '$';
    b[n++] = 'r';
    if (len == 1 && pt[0] == 'm') {
        n = put_sgr(t, b, n);
        b[n++] = 'm';
    } else if (len == 1 && pt[0] == 'r') {
        n = fmt_uint(b, n, t->top + 1);
        b[n++] = ';';
        n = fmt_uint(b, n, t->bot);
        b[n++] = 'r';
    } else if (len == 2 && pt[0] == ' ' && pt[1] == 'q') {
        n = fmt_uint(b, n, t->cursor_style ? t->cursor_style : 1);
        b[n++] = ' ';
        b[n++] = 'q';
    } else if (len == 2 && pt[0] == '"' && pt[1] == 'p') {
        memcpy(b + n, "64;1\"p", 6); /* a level 4 terminal, 7-bit controls */
        n += 6;
    } else if (len == 2 && pt[0] == '"' && pt[1] == 'q') {
        memcpy(b + n, "0\"q", 3);    /* DECSCA: no protected cells */
        n += 3;
    } else {
        n = 2;
        b[n++] = '0';
        b[n++] = '$';
        b[n++] = 'r';
    }
    b[n++] = 0x1B;
    b[n++] = '\\';
    reply(t, b, n);
}

/* XTGETTCAP (DCS + q hexname;hexname ST): a terminfo capability, as
 * DCS 1 + r hexname=hexvalue ST, or DCS 0 + r hexname ST when unknown. */
static void xtgettcap(vt_term *t, const char *s, int len)
{
    static const char *const caps[][2] = {
        { "TN", "vtcon" }, { "name", "vtcon" }, { "Co", "256" }, { "colors", "256" },
        { "RGB", "8/8/8" }
    };
    static const char hex[] = "0123456789ABCDEF";
    while (len > 0) {
        char name[16], b[96];
        int k = 0, i, n = 0, h1, h2;
        const char *v = 0;
        while (len >= 2 && *s != ';') {
            h1 = hexval(s[0]);
            h2 = hexval(s[1]);
            if (h1 < 0 || h2 < 0 || k >= (int)sizeof(name) - 1)
                return;
            name[k++] = (char)(h1 * 16 + h2);
            s += 2;
            len -= 2;
        }
        name[k] = 0;
        if (len > 0 && *s == ';') {
            s++;
            len--;
        }
        for (i = 0; i < (int)(sizeof(caps) / sizeof(caps[0])); i++)
            if (!strcmp(name, caps[i][0]))
                v = caps[i][1];
        b[n++] = 0x1B;
        b[n++] = 'P';
        b[n++] = v ? '1' : '0';
        b[n++] = '+';
        b[n++] = 'r';
        for (i = 0; i < k; i++) {
            b[n++] = hex[(unsigned char)name[i] >> 4];
            b[n++] = hex[name[i] & 15];
        }
        if (v) {
            b[n++] = '=';
            for (i = 0; v[i]; i++) {
                b[n++] = hex[(unsigned char)v[i] >> 4];
                b[n++] = hex[v[i] & 15];
            }
        }
        b[n++] = 0x1B;
        b[n++] = '\\';
        reply(t, b, n);
        if (!k)
            break;
    }
}

static void dcs_dispatch(vt_term *t)
{
    if (t->str_len >= 2 && t->str[0] == '$' && t->str[1] == 'q')
        decrqss(t, t->str + 2, t->str_len - 2);
    else if (t->str_len >= 2 && t->str[0] == '+' && t->str[1] == 'q')
        xtgettcap(t, t->str + 2, t->str_len - 2);
    else
        note_value(t, 'D', 0);
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
    t->str_bel = 0;
}

static void end_string(vt_term *t)
{
    if (t->state == S_OSC)
        osc_dispatch(t);
    else if (t->str_kind == 'P')
        dcs_dispatch(t);
    else
        note_value(t, 'X', 0);
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
            t->str_bel = 1;
            end_string(t);
            return;
        } else if (c == 0x9C) {
            end_string(t);
            return;
        } else {
            if ((t->state == S_OSC || t->str_kind == 'P') && c >= 0x20 &&
                t->str_len < VT_STR_MAX - 1) {
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
    memset(*scr, 0, rows * sizeof(vt_line *)); /* free_screen after a failure frees only what was made */
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
    t->utf8 = 1;
    t->bold_bright = 1;
    t->sb_cap = scrollback > 0 ? scrollback : 0;
    if (t->sb_cap)
        t->sb = (vt_line **)VT_MALLOC(t->sb_cap * sizeof(vt_line *));
    if (!t->tabs || (t->sb_cap && !t->sb) ||
        !alloc_screen(&t->pri, rows, cols, t)) {
        vt_free(t);
        return 0;
    }
    t->scr = t->pri;
    vt_reset(t);
    return t;
}

void vt_clear_scrollback(vt_term *t)
{
    while (t->sb_len) {
        t->sb_head = (t->sb_head + t->sb_cap - 1) % t->sb_cap;
        VT_FREE(t->sb[t->sb_head]);
        t->sb_len--;
    }
}

int vt_set_scrollback(vt_term *t, int lines)
{
    vt_line **ring = 0;
    int keep, i;
    if (!t || lines < 0)
        return 0;
    if (lines == t->sb_cap)
        return 1;
    if (lines && !(ring = (vt_line **)VT_MALLOC(lines * sizeof(vt_line *))))
        return 0;
    keep = t->sb_len < lines ? t->sb_len : lines;
    /* the oldest lines past the new size go */
    while (t->sb_len > keep) {
        int oldest = (t->sb_head + t->sb_cap - t->sb_len) % t->sb_cap;
        VT_FREE(t->sb[oldest]);
        t->sb_len--;
    }
    /* the rest, oldest first, to the start of the new ring */
    for (i = 0; i < keep; i++)
        ring[i] = t->sb[(t->sb_head + t->sb_cap - keep + i) % t->sb_cap];
    if (t->sb)
        VT_FREE(t->sb);
    t->sb = ring;
    t->sb_cap = lines;
    t->sb_len = keep;
    t->sb_head = lines ? keep % lines : 0;
    return 1;
}

void vt_free(vt_term *t)
{
    if (!t)
        return;
    free_screen(t->pri, t->rows);
    free_screen(t->alt, t->rows);
    ovf_drop(t); /* into the scrollback, freed below */
    if (t->ovf)
        VT_FREE(t->ovf);
    while (t->sb_len) {
        t->sb_head = (t->sb_head + t->sb_cap - 1) % t->sb_cap;
        VT_FREE(t->sb[t->sb_head]);
        t->sb_len--;
    }
    if (t->sb)
        VT_FREE(t->sb);
    if (t->tabs)
        VT_FREE(t->tabs);
    VT_FREE(t);
}

void vt_reset(vt_term *t)
{
    t->scr = t->pri;
    t->modes = VT_MODE_CURSOR_VISIBLE | VT_MODE_AUTOREPEAT;
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
    if (t->alt) {
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

/* xterm's 256 colours: the 16 ANSI ones, a 6x6x6 cube, 24 greys. */
vt_u32 vt_palette_rgb(const vt_term *t, int i)
{
    static const vt_u32 ansi16[16] = {
        0x000000, 0xCD0000, 0x00CD00, 0xCDCD00, 0x0000EE, 0xCD00CD, 0x00CDCD, 0xE5E5E5,
        0x7F7F7F, 0xFF0000, 0x00FF00, 0xFFFF00, 0x5C5CFF, 0xFF00FF, 0x00FFFF, 0xFFFFFF
    };
    static const vt_u8 level[6] = { 0x00, 0x5F, 0x87, 0xAF, 0xD7, 0xFF };
    if (i < 0 || i > 255)
        return 0;
    if (t && t->pal_set[i])
        return t->pal_set[i] & 0xFFFFFFUL;
    if (i < 16)
        return ansi16[i];
    if (i < 232) {
        i -= 16;
        return ((vt_u32)level[i / 36] << 16) | ((vt_u32)level[(i / 6) % 6] << 8) | level[i % 6];
    }
    i = 8 + (i - 232) * 10;
    return ((vt_u32)i << 16) | ((vt_u32)i << 8) | (vt_u32)i;
}

/* One channel to its nearest level of the xterm cube (0, 95, 135, 175, 215, 255). */
static int cube_step(int v)
{
    return v < 48 ? 0 : v < 115 ? 1 : (v - 35) / 40;
}

static long dist2(int r, int g, int b, vt_u32 c)
{
    long dr = r - (int)((c >> 16) & 0xFF), dg = g - (int)((c >> 8) & 0xFF), db = b - (int)(c & 0xFF);
    return dr * dr + dg * dg + db * db;
}

int vt_rgb_to_256(vt_u32 rgb)
{
    static const int level[6] = { 0x00, 0x5F, 0x87, 0xAF, 0xD7, 0xFF };
    int r = (int)((rgb >> 16) & 0xFF), g = (int)((rgb >> 8) & 0xFF), b = (int)(rgb & 0xFF);
    int cr = cube_step(r), cg = cube_step(g), cb = cube_step(b);
    int avg = (r + g + b) / 3, grey = avg > 238 ? 23 : avg < 3 ? 0 : (avg - 3) / 10;
    int gv = 8 + 10 * grey;
    vt_u32 cube = ((vt_u32)level[cr] << 16) | ((vt_u32)level[cg] << 8) | (vt_u32)level[cb];
    vt_u32 gc = ((vt_u32)gv << 16) | ((vt_u32)gv << 8) | (vt_u32)gv;
    /* the cube or the grey ramp, whichever is nearer */
    return dist2(r, g, b, gc) < dist2(r, g, b, cube) ? 232 + grey : 16 + 36 * cr + 6 * cg + cb;
}

void vt_set_default_colors(vt_term *t, vt_u32 fg, vt_u32 bg, vt_u32 cursor)
{
    t->dflt[0] = fg & 0xFFFFFFUL;
    t->dflt[1] = bg & 0xFFFFFFUL;
    t->dflt[2] = cursor & 0xFFFFFFUL;
    scheme_check(t);
}

vt_u32 vt_default_color(const vt_term *t, int which)
{
    if (which < 0 || which > 2)
        return 0;
    return t->dflt_set[which] ? t->dflt_set[which] & 0xFFFFFFUL : t->dflt[which];
}

/* The host's own palette (a profile default), as OSC 4 does it from a
 * program: entry i becomes 0xRRGGBB for good, until the program changes it.
 * The colours callback fires: the renderer's pens are out of date. */
void vt_set_palette(vt_term *t, int i, vt_u32 rgb)
{
    if (!t || i < 0 || i > 255)
        return;
    t->pal_set[i] = 0x01000000UL | (rgb & 0xFFFFFFUL);
    colors_changed(t);
}

void vt_clear_palette(vt_term *t, int i)
{
    if (!t || i < 0 || i > 255)
        return;
    t->pal_set[i] = 0;
    colors_changed(t);
}

/* xterm SGR 1: bright colours 8-15 for the default 0-7 (on, as xterm
 * draws it) or the plain colours, the bold font style either way. A host
 * setting, so vt_reset leaves it. */
void vt_set_bold_bright(vt_term *t, int on)
{
    if (t)
        t->bold_bright = on != 0;
}

/* DECSCUSR (0/1 blinking block, 2 block, 3/4 underline, 5/6 bar) set by
 * the host before the first output, as a default the programs' DECSCUSR
 * still overrides. */
void vt_set_cursor_style(vt_term *t, int style)
{
    if (t)
        t->cursor_style = (vt_u8)(style >= 0 && style <= 6 ? style : 0);
}

/* ?12 from the host: a profile's cursor-blink default,
 * overridable by the programs' ?12 like anything else. A blinking DECSCUSR
 * shape blinks on its own. */
void vt_set_cursor_blink(vt_term *t, int on)
{
    if (!t)
        return;
    if (on)
        t->modes |= VT_MODE_CURSOR_BLINK;
    else
        t->modes &= ~(vt_u32)VT_MODE_CURSOR_BLINK;
}

/* ?5 (DECSCNM) from the host: the whole screen in reverse video. The host
 * redraws; the engine only keeps the state (vt_resolve_colors does the work). */
void vt_screen_reverse(vt_term *t, int on)
{
    if (!t)
        return;
    if (on)
        t->modes |= VT_MODE_SCREEN_REVERSE;
    else
        t->modes &= ~(vt_u32)VT_MODE_SCREEN_REVERSE;
}

void vt_set_cell_pixels(vt_term *t, int w, int h)
{
    t->cell_w = w;
    t->cell_h = h;
}

int vt_cursor_style(const vt_term *t)
{
    return t->cursor_style;
}

void vt_set_onlcr(vt_term *t, int on)
{
    t->onlcr = on != 0;
}

void vt_set_reflow(vt_term *t, int on)
{
    t->reflow = on != 0;
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
    if (t->wrap_pending || t->cx >= row_cols(t, t->cy) - 1)
        return 0;
    room = row_cols(t, t->cy) - 1 - t->cx;
    if (n > room)
        n = room;
    c = &t->scr[t->cy]->c[t->cx];
#ifdef VT_ASM
    {
        /* the loop below in assembler (vtengine_68k.s): 14 us a character
         * in C on a 14 MHz 68020 (S1) */
        vt_cell p;
        p.fg = t->fg;
        p.bg = t->bg;
        p.ch = 0;
        p.attr = t->attr;
        p.width = 1;
        p.deco = t->deco;
        p.ext = t->ext;
        p.pad = 0;
        k = vt_asm_put_run(c, b, n, &p);
    }
#else
    for (k = 0; k < n; k++) {
        if (b[k] < 0x20 || b[k] >= 0x7F)
            break; /* the run of printable ASCII ends: the parser's byte */
        if (c[k].width != 1 || (t->cx + k + 1 < t->cols && c[k + 1].width == 0))
            break; /* a wide glyph here: put_char unwides it */
        c[k].ch = b[k];
        c[k].fg = t->fg;
        c[k].bg = t->bg;
        c[k].attr = t->attr;
        c[k].deco = t->deco;
        c[k].ext = t->ext;
    }
#endif
    if (k) {
        mark(t, t->cx, t->cy, t->cx + (int)k);
        t->cx += (int)k;
        t->last_ch = b[k - 1];
    }
    return k;
}

/* "ESC [ parameters final" whole in the buffer, the parameters only
 * digits, ';' and ':': parsed here in one go and dispatched, instead of a
 * walk through decode() and feed() for every byte (colour output is
 * mostly these: 52 us a byte on a 14 MHz 68020, the terminal's slowest
 * path; S1). Anything else -- a private marker, an intermediate, a
 * control inside, the sequence cut by the end of the write -- returns 0
 * with nothing changed that the parser's own ESC does not set again.
 * The bytes consumed. */
static long csi_fast(vt_term *t, const vt_u8 *b, long len)
{
    long j = 2, v;
    int np = 0;
    vt_u8 c;
    if (len < 3 || b[1] != '[')
        return 0;
    clear_params(t);
    for (;;) {
        if (j >= len)
            return 0;
        c = b[j];
        if (c >= '0' && c <= '9') {
            if (!np)
                np = 1;
            v = t->params[np - 1];
            t->params[np - 1] = v < VT_PARAM_MAX / 10 ? v * 10 + (long)(c - '0') : VT_PARAM_MAX;
        } else if (c == ';' || c == ':') {
            if (!np)
                np = 1;
            if (np < VT_MAX_PARAMS) {
                t->params[np] = 0;
                t->sub[np] = (vt_u8)(c == ':');
                np++;
            }
        } else {
            break;
        }
        j++;
    }
    if (c < 0x40 || c > 0x7E)
        return 0;
    t->np = np;
    t->csi8 = 0;
    /* the state is ground, as after the final byte. An xterm SGR (no
     * private marker, no intermediate here) is csi_xterm's last case:
     * straight to it */
    if (c == 'm' && t->pers == VT_XTERM)
        sgr(t);
    else
        csi_dispatch(t, c);
    return j + 1;
}

void vt_write(vt_term *t, const vt_u8 *buf, long len)
{
    vt_feed(t, buf, len);
    flush(t);
}

void vt_flush(vt_term *t)
{
    flush(t);
}

void vt_feed(vt_term *t, const vt_u8 *buf, long len)
{
    long i = 0;
    while (i < len) {
        vt_u8 b = buf[i];
        if (b != 0x09)
            t->tab_end = 0; /* only a tab right after a tab keeps it */
        if (b >= 0x20 && b < 0x7F && t->state == S_GROUND && !t->u_need && !t->insert &&
            !t->single_shift && t->charset[t->gl] == 'B' && !t->amiga_msb) {
            long k = put_ascii_run(t, buf + i, len - i); /* as far as printable ASCII and the row go */
            if (k) {
                i += k;
                continue;
            }
        }
        if (b == 0x1B && t->state == S_GROUND && !t->u_need) {
            long k = csi_fast(t, buf + i, len - i);
            if (k) {
                i += k;
                continue;
            }
        }
        decode(t, b);
        i++;
    }
}

/* ---- reflow (vt_set_reflow) ---------------------------------------------- */

/* A blank nobody wrote: what the end of a line is padded with. A space
 * with a colour or an attribute is text and keeps its place. (An xterm
 * BCE erase leaves coloured blanks, so there the painted tail counts as
 * text; the amiga personality erases with the default colours.) */
static int cell_plain_blank(const vt_cell *c)
{
    return c->ch == ' ' && c->width == 1 && !c->attr && !c->deco && !c->ext &&
           c->fg == VT_COLOR_DEFAULT && c->bg == VT_COLOR_DEFAULT;
}

/* The cells of a line up to its last written one. */
static int line_text_len(const vt_line *l, int n)
{
    while (n > 0 && cell_plain_blank(&l->c[n - 1]))
        n--;
    return n;
}

/* A logical line being typed again at a new width. */
typedef struct vt_rewrap {
    vt_term *t;
    vt_line **out;  /* the new rows; NULL only counts them */
    int cols;
    int eager;      /* wrap as soon as a row is full (the ROM console) */
    int nx, ny;     /* where the next cell goes */
} vt_rewrap;

static int rw_row(vt_rewrap *w)
{
    vt_line *l;
    if (!w->out)
        return 1;
    l = line_new(w->cols);
    if (!l)
        return 0;
    line_clear(w->t, l, w->cols);
    w->out[w->ny] = l;
    return 1;
}

static int rw_wrap(vt_rewrap *w)
{
    if (w->out)
        w->out[w->ny]->wrapped = 1;
    w->ny++;
    w->nx = 0;
    return rw_row(w);
}

/* Rows old[s..e], one logical line (each but the last wrapped into the
 * next), typed again from row w->ny on, with the wrap rule of the
 * personality: the amiga one wraps the moment a row fills (a line that
 * ends exactly at the margin owns the empty row after it, where its cursor
 * went), the others only when the next character comes. When the cursor
 * (cx, cy) is on the line, *ncx / *ncy / *nwp get where it lands: on the
 * same character, or as many cells past the text as it was. Leaves w->ny
 * on the line's last row; 0 when out of memory. */
static int rewrap_line(vt_rewrap *w, vt_line **old, int s, int e, int oc,
                       int cx, int cy, int wp, int *ncx, int *ncy, int *nwp)
{
    int r, x, n, last, width, found = 0;
    long d;
    const vt_cell *c;
    vt_cell *o;
    w->nx = 0;
    if (!rw_row(w))
        return 0;
    last = line_text_len(old[e], oc);
    for (r = s; r <= e; r++) {
        n = r < e ? oc : last;
        for (x = 0; x < n; x++) {
            c = &old[r]->c[x];
            if (c->width == 0)
                continue; /* the right half of a wide glyph moves with its left */
            if (r < e && x == oc - 1 && cell_plain_blank(c) && old[r + 1]->c[0].width == 2 &&
                !(r == cy && x == cx))
                continue; /* put_char's padding before a wide glyph that did not fit */
            width = c->width == 2 && w->cols >= 2 ? 2 : 1;
            if (w->nx + width > w->cols && !rw_wrap(w))
                return 0;
            if (r == cy && (x == cx || (width == 2 && x + 1 == cx))) {
                *ncx = w->nx + (x != cx);
                *ncy = w->ny;
                found = 1;
            }
            if (w->out) {
                o = &w->out[w->ny]->c[w->nx];
                o[0] = *c;
                o[0].width = (vt_u8)width;
                if (width == 2) {
                    o[1] = o[0];
                    o[1].ch = ' ';
                    o[1].width = 0;
                }
            }
            w->nx += width;
            if (w->eager && w->nx >= w->cols && !rw_wrap(w))
                return 0;
        }
    }
    if (cy < s || cy > e)
        return 1;
    *nwp = 0;
    if (found) {
        /* xterm's deferred wrap: the cursor waits on the last character
         * of the line; if that is no longer at the margin it moves past */
        if (wp) {
            if (*ncx + 1 < w->cols)
                (*ncx)++;
            else
                *nwp = 1;
        }
        return 1;
    }
    /* past the text: the rows it runs over stay part of the line */
    d = (long)(cy - e) * oc + cx - last;
    w->nx += d > 0 ? (int)d : 0;
    while (w->nx >= w->cols) {
        d = w->nx - w->cols;
        if (!rw_wrap(w))
            return 0;
        w->nx = (int)d;
    }
    *ncx = w->nx;
    *ncy = w->ny;
    return 1;
}

/* One pass over the primary screen: its logical lines typed again into
 * out (NULL: count only). Rows below both the cursor and the last text
 * are left out (the caller pads with blanks). A double-width or -height
 * row is never joined: it keeps its place, cut or padded. Returns the row
 * count, -1 when out of memory (the rows made so far stay in out). */
static int reflow_pass(vt_term *t, vt_line **out, int cols, int cx, int cy, int wp,
                       int *ncx, int *ncy, int *nwp)
{
    vt_line **old = t->pri, *l;
    vt_rewrap w;
    int s, e, last, n;
    w.t = t;
    w.out = out;
    w.cols = cols;
    w.eager = t->pers == VT_AMIGA && t->autowrap;
    w.ny = 0;
    for (last = t->rows - 1; last > cy; last--)
        if (old[last]->wrapped || old[last]->dbl || line_text_len(old[last], t->cols))
            break;
    for (s = 0; s <= last; s = e + 1) {
        e = s;
        if (old[s]->dbl) {
            if (out) {
                if (!(l = line_new(cols)))
                    return -1;
                line_clear(t, l, cols);
                n = cols < t->cols ? cols : t->cols;
                memcpy(l->c, old[s]->c, n * sizeof(vt_cell));
                if (l->c[cols - 1].width == 2)
                    blank_cell(t, &l->c[cols - 1]);
                l->dbl = old[s]->dbl;
                out[w.ny] = l;
            }
            if (s == cy) {
                *ncx = clampi(cx, 0, cols - 1);
                *ncy = w.ny;
                *nwp = 0;
            }
        } else {
            while (old[e]->wrapped && e + 1 < t->rows && !old[e + 1]->dbl)
                e++;
            if (!rewrap_line(&w, old, s, e, t->cols, cx, cy, wp, ncx, ncy, nwp))
                return -1;
        }
        w.ny++;
    }
    return w.ny;
}

/* vt_resize with reflow, for the primary screen (the alternate one is
 * resized plainly, as xterm does: full-screen programs redraw it anyway).
 * The scrollback is not reflowed either: its lines keep the width they
 * had, which vt_row reports per line, and the amiga personality (the one
 * the ROM console's reflow is copied for) keeps none. Rows that do not fit
 * go from below the cursor first, then off the top into the scrollback,
 * as resize_screen does. 0 when out of memory, nothing changed. */
static int reflow_screen(vt_term *t, int cols, int rows, int *cx, int *cy, int *wp)
{
    vt_line **nr, **nw, **pri = t->pri, **all = 0;
    int total, i, ok, ncx = 0, ncy = 0, nwp = 0, excess, below, drop_top = 0, above = t->novf;
    int orows = t->rows;
    if (above) {
        /* the rows an earlier resize pushed out, then the screen: laid out
         * together, as one screen that starts `above` rows higher */
        all = (vt_line **)VT_MALLOC((above + orows) * sizeof(vt_line *));
        if (!all)
            return 0;
        memcpy(all, t->ovf, above * sizeof(vt_line *));
        memcpy(all + above, pri, orows * sizeof(vt_line *));
        t->pri = all;
        t->rows = above + orows;
        *cy += above;
    }
    total = reflow_pass(t, 0, cols, *cx, *cy, *wp, &ncx, &ncy, &nwp);
    nr = (vt_line **)VT_MALLOC(total * sizeof(vt_line *));
    nw = (vt_line **)VT_MALLOC(rows * sizeof(vt_line *));
    if (!nr || !nw) {
        if (nr)
            VT_FREE(nr);
        if (nw)
            VT_FREE(nw);
        goto undo;
    }
    memset(nr, 0, total * sizeof(vt_line *));
    memset(nw, 0, rows * sizeof(vt_line *));
    ok = reflow_pass(t, nr, cols, *cx, *cy, *wp, &ncx, &ncy, &nwp) >= 0;
    for (i = total; ok && i < rows; i++) { /* the blank rows below */
        nw[i] = line_new(cols);
        if (nw[i])
            line_clear(t, nw[i], cols);
        else
            ok = 0;
    }
    if (!ok) {
        for (i = 0; i < total; i++)
            if (nr[i])
                VT_FREE(nr[i]);
        for (i = total; i < rows; i++)
            if (nw[i])
                VT_FREE(nw[i]);
        VT_FREE(nr);
        VT_FREE(nw);
        goto undo;
    }
    if (total > rows) {
        excess = total - rows;
        below = total - 1 - ncy;
        if (below > excess)
            below = excess;
        drop_top = excess - below;
        for (i = total - below; i < total; i++)
            VT_FREE(nr[i]);
        total = rows;
        ncy -= drop_top;
    }
    /* the old rows (the pushed-out ones among them) are laid out anew */
    for (i = 0; i < t->rows; i++)
        VT_FREE(t->pri[i]);
    t->novf = 0;
    if (all)
        VT_FREE(all);
    t->pri = pri;
    t->rows = orows;
    VT_FREE(t->pri);
    for (i = 0; i < drop_top; i++)
        ovf_push(t, nr[i]); /* pushed out now: back on a later grow */
    for (i = 0; i < total; i++)
        nw[i] = nr[drop_top + i];
    VT_FREE(nr);
    t->pri = nw;
    *cx = ncx;
    *cy = ncy;
    *wp = nwp;
    return 1;
undo:
    if (all) {
        VT_FREE(all);
        t->pri = pri;
        t->rows = orows;
        *cy -= above;
    }
    return 0;
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
                nl->dbl = l->dbl;
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
    int y, cy, alt_active, i, reflowed, wp = 0;
    vt_u8 *tabs;
    if (cols < 1 || rows < 1 || (cols == t->cols && rows == t->rows))
        return;
    flush(t);
    if (cols > t->tabs_cap) {
        tabs = (vt_u8 *)VT_MALLOC(cols);
        if (!tabs)
            return;
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
        int pcx = alt_active ? t->sav_1049.x : t->cx;
        int pwp = alt_active ? t->sav_1049.wrap_pending : t->wrap_pending;
        int acy = alt_active ? t->cy : 0;
        /* Only the primary screen reflows: xterm leaves the alternate one
         * to the full-screen program that owns it, which redraws. */
        reflowed = t->reflow && (cols != t->cols || rows != t->rows || t->novf) &&
                   reflow_screen(t, cols, rows, &pcx, &pcy, &pwp);
        if (!reflowed) {
            resize_screen(t, &t->pri, cols, rows, 1, &pcy);
            pwp = 0;
        }
        if (t->alt)
            resize_screen(t, &t->alt, cols, rows, 0, &acy);
        if (alt_active) {
            t->sav_1049.y = clampi(pcy, 0, rows - 1);
            if (reflowed) {
                t->sav_1049.x = pcx;
                t->sav_1049.wrap_pending = pwp;
            }
            cy = acy;
        } else {
            cy = pcy;
            if (reflowed)
                t->cx = pcx;
            wp = pwp;
        }
    }
    t->scr = alt_active ? t->alt : t->pri;
    t->cols = cols;
    t->rows = rows;
    for (y = 0; y < rows; y++) {
        t->scr[y]->dx0 = 0x7FFF;
        t->scr[y]->dx1 = 0;
    }
    t->top = 0;
    t->bot = rows;
    t->cx = clampi(t->cx, 0, cols - 1);
    t->cy = clampi(cy, 0, rows - 1);
    t->wrap_pending = wp;
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

int vt_row_used(const vt_term *t, int row)
{
    const vt_line *l;
    int n;
    if (row >= 0) {
        if (row >= t->rows)
            return 0;
        l = t->scr[row];
        n = t->cols;
    } else {
        if (-row > t->sb_len)
            return 0;
        l = t->sb[(t->sb_head + t->sb_cap + row) % t->sb_cap];
        n = l->n;
    }
    return l->used < n ? l->used : n;
}

int vt_row_size(const vt_term *t, int row)
{
    return row >= 0 && row < t->rows ? t->scr[row]->dbl : 0;
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

int vt_modify_other_keys(const vt_term *t)
{
    return t->pers == VT_XTERM ? t->mok : 0;
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

/* ASCII case fold: A-Z to a-z, everything else (UTF-8 continuation bytes,
 * PETSCII) as it is. */
static char fold(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

/* The offset of the ASCII-case-insensitive `needle` in the `hlen` bytes of
 * `hay`, or -1. */
static long ci_find(const char *hay, long hlen, const char *needle, long nlen)
{
    long i, k;
    if (nlen == 0 || nlen > hlen)
        return -1;
    for (i = 0; i + nlen <= hlen; i++) {
        for (k = 0; k < nlen; k++)
            if (fold(hay[i + k]) != fold(needle[k]))
                break;
        if (k == nlen)
            return i;
    }
    return -1;
}

long vt_find(const vt_term *t, const char *q, long from)
{
    char buf[VT_FIND_MAX];
    long hist, rows, nq, first, i, span;
    if (!t || !q)
        return VT_ROW_NONE;
    nq = 0;
    while (q[nq])
        nq++;
    if (nq == 0)
        return VT_ROW_NONE;
    hist = vt_scrollback_lines(t);
    rows = vt_rows(t);
    /* reading order as one run: the oldest scrollback line, then each newer
     * one, then the grid */
    if (from < 0)
        first = hist + from;
    else
        first = hist + from;
    if (first < 0)
        first = 0;
    for (i = first; i < hist + rows; i++) {
        long row = i < hist ? -(hist - i) : i - hist;
        long last = row;
        /* a wrapped line is searched whole: take in every row it continues
         * into, stopping at the oldest edge of the run */
        while (last < rows - 1 && vt_row_wrapped(t, last)) {
            last++;
            if (i + (last - row) >= hist + rows)
                break; /* past the newest line */
        }
        span = last - row + 1;
        if (vt_copy_text(t, 0, (int)row, (int)span > 0 ? 32767 : 0,
                         (int)(row + span - 1), buf, sizeof(buf)) > 0 &&
            ci_find(buf, (long)strlen(buf), q, nq) >= 0)
            return row;
    }
    return VT_ROW_NONE;
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

void vt_resolve_colors(const vt_term *t, const vt_cell *c, vt_color *fg, vt_color *bg)
{
    vt_color f = c->fg, b = c->bg, tmp;
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
            f = (vt_color)(f + 8);
        if ((c->attr & VT_ATTR_BLINK) && b < 8)
            b = (vt_color)(b + 8); /* iCE colours */
        break;
    default:
        /* bold-as-bright is on as xterm draws it; a host setting (a profile
         * option) turns just the colour shift off, the bold font style stays */
        if (t->bold_bright && (c->attr & VT_ATTR_BOLD) && f < 8)
            f = (vt_color)(f + 8);
        if (b == VT_COLOR_DEFAULT)
            b = VT_COLOR_DEFAULT_BG;
        break;
    }
    if ((c->attr & VT_ATTR_FAINT) && (f == VT_COLOR_DEFAULT || f == 7 || f == 15)) {
        /* faint: grey (the line editor's suggestions use it); pen 2 on the
         * Amiga console's palette */
        f = t->pers == VT_AMIGA ? 2 : 8;
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

/* modifyOtherKeys' form: CSI 27 ; 1 + mods ; code ~ */
static int mok_report(vt_u8 *out, int mods, long code)
{
    char b[24];
    int k = 0, i;
    b[k++] = 0x1B;
    b[k++] = '[';
    b[k++] = '2';
    b[k++] = '7';
    b[k++] = ';';
    k = fmt_uint(b, k, 1 + mods);
    b[k++] = ';';
    k = fmt_uint(b, k, code);
    b[k++] = '~';
    for (i = 0; i < k; i++)
        out[i] = (vt_u8)b[i];
    return k;
}

/* Return, Tab, Backspace and Escape with modifiers, as xterm sends them:
 * modifyOtherKeys 2 reports every modified one, level 1 those Ctrl or
 * Shift would otherwise lose (Shift+Tab is back-tab and Ctrl+Backspace BS
 * at both levels: keys of their own); without it Alt is the ESC prefix
 * and the rest is the plain key. 0: nothing here (the plain key). */
static int c0_key_modified(const vt_term *t, long key, int mods, vt_u8 *out)
{
    int n = 0;
    long code = key == VT_KEY_RETURN ? 13 : key == VT_KEY_TAB ? 9
              : key == VT_KEY_BACKSPACE ? 0x7F : 0x1B;
    int backtab = key == VT_KEY_TAB && mods == VT_MOD_SHIFT;
    int ctrl_bs = key == VT_KEY_BACKSPACE && (mods & VT_MOD_CTRL);
    if (!mods || t->pers == VT_AMIGA)
        return 0;
    if (t->pers == VT_XTERM && t->mok && !backtab &&
        (t->mok == 2 || ((mods & (VT_MOD_CTRL | VT_MOD_SHIFT)) && !(ctrl_bs && mods == VT_MOD_CTRL))))
        return mok_report(out, mods, code);
    if (!(mods & VT_MOD_ALT) && !ctrl_bs)
        return 0; /* Shift+Tab, or a modifier the plain key does not show */
    if (mods & VT_MOD_ALT)
        out[n++] = 0x1B; /* Meta: readline's M-DEL, M-RET, M-TAB, M-ESC */
    if (ctrl_bs && t->pers == VT_XTERM) {
        out[n++] = 0x08; /* xterm: Ctrl+Backspace is BS */
        return n;
    }
    return n + vt_encode_key(t, key, mods & ~(VT_MOD_ALT | VT_MOD_CTRL), out + n);
}

int vt_encode_key(const vt_term *t, long key, int mods, vt_u8 *out)
{
    int n = 0;
    int app = (t->modes & VT_MODE_APP_CURSOR) != 0;

    if (key < 0x110000) { /* a character */
        long c = key;
        /* modifyOtherKeys: CSI 27 ; mod ; code ~ for what the plain forms
         * cannot say -- level 2 every modified key, level 1 the ambiguous;
         * Shift alone is never one (the character already says it) */
        if (t->pers == VT_XTERM && t->mok && (mods & ~VT_MOD_SHIFT) && key < 0x110000 &&
            (t->mok == 2 ||
             ((mods & VT_MOD_CTRL) && ((mods & VT_MOD_SHIFT) ||
                                       !((c >= 'a' && c <= 'z') || (c >= '@' && c <= '_') ||
                                         c == ' ' || c == '?')))))
            return mok_report(out, mods, c);
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
        if ((mods & VT_MOD_ALT) && t->pers == VT_XTERM && (t->modes & VT_MODE_META_8BIT) &&
            c < 0x80) {
            out[n++] = (vt_u8)(c | 0x80); /* ?1034: Meta sets the 8th bit */
            return n;
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

    if ((key == VT_KEY_RETURN || key == VT_KEY_TAB || key == VT_KEY_BACKSPACE ||
         key == VT_KEY_ESCAPE) && (n = c0_key_modified(t, key, mods, out)) != 0)
        return n;
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
        if (t->pers == VT_XTERM && (t->modes & VT_MODE_APP_ESCAPE)) {
            out[n++] = 'O'; /* ?7727: never mistaken for the start of a sequence */
            out[n++] = '[';
        }
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
    } else if (m & VT_MODE_MOUSE_UTF8) {
        /* ?1005: each value one UTF-8 character, to 2015 */
        long v[3];
        int k;
        v[0] = 32 + cb;
        v[1] = 33 + x;
        v[2] = 33 + y;
        if (v[1] > 2047 || v[2] > 2047)
            return 0;
        b[n++] = 'M';
        for (k = 0; k < 3; k++)
            n += put_utf8((vt_u8 *)b + n, v[k]);
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
