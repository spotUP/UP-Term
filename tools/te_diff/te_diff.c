/* te_diff: DCTelnet's terminal engine (retro32-term term-engine.c) and
 * vtcon's pcansi personality, fed the same BBS art, compared pixel for
 * pixel.
 *
 * term-engine draws straight into four bitplanes; here those planes are
 * host memory (te_shim.h). vtcon's cell grid is drawn into a second set
 * of planes by the same rule term-engine uses for a glyph (fg where the
 * glyph has a 1, bg where it has a 0, row 7 in fg when underlined), and
 * the two are compared. The font is synthetic: every code has its own
 * bitmap (the space is empty, as in a real font), so a differing cell
 * decodes back to code, fg and bg on both sides.
 *
 * Usage: te_diff [-r rows] [-v] [-f] file.ans ...
 * -f feeds both a byte at a time and names the first byte after which
 * the screens differ.
 * A file is cut at its first ^Z (the SAUCE record follows it). Exit 0 when
 * every file renders identically. */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "te_shim.h"

static void te_reply(const UBYTE *b, int n) { (void)b; (void)n; }
#define send_data(data, len) te_reply((data), (len))
#include "term-engine.c"

#include "../../engine/vtengine.h"

#define W 640
#define BPR (W / 8)
#define MAXROWS 64

static UBYTE font_data[8 * 256];
static ULONG font_loc[256];

static UBYTE glyph_row(int c, int r)
{
    if (c == 0x20)
        return 0;
    switch (r) {
    case 0: return 0x80;             /* pixel 0 is fg, pixel 1 is bg */
    case 1: return (UBYTE)c;
    case 2: return (UBYTE)(c ^ 0x5A);
    default: return 0x0F;
    }
}

static void make_font(struct TextFont *tf)
{
    int c, r;
    for (c = 0; c < 256; c++) {
        for (r = 0; r < 8; r++)
            font_data[r * 256 + c] = glyph_row(c, r);
        font_loc[c] = (ULONG)(c * 8) << 16 | 8;
    }
    memset(tf, 0, sizeof(*tf));
    tf->tf_YSize = 8;
    tf->tf_XSize = 8;
    tf->tf_Baseline = 6;
    tf->tf_Modulo = 256;
    tf->tf_LoChar = 0;
    tf->tf_HiChar = 255;
    tf->tf_CharData = font_data;
    tf->tf_CharLoc = font_loc;
}

static int pen_at(UBYTE *const *planes, int x, int y)
{
    int p, pen = 0;
    for (p = 0; p < 4; p++)
        if (planes[p][y * BPR + (x >> 3)] & (0x80 >> (x & 7)))
            pen |= 1 << p;
    return pen;
}

/* One cell as text: "blank/pen" or "code fg/bg" (+u underlined). */
static void describe(UBYTE *const *planes, int col, int row, char *out)
{
    int x0 = col * 8, y0 = row * 8, x, y, first = pen_at(planes, x0, y0), solid = 1;
    int fg, bg, code = 0, ul;
    for (y = 0; y < 8 && solid; y++)
        for (x = 0; x < 8; x++)
            if (pen_at(planes, x0 + x, y0 + y) != first) {
                solid = 0;
                break;
            }
    if (solid) {
        sprintf(out, "blank/%d", first);
        return;
    }
    fg = pen_at(planes, x0, y0);
    bg = pen_at(planes, x0 + 1, y0);
    if (fg == bg) {
        /* the space glyph over an underline, or a glyph with fg == bg */
        bg = pen_at(planes, x0, y0 + 1 < y0 + 7 ? y0 + 1 : y0);
        sprintf(out, "?(%d/%d)", fg, bg);
        return;
    }
    for (x = 0; x < 8; x++)
        if (pen_at(planes, x0 + x, y0 + 1) == fg)
            code |= 0x80 >> x;
    ul = 1;
    for (x = 0; x < 8; x++)
        if (pen_at(planes, x0 + x, y0 + 7) != fg)
            ul = 0;
    sprintf(out, "%02X %d/%d%s", code, fg, bg, ul ? "+u" : "");
}

/* vtcon's cell code point back to the CP437 byte term-engine would draw. */
static int to_cp437(vt_u16 ch)
{
    const vt_u16 *tab = vt_cp437_table();
    int i;
    if (ch >= 0x20 && ch < 0x7F)
        return ch;
    for (i = 0; i < 128; i++)
        if (tab[i] == ch)
            return 0x80 + i;
    if (ch == 0x2302)
        return 0x7F;
    return -1;
}

static void draw_cell(UBYTE *const *planes, int col, int row, int code, int fg, int bg, int ul)
{
    int p, r;
    for (p = 0; p < 4; p++) {
        int f = (fg >> p) & 1, b = (bg >> p) & 1;
        UBYTE *dst = planes[p] + (row * 8) * BPR + col;
        for (r = 0; r < 8; r++) {
            UBYTE g = glyph_row(code, r);
            UBYTE v = f == b ? (f ? 0xFF : 0) : f ? g : (UBYTE)~g;
            if (r == 7 && ul)
                v = f ? 0xFF : 0;
            dst[r * BPR] = v;
        }
    }
}

static int draw_vt(vt_term *t, UBYTE **vt_planes, UBYTE (*mem)[BPR * MAXROWS * 8], int rows)
{
    int p, x, y, unmapped = 0;
    for (p = 0; p < 4; p++) {
        memset(mem[p], 0, BPR * MAXROWS * 8);
        vt_planes[p] = mem[p];
    }
    for (y = 0; y < rows; y++) {
        int nc;
        const vt_cell *row = vt_row(t, y, &nc);
        for (x = 0; x < 80 && x < nc; x++) {
            vt_color fg, bg;
            int code = to_cp437(row[x].ch);
            vt_resolve_colors(t, &row[x], &fg, &bg);
            if (code < 0) {
                unmapped++;
                code = 0xFE;
            }
            draw_cell(vt_planes, x, y, code, fg & 15, bg & 15,
                      (row[x].attr & VT_ATTR_UNDERLINE) != 0);
        }
    }
    return unmapped;
}

static int first_div = 0;
static double time_te, time_vt; /* -t: seconds in each engine */ /* -f: find the first byte after which the screens differ */

static void null_damage(void *u, int x, int y, int w, int h) { (void)u; (void)x; (void)y; (void)w; (void)h; }

static int run_file(const char *path, int rows, int verbose, long *cells_bad)
{
    static UBYTE mem_te[4][BPR * MAXROWS * 8], mem_vt[4][BPR * MAXROWS * 8];
    UBYTE *vt_planes[4];
    struct BitMap bm;
    struct Screen scr;
    struct TextFont tf;
    vt_callbacks cb;
    vt_term *t;
    FILE *f = fopen(path, "rb");
    unsigned char *buf;
    long n, i;
    int p, x, y, bad = 0, shown = 0, unmapped = 0;

    if (!f) {
        perror(path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (unsigned char *)malloc(n ? n : 1);
    n = (long)fread(buf, 1, n, f);
    fclose(f);
    for (i = 0; i < n; i++)
        if (buf[i] == 0x1A) {
            n = i;
            break;
        }

    /* term-engine */
    memset(&bm, 0, sizeof(bm));
    bm.BytesPerRow = BPR;
    bm.Rows = (UWORD)(rows * 8);
    bm.Depth = 4;
    for (p = 0; p < 4; p++) {
        memset(mem_te[p], 0, sizeof(mem_te[p]));
        bm.Planes[p] = mem_te[p];
    }
    memset(&scr, 0, sizeof(scr));
    scr.RastPort.BitMap = &bm;
    scr.Width = W;
    scr.Height = (WORD)(rows * 8);
    make_font(&tf);
    term_rp = NULL;
    term_win_rp = NULL;
    p_state = 0;
    if (term_init(&scr, &tf)) {
        fprintf(stderr, "term_init failed\n");
        return -1;
    }
    if (!first_div) {
        clock_t c0 = clock();
        for (i = 0; i < n; i++)
            term_feed(buf[i]);
        term_flush();
        time_te += (double)(clock() - c0) / CLOCKS_PER_SEC;
    }

    /* vtcon */
    memset(&cb, 0, sizeof(cb));
    cb.damage = null_damage;
    t = vt_new(80, rows, 0, &cb, NULL);
    vt_set_personality(t, VT_PCANSI);
    vt_set_charset(t, VT_CS_CP437);
    if (first_div) {
        for (i = 0; i < n; i++) {
            term_feed(buf[i]);
            vt_write(t, buf + i, 1);
            draw_vt(t, vt_planes, mem_vt, rows);
            for (p = 0; p < 4; p++)
                if (memcmp(mem_te[p], mem_vt[p], BPR * rows * 8))
                    break;
            if (p < 4) {
                long k, from = i > 40 ? i - 40 : 0;
                int cx, cy;
                vt_cursor(t, &cx, &cy);
                printf("%s: first difference after byte %ld; term-engine cursor %d,%d, vtcon %d,%d\n    ",
                       path, i, cur_y, cur_x, cy, cx);
                for (k = from; k <= i; k++)
                    printf(buf[k] >= 0x20 && buf[k] < 0x7F ? "%c" : "<%02X>", buf[k]);
                printf("\n");
                break;
            }
        }
    } else {
        clock_t c0 = clock();
        vt_write(t, buf, n);
        time_vt += (double)(clock() - c0) / CLOCKS_PER_SEC;
    }
    unmapped = draw_vt(t, vt_planes, mem_vt, rows);
    for (y = 0; y < rows; y++)
        for (x = 0; x < 80; x++) {
            int same = 1, r;
            for (p = 0; p < 4 && same; p++)
                for (r = 0; r < 8; r++)
                    if (mem_te[p][(y * 8 + r) * BPR + x] != mem_vt[p][(y * 8 + r) * BPR + x]) {
                        same = 0;
                        break;
                    }
            if (!same) {
                bad++;
                if (verbose && shown < 12) {
                    char a[32], b[32];
                    describe(term_plane, x, y, a);
                    describe(vt_planes, x, y, b);
                    printf("    row %2d col %2d  term-engine %-12s vtcon %s\n", y, x, a, b);
                    shown++;
                }
            }
        }
    printf("%-60s %5d cells differ%s\n", path, bad, unmapped ? " (vtcon had unmapped code points)" : "");
    *cells_bad += bad;
    vt_free(t);
    free(buf);
    return bad;
}

int main(int argc, char **argv)
{
    int rows = 25, verbose = 0, a, files = 0, differ = 0;
    long cells = 0;
    for (a = 1; a < argc; a++) {
        if (!strcmp(argv[a], "-r") && a + 1 < argc) {
            rows = atoi(argv[++a]);
            if (rows < 1 || rows > MAXROWS)
                rows = 25;
        } else if (!strcmp(argv[a], "-f")) {
            first_div = 1;
        } else if (!strcmp(argv[a], "-v")) {
            verbose = 1;
        } else {
            int r = run_file(argv[a], rows, verbose, &cells);
            files++;
            if (r)
                differ++;
        }
    }
    printf("%d files, %d differ, %ld cells differ\n", files, differ, cells);
    printf("parse+draw: term-engine %.3f s; vtcon parse+model (no drawing) %.3f s\n", time_te, time_vt);
    return differ != 0;
}
