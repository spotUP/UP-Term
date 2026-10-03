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
            centred(18, "next: colours, then the demoscene part");
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
    static const char *const lines[] = {
        "UP-Term",
        "",
        "an xterm on the Amiga, by Up Rough",
        "",
        "everything you saw was plain text:",
        "escape sequences, UTF-8, 256 and 24-bit colours",
        "",
        "your shell is behind this screen, as you left it"
    };
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

/* ---- the run ------------------------------------------------------------------ */

static const struct {
    const char *name;
    void (*fn)(long t, int first);
    int pixels; /* draws into the picture: blit() after it */
    long ticks;
} scene[] = {
    { "text", sc_text, 0, 500 },
    { "colours", sc_colours, 0, 450 },
    { "scroll regions", sc_region, 0, 400 },
    { "copper bars", sc_bars, 1, 500 },
    { "plasma", sc_plasma, 1, 500 },
    { "fire", sc_fire, 1, 500 },
    { "rotozoomer", sc_roto, 1, 500 },
    { "vector cubes", sc_cube, 0, 500 },
    { "sine scroller", sc_scroll, 1, 900 },
    { "palette cycling", sc_cycle, 1, 500 },
    { "the end", sc_end, 0, 400 }
};
#define SCENES ((int)(sizeof(scene) / sizeof(scene[0])))

static long stat_frames[sizeof(scene) / sizeof(scene[0])], stat_ticks[sizeof(scene) / sizeof(scene[0])];

int updemo_scenes(void)
{
    return SCENES;
}

const char *updemo_scene_name(int s)
{
    return s >= 0 && s < SCENES ? scene[s].name : "";
}

void updemo_stats(int s, long *frames, long *ticks)
{
    *frames = s >= 0 && s < SCENES ? stat_frames[s] : 0;
    *ticks = s >= 0 && s < SCENES ? stat_ticks[s] : 0;
}

static void status(int s, long fps)
{
    at(R + 1, 1);
    ps("\033[0;30;47m");
    cfg = cbg = -1;
    ps(" UP-Term demo  ");
    pn(s + 1);
    pc('/');
    pn(SCENES);
    ps("  ");
    ps(scene[s].name);
    ps("   ");
    pn(fps);
    ps(" fps   Space next  B back  Q quit\033[K");
    sgr0();
}

/* a key's meaning: 0 nothing, 1 next, 2 back, 3 quit. A function key's
 * sequence (ESC [ ... or CSI ...) is read to its end and means nothing. */
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

int updemo_run(const updemo_io *i, int cols, int rows, int first, long scene_ticks)
{
    int s, act = 0;
    if (cols < UPDEMO_MIN_COLS || rows < UPDEMO_MIN_ROWS)
        return 1;
    io = i;
    on = 0;
    W = cols > MAXC ? MAXC : cols;
    R = (rows > MAXR ? MAXR : rows) - 1;
    H = 2 * R;
    numbers_init();
    memset(stat_frames, 0, sizeof(stat_frames));
    memset(stat_ticks, 0, sizeof(stat_ticks));
    ps("\033[?1049h\033[?25l");
    for (s = first < 0 || first >= SCENES ? 0 : first; s >= 0 && s < SCENES && act != 3; s += act == 2 ? -1 : 1) {
        long t0 = io->ticks(io->user), dur = scene_ticks ? scene_ticks : scene[s].ticks;
        long t, mark = 0, frames = 0, fps = 0;
        int fresh = 1;
        ps("\033[r");
        sgr0();
        ps("\033[2J");
        old_ok = 0;
        act = 0;
        while ((t = io->ticks(io->user) - t0) < dur) {
            long spent;
            scene[s].fn(t, fresh);
            if (scene[s].pixels)
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
                status(s, fps);
            }
            fresh = 0;
            flush();
            /* 25 frames a second at most: the rest of the two ticks waits for a key */
            spent = io->ticks(io->user) - t0 - t;
            act = key_action(io->key(io->user, spent >= 2 ? 0 : (int)(2 - spent) * 20));
            if (act)
                break;
        }
        flush();
        stat_ticks[s] += io->ticks(io->user) - t0;
        ps("\033[r\033]104\007"); /* the whole screen scrolls again; the palette is the terminal's */
        if (act == 2 && s == 0) {
            act = 0; /* back from the first scene: the first again */
            s = -1;
        }
    }
    sgr0();
    ps("\033[?25h\033[?1049l");
    flush();
    return 0;
}
