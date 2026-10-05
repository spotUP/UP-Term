/* render/unifont and the fallback chain (ledger U2): the page files
 * tools/gen_unifont.py writes (tests/unifont/golden, checked against the
 * converter by tests/test_gen_unifont.py), the masks at 16 and 8 pixels,
 * the LRU cache, the order of the glyph sources, and the path from text
 * written to the terminal to a page read. */
#include "harness.h"
#include <stdio.h>
#include <stdlib.h>
#include "../render/glyphmap.h"
#include "../render/unifont.h"

/* ---- loaders ---------------------------------------------------------------- */

typedef struct fileload {
    int loads, releases;
    int calls[UF_PAGES]; /* per page */
} fileload;

/* The golden pages, the way the Amiga reads SYS:UP-Term/unifont/XX (or 1XX). */
static long golden_load(void *u, int page, vt_u8 **buf)
{
    fileload *l = (fileload *)u;
    char path[64], name[4];
    FILE *f;
    long n;
    l->loads++;
    l->calls[page]++;
    uf_page_name(page, name);
    strcpy(path, "tests/unifont/golden/");
    strcat(path, name);
    if (!(f = fopen(path, "rb")))
        return -1;
    *buf = (vt_u8 *)malloc(UF_PAGE_MAX);
    n = (long)fread(*buf, 1, UF_PAGE_MAX, f);
    fclose(f);
    return n;
}

static void golden_release(void *u, vt_u8 *buf)
{
    ((fileload *)u)->releases++;
    free(buf);
}

static long read_golden(const char *name, vt_u8 *buf)
{
    char path[64];
    FILE *f;
    long n;
    strcpy(path, "tests/unifont/golden/");
    strcat(path, name);
    if (!(f = fopen(path, "rb")))
        return -1;
    n = (long)fread(buf, 1, UF_PAGE_MAX, f);
    fclose(f);
    return n;
}

/* A page of one narrow glyph (lo 0x41, a vertical bar), for any page
 * number: what the cache tests fill eight slots with. */
static long one_glyph_page(int page, vt_u8 **buf)
{
    vt_u8 *p = (vt_u8 *)calloc(1, UF_HEADER + 16);
    int i;
    p[0] = 'U';
    p[1] = 'F';
    p[2] = 'P';
    p[3] = '2';
    p[4] = (vt_u8)(page >> 8);
    p[5] = (vt_u8)page;
    p[7] = 1;
    p[8 + (0x41 >> 3)] = (vt_u8)(0x80 >> (0x41 & 7));
    for (i = 0; i < 8; i++) {
        int off = UF_HEADER + (i > 2 ? 16 : 0); /* groups after 0x41's (group 2) start past it */
        p[72 + 2 * i] = (vt_u8)(off >> 8);
        p[73 + 2 * i] = (vt_u8)off;
    }
    for (i = 0; i < 16; i++)
        p[UF_HEADER + i] = 0x10;
    *buf = p;
    return UF_HEADER + 16;
}

typedef struct fakeload {
    int loads, releases, answer; /* answer: 0 a page, else what to return */
} fakeload;

static long fake_load(void *u, int page, vt_u8 **buf)
{
    fakeload *l = (fakeload *)u;
    l->loads++;
    if (l->answer)
        return l->answer;
    return one_glyph_page(page, buf);
}

static void fake_release(void *u, vt_u8 *buf)
{
    ((fakeload *)u)->releases++;
    free(buf);
}

/* ---- page files --------------------------------------------------------------- */

static const vt_u8 zhe[16] = { 0, 0, 0, 0, 0x49, 0x49, 0x2A, 0x2A, 0x1C, 0x1C, 0x2A, 0x2A, 0x49, 0x49, 0, 0 };

static void page_file_gives_the_hex_rows(void)
{
    static vt_u8 p[UF_PAGE_MAX];
    long n = read_golden("04", p);
    int wide;
    const vt_u8 *g;
    CHECK(uf_page_check(p, n, 0x04));
    g = uf_page_glyph(p, 0x16, &wide); /* U+0416 */
    CHECK(g != 0);
    CHECK_INT(wide, 0);
    CHECK(g && !memcmp(g, zhe, 16));
    CHECK(uf_page_glyph(p, 0x17, &wide) == 0);
    n = read_golden("4E", p);
    g = uf_page_glyph(p, 0x2D, &wide); /* U+4E2D, after U+4E00 */
    CHECK(g && wide && g[6] == 0x3F && g[7] == 0xFE);
}

static void bad_page_files_are_refused(void)
{
    static vt_u8 p[UF_PAGE_MAX];
    long n = read_golden("FF", p);
    CHECK(uf_page_check(p, n, 0xFF));
    CHECK(!uf_page_check(p, n, 0xFE));  /* another page's file */
    CHECK(!uf_page_check(p, n - 1, 0xFF)); /* cut short */
    CHECK(!uf_page_check(p, 40, 0xFF));
    CHECK(!uf_page_check(p, n, 0x1FF)); /* the same low byte in plane 1 */
    p[3] = '1'; /* the old one-byte-page format: refused, not misread */
    CHECK(!uf_page_check(p, n, 0xFF));
}

static void page_files_are_named_by_plane(void)
{
    char name[4];
    uf_page_name(0x4E, name);
    CHECK_STR(name, "4E");
    uf_page_name(0x04, name);
    CHECK_STR(name, "04");
    uf_page_name(0x1F6, name);
    CHECK_STR(name, "1F6");
}

/* ---- masks -------------------------------------------------------------------- */

static void sixteen_pixel_font_draws_one_to_one(void)
{
    vt_u8 m[UF_MASK_MAX];
    int y;
    CHECK(uf_make_mask(zhe, 0, 8, 16, 1, m, 2, sizeof(m)));
    for (y = 0; y < 16; y++) {
        CHECK_INT(m[y * 2], zhe[y]);
        CHECK_INT(m[y * 2 + 1], 0);
    }
}

/* W33: the TopazPro 16 window (tf_Baseline 6, topaz 8's) drew Unifont's
 * glyphs raised 7 rows, their top half cut off -- "half height". Through
 * the cache as the window has it: every one of the 16 rows in its row. */
static void topazpro_window_draws_unifont_glyph_whole_not_raised(void)
{
    vt_u8 m[UF_MASK_MAX];
    uf_cache c;
    fileload l;
    const vt_u8 *g;
    int bpr = 0, y, same = 1;
    memset(&l, 0, sizeof(l));
    uf_init(&c, golden_load, golden_release, &l, m, sizeof(m));
    uf_set_cell(&c, 8, 16); /* TopazPro 16 */
    g = uf_glyph(&c, 0x0416, 1, &bpr);
    CHECK(g != 0);
    CHECK_INT(bpr, 2);
    for (y = 0; g && y < 16; y++)
        same &= g[y * 2] == zhe[y];
    CHECK(same);
    CHECK(g && g[4 * 2] == 0x49 && g[13 * 2] == 0x49); /* its top and bottom stroke rows */
    uf_flush(&c);
}

static void eight_pixel_font_ors_pairs_of_rows(void)
{
    vt_u8 m[UF_MASK_MAX];
    int y;
    CHECK(uf_make_mask(zhe, 0, 8, 8, 1, m, 2, sizeof(m))); /* topaz 8 */
    for (y = 0; y < 8; y++)
        CHECK_INT(m[y * 2], zhe[2 * y] | zhe[2 * y + 1]);
}

static void wide_glyph_spans_two_cells_or_squeezes_into_one(void)
{
    static vt_u8 p[UF_PAGE_MAX];
    vt_u8 m[UF_MASK_MAX];
    int wide;
    const vt_u8 *g;
    read_golden("4E", p);
    g = uf_page_glyph(p, 0x2D, &wide); /* 中: row 3 is 0x3FFE */
    CHECK(uf_make_mask(g, 1, 8, 16, 2, m, 2, sizeof(m)));
    CHECK_INT(m[3 * 2], 0x3F);
    CHECK_INT(m[3 * 2 + 1], 0xFE);
    /* in one 8-pixel cell: pixel pairs OR'd, 0011 1111 1111 1110 -> 0111 1111 */
    CHECK(uf_make_mask(g, 1, 8, 16, 1, m, 2, sizeof(m)));
    CHECK_INT(m[3 * 2], 0x7F);
    CHECK_INT(m[0], 0x08); /* 0000 0000 1000 0000 -> 0000 1000 */
}

static void other_font_heights_get_no_glyph(void)
{
    vt_u8 m[UF_MASK_MAX];
    uf_cache c;
    fileload l;
    int bpr;
    memset(&l, 0, sizeof(l));
    CHECK(!uf_make_mask(zhe, 0, 8, 11, 1, m, 2, sizeof(m)));
    CHECK(!uf_make_mask(zhe, 0, 8, 16, 1, m, 2, 16)); /* the buffer is too small */
    uf_init(&c, golden_load, golden_release, &l, m, sizeof(m));
    uf_set_cell(&c, 8, 11);
    CHECK(uf_glyph(&c, 0x0416, 1, &bpr) == 0);
    CHECK_INT(l.loads, 0); /* nothing read for a cell it cannot draw */
    uf_flush(&c);
}

/* ---- the cache ------------------------------------------------------------------ */

static void least_recently_used_page_goes_first(void)
{
    vt_u8 m[UF_MASK_MAX];
    uf_cache c;
    fakeload l;
    int bpr, page;
    memset(&l, 0, sizeof(l));
    uf_init(&c, fake_load, fake_release, &l, m, sizeof(m));
    uf_set_cell(&c, 8, 16);
    for (page = 0x30; page < 0x30 + UF_SLOTS; page++)
        CHECK(uf_glyph(&c, (vt_u32)(page << 8 | 0x41), 1, &bpr) != 0);
    CHECK_INT(l.loads, UF_SLOTS);
    CHECK(uf_glyph(&c, 0x3041, 1, &bpr) != 0); /* the first page used again: now the newest */
    CHECK_INT(l.loads, UF_SLOTS);
    CHECK(uf_glyph(&c, 0x5041, 1, &bpr) != 0); /* a ninth: 0x31 (oldest) goes */
    CHECK_INT(l.loads, UF_SLOTS + 1);
    CHECK_INT(l.releases, 1);
    CHECK(uf_glyph(&c, 0x3041, 1, &bpr) != 0);
    CHECK_INT(l.loads, UF_SLOTS + 1); /* kept */
    CHECK(uf_glyph(&c, 0x3141, 1, &bpr) != 0);
    CHECK_INT(l.loads, UF_SLOTS + 2); /* read again */
    CHECK(uf_glyph(&c, 0x3142, 1, &bpr) == 0); /* the page is there, the glyph is not */
    CHECK_INT(l.loads, UF_SLOTS + 2);
    uf_flush(&c);
    CHECK_INT(l.releases, l.loads); /* every page given back */
}

static void missing_page_file_is_a_replacement_and_read_once(void)
{
    vt_u8 m[UF_MASK_MAX];
    uf_cache c;
    fakeload l;
    int bpr;
    memset(&l, 0, sizeof(l));
    l.answer = -1;
    uf_init(&c, fake_load, fake_release, &l, m, sizeof(m));
    uf_set_cell(&c, 8, 16);
    CHECK(uf_glyph(&c, 0x0416, 1, &bpr) == 0);
    CHECK(uf_glyph(&c, 0x0417, 1, &bpr) == 0);
    CHECK_INT(l.loads, 1);
    /* no memory now: asked again */
    l.answer = -2;
    CHECK(uf_glyph(&c, 0x0516, 1, &bpr) == 0);
    CHECK(uf_glyph(&c, 0x0516, 1, &bpr) == 0);
    CHECK_INT(l.loads, 3);
    /* past plane 1 (plane 2's ideographs): no page at all */
    CHECK(uf_glyph(&c, 0x20000, 2, &bpr) == 0);
    CHECK_INT(l.loads, 3);
    /* flushed: a page installed since is looked for again */
    uf_flush(&c);
    l.answer = 0;
    CHECK(uf_glyph(&c, 0x0441, 1, &bpr) != 0);
    CHECK_INT(l.loads, 4);
    uf_flush(&c);
}

static long short_load(void *u, int page, vt_u8 **buf)
{
    long n = fake_load(u, page, buf);
    return n - 1; /* a file cut short */
}

static void corrupt_page_is_given_back_and_remembered(void)
{
    vt_u8 m[UF_MASK_MAX];
    uf_cache c;
    fakeload l;
    int bpr;
    memset(&l, 0, sizeof(l));
    uf_init(&c, short_load, fake_release, &l, m, sizeof(m));
    uf_set_cell(&c, 8, 16);
    CHECK(uf_glyph(&c, 0x0441, 1, &bpr) == 0);
    CHECK(uf_glyph(&c, 0x0441, 1, &bpr) == 0);
    CHECK_INT(l.loads, 1);
    CHECK_INT(l.releases, 1);
}

/* ---- the order of the sources ----------------------------------------------------- */

static int n_outline, n_unifont;
static vt_u8 fake_mask[32];

static const vt_u8 *fake_outline(void *src, vt_u32 cp, int cells, int *bpr)
{
    (void)src;
    (void)cells;
    n_outline++;
    *bpr = 2;
    return cp == 0x2603 || cp == 0x1F600 ? fake_mask : 0;
}

static const vt_u8 *fake_unifont(void *src, vt_u32 cp, int cells, int *bpr)
{
    (void)src;
    (void)cp;
    (void)cells;
    n_unifont++;
    *bpr = 2;
    return fake_mask + 16;
}

static void fallback_asks_outline_then_unifont_after_the_font(void)
{
    vt_fallback f;
    int bpr;
    f.enc = VT_ENC_LATIN1;
    f.outline = fake_outline;
    f.outline_src = 0;
    f.unifont = fake_unifont;
    f.unifont_src = 0;
    f.colour = 0;
    n_outline = n_unifont = 0;
    /* the font's own and the drawn ones: no source asked */
    CHECK(vt_fallback_glyph(&f, 'A', 1, &bpr) == 0);
    CHECK(vt_fallback_glyph(&f, 0xE9, 1, &bpr) == 0);
    CHECK(vt_fallback_glyph(&f, 0x2500, 1, &bpr) == 0);
    CHECK(vt_fallback_glyph(&f, 0x2588, 1, &bpr) == 0);
    CHECK_INT(n_outline + n_unifont, 0);
    /* the outline font first */
    CHECK(vt_fallback_glyph(&f, 0x2603, 1, &bpr) == fake_mask);
    CHECK_INT(n_unifont, 0);
    /* then Unifont, for a character with no stand-in */
    CHECK(vt_fallback_glyph(&f, 0x0416, 1, &bpr) == fake_mask + 16);
    CHECK_INT(n_outline, 2);
    CHECK_INT(n_unifont, 1);
    /* the IBM font: what CP437 has is its own, the rest goes to Unifont */
    f.enc = VT_ENC_CP437;
    CHECK(vt_fallback_glyph(&f, 0xE9, 1, &bpr) == 0);
    CHECK_INT(n_unifont, 1);
    CHECK(vt_fallback_glyph(&f, 0xC0, 1, &bpr) == fake_mask + 16);
    CHECK_INT(n_unifont, 2);
    /* no sources: the stand-in */
    CHECK(vt_fallback_glyph(0, 0x2013, 1, &bpr) == 0);
}

/* W33: Claude Code's prompt U+276F, its en dashes, U+23F5, the middle dot
 * drew as Unifont glyphs on the U2 build; main drew '>' '-' '>' and the
 * font's dot. The stand-in comes before Unifont; an outline font the
 * profile names is still asked first (F1, as on main). */
static void stand_in_cells_keep_their_stand_in_not_unifont(void)
{
    static const vt_u32 cps[4] = { 0x276F, 0x2013, 0x23F5, 0xB7 };
    static const int want[4] = { '>', '-', '>', 0xB7 };
    vt_term *t = h_new(20, 2, VT_XTERM);
    vt_fallback f;
    int i, n, bpr, x = 0;
    const vt_cell *row;
    f.enc = VT_ENC_LATIN1;
    f.outline = 0;
    f.outline_src = 0;
    f.unifont = fake_unifont;
    f.unifont_src = 0;
    f.colour = 0;
    n_outline = n_unifont = 0;
    h_put(t, "\xE2\x9D\xAF\xE2\x80\x93\xE2\x8F\xB5\xC2\xB7"); /* ❯–⏵· */
    row = vt_row(t, 0, &n);
    for (i = 0; i < 4; i++, x++) {
        vt_u32 cp[VT_CLUSTER_CPS];
        int ncp;
        vt_glyph g;
        while (x < n && row[x].width == 0)
            x++;
        CHECK_INT(row[x].ch, cps[i]);
        CHECK(vt_cell_glyph(&f, t, &row[x], cp, &ncp, &bpr, &g) == 0);
        CHECK_INT(g.kind, VT_GLYPH_FONT);
        CHECK_INT(g.code, want[i]);
    }
    CHECK_INT(n_unifont, 0);
    /* an outline font named in the profile is asked for them first */
    f.outline = fake_outline;
    CHECK(vt_fallback_glyph(&f, 0x2013, 1, &bpr) == 0);
    CHECK_INT(n_outline, 1);
    CHECK_INT(n_unifont, 0);
    vt_free(t);
}

/* An emoji (plane 1) has no stand-in: Unifont is asked, over its two cells. */
static void emoji_asks_unifont_over_two_cells(void)
{
    vt_fallback f;
    int bpr;
    f.enc = VT_ENC_LATIN1;
    f.outline = 0;
    f.outline_src = 0;
    f.unifont = fake_unifont;
    f.unifont_src = 0;
    f.colour = 0;
    n_unifont = 0;
    CHECK(vt_fallback_glyph(&f, 0x1F600, 2, &bpr) == fake_mask + 16);
    CHECK_INT(n_unifont, 1);
    /* without Unifont: the box, as before */
    f.unifont = 0;
    CHECK(vt_fallback_glyph(&f, 0x1F600, 2, &bpr) == 0);
    CHECK_INT(vt_map_glyph(0x1F600, VT_ENC_LATIN1).kind, VT_GLYPH_MISSING);
}

/* U+1F600 from the emoji page: 16x16 over two 8-pixel cells, row for row
 * at 16 pixels; pairs of rows OR'd at 8 (topaz 8), still two cells wide. */
static const vt_u8 grin[32] = { 0x00, 0x00, 0x03, 0xE0, 0x0C, 0x18, 0x10, 0x04, 0x20, 0x02, 0x26, 0x32,
                                0x46, 0x31, 0x40, 0x01, 0x40, 0x01, 0x4F, 0xF9, 0x2A, 0xAA, 0x26, 0xB2,
                                0x13, 0xE4, 0x0C, 0x18, 0x03, 0xE0, 0x00, 0x00 };

static void emoji_page_gives_a_glyph_two_cells_wide(void)
{
    vt_u8 m[UF_MASK_MAX];
    uf_cache c;
    fileload l;
    const vt_u8 *g;
    int bpr = 0, y, same = 1;
    memset(&l, 0, sizeof(l));
    uf_init(&c, golden_load, golden_release, &l, m, sizeof(m));
    uf_set_cell(&c, 8, 16);
    g = uf_glyph(&c, 0x1F600, 2, &bpr);
    CHECK(g != 0);
    CHECK_INT(bpr, 2);
    for (y = 0; g && y < 16; y++)
        same &= g[y * 2] == grin[2 * y] && g[y * 2 + 1] == grin[2 * y + 1];
    CHECK(same);
    CHECK_INT(l.calls[0x1F6], 1);
    CHECK(uf_glyph(&c, 0x1F601, 2, &bpr) == 0); /* not in the page: the box */
    CHECK(uf_glyph(&c, 0x1F700, 2, &bpr) == 0); /* no page 1F7 among the goldens */
    CHECK_INT(l.calls[0x1F7], 1);
    uf_set_cell(&c, 8, 8); /* topaz 8 */
    g = uf_glyph(&c, 0x1F600, 2, &bpr);
    same = 1;
    for (y = 0; g && y < 8; y++)
        same &= g[y * 2] == (grin[4 * y] | grin[4 * y + 2]) && g[y * 2 + 1] == (grin[4 * y + 1] | grin[4 * y + 3]);
    CHECK(g && same);
    CHECK_INT(l.calls[0x1F6], 1); /* the page stays read */
    uf_flush(&c);
}

/* ---- reachability ----------------------------------------------------------------- */

/* Text written to the terminal, its row walked the way the renderer's
 * draw_rows walks it (render/amiga_render.c: every cell past ASCII through
 * vt_cell_glyph; the renderer itself has no host build, it draws through
 * Intuition): each character the font lacks and has no stand-in for
 * (an emoji too, over its two cells) reads its page once
 * (the loader's call count is the sentinel) and draws Unifont's glyph. */
static void text_reaches_the_unifont_pages(void)
{
    vt_term *t = h_new(20, 2, VT_XTERM);
    vt_u8 m[UF_MASK_MAX];
    uf_cache c;
    fileload l;
    vt_fallback f;
    int pass, drawn = 0, wide_drawn = 0;
    memset(&l, 0, sizeof(l));
    uf_init(&c, golden_load, golden_release, &l, m, sizeof(m));
    uf_set_cell(&c, 8, 16); /* TopazPro 16 */
    f.enc = VT_ENC_LATIN1;
    f.outline = 0;
    f.outline_src = 0;
    f.unifont = uf_glyph;
    f.unifont_src = &c;
    f.colour = 0;
    h_put(t, "a\xD0\x96\xE4\xB8\xAD\xE2\x80\x93\xE2\x98\x83\xC3\xA9\xF0\x9F\x98\x80"); /* aЖ中–☃é😀 */
    for (pass = 0; pass < 2; pass++) { /* a redraw reads nothing again */
        int n, x;
        const vt_cell *row = vt_row(t, 0, &n);
        for (x = 0; x < n; x++) {
            vt_u32 cp[VT_CLUSTER_CPS];
            int ncp, bpr = 0;
            vt_glyph g;
            const vt_u8 *mk;
            if (row[x].width == 0 || row[x].ch < 0x80)
                continue;
            mk = vt_cell_glyph(&f, t, &row[x], cp, &ncp, &bpr, &g);
            if (cp[0] == 0x0416) {
                int y, same = 1;
                for (y = 0; y < 16; y++)
                    same &= mk && mk[y * bpr] == zhe[y];
                CHECK(same);
            }
            if (cp[0] == 0x4E2D) {
                CHECK(mk && bpr == 2 && mk[3 * 2] == 0x3F && mk[3 * 2 + 1] == 0xFE);
                wide_drawn += mk != 0;
            }
            if (cp[0] == 0xE9 || cp[0] == 0x2013)
                CHECK(mk == 0); /* the font's own; the stand-in '-' */
            if (cp[0] == 0x1F600) {
                /* the emoji: two cells, Unifont's 16x16 row for row */
                CHECK_INT(row[x].width, 2);
                CHECK(mk && bpr == 2 && mk[1 * 2] == 0x03 && mk[1 * 2 + 1] == 0xE0);
                wide_drawn += mk != 0;
            }
            drawn += mk != 0;
        }
    }
    CHECK_INT(drawn, 8);      /* Ж 中 ☃ 😀, twice */
    CHECK_INT(wide_drawn, 4);
    CHECK_INT(l.loads, 4);    /* pages 04, 4E, 26, 1F6: once each */
    CHECK_INT(l.calls[0x04], 1);
    CHECK_INT(l.calls[0x4E], 1);
    CHECK_INT(l.calls[0x1F6], 1);
    CHECK_INT(l.calls[0x20], 0); /* the en dash keeps its stand-in */
    CHECK_INT(l.calls[0x00], 0); /* é is the font's */
    uf_flush(&c);
    CHECK_INT(l.releases, 4);
    vt_free(t);
}

void suite_unifont(void)
{
    page_file_gives_the_hex_rows();
    bad_page_files_are_refused();
    page_files_are_named_by_plane();
    sixteen_pixel_font_draws_one_to_one();
    topazpro_window_draws_unifont_glyph_whole_not_raised();
    eight_pixel_font_ors_pairs_of_rows();
    wide_glyph_spans_two_cells_or_squeezes_into_one();
    other_font_heights_get_no_glyph();
    least_recently_used_page_goes_first();
    missing_page_file_is_a_replacement_and_read_once();
    corrupt_page_is_given_back_and_remembered();
    fallback_asks_outline_then_unifont_after_the_font();
    stand_in_cells_keep_their_stand_in_not_unifont();
    emoji_asks_unifont_over_two_cells();
    emoji_page_gives_a_glyph_two_cells_wide();
    text_reaches_the_unifont_pages();
}
