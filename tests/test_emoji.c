/* render/emoji (ledger U4): the colour pages tools/gen_emoji.py writes
 * (tests/emoji/golden, drawn and checked by tests/test_gen_emoji.py), the
 * blend over a background, where the colour source stands in the order
 * of glyph sources (colour on RTG of 15 bits or more, Unifont's mono
 * glyph otherwise), and the path from text written to the terminal to
 * a colour emoji drawn. */
#include "harness.h"
#include <stdio.h>
#include <stdlib.h>
#include "../render/glyphmap.h"
#include "../render/emoji.h"

/* ---- the golden pages ----------------------------------------------------------- */

typedef struct pageload {
    int loads, releases;
    int calls[UF_PAGES];
} pageload;

static long golden_size(const char *name, vt_u8 *buf, long max)
{
    char path[64];
    FILE *f;
    long n;
    strcpy(path, "tests/emoji/golden/");
    strcat(path, name);
    if (!(f = fopen(path, "rb")))
        return -1;
    n = (long)fread(buf, 1, (size_t)max, f);
    fclose(f);
    return n;
}

/* The way the Amiga reads SYS:UP-Term/emoji/1F6: the header for the
 * length, then the page into a buffer of that size. */
static long golden_load(void *u, int page, vt_u8 **buf)
{
    pageload *l = (pageload *)u;
    vt_u8 head[CE_HEADER];
    char name[4];
    long len;
    l->loads++;
    l->calls[page]++;
    uf_page_name(page, name);
    if (golden_size(name, head, CE_HEADER) < CE_HEADER || (len = ce_page_length(head, CE_HEADER)) < 0)
        return -1;
    *buf = (vt_u8 *)malloc((size_t)len);
    return golden_size(name, *buf, len);
}

static void golden_release(void *u, vt_u8 *buf)
{
    ((pageload *)u)->releases++;
    free(buf);
}

static const vt_u8 *pal_at(const vt_u8 *g, int i)
{
    return g + 2 + 4 * i;
}

static int rgba_is(const vt_u8 *c, int r, int g, int b, int a)
{
    return c[0] == r && c[1] == g && c[2] == b && c[3] == a;
}

/* ---- page files ------------------------------------------------------------------ */

static void golden_page_gives_the_converters_colours(void)
{
    static vt_u8 p[4096];
    vt_u8 idx[CE_BOX_MAX];
    long n = golden_size("1F6", p, sizeof(p));
    const vt_u8 *g;
    CHECK_INT(ce_page_length(p, n), n);
    CHECK(ce_page_check(p, n, 0x1F6));
    /* U+1F600: 4-bit indices, the disc yellow, its corner clear */
    g = ce_page_glyph(p, 0x00);
    CHECK(g != 0);
    CHECK_INT(g[0], 4);
    CHECK(g[1] < 16);
    CHECK(ce_image(g, 16, 16, idx));
    CHECK(rgba_is(pal_at(g, idx[8 * 16 + 8]), 255, 204, 77, 255));
    CHECK_INT(pal_at(g, idx[0])[3], 0);
    /* U+1F680: 64 colours far apart, kept exactly at 8 bits */
    g = ce_page_glyph(p, 0x80);
    CHECK(g && g[0] == 8 && g[1] == 63);
    CHECK(g && ce_image(g, 16, 16, idx) && rgba_is(pal_at(g, idx[0]), 0, 0, 18, 255));
    CHECK(g && rgba_is(pal_at(g, idx[15 * 16 + 15]), 252 & 255, 252 & 255, 18, 255));
    CHECK(ce_page_glyph(p, 0x01) == 0);
    /* U+2705 at 8 rows: the check mark's block (2, 4) is row 4 there */
    n = golden_size("27", p, sizeof(p));
    CHECK(ce_page_check(p, n, 0x27));
    g = ce_page_glyph(p, 0x05);
    CHECK(g && ce_image(g, 16, 8, idx));
    CHECK(g && rgba_is(pal_at(g, idx[4 * 16 + 4]), 255, 255, 255, 255));
    CHECK(g && rgba_is(pal_at(g, idx[3 * 16 + 12]), 120, 177, 89, 255));
}

static void bad_colour_pages_are_refused(void)
{
    static vt_u8 p[4096];
    long n = golden_size("1F6", p, sizeof(p));
    CHECK(!ce_page_check(p, n, 0x1F7));     /* another page's file */
    CHECK(!ce_page_check(p, n - 1, 0x1F6)); /* cut short */
    CHECK(!ce_page_check(p, 40, 0x1F6));
    p[CE_HEADER] = 5;                       /* neither 4 nor 8 bits */
    CHECK(!ce_page_check(p, n, 0x1F6));
    p[CE_HEADER] = 4;
    CHECK(ce_page_check(p, n, 0x1F6));
    p[3] = '2';                             /* another format */
    CHECK_INT(ce_page_length(p, n), -1);
    CHECK(!ce_page_check(p, n, 0x1F6));
}

/* A box wider than 16 (a 10-pixel font) or of a height between 8 and 16:
 * the image centred, the background around it; a box too narrow: none. */
static void image_centres_in_the_two_cell_box(void)
{
    static vt_u8 p[4096];
    vt_u8 idx[CE_BOX_MAX];
    const vt_u8 *g;
    golden_size("1F6", p, sizeof(p));
    g = ce_page_glyph(p, 0x80);
    CHECK(ce_image(g, 20, 16, idx));
    CHECK_INT(idx[0], CE_BG);
    CHECK_INT(idx[1], CE_BG);
    CHECK_INT(idx[2], 0); /* the image's first pixel */
    CHECK_INT(idx[19], CE_BG);
    CHECK(ce_image(g, 16, 11, idx)); /* 11 rows: the 16x8 image from row 1 */
    CHECK_INT(idx[0], CE_BG);
    CHECK(idx[16] != CE_BG);
    CHECK_INT(idx[9 * 16], CE_BG);
    CHECK(!ce_image(g, 14, 16, idx));
    CHECK(!ce_image(g, 16, 7, idx));
    CHECK(!ce_image(g, 64, 64, idx)); /* past CE_BOX_MAX */
}

/* ---- the cache's cap ---------------------------------------------------------------- */

static long tiny_load(void *u, int page, vt_u8 **buf)
{
    ((pageload *)u)->loads++;
    ((pageload *)u)->calls[page]++;
    *buf = (vt_u8 *)malloc(4);
    return 4;
}

static int any_page(const vt_u8 *p, long len, int page)
{
    (void)p;
    (void)page;
    return len == 4;
}

/* The colour pages are up to ~106 KB: their cache holds CE_SLOTS, not
 * Unifont's 8, the least recently used given back first. */
static void colour_cache_keeps_at_most_its_slots(void)
{
    uf_cache c;
    pageload l;
    long len;
    int page;
    memset(&l, 0, sizeof(l));
    uf_init_pages(&c, tiny_load, golden_release, &l, any_page, CE_SLOTS);
    for (page = 0x1F0; page < 0x1F0 + CE_SLOTS; page++)
        CHECK(uf_page(&c, page, &len) != 0);
    CHECK_INT(l.releases, 0);
    CHECK(uf_page(&c, 0x1F0, &len) != 0); /* used again: the newest */
    CHECK(uf_page(&c, 0x1FA, &len) != 0); /* one more: 0x1F1 goes */
    CHECK_INT(l.releases, 1);
    CHECK(uf_page(&c, 0x1F0, &len) != 0);
    CHECK_INT(l.calls[0x1F0], 1);
    CHECK(uf_page(&c, 0x1F1, &len) != 0);
    CHECK_INT(l.calls[0x1F1], 2); /* read again */
    uf_flush(&c);
    CHECK_INT(l.releases, l.loads);
}

/* ---- the blend ------------------------------------------------------------------- */

static void palette_blends_over_the_background(void)
{
    static const vt_u8 pal[12] = { 255, 0, 0, 255, 0, 0, 255, 0, 255, 255, 255, 128 };
    vt_u32 ctab[256];
    int i, rest = 1;
    ce_blend(pal, 3, 0x204060UL, ctab);
    CHECK_INT(ctab[0], 0xFF0000L);  /* opaque: its own colour */
    CHECK_INT(ctab[1], 0x204060L);  /* transparent: the background */
    /* half: (255 * 128 + c * 127 + 127) / 255 -- 144, 160, 176 */
    CHECK_INT(ctab[2], 0x90A0B0L);
    for (i = 3; i < 256; i++)
        rest &= ctab[i] == 0x204060UL;
    CHECK(rest); /* every other index, CE_BG among them: the background */
    /* over the inverse cell's background (its foreground colour) */
    ce_blend(pal, 3, 0xE0E0E0UL, ctab);
    CHECK_INT(ctab[1], 0xE0E0E0L);
    CHECK_INT(ctab[2], 0xF0F0F0L);
    /* rounded to nearest, as the converter judged its colours (over()):
     * 255 at half over black is 128.0 -> 128, 3 at half is 1.5 -> 2 */
    {
        static const vt_u8 faint[8] = { 255, 3, 0, 128, 3, 255, 0, 128 };
        ce_blend(faint, 2, 0, ctab);
        CHECK_INT(ctab[0], 0x800200L);
        CHECK_INT(ctab[1], 0x028000L);
    }
}

/* ---- the target and the order ------------------------------------------------------ */

#define FB_W 160
#define FB_H 16

typedef struct fakefb {
    int calls;
    vt_u32 px[FB_W * FB_H];
} fakefb;

/* WriteLUTPixelArray's part: each index through the table into the frame */
static void fake_lut(void *u, const vt_u8 *idx, int w, int h, const vt_u32 *ctab, int px, int py)
{
    fakefb *fb = (fakefb *)u;
    int x, y;
    fb->calls++;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            if (px + x < FB_W && py + y < FB_H)
                fb->px[(py + y) * FB_W + px + x] = ctab[idx[y * w + x]];
}

static int n_mono;
static vt_u8 mono_mask[64];

static const vt_u8 *fake_unifont(void *src, vt_u32 cp, int cells, int *bpr)
{
    (void)src;
    (void)cp;
    (void)cells;
    n_mono++;
    *bpr = 2;
    return mono_mask;
}

static void fb_init(vt_fallback *f, ce_painter *p)
{
    f->enc = VT_ENC_LATIN1;
    f->outline = 0;
    f->outline_src = 0;
    f->unifont = fake_unifont;
    f->unifont_src = 0;
    f->colour = ce_has;
    f->colour_src = p;
}

/* What vt_cell_glyph gives the emoji `utf8` written to a terminal, on a
 * target: 1 colour, 0 Unifont's mono glyph (the mask), -1 anything else. */
static int emoji_drawn_as(ce_painter *p, int rtg, int depth, int cgx, const char *utf8)
{
    vt_term *t = h_new(4, 1, VT_XTERM);
    vt_fallback f;
    vt_u32 cps[VT_CLUSTER_CPS];
    vt_glyph g;
    int ncp, bpr, n, r;
    const vt_u8 *m;
    const vt_cell *row;
    fakefb fb;
    fb_init(&f, p);
    p->tg.rtg = rtg;
    p->tg.depth = depth;
    p->tg.write_lut = cgx ? fake_lut : 0;
    p->tg.user = &fb;
    h_put(t, utf8);
    row = vt_row(t, 0, &n);
    g.kind = VT_GLYPH_FONT;
    m = vt_cell_glyph(&f, t, &row[0], cps, &ncp, &bpr, &g);
    r = !m && g.kind == VT_GLYPH_COLOUR && g.code == 2 ? 1 : m == mono_mask ? 0 : -1;
    vt_free(t);
    return r;
}

#define GRIN "\xF0\x9F\x98\x80"     /* U+1F600 */
#define BEAM "\xF0\x9F\x98\x81"     /* U+1F601, not in the fixture */
#define ROCKET "\xF0\x9F\x9A\x80"   /* U+1F680 */
#define BRAIN "\xF0\x9F\xA7\xA0"    /* U+1F9E0, page 1F9 not installed */
#define HEART "\xF0\x9F\xA7\xA1"    /* U+1F9E1 */

static void colour_page_on_rtg_of_15_bits_mono_unifont_elsewhere(void)
{
    static ce_store st;
    pageload l;
    ce_painter p;
    memset(&l, 0, sizeof(l));
    uf_init_pages(&st.pages, golden_load, golden_release, &l, ce_page_check, CE_SLOTS);
    ce_init(&p);
    p.store = &st;
    ce_set_cell(&p, 8, 16); /* TopazPro 16 */
    n_mono = 0;
    /* planar AGA, 8-bit RTG, RTG without cybergraphics: Unifont, no page read */
    CHECK_INT(emoji_drawn_as(&p, 0, 8, 1, GRIN), 0);
    CHECK_INT(emoji_drawn_as(&p, 1, 8, 1, GRIN), 0);
    CHECK_INT(emoji_drawn_as(&p, 1, 24, 0, GRIN), 0);
    CHECK_INT(l.loads, 0);
    CHECK_INT(n_mono, 3);
    /* RTG of 15, 16 and 24 bits with cybergraphics: colour, Unifont not asked */
    CHECK_INT(emoji_drawn_as(&p, 1, 15, 1, GRIN), 1);
    CHECK_INT(emoji_drawn_as(&p, 1, 16, 1, GRIN), 1);
    CHECK_INT(emoji_drawn_as(&p, 1, 24, 1, ROCKET), 1);
    CHECK_INT(n_mono, 3);
    CHECK_INT(l.calls[0x1F6], 1); /* the page read once */
    /* one the page lacks, one on a page not installed: Unifont's */
    CHECK_INT(emoji_drawn_as(&p, 1, 16, 1, BEAM), 0);
    CHECK_INT(emoji_drawn_as(&p, 1, 16, 1, BRAIN), 0);
    CHECK_INT(emoji_drawn_as(&p, 1, 16, 1, HEART), 0);
    CHECK_INT(l.calls[0x1F9], 1); /* missing: remembered */
    CHECK_INT(n_mono, 6);
    /* a cell too narrow for the image (a 6-pixel font): Unifont's */
    ce_set_cell(&p, 6, 16);
    CHECK_INT(emoji_drawn_as(&p, 1, 16, 1, GRIN), 0);
    /* topaz 8: the 16x8 image */
    ce_set_cell(&p, 8, 8);
    CHECK_INT(emoji_drawn_as(&p, 1, 16, 1, GRIN), 1);
    /* no colour pages at all: Unifont's */
    p.store = 0;
    CHECK_INT(emoji_drawn_as(&p, 1, 16, 1, GRIN), 0);
    /* a one-cell character never asks the colour source */
    p.store = &st;
    CHECK(!ce_has(&p, 0x1F600, 1));
    uf_flush(&st.pages);
    CHECK_INT(l.releases, 1);
}

/* ---- reachability ---------------------------------------------------------------- */

/* Text written to the terminal, its row walked the way the renderer's
 * draw_rows walks it (render/amiga_render.c: every cell past ASCII through
 * vt_cell_glyph; a VT_GLYPH_COLOUR answer drawn with ce_paint over the
 * cell's background at its pixel; the renderer itself has no host build)
 * on a fake RTG screen of 16 bits with cybergraphics: the emoji after
 * "Grüße - ok" is drawn in colour by one write_lut call (the sentinel), its
 * yellow in the frame, the background around it; the same row on a
 * planar screen draws it with Unifont's glyph and writes no pixel. */
static int walk_row(vt_term *t, ce_painter *p, vt_u32 bg)
{
    vt_fallback f;
    int n, x, colour = 0;
    const vt_cell *row = vt_row(t, 0, &n);
    fb_init(&f, p);
    for (x = 0; x < n; x++) {
        vt_u32 cp[VT_CLUSTER_CPS];
        int ncp, bpr = 0;
        vt_glyph g;
        if (row[x].width == 0 || row[x].ch < 0x80)
            continue;
        g.kind = VT_GLYPH_FONT;
        if (!vt_cell_glyph(&f, t, &row[x], cp, &ncp, &bpr, &g) && g.kind == VT_GLYPH_COLOUR)
            colour += ce_paint(p, x * p->cw, 0, bg);
        else
            CHECK(cp[0] != 0x1F600 || ce_target_ok(&p->tg) == 0);
    }
    return colour;
}

static void text_reaches_the_colour_painter(void)
{
    vt_term *t = h_new(20, 2, VT_XTERM);
    static ce_store st;
    pageload l;
    ce_painter p;
    static fakefb fb;
    int x0 = 11 * 8; /* "Grüße - ok " is 11 cells */
    memset(&l, 0, sizeof(l));
    memset(&fb, 0, sizeof(fb));
    uf_init_pages(&st.pages, golden_load, golden_release, &l, ce_page_check, CE_SLOTS);
    ce_init(&p);
    p.store = &st;
    ce_set_cell(&p, 8, 16);
    p.tg.rtg = 1;
    p.tg.depth = 16;
    p.tg.write_lut = fake_lut;
    p.tg.user = &fb;
    h_put(t, "Gr\xC3\xBC\xC3\x9F" "e - ok \xF0\x9F\x98\x80");
    CHECK_INT(walk_row(t, &p, 0x282A36UL), 1);
    CHECK_INT(fb.calls, 1);
    CHECK_INT(p.drawn, 1);
    CHECK_INT(l.calls[0x1F6], 1);
    CHECK_INT(l.loads, 1); /* ü and ß are the font's: no page for them */
    CHECK_INT(fb.px[8 * FB_W + x0 + 8], 0xFFCC4DL); /* the disc's middle */
    CHECK_INT(fb.px[0 * FB_W + x0], 0x282A36L);     /* its corner: the background */
    CHECK_INT(fb.px[8 * FB_W + x0 - 1], 0);         /* the cell before: not touched */
    /* the same row on a planar (AGA) screen: Unifont's glyph, no pixel written */
    p.tg.rtg = 0;
    p.tg.depth = 8;
    n_mono = 0;
    CHECK_INT(walk_row(t, &p, 0x282A36UL), 0);
    CHECK_INT(fb.calls, 1);
    CHECK_INT(n_mono, 1);
    uf_flush(&st.pages);
    vt_free(t);
}

void suite_emoji(void)
{
    golden_page_gives_the_converters_colours();
    bad_colour_pages_are_refused();
    image_centres_in_the_two_cell_box();
    colour_cache_keeps_at_most_its_slots();
    palette_blends_over_the_background();
    colour_page_on_rtg_of_15_bits_mono_unifont_elsewhere();
    text_reaches_the_colour_painter();
}
