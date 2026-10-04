/* updemo core: see updemo.h. Portable C89, no OS calls.
 *
 * The effects draw into a picture of 256-colour pixels, two to a
 * character cell (the upper half block: its foreground is the top pixel,
 * its background the bottom one), and blit() sends only the rows that
 * changed, with a colour sequence only where the colour changes. The
 * vector scene uses the quadrant blocks instead: four pixels a cell.
 * All arithmetic is integer; the sine table is built at the start. */
#include <string.h>
#include "updemo.h"

#define MAXC 132
#define MAXR 60

static const updemo_io *io;
static int W, R, H; /* columns; the picture's text rows (the last row is the status line); its pixel rows (2 R) */
static int touring;  /* the tour plays (updemo_tour): captions, any key ends it */

/* ---- output -------------------------------------------------------------- */

static char ob[16384];
static long on;

static void flush(void)
{
    if (on)
        io->write(io->user, ob, on);
    on = 0;
}

static void pc(int c)
{
    if (on >= (long)sizeof(ob))
        flush();
    ob[on++] = (char)c;
}

static void ps(const char *s)
{
    while (*s)
        pc(*s++);
}

static void pn(long n)
{
    char b[12];
    int i = 0;
    if (n < 0) {
        pc('-');
        n = -n;
    }
    do {
        b[i++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);
    while (i)
        pc(b[--i]);
}

static void at(int row, int col)
{
    ps("\033[");
    pn(row);
    pc(';');
    pn(col);
    pc('H');
}

/* the colours the terminal has now, as far as we set them: -1 unknown */
static int cfg = -1, cbg = -1;

static void sgr0(void)
{
    ps("\033[0m");
    cfg = cbg = -1;
}

static void fg(int c)
{
    if (c == cfg)
        return;
    ps("\033[38;5;");
    pn(c);
    pc('m');
    cfg = c;
}

static void bg(int c)
{
    if (c == cbg)
        return;
    ps("\033[48;5;");
    pn(c);
    pc('m');
    cbg = c;
}

/* s, then blanks up to width cells (s is ASCII) */
static void padded(const char *s, int width)
{
    int n = (int)strlen(s);
    ps(s);
    for (; n < width; n++)
        pc(' ');
}

static void centred(int row, const char *s)
{
    at(row, (W - (int)strlen(s)) / 2 + 1);
    ps(s);
}

/* ---- numbers ------------------------------------------------------------- */

static signed char sn[256]; /* 127 sin(2 pi i / 256) */
#define SN(a) ((int)sn[(a) & 255])
#define CS(a) ((int)sn[((a) + 64) & 255])

static unsigned long seed = 0x55502D54UL;

static int rnd(void)
{
    seed = seed * 1103515245UL + 12345UL;
    return (int)((seed >> 16) & 0x7FFF);
}

static void numbers_init(void)
{
    int i;
    for (i = 0; i < 128; i++) {
        /* a parabola arch for each half wave: within 1 % of the sine's shape
         * where it matters here (smooth, symmetric, the right period) */
        int v = i * (128 - i) * 127 / 4096;
        sn[i] = (signed char)v;
        sn[i + 128] = (signed char)-v;
    }
}

/* the 6 x 6 x 6 colour cube's index: r, g, b 0..5 */
static int cube(int r, int g, int b)
{
    return 16 + 36 * r + 6 * g + b;
}

static unsigned char pal[256]; /* the scene's value -> 256-colour index */

static void pal_rainbow(void)
{
    int i;
    for (i = 0; i < 256; i++)
        pal[i] = (unsigned char)cube(((SN(i) + 127) * 5 + 127) / 254, ((SN(i + 85) + 127) * 5 + 127) / 254,
                                     ((SN(i + 170) + 127) * 5 + 127) / 254);
}

static void pal_fire(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        if (i < 64)
            pal[i] = (unsigned char)cube(i * 5 / 63, 0, 0);
        else if (i < 128)
            pal[i] = (unsigned char)cube(5, (i - 64) * 5 / 63, 0);
        else if (i < 192)
            pal[i] = (unsigned char)cube(5, 5, (i - 128) * 5 / 63);
        else
            pal[i] = 231;
    }
}

/* ---- the picture ---------------------------------------------------------- */

static unsigned char pix[MAXR * 2][MAXC], old[MAXR * 2][MAXC];
static int old_ok;

static void pix_clear(int c)
{
    int y;
    for (y = 0; y < H; y++)
        memset(pix[y], c, (size_t)W);
}

/* The rows that changed, to the terminal. A cell of one colour is a blank
 * on that background (or a full block when it is the foreground already);
 * two colours are a half block, the upper or the lower one, whichever the
 * colours set now fit. */
static void blit(void)
{
    int r, x;
    for (r = 0; r < R; r++) {
        const unsigned char *a = pix[2 * r], *b = pix[2 * r + 1];
        if (old_ok && !memcmp(a, old[2 * r], (size_t)W) && !memcmp(b, old[2 * r + 1], (size_t)W))
            continue;
        memcpy(old[2 * r], a, (size_t)W);
        memcpy(old[2 * r + 1], b, (size_t)W);
        at(r + 1, 1);
        for (x = 0; x < W; x++) {
            int t = a[x], u = b[x];
            if (t == u) {
                if (cbg == t)
                    pc(' ');
                else if (cfg == t)
                    ps("\342\226\210"); /* full block */
                else {
                    bg(t);
                    pc(' ');
                }
            } else if (cfg == u && cbg == t)
                ps("\342\226\204"); /* lower half */
            else {
                fg(t);
                bg(u);
                ps("\342\226\200"); /* upper half */
            }
        }
    }
    old_ok = 1;
}

/* ---- the 5 x 7 font (columns, bit 0 the top row) --------------------------- */

static const char font_chars[] = " !',-./0123456789:?ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const unsigned char font_cols[][5] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x5F, 0x00, 0x00 }, { 0x00, 0x05, 0x03, 0x00, 0x00 },
    { 0x00, 0x50, 0x30, 0x00, 0x00 }, { 0x08, 0x08, 0x08, 0x08, 0x08 }, { 0x00, 0x60, 0x60, 0x00, 0x00 },
    { 0x20, 0x10, 0x08, 0x04, 0x02 },
    { 0x3E, 0x51, 0x49, 0x45, 0x3E }, { 0x00, 0x42, 0x7F, 0x40, 0x00 }, { 0x42, 0x61, 0x51, 0x49, 0x46 },
    { 0x21, 0x41, 0x45, 0x4B, 0x31 }, { 0x18, 0x14, 0x12, 0x7F, 0x10 }, { 0x27, 0x45, 0x45, 0x45, 0x39 },
    { 0x3C, 0x4A, 0x49, 0x49, 0x30 }, { 0x01, 0x71, 0x09, 0x05, 0x03 }, { 0x36, 0x49, 0x49, 0x49, 0x36 },
    { 0x06, 0x49, 0x49, 0x29, 0x1E },
    { 0x00, 0x36, 0x36, 0x00, 0x00 }, { 0x02, 0x01, 0x51, 0x09, 0x06 },
    { 0x7E, 0x11, 0x11, 0x11, 0x7E }, { 0x7F, 0x49, 0x49, 0x49, 0x36 }, { 0x3E, 0x41, 0x41, 0x41, 0x22 },
    { 0x7F, 0x41, 0x41, 0x22, 0x1C }, { 0x7F, 0x49, 0x49, 0x49, 0x41 }, { 0x7F, 0x09, 0x09, 0x09, 0x01 },
    { 0x3E, 0x41, 0x49, 0x49, 0x7A }, { 0x7F, 0x08, 0x08, 0x08, 0x7F }, { 0x00, 0x41, 0x7F, 0x41, 0x00 },
    { 0x20, 0x40, 0x41, 0x3F, 0x01 }, { 0x7F, 0x08, 0x14, 0x22, 0x41 }, { 0x7F, 0x40, 0x40, 0x40, 0x40 },
    { 0x7F, 0x02, 0x0C, 0x02, 0x7F }, { 0x7F, 0x04, 0x08, 0x10, 0x7F }, { 0x3E, 0x41, 0x41, 0x41, 0x3E },
    { 0x7F, 0x09, 0x09, 0x09, 0x06 }, { 0x3E, 0x41, 0x51, 0x21, 0x5E }, { 0x7F, 0x09, 0x19, 0x29, 0x46 },
    { 0x46, 0x49, 0x49, 0x49, 0x31 }, { 0x01, 0x01, 0x7F, 0x01, 0x01 }, { 0x3F, 0x40, 0x40, 0x40, 0x3F },
    { 0x1F, 0x20, 0x40, 0x20, 0x1F }, { 0x3F, 0x40, 0x38, 0x40, 0x3F }, { 0x63, 0x14, 0x08, 0x14, 0x63 },
    { 0x07, 0x08, 0x70, 0x08, 0x07 }, { 0x61, 0x51, 0x49, 0x45, 0x43 }
};

/* column col (0..5; 5 is the gap) of ch: 7 bits */
static int font_col(int ch, int col)
{
    const char *p;
    if (col > 4)
        return 0;
    p = strchr(font_chars, ch);
    return (p && ch) ? font_cols[p - font_chars][col] : 0;
}

/* s into the picture: a font pixel is one cell wide and one text row high */
static void big_text(const char *s, int x0, int y0, int colour)
{
    int i, c, j;
    for (i = 0; s[i]; i++)
        for (c = 0; c < 6; c++) {
            int bits = font_col(s[i], c), x = x0 + i * 6 + c;
            if (x < 0 || x >= W)
                continue;
            for (j = 0; j < 7; j++)
                if ((bits >> j) & 1) {
                    int y = y0 + j * 2;
                    if (y >= 0 && y + 1 < H)
                        pix[y][x] = pix[y + 1][x] = (unsigned char)colour;
                }
        }
}

/* ---- text scenes ------------------------------------------------------------ */

static int shown; /* the items a text scene has drawn so far */

static void box_line(int row, int x0, const char *l, const char *m, const char *r)
{
    int i;
    at(row, x0);
    ps(l);
    for (i = 0; i < 20; i++)
        ps("\342\224\200");
    ps(m);
    for (i = 0; i < 41; i++)
        ps("\342\224\200");
    ps(r);
}

static void box_row(int row, int x0, const char *a, const char *b)
{
    at(row, x0);
    ps("\342\224\202 ");
    fg(81);
    padded(a, 19);
    sgr0();
    ps("\342\224\202 ");
    padded(b, 40);
    ps("\342\224\202");
}

static void sc_text(long t, int first)
{
    static const char title[] = "UP-Term";
    static const char sub[] = "a modern terminal for AmigaOS";
    static const char *const rows[][2] = {
        { "dialect", "xterm, with Amiga and PC-ANSI windows" },
        { "colours", "16, 256 and 24-bit" },
        { "text", "UTF-8, box drawing, outline fonts" },
        { "windows", "tabs, a screen of its own, full screen" },
        { "programs", "vim, tmux, screen, ssh, less, mc" }
    };
    int x0 = (W - 64) / 2 + 1, i, half;
    if (first)
        shown = 0;
    while (shown < 12 && t >= shown * 12L) {
        switch (shown) {
        case 0: /* double height: the same text on both halves' lines */
            for (half = 0; half < 2; half++) {
                at(2 + half, 1);
                ps(half ? "\033#4" : "\033#3");
                for (i = 0; i < (W / 2 - 7) / 2; i++)
                    pc(' ');
                pal_rainbow();
                for (i = 0; title[i]; i++) {
                    fg(pal[i * 24]);
                    pc(title[i]);
                }
                sgr0();
            }
            break;
        case 1: /* double width */
            at(5, 1);
            ps("\033#6");
            for (i = 0; i < (W / 2 - (int)sizeof(sub) + 1) / 2; i++)
                pc(' ');
            ps(sub);
            break;
        case 2:
            at(7, (W - 62) / 2 + 1);
            ps("\033[1mbold\033[0m  \033[2mfaint\033[0m  \033[3mitalic\033[0m  \033[4munderline\033[0m  "
               "\033[21mdouble\033[0m  \033[5mblink\033[0m  \033[7minverse\033[0m  \033[9mstrike\033[0m");
            break;
        case 3:
            box_line(9, x0, "\342\225\255", "\342\224\254", "\342\225\256");
            break;
        case 4:
            box_row(10, x0, rows[0][0], rows[0][1]);
            box_line(11, x0, "\342\224\234", "\342\224\274", "\342\224\244");
            break;
        case 5: case 6: case 7: case 8:
            box_row(7 + shown, x0, rows[shown - 4][0], rows[shown - 4][1]);
            break;
        case 9:
            box_line(16, x0, "\342\225\260", "\342\224\264", "\342\225\257");
            break;
        case 10:
            fg(244);
            centred(18, touring ? "a tour of what UP-Term does: any key ends it"
                                : "next: colours, then the demoscene part");
            sgr0();
            break;
        default:
            break;
        }
        shown++;
    }
}

static void rgb_of_hue(int h, int dim, int *r, int *g, int *b)
{
    *r = (SN(h) + 128) >> dim;
    *g = (SN(h + 85) + 128) >> dim;
    *b = (SN(h + 170) + 128) >> dim;
}

static void sc_colours(long t, int first)
{
    int x0 = (W - 72) / 2 + 1, i, r, g, b, row;
    if (first) {
        at(2, x0);
        ps("16 colours");
        at(3, x0);
        for (i = 0; i < 16; i++) {
            bg(i);
            ps("    ");
        }
        sgr0();
        ps("        ");
        at(5, x0);
        ps("256 colours");
        for (g = 0; g < 6; g++) {
            at(6 + g, x0);
            for (r = 0; r < 6; r++)
                for (b = 0; b < 6; b++) {
                    bg(cube(r, g, b));
                    ps("  ");
                }
            sgr0();
        }
        at(12, x0);
        for (i = 232; i < 256; i++) {
            bg(i);
            ps("   ");
        }
        sgr0();
        at(14, x0);
        ps("24-bit colour");
    }
    /* two rows of half blocks, four hue bands sliding along */
    for (row = 0; row < 2; row++) {
        at(15 + row, x0);
        for (i = 0; i < 72; i++) {
            int tr, tg, tb, br, bgr, bb;
            rgb_of_hue(i * 3 + (int)t * 2, row, &tr, &tg, &tb);
            rgb_of_hue(i * 3 + (int)t * 2 + 128, row ? 2 : 1, &br, &bgr, &bb);
            ps("\033[38;2;");
            pn(tr); pc(';'); pn(tg); pc(';'); pn(tb);
            ps(";48;2;");
            pn(br); pc(';'); pn(bgr); pc(';'); pn(bb);
            ps("m\342\226\200");
        }
        sgr0();
    }
}

static void sc_region(long t, int first)
{
    static const char *const verb[] = { "compiling", "linking", "packing", "testing", "installing" };
    static const char *const what[] = { "engine/vtengine.c", "render/amiga_render.c", "handler/vtcon_handler.c",
                                        "shell/sh_exec.c", "tty/ldisc.c", "render/outline.c", "zm/zmodem.c",
                                        "demo/updemo.c" };
    int v = rnd() % 5, w = rnd() % 8, ok = rnd() % 12;
    if (first) {
        at(1, 1);
        ps("\033[1mScroll regions\033[0m: the two lines above the rule stay, the log scrolls under them.");
        at(2, 1);
        fg(244);
        ps("Programs like vim, less and tmux move text this way instead of redrawing it.");
        sgr0();
        at(3, 1);
        fg(240);
        for (v = 0; v < W; v++)
            ps("\342\224\200");
        sgr0();
        ps("\033[4;");
        pn(R);
        pc('r');
        return;
    }
    at(R, 1);
    pc('\n');
    fg(240);
    pc('[');
    pn(t / 50);
    pc('.');
    pn(t % 50 * 2 / 10); /* hundredths, two digits */
    pn(t % 50 * 2 % 10);
    ps("] ");
    fg(75 + v * 36);
    padded(verb[v], 11);
    sgr0();
    padded(what[w], 26);
    if (ok == 0) {
        fg(214);
        ps("warning: unused variable");
    } else {
        fg(77);
        ps("ok");
    }
    sgr0();
}

static void sc_end(long t, int first)
{
    static const char *const show_lines[] = {
        "UP-Term",
        "",
        "an xterm on the Amiga, by Up Rough",
        "",
        "everything you saw was plain text:",
        "escape sequences, UTF-8, 256 and 24-bit colours",
        "",
        "your shell is behind this screen, as you left it"
    };
    static const char *const tour_lines[] = {
        "UP-Term",
        "",
        "an xterm on the Amiga, by Up Rough",
        "",
        "everything you saw was plain text:",
        "escape sequences, UTF-8, 256 and 24-bit colours",
        "",
        "Help > Demo tour (or /demo) plays it again"
    };
    const char *const *lines = touring ? tour_lines : show_lines;
    int i;
    if (first)
        pal_rainbow();
    for (i = 0; i < 8; i++) {
        if (i == 0)
            ps("\033[1m");
        fg(pal[(i * 20 + (int)t * 3) & 255]);
        centred(R / 2 - 4 + i, lines[i]);
        if (i == 0)
            ps("\033[22m");
    }
    sgr0();
}

/* ---- the demoscene part ------------------------------------------------------- */

static void sc_bars(long t, int first)
{
    static const unsigned char hue[6][3] = { { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 }, { 0, 1, 1 }, { 0, 0, 1 }, { 1, 0, 1 } };
    int k, d, x0, y0;
    (void)first;
    pix_clear(16);
    for (k = 0; k < 6; k++) {
        int yc = H / 2 + SN((int)t * 2 + k * 22) * (H / 2 - 6) / 127;
        for (d = -5; d <= 5; d++) {
            int y = yc + d, s = 5 - (d < 0 ? -d : d);
            if (y < 0 || y >= H)
                continue;
            if (s == 5) /* the bar's highlight: its colour towards white */
                memset(pix[y], cube(hue[k][0] ? 5 : 3, hue[k][1] ? 5 : 3, hue[k][2] ? 5 : 3), (size_t)W);
            else
                memset(pix[y], cube(hue[k][0] * (s + 1), hue[k][1] * (s + 1), hue[k][2] * (s + 1)), (size_t)W);
        }
    }
    x0 = (W - 41) / 2 + SN((int)t * 3) * 6 / 127;
    y0 = ((H - 14) / 2) & ~1;
    big_text("UP-TERM", x0 + 1, y0 + 2, 16);
    big_text("UP-TERM", x0, y0, 231);
}

static void sc_plasma(long t, int first)
{
    int x, y, a = (int)t;
    if (first)
        pal_rainbow();
    for (y = 0; y < H; y++) {
        unsigned char *p = pix[y];
        int ry = SN(y * 6 + SN(a * 2)) + SN(y * 3 - a * 3), kx = SN(a) >> 5, ky = y * CS(a) >> 6;
        for (x = 0; x < W; x++) {
            int v = ry + SN(x * 4 + a * 3) + SN((x * 2 + y * 3) + a * 5) + SN(x * kx + ky);
            p[x] = pal[((v >> 2) + a) & 255];
        }
    }
}

static unsigned char fire[MAXR * 2 + 2][MAXC];

static void sc_fire(long t, int first)
{
    int x, y;
    (void)t;
    if (first) {
        pal_fire();
        memset(fire, 0, sizeof(fire));
    }
    for (x = 0; x < W; x++) {
        int v = (rnd() & 3) ? 255 : 0;
        fire[H][x] = fire[H + 1][x] = (unsigned char)v;
    }
    for (y = 0; y < H; y++) {
        const unsigned char *b1 = fire[y + 1], *b2 = fire[y + 2];
        unsigned char *p = fire[y], *o = pix[y];
        for (x = 0; x < W; x++) {
            int l = x ? x - 1 : 0, r = x < W - 1 ? x + 1 : x;
            int v = (b1[l] + b1[x] + b1[r] + b2[x]) * 8 / 33;
            p[x] = (unsigned char)v;
            o[x] = v < 12 ? 16 : pal[v];
        }
    }
}

static void sc_roto(long t, int first)
{
    int a = (int)t * 2, z = 150 + SN((int)t * 3), x, y;
    long du = (long)CS(a) * z >> 3, dv = (long)SN(a) * z >> 3;
    long u0 = (long)t * 700, v0 = (long)SN((int)t) * 300;
    if (first)
        pal_rainbow();
    for (y = 0; y < H; y++) {
        long u = u0 - du * (W / 2) - dv * (y - H / 2), v = v0 - dv * (W / 2) + du * (y - H / 2);
        unsigned char *p = pix[y];
        for (x = 0; x < W; x++) {
            int c = (int)(((u >> 12) ^ (v >> 12)) & 31);
            p[x] = c & 16 ? pal[(c * 8 + (int)t) & 255] : 16 + (c & 1 ? 0 : 1);
            u += du;
            v += dv;
        }
    }
}

/* the vector scene: four pixels a cell, the quadrant blocks */
static unsigned char qb[MAXR * 2][MAXC * 2];
static int QW, QH;

static void q_line(int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx - dy;
    for (;;) {
        int e2;
        if (x0 >= 0 && x0 < QW && y0 >= 0 && y0 < QH)
            qb[y0][x0] = 1;
        if (x0 == x1 && y0 == y1)
            break;
        e2 = 2 * e;
        if (e2 > -dy) {
            e -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            e += dx;
            y0 += sy;
        }
    }
}

static void q_cube(int size, int ax, int ay, int az)
{
    static const signed char vtx[8][3] = { { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 },
                                           { -1, -1, 1 },  { 1, -1, 1 },  { 1, 1, 1 },  { -1, 1, 1 } };
    static const unsigned char edge[12][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 },
                                               { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
    int px[8], py[8], i, k = (QH / 2 - 1) * 5 / 2;
    for (i = 0; i < 8; i++) {
        int x = vtx[i][0] * size, y = vtx[i][1] * size, z = vtx[i][2] * size, n;
        n = (y * CS(ax) - z * SN(ax)) >> 7; z = (y * SN(ax) + z * CS(ax)) >> 7; y = n;
        n = (x * CS(ay) + z * SN(ay)) >> 7; z = (z * CS(ay) - x * SN(ay)) >> 7; x = n;
        n = (x * CS(az) - y * SN(az)) >> 7; y = (x * SN(az) + y * CS(az)) >> 7; x = n;
        z += 300;
        /* a quadrant pixel is twice as high as wide: x counts double */
        px[i] = QW / 2 + 2 * x * k / z;
        py[i] = QH / 2 + y * k / z;
    }
    for (i = 0; i < 12; i++)
        q_line(px[edge[i][0]], py[edge[i][0]], px[edge[i][1]], py[edge[i][1]]);
}

static void sc_cube(long t, int first)
{
    /* bit 0 upper left, 1 upper right, 2 lower left, 3 lower right */
    static const char *const quad[16] = {
        " ", "\342\226\230", "\342\226\235", "\342\226\200", "\342\226\226", "\342\226\214", "\342\226\236",
        "\342\226\233", "\342\226\227", "\342\226\232", "\342\226\220", "\342\226\234", "\342\226\204",
        "\342\226\231", "\342\226\237", "\342\226\210"
    };
    int a = (int)t, r, x;
    if (first)
        pal_rainbow();
    QW = 2 * W;
    QH = 2 * R;
    for (r = 0; r < QH; r++)
        memset(qb[r], 0, (size_t)QW);
    q_cube(64, a * 2, a * 3, a);
    q_cube(30, -a * 3, a * 2, -a * 2);
    bg(16);
    for (r = 0; r < R; r++) {
        const unsigned char *u = qb[2 * r], *l = qb[2 * r + 1];
        at(r + 1, 1);
        fg(pal[(r * 9 + a * 2) & 255]);
        for (x = 0; x < W; x++)
            ps(quad[u[2 * x] | u[2 * x + 1] << 1 | l[2 * x] << 2 | l[2 * x + 1] << 3]);
    }
}

static struct { long x; int y, speed; } star[64];

static void sc_scroll(long t, int first)
{
    static const char msg[] = "UP-TERM ... A MODERN TERMINAL FOR AMIGAOS ... XTERM, 256 COLOURS, 24-BIT COLOUR, "
                              "UTF-8, TABS, VIM, TMUX AND SSH ... ALL OF THIS IS TEXT IN A SHELL WINDOW ... "
                              "GREETINGS TO EVERYONE WHO KEEPS THE AMIGA ALIVE!       ";
    int i, x, j, amp = (H - 14) / 2 - 1, len = (int)sizeof(msg) - 1;
    long scroll = t * 2 / 3;
    if (first) {
        pal_rainbow();
        for (i = 0; i < 64; i++) {
            star[i].x = rnd() % (W * 16);
            star[i].y = rnd() % H;
            star[i].speed = 1 + rnd() % 3;
        }
    }
    pix_clear(16);
    for (i = 0; i < 64; i++) {
        star[i].x -= star[i].speed * 6;
        if (star[i].x < 0)
            star[i].x += W * 16;
        pix[star[i].y][star[i].x >> 4] = (unsigned char)(236 + star[i].speed * 6);
    }
    if (amp < 0)
        amp = 0;
    for (x = 0; x < W; x++) {
        long p = x + scroll;
        int bits = font_col(msg[(p / 12) % len], (int)(p % 12) / 2);
        int y0 = (H - 14) / 2 + SN(x * 3 + (int)t * 4) * amp / 127, c = pal[(x * 2 + (int)t * 3) & 255];
        if (!bits)
            continue;
        for (j = 0; j < 7; j++)
            if ((bits >> j) & 1) {
                int y = y0 + j * 2;
                if (y >= 0 && y + 1 < H)
                    pix[y][x] = pix[y + 1][x] = (unsigned char)c;
            }
    }
}

/* palette cycling: the picture is drawn once in colours 16-47, then only
 * those 32 palette entries change (OSC 4) -- the terminal recolours what
 * is on screen */
static void sc_cycle(long t, int first)
{
    int x, y, k;
    if (first) {
        for (y = 0; y < H; y++)
            for (x = 0; x < W; x++) {
                long dx = x - W / 2, dy = y - H / 2, d2 = dx * dx + dy * dy, d = 0;
                while ((d + 1) * (d + 1) <= d2)
                    d++;
                pix[y][x] = (unsigned char)(16 + ((d * 3 / 2 + (dx * dy >> 5)) & 31));
            }
    }
    for (k = 0; k < 32; k++) {
        static const char hex[] = "0123456789abcdef";
        int r, g, b, v[3], i;
        rgb_of_hue(k * 8 - (int)t * 5, 0, &r, &g, &b);
        v[0] = r; v[1] = g; v[2] = b;
        ps("\033]4;");
        pn(16 + k);
        ps(";#");
        for (i = 0; i < 3; i++) {
            int c = v[i] > 255 ? 255 : v[i];
            if (k & 8) /* every other band darker: rings */
                c >>= 2;
            pc(hex[c >> 4]);
            pc(hex[c & 15]);
        }
        pc(7);
    }
}


/* ---- the tour (updemo_tour) ----------------------------------------------------- */

/* What the tour reads back from the terminal: keys end it; mouse reports
 * are kept for the mouse scene; the window's size (CSI 18 t) is how it
 * follows a resize. */
enum { EV_NONE, EV_KEY, EV_MOUSE, EV_SIZE };

static int quit_req;         /* a key came while the tour waited for something else */
static int want_size;        /* a scene asks: the size, after this frame */
static int ev_rows, ev_cols; /* the size the terminal reported last */
static struct { int b, x, y, press; } mev[64];
static int nmev;

/* "a;b;c": up to max numbers, the missing ones 0 */
static void nums(const char *s, int *v, int max)
{
    int n = 0;
    while (n < max) {
        v[n] = 0;
        while (*s >= '0' && *s <= '9')
            v[n] = v[n] * 10 + (*s++ - '0');
        n++;
        if (*s != ';')
            break;
        s++;
    }
    while (n < max)
        v[n++] = 0;
}

static void add_mouse(int b, int x, int y, int press)
{
    if (nmev == (int)(sizeof(mev) / sizeof(mev[0])))
        return;
    mev[nmev].b = b;
    mev[nmev].x = x;
    mev[nmev].y = y;
    mev[nmev].press = press;
    nmev++;
}

/* One thing from the terminal, waiting up to wait_ms for it. A key's
 * sequence (a cursor key: ESC [ A, CSI A, ESC O P) is one key. */
static int read_event(int wait_ms)
{
    char sq[40];
    int k = io->key(io->user, wait_ms), n = 0, f, v[3];
    if (k < 0)
        return EV_NONE;
    if (k == 27) {
        k = io->key(io->user, 40);
        if (k == 'O') {
            io->key(io->user, 40);
            return EV_KEY;
        }
        if (k != '[')
            return EV_KEY; /* Esc alone, or Meta and a key */
        k = 0x9B;
    }
    if (k != 0x9B)
        return EV_KEY;
    for (;;) {
        f = io->key(io->user, 40);
        if (f < 0)
            return EV_KEY;
        if (f >= 0x40 && f <= 0x7E)
            break;
        if (n < (int)sizeof(sq) - 1)
            sq[n++] = (char)f;
    }
    sq[n] = 0;
    if (f == 'M' && n == 0) { /* X10 mouse: three bytes, each 32 + the value */
        v[0] = io->key(io->user, 40) - 32;
        v[1] = io->key(io->user, 40) - 32;
        v[2] = io->key(io->user, 40) - 32;
        if (v[0] < 0 || v[1] < 1 || v[2] < 1)
            return EV_NONE;
        add_mouse(v[0], v[1], v[2], (v[0] & 3) != 3);
        return EV_MOUSE;
    }
    if (sq[0] == '<' && (f == 'M' || f == 'm')) { /* SGR mouse: < b ; x ; y M (m: released) */
        nums(sq + 1, v, 3);
        add_mouse(v[0], v[1], v[2], f == 'M');
        return EV_MOUSE;
    }
    if (f == 't' && sq[0] == '8' && sq[1] == ';') { /* CSI 8 ; rows ; cols t */
        nums(sq + 2, v, 2);
        ev_rows = v[0];
        ev_cols = v[1];
        return EV_SIZE;
    }
    return EV_KEY;
}

/* The window's size, asked (CSI 18 t); 1 when the answer came */
static int ask_size(void)
{
    int i;
    ps("\033[18t");
    flush();
    for (i = 0; i < 25 && !quit_req; i++)
        switch (read_event(40)) {
        case EV_SIZE:
            return 1;
        case EV_KEY:
            quit_req = 1;
            break;
        default:
            break;
        }
    return 0;
}

/* the layout for a cols x rows window; 0 when it is too small */
static int set_size(int cols, int rows)
{
    if (cols < UPDEMO_MIN_COLS || rows < UPDEMO_MIN_ROWS)
        return 0;
    W = cols > MAXC ? MAXC : cols;
    R = (rows > MAXR ? MAXR : rows) - 1;
    H = 2 * R;
    return 1;
}

static int digits(long n)
{
    int d = 1;
    while (n >= 10) {
        n /= 10;
        d++;
    }
    return d;
}

static void heading(const char *title, const char *sub)
{
    at(1, 2);
    ps("\033[1m");
    ps(title);
    sgr0();
    if (sub) {
        at(2, 2);
        fg(246);
        ps(sub);
        sgr0();
    }
}

static void grey_line(int row, const char *s)
{
    at(row, 2);
    fg(244);
    ps(s);
    sgr0();
}

/* every SGR style, one more each 6 ticks */
static void sc_styles(long t, int first)
{
    static const char *const st[][2] = {
        { "1", "bold" }, { "2", "faint" }, { "3", "italic" }, { "4", "underline" },
        { "21", "double underline" }, { "4:3", "curly underline" }, { "4:4", "dotted underline" },
        { "4:5", "dashed underline" }, { "4:3;58;5;196", "red curly underline" }, { "53", "overline" },
        { "9", "strike through" }, { "7", "inverse" }, { "5", "blink" }, { "6;5", "rapid blink" },
        { "73", "superscript" }, { "74", "subscript" }, { "51", "framed" }, { "52", "encircled" },
        { "8", "concealed (select it)" }, { "1;4;38;5;208", "several at once" }
    };
    int n = (int)(sizeof(st) / sizeof(st[0])), half = (n + 1) / 2;
    if (first) {
        shown = 0;
        heading("Text styles", "SGR: what a program asks for with ESC [ n m");
    }
    while (shown < n && t >= shown * 6L) {
        int col = shown < half ? 2 : W / 2 + 1, row = 4 + (shown < half ? shown : shown - half);
        at(row, col);
        fg(244);
        ps("SGR ");
        padded(st[shown][0], 13);
        sgr0();
        ps("\033[");
        ps(st[shown][0]);
        pc('m');
        ps(st[shown][1]);
        sgr0();
        shown++;
    }
}

/* DEC line graphics, then the same letters as Latin-1 bytes and as UTF-8 */
static void sc_charsets(long t, int first)
{
    (void)t;
    if (!first)
        return;
    heading("Character sets", "the VT100's line graphics, Latin-1 and UTF-8");
    at(4, 2);
    ps("ESC ( 0 makes letters lines, as on a VT100:");
    at(5, 4);
    ps("\033(0lqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqqqqqqqqk\033(B");
    at(6, 4);
    ps("\033(0x\033(B UP-Term          \033(0x\033(B l q k x m j      \033(0x\033(B");
    at(7, 4);
    ps("\033(0tqqqqqqqqqqqqqqqqqqnqqqqqqqqqqqqqqqqqqu\033(B");
    at(8, 4);
    ps("\033(0x\033(B the set:         \033(0x `afgjklmnopqrstu x");
    at(9, 4);
    ps("mqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqqj\033(B");
    at(11, 2);
    ps("ESC % @, then Latin-1 bytes (the Amiga's own set):");
    at(12, 4);
    fg(81);
    ps("\033%@\306ble p\345 \305land, gar\347on, se\361or, Gr\374\337e, \251 \261 \274 \275 \276\033%G");
    sgr0();
    at(14, 2);
    ps("ESC % G, back to UTF-8: the same letters");
    at(15, 4);
    fg(81);
    ps("\303\206ble p\303\245 \303\205land, gar\303\247on, se\303\261or, Gr\303\274\303\237e, "
       "\302\251 \302\261 \302\274 \302\275 \302\276");
    sgr0();
    grey_line(17, "Amiga programs keep their 8-bit CSI and Latin-1 in every dialect.");
}

/* Unicode past Latin-1: scripts, symbols, wide characters, combining marks */
static void sc_unicode(long t, int first)
{
    static const char *const rows[][2] = {
        { "Greek", "\316\232\316\261\316\273\316\267\316\274\316\255\317\201\316\261 "
                   "\316\272\317\214\317\203\316\274\316\265" },
        { "Cyrillic", "\320\227\320\264\321\200\320\260\320\262\321\201\321\202\320\262\321\203\320\271, "
                      "\320\274\320\270\321\200" },
        { "Symbols", "\342\210\221 \342\210\253 \342\210\232 \342\210\236 \342\211\240 \342\211\244 "
                     "\342\211\245 \342\206\222 \342\207\222 \342\210\200 \342\210\203 \342\210\210 "
                     "\302\260 \342\202\254 \302\243 \302\245" },
        { "Blocks", "\342\226\221\342\226\222\342\226\223\342\226\210 \342\226\200\342\226\204\342\226\214"
                    "\342\226\220 \342\227\242\342\227\243\342\227\244\342\227\245 "
                    "\342\227\217\342\227\213\342\227\206\342\227\207" },
        { "Wide, two cells", "\346\274\242\345\255\227 \343\201\262\343\202\211\343\201\214\343\201\252 "
                             "\343\202\253\343\202\277\343\202\253\343\203\212 "
                             "\355\225\234\352\265\255\354\226\264" },
        { "Full width", "\357\274\265\357\274\260\357\274\215\357\274\264\357\275\205\357\275\222\357\275\215" },
        { "Combining marks", "e\314\201 a\314\210 n\314\203 o\314\202 c\314\247 u\314\212   "
                             "Z\314\266a\314\266l\314\266g\314\266o\314\266" },
        { "Past U+FFFF", "\360\235\220\224\360\235\220\217-\360\235\220\223\360\235\220\236\360\235\220\253"
                         "\360\235\220\246  \360\235\225\254\360\235\226\222\360\235\226\216\360\235\226\214"
                         "\360\235\226\206" }
    };
    /* a blank line between the rows when the window has room for them */
    int n = (int)(sizeof(rows) / sizeof(rows[0])), gap = R >= 4 + 2 * n ? 2 : 1;
    if (first) {
        shown = 0;
        heading("Unicode", "UTF-8: a character a cell (two for the wide ones), accents on their letter");
    }
    while (shown < n && t >= shown * 10L) {
        at(4 + gap * shown, 2);
        fg(244);
        padded(rows[shown][0], 18);
        sgr0();
        ps(rows[shown][1]);
        shown++;
        if (shown == n)
            grey_line(4 + gap * n, "What your font lacks comes from an outline font: /font-fallback NAME");
    }
}

/* a tmux window split in two: each pane is a scroll region of its own */
static void sc_tmux(long t, int first)
{
    static const char *const verb[] = { "compiling", "linking", "packing", "testing" };
    static const char *const what[] = { "engine/vtengine.c", "render/vtwin.c", "handler/slash.c",
                                        "shell/vsh.c", "demo/updemo.c" };
    static long last;
    int mid = R / 2, x;
    if (first) {
        last = -1;
        at(1, 1);
        ps("\033[36m~\033[0m % make");
        at(mid, 1);
        fg(34);
        for (x = 0; x < W; x++)
            ps("\342\224\200");
        sgr0();
        at(mid + 1, 1);
        ps("\033[36m~\033[0m % ping amiga.lan");
        at(R, 1);
        ps("\033[30;42m[0] 0:vsh* 1:vim-");
        for (x = 17; x < W - 14; x++)
            pc(' ');
        ps("\"amiga\" 12:00\033[K");
        sgr0();
    }
    if (t / 8 == last)
        return;
    last = t / 8;
    if (last & 1) { /* the top pane: rows 2 .. mid - 1 */
        ps("\033[2;");
        pn(mid - 1);
        pc('r');
        at(mid - 1, 1);
        pc('\n');
        fg(75 + (int)(last % 4) * 36);
        padded(verb[last / 2 % 4], 11);
        sgr0();
        ps(what[last / 2 % 5]);
    } else { /* the bottom pane: rows mid + 2 .. R - 1 */
        ps("\033[");
        pn(mid + 2);
        pc(';');
        pn(R - 1);
        pc('r');
        at(R - 1, 1);
        pc('\n');
        ps("64 bytes from 192.168.1.10: icmp_seq=");
        pn(last / 2);
        ps(" ttl=64 time=");
        pn(1 + last % 3);
        pc('.');
        pn(last * 7 % 10);
        ps(" ms");
    }
    ps("\033[r");
}

/* a shell session, typed */
struct shot {
    int at;          /* the tick its prompt shows */
    const char *cmd; /* typed, a character every two ticks */
    const char *out; /* then shown at once */
};

static const struct shot vsh_shots[] = {
    { 0, "ls --color", "\033[1;34mDevs\033[0m  \033[1;34mFonts\033[0m  \033[1;32mUPDemo\033[0m  notes.txt  "
                       "todo.txt  \033[1;34mWork\033[0m" },
    { 60, "cat notes.txt todo.txt | grep -c Amiga", "7" },
    { 160, "echo $(( 6 * 7 )) $HOME", "42 SYS:" },
    { 230, "for f in *.txt; do wc -l $f; done", "12 notes.txt\r\n4 todo.txt" },
    { 330, "sleep 30 &", "[1] 1083712" },
    { 380, "jobs", "[1] Running  sleep 30" },
    { 430, "", "" }
};

static int shot_n, shot_typed, shot_prompt;

static void shots(long t, int first, const struct shot *sh, int n, const char *prompt)
{
    if (first)
        shot_n = shot_typed = shot_prompt = 0;
    while (shot_n < n && t >= sh[shot_n].at) {
        const struct shot *s = &sh[shot_n];
        const char *blank = strchr(s->cmd, ' ');
        int len = (int)strlen(s->cmd), want = (int)(t - s->at) / 2;
        if (!shot_prompt) {
            ps(prompt);
            shot_prompt = 1;
        }
        if (want > len)
            want = len;
        while (shot_typed < want) {
            /* the command word green, as vsh shows a command it found */
            int word = !blank || s->cmd + shot_typed < blank;
            if (word)
                fg(2);
            pc(s->cmd[shot_typed]);
            if (word)
                sgr0();
            shot_typed++;
        }
        if (!len || shot_typed < len || t < s->at + len * 2L + 10)
            break;
        ps("\r\n");
        ps(s->out);
        if (s->out[0])
            ps("\r\n");
        shot_n++;
        shot_typed = shot_prompt = 0;
    }
}

static void sc_vsh(long t, int first)
{
    if (first) {
        heading("vsh, the shell", "pipes, $( ) and $(( )), loops, jobs; Tab completes, Ctrl-R searches history");
        ps("\033[4;");
        pn(R);
        pc('r');
        at(4, 1);
        ps("\033[?25h");
    }
    shots(t, first, vsh_shots, (int)(sizeof(vsh_shots) / sizeof(vsh_shots[0])), "\033[36m~\033[0m % ");
}

/* Tab completion: the Unix style, then KingCON's */
static void sc_complete(long t, int first)
{
    static const char *const names[] = { "Network-Startup", "Shell-Startup", "Startup-Sequence",
                                         "User-Startup" };
    static int step;
    int i, x0 = 16;
    if (first) {
        step = 0;
        heading("Tab completion", "Settings > Tab completion: Unix (the default) or KingCON");
        at(4, 2);
        ps("\033[1mUnix\033[0m");
        at(12, 2);
        ps("\033[1mKingCON\033[0m");
        at(5, 2);
        ps("\033[36m~\033[0m % ed S:Sta");
    }
    if (step == 0 && t >= 40) {
        at(5, 2);
        ps("\033[36m~\033[0m % ed S:Startup-Sequence ");
        at(5, 36);
        fg(244);
        ps("Tab: the one name that fits");
        sgr0();
        step++;
    }
    if (step == 1 && t >= 90) {
        at(7, 2);
        ps("\033[36m~\033[0m % ls S:");
        step++;
    }
    if (step == 2 && t >= 130) {
        at(8, 2);
        for (i = 0; i < 4; i++) {
            ps(names[i]);
            ps("  ");
        }
        at(9, 2);
        ps("\033[36m~\033[0m % ls S:");
        at(9, 36);
        fg(244);
        ps("Tab again: the list, then each in turn");
        sgr0();
        step++;
    }
    if (step == 3 && t >= 200) {
        at(13, 2);
        ps("1.SYS:> ed S:");
        step++;
    }
    if (step >= 4 && step < 8 && t >= 240 + (step - 4) * 30L) {
        /* KingCON's Select window, drawn here as text (UP-Term opens a real one) */
        at(14, x0);
        ps("\342\224\214\342\224\200 Select file ");
        for (i = 0; i < 7; i++)
            ps("\342\224\200");
        ps("\342\224\220");
        for (i = 0; i < 4; i++) {
            at(15 + i, x0);
            ps("\342\224\202 ");
            if (i == step - 4)
                ps("\033[7m");
            padded(names[i], 20);
            sgr0();
            ps("\342\224\202");
        }
        at(19, x0);
        ps("\342\224\224");
        for (i = 0; i < 21; i++)
            ps("\342\224\200");
        ps("\342\224\230");
        at(16, x0 + 26);
        fg(244);
        ps("Tab moves, Return takes it");
        sgr0();
        step++;
    }
    if (step == 8 && t >= 380) {
        for (i = 14; i <= 19; i++) { /* the window closes with the choice */
            at(i, x0);
            ps("\033[K");
        }
        at(13, 2);
        ps("1.SYS:> ed S:User-Startup ");
        step++;
    }
}

#include "tour_themes.inc"

/* the kit's themes, one after the other: OSC 4 for the 16 colours, OSC 10
 * and 11 for the default ones (what Settings > Theme... does to a window) */
static void sc_themes(long t, int first)
{
    static int cur;
    int k = (int)(t / 120) % (int)(sizeof(tour_themes) / sizeof(tour_themes[0])), i;
    if (first) {
        cur = -1;
        heading("Themes, switched live", "the 16 colours and the default ones change; what is on screen follows");
        at(5, 4);
        ps("\033[36m~\033[0m % ls --color");
        at(6, 4);
        ps("\033[1;34mDevs\033[0m  \033[1;34mFonts\033[0m  \033[1;32mUPDemo\033[0m  notes.txt  "
           "\033[31mbackup.lha\033[0m  \033[1;36mlink\033[0m");
        at(7, 4);
        ps("\033[36m~\033[0m % make");
        at(8, 4);
        ps("\033[1;31merror:\033[0m \033[1mexpected ';'\033[0m  \033[33mwarning:\033[0m unused  "
           "\033[32mok\033[0m  \033[35mnote\033[0m");
        for (i = 0; i < 16; i++) {
            at(10 + i / 8, 4 + (i % 8) * 6);
            ps("\033[");
            pn(i < 8 ? 40 + i : 100 + i - 8);
            ps(i == 7 || i >= 9 ? ";30m " : "m ");
            pn(i);
            ps(i < 10 ? "   " : "  ");
            sgr0();
        }
        grey_line(13, "Settings > Theme... picks one of the kit's 112 for this window.");
    }
    if (k == cur)
        return;
    cur = k;
    ps("\033]4");
    for (i = 0; i < 16; i++) {
        pc(';');
        pn(i);
        ps(";#");
        ps(tour_themes[k].pal[i]);
    }
    ps("\007\033]10;#");
    ps(tour_themes[k].fg);
    ps("\007\033]11;#");
    ps(tour_themes[k].bg);
    pc(7);
    at(3, 2);
    ps("\033[1m");
    padded(tour_themes[k].name, 24);
    sgr0();
}

static void sc_mouse(long t, int first)
{
    static int seen;
    (void)t;
    if (first) {
        seen = 0;
        ps("\033[?1000h\033[?1006h");
        heading("Mouse reports", "vim, mc and tmux ask for the mouse: each click is sent to them");
        grey_line(4, "Click anywhere with the left button (the right one is the menu's).");
    }
    while (seen < nmev) {
        int x = mev[seen].x, y = mev[seen].y, b = mev[seen].b;
        at(6, 2);
        if (b == 64 || b == 65) {
            ps(b == 64 ? "wheel up" : "wheel down");
        } else {
            ps("button ");
            pn((b & 3) + 1);
            ps(mev[seen].press ? " pressed" : " released");
        }
        ps(" at column ");
        pn(x);
        ps(", row ");
        pn(y);
        ps(": ESC [ < ");
        pn(b);
        ps(" ; ");
        pn(x);
        ps(" ; ");
        pn(y);
        ps(mev[seen].press ? " M" : " m");
        ps("\033[K");
        if (mev[seen].press && y >= 8 && y <= R && x >= 1 && x <= W) {
            at(y, x);
            fg(196 + seen % 6);
            pc('X');
            sgr0();
        }
        seen++;
    }
}

static void sc_links(long t, int first)
{
    static int told;
    if (first) {
        told = 0;
        heading("Hyperlinks and notifications", "OSC 8 makes text a link, OSC 9 tells you something");
        grey_line(4, "Ctrl + click a link to open it (link-open in the profile; OpenURL by default):");
        at(6, 4);
        ps("\033]8;;https://aminet.net/\033\\\033[4;38;5;75mAminet, the Amiga's software archive\033[0m"
           "\033]8;;\033\\");
        at(7, 4);
        ps("\033]8;;https://www.amigaos.net/\033\\\033[4;38;5;75mAmigaOS\033[0m\033]8;;\033\\");
        cfg = cbg = -1;
        grey_line(9, "ls --hyperlink, gcc and delta print their links this way.");
    }
    if (!told && t >= 100) {
        ps("\033]9;UP-Term tour: a notification from a program\007");
        at(11, 2);
        ps("A program's notification (OSC 9: a build finished...) shows in the title bar.");
        told = 1;
    }
}

/* the Amiga's ball as a sixel image: a red and white checked sphere that
 * turns (its checks slide along the lines of longitude) */
#define BALL 96

static void ball_sixel(int phase)
{
    static unsigned char img[BALL][BALL];
    int x, y, c, band, r2 = 44 * 44;
    for (y = 0; y < BALL; y++) {
        int dy = y - BALL / 2, w2 = r2 - dy * dy, half = 0;
        while ((half + 1) * (half + 1) <= w2)
            half++;
        for (x = 0; x < BALL; x++) {
            /* longitude: x over the row's half width (the sphere narrows to
             * its poles); latitude: y */
            int dx = x - BALL / 2, u = (dx * 64 / (half + 1) + 256 + phase) >> 4, v = (dy + 64) >> 3;
            img[y][x] = (unsigned char)(dx * dx + dy * dy >= r2 ? 0 : ((u ^ v) & 1) ? 1 : 2);
        }
    }
    ps("\033P0;1;0q\"1;1;");
    pn(BALL);
    pc(';');
    pn(BALL);
    ps("#1;2;87;0;0#2;2;100;100;100");
    for (band = 0; band < BALL; band += 6)
        for (c = 1; c <= 2; c++) {
            int run = 0, prev = -1;
            pc('#');
            pn(c);
            for (x = 0; x <= BALL; x++) {
                int bits = 0, k;
                if (x < BALL)
                    for (k = 0; k < 6 && band + k < BALL; k++)
                        if (img[band + k][x] == c)
                            bits |= 1 << k;
                if (x < BALL && bits == prev) {
                    run++;
                    continue;
                }
                if (run > 3) { /* a run: ! count sixel */
                    pc('!');
                    pn(run);
                    pc(63 + prev);
                } else {
                    for (; run > 0; run--)
                        pc(63 + prev);
                }
                prev = bits;
                run = 1;
            }
            pc(c == 2 ? '-' : '$');
        }
    ps("\033\\");
}

static void sc_sixel(long t, int first)
{
    static long last;
    if (first) {
        last = -1;
        heading("Sixel images", "pictures in the text: img2sixel, lsix and gnuplot draw this way");
        at(5, 40);
        ps("A picture sits in the text:");
        at(6, 40);
        ps("it scrolls with it, and goes");
        at(7, 40);
        ps("into the scrollback.");
    }
    if (t / 6 == last)
        return;
    last = t / 6;
    at(4, 6);
    ball_sixel((int)last * 4);
}

/* A frame counter in big letters. With mode 2026 the terminal shows each
 * frame whole; every other second the frames go without it, and a slow
 * machine shows them being drawn. */
static void sc_sync(long t, int first)
{
    static long frame;
    char num[6];
    int on = t % 100 < 50, i;
    long f;
    if (first) {
        frame = 0;
        pal_rainbow();
    }
    if (!on)
        ps("\033[?2026l");
    frame++;
    for (f = frame, i = 4; i >= 0; i--, f /= 10)
        num[i] = (char)('0' + f % 10);
    num[5] = 0;
    pix_clear(16);
    big_text(on ? "SYNC ON" : "SYNC OFF", (W - (on ? 41 : 47)) / 2, H / 2 - 18, 231);
    big_text(num, (W - 29) / 2, H / 2 + 2, pal[(int)(t * 4) & 255]);
}

/* DECCOLM: a program switches the window between 80 and 132 columns (mode
 * 40 allowing it). From 80 only: 80 -> 132 -> 80 leaves the window as it
 * was. */
static int colm_on, colm_step;

static void ruler(int row, int cols)
{
    int x;
    at(row, 1);
    fg(75);
    for (x = 1; x <= cols; x++)
        pc(x % 10 ? (x % 5 ? '-' : '+') : '0' + x / 10 % 10);
    sgr0();
}

static void sc_resize(long t, int first)
{
    if (first) {
        colm_step = 0;
        if (W != 80) {
            heading("Resizing", "a program can switch the window between 80 and 132 columns (DECCOLM)");
            at(4, 2);
            ps("This window is ");
            pn(W);
            ps(" columns wide; from 80 the tour shows it.");
            grey_line(6, "View > 80 x 24 and View > 132 x 43 size the window from the menu.");
            colm_step = 9;
            return;
        }
        ps("\033[?40h\033[?3h"); /* the screen clears, as a VT100's does */
        colm_on = 1;
        colm_step = 1;
        return;
    }
    if (colm_step == 1 && t >= 75) {
        want_size = 1; /* the window has had time to grow */
        colm_step = 2;
    } else if (colm_step == 2) {
        heading("Resizing", "the program sent ESC [ ? 3 h (DECCOLM): 132 columns");
        at(4, 2);
        if (ev_cols > 80) {
            ps("The window is now ");
            pn(ev_cols);
            ps(" columns wide:");
            ruler(5, ev_cols);
        } else {
            ps("This screen has no room for 132 columns: the window stayed.");
        }
        colm_step = 3;
    } else if (colm_step == 3 && t >= 250) {
        ps("\033[?3l");
        colm_on = 0;
        colm_step = 4;
    } else if (colm_step == 4 && t >= 300) {
        heading("Resizing", "ESC [ ? 3 l: back to 80 columns");
        ruler(4, 80);
        colm_step = 5;
    }
}

/* Reflow, on the main screen: a paragraph, and the window's size in the
 * title while the user drags its size gadget */
static void sc_reflow(long t, int first)
{
    static const char para[] =
        "UP-Term keeps a line that wrapped as one line. Drag the window's size gadget now: make it "
        "narrower, then wider. This paragraph wraps again to fit, as the text in a word processor "
        "does, instead of being cut at the edge. The tour runs on the alternate screen, as vim and "
        "less do; this is the main screen, where a shell's text stays. reflow = off in the profile "
        "keeps the old way.";
    static long last;
    int i;
    if (first) {
        last = -1;
        /* what was on the main screen goes into the scrollback, not away */
        at(R + 1, 1);
        for (i = 0; i <= R; i++)
            pc('\n');
        at(1, 1);
        ps("\033[1mReflow on resize\033[0m\r\n\r\n");
        ps(para);
        ps("\r\n\r\n");
        fg(244);
        ps("(the size is in the title bar; any key ends the tour)");
        sgr0();
        ps("\r\n");
    }
    if (t / 50 != last) {
        last = t / 50;
        want_size = 1;
    }
}

/* ---- the run ------------------------------------------------------------------ */

typedef struct scene_def {
    const char *name;
    void (*fn)(long t, int first);
    int pixels;          /* draws into the picture: blit() after it */
    long ticks;
    const char *caption; /* the tour's status line */
    int main_screen;     /* the tour draws it on the main screen, with no status line */
} scene_def;

static const scene_def show[] = {
    { "text", sc_text, 0, 500, 0, 0 },
    { "colours", sc_colours, 0, 450, 0, 0 },
    { "scroll regions", sc_region, 0, 400, 0, 0 },
    { "copper bars", sc_bars, 1, 500, 0, 0 },
    { "plasma", sc_plasma, 1, 500, 0, 0 },
    { "fire", sc_fire, 1, 500, 0, 0 },
    { "rotozoomer", sc_roto, 1, 500, 0, 0 },
    { "vector cubes", sc_cube, 0, 500, 0, 0 },
    { "sine scroller", sc_scroll, 1, 900, 0, 0 },
    { "palette cycling", sc_cycle, 1, 500, 0, 0 },
    { "the end", sc_end, 0, 400, 0, 0 }
};

static const scene_def tour[] = {
    { "text", sc_text, 0, 450, "Text styles, double-size lines, box drawing", 0 },
    { "styles", sc_styles, 0, 400, "Every text style a program can ask for", 0 },
    { "colours", sc_colours, 0, 400, "16, 256 and 24-bit colours", 0 },
    { "character sets", sc_charsets, 0, 400, "DEC line graphics, Latin-1 and UTF-8", 0 },
    { "unicode", sc_unicode, 0, 450, "Unicode: scripts, wide characters, accents", 0 },
    { "scroll regions", sc_region, 0, 350, "Scroll regions: the header stays put", 0 },
    { "tmux", sc_tmux, 0, 450, "tmux and screen: each pane a scroll region", 0 },
    { "vsh", sc_vsh, 0, 500, "vsh, the shell of UP-Term windows", 0 },
    { "completion", sc_complete, 0, 450, "Tab completion, Unix style and KingCON style", 0 },
    { "themes", sc_themes, 0, 480, "Themes switched live (OSC 4, 10, 11)", 0 },
    { "mouse", sc_mouse, 0, 600, "Mouse reports: click in the window", 0 },
    { "links", sc_links, 0, 400, "Hyperlinks (OSC 8) and notifications (OSC 9)", 0 },
    { "sixel", sc_sixel, 0, 450, "Sixel images: pictures in the text", 0 },
    { "synchronized output", sc_sync, 1, 400, "Synchronized output: frames shown whole", 0 },
    { "resize", sc_resize, 0, 400, "A program resizes the window (DECCOLM)", 0 },
    { "reflow", sc_reflow, 0, 750, "Reflow on resize", 1 },
    { "plasma", sc_plasma, 1, 350, "The demoscene part: a plasma in 256 colours", 0 },
    { "vector cubes", sc_cube, 0, 350, "Vector cubes in quadrant blocks", 0 },
    { "sine scroller", sc_scroll, 1, 500, "A sine scroller over a star field", 0 },
    { "the end", sc_end, 0, 350, "The end", 0 }
};

#define SHOW_N ((int)(sizeof(show) / sizeof(show[0])))
#define TOUR_N ((int)(sizeof(tour) / sizeof(tour[0])))
#define STAT_N 32

static long stat_frames[STAT_N], stat_ticks[STAT_N];
static const scene_def *played = show; /* the scenes updemo_stats speaks of */
static int played_n = SHOW_N;

int updemo_scenes(void)
{
    return SHOW_N;
}

const char *updemo_scene_name(int s)
{
    return s >= 0 && s < SHOW_N ? show[s].name : "";
}

int updemo_tour_scenes(void)
{
    return TOUR_N;
}

const char *updemo_tour_scene_name(int s)
{
    return s >= 0 && s < TOUR_N ? tour[s].name : "";
}

void updemo_stats(int s, long *frames, long *ticks)
{
    *frames = s >= 0 && s < played_n ? stat_frames[s] : 0;
    *ticks = s >= 0 && s < played_n ? stat_ticks[s] : 0;
}

static void status(int s, long fps)
{
    if (touring) {
        const char *cap = played[s].caption;
        int n = 6 + digits(s + 1) + 1 + digits(played_n) + 2 + (int)strlen(cap);
        ps("\0337"); /* a scene may be typing: its cursor stays where it is */
        at(R + 1, 1);
        ps("\033[0;30;47m Tour ");
        pn(s + 1);
        pc('/');
        pn(played_n);
        ps("  ");
        ps(cap);
        if (W - n >= 24) {
            for (; n < W - 22; n++)
                pc(' ');
            ps("Any key ends the tour");
        }
        ps("\033[K\0338");
        cfg = cbg = -1;
        return;
    }
    at(R + 1, 1);
    ps("\033[0;30;47m");
    cfg = cbg = -1;
    ps(" UP-Term demo  ");
    pn(s + 1);
    pc('/');
    pn(played_n);
    ps("  ");
    ps(played[s].name);
    ps("   ");
    pn(fps);
    ps(" fps   Space next  B back  Q quit\033[K");
    sgr0();
}

/* a key's meaning in the show: 0 nothing, 1 next, 2 back, 3 quit. A
 * function key's sequence (ESC [ ... or CSI ...) is read to its end and
 * means nothing. */
static int key_action(int k)
{
    int n;
    if (k < 0)
        return 0;
    if (k == 27) {
        k = io->key(io->user, 40);
        if (k < 0)
            return 3; /* Esc alone */
        if (k != '[' && k != 'O')
            return 0;
        k = 0x9B;
    }
    if (k == 0x9B) {
        for (n = 0; n < 16; n++) {
            k = io->key(io->user, 40);
            if (k < 0 || (k >= 0x40 && k <= 0x7E))
                break;
        }
        return 0;
    }
    if (k == 'q' || k == 'Q' || k == 3)
        return 3;
    if (k == 'b' || k == 'B' || k == 8 || k == 127)
        return 2;
    if (k == ' ' || k == '\r' || k == '\n' || k == 'n' || k == 'N')
        return 1;
    return 0;
}

/* the reflow scene's title: the window's size */
static void size_title(void)
{
    ps("\033]2;UP-Term tour: ");
    pn(ev_cols);
    ps(" x ");
    pn(ev_rows);
    pc(7);
    flush();
}

/* The show or the tour (played), from scene `first`. The tour also asks
 * the window's size before each scene (it follows a resize), draws some
 * scenes on the main screen, and puts back what a scene changed. */
static int play(int first, long scene_ticks)
{
    int s, act = 0, alt = 1;
    numbers_init();
    memset(stat_frames, 0, sizeof(stat_frames));
    memset(stat_ticks, 0, sizeof(stat_ticks));
    quit_req = want_size = nmev = colm_on = 0;
    ps("\033[?1049h\033[?25l");
    if (touring)
        ps("\033[22;0t\033]2;UP-Term tour\007"); /* the title, pushed to be put back */
    for (s = first < 0 || first >= played_n ? 0 : first; s >= 0 && s < played_n && act != 3;
         s += act == 2 ? -1 : 1) {
        const scene_def *sc = &played[s];
        long t0, dur = scene_ticks ? scene_ticks : sc->ticks;
        long t, mark = 0, frames = 0, fps = 0;
        int fresh = 1;
        if (touring) {
            if (sc->main_screen == alt) {
                ps(alt ? "\033[?1049l" : "\033[?1049h");
                alt = !alt;
            }
            /* the window may have changed: the tour follows it */
            if (ask_size() && !set_size(ev_cols, ev_rows))
                break;
            if (quit_req)
                break;
            nmev = 0;
            ps("\033[?25l");
        }
        t0 = io->ticks(io->user);
        ps("\033[r");
        sgr0();
        if (!sc->main_screen)
            ps("\033[2J");
        old_ok = 0;
        act = 0;
        while ((t = io->ticks(io->user) - t0) < dur) {
            long spent;
            ps("\033[?2026h"); /* a frame: shown whole (synchronized output), not as its writes arrive */
            sc->fn(t, fresh);
            if (sc->pixels)
                blit();
            frames++;
            stat_frames[s]++;
            if (fresh || t - mark >= 50) {
                if (t > mark)
                    fps = frames * 50 / (t - mark);
                if (!fresh) {
                    frames = 0;
                    mark = t;
                }
                if (!sc->main_screen)
                    status(s, fps);
            }
            fresh = 0;
            ps("\033[?2026l");
            flush();
            if (want_size) {
                want_size = 0;
                if (ask_size() && sc->main_screen)
                    size_title();
                if (quit_req) {
                    act = 3;
                    break;
                }
            }
            /* 25 frames a second at most: the rest of the two ticks waits for a key */
            spent = io->ticks(io->user) - t0 - t;
            if (touring)
                act = read_event(spent >= 2 ? 0 : (int)(2 - spent) * 20) == EV_KEY ? 3 : 0;
            else
                act = key_action(io->key(io->user, spent >= 2 ? 0 : (int)(2 - spent) * 20));
            if (act)
                break;
        }
        flush();
        stat_ticks[s] += io->ticks(io->user) - t0;
        ps("\033[r\033]104\007"); /* the whole screen scrolls again; the palette is the terminal's */
        if (touring) {
            /* what a scene may have changed: the width, the mouse, the default
             * colours, the line graphics, the title */
            if (colm_on)
                ps("\033[?3l");
            colm_on = 0;
            ps("\033[?1000l\033[?1006l\033[?40l\033]110\007\033]111\007\033(B\033]2;UP-Term tour\007");
        }
        if (act == 2 && s == 0) {
            act = 0; /* back from the first scene: the first again */
            s = -1;
        }
    }
    sgr0();
    if (touring)
        ps("\033[23;0t"); /* the title as it was */
    ps("\033[?25h");
    if (alt)
        ps("\033[?1049l");
    flush();
    return 0;
}

static int begin(const updemo_io *i, int cols, int rows, int tour_on)
{
    if (cols < UPDEMO_MIN_COLS || rows < UPDEMO_MIN_ROWS)
        return 0;
    io = i;
    on = 0;
    touring = tour_on;
    played = tour_on ? tour : show;
    played_n = tour_on ? TOUR_N : SHOW_N;
    return set_size(cols, rows);
}

int updemo_run(const updemo_io *i, int cols, int rows, int first, long scene_ticks)
{
    return begin(i, cols, rows, 0) ? play(first, scene_ticks) : 1;
}

int updemo_tour(const updemo_io *i, int cols, int rows, int first, long scene_ticks)
{
    return begin(i, cols, rows, 1) ? play(first, scene_ticks) : 1;
}
