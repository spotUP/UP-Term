/* vtengine -- see vtengine.h. The parser is Paul Williams' DEC-compatible
 * state machine (vt100.net/emu/dec_ansi_parser); the dispatch tables below
 * it are per personality, because the Amiga console and xterm give the same
 * CSI finals different meanings (the conformance matrix in
 * thoughts/shared/research/2026-09-28_console-conformance-matrix.md). */
#include "vtengine.h"
#include "vtwidth.h"
#include <stddef.h>
#include <string.h>

#ifdef __VBCC__
/* vbcc's -O2 is -O=1023. With it, a loop over vt_term fields got every
 * field's address (t + offset) hoisted out of the loop and, short of
 * registers, spilled to the stack: each t->x became a load of the address
 * and then the access, and vt_feed set up fifteen such slots on every
 * call -- though (d16,An) addressing is free on a 68k. -O=991 (bit 32 off)
 * keeps the accesses in place: engbench plain lines with 79-byte writes
 * 19.4 -> 16.5 instructions a byte under vamos (ASM1; -O=895, bit 128
 * off, does the same but warns 172 in feed()). It sets the whole mask:
 * this file builds at this level whatever -O the command line gives. */
#pragma opt 991
#endif

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

#define VT_CLUSTERS 2048          /* VT_CLUSTER_FIRST..LAST */
#define VT_CLU_HASH 4096          /* a power of two, at least twice VT_CLUSTERS */
#define VT_MAX_PARAMS 16
#define VT_STR_MAX 256
#define VT_PARAM_MAX 65535L
#define VT_LINKS_MAX 63 /* OSC 8 links the cells may name at once */

/* Parser states. */
enum {
    S_GROUND, S_ESC, S_ESC_INT, S_CSI_ENTRY, S_CSI_PARAM, S_CSI_INT,
    S_CSI_IGNORE, S_OSC, S_STRING, /* DCS, SOS, PM, APC: consumed, ignored */
    S_SIXEL, /* DCS P1;P2;P3 q: the bytes go to the sixel decoder (after S_STRING:
              * feed tests state >= S_OSC once for all three) */
    S_PRINT /* printer controller mode (CSI 5 i): the bytes go to no printer
             * (feed handles it before anything else) */
};

/* A grid line: one allocation, cells follow the header. */
typedef struct vt_line {
    vt_u16 cap;      /* cells allocated */
    vt_u16 n;        /* cells in use (== cols for grid lines) */
    vt_u8 wrapped;   /* the text continues on the next line */
    vt_u8 dbl;       /* DEC line size: 0 single, VT_LINE_DOUBLE_WIDTH, _TOP, _BOTTOM */
    vt_u8 mark;      /* OSC 133: VT_MARK_* that started on this line */
    vt_u8 chonly;    /* 1: the cells [0, used) differ from the default blank in
                      * their ch alone (plain text written by put_ascii_run's
                      * plain case), so a clear resets only ch. mark() makes
                      * it 0; line_clear to the default blank makes it 1.
                      * (ASM1: a recycled line's clear wrote 16 bytes a cell
                      * where 2 had changed.) */
    vt_u16 used;    /* cells [used, n) are still the default blank the last
                      * line_clear wrote: clearing again need not touch them.
                      * mark() -- every change to a grid cell passes it --
                      * raises it; a line whose cells are not known (new, or
                      * cleared in a colour) has used == cap. (S1: a scroll
                      * cleared 80 cells a line, 0.69 ms on a 14 MHz 68020.) */
    short dx0, dx1;  /* the row's cells [dx0, dx1) changed since the last flush
                      * (empty when dx0 >= dx1). Kept in the line: a scroll
                      * moves the lines, and their spans with them. */
    vt_u16 img;      /* the images placed on the row: the first of its
                      * entries in vt_term.pl (0 none). In the line, so the
                      * images go wherever a scroll or the scrollback takes it */
    vt_cell c[1];
} vt_line;

/* An image's tile row on a line: cells from col0 show row `row` (in cells)
 * of image slot `img`, while the slot's gen is still `gen`. */
typedef struct vt_place {
    vt_u16 img, gen, row, next; /* next: the line's next entry, or the free list's */
    short col0;
} vt_place;

/* A decoded image (vt_term.imgs; pix 0 = a free slot). */
typedef struct vt_image {
    vt_u8 *pix;         /* w x h indices into pal */
    long bytes;         /* what pix took, counted against VT_IMAGE_MEMORY */
    long serial;
    vt_u32 pal[256];    /* 0xRRGGBB; 0 the unset pixels */
    int w, h, cw, ch, npal, refs;
    vt_u16 gen;
} vt_image;

#define VT_IMG_SLOTS 64 /* images alive at once (the oldest gives way) */

struct vt_sixel; /* the decoder of the sixel being received */

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
    /* The primary screen's rows as a window sliding through twice as many
     * slots (pri_mem; 0: pri is a plain array): a scroll of the whole
     * screen moves the window by one slot instead of every row pointer,
     * and once a page has gone by the window goes back to the start --
     * O(1) a line (S1; CCON 1.2.8 does the same). pri_spare: slots left
     * below the window. */
    vt_line **pri_mem;
    int pri_spare;
    vt_u8 bs_default;      /* vt_set_backspace_bs: what RIS gives ?67 back */
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
    struct { vt_color ul; vt_u8 font; vt_u8 link; } styles[255]; /* link: OSC 8, 1 + links[] */
    /* OSC 8 hyperlinks the cells name through their style entry; link is
     * the one new cells get (0 none). OSC 7 cwd: the last one reported. */
    struct { char *uri; char id[24]; } links[VT_LINKS_MAX];
    int n_links;
    vt_u8 link;
    char *cwd;
    int n_styles;
    /* the cluster table (VT_CLUSTER_FIRST): made on the first character
     * that needs it, grown to VT_CLUSTERS entries, swept when full */
    struct vt_cluster *clu;
    int clu_cap, clu_top;      /* entries allocated; entries ever used (the rest never were) */
    int clu_free;              /* a free entry below clu_top is searched from here */
    vt_u16 *clu_hash;          /* VT_CLU_HASH slots: entry + 1, 0 empty */
    int top, bot;          /* scroll region rows [top, bot) */
    vt_u8 *tabs;
    int tabs_cap;

    vt_u32 modes;
    int autowrap, origin, insert;
    vt_u8 charset[4];      /* 'B' ASCII, '0' DEC graphics, 'A' UK */
    int gl, single_shift;
    int gr;                /* LS1R-LS3R: G1-G3 in GR (8-bit windows); 0 Latin-1 itself */
    int prn_match;         /* S_PRINT: how much of CSI 4 i has come */
    vt_saved sav, sav_1049;
    vt_u8 saved_1048;      /* ?1048 / ?1049 saved a cursor (DECRQM ?1048) */
    vt_u8 rect_extent;     /* DECSACE 2: DECCARA / DECRARA cover a rectangle, not a stream */
    vt_u32 last_ch;            /* REP repeats it: the last character put */

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
    /* OSC 52: its payload streams past str, base64 decoded as it comes
     * into a buffer that grows to VT_CLIP_MAX (osc52_byte) */
    vt_u8 osc52;               /* 0 not an OSC 52; 1 its selection; 2 its data;
                                * 3 an OSC 7 / 8 (long_cmd) kept whole in clip */
    vt_u8 long_cmd;
    vt_u8 clip_bad, clip_query;
    char clip_sel[8];
    int clip_nsel;
    vt_u8 *clip;
    long clip_len, clip_cap;
    vt_u32 b64_acc;
    int b64_n, b64_pad;
    int clip_access;           /* VT_CLIP_*: what the host lets OSC 52 do */

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
    vt_u8 cursor_dflt;         /* the host's DECSCUSR default (vt_set_cursor_style): RIS's */
    vt_u8 allow_cols;          /* ?40: DECCOLM may change the width */
    vt_u8 mok;                 /* modifyOtherKeys level, CSI > 4 ; n m */
    vt_u8 kbd[2][8];           /* kitty keyboard flags, a stack per screen (0 main, 1 alternate) */
    vt_u8 kbd_top[2];          /* its top entry: the flags in force */
    vt_u8 scheme;              /* the last dark (1) / light (2) scheme reported */

    /* images (sixel): the slots, their placements' pool (entry 0 unused,
     * pl_free the free list), the pixels all images hold, a counter for
     * serials; the decoder while a sixel arrives; the shared colour
     * registers (?1070 off; 0 until used) */
    vt_image *imgs[VT_IMG_SLOTS];
    int n_img;
    long img_mem, img_serial;
    vt_place *pl;
    long pl_cap;
    vt_u16 pl_free;
    struct vt_sixel *six;
    vt_u32 *six_regs;
    vt_u8 sixel_dm;            /* ?80 DECSDM: images at the top left, no scrolling */
    vt_u8 six_private;         /* ?1070: each image its own colour registers (default) */

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
        l->img = 0;
        l->mark = 0;
        l->chonly = 0;
    }
    return l;
}

/* ---- images on lines (vt_line.img) --------------------------------------- */

static void image_free(vt_term *t, int slot)
{
    vt_image *im = t->imgs[slot];
    if (!im || !im->pix)
        return;
    VT_FREE(im->pix);
    im->pix = 0;
    t->img_mem -= im->bytes;
    im->bytes = 0;
    im->refs = 0;
    im->gen++; /* the placements left naming it are stale now */
    t->n_img--;
}

/* The image a placement shows, 0 when it went (evicted, its slot reused). */
static vt_image *place_image(const vt_term *t, const vt_place *p)
{
    vt_image *im = t->imgs[p->img];
    return im && im->pix && im->gen == p->gen ? im : 0;
}

static int image_cols(const vt_image *im)
{
    return (im->w + im->cw - 1) / im->cw;
}

/* Placement i back to the pool, its image released. */
static void place_release(vt_term *t, vt_u16 i)
{
    vt_place *p = &t->pl[i];
    vt_image *im = place_image(t, p);
    if (im && --im->refs <= 0)
        image_free(t, p->img);
    p->next = t->pl_free;
    t->pl_free = i;
}

/* Every image on line l let go (the line is cleared or freed). */
static void place_drop(vt_term *t, vt_line *l)
{
    vt_u16 i = l->img, next;
    l->img = 0;
    while (i) {
        next = t->pl[i].next;
        place_release(t, i);
        i = next;
    }
}

/* A line's memory back, and its images let go. */
static void line_free(vt_term *t, vt_line *l)
{
    if (l->img)
        place_drop(t, l);
    VT_FREE(l);
}

/* Image slot `slot` (gen) at col0 on line l, tile row `row`, after the
 * line's others (drawn on top of them). 0 when the pool cannot grow. */
static int place_link(vt_term *t, vt_line *l, int slot, vt_u16 gen, int col0, int row)
{
    vt_u16 i, *at;
    vt_place *p;
    if (!t->pl_free) {
        long cap = t->pl_cap ? t->pl_cap * 2 : 64, k;
        vt_place *n;
        if (cap > 65535L)
            cap = 65535L;
        if (cap <= t->pl_cap)
            return 0;
        n = (vt_place *)VT_MALLOC(cap * sizeof(vt_place));
        if (!n)
            return 0;
        if (t->pl_cap)
            memcpy(n, t->pl, t->pl_cap * sizeof(vt_place));
        for (k = cap - 1; k >= (t->pl_cap ? t->pl_cap : 1); k--) {
            n[k].next = t->pl_free;
            t->pl_free = (vt_u16)k;
        }
        if (t->pl)
            VT_FREE(t->pl);
        t->pl = n;
        t->pl_cap = cap;
    }
    i = t->pl_free;
    p = &t->pl[i];
    t->pl_free = p->next;
    p->img = (vt_u16)slot;
    p->gen = gen;
    p->row = (vt_u16)row;
    p->col0 = (short)col0;
    p->next = 0;
    for (at = &l->img; *at; at = &t->pl[*at].next)
        ;
    *at = i;
    t->imgs[slot]->refs++;
    return 1;
}

/* A new image on line l over the columns [col0, col0 + its width): the
 * line's images it covers whole (and the stale ones) go first, so a
 * program drawing frame after frame in one place keeps one image. */
static void place_add(vt_term *t, vt_line *l, int slot, int col0, int row)
{
    vt_image *im = t->imgs[slot], *o;
    int c1 = col0 + image_cols(im);
    vt_u16 *at = &l->img, i;
    while (*at) {
        i = *at;
        o = place_image(t, &t->pl[i]);
        if (!o || (t->pl[i].col0 >= col0 && t->pl[i].col0 + image_cols(o) <= c1)) {
            *at = t->pl[i].next;
            place_release(t, i);
        } else {
            at = &t->pl[i].next;
        }
    }
    place_link(t, l, slot, im->gen, col0, row);
}

/* Reflow: the image cell at column x of `from` moved to column nx of `to`;
 * the image tile it shows goes along (once per run of cells). */
static void place_carry(vt_term *t, const vt_line *from, int x, vt_line *to, int nx)
{
    vt_u16 i, last = 0;
    int slot = -1, row = 0, col0 = 0;
    vt_u16 gen = 0;
    const vt_image *im;
    for (i = from->img; i; i = t->pl[i].next) {
        im = place_image(t, &t->pl[i]);
        if (im && x >= t->pl[i].col0 && x < t->pl[i].col0 + image_cols(im)) {
            slot = t->pl[i].img; /* the newest one covering x: it is on top */
            gen = t->pl[i].gen;
            row = t->pl[i].row;
            col0 = nx - (x - t->pl[i].col0);
        }
    }
    if (slot < 0)
        return;
    for (i = to->img; i; i = t->pl[i].next)
        last = i;
    if (last && t->pl[last].img == slot && t->pl[last].gen == gen && t->pl[last].row == row &&
        t->pl[last].col0 == col0)
        return;
    place_link(t, to, slot, gen, col0, row);
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
long vt_asm_put_ch(vt_cell *c, const vt_u8 *b, long n);
void vt_asm_fill(vt_cell *c, long n, const vt_cell *proto);
void vt_asm_ch_blank(vt_cell *c, long n);
long vt_asm_csi(const vt_u8 *p, long n, long *params, vt_u8 *sub);
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

static int vacated_default_at(const vt_term *t);
/* the amiga dialect's answer (always) in place, the rest a call */
#define vacated_default(t) ((t)->pers == VT_AMIGA || vacated_default_at(t))

static void line_clear(vt_term *t, vt_line *l, int n)
{
    int dflt = vacated_default(t);
    if (l->img)
        place_drop(t, l); /* one test a line: no images, no cost */
    if (!l->used && l->n == n && dflt) {
        /* nothing written since its last clear: already blank (a flood of
         * newlines clears a blank line each, S1) */
        l->wrapped = 0;
        l->dbl = 0;
        return;
    }
    if (dflt && l->n == n && (l->used < n || l->chonly)) {
        /* the same width as its last clear, and the blank is the default
         * one: only the cells written since need it (none after a flood of
         * newlines, S1), and of plain text only the characters (ASM1) */
#ifdef VT_CHECK_USED
        int i;
        vt_cell b;
        blank_cell(t, &b);
        for (i = 0; i < n; i++)
            if ((i >= l->used && l->c[i].ch != b.ch) || (i < l->used && l->chonly && l->c[i].ch > 0xFF) ||
                ((i >= l->used || l->chonly) &&
                 (l->c[i].fg != b.fg || l->c[i].bg != b.bg || l->c[i].attr != b.attr || l->c[i].width != b.width ||
                  l->c[i].deco != b.deco || l->c[i].ext != b.ext || l->c[i].pad != b.pad)))
                abort(); /* a cell changed without mark(): host tests only */
#endif
        if (l->chonly) {
#ifdef VT_ASM
            vt_asm_ch_blank(l->c, l->used);
#else
            int k;
            for (k = 0; k < l->used; k++)
                l->c[k].ch = ' ';
#endif
        } else {
            cells_blank(t, l->c, l->used);
        }
    } else {
        cells_blank(t, l->c, n);
    }
    l->used = (vt_u16)(dflt ? 0 : n);
    l->chonly = (vt_u8)dflt;
    l->n = (vt_u16)n;
    l->wrapped = 0;
    l->dbl = 0;
    l->mark = 0;
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
        l->chonly = 0;
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

/* Overwriting either half of a wide glyph blanks the other half. unwide()
 * is the cell test in place, the call only for a wide glyph's half (ASM1:
 * every erase and character paid a call for the common plain cell). */
static void unwide_at(vt_term *t, int x, int y)
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
#define unwide(t, x, y)                         \
    do {                                        \
        if ((t)->scr[y]->c[x].width != 1)       \
            unwide_at(t, x, y);                 \
    } while (0)

/* The renderer's scroll fills the rows it vacates with the default
 * background (the contract in vtengine.h), so blank rows in that colour
 * need no drawing: only a BCE erase in another colour does. */
static int vacated_default_at(const vt_term *t)
{
    /* blank_cell's answer without building the cell: it is asked on every
     * scroll and line clear (S1: a third of a newline's instructions); the
     * amiga dialect's (always) in place, no call (ASM1) */
    if (t->pers == VT_AMIGA)
        return 1;
    if (t->bg != VT_COLOR_DEFAULT)
        return 0;
    return t->pers != VT_PCANSI || !(t->attr & (VT_ATTR_BLINK | VT_ATTR_INVERSE));
}

static void damage_rows(vt_term *t, int y0, int y1);

/* Before the grid moves: a pending scroll of another region or direction
 * cannot be added to. Its rows are drawn again from the grid at the flush
 * instead of being moved on screen -- no drawing in the middle of a
 * write: an insert-line / delete-line pair was two blits of its own each
 * time (conbench insdel-line, 9.5 ms a pair on the stock rig; S1). */
static void pend_drop(vt_term *t)
{
    t->pend_n = 0;
    damage_rows(t, t->pend_top, t->pend_bot);
}
/* the test in place, the call only when the pending scroll goes (a pending
 * scroll exists only with cb.scroll: pend_scroll) -- every newline of
 * scrolling output asked it (ASM1) */
#define pend_prepare(t, top, bot, n)                                                                  \
    do {                                                                                              \
        if ((t)->pend_n && ((t)->pend_top != (top) || (t)->pend_bot != (bot) || ((t)->pend_n > 0) != ((n) > 0))) \
            pend_drop(t);                                                                             \
    } while (0)

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
        line_free(t, l);
        return;
    }
    if (t->sb_len == t->sb_cap)
        line_free(t, t->sb[t->sb_head]);
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
            line_free(t, t->ovf[i]);
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
                line_free(t, t->ovf[0]);
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

/* The primary screen's window back at the start of its slots (pri_mem):
 * before anything that frees or replaces t->pri, and when the window has
 * reached the end. */
static void pri_settle(vt_term *t)
{
    int y;
    if (!t->pri_mem || t->pri == t->pri_mem)
        return;
    for (y = 0; y < t->rows; y++) /* forward: the window lies above its new place */
        t->pri_mem[y] = t->pri[y];
    if (t->scr == t->pri)
        t->scr = t->pri_mem;
    t->pri = t->pri_mem;
    t->pri_spare = t->rows;
}

/* The primary screen's rows in twice as many slots, made on its first
 * whole-screen scroll (and again after a resize replaced the array). No
 * memory: the rows stay a plain array, scrolled by moving them. */
static void pri_widen(vt_term *t)
{
    vt_line **m = (vt_line **)VT_MALLOC(2 * t->rows * sizeof(vt_line *));
    int y;
    if (!m)
        return;
    for (y = 0; y < t->rows; y++)
        m[y] = t->pri[y];
    if (t->scr == t->pri)
        t->scr = m;
    VT_FREE(t->pri);
    t->pri = t->pri_mem = m;
    t->pri_spare = t->rows;
}

/* Rows [top, bot) move up by n; n blank rows enter at the bottom. Lines
 * leaving the top of the primary screen go to the scrollback. */
static void scroll_up(vt_term *t, int top, int bot, int n)
{
    int i, h = bot - top, slide, to_sb, dflt;
    vt_line *l;
    if (n <= 0 || h <= 0)
        return;
    if (n > h)
        n = h;
    pend_prepare(t, top, bot, n);
    slide = top == 0 && bot == t->rows && t->scr == t->pri && h > 1;
    if (slide && !t->pri_mem)
        pri_widen(t);
    slide = slide && t->pri_mem;
    if (top == 0 && t->scr == t->pri) {
        t->scrolled += n;
        if (t->novf)
            ovf_drop(t); /* above the rows scrolling out: history first */
    }
    /* the lines leaving go to the scrollback (the loop keeps scr == pri) */
    to_sb = top == 0 && t->scr == t->pri && t->pers != VT_AMIGA && t->sb_cap;
    dflt = vacated_default(t);
    for (i = 0; i < n; i++) {
        vt_line **p = &t->scr[top];
        l = *p;
        if (slide) {
            if (!t->pri_spare)
                pri_settle(t);
            t->pri++;
            t->pri_spare--;
            t->scr = t->pri;
        } else {
#ifdef VT_ASM
            vt_asm_rows_up(p, h - 1);
#else
            int k;
            for (k = h - 1; k > 0; k--, p++)
                p[0] = p[1];
#endif
        }
        if (to_sb) {
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
                        line_free(t, t->sb[t->sb_head]);
                    else
                        t->sb_len++;
                }
            }
            if (blank) { /* no memory: the line is dropped, not saved */
                t->sb[t->sb_head] = l;
                if (++t->sb_head == t->sb_cap)
                    t->sb_head = 0;
                l = blank;
            }
        }
        if (!l->used && dflt && !l->img && l->n == t->cols) {
            /* line_clear's first case in place: nothing written since its
             * last clear (a flood of newlines; ASM1) */
            l->wrapped = 0;
            l->dbl = 0;
        } else {
            line_clear(t, l, t->cols);
        }
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
    vt_line *l;
    x0 = clampi(x0, 0, t->cols);
    x1 = clampi(x1, 0, t->cols);
    if (x0 >= x1)
        return;
    unwide(t, x0, y);
    if (x1 < t->cols)
        unwide(t, x1 - 1, y);
    l = t->scr[y];
    if (x1 == t->cols && !l->img && vacated_default(t)) {
        /* to the end of the row with the default blank: the cells from
         * `used` on are that blank already, so only [x0, used) changes --
         * in its characters alone on a chonly line -- and only that needs
         * drawing (ASM1: a repaint's CSI K blanked and drew 80 cells
         * where 50 had text). The row is unused again from x0. */
        int u = l->used < t->cols ? l->used : t->cols;
        l->wrapped = 0;
        if (u > x0) {
            if (l->chonly) {
#ifdef VT_ASM
                vt_asm_ch_blank(&l->c[x0], u - x0);
#else
                int k;
                for (k = x0; k < u; k++)
                    l->c[k].ch = ' ';
#endif
            } else {
                cells_blank(t, &l->c[x0], u - x0);
            }
            if (l->dx0 > x0)
                l->dx0 = (short)x0;
            if (l->dx1 < u)
                l->dx1 = (short)u;
            l->used = (vt_u16)x0;
            t->dirty = 1;
        }
        if (!x0)
            l->chonly = 1; /* (after the blanking) all of it the default blank: [0, used) is empty */
        return;
    }
    cells_blank(t, &t->scr[y]->c[x0], x1 - x0);
    if (x1 == t->cols) {
        t->scr[y]->wrapped = 0;
        if (!x0 && t->scr[y]->img)
            place_drop(t, t->scr[y]); /* the whole row erased: its images go */
    }
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
        t->scr[y]->mark = 0;  /* and no prompt starts on it */
    }
}

static void move_to(vt_term *t, int x, int y)
{
    /* home_limits, clampi and row_cols in place: CUP is in every screen
     * update (ASM1) */
    int y0 = 0, y1 = t->rows - 1, x1;
    if (t->origin) {
        y0 = t->top;
        y1 = t->bot - 1;
    }
    if (y < y0)
        y = y0;
    else if (y > y1)
        y = y1;
    t->cy = y;
    x1 = (t->scr[y]->dbl ? (t->cols + 1) / 2 : t->cols) - 1;
    if (x < 0)
        x = 0;
    else if (x > x1)
        x = x1;
    t->cx = x;
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

/* k line feeds at once, as k of exec_c0's LF: the cursor down to the
 * region's last row, the rest one scroll_up of that many lines (a page
 * at a time, so the scrollback gets every line) -- one call for a run of
 * newlines instead of the whole path for each (conbench scroll-nl writes
 * ten a time; S1). */
static void lf_run(vt_term *t, long k)
{
    t->wrap_pending = 0;
    if (t->cy >= t->bot) { /* below the region: down to the last row, no scroll */
        t->cy = (int)(t->cy + k < t->rows - 1 ? t->cy + k : t->rows - 1);
    } else {
        long d = t->bot - 1 - t->cy;
        if (k <= d) {
            t->cy += (int)k;
        } else {
            t->cy = t->bot - 1;
            k -= d;
            if (t->pers != VT_AMIGA || t->scroll_enabled) {
                int h = t->bot - t->top;
                while (k > 0) {
                    int n = k > h ? h : (int)k;
                    scroll_up(t, t->top, t->bot, n);
                    k -= n;
                }
            }
        }
    }
    if ((t->modes & VT_MODE_NEWLINE) || t->onlcr)
        t->cx = 0;
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

/* Every line that holds cells a table entry may be named by: the grid, the
 * alternate screen, the scrollback and the rows a reflow keeps above the
 * screen. The sweeps of the side tables (styles here, clusters below)
 * walk them all, or an entry still in use is taken for free. */
static void each_line(vt_term *t, void (*fn)(vt_line *, void *), void *u)
{
    int i;
    for (i = 0; i < t->rows; i++) {
        fn(t->pri[i], u);
        if (t->alt)
            fn(t->alt[i], u);
    }
    for (i = 0; i < t->sb_len; i++)
        fn(t->sb[(t->sb_head + t->sb_cap - t->sb_len + i) % t->sb_cap], u);
    for (i = 0; i < t->novf; i++)
        fn(t->ovf[i], u);
}

/* sweep_styles: with used, the entries the line names; else renumbered */
typedef struct {
    vt_u8 *used;
    const vt_u8 *remap;
} vt_style_sweep;

static void sweep_line_styles(vt_line *l, void *u)
{
    vt_style_sweep *s = (vt_style_sweep *)u;
    int x;
    for (x = 0; x < l->n; x++) {
        if (!l->c[x].ext)
            continue;
        if (s->used)
            s->used[l->c[x].ext] = 1;
        else
            l->c[x].ext = s->remap[l->c[x].ext];
    }
}

static void sweep_styles(vt_term *t)
{
    vt_u8 used[256], remap[256];
    vt_style_sweep s;
    int i, k = 0;
    memset(used, 0, sizeof(used));
    s.used = used;
    s.remap = 0;
    each_line(t, sweep_line_styles, &s);
    remap[0] = 0;
    for (i = 1; i <= t->n_styles; i++) {
        remap[i] = 0;
        if (used[i]) {
            t->styles[k] = t->styles[i - 1];
            remap[i] = (vt_u8)++k;
        }
    }
    t->n_styles = k;
    s.used = 0;
    s.remap = remap;
    each_line(t, sweep_line_styles, &s);
}

/* The entry for the current underline colour and font (0: both default). */
static vt_u8 style_index(vt_term *t)
{
    int i, pass;
    if (t->ul == VT_COLOR_DEFAULT && !t->font && !t->link)
        return 0;
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < t->n_styles; i++)
            if (t->styles[i].ul == t->ul && t->styles[i].font == t->font && t->styles[i].link == t->link)
                return (vt_u8)(i + 1);
        if (t->n_styles < 255) {
            t->styles[t->n_styles].ul = t->ul;
            t->styles[t->n_styles].font = t->font;
            t->styles[t->n_styles].link = t->link;
            return (vt_u8)++t->n_styles;
        }
        sweep_styles(t);
    }
    return 0; /* 255 distinct styles on screen at once: drawn plain */
}
/* the cells' ext for the current pen: style_index's first answer (both
 * default: 0) in place, the call only for a rare style (ASM1) */
#define pen_ext(t) ((t)->ul == VT_COLOR_DEFAULT && !(t)->font && !(t)->link ? 0 : style_index(t))

vt_color vt_cell_underline_color(const vt_term *t, const vt_cell *c)
{
    return c->ext && c->ext <= t->n_styles ? t->styles[c->ext - 1].ul : VT_COLOR_DEFAULT;
}

int vt_cell_font(const vt_term *t, const vt_cell *c)
{
    return c->ext && c->ext <= t->n_styles ? t->styles[c->ext - 1].font : 0;
}

/* ---- clusters: characters beyond the BMP, combining marks -------------------
 * A cell's ch in VT_CLUSTER_FIRST..LAST is entry ch - VT_CLUSTER_FIRST of
 * this table. Equal clusters share an entry (found through a hash). Cells
 * are copied, moved and dropped by scrolls, erases, resizes and reflows
 * without a word to the table: when it is full, a sweep over every line
 * (each_line) frees the entries no cell names any more. A free entry has
 * n == 0. Nothing here runs for printable ASCII (put_ascii_run). */
typedef struct vt_cluster {
    vt_u8 n;                    /* code points in cp; 0: a free entry */
    vt_u32 cp[VT_CLUSTER_CPS];
} vt_cluster;

int vt_cell_text(const vt_term *t, const vt_cell *c, vt_u32 *cp)
{
    if (VT_CELL_IS_CLUSTER(c)) {
        int e = c->ch - VT_CLUSTER_FIRST, i;
        if (e < t->clu_top && t->clu[e].n) {
            for (i = 0; i < t->clu[e].n; i++)
                cp[i] = t->clu[e].cp[i];
            return t->clu[e].n;
        }
        cp[0] = 0xFFFD;
        return 1;
    }
    cp[0] = c->ch;
    return 1;
}

vt_u32 vt_cell_char(const vt_term *t, const vt_cell *c)
{
    vt_u32 cp[VT_CLUSTER_CPS];
    if (!VT_CELL_IS_CLUSTER(c))
        return c->ch;
    vt_cell_text(t, c, cp);
    return cp[0];
}

int vt_cell_utf8(const vt_term *t, const vt_cell *c, char *out)
{
    vt_u32 cp[VT_CLUSTER_CPS];
    vt_u8 u[4];
    int i, k, m, len = 0, n = vt_cell_text(t, c, cp);
    for (i = 0; i < n; i++) {
        m = put_utf8(u, (long)cp[i]);
        for (k = 0; k < m; k++)
            out[len++] = (char)u[k];
    }
    return len;
}

static unsigned clu_hash_of(const vt_u32 *cp, int n)
{
    vt_u32 h = (vt_u32)n;
    int i;
    for (i = 0; i < n; i++)
        h = ((h ^ cp[i]) * 0x9E3779B1UL) & 0xFFFFFFFFUL;
    return (unsigned)(h >> 16) & (VT_CLU_HASH - 1);
}

static void clu_hash_put(vt_term *t, int e)
{
    unsigned h = clu_hash_of(t->clu[e].cp, t->clu[e].n);
    while (t->clu_hash[h])
        h = (h + 1) & (VT_CLU_HASH - 1);
    t->clu_hash[h] = (vt_u16)(e + 1);
}

static void sweep_line_clusters(vt_line *l, void *u)
{
    vt_u8 *live = (vt_u8 *)u;
    int x;
    for (x = 0; x < l->n; x++)
        if (VT_CELL_IS_CLUSTER(&l->c[x])) {
            int e = l->c[x].ch - VT_CLUSTER_FIRST;
            live[e >> 3] |= (vt_u8)(1 << (e & 7));
        }
}

/* The entries no line names any more become free; the hash is made again
 * from those left. Returns how many are free. */
static int sweep_clusters(vt_term *t)
{
    vt_u8 live[VT_CLUSTERS / 8];
    int e, freed = 0;
    memset(live, 0, sizeof(live));
    each_line(t, sweep_line_clusters, live);
    memset(t->clu_hash, 0, VT_CLU_HASH * sizeof(vt_u16));
    for (e = 0; e < t->clu_top; e++) {
        if (!(live[e >> 3] & (1 << (e & 7))))
            t->clu[e].n = 0;
        if (t->clu[e].n)
            clu_hash_put(t, e);
        else
            freed++;
    }
    t->clu_free = 0;
    return freed;
}

/* A free entry: one never used, then a grown table, then one a sweep
 * freed. -1 when every entry is named by a cell (on screen, in the
 * scrollback). */
static int clu_alloc(vt_term *t)
{
    int pass;
    for (pass = 0; pass < 2; pass++) {
        if (t->clu_top < t->clu_cap)
            return t->clu_top++;
        if (t->clu_cap < VT_CLUSTERS) {
            int cap = t->clu_cap ? t->clu_cap * 2 : 64;
            vt_cluster *n = (vt_cluster *)VT_MALLOC(cap * sizeof(vt_cluster));
            if (n) {
                if (t->clu_top)
                    memcpy(n, t->clu, t->clu_top * sizeof(vt_cluster));
                if (t->clu)
                    VT_FREE(t->clu);
                t->clu = n;
                t->clu_cap = cap;
                return t->clu_top++;
            }
        }
        for (; t->clu_free < t->clu_top; t->clu_free++)
            if (!t->clu[t->clu_free].n)
                return t->clu_free++;
        if (pass == 0 && !sweep_clusters(t))
            break;
    }
    return -1;
}

/* The cell value for the code points cp[0..n): an entry of the table,
 * shared with an equal cluster. -1 when the table has no room. */
static int cluster_of(vt_term *t, const vt_u32 *cp, int n)
{
    unsigned h;
    int e, i;
    if (!t->clu_hash) {
        t->clu_hash = (vt_u16 *)VT_MALLOC(VT_CLU_HASH * sizeof(vt_u16));
        if (!t->clu_hash)
            return -1;
        memset(t->clu_hash, 0, VT_CLU_HASH * sizeof(vt_u16));
    }
    for (h = clu_hash_of(cp, n); t->clu_hash[h]; h = (h + 1) & (VT_CLU_HASH - 1)) {
        const vt_cluster *c = &t->clu[t->clu_hash[h] - 1];
        if (c->n != n)
            continue;
        for (i = 0; i < n && c->cp[i] == cp[i]; i++)
            ;
        if (i == n)
            return VT_CLUSTER_FIRST + t->clu_hash[h] - 1;
    }
    e = clu_alloc(t);
    if (e < 0)
        return -1;
    t->clu[e].n = (vt_u8)n;
    for (i = 0; i < n; i++)
        t->clu[e].cp[i] = cp[i];
    clu_hash_put(t, e);
    return VT_CLUSTER_FIRST + e;
}

/* A code point of width 0 (a combining mark, a variation selector, a
 * joiner) joins the character left of the cursor -- the cell the cursor
 * waits on after the last column -- as xterm does. At the start of a row
 * there is none: dropped, as is a mark the cell or the table has no room
 * for. */
static void combine(vt_term *t, vt_u32 mark_cp)
{
    vt_u32 cp[VT_CLUSTER_CPS];
    vt_cell *c;
    int x = t->wrap_pending ? t->cx : t->cx - 1, n, v;
    if (x < 0)
        return;
    c = cell_at(t, x, t->cy);
    if (c->width == 0 && x > 0) {
        x--; /* the right half of a wide character: the mark is the character's */
        c--;
    }
    n = vt_cell_text(t, c, cp);
    if (n >= VT_CLUSTER_CPS)
        return;
    cp[n++] = mark_cp;
    v = cluster_of(t, cp, n);
    if (v < 0)
        return;
    c->ch = (vt_u16)v;
    mark(t, x, t->cy, x + (c->width == 2 ? 2 : 1));
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
    t->ext = t->link ? style_index(t) : 0; /* SGR 0 ends no hyperlink */
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
    t->gr = 0;
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

static int alloc_screen(vt_line ***scr, int rows, int cols, vt_term *t);
static void free_screen(vt_term *t, vt_line **scr, int rows);

static void set_alt(vt_term *t, int on, int clear)
{
    if (on && !t->alt) {
        /* made on first use: a terminal whose programs never switch (the
         * amiga personality, a console.device unit) never pays for it --
         * 16 bytes a cell, 32 KB at 80 x 25 (plan DV4) */
        if (!alloc_screen(&t->alt, t->rows, t->cols, t)) {
            free_screen(t, t->alt, t->rows);
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

/* put_char's characters that are not one BMP code point: 1 when cp was a
 * mark and is done, else *ch the cell value for it. Out of put_char, so
 * the common case stays a small function. */
static int rare_char(vt_term *t, vt_u32 cp, int w, vt_u16 *ch)
{
    int v;
    if (w == 0) {
        combine(t, cp);
        return 1;
    }
    v = cluster_of(t, &cp, 1);
    *ch = (vt_u16)(v < 0 ? 0xFFFD : v);
    return 0;
}

static void put_char(vt_term *t, vt_u32 cp)
{
    int w, lc;
    vt_cell *c;
    vt_u8 cs;
    vt_u16 ch;

    cs = t->charset[t->single_shift ? t->single_shift : t->gl];
    t->single_shift = 0;
    if (t->gr && cp >= 0xA0 && cp <= 0xFF && !t->utf8 && !t->cp437) {
        /* an 8-bit byte through GR (LS1R-LS3R): the set's own 20-7F */
        cs = t->charset[t->gr];
        cp -= 0x80;
    }
    if (cs == '0' && cp >= 0x5F && cp <= 0x7E)
        cp = dec_graphics[cp - 0x5F];
    else if (cs == 'A' && cp == '#')
        cp = 0xA3;

    w = vt_char_width(cp);
    ch = (vt_u16)cp;
    if (w == 0 || cp > 0xFFFF) {
        /* a mark joins the character before it; a character beyond the
         * BMP is an entry of the cluster table */
        if (rare_char(t, cp, w, &ch))
            return;
    }
    if (w == 2 && t->cols < 2)
        w = 1;

    if (t->wrap_pending && t->autowrap) {
        t->scr[t->cy]->wrapped = 1;
        t->cx = 0;
        index_down(t);
    }
    t->wrap_pending = 0;
    lc = t->scr[t->cy]->dbl ? (t->cols + 1) / 2 : t->cols; /* row_cols: cy is a row */
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
    c = &t->scr[t->cy]->c[t->cx];
    c->ch = ch;
    c->fg = t->fg;
    c->bg = t->bg;
    c->attr = t->attr;
    c->deco = t->deco;
    c->ext = t->ext;
    c->width = (vt_u8)w;
    c->pad = 0; /* text over an image cell */
    if (w == 2) {
        c[1] = c[0];
        c[1].ch = ' ';
        c[1].width = 0;
    }
    if (w == 1) {
        /* mark(), its checks known true for one cell at the cursor */
        vt_line *l = t->scr[t->cy];
        int x = t->cx;
        if (l->dx0 > x)
            l->dx0 = (short)x;
        if (l->dx1 < x + 1)
            l->dx1 = (short)(x + 1);
        if (l->used < x + 1)
            l->used = (vt_u16)(x + 1);
        l->chonly = 0;
        t->dirty = 1;
    } else {
        mark(t, t->cx, t->cy, t->cx + w);
    }
    t->last_ch = cp;

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
    case 'n': /* LS2, LS3: G2 / G3 into GL */
    case 'o':
        if (t->pers != VT_XTERM) {
            note_unhandled(t, 'E', final);
            break;
        }
        t->gl = final == 'n' ? 2 : 3;
        break;
    case '~': /* LS1R, LS2R, LS3R: G1 / G2 / G3 into GR */
    case '}':
    case '|':
        if (t->pers != VT_XTERM) {
            note_unhandled(t, 'E', final);
            break;
        }
        t->gr = final == '~' ? 1 : final == '}' ? 2 : 3;
        break;
    default:
        note_unhandled(t, 'E', final);
        break;
    }
}

/* ---- CSI dispatch ---------------------------------------------------------- */

/* Parameter i, or def when it is missing or 0 (param) / 0 when missing
 * (param0). Macros: every CSI asks one, and a call cost more than the
 * test (ASM1); the arguments have no side effects at any use. */
#define param(t, i, def) ((i) < (t)->np && (t)->params[i] ? (t)->params[i] : (long)(def))
#define param0(t, i) ((i) < (t)->np ? (t)->params[i] : 0L)

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
    if (t->np == 1 && !t->sub[0] && (vt_u32)(t->params[0] - 30) <= 17 && t->params[0] != 38 && t->params[0] != 39) {
        /* one colour, 30-37 or 40-47, the commonest SGR: what the loop
         * below does for it (every personality), without the loop */
        long p = t->params[0];
        if (p <= 37)
            t->fg = (vt_color)(p - 30);
        else
            t->bg = (vt_color)(p - 40);
        t->ext = pen_ext(t);
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
            if (p == 38 || p == 48) {
                vt_color col;
                if (i + 2 < t->np && t->params[i + 1] == 5) {
                    col = (vt_color)(t->params[i + 2] & 0xFF); /* 38;5;n, ext_colour's first case in place */
                    i += 2;
                } else {
                    col = ext_colour(t, &i);
                }
                if (p == 38)
                    t->fg = col;
                else
                    t->bg = col;
            } else if (p <= 37) {
                t->fg = (vt_color)(p - 30);
            } else {
                t->bg = (vt_color)(p - 40);
            }
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
    t->ext = pen_ext(t);
}

static void report_size(vt_term *t);

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
            case 2048: /* in-band resize: the size now, then at each resize */
                if (on) {
                    t->modes |= VT_MODE_IN_BAND_RESIZE;
                    report_size(t);
                } else {
                    t->modes &= ~(vt_u32)VT_MODE_IN_BAND_RESIZE;
                }
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
                if (on) {
                    save_cursor(t, &t->sav_1049);
                    t->saved_1048 = 1;
                } else {
                    restore_cursor(t, &t->sav_1049);
                }
                break;
            case 1049:
                if (on) {
                    save_cursor(t, &t->sav_1049);
                    t->saved_1048 = 1;
                    set_alt(t, 1, 1);
                } else {
                    set_alt(t, 0, 0);
                    restore_cursor(t, &t->sav_1049);
                }
                break;
            case 8: case 12: case 45: case 67: case 1005: case 1007: case 1015: case 1016: case 1034:
            case 2031: case 7727: {
                vt_u32 bit = p == 8 ? VT_MODE_AUTOREPEAT : p == 12 ? VT_MODE_CURSOR_BLINK
                           : p == 45 ? VT_MODE_REVERSE_WRAP : p == 67 ? VT_MODE_BACKSPACE_BS
                           : p == 1005 ? VT_MODE_MOUSE_UTF8
                           : p == 1007 ? VT_MODE_ALT_SCROLL
                           : p == 1015 ? VT_MODE_MOUSE_URXVT : p == 1016 ? VT_MODE_MOUSE_PIXELS
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
            case 80: /* DECSDM: sixel images at the top left, no scrolling */
                t->sixel_dm = (vt_u8)on;
                break;
            case 1070: /* each sixel image its own colour registers */
                t->six_private = (vt_u8)on;
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

/* ?2048 in-band resize: CSI 48;rows;cols;height px;width px t (pixels 0
 * when the host has not said), always 7-bit -- nobody asked in 8 bits. */
static void report_size(vt_term *t)
{
    char b[48];
    int n = 0;
    if (!(t->modes & VT_MODE_IN_BAND_RESIZE) || t->pers != VT_XTERM)
        return;
    b[n++] = 0x1B;
    b[n++] = '[';
    b[n++] = '4';
    b[n++] = '8';
    b[n++] = ';';
    n = fmt_uint(b, n, t->rows);
    b[n++] = ';';
    n = fmt_uint(b, n, t->cols);
    b[n++] = ';';
    n = fmt_uint(b, n, (long)t->rows * t->cell_h);
    b[n++] = ';';
    n = fmt_uint(b, n, (long)t->cols * t->cell_w);
    b[n++] = 't';
    reply(t, b, n);
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
        case 66: bit = VT_MODE_APP_KEYPAD; break; /* DECNKM is DECKPAM's state */
        case 5: bit = VT_MODE_SCREEN_REVERSE; break;
        case 9: bit = VT_MODE_MOUSE_X10; break;
        case 25: bit = VT_MODE_CURSOR_VISIBLE; break;
        case 47: case 1047: case 1049: bit = VT_MODE_ALT_SCREEN; break;
        case 1000: bit = VT_MODE_MOUSE_NORMAL; break;
        case 1002: bit = VT_MODE_MOUSE_BUTTON; break;
        case 1003: bit = VT_MODE_MOUSE_ANY; break;
        case 1048: v = t->saved_1048 ? 1 : 2; break;
        case 4: v = 4; break; /* DECSCLM: taken, never smooth: permanently reset */
        case 1004: bit = VT_MODE_FOCUS; break;
        case 1006: bit = VT_MODE_MOUSE_SGR; break;
        case 2004: bit = VT_MODE_BRACKET_PASTE; break;
        case 2026: bit = VT_MODE_SYNC; break;
        case 2048: bit = VT_MODE_IN_BAND_RESIZE; break;
        case 6: v = t->origin ? 1 : 2; break;
        case 7: v = t->autowrap ? 1 : 2; break;
        case 12: bit = VT_MODE_CURSOR_BLINK; break;
        case 8: bit = VT_MODE_AUTOREPEAT; break;
        case 45: bit = VT_MODE_REVERSE_WRAP; break;
        case 1005: bit = VT_MODE_MOUSE_UTF8; break;
        case 1007: bit = VT_MODE_ALT_SCROLL; break;
        case 67: bit = VT_MODE_BACKSPACE_BS; break;
        case 1015: bit = VT_MODE_MOUSE_URXVT; break;
        case 1016: bit = VT_MODE_MOUSE_PIXELS; break;
        case 1034: bit = VT_MODE_META_8BIT; break;
        case 2031: bit = VT_MODE_SCHEME_UPDATES; break;
        case 7727: bit = VT_MODE_APP_ESCAPE; break;
        case 40: v = t->allow_cols ? 1 : 2; break;
        case 80: v = t->sixel_dm ? 1 : 2; break;
        case 1070: v = t->six_private ? 1 : 2; break;
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

/* ---- VT420 column and rectangle editing (xterm) ---------------------------
 * No left/right margins (DECSLRM is not implemented, DA1 says VT220), so a
 * column operation covers the scroll region's rows across the whole width. */

/* DECIC / DECDC: n columns inserted / deleted at the cursor's in every row
 * of the scroll region; the cursor stays. */
static void edit_columns(vt_term *t, int n, int insert)
{
    int y, cy = t->cy;
    if (cy < t->top || cy >= t->bot)
        return;
    for (y = t->top; y < t->bot; y++) {
        t->cy = y;
        if (insert)
            insert_chars(t, n);
        else
            delete_chars(t, n);
    }
    t->cy = cy;
}

/* The rectangle Pt;Pl;Pb;Pr at params[i]: rows [*y0, *y1), columns
 * [*x0, *x1) of the screen, relative to the region in origin mode, clamped.
 * 0 when it is empty. */
static int rect_of(const vt_term *t, int i, int *x0, int *y0, int *x1, int *y1)
{
    int org = t->origin ? t->top : 0, lim = t->origin ? t->bot : t->rows;
    *y0 = clampi((int)param(t, i, 1) - 1 + org, org, lim);
    *x0 = clampi((int)param(t, i + 1, 1) - 1, 0, t->cols);
    *y1 = clampi((int)param(t, i + 2, lim - org) + org, org, lim);
    *x1 = clampi((int)param(t, i + 3, t->cols), 0, t->cols);
    return *y0 < *y1 && *x0 < *x1;
}

/* DECFRA (fill with ch in the current rendition) and DECERA / DECSERA
 * (ch 0: erase, as ECH does). */
static void rect_fill(vt_term *t, int x0, int y0, int x1, int y1, vt_u32 ch)
{
    int y, x;
    for (y = y0; y < y1; y++) {
        unwide(t, x0, y);
        if (x1 < t->cols)
            unwide(t, x1 - 1, y);
        for (x = x0; x < x1; x++) {
            vt_cell *c = cell_at(t, x, y);
            blank_cell(t, c);
            if (ch) {
                c->ch = (vt_u16)ch;
                c->fg = t->fg;
                c->bg = t->bg;
                c->attr = t->attr;
                c->deco = t->deco;
                c->ext = t->ext;
            }
        }
        mark(t, x0, y, x1);
    }
}

/* DECCRA: the rectangle to the place (dx, dy), overlapping or not. */
static void rect_copy(vt_term *t, int x0, int y0, int x1, int y1, int dx, int dy)
{
    int w = x1 - x0, h = y1 - y0, k, y;
    if (dx + w > t->cols)
        w = t->cols - dx;
    if (dy + h > t->rows)
        h = t->rows - dy;
    if (w <= 0 || h <= 0)
        return;
    for (k = 0; k < h; k++) {
        y = dy > y0 ? h - 1 - k : k; /* downwards: the bottom row first */
        cells_move(&t->scr[dy + y]->c[dx], &t->scr[y0 + y]->c[x0], w);
        if (t->scr[dy + y]->c[dx].width == 0)
            t->scr[dy + y]->c[dx].ch = ' ', t->scr[dy + y]->c[dx].width = 1;
        if (t->scr[dy + y]->c[dx + w - 1].width == 2)
            t->scr[dy + y]->c[dx + w - 1].ch = ' ', t->scr[dy + y]->c[dx + w - 1].width = 1;
        mark(t, dx, dy + y, dx + w);
    }
}

/* DECCARA (rev 0) sets, DECRARA (rev 1) reverses the attributes the SGR
 * parameters from params[4] name (0 all off; 1 4 5 7 and their 22 24 25
 * 27): in the rectangle, or with DECSACE 0/1 in the stream of cells from
 * its first to its last. */
static void rect_attrs(vt_term *t, int rev)
{
    int x0, y0, x1, y1, y, i;
    vt_attr set = 0, clr = 0;
    if (!rect_of(t, 0, &x0, &y0, &x1, &y1) && (t->rect_extent || y0 >= y1 - 1))
        return; /* a stream may end left of where it starts, on a later row */
    for (i = 4; i < t->np || i == 4; i++) {
        long p = param0(t, i);
        vt_attr a = p == 1 || p == 22 ? VT_ATTR_BOLD : p == 4 || p == 24 ? VT_ATTR_UNDERLINE
                  : p == 5 || p == 25 ? VT_ATTR_BLINK : p == 7 || p == 27 ? VT_ATTR_INVERSE : 0;
        if (p == 0)
            clr = (vt_attr)(VT_ATTR_BOLD | VT_ATTR_UNDERLINE | VT_ATTR_BLINK | VT_ATTR_INVERSE), set = 0;
        else if (p < 10)
            set |= a, clr &= (vt_attr)~a;
        else
            clr |= a, set &= (vt_attr)~a;
    }
    for (y = y0; y < y1; y++) {
        int a = x0, b = x1, x;
        if (!t->rect_extent) { /* the stream: whole rows between the ends */
            a = y == y0 ? x0 : 0;
            b = y == y1 - 1 ? x1 : t->cols;
            if (y0 == y1 - 1 && x1 <= x0)
                break;
        }
        for (x = a; x < b; x++) {
            vt_cell *c = cell_at(t, x, y);
            if (rev) {
                c->attr ^= set;
            } else {
                c->attr = (vt_attr)((c->attr & ~clr) | set);
                if (clr & VT_ATTR_UNDERLINE)
                    c->deco &= ~VT_DECO_UL_MASK;
                if ((set & VT_ATTR_UNDERLINE) && !(c->deco & VT_DECO_UL_MASK))
                    c->deco |= VT_UL_SINGLE;
            }
        }
        mark(t, a, y, b);
    }
}

/* CSI ... with the intermediate ' or $ or *: the VT420 editing the
 * xterm personality does. 0 when the sequence is not one of them. */
static int csi_vt420(vt_term *t, vt_u8 final)
{
    int x0, y0, x1, y1;
    if (t->inter == '\'' && (final == '}' || final == '~')) {
        edit_columns(t, (int)param(t, 0, 1), final == '}');
        return 1;
    }
    if (t->inter == '*' && final == 'x') { /* DECSACE */
        t->rect_extent = param0(t, 0) == 2;
        return 1;
    }
    if (t->inter != '$')
        return 0;
    switch (final) {
    case 'x': { /* DECFRA Pch;Pt;Pl;Pb;Pr */
        long ch = param0(t, 0);
        int i;
        if (!((ch >= 32 && ch <= 126) || (ch >= 160 && ch <= 255)))
            return 1;
        for (i = 0; i < 4; i++) /* the rectangle starts at the second parameter */
            t->params[i] = i + 1 < t->np ? t->params[i + 1] : 0;
        t->np = t->np > 0 ? t->np - 1 : 0;
        if (rect_of(t, 0, &x0, &y0, &x1, &y1))
            rect_fill(t, x0, y0, x1, y1, (vt_u32)ch);
        return 1;
    }
    case 'z': /* DECERA */
    case '{': /* DECSERA: no cell is protected, so the same */
        if (rect_of(t, 0, &x0, &y0, &x1, &y1))
            rect_fill(t, x0, y0, x1, y1, 0);
        return 1;
    case 'v': { /* DECCRA Pts;Pls;Pbs;Prs;Pps;Ptd;Pld;Ppd: one page */
        int org = t->origin ? t->top : 0;
        if (rect_of(t, 0, &x0, &y0, &x1, &y1))
            rect_copy(t, x0, y0, x1, y1, clampi((int)param(t, 6, 1) - 1, 0, t->cols - 1),
                      clampi((int)param(t, 5, 1) - 1 + org, org, (t->origin ? t->bot : t->rows) - 1));
        return 1;
    }
    case 'r': /* DECCARA */
    case 't': /* DECRARA */
        rect_attrs(t, final == 't');
        return 1;
    default:
        return 0;
    }
}

/* The kitty keyboard protocol's flag stack (CSI ? u, > u, < u, = u): one
 * per screen, 8 deep (a full one drops its oldest), entry 0 the base. */
static void kitty_kbd(vt_term *t)
{
    int s = t->scr == t->alt && t->alt, top = t->kbd_top[s];
    vt_u8 *k = t->kbd[s];
    long f = param0(t, 0) & 31;
    switch (t->priv) {
    case '?': {
        char b[16];
        int n = put_csi(t, b);
        b[n++] = '?';
        n = fmt_uint(b, n, k[top]);
        b[n++] = 'u';
        reply(t, b, n);
        return;
    }
    case '>':
        if (top == 7)
            memmove(k + 1, k + 2, 6); /* full: the oldest pushed goes */
        else
            top++;
        k[top] = (vt_u8)f;
        break;
    case '<': {
        long n = param(t, 0, 1);
        while (n-- > 0) {
            k[top] = 0;
            if (top)
                top--;
            else
                break;
        }
        break;
    }
    default: { /* '=' flags ; mode: 1 set, 2 or, 3 and-not */
        long m = param(t, 1, 1);
        k[top] = (vt_u8)(m == 2 ? (k[top] | f) : m == 3 ? (k[top] & ~f) : f);
        break;
    }
    }
    t->kbd_top[s] = (vt_u8)top;
}

static int scheme_of(const vt_term *t);
static void xtsmgraphics(vt_term *t);

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
    if (t->inter && !t->priv && csi_vt420(t, final))
        return;
    if (final == 'u' && !t->inter && t->priv) {
        kitty_kbd(t); /* CSI ? u, CSI > f u, CSI < n u, CSI = f ; m u */
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
        else if (final == 'S')
            xtsmgraphics(t);
        else if (final == 'i')
            ; /* DEC media copy (auto print, print cursor line): no printer */
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
    if (t->priv == '=') {
        if (final == 'c' && param0(t, 0) == 0)
            reply(t, "\033P!|00000000\033\\", 14); /* DA3: DECRPTUI, as xterm */
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
            reply(t, "\033[?62;4;22c", 11); /* DA1: VT220 with sixel graphics and ANSI colour */
        return;
    case 'i': /* media copy: there is no printer */
        if (param0(t, 0) == 5) {
            t->state = S_PRINT; /* printer controller: to CSI 4 i, off the screen */
            t->prn_match = 0;
        }
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

/* X11 colour names (rgb.txt values) the programs name in OSC 4 / 10-12 --
 * the common ones, not all 750; plus grayN / greyN (0-100) and tmux's
 * colourN / colorN (palette entry N). Case and blanks do not count. */
static vt_u32 color_name(const vt_term *t, const char *s, int n)
{
    static const struct { const char *name; vt_u32 rgb; } names[] = {
        { "black", 0x000000 }, { "white", 0xFFFFFF }, { "red", 0xFF0000 }, { "green", 0x00FF00 },
        { "blue", 0x0000FF }, { "yellow", 0xFFFF00 }, { "cyan", 0x00FFFF }, { "magenta", 0xFF00FF },
        { "gray", 0xBEBEBE }, { "grey", 0xBEBEBE }, { "darkgray", 0xA9A9A9 }, { "darkgrey", 0xA9A9A9 },
        { "lightgray", 0xD3D3D3 }, { "lightgrey", 0xD3D3D3 }, { "dimgray", 0x696969 },
        { "dimgrey", 0x696969 }, { "slategray", 0x708090 }, { "darkslategray", 0x2F4F4F },
        { "darkslategrey", 0x2F4F4F }, { "orange", 0xFFA500 }, { "darkorange", 0xFF8C00 },
        { "purple", 0xA020F0 }, { "violet", 0xEE82EE }, { "brown", 0xA52A2A }, { "pink", 0xFFC0CB },
        { "hotpink", 0xFF69B4 }, { "navy", 0x000080 }, { "navyblue", 0x000080 },
        { "darkred", 0x8B0000 }, { "darkgreen", 0x006400 }, { "darkblue", 0x00008B },
        { "darkcyan", 0x008B8B }, { "darkmagenta", 0x8B008B }, { "lightblue", 0xADD8E6 },
        { "lightgreen", 0x90EE90 }, { "lightyellow", 0xFFFFE0 }, { "lightcyan", 0xE0FFFF },
        { "skyblue", 0x87CEEB }, { "steelblue", 0x4682B4 }, { "royalblue", 0x4169E1 },
        { "dodgerblue", 0x1E90FF }, { "turquoise", 0x40E0D0 }, { "gold", 0xFFD700 },
        { "khaki", 0xF0E68C }, { "coral", 0xFF7F50 }, { "salmon", 0xFA8072 }, { "tomato", 0xFF6347 },
        { "orchid", 0xDA70D6 }, { "plum", 0xDDA0DD }, { "maroon", 0xB03060 }, { "beige", 0xF5F5DC },
        { "ivory", 0xFFFFF0 }, { "wheat", 0xF5DEB3 }, { "tan", 0xD2B48C }, { "chocolate", 0xD2691E },
        { "firebrick", 0xB22222 }, { "forestgreen", 0x228B22 }, { "seagreen", 0x2E8B57 },
        { "limegreen", 0x32CD32 }, { "olivedrab", 0x6B8E23 }, { "aquamarine", 0x7FFFD4 },
        { "chartreuse", 0x7FFF00 }, { "indigo", 0x4B0082 }, { "lavender", 0xE6E6FA },
        { "silver", 0xC0C0C0 }, { "teal", 0x008080 }, { "olive", 0x808000 }, { "lime", 0x00FF00 },
        { "aqua", 0x00FFFF }, { "fuchsia", 0xFF00FF }
    };
    char k[24];
    int i, m = 0;
    long v = 0;
    for (i = 0; i < n && m < (int)sizeof(k) - 1; i++)
        if (s[i] != ' ')
            k[m++] = (char)(s[i] >= 'A' && s[i] <= 'Z' ? s[i] + 32 : s[i]);
    if (i < n)
        return 0;
    k[m] = 0;
    for (i = 0; i < (int)(sizeof(names) / sizeof(names[0])); i++)
        if (!strcmp(k, names[i].name))
            return 0x01000000UL | names[i].rgb;
    for (i = 0; i < 2; i++) {
        const char *pre = i ? "colo" : "gr";
        int pl = (int)strlen(pre), j;
        if (strncmp(k, pre, pl))
            continue;
        j = pl;
        if (i ? (k[j] == 'u' && k[j + 1] == 'r') : (k[j] == 'a' || k[j] == 'e') && k[j + 1] == 'y')
            j += 2;
        else if (i && k[j] == 'r')
            j += 1;
        else
            continue;
        if (!k[j])
            continue;
        for (v = 0; k[j] >= '0' && k[j] <= '9' && v < 1000; j++)
            v = v * 10 + (k[j] - '0');
        if (k[j])
            continue;
        if (i && v < 256)
            return 0x01000000UL | vt_palette_rgb(t, (int)v);
        if (!i && v <= 100) {
            vt_u32 g = (vt_u32)((v * 255 + 50) / 100);
            return 0x01000000UL | (g << 16) | (g << 8) | g;
        }
    }
    return 0;
}

/* An X colour spec: rgb:r/g/b (1-4 hex digits each) or #rgb, #rrggbb,
 * #rrrgggbbb, #rrrrggggbbbb, or a colour name (color_name). 0x01RRGGBB, 0
 * when it is not one. */
static vt_u32 parse_color(const vt_term *t, const char *s, int n)
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
    return color_name(t, s, n);
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

/* ---- OSC 52: the clipboard -------------------------------------------------
 * OSC 52 ; selection ; base64 ST sets it, OSC 52 ; selection ; ? ST asks.
 * A payload is as big as what was copied, so it never goes into str: the
 * bytes after "52;" stream through osc52_byte, decoded as they come into a
 * buffer that grows to VT_CLIP_MAX; past that the set is dropped whole. */

static void osc52_begin(vt_term *t)
{
    t->osc52 = 1;
    t->clip_nsel = 0;
    t->clip_sel[0] = 0;
    t->clip_bad = t->clip_query = 0;
    t->clip_len = 0;
    t->b64_acc = 0;
    t->b64_n = t->b64_pad = 0;
}

static void clip_drop(vt_term *t)
{
    if (t->clip)
        VT_FREE(t->clip);
    t->clip = 0;
    t->clip_cap = t->clip_len = 0;
}

static void clip_put(vt_term *t, vt_u8 b)
{
    if (t->clip_bad)
        return;
    if (t->clip_len == t->clip_cap) {
        long cap = t->clip_cap ? t->clip_cap * 2 : 1024;
        vt_u8 *n;
        if (cap > VT_CLIP_MAX)
            cap = VT_CLIP_MAX;
        if (t->clip_len >= cap || !(n = (vt_u8 *)VT_MALLOC(cap))) {
            t->clip_bad = 1; /* too big, or no memory: the whole set goes */
            clip_drop(t);
            return;
        }
        if (t->clip_len)
            memcpy(n, t->clip, t->clip_len);
        if (t->clip)
            VT_FREE(t->clip);
        t->clip = n;
        t->clip_cap = cap;
    }
    t->clip[t->clip_len++] = b;
}

static int b64_value(vt_u32 c)
{
    if (c >= 'A' && c <= 'Z')
        return (int)(c - 'A');
    if (c >= 'a' && c <= 'z')
        return (int)(c - 'a' + 26);
    if (c >= '0' && c <= '9')
        return (int)(c - '0' + 52);
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;
    return -1;
}

/* The bytes a group of n base64 digits (2-4) in acc stands for. */
static void b64_flush(vt_term *t)
{
    vt_u32 a = t->b64_acc << (6 * (4 - t->b64_n));
    if (t->b64_n == 1)
        t->clip_bad = 1; /* six bits are no byte */
    if (t->b64_n >= 2)
        clip_put(t, (vt_u8)(a >> 16));
    if (t->b64_n >= 3)
        clip_put(t, (vt_u8)(a >> 8));
    if (t->b64_n == 4)
        clip_put(t, (vt_u8)a);
    t->b64_acc = 0;
    t->b64_n = 0;
}

static void osc52_byte(vt_term *t, vt_u32 c)
{
    int v;
    if (t->osc52 == 3) { /* OSC 7 / 8: the text as it is, to VT_URI_MAX */
        vt_u8 u[4];
        int k, n;
        if (c < 0x20 || t->clip_bad)
            return;
        n = put_utf8(u, (long)c);
        if (t->clip_len + n >= VT_URI_MAX) {
            t->clip_bad = 1;
            clip_drop(t);
            return;
        }
        for (k = 0; k < n; k++)
            clip_put(t, u[k]);
        return;
    }
    if (t->osc52 == 1) { /* the selection: c, p, q, s, 0-7, or none */
        if (c == ';')
            t->osc52 = 2;
        else if (t->clip_nsel < (int)sizeof(t->clip_sel) - 1)
            t->clip_sel[t->clip_nsel++] = (char)c, t->clip_sel[t->clip_nsel] = 0;
        return;
    }
    if (t->clip_bad)
        return;
    if (c == '?' && !t->clip_len && !t->b64_n && !t->b64_pad && !t->clip_query) {
        t->clip_query = 1;
        return;
    }
    if (t->clip_query) {
        t->clip_bad = 1;
        return;
    }
    if (c == '=') {
        if (t->b64_n < 2 || ++t->b64_pad + t->b64_n > 4)
            t->clip_bad = 1;
        return;
    }
    if ((v = b64_value(c)) < 0 || t->b64_pad) {
        t->clip_bad = 1; /* not base64 (or data after the padding): ignored, as xterm does */
        clip_drop(t);
        return;
    }
    t->b64_acc = (t->b64_acc << 6) | (vt_u32)v;
    if (++t->b64_n == 4)
        b64_flush(t);
}

/* OSC 52 ; selection ; ? -- the clipboard, base64, in the request's ST. */
static void osc52_answer(vt_term *t)
{
    static const char a[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    vt_u8 *data = (vt_u8 *)VT_MALLOC(VT_CLIP_QUERY_MAX);
    char *b;
    long n, i, k = 0;
    if (!data)
        return;
    n = t->cb.clipboard_get(t->user, data, VT_CLIP_QUERY_MAX);
    if (n < 0)
        n = 0;
    if (n > VT_CLIP_QUERY_MAX)
        n = VT_CLIP_QUERY_MAX;
    b = (char *)VT_MALLOC((n + 2) / 3 * 4 + 32);
    if (b) {
        b[k++] = 0x1B;
        b[k++] = ']';
        b[k++] = '5';
        b[k++] = '2';
        b[k++] = ';';
        for (i = 0; t->clip_sel[i]; i++)
            b[k++] = t->clip_sel[i];
        if (!i)
            b[k++] = 'c';
        b[k++] = ';';
        for (i = 0; i < n; i += 3) {
            vt_u32 v = (vt_u32)data[i] << 16 | (i + 1 < n ? (vt_u32)data[i + 1] << 8 : 0) |
                       (i + 2 < n ? data[i + 2] : 0);
            b[k++] = a[(v >> 18) & 63];
            b[k++] = a[(v >> 12) & 63];
            b[k++] = i + 1 < n ? a[(v >> 6) & 63] : '=';
            b[k++] = i + 2 < n ? a[v & 63] : '=';
        }
        k = put_st(t, b, (int)k);
        reply(t, b, (int)k);
        VT_FREE(b);
    }
    VT_FREE(data);
}

static void osc7_set(vt_term *t, const char *uri);
static void osc8_set(vt_term *t, char *s);

static void osc52_end(vt_term *t)
{
    if (t->osc52 == 3 && !t->clip_bad) {
        clip_put(t, 0);
        if (!t->clip_bad) {
            if (t->long_cmd == 7)
                osc7_set(t, (const char *)t->clip);
            else
                osc8_set(t, (char *)t->clip);
        }
    }
    if (t->osc52 == 2 && !t->clip_bad) {
        if (t->clip_query) {
            if ((t->clip_access & VT_CLIP_READ) && t->cb.clipboard_get)
                osc52_answer(t);
        } else {
            if (t->b64_n)
                b64_flush(t); /* base64 without its padding */
            if (!t->clip_bad && (t->clip_access & VT_CLIP_WRITE) && t->cb.clipboard_set)
                t->cb.clipboard_set(t->user, t->clip_sel, t->clip ? t->clip : (const vt_u8 *)"",
                                    t->clip_len);
        }
    }
    t->osc52 = 0;
    clip_drop(t); /* a megabyte is not kept for the next one */
}

/* ---- OSC 7: the working directory; OSC 8: hyperlinks ------------------------ */

static char *str_dup(const char *s)
{
    long n = (long)strlen(s) + 1;
    char *d = (char *)VT_MALLOC(n);
    if (d)
        memcpy(d, s, n);
    return d;
}

static void osc7_set(vt_term *t, const char *uri)
{
    char *d = str_dup(uri);
    if (!d)
        return;
    if (t->cwd)
        VT_FREE(t->cwd);
    t->cwd = d;
    if (t->cb.cwd)
        t->cb.cwd(t->user, d);
}

const char *vt_cwd(const vt_term *t)
{
    return t->cwd ? t->cwd : "";
}

/* Room in a full link table: the style entries no cell uses go
 * (sweep_styles), then the links no style entry names, the rest moved down
 * and the style entries renumbered. 0 when every link is still shown. */
static int links_sweep(vt_term *t)
{
    vt_u8 used[VT_LINKS_MAX + 1], remap[VT_LINKS_MAX + 1];
    int i, k = 0;
    sweep_styles(t);
    memset(used, 0, sizeof(used));
    for (i = 0; i < t->n_styles; i++)
        used[t->styles[i].link] = 1;
    if (t->link)
        used[t->link] = 1; /* the one being written */
    remap[0] = 0;
    for (i = 1; i <= t->n_links; i++) {
        remap[i] = 0;
        if (used[i]) {
            t->links[k] = t->links[i - 1];
            remap[i] = (vt_u8)++k;
        } else {
            VT_FREE(t->links[i - 1].uri);
        }
    }
    t->n_links = k;
    for (i = 0; i < t->n_styles; i++)
        t->styles[i].link = remap[t->styles[i].link];
    t->link = remap[t->link];
    return k < VT_LINKS_MAX;
}

/* OSC 8 ; params ; URI: the cells written from here on link to URI (an
 * empty one ends the link). params is key=value pairs split by ':' -- id=
 * joins cells of one link written apart (the same id and URI are one). */
static void osc8_set(vt_term *t, char *s)
{
    char *uri = s, id[24];
    int i, n;
    id[0] = 0;
    while (*uri && *uri != ';')
        uri++;
    if (!*uri)
        return; /* no second ';': not OSC 8 */
    *uri++ = 0;
    for (i = 0; s[i];) { /* params: id=... */
        int e = i;
        while (s[e] && s[e] != ':')
            e++;
        if (e - i > 3 && !memcmp(s + i, "id=", 3)) {
            n = e - i - 3 < (int)sizeof(id) - 1 ? e - i - 3 : (int)sizeof(id) - 1;
            memcpy(id, s + i + 3, n);
            id[n] = 0;
        }
        i = s[e] ? e + 1 : e;
    }
    t->link = 0;
    if (*uri) {
        for (i = 0; i < t->n_links; i++)
            if (!strcmp(t->links[i].uri, uri) && !strcmp(t->links[i].id, id))
                break;
        if (i == t->n_links) {
            char *d;
            if (t->n_links == VT_LINKS_MAX && !links_sweep(t)) {
                t->ext = style_index(t); /* every link is on screen: the text goes unlinked */
                return;
            }
            i = t->n_links;
            if (!(d = str_dup(uri)))
                return;
            t->links[i].uri = d;
            memcpy(t->links[i].id, id, sizeof(id));
            t->n_links++;
        }
        t->link = (vt_u8)(i + 1);
    }
    t->ext = style_index(t);
}

const char *vt_cell_link(const vt_term *t, const vt_cell *c)
{
    int k;
    if (!c->ext || c->ext > t->n_styles)
        return 0;
    k = t->styles[c->ext - 1].link;
    return k && k <= t->n_links ? t->links[k - 1].uri : 0;
}

/* ---- OSC 133: semantic prompt marks ----------------------------------------- */

static const vt_line *line_of(const vt_term *t, long row)
{
    if (row >= 0)
        return row < t->rows ? t->scr[row] : 0;
    if (-row > t->sb_len)
        return 0;
    return t->sb[(t->sb_head + t->sb_cap + (int)row) % t->sb_cap];
}

int vt_row_marks(const vt_term *t, int row)
{
    const vt_line *l = line_of(t, row);
    return l ? l->mark : 0;
}

long vt_find_mark(const vt_term *t, long from, int dir, int mark)
{
    long r, lo = -(long)t->sb_len;
    for (r = from + dir; r >= lo && r < t->rows; r += dir) {
        const vt_line *l = line_of(t, r);
        if (l && (l->mark & mark))
            return r;
    }
    return VT_ROW_NONE;
}

void vt_set_clipboard_access(vt_term *t, int bits)
{
    if (t)
        t->clip_access = bits & (VT_CLIP_WRITE | VT_CLIP_READ);
}

static void osc_dispatch(vt_term *t)
{
    int i = 0, changed = 0;
    long cmd = 0;
    if (t->osc52) {
        osc52_end(t);
        return;
    }
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
                vt_u32 c = parse_color(t, spec, m);
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
                vt_u32 c = parse_color(t, spec, m);
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
    if (cmd == 133) { /* FinalTerm semantic prompt: A prompt, B command, C output, D end */
        char k = i < t->str_len ? t->str[i] : 0;
        int m = k == 'A' ? VT_MARK_PROMPT : k == 'B' ? VT_MARK_COMMAND : k == 'C' ? VT_MARK_OUTPUT
              : k == 'D' ? VT_MARK_DONE : 0;
        if (m)
            t->scr[t->cy]->mark |= (vt_u8)m;
        else
            note_value(t, 'O', cmd);
        return;
    }
    if (cmd == 9) { /* iTerm2's notification; 9;<digits>; is ConEmu's (progress...) */
        int j = i;
        while (j < t->str_len && t->str[j] >= '0' && t->str[j] <= '9')
            j++;
        if (!(j > i && j < t->str_len && t->str[j] == ';') && t->cb.notify)
            t->cb.notify(t->user, "", t->str + i);
        return;
    }
    if (cmd == 777) { /* urxvt / foot: 777;notify;title;body */
        char *s = t->str + i, *b;
        if (strncmp(s, "notify;", 7)) {
            note_value(t, 'O', cmd);
            return;
        }
        s += 7;
        b = s;
        while (*b && *b != ';')
            b++;
        if (*b)
            *b++ = 0;
        if (t->cb.notify)
            t->cb.notify(t->user, s, b);
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
    if (t->attr & (VT_ATTR_FRAMED | VT_ATTR_ENCIRCLED)) {
        memcpy(b + n, t->attr & VT_ATTR_FRAMED ? ";51" : ";52", 3);
        n += 3;
    }
    if (t->attr & (VT_ATTR_SUPER | VT_ATTR_SUB)) {
        memcpy(b + n, t->attr & VT_ATTR_SUPER ? ";73" : ";74", 3);
        n += 3;
    }
    if (t->deco & VT_DECO_IDEO_MASK) {
        b[n++] = ';';
        n = fmt_uint(b, n, 59 + ((t->deco & VT_DECO_IDEO_MASK) >> VT_DECO_IDEO_SHIFT));
    }
    if (t->font) {
        b[n++] = ';';
        n = fmt_uint(b, n, 10 + t->font);
    }
    for (w = 0; w < 3; w++) {
        /* the text, the background, the underline (SGR 58: no 30-37 form) */
        vt_color c = w == 2 ? t->ul : w ? t->bg : t->fg;
        int base = w == 2 ? 58 : w ? 48 : 38;
        if (c == VT_COLOR_DEFAULT)
            continue;
        b[n++] = ';';
        if (c & VT_COLOR_RGB) {
            n = fmt_uint(b, n, base);
            memcpy(b + n, ";2;", 3);
            n += 3;
            n = fmt_uint(b, n, (long)((c >> 16) & 0xFF));
            b[n++] = ';';
            n = fmt_uint(b, n, (long)((c >> 8) & 0xFF));
            b[n++] = ';';
            n = fmt_uint(b, n, (long)(c & 0xFF));
        } else if (c < 8 && w < 2) {
            n = fmt_uint(b, n, (long)(c + (w ? 40 : 30)));
        } else if (c < 16 && w < 2) {
            n = fmt_uint(b, n, (long)(c - 8 + (w ? 100 : 90)));
        } else {
            n = fmt_uint(b, n, base);
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
        memcpy(b + n, "62;1\"p", 6); /* what DA1 says: a VT220, 7-bit controls */
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
/* The terminfo entry, generated from terminfo/vtcon.terminfo
 * (tools/gen_vtcaps.py): what XTGETTCAP answers is what the entry says. */
static const struct { const char *name; char kind; const char *value; } vt_caps[] = {
#include "vtcaps.inc"
};

/* xterm's names beside the terminfo ones: the terminal's name, termcap's
 * Co, and RGB as the bits per channel (what xterm answers for it). */
static const char *const caps_extra[][2] = {
    { "TN", "vtcon" }, { "name", "vtcon" }, { "Co", "256" }, { "RGB", "8/8/8" }
};

static void xtgettcap(vt_term *t, const char *s, int len)
{
    static const char hex[] = "0123456789ABCDEF";
    while (len > 0) {
        char name[16], b[320];
        int k = 0, i, n = 0, h1, h2, found = 0;
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
        for (i = 0; !found && i < (int)(sizeof(caps_extra) / sizeof(caps_extra[0])); i++)
            if (!strcmp(name, caps_extra[i][0]))
                v = caps_extra[i][1], found = 1;
        for (i = 0; !found && i < (int)(sizeof(vt_caps) / sizeof(vt_caps[0])); i++)
            if (!strcmp(name, vt_caps[i].name)) {
                found = 1;
                v = vt_caps[i].kind == 'b' ? 0 : vt_caps[i].value; /* a boolean: no value */
            }
        if (v && (int)strlen(v) * 2 + 2 * k + 16 > (int)sizeof(b))
            found = 0, v = 0; /* cannot happen with the entry's lengths */
        b[n++] = 0x1B;
        b[n++] = 'P';
        b[n++] = found ? '1' : '0';
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

/* ---- sixel graphics (DCS P1;P2;P3 q ... ST) ------------------------------ */

/* The decoder of the image arriving: a streaming state machine, one byte
 * at a time from any number of writes; the pixels grow as the stream
 * draws them, up to VT_SIXEL_MAX_W x VT_SIXEL_MAX_H and the memory all
 * images share. Colours are kept per image in a compact palette: entry 0
 * for pixels nothing drew, then one entry per register colour in use, so
 * a renderer needs pens only for the colours the picture has. */
typedef struct vt_sixel {
    vt_u8 *pix;
    long aw, ah;            /* allocated */
    long x, y;              /* the next sixel's column, the band's top pixel row */
    long w, h;              /* how far the pixels drawn reach */
    long rw, rh;            /* raster attributes' size, 0 none */
    long prm[5];
    int np;
    int mode;               /* 0 data, or '"', '#', '!' while their numbers come */
    long rep;               /* the repeat count for the next sixel, 0 none */
    int aspect;             /* pixel rows a sixel bit covers */
    int reg;                /* the colour register drawing */
    int npal;
    int full;               /* the memory is spent: no more growth */
    vt_u8 regmap[256];      /* register -> pal entry, 0 not yet in pal */
    vt_u32 pal[256];
    vt_u32 own[256];        /* the registers when the image has its own (?1070) */
    vt_u32 *regs;
} vt_sixel;

static long dist2(int r, int g, int b, vt_u32 c);

/* The VT340's 16 colour registers (percent R, G, B), xterm's defaults. */
static const vt_u8 vt340_pal[16][3] = {
    { 0, 0, 0 }, { 20, 20, 80 }, { 80, 13, 13 }, { 20, 80, 20 }, { 80, 20, 80 }, { 20, 80, 80 },
    { 80, 80, 20 }, { 53, 53, 53 }, { 26, 26, 26 }, { 33, 33, 60 }, { 60, 26, 26 }, { 33, 60, 33 },
    { 60, 33, 60 }, { 33, 60, 60 }, { 60, 60, 33 }, { 80, 80, 80 }
};

static vt_u32 pct_rgb(long r, long g, long b)
{
    r = r < 0 ? 0 : r > 100 ? 100 : r;
    g = g < 0 ? 0 : g > 100 ? 100 : g;
    b = b < 0 ? 0 : b > 100 ? 100 : b;
    return ((vt_u32)((r * 255 + 50) / 100) << 16) | ((vt_u32)((g * 255 + 50) / 100) << 8) |
           (vt_u32)((b * 255 + 50) / 100);
}

static long hls_part(long m1, long m2, long h)
{
    while (h < 0)
        h += 360;
    while (h >= 360)
        h -= 360;
    if (h < 60)
        return m1 + (m2 - m1) * h / 60;
    if (h < 180)
        return m2;
    if (h < 240)
        return m1 + (m2 - m1) * (240 - h) / 60;
    return m1;
}

/* DEC HLS (hue 0-360 with blue at 0 and red at 120, lightness and
 * saturation 0-100) to 0xRRGGBB. */
static vt_u32 hls_rgb(long h, long l, long s)
{
    long m1, m2;
    l = l < 0 ? 0 : l > 100 ? 100 : l;
    s = s < 0 ? 0 : s > 100 ? 100 : s;
    if (!s)
        return pct_rgb(l, l, l);
    h += 240; /* DEC's blue at 0 to the usual red at 0 */
    m2 = l <= 50 ? l * (100 + s) / 100 : l + s - l * s / 100;
    m1 = 2 * l - m2;
    return pct_rgb(hls_part(m1, m2, h + 120), hls_part(m1, m2, h), hls_part(m1, m2, h - 120));
}

static void sixel_default_regs(const vt_term *t, vt_u32 *regs)
{
    int i;
    for (i = 0; i < 256; i++)
        regs[i] = i < 16 ? pct_rgb(vt340_pal[i][0], vt340_pal[i][1], vt340_pal[i][2])
                         : vt_palette_rgb(t, i);
}

/* The oldest image goes: its placements draw nothing from now on. */
static int image_evict(vt_term *t)
{
    int i, old = -1;
    for (i = 0; i < VT_IMG_SLOTS; i++)
        if (t->imgs[i] && t->imgs[i]->pix && (old < 0 || t->imgs[i]->serial < t->imgs[old]->serial))
            old = i;
    if (old < 0)
        return 0;
    image_free(t, old);
    return 1;
}

/* Room for the pixels up to (nx, ny), both exclusive; 0 when they cannot
 * have it (past the size limit, or out of memory). */
static int sixel_room(vt_term *t, vt_sixel *s, long nx, long ny)
{
    long aw = s->aw, ah = s->ah, y;
    vt_u8 *n;
    if (nx <= s->aw && ny <= s->ah)
        return 1;
    if (s->full || nx > VT_SIXEL_MAX_W || ny > VT_SIXEL_MAX_H)
        return 0;
    if (aw < s->rw)
        aw = s->rw; /* the size the raster attributes promised: one allocation */
    if (ah < s->rh)
        ah = s->rh;
    while (aw < nx)
        aw = aw ? aw * 2 : 64;
    while (ah < ny)
        ah = ah ? ah * 2 : 6L * s->aspect;
    if (aw > VT_SIXEL_MAX_W)
        aw = VT_SIXEL_MAX_W;
    if (ah > VT_SIXEL_MAX_H)
        ah = VT_SIXEL_MAX_H;
    while (t->img_mem + aw * ah > VT_IMAGE_MEMORY)
        if (!image_evict(t)) {
            s->full = 1;
            return 0;
        }
    n = (vt_u8 *)VT_MALLOC(aw * ah);
    if (!n) {
        s->full = 1;
        return 0;
    }
    memset(n, 0, aw * ah);
    for (y = 0; y < s->ah; y++)
        memcpy(n + y * aw, s->pix + y * s->aw, s->aw);
    if (s->pix)
        VT_FREE(s->pix);
    s->pix = n;
    s->aw = aw;
    s->ah = ah;
    return 1;
}

/* The pal entry of the register drawing now. */
static vt_u8 sixel_ink(vt_sixel *s)
{
    vt_u32 rgb = s->regs[s->reg];
    long best = -1, d;
    int i, k = 0;
    if (s->regmap[s->reg])
        return s->regmap[s->reg];
    if (s->npal < 256) {
        s->pal[s->npal] = rgb;
        return s->regmap[s->reg] = (vt_u8)s->npal++;
    }
    for (i = 1; i < 256; i++) { /* 255 colours in one image: the nearest */
        d = dist2((int)(rgb >> 16) & 0xFF, (int)(rgb >> 8) & 0xFF, (int)rgb & 0xFF, s->pal[i]);
        if (best < 0 || d < best) {
            best = d;
            k = i;
        }
    }
    return s->regmap[s->reg] = (vt_u8)k;
}

static void sixel_draw(vt_term *t, vt_sixel *s, int bits)
{
    long n = s->rep ? s->rep : 1, x = s->x, nx, row;
    int b, a, top = -1;
    vt_u8 ink;
    s->rep = 0;
    s->x += n;
    if (!bits || x >= VT_SIXEL_MAX_W || s->y >= VT_SIXEL_MAX_H)
        return;
    nx = x + n > VT_SIXEL_MAX_W ? VT_SIXEL_MAX_W : x + n;
    for (b = 5; b >= 0 && top < 0; b--)
        if (bits & (1 << b))
            top = b;
    if (!sixel_room(t, s, nx, s->y + (top + 1) * (long)s->aspect)) {
        /* past the limits: what still fits is drawn */
        nx = nx < s->aw ? nx : s->aw;
        if (x >= nx)
            return;
    }
    ink = sixel_ink(s);
    for (b = 0; b <= top; b++) {
        if (!(bits & (1 << b)))
            continue;
        for (a = 0; a < s->aspect; a++) {
            row = s->y + (long)b * s->aspect + a;
            if (row < s->ah)
                memset(s->pix + row * s->aw + x, ink, nx - x);
        }
    }
    if (nx > s->w)
        s->w = nx;
    row = s->y + (long)(top + 1) * s->aspect;
    if (row > s->ah)
        row = s->ah;
    if (row > s->h)
        s->h = row;
}

/* The numbers of a '"', '#' or '!' are complete. */
static void sixel_command(vt_sixel *s)
{
    long *p = s->prm;
    switch (s->mode) {
    case '"': /* raster attributes: Pan;Pad;Ph;Pv */
        if (s->np >= 2 && p[0] > 0 && p[1] > 0) {
            long a = (p[0] + p[1] / 2) / p[1];
            s->aspect = (int)(a < 1 ? 1 : a > 10 ? 10 : a);
        }
        if (s->np >= 3)
            s->rw = p[2] > VT_SIXEL_MAX_W ? VT_SIXEL_MAX_W : p[2];
        if (s->np >= 4)
            s->rh = p[3] > VT_SIXEL_MAX_H ? VT_SIXEL_MAX_H : p[3];
        break;
    case '#': /* #Pc selects a register; #Pc;Pu;Px;Py;Pz defines it */
        s->reg = (int)(p[0] > 255 ? 255 : p[0]);
        if (s->np >= 5 && (p[1] == 1 || p[1] == 2)) {
            s->regs[s->reg] = p[1] == 1 ? hls_rgb(p[2], p[3], p[4]) : pct_rgb(p[2], p[3], p[4]);
            s->regmap[s->reg] = 0; /* the new colour takes a new entry: drawn pixels keep theirs */
        }
        break;
    case '!':
        s->rep = p[0] > 0 ? p[0] : 1;
        break;
    }
    s->mode = 0;
}

static void sixel_byte(vt_term *t, vt_u8 c)
{
    vt_sixel *s = t->six;
    if (s->mode) {
        if (c >= '0' && c <= '9') {
            if (!s->np)
                s->np = 1;
            if (s->prm[s->np - 1] < 100000L)
                s->prm[s->np - 1] = s->prm[s->np - 1] * 10 + (c - '0');
            return;
        }
        if (c == ';') {
            if (!s->np)
                s->np = 1;
            if (s->np < 5)
                s->prm[s->np++] = 0;
            return;
        }
        sixel_command(s);
    }
    if (c >= 0x3F && c <= 0x7E) {
        sixel_draw(t, s, c - 0x3F);
    } else if (c == '$') {
        s->x = 0;
    } else if (c == '-') {
        s->x = 0;
        s->y += 6L * s->aspect;
    } else if (c == '"' || c == '#' || c == '!') {
        s->mode = c;
        s->np = 0;
        s->prm[0] = s->prm[1] = s->prm[2] = s->prm[3] = s->prm[4] = 0;
    }
}

/* vt_feed's run of printable bytes inside a sixel: the decoder without a
 * trip through decode() and feed() for each. The count taken. */
static long sixel_run(vt_term *t, const vt_u8 *b, long n)
{
    long k;
    for (k = 0; k < n && b[k] >= 0x20 && b[k] < 0x7F; k++)
        sixel_byte(t, b[k]);
    return k;
}

static int dcs_is_params(const vt_term *t)
{
    int i;
    for (i = 0; i < t->str_len; i++)
        if ((t->str[i] < '0' || t->str[i] > '9') && t->str[i] != ';')
            return 0;
    return 1;
}

static void sixel_abort(vt_term *t);

/* DCS P1;P2;P3 q: the decoder takes the bytes until ST. */
static void sixel_begin(vt_term *t)
{
    vt_sixel *s;
    long p1 = 0;
    int i;
    for (i = 0; i < t->str_len && t->str[i] != ';'; i++)
        p1 = p1 * 10 + (t->str[i] - '0');
    sixel_abort(t); /* never two decoders */
    s = (vt_sixel *)VT_MALLOC(sizeof(vt_sixel));
    if (!s) {
        t->str_len = VT_STR_MAX - 1; /* no memory: swallowed as any string */
        return;
    }
    memset(s, 0, sizeof(*s));
    /* P1, the pixel aspect ratio (DEC: 2:1 unless said otherwise; the
     * raster attributes override it) */
    s->aspect = p1 == 2 ? 5 : (p1 == 3 || p1 == 4) ? 3 : (p1 >= 7 && p1 <= 9) ? 1 : 2;
    s->npal = 1; /* entry 0: the pixels nothing drew */
    if (t->six_private) {
        sixel_default_regs(t, s->own);
        s->regs = s->own;
    } else {
        if (!t->six_regs) {
            t->six_regs = (vt_u32 *)VT_MALLOC(256 * sizeof(vt_u32));
            if (t->six_regs)
                sixel_default_regs(t, t->six_regs);
        }
        if (t->six_regs) {
            s->regs = t->six_regs;
        } else {
            sixel_default_regs(t, s->own);
            s->regs = s->own;
        }
    }
    t->six = s;
    t->state = S_SIXEL;
    t->str_esc = 0;
}

static void sixel_abort(vt_term *t)
{
    if (!t->six)
        return;
    if (t->six->pix)
        VT_FREE(t->six->pix);
    VT_FREE(t->six);
    t->six = 0;
}

/* The image's cells: blanks marked VT_CELL_IMAGE on line y from x0, its
 * tile row k. */
static void sixel_place_row(vt_term *t, int slot, int y, int x0, int k)
{
    vt_line *l = t->scr[y];
    int x, x1 = x0 + image_cols(t->imgs[slot]);
    vt_cell b;
    if (x1 > t->cols)
        x1 = t->cols;
    if (x0 >= x1)
        return;
    unwide(t, x0, y);
    if (x1 < t->cols)
        unwide(t, x1 - 1, y);
    b.ch = ' ';
    b.fg = b.bg = VT_COLOR_DEFAULT;
    b.attr = 0;
    b.width = 1;
    b.deco = b.ext = 0;
    b.pad = VT_CELL_IMAGE;
    for (x = x0; x < x1; x++)
        l->c[x] = b;
    mark(t, x0, y, x1);
    place_add(t, l, slot, x0, k);
}

/* ST: the image is complete. It goes on the screen like text: at the
 * cursor, scrolling the region up when it runs past the bottom margin,
 * the cursor left on its last row in the column it started (xterm, with
 * sixel scrolling on, the default); with ?80 (DECSDM) at the top left,
 * cut at the bottom, the cursor where it was. */
static void sixel_end(vt_term *t)
{
    vt_sixel *s = t->six;
    vt_image *im;
    long w, h, y;
    int slot, i, rows, k, cy, cw, ch;
    if (!s)
        return;
    t->six = 0;
    w = s->w > s->rw ? s->w : s->rw;
    h = s->h > s->rh ? s->h : s->rh;
    if (w > s->aw || h > s->ah) {
        /* a raster size nothing drew into: the pixels for it, if there is room */
        if (!sixel_room(t, s, w, h)) {
            w = w < s->aw ? w : s->aw;
            h = h < s->ah ? h : s->ah;
        }
    }
    if (w <= 0 || h <= 0 || !s->pix) {
        if (s->pix)
            VT_FREE(s->pix);
        VT_FREE(s);
        return;
    }
    for (y = 1; y < h; y++) /* rows of w pixels, one after the other */
        memmove(s->pix + y * w, s->pix + y * s->aw, w);
    for (slot = 0; slot < VT_IMG_SLOTS && t->imgs[slot] && t->imgs[slot]->pix; slot++)
        ;
    if (slot == VT_IMG_SLOTS) {
        image_evict(t);
        for (slot = 0; slot < VT_IMG_SLOTS && t->imgs[slot] && t->imgs[slot]->pix; slot++)
            ;
    }
    if (slot < VT_IMG_SLOTS && !t->imgs[slot]) {
        t->imgs[slot] = (vt_image *)VT_MALLOC(sizeof(vt_image));
        if (t->imgs[slot])
            memset(t->imgs[slot], 0, sizeof(vt_image));
    }
    if (slot == VT_IMG_SLOTS || !t->imgs[slot]) {
        VT_FREE(s->pix);
        VT_FREE(s);
        return;
    }
    im = t->imgs[slot];
    im->pix = s->pix;
    im->bytes = s->aw * s->ah;
    im->w = (int)w;
    im->h = (int)h;
    cw = t->cell_w > 0 ? t->cell_w : 8;
    ch = t->cell_h > 0 ? t->cell_h : 16;
    im->cw = cw;
    im->ch = ch;
    im->npal = s->npal;
    im->refs = 0;
    im->serial = ++t->img_serial;
    for (i = 0; i < s->npal; i++)
        im->pal[i] = s->pal[i];
    im->pal[0] = vt_default_color(t, 1); /* unset: the background (the renderer's own) */
    t->img_mem += im->bytes;
    t->n_img++;
    VT_FREE(s);

    rows = (int)((h + ch - 1) / ch);
    if (t->sixel_dm) {
        for (k = 0; k < rows && k < t->rows; k++)
            sixel_place_row(t, slot, k, 0, k);
    } else {
        cy = t->cy;
        for (k = 0; k < rows; k++) {
            if (k) {
                if (cy == t->bot - 1)
                    scroll_up(t, t->top, t->bot, 1);
                else if (cy < t->rows - 1)
                    cy++;
                else
                    break; /* below the margins at the screen's bottom: cut */
            }
            sixel_place_row(t, slot, cy, t->cx, k);
        }
        t->cy = cy;
        t->wrap_pending = 0;
    }
    if (!im->refs)
        image_free(t, slot); /* nowhere on the screen */
}

/* XTSMGRAPHICS, CSI ? Pi ; Pa ; Pv S: the number of colour registers
 * (Pi 1) and the largest sixel image (Pi 2), read (Pa 1), reset (2), set
 * (3, answered with what stays in effect: both are fixed) or their
 * maximum (4). ReGIS (Pi 3) is not here. */
static void xtsmgraphics(vt_term *t)
{
    char b[48];
    long pi = param0(t, 0), pa = param0(t, 1), w, h;
    int n = put_csi(t, b);
    b[n++] = '?';
    n = fmt_uint(b, n, pi);
    b[n++] = ';';
    if (pi != 1 && pi != 2) {
        b[n++] = '1'; /* error in Pi */
        b[n++] = ';';
        b[n++] = '0';
    } else if (pa < 1 || pa > 4) {
        b[n++] = '2'; /* error in Pa */
        b[n++] = ';';
        b[n++] = '0';
    } else if (pi == 1) {
        b[n++] = '0';
        b[n++] = ';';
        n = fmt_uint(b, n, 256);
    } else {
        w = VT_SIXEL_MAX_W;
        h = VT_SIXEL_MAX_H;
        if (pa != 4 && t->cell_w > 0 && t->cell_h > 0) {
            /* what the window shows (xterm's answer), within the maximum */
            if ((long)t->cols * t->cell_w < w)
                w = (long)t->cols * t->cell_w;
            if ((long)t->rows * t->cell_h < h)
                h = (long)t->rows * t->cell_h;
        }
        b[n++] = '0';
        b[n++] = ';';
        n = fmt_uint(b, n, w);
        b[n++] = ';';
        n = fmt_uint(b, n, h);
    }
    b[n++] = 'S';
    reply(t, b, n);
}

int vt_images(const vt_term *t)
{
    return t->n_img;
}

int vt_row_image(const vt_term *t, int row, int i, vt_image_view *v)
{
    const vt_line *l;
    const vt_image *im;
    vt_u16 k;
    if (!t->n_img)
        return 0;
    if (row >= 0) {
        if (row >= t->rows)
            return 0;
        l = t->scr[row];
    } else {
        if (-row > t->sb_len)
            return 0;
        l = t->sb[(t->sb_head + t->sb_cap + row) % t->sb_cap];
    }
    for (k = l->img; k; k = t->pl[k].next) {
        im = place_image(t, &t->pl[k]);
        if (!im || i--)
            continue;
        v->pix = im->pix;
        v->w = im->w;
        v->h = im->h;
        v->pal = im->pal;
        v->npal = im->npal;
        v->cw = im->cw;
        v->ch = im->ch;
        v->col0 = t->pl[k].col0;
        v->py = t->pl[k].row * im->ch;
        v->serial = im->serial;
        return 1;
    }
    return 0;
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
    t->osc52 = 0;
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
    if (t->state == S_PRINT) {
        /* printer controller mode: everything is the printer's (there is
         * none) until CSI 4 i -- ESC [ 4 i or the 8-bit CSI 4 i */
        static const vt_u8 end[] = { 0x1B, '[', '4', 'i' };
        if (c == 0x9B)
            t->prn_match = 2;
        else if (c == end[t->prn_match])
            t->prn_match++;
        else
            t->prn_match = c == 0x1B;
        if (t->prn_match == 4)
            t->state = S_GROUND;
        return;
    }
    /* Anywhere transitions. */
    if (c == 0x18 || c == 0x1A) { /* CAN, SUB */
        if (t->state == S_SIXEL)
            sixel_abort(t); /* cancelled: no image */
        t->state = S_GROUND;
        return;
    }
    if (t->state == S_SIXEL) {
        if (t->str_esc) {
            t->str_esc = 0;
            sixel_end(t);
            t->state = S_GROUND;
            if (c == '\\')
                return;
            t->state = S_ESC; /* ESC ends the image and starts a new ESC */
            clear_params(t);
        } else if (c == 0x1B) {
            t->str_esc = 1;
            return;
        } else if (c == 0x9C) {
            sixel_end(t);
            t->state = S_GROUND;
            return;
        } else {
            if (c >= 0x20 && c < 0x7F)
                sixel_byte(t, (vt_u8)c);
            return; /* other controls inside the data are ignored */
        }
    } else if (t->state >= S_OSC) {
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
            if (t->osc52) {
                osc52_byte(t, c); /* the payload streams past str */
                return;
            }
            if (t->state == S_OSC && t->str_len == 2 && c == ';' && t->str[0] == '5' && t->str[1] == '2') {
                osc52_begin(t);
                return;
            }
            if (t->state == S_OSC && t->str_len == 1 && c == ';' && (t->str[0] == '7' || t->str[0] == '8')) {
                osc52_begin(t); /* OSC 7 and 8: URLs, longer than str can hold */
                t->osc52 = 3;
                t->long_cmd = (vt_u8)(t->str[0] - '0');
                return;
            }
            if (t->str_kind == 'P' && c == 'q' && t->state == S_STRING && dcs_is_params(t)) {
                sixel_begin(t); /* DCS P1;P2;P3 q: a sixel image */
                return;
            }
            if ((t->state == S_OSC || t->str_kind == 'P') && c >= 0x20) {
                /* as UTF-8, whole characters only (a title beyond the BMP
                 * was cut to 3 bytes of garbage) */
                vt_u8 u[4];
                int k, m = put_utf8(u, (long)c);
                if (t->str_len + m < VT_STR_MAX)
                    for (k = 0; k < m; k++)
                        t->str[t->str_len++] = (char)u[k];
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

static int alloc_screen(vt_line ***scr, int rows, int cols, vt_term *t)
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

static void free_screen(vt_term *t, vt_line **scr, int rows)
{
    int y;
    if (!scr)
        return;
    for (y = 0; y < rows; y++)
        if (scr[y])
            line_free(t, scr[y]);
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
    /* until the host reports what it draws with (vt_set_default_colors):
     * xterm's colour 7 on black, so a faint default is grey, not black */
    t->dflt[0] = t->dflt[2] = 0xE5E5E5UL;
    t->dflt[1] = 0x000000UL;
    t->clip_access = VT_CLIP_WRITE;
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
        line_free(t, t->sb[t->sb_head]);
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
        line_free(t, t->sb[oldest]);
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

static void sixel_abort(vt_term *t);

void vt_free(vt_term *t)
{
    int i;
    if (!t)
        return;
    pri_settle(t); /* t->pri is the allocation again */
    free_screen(t, t->pri, t->rows);
    free_screen(t, t->alt, t->rows);
    ovf_drop(t); /* into the scrollback, freed below */
    if (t->ovf)
        VT_FREE(t->ovf);
    while (t->sb_len) {
        t->sb_head = (t->sb_head + t->sb_cap - 1) % t->sb_cap;
        line_free(t, t->sb[t->sb_head]);
        t->sb_len--;
    }
    if (t->sb)
        VT_FREE(t->sb);
    sixel_abort(t);
    for (i = 0; i < VT_IMG_SLOTS; i++)
        if (t->imgs[i]) {
            image_free(t, i);
            VT_FREE(t->imgs[i]);
        }
    if (t->pl)
        VT_FREE(t->pl);
    if (t->six_regs)
        VT_FREE(t->six_regs);
    if (t->tabs)
        VT_FREE(t->tabs);
    if (t->clu)
        VT_FREE(t->clu);
    if (t->clu_hash)
        VT_FREE(t->clu_hash);
    if (t->clip)
        VT_FREE(t->clip);
    while (t->n_links)
        VT_FREE(t->links[--t->n_links].uri);
    if (t->cwd)
        VT_FREE(t->cwd);
    VT_FREE(t);
}

void vt_reset(vt_term *t)
{
    t->link = 0; /* no hyperlink open (the cells keep theirs) */
    if (t->cwd) {
        VT_FREE(t->cwd); /* the shell will say again */
        t->cwd = 0;
    }
    t->scr = t->pri;
    t->modes = VT_MODE_CURSOR_VISIBLE | VT_MODE_AUTOREPEAT | (t->bs_default ? VT_MODE_BACKSPACE_BS : 0);
    if (t->pers == VT_AMIGA)
        t->modes |= VT_MODE_NEWLINE; /* the console's LF starts a new line */
    t->amiga_dfg = VT_COLOR_DEFAULT;
    t->amiga_dbg = VT_COLOR_DEFAULT;
    t->amiga_dattr = 0;
    t->amiga_msb = 0;
    soft_reset(t);
    t->sav_1049 = t->sav;
    t->saved_1048 = 0;
    t->rect_extent = 0;
    t->raw_events = 0;
    t->amiga_bg = 0;
    t->scroll_enabled = 1;
    sixel_abort(t);
    t->state = S_GROUND;
    t->u_need = 0;
    t->last_ch = 0;
    t->sixel_dm = 0;
    t->six_private = 1;
    if (t->six_regs)
        sixel_default_regs(t, t->six_regs);
    t->title[0] = 0;
    /* what programs set that a full reset takes back (xterm's RIS): the
     * key encoding, the pushed titles, the cursor shape (to the host's) */
    t->n_titles = 0;
    t->mok = 0; /* modifyOtherKeys back to xterm's default */
    memset(t->kbd, 0, sizeof(t->kbd));
    t->kbd_top[0] = t->kbd_top[1] = 0;
    t->cursor_style = t->cursor_dflt;
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
        t->cursor_style = t->cursor_dflt = (vt_u8)(style >= 0 && style <= 6 ? style : 0);
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

void vt_set_backspace_bs(vt_term *t, int bs)
{
    t->bs_default = (vt_u8)(bs != 0);
    if (bs)
        t->modes |= VT_MODE_BACKSPACE_BS;
    else
        t->modes &= ~(vt_u32)VT_MODE_BACKSPACE_BS;
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
 * straight into the row: no decoding, one dirty mark per run. The last
 * column and the wrap are done here too, as put_char does them for a
 * plain character (ASM1: a long line paid a whole put_char, decode and
 * feed at every row's end). Anything that needs put_char's care (wide
 * cells being overwritten, no autowrap with a wrap pending) ends the run
 * there; the caller has checked the character set and insert mode, and
 * says whether the colours and attributes are the default ones (plain). */
static long put_ascii_run(vt_term *t, const vt_u8 *b, long n, int plain)
{
    long done = 0, k, room;
    for (;;) {
        vt_line *l;
        vt_cell *c;
        int x, x1, lc;
        if (t->wrap_pending) {
            if (!t->autowrap)
                break; /* put_char writes the last column again */
            t->scr[t->cy]->wrapped = 1;
            t->cx = 0;
            index_down(t);
            t->wrap_pending = 0;
        }
        l = t->scr[t->cy];
        x = t->cx;
        lc = l->dbl ? (t->cols + 1) / 2 : t->cols; /* row_cols */
        room = lc - 1 - x;                          /* the run stops short of the last column */
        if (room > n - done)
            room = n - done;
        if (room > 0) {
            c = &l->c[x];
            if (plain && x >= l->used) {
                /* Plain text into the untouched end of a line: those cells
                 * are default blanks (line_clear, `used`), so only their
                 * characters change -- 2 bytes a cell written instead of
                 * 16 (S1; plain lines on a stock A1200 are bound by these
                 * writes to chip RAM). The line stays chonly: its clear
                 * resets the characters alone. */
#ifdef VT_ASM
                k = vt_asm_put_ch(c, b + done, room);
#else
                for (k = 0; k < room && b[done + k] >= 0x20 && b[done + k] < 0x7F; k++)
                    c[k].ch = b[done + k];
#endif
            } else if (room == 1 || b[done + 1] < 0x20 || b[done + 1] >= 0x7F) {
                /* one character (a colour each, as colour output mostly
                 * has it): the cell in place -- no proto and no call
                 * (ASM1: 60 instructions the character through the
                 * assembler loop) */
                k = 0;
                if (c->width == 1 && (x + 1 >= t->cols || c[1].width != 0)) {
                    c->ch = b[done];
                    c->fg = t->fg;
                    c->bg = t->bg;
                    c->attr = t->attr;
                    c->deco = t->deco;
                    c->ext = t->ext;
                    c->pad = 0;
                    l->chonly = 0;
                    k = 1;
                }
            } else {
#ifdef VT_ASM
                /* the loop below in assembler (vtengine_68k.s): 14 us a
                 * character in C on a 14 MHz 68020 (S1) */
                vt_cell p;
                p.fg = t->fg;
                p.bg = t->bg;
                p.ch = 0;
                p.attr = t->attr;
                p.width = 1;
                p.deco = t->deco;
                p.ext = t->ext;
                p.pad = 0;
                k = vt_asm_put_run(c, b + done, room, &p);
#else
                const vt_u8 *q = b + done;
                for (k = 0; k < room; k++) {
                    if (q[k] < 0x20 || q[k] >= 0x7F)
                        break; /* the run of printable ASCII ends: the parser's byte */
                    if (c[k].width != 1 || (x + k + 1 < t->cols && c[k + 1].width == 0))
                        break; /* a wide glyph here: put_char unwides it */
                    c[k].ch = q[k];
                    c[k].fg = t->fg;
                    c[k].bg = t->bg;
                    c[k].attr = t->attr;
                    c[k].deco = t->deco;
                    c[k].ext = t->ext;
                    c[k].pad = 0; /* text over an image cell (the assembler copies the proto's 0) */
                }
#endif
                if (k)
                    l->chonly = 0;
            }
            if (!k)
                break;
            /* mark(), its checks known true here: y is the cursor's row,
             * and x < x1 < cols */
            x1 = x + (int)k;
            if (l->dx0 > x)
                l->dx0 = (short)x;
            if (l->dx1 < x1)
                l->dx1 = (short)x1;
            if (l->used < x1)
                l->used = (vt_u16)x1;
            t->dirty = 1;
            t->cx = x = x1;
            done += k;
            if (k < room)
                break; /* a byte or a cell for the parser or put_char */
        }
        /* the last column: put_char's work for a plain character */
        if (done >= n || x != lc - 1 || b[done] < 0x20 || b[done] >= 0x7F)
            break;
        c = &l->c[x];
        if (c->width != 1)
            break; /* half of a wide glyph: put_char unwides it */
        if (plain && x >= l->used) {
            c->ch = b[done];
        } else {
            c->ch = b[done];
            c->fg = t->fg;
            c->bg = t->bg;
            c->attr = t->attr;
            c->deco = t->deco;
            c->ext = t->ext;
            c->pad = 0;
            l->chonly = 0;
        }
        if (l->dx0 > x)
            l->dx0 = (short)x;
        if (l->dx1 < x + 1)
            l->dx1 = (short)(x + 1);
        if (l->used < x + 1)
            l->used = (vt_u16)(x + 1);
        t->dirty = 1;
        done++;
        if (t->pers == VT_AMIGA && t->autowrap) {
            /* the ROM console wraps at once, no deferred wrap */
            l->wrapped = 1;
            t->cx = 0;
            index_down(t);
        } else {
            t->wrap_pending = 1;
        }
        if (done >= n || b[done] < 0x20 || b[done] >= 0x7F)
            break;
    }
    if (done)
        t->last_ch = b[done - 1];
    return done;
}

/* "ESC [ parameters final" whole in the buffer, the parameters only
 * digits, ';' and ':': parsed here in one go and dispatched, instead of a
 * walk through decode() and feed() for every byte (colour output is
 * mostly these: 52 us a byte on a 14 MHz 68020, the terminal's slowest
 * path; S1). Anything else -- a private marker, an intermediate, a
 * control inside, the sequence cut by the end of the write -- returns 0
 * with nothing changed that the parser's own ESC does not set again.
 * The bytes consumed. */
#ifdef VT_ASM
#define csi_scan vt_asm_csi
#else
/* csi_fast's parameter scan (vtengine_68k.s has it as vt_asm_csi): the
 * bytes from p (after "ESC [") are digits, ';' and ':' up to the first
 * other byte, the final. Their values go to params / sub as feed()'s CSI
 * states put them: a value saturates at VT_PARAM_MAX, ':' marks the
 * parameter it starts, parameters past VT_MAX_PARAMS run on into the last.
 * No parameter: params[0] = sub[0] = 0. np << 16 | the final's offset
 * from p, or -1 when the n bytes (at most 32767 are looked at) end first.
 * The parameter being read stays in v until its separator. */
static long csi_scan(const vt_u8 *p, long n, long *params, vt_u8 *sub)
{
    long i, v = 0;
    int np = 0;
    if (n > 0x7FFF)
        n = 0x7FFF;
    for (i = 0; i < n; i++) {
        vt_u8 c = p[i];
        if ((vt_u8)(c - '0') <= 9) {
            v = v < VT_PARAM_MAX / 10 ? v * 10 + (long)(c - '0') : VT_PARAM_MAX;
            if (!np) {
                np = 1;
                sub[0] = 0;
            }
        } else if (c == ';' || c == ':') {
            if (!np) {
                np = 1;
                sub[0] = 0;
            }
            params[np - 1] = v;
            if (np < VT_MAX_PARAMS) {
                sub[np] = (vt_u8)(c == ':');
                np++;
                v = 0;
            } /* else: more digits go on into the last one, as feed() */
        } else {
            if (np) {
                params[np - 1] = v;
            } else {
                params[0] = 0;
                sub[0] = 0;
            }
            return (long)np << 16 | i;
        }
    }
    return -1;
}
#endif

static long csi_fast(vt_term *t, const vt_u8 *b, long len)
{
    long r;
    int np;
    vt_u8 c;
    if (len < 3 || b[1] != '[')
        return 0;
    r = csi_scan(b + 2, len - 2, t->params, t->sub);
    if (r < 0)
        return 0;
    np = (int)(r >> 16);
    r = 2 + (r & 0xFFFF); /* the final's offset from ESC */
    c = b[r];
    if (c < 0x40 || c > 0x7E)
        return 0;
    /* clear_params' work, now that the sequence is known whole */
    t->np = np;
    t->have = 0;
    t->priv = 0;
    t->inter = 0;
    t->ninter = 0;
    t->csi8 = 0;
    /* the state is ground, as after the final byte. With no private marker
     * and no intermediate every personality takes SGR, CUP and the erases
     * to the same place: straight there */
    if (c == 'm')
        sgr(t);
    else if (c == 'H' || c == 'K' || c == 'J' || c == 'f')
        csi_common(t, c);
    else
        csi_dispatch(t, c);
    return r + 1;
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

/* A printable character of U+00A0 and up whole at b, in the ground state:
 * its code point in *cp and its length in bytes; 0 when it is anything
 * else (a C1 control, a cut or bad UTF-8 sequence, a surrogate), which
 * decode() and feed() take as before. decode -> feed -> put_char cost
 * three calls and a personality switch for every byte of it (ASM1:
 * UPDemo's half blocks, three UTF-8 bytes a cell). */
static int fast_cp(const vt_term *t, const vt_u8 *b, long len, vt_u32 *cp)
{
    vt_u32 v;
    vt_u8 c0 = b[0];
    if (t->pers == VT_PCANSI || (t->pers == VT_XTERM && t->cp437)) {
        if (c0 < 0x80 || c0 == 0x9B)
            return 0;
        *cp = cp437_hi[c0 - 0x80];
        return *cp >= 0xA0;
    }
    if (t->pers == VT_AMIGA || !t->utf8) { /* Latin-1 */
        *cp = c0;
        return c0 >= 0xA0;
    }
    if ((c0 & 0xE0) == 0xC0 && c0 >= 0xC2) {
        if (len < 2 || (b[1] & 0xC0) != 0x80)
            return 0;
        v = ((vt_u32)(c0 & 0x1F) << 6) | (b[1] & 0x3F);
        if (v < 0xA0)
            return 0;
        *cp = v;
        return 2;
    }
    if ((c0 & 0xF0) == 0xE0) {
        if (len < 3 || (b[1] & 0xC0) != 0x80 || (b[2] & 0xC0) != 0x80)
            return 0;
        v = ((vt_u32)(c0 & 0x0F) << 12) | ((vt_u32)(b[1] & 0x3F) << 6) | (b[2] & 0x3F);
        if (v < 0xA0 || (v >= 0xD800 && v <= 0xDFFF))
            return 0;
        *cp = v;
        return 3;
    }
    return 0;
}

void vt_feed(vt_term *t, const vt_u8 *buf, long len)
{
    long i = 0, k;
    /* what put_ascii_run may do, worked out once and kept while only text,
     * CR and LF go by (none of them changes it): -1 not known, 0 printable
     * ASCII needs put_char (a character set, insert mode, SO), 1 it may
     * run, 2 it may run and the colours are the default ones (ASM1: the
     * five tests a run cost 17 instructions, twice a line) */
    int text = -1;
    while (i < len) {
        vt_u8 b = buf[i];
        if (b != 0x09)
            t->tab_end = 0; /* only a tab right after a tab keeps it */
        if (t->state == S_GROUND && !t->u_need) {
            /* the ground state's common bytes straight to their action
             * (S1): decode -> feed -> put_char / exec_c0 cost three calls
             * and a personality switch a byte */
            if (b >= 0x20) {
                if (b < 0x7F) {
                    if (text < 0) /* (or-ed, not ||: one branch each instead of nine) */
                        text = (t->insert | t->single_shift | t->amiga_msb) || t->charset[t->gl] != 'B' ? 0
                             : ((t->fg ^ VT_COLOR_DEFAULT) | (t->bg ^ VT_COLOR_DEFAULT) | t->attr | t->deco | t->ext) ? 1
                             : 2;
                    if (text) {
                        k = put_ascii_run(t, buf + i, len - i, text == 2); /* as far as printable ASCII goes */
                        if (k) {
                            i += k;
                            continue;
                        }
                    }
                } else if (b >= 0x80) {
                    vt_u32 cp;
                    int m = fast_cp(t, buf + i, len - i, &cp);
                    if (m) {
                        put_char(t, cp); /* (it ends a single shift) */
                        text = -1;
                        i += m;
                        continue;
                    }
                }
            } else if (b == 0x0D || b == 0x0A) {
                if (b == 0x0D) {
                    t->cx = 0;
                    t->wrap_pending = 0;
                    if (++i >= len || buf[i] != 0x0A)
                        continue;
                    /* CR LF: the LF in the same turn of the loop */
                }
                /* a run of newlines is one lf_run */
                k = 1;
                while (i + k < len && buf[i + k] == 0x0A)
                    k++;
                lf_run(t, k);
                i += k;
                continue;
            } else if (b == 0x1B) {
                k = csi_fast(t, buf + i, len - i);
                if (k) {
                    text = -1;
                    i += k;
                    continue;
                }
            }
        } else if (t->state == S_SIXEL && b >= 0x20 && b < 0x7F && !t->str_esc && !t->u_need) {
            i += sixel_run(t, buf + i, len - i); /* image data: at least this byte */
            continue;
        }
        decode(t, b);
        text = -1;
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
    return c->ch == ' ' && c->width == 1 && !c->attr && !c->deco && !c->ext && !c->pad &&
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
    l->used = (vt_u16)w->cols; /* the cells copied in are not marked one by one */
    l->chonly = 0;
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
    last = line_text_len(old[e], old[e]->n);
    for (r = s; r <= e; r++) {
        n = r < e ? old[r]->n : last; /* a scrollback row keeps the width it had */
        for (x = 0; x < n; x++) {
            c = &old[r]->c[x];
            if (c->width == 0)
                continue; /* the right half of a wide glyph moves with its left */
            if (r < e && x == n - 1 && cell_plain_blank(c) && old[r + 1]->c[0].width == 2 &&
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
                if (c->pad & VT_CELL_IMAGE)
                    place_carry(w->t, old[r], x, w->out[w->ny], w->nx);
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

/* One pass over rows old[0..nold), oldest first: their logical lines typed
 * again into out (NULL: count only). The screen's rows are oc wide; a
 * scrollback row keeps the width it had. Rows below both the cursor and
 * the last text are left out (the caller pads with blanks). A double-width
 * or -height row is never joined: it keeps its place, cut or padded.
 * Returns the row count, -1 when out of memory (the rows made so far stay
 * in out). */
static int reflow_pass(vt_term *t, vt_line **old, int nold, int oc, vt_line **out, int cols, int cx, int cy,
                       int wp, int *ncx, int *ncy, int *nwp)
{
    vt_line *l;
    vt_rewrap w;
    int s, e, last, n;
    w.t = t;
    w.out = out;
    w.cols = cols;
    w.eager = t->pers == VT_AMIGA && t->autowrap;
    w.ny = 0;
    for (last = nold - 1; last > cy; last--)
        if (old[last]->wrapped || old[last]->dbl || line_text_len(old[last], old[last]->n))
            break;
    for (s = 0; s <= last; s = e + 1) {
        e = s;
        if (old[s]->dbl) {
            if (out) {
                if (!(l = line_new(cols)))
                    return -1;
                line_clear(t, l, cols);
                n = cols < old[s]->n ? cols : old[s]->n;
                memcpy(l->c, old[s]->c, n * sizeof(vt_cell));
                if (l->c[cols - 1].width == 2)
                    blank_cell(t, &l->c[cols - 1]);
                l->dbl = old[s]->dbl;
                l->used = (vt_u16)cols;
                l->chonly = 0;
                l->mark = old[s]->mark;
                out[w.ny] = l;
            }
            if (s == cy) {
                *ncx = clampi(cx, 0, cols - 1);
                *ncy = w.ny;
                *nwp = 0;
            }
        } else {
            while (old[e]->wrapped && e + 1 < nold && !old[e + 1]->dbl)
                e++;
            if (!rewrap_line(&w, old, s, e, oc, cx, cy, wp, ncx, ncy, nwp))
                return -1;
        }
        w.ny++;
    }
    return w.ny;
}

/* vt_resize with reflow, for the primary screen (the alternate one is
 * resized plainly, as xterm does: full-screen programs redraw it anyway).
 * hist: the scrollback is laid out with it, as one text -- a line that
 * began in the scrollback joins its rest on the screen, and the rows above
 * the new screen go back to the scrollback (a grow brings them down again,
 * as iTerm2 and Terminal.app do). Without hist (the amiga personality,
 * the ROM console's reflow, keeps no scrollback; or no memory for the
 * whole history) the screen and the rows an earlier resize pushed out are
 * laid out, and the rows that do not fit are pushed out again for a later
 * grow. Rows that do not fit go from below the cursor first, then off the
 * top, as resize_screen does. 0 when out of memory, nothing changed. */
static int reflow_screen(vt_term *t, int cols, int rows, int *cx, int *cy, int *wp, int hist)
{
    vt_line **nr, **nw, **all;
    int nsb = hist ? t->sb_len : 0, above = nsb + t->novf, nold = above + t->rows;
    int total, i, ok, ncx = 0, ncy = 0, nwp = 0, excess, below, drop_top = 0;
    /* the scrollback (oldest first), the rows an earlier resize pushed out,
     * then the screen: laid out together, as one screen `above` rows taller */
    all = (vt_line **)VT_MALLOC(nold * sizeof(vt_line *));
    if (!all)
        return 0;
    for (i = 0; i < nsb; i++)
        all[i] = t->sb[(t->sb_head + t->sb_cap - nsb + i) % t->sb_cap];
    if (t->novf)
        memcpy(all + nsb, t->ovf, t->novf * sizeof(vt_line *));
    memcpy(all + above, t->pri, t->rows * sizeof(vt_line *));
    total = reflow_pass(t, all, nold, t->cols, 0, cols, *cx, *cy + above, *wp, &ncx, &ncy, &nwp);
    nr = (vt_line **)VT_MALLOC(total * sizeof(vt_line *));
    nw = (vt_line **)VT_MALLOC(rows * sizeof(vt_line *));
    if (!nr || !nw) {
        if (nr)
            VT_FREE(nr);
        if (nw)
            VT_FREE(nw);
        VT_FREE(all);
        return 0;
    }
    memset(nr, 0, total * sizeof(vt_line *));
    memset(nw, 0, rows * sizeof(vt_line *));
    ok = reflow_pass(t, all, nold, t->cols, nr, cols, *cx, *cy + above, *wp, &ncx, &ncy, &nwp) >= 0;
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
                line_free(t, nr[i]);
        for (i = total; i < rows; i++)
            if (nw[i])
                VT_FREE(nw[i]);
        VT_FREE(nr);
        VT_FREE(nw);
        VT_FREE(all);
        return 0;
    }
    if (total > rows) {
        excess = total - rows;
        below = total - 1 - ncy;
        if (below > excess)
            below = excess;
        drop_top = excess - below;
        for (i = total - below; i < total; i++)
            line_free(t, nr[i]);
        total = rows;
        ncy -= drop_top;
    }
    /* the old rows (the scrollback's and the pushed-out ones among them)
     * are laid out anew */
    for (i = 0; i < nold; i++)
        line_free(t, all[i]);
    VT_FREE(all);
    t->novf = 0;
    if (hist)
        t->sb_len = t->sb_head = 0;
    VT_FREE(t->pri);
    for (i = 0; i < drop_top; i++) {
        if (hist)
            sb_push(t, nr[i]); /* history again, oldest first */
        else
            ovf_push(t, nr[i]); /* pushed out now: back on a later grow */
    }
    for (i = 0; i < total; i++)
        nw[i] = nr[drop_top + i];
    VT_FREE(nr);
    t->pri = nw;
    if (hist)
        t->scrolled += t->sb_len - nsb; /* grid row + scrolled still counts from the oldest line */
    *cx = ncx;
    *cy = ncy;
    *wp = nwp;
    return 1;
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
            line_free(t, old[y]);
        for (y = 0; y < drop_top; y++) {
            if (is_pri)
                sb_push(t, old[y]);
            else
                line_free(t, old[y]);
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
                nl->used = l->used;
                nl->img = l->img; /* its images move with the cells */
                nl->mark = l->mark;
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
    pri_settle(t); /* resize and reflow free or keep t->pri as an allocation */
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
        reflowed = t->reflow &&
                   ((t->pers != VT_AMIGA && t->sb_cap && reflow_screen(t, cols, rows, &pcx, &pcy, &pwp, 1)) ||
                    reflow_screen(t, cols, rows, &pcx, &pcy, &pwp, 0));
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
    if (t->pri != t->pri_mem) { /* replaced by a plain array: widened again on the next scroll */
        t->pri_mem = 0;
        t->pri_spare = 0;
    }
    mark_rows(t, 0, rows);
    flush(t);
    report_size(t);
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

int vt_wrap_pending(const vt_term *t)
{
    return t->wrap_pending != 0;
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
    if (out && max < 1)
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
            char u[VT_CELL_UTF8_MAX];
            int k, m;
            if (c[x].width == 0)
                continue;
            m = vt_cell_utf8(t, &c[x], u);
            if (!out) {
                len += m;
                continue;
            }
            if (len + m >= max)
                goto done;
            for (k = 0; k < m; k++)
                out[len++] = u[k];
        }
        if (y < by && !vt_row_wrapped(t, y)) {
            if (out && len + 1 >= max)
                goto done;
            if (out)
                out[len++] = '\n';
            else
                len++;
        }
    }
done:
    if (out)
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

/* 0xRRGGBB of a colour vt_resolve_colors works with (xterm, pcansi) */
static vt_u32 colour_rgb(const vt_term *t, vt_color c)
{
    if (c & VT_COLOR_RGB)
        return VT_RGB_OF(c);
    if (c == VT_COLOR_DEFAULT)
        return vt_default_color(t, 0);
    if (c == VT_COLOR_DEFAULT_BG)
        return vt_default_color(t, 1);
    return vt_palette_rgb(t, (int)(c & 0xFF));
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
    if (c->attr & VT_ATTR_FAINT) {
        if (t->pers == VT_AMIGA) {
            /* pens 7 and 15 become pen 2, as before (its pens have no RGB
             * the engine knows; the console's own pen is unverified,
             * conformance matrix SGR 2) */
            if (f == 7 || f == 15)
                f = 2;
        } else {
            /* any colour, halfway to the background: as a direct colour,
             * which a palette screen shows with the nearest xterm-256 pen */
            vt_u32 fr = colour_rgb(t, f), br = colour_rgb(t, b);
            f = VT_RGB((((fr >> 16) & 0xFF) + ((br >> 16) & 0xFF)) / 2,
                       (((fr >> 8) & 0xFF) + ((br >> 8) & 0xFF)) / 2, ((fr & 0xFF) + (br & 0xFF)) / 2);
        }
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

static int legacy_key(const vt_term *t, long key, int mods, vt_u8 *out);

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
        /* xterm: Ctrl+Backspace is the other one of BS and DEL */
        out[n++] = (vt_u8)(t->modes & VT_MODE_BACKSPACE_BS ? 0x7F : 0x08);
        return n;
    }
    return n + legacy_key(t, key, mods & ~(VT_MOD_ALT | VT_MOD_CTRL), out + n);
}

/* ---- the kitty keyboard protocol (sw.kovidgoyal.net/kitty/keyboard-protocol) ---- */

int vt_kitty_flags(const vt_term *t)
{
    int s;
    if (!t || t->pers != VT_XTERM)
        return 0;
    s = t->scr == t->alt && t->alt;
    return t->kbd[s][t->kbd_top[s]];
}

/* CSI number [; modifiers] final for a functional key; 0 when key is not one. */
static int kitty_functional(long key, long *num, char *fin)
{
    static const struct { long key; long num; char fin; } fk[] = {
        { VT_KEY_UP, 1, 'A' }, { VT_KEY_DOWN, 1, 'B' }, { VT_KEY_RIGHT, 1, 'C' }, { VT_KEY_LEFT, 1, 'D' },
        { VT_KEY_HOME, 1, 'H' }, { VT_KEY_END, 1, 'F' }, { VT_KEY_F1, 1, 'P' }, { VT_KEY_F2, 1, 'Q' },
        { VT_KEY_F3, 13, '~' }, { VT_KEY_F4, 1, 'S' }, { VT_KEY_INSERT, 2, '~' }, { VT_KEY_DELETE, 3, '~' },
        { VT_KEY_PAGE_UP, 5, '~' }, { VT_KEY_PAGE_DOWN, 6, '~' }, { VT_KEY_F5, 15, '~' },
        { VT_KEY_F6, 17, '~' }, { VT_KEY_F7, 18, '~' }, { VT_KEY_F8, 19, '~' }, { VT_KEY_F9, 20, '~' },
        { VT_KEY_F10, 21, '~' }, { VT_KEY_F11, 23, '~' }, { VT_KEY_F12, 24, '~' }, { VT_KEY_HELP, 28, '~' },
        { VT_KEY_ESCAPE, 27, 'u' }, { VT_KEY_RETURN, 13, 'u' }, { VT_KEY_TAB, 9, 'u' },
        { VT_KEY_BACKSPACE, 127, 'u' }, { VT_KEY_KP_ENTER, 57414, 'u' }
    };
    int i;
    for (i = 0; i < (int)(sizeof(fk) / sizeof(fk[0])); i++)
        if (fk[i].key == key) {
            *num = fk[i].num;
            *fin = fk[i].fin;
            return 1;
        }
    return 0;
}

/* The kitty protocol's own numbers for the keypad (KP_0 57399 .. KP_ADD
 * 57413); 0 for a key it has none for (the parentheses). */
static long kitty_keypad(long key)
{
    static const long kp[] = { 57399, 57400, 57401, 57402, 57403, 57404, 57405, 57406, 57407, 57408,
                               57409, 57412, 57413, 57411, 57410 }; /* 0-9 . - + * / */
    if (key >= VT_KEY_KP_0 && key <= VT_KEY_KP_SLASH)
        return kp[key - VT_KEY_KP_0];
    return 0;
}

int vt_encode_key_kitty(const vt_term *t, long key, int mods, int event, long shifted, long base,
                        long text, vt_u8 *out)
{
    int f = vt_kitty_flags(t), dis, n = 0, i, k;
    long num = 0, kp;
    char fin = 'u', b[80];
    int with_mods, with_text;
    if (!f)
        return event == VT_KEY_EV_RELEASE ? 0 : legacy_key(t, key, mods, out);
    dis = f & (VT_KITTY_DISAMBIGUATE | VT_KITTY_ALL_KEYS);
    if (!(f & VT_KITTY_EVENTS)) {
        if (event == VT_KEY_EV_RELEASE)
            return 0;
        event = VT_KEY_EV_PRESS;
    }
    mods &= VT_MOD_SHIFT | VT_MOD_ALT | VT_MOD_CTRL;
    if (key < 0x110000) {
        /* a text key: the text itself, unless a modifier other than Shift
         * makes it ambiguous, or every key is to be an escape code */
        if (event != VT_KEY_EV_RELEASE && !(f & VT_KITTY_ALL_KEYS) && !(mods & ~VT_MOD_SHIFT))
            return legacy_key(t, text ? text : (mods & VT_MOD_SHIFT) && shifted ? shifted : key, 0, out);
        num = key;
    } else if ((kp = kitty_keypad(key)) != 0) {
        if (!(f & VT_KITTY_ALL_KEYS))
            return event == VT_KEY_EV_RELEASE ? 0 : legacy_key(t, key, mods, out);
        num = kp;
    } else if (kitty_functional(key, &num, &fin)) {
        int legacy_bytes = key == VT_KEY_RETURN || key == VT_KEY_TAB || key == VT_KEY_BACKSPACE;
        if (!dis && event == VT_KEY_EV_PRESS)
            return legacy_key(t, key, mods, out);
        if (legacy_bytes && !(f & VT_KITTY_ALL_KEYS)) {
            /* typing "reset" after a crash must still work: plain Return,
             * Tab and Backspace stay as they were, and send no release */
            if (event == VT_KEY_EV_RELEASE)
                return 0;
            if (!mods)
                return legacy_key(t, key, 0, out);
        }
    } else {
        return event == VT_KEY_EV_RELEASE ? 0 : legacy_key(t, key, mods, out);
    }
    with_text = (f & VT_KITTY_TEXT) && (f & VT_KITTY_ALL_KEYS) && key < 0x110000 && text >= 0x20 &&
                text != 0x7F && event != VT_KEY_EV_RELEASE;
    with_mods = mods || event != VT_KEY_EV_PRESS;
    b[n++] = 0x1B;
    b[n++] = '[';
    if (fin == 'u' || num != 1 || with_mods)
        n = fmt_uint(b, n, num);
    if ((f & VT_KITTY_ALTERNATES) && key < 0x110000) {
        int sh = (mods & VT_MOD_SHIFT) && shifted && shifted != num;
        if (sh) {
            b[n++] = ':';
            n = fmt_uint(b, n, shifted);
        }
        if (base && base != num) {
            if (!sh)
                b[n++] = ':';
            b[n++] = ':';
            n = fmt_uint(b, n, base);
        }
    }
    if (with_mods || with_text)
        b[n++] = ';';
    if (with_mods) {
        n = fmt_uint(b, n, 1 + mods);
        if (event != VT_KEY_EV_PRESS) {
            b[n++] = ':';
            b[n++] = (char)('0' + event);
        }
    }
    if (with_text) {
        b[n++] = ';';
        n = fmt_uint(b, n, text);
    }
    b[n++] = fin;
    for (i = 0, k = n; i < k; i++)
        out[i] = (vt_u8)b[i];
    return n;
}

/* What the keymap made, for the kitty protocol: a control character back
 * to its key (Ctrl+A is ^A), Shift's capital back to the key's own letter. */
static int kitty_from_char(const vt_term *t, long key, int mods, vt_u8 *out)
{
    long code = key, shifted = 0, text = 0;
    if (key < 0x110000) {
        if ((mods & VT_MOD_CTRL) && (key < 0x20 || key == 0x7F)) {
            code = key == 0x7F ? '?' : key + 0x40;
            if (code >= 'A' && code <= 'Z')
                code += 0x20;
        } else {
            text = key;
            if (key >= 'A' && key <= 'Z' && (mods & VT_MOD_SHIFT)) {
                shifted = key;
                code = key + 0x20;
            }
        }
    }
    return vt_encode_key_kitty(t, code, mods, VT_KEY_EV_PRESS, shifted, 0, text, out);
}

int vt_encode_key(const vt_term *t, long key, int mods, vt_u8 *out)
{
    if (vt_kitty_flags(t))
        return kitty_from_char(t, key, mods, out);
    return legacy_key(t, key, mods, out);
}

/* xterm's keys (and the other personalities'): what vt_encode_key sends
 * when no kitty flags are set. */
static int legacy_key(const vt_term *t, long key, int mods, vt_u8 *out)
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
        /* xterm: DEL, or BS under DECBKM (?67) / the profile's backspace = bs */
        out[n++] = (vt_u8)(t->pers != VT_XTERM || (t->modes & VT_MODE_BACKSPACE_BS) ? 0x08 : 0x7F);
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
    return vt_encode_mouse_px(t, button, kind, x, y, x * t->cell_w, y * t->cell_h, mods, out);
}

int vt_encode_mouse_px(const vt_term *t, int button, int kind, int x, int y, int px, int py,
                       int mods, vt_u8 *out)
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
    if (kind == 2 && button == 3 && !(m & VT_MODE_MOUSE_ANY))
        return 0; /* ?1002 reports moves only with a button down */
    if (kind == 1 && (m & VT_MODE_MOUSE_X10) && !(m & (VT_MODE_MOUSE_NORMAL | VT_MODE_MOUSE_BUTTON |
                                                      VT_MODE_MOUSE_ANY)))
        return 0; /* X10 reports presses only */
    if (button >= 64 && kind != 0)
        return 0; /* the wheel has no release */
    cb = button;
    if (kind == 1 && !(m & (VT_MODE_MOUSE_SGR | VT_MODE_MOUSE_PIXELS)))
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
    if (m & (VT_MODE_MOUSE_SGR | VT_MODE_MOUSE_PIXELS)) {
        int pix = (m & VT_MODE_MOUSE_PIXELS) != 0; /* ?1016: pixels, from 1 */
        b[n++] = '<';
        n = fmt_uint(b, n, cb);
        b[n++] = ';';
        n = fmt_uint(b, n, (pix ? px : x) + 1);
        b[n++] = ';';
        n = fmt_uint(b, n, (pix ? py : y) + 1);
        b[n++] = (char)(kind == 1 ? 'm' : 'M');
    } else if (m & VT_MODE_MOUSE_URXVT) {
        /* ?1015: CSI Cb;Cx;Cy M, decimal, the legacy button code */
        n = fmt_uint(b, n, 32 + cb);
        b[n++] = ';';
        n = fmt_uint(b, n, x + 1);
        b[n++] = ';';
        n = fmt_uint(b, n, y + 1);
        b[n++] = 'M';
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

int vt_encode_focus(const vt_term *t, int in, vt_u8 *out)
{
    if (!(t->modes & VT_MODE_FOCUS) || t->pers != VT_XTERM)
        return 0;
    out[0] = 0x1B;
    out[1] = '[';
    out[2] = (vt_u8)(in ? 'I' : 'O');
    return 3;
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
