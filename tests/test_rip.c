/* RIPscrip: RIP-1 byte parser against the fixtures in tests/rip/ (goldens are the
 * .dump files beside them), and the RIP-3 canvas and palette. Run from the
 * repository root. */
#include <stdio.h>
#include <stdlib.h>
#include "harness.h"
#include "../engine/rip.h"

#define DUMP_MAX 65536

struct sink {
    char dump[DUMP_MAX];
    int len;
    long text_bytes;
};

static void on_cmd(void *ud, const struct rip_cmd *c)
{
    struct sink *s = ud;
    char line[2048];
    int n = rip_dump(c, line, sizeof(line));
    if (s->len + n + 2 < DUMP_MAX) {
        memcpy(s->dump + s->len, line, (size_t)n);
        s->len += n;
        s->dump[s->len++] = '\n';
        s->dump[s->len] = 0;
    }
}

static void on_text(void *ud, const unsigned char *p, long n)
{
    (void)p;
    ((struct sink *)ud)->text_bytes += n;
}

static long slurp(const char *path, unsigned char **out)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    *out = malloc((size_t)n + 1);
    if (fread(*out, 1, (size_t)n, f) != (size_t)n)
        n = -1;
    fclose(f);
    return n;
}

/* Parse a buffer in pieces of `step` bytes (0 = whole). */
static void parse(const unsigned char *b, long n, long step, struct sink *s, struct rip_state *st)
{
    struct rip_sinks sk;
    long i;
    sk.ud = s;
    sk.text = on_text;
    sk.cmd = on_cmd;
    s->len = 0;
    s->dump[0] = 0;
    s->text_bytes = 0;
    rip_init(st);
    if (step <= 0)
        step = n ? n : 1;
    for (i = 0; i < n; i += step)
        rip_feed(st, b + i, (n - i < step) ? n - i : step, &sk);
}

static void fixture(const char *name)
{
    char rip[256], gold[256];
    unsigned char *b = NULL, *g = NULL;
    long n, gn, off;
    static struct sink s, s2;
    struct rip_state st;
    strcpy(rip, "tests/rip/");
    strcat(rip, name);
    strcpy(gold, rip);
    strcat(rip, ".rip");
    strcat(gold, ".dump");
    n = slurp(rip, &b);
    gn = slurp(gold, &g);
    CHECK(n > 0);
    CHECK(gn > 0);
    if (n <= 0 || gn <= 0)
        return;
    g[gn] = 0;
    parse(b, n, 0, &s, &st);
    CHECK_STR(s.dump, (const char *)g);
    CHECK_INT(rip_errors(&st), 0);
    CHECK_INT(rip_unknown(&st), 0);
    /* the same stream cut at every offset gives the same commands */
    for (off = 1; off < n; off++) {
        struct rip_sinks sk;
        struct rip_state st2;
        sk.ud = &s2;
        sk.text = on_text;
        sk.cmd = on_cmd;
        s2.len = 0;
        s2.dump[0] = 0;
        s2.text_bytes = 0;
        rip_init(&st2);
        rip_feed(&st2, b, off, &sk);
        rip_feed(&st2, b + off, n - off, &sk);
        if (strcmp(s2.dump, (const char *)g)) {
            CHECK(0 && "split changes the commands");
            printf("    split at %ld of %s\n", off, name);
            break;
        }
    }
    parse(b, n, 1, &s2, &st);
    CHECK_STR(s2.dump, (const char *)g);
    free(b);
    free(g);
}

static void fixture_01_line_grammar(void) { fixture("01-line-grammar"); }
static void fixture_02_continuation(void) { fixture("02-continuation"); }
static void fixture_03_escapes(void) { fixture("03-escapes"); }

static void text_lines_pass_through(void)
{
    static struct sink s;
    struct rip_state st;
    const char *in = "plain\r\n!|c0F\r\nmore\r\n";
    parse((const unsigned char *)in, (long)strlen(in), 0, &s, &st);
    CHECK_STR(s.dump, "0 c 15\n");
    /* "plain\r\n" and "more\r\n" reach the text sink; the command line and its EOL do not */
    CHECK_INT(s.text_bytes, 7 + 6);
}

static void malformed_commands_are_counted(void)
{
    static struct sink s;
    struct rip_state st;
    const char *in = "!|L0A0A|c0F|L0A0A5050|c!!|1~\n";
    parse((const unsigned char *)in, (long)strlen(in), 0, &s, &st);
    CHECK_STR(s.dump, "0 c 15\n0 L 10,10,180,180\n");
    CHECK_INT(rip_errors(&st), 2);  /* short L, bad digits in c */
    CHECK_INT(rip_unknown(&st), 1); /* level 1 `~` */
}

static void level_and_pair_lists(void)
{
    static struct sink s;
    struct rip_state st;
    const char *in = "!|1M002S2S4G3W1000000region|P03000A0A0K0K0A\n";
    parse((const unsigned char *)in, (long)strlen(in), 0, &s, &st);
    CHECK(strstr(s.dump, "1 M ") == s.dump);
    CHECK(strstr(s.dump, "\"region\"") != NULL);
    CHECK(strstr(s.dump, "0 P 3,") != NULL);
    CHECK_INT(rip_errors(&st), 0);
}


/* ---- RIP-3: palette and canvas ---- */

/* A test-side mini interpreter standing in for the RIP-4 executor: just
 * enough (* reset, S fill colour, B bar, Q/a through rip_pal_exec) to run
 * fixture 05 end to end through the real parser. */
struct mini {
    struct cv *cv;
    struct rip_pal pal;
    int fill;
    int calls;
    unsigned long bar1_before_a; /* pixel of bar 1 as drawn on screen when `a` arrives */
};

static unsigned long shown(const struct cv *c, int x, int y)
{
    return c->pal[cv_get(c, x, y)];
}

static void mini_cmd(void *ud, const struct rip_cmd *c)
{
    struct mini *m = ud;
    m->calls++;
    if (c->level)
        return;
    if (c->ch == '*') {
        rip_pal_reset(&m->pal, m->cv);
    } else if (c->ch == 'S') {
        m->fill = (int)c->arg[1];
    } else if (c->ch == 'B') {
        cv_rect(m->cv, (int)c->arg[0], (int)c->arg[1], (int)c->arg[2], (int)c->arg[3], m->fill);
    } else if (c->ch == 'a') {
        m->bar1_before_a = shown(m->cv, 40, 50);
        rip_pal_exec(&m->pal, m->cv, c);
    } else {
        rip_pal_exec(&m->pal, m->cv, c);
    }
}

static void run_mini(struct mini *m, const unsigned char *b, long n)
{
    struct rip_sinks sk;
    struct rip_state st;
    sk.ud = m;
    sk.text = NULL;
    sk.cmd = mini_cmd;
    rip_init(&st);
    rip_feed(&st, b, n, &sk);
}

static void palette_fixture_05(void)
{
    unsigned char *b = NULL;
    long n = slurp("tests/rip/05-palette.rip", &b);
    struct mini m;
    int i;
    static const unsigned long want[16] = {
        0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
    };
    CHECK(n > 0);
    if (n <= 0)
        return;
    memset(&m, 0, sizeof(m));
    m.cv = cv_open(CV_RIP_W, CV_RIP_H, 256);
    CHECK(m.cv != NULL);
    run_mini(&m, b, n);
    CHECK(m.calls > 17);
    /* bar k is x = 36k.., y 20..100; bar 1 was blue until the last line recoloured index 1 */
    CHECK_INT(m.bar1_before_a, 0x0000AA);
    CHECK_INT(shown(m.cv, 40, 50), 0xAA0000);
    for (i = 0; i < 16; i++) {
        if (i == 1)
            continue;
        CHECK_INT(shown(m.cv, 36 * i + 5, 50), want[i]);
    }
    CHECK_INT(cv_get(m.cv, 36 * 6 + 5, 50), 6); /* the pixel stays index 6 */
    CHECK_INT(cv_get(m.cv, 5, 5), 0);           /* outside the bars: still index 0 */
    cv_close(m.cv);
    free(b);
}

static void palette_commands_move_output(void)
{
    struct cv *c = cv_open(8, 8, 256);
    struct rip_pal p;
    struct rip_cmd q;
    struct rip_cmd a;
    int i;
    memset(&q, 0, sizeof(q));
    memset(&a, 0, sizeof(a));
    rip_pal_reset(&p, c);
    cv_pixel(c, 2, 2, 12);
    CHECK_INT(shown(c, 2, 2), 0xFF5555);
    cv_take_dirty(c);
    /* a: one entry */
    a.level = 0, a.ch = 'a', a.nargs = 2, a.arg[0] = 12, a.arg[1] = 1;
    CHECK_INT(rip_pal_exec(&p, c, &a), 1);
    CHECK_INT(shown(c, 2, 2), 0x0000AA);
    CHECK_INT(cv_take_dirty(c).x1, 8); /* a palette change dirties the whole canvas */
    /* Q: all sixteen, every entry moves */
    q.level = 0, q.ch = 'Q', q.nargs = 16;
    for (i = 0; i < 16; i++)
        q.arg[i] = 63 - i;
    CHECK_INT(rip_pal_exec(&p, c, &q), 1);
    for (i = 0; i < 16; i++)
        CHECK_INT(c->pal[i], rip_ega_rgb(63 - i));
    /* a wrong argument count, level or index is refused and changes nothing */
    a.arg[0] = 16;
    CHECK_INT(rip_pal_exec(&p, c, &a), 0);
    a.arg[0] = 3, a.nargs = 1;
    CHECK_INT(rip_pal_exec(&p, c, &a), 0);
    a.nargs = 2, a.level = 1;
    CHECK_INT(rip_pal_exec(&p, c, &a), 0);
    /* reset restores the defaults, brown at 6 */
    rip_pal_reset(&p, c);
    CHECK_INT(c->pal[6], 0xAA5500);
    CHECK_INT(c->pal[14], 0xFFFF55);
    cv_close(c);
}

static void ega_master_levels(void)
{
    CHECK_INT(rip_ega_rgb(0), 0x000000);
    CHECK_INT(rip_ega_rgb(7), 0xAAAAAA);
    CHECK_INT(rip_ega_rgb(56), 0x555555);
    CHECK_INT(rip_ega_rgb(63), 0xFFFFFF);
    CHECK_INT(rip_ega_rgb(20), 0xAA5500);
    CHECK_INT(rip_ega_rgb(64 + 4), 0xAA0000); /* only the low six bits count */
}

static void canvas_dirty_and_serial(void)
{
    struct cv *c = cv_open(CV_RIP_W, CV_RIP_H, 256);
    struct cv_rect_s d;
    CHECK(c != NULL);
    CHECK_INT(cv_flush(c), 0); /* nothing drawn: no serial bump */
    CHECK_INT(c->serial, 0);
    cv_pixel(c, 10, 20, 5);
    cv_rect(c, 100, 50, 199, 99, 7);
    cv_pixel(c, -1, 0, 3);     /* clipped: no effect */
    cv_pixel(c, 640, 0, 3);
    CHECK_INT(cv_get(c, 10, 20), 5);
    CHECK_INT(cv_get(c, 199, 99), 7);
    CHECK_INT(cv_get(c, 200, 99), 0);
    CHECK_INT(cv_get(c, 640, 0), -1);
    CHECK_INT(cv_flush(c), 1);
    CHECK_INT(c->serial, 1);
    d = cv_take_dirty(c);
    CHECK_INT(d.x0, 10);
    CHECK_INT(d.y0, 20);
    CHECK_INT(d.x1, 200);
    CHECK_INT(d.y1, 100);
    d = cv_take_dirty(c);
    CHECK_INT(d.x1 - d.x0, 0);
    CHECK_INT(cv_flush(c), 0);
    CHECK_INT(c->serial, 1);
    /* spans clip and accept reversed corners */
    cv_hspan(c, 700, 600, 5, 9);
    CHECK_INT(cv_get(c, 639, 5), 9);
    cv_rect(c, -5, 340, 3, 400, 4);
    CHECK_INT(cv_get(c, 0, 349), 4);
    CHECK_INT(cv_get(c, 4, 349), 0);
    CHECK(cv_open(0, 5, 16) == NULL);
    CHECK(cv_open(2000, 5, 16) == NULL);
    cv_close(c);
}

static void canvas_placement(void)
{
    struct cv_map_s m;
    /* O7: 1:1 at the grid origin when the window fits */
    CHECK(cv_map(640, 350, 1000, 700, &m));
    CHECK_INT(m.out_w, 640);
    CHECK_INT(m.out_h, 350);
    CHECK_INT(m.num, 1);
    CHECK_INT(m.den, 1);
    /* the window is the knob: half width halves the drawn size, aspect kept */
    CHECK(cv_map(640, 350, 320, 700, &m));
    CHECK_INT(m.out_w, 320);
    CHECK_INT(m.out_h, 175);
    CHECK_INT(m.num * 2, m.den); /* 320/640, not reduced */
    /* the height is the tight side */
    CHECK(cv_map(640, 350, 1000, 175, &m));
    CHECK_INT(m.out_h, 175);
    CHECK_INT(m.out_w, 320);
    CHECK(!cv_map(640, 350, 0, 100, &m));
}

/* ---- RIP-2 / RIP-4: through the terminal ---- */

/* an 80x25 terminal with 8x14 cells (the 640x350 canvas is 1:1) and RIP in auto */
static vt_term *rip_term(void)
{
    vt_term *t = h_new(80, 25, VT_XTERM);
    vt_set_cell_pixels(t, 8, 14);
    vt_set_rip(t, 1);
    return t;
}

static int view0(vt_term *t, vt_image_view *v) { return vt_row_image(t, 0, 0, v); }

/* canvas index at (x, y); -2 when there is no canvas on row 0 */
static int rpx(vt_term *t, int x, int y)
{
    vt_image_view v;
    if (!view0(t, &v))
        return -2;
    return v.pix[(long)y * v.w + x];
}

static long rcount(vt_term *t, int idx)
{
    vt_image_view v;
    long n = 0, i;
    if (!view0(t, &v))
        return -1;
    for (i = 0; i < (long)v.w * v.h; i++)
        n += v.pix[i] == idx;
    return n;
}

/* pixels equal to idx on row y between x0 and x1 inclusive */
static long rrow(vt_term *t, int y, int x0, int x1, int idx)
{
    long n = 0;
    int x;
    for (x = x0; x <= x1; x++)
        n += rpx(t, x, y) == idx;
    return n;
}

static vt_term *rip_draw(const char *cmds)
{
    vt_term *t = rip_term();
    h_put(t, cmds);
    return t;
}

/* FNV-1a over the whole canvas, one byte a pixel */
static unsigned long canvas_sum(vt_term *t)
{
    vt_image_view v;
    unsigned long h = 2166136261UL;
    long i;
    if (!view0(t, &v))
        return 0;
    for (i = 0; i < (long)v.w * v.h; i++)
        h = ((h ^ v.pix[i]) * 16777619UL) & 0xFFFFFFFFUL;
    return h;
}

/* font f (1..4), direction d, size z: the RIP stream of one text fixture, @ then T: 10 glyphs and a space */
static void stroke_fixture(char *cmd, size_t n, int f, int d, int z)
{
    snprintf(cmd, n, "!|c0F|Y%02d%02d%02d00|@2S3CHgRip, a|TZy&\r\n", f, d, z);
}

/* golden checksums, [style 1..4][size 1,2][across, up]; stroke on */
static const unsigned long stroke_gold[4][2][2] = {
    { { 2193060398UL, 1752216166UL }, { 2382301143UL, 3844413134UL } },
    { { 2360949884UL, 1021602710UL }, { 3259880719UL, 3061184685UL } },
    { { 2590449555UL, 4163509401UL }, { 2611125737UL, 2621238633UL } },
    { { 40918293UL, 3834845909UL }, { 1273473025UL, 1397791636UL } }
};
static const unsigned long bitmap_gold[2][2] = { { 2972558292UL, 3012486204UL }, { 2480458793UL, 4238776281UL } }; /* the same fixtures, option off, font 1 */

/* The one reachability test: vt_write -> detection -> scanner -> parser -> drawing -> canvas
 * -> the image store's view. Numbers are the plan's (section 2.1). */
static void reach_rip_through_vt_write(void)
{
    vt_term *t = rip_term();
    vt_image_view v;
    int x, y;
    long red = 0;
    CHECK_INT(vt_rip_commands(t), 0);
    CHECK_INT(vt_images(t), 0);
    h_reply_clear();
    h_put(t, "\033[!" "!|c0F|L00001E00|S010C|B0A0A1414\r\n" "after\r\n");
    CHECK_INT(h_reply_len, 14);
    CHECK(memcmp(h_reply, "RIPSCRIP015410", 14) == 0);
    CHECK_INT(vt_rip_commands(t), 4);
    CHECK_INT(vt_images(t), 1);
    CHECK(view0(t, &v));
    CHECK_INT(v.w, 640);
    CHECK_INT(v.h, 350);
    CHECK_INT(v.cw, 8);
    CHECK_INT(v.ch, 14);
    CHECK_INT(v.z, -1);
    CHECK(!v.kitty);
    for (x = 0; x <= 50; x++)
        CHECK_INT(rpx(t, x, 0), 15);
    CHECK_INT(rpx(t, 51, 0), 0);
    CHECK(v.pal[15] == 0xFFFFFF);
    for (y = 0; y < v.h; y++)
        for (x = 0; x < v.w; x++)
            red += v.pix[(long)y * v.w + x] == 12;
    CHECK_INT(red, 961); /* 31 x 31 */
    CHECK_INT(rpx(t, 10, 10), 12);
    CHECK_INT(rpx(t, 40, 40), 12);
    CHECK_INT(rpx(t, 41, 41), 0);
    CHECK_INT(rpx(t, 9, 9), 0);
    CHECK(v.pal[12] == rip_ega_rgb(0x3C)); /* RIP-0: light red */
    CHECK(strstr(h_screen(t), "|") == 0);
    CHECK(strstr(h_screen(t), "after") != 0);
    CHECK_INT(vt_image_memory(t), 640L * 350L);
    /* RIP-5 / RIP-6 through the same door: a text window and a mouse field; the sentinels prove both ran */
    CHECK_INT(vt_rip_mouse_fields(t), 0);
    {
        vt_u8 out[VT_RIP_OUT_MAX];
        h_put(t, "!|w0005270A10\r\n" "!|1M000A0A1414" "1000000go^M\r\n" "!|g0001\r\n" "inside\r\n");
        CHECK_INT(vt_rip_commands(t), 7);   /* 4 + w + 1M + g */
        CHECK_INT(vt_rip_mouse_fields(t), 1); /* the mouse path ran */
        CHECK(strstr(h_row(t, 6), "inside") != 0); /* the text path ran: window row 1 is grid row 6 */
        CHECK_INT(vt_rip_click(t, 15, 15, out), 3);
        CHECK(memcmp(out, "go\r", 3) == 0);
        CHECK_INT(vt_rip_click(t, 300, 300, out), 0);
    }
    vt_free(t);
    /* RIP-3b through the same door: a stroke-font stream gives the golden checksum and the stroke code ran */
    {
        char cmd[128];
        long before = vt_rip_stroke_glyphs();
        t = rip_term();
        vt_set_rip_stroke(t, 1);
        stroke_fixture(cmd, sizeof cmd, 3, 0, 2);
        h_put(t, "\033[!");
        h_put(t, cmd);
        CHECK_INT(vt_rip_stroke_glyphs() - before, 10); /* "HgRip, a" 7 + "Zy&" 3; the space draws nothing */
        CHECK(canvas_sum(t) == stroke_gold[2][1][0]);
        vt_free(t);
    }
}

/* ---- RIP-7: clipboard and icons ---- */
#define ICN_GOLD 653013169UL /* canvas checksum after the 1I fixture */
static const unsigned char icn_file[] = { 8, 0, 2, 0, 0xF0, 0x0F, 0xAA, 0x00, 0xFF, 0xFF, 0xFF, 0xFF };
static int icn_reads, icn_frees;
static char icn_last[64];

static int icn_read(const char *path, long off, long size, vt_u8 **buf, long *n)
{
    (void)off;
    (void)size;
    icn_reads++;
    strncpy(icn_last, path, sizeof(icn_last) - 1);
    if (strcmp(path, "TEST.ICN"))
        return 0;
    *buf = (vt_u8 *)icn_file; /* host-owned; the release below only counts */
    *n = (long)sizeof(icn_file);
    return 1;
}

static int icn_decode(const vt_u8 *d, long n, vt_u32 **argb, int *w, int *h)
{
    (void)d; (void)n; (void)argb; (void)w; (void)h;
    return 0;
}

static void icn_release(vt_u32 *p)
{
    (void)p;
    icn_frees++;
}

static void icons_and_clipboard(void)
{
    static const int row0[8] = { 5, 1, 5, 1, 6, 2, 6, 2 };
    vt_term *t = rip_term();
    long before = vt_rip_icons(t);
    int x;
    vt_set_image_decoder(t, icn_decode, icn_release);
    vt_set_file_reader(t, icn_read, 0);
    h_put(t, "\033[!");
    /* 1I at 4,3 copy, clipboard flag 1, file TEST (".ICN" added) */
    h_put(t, "!|1I040300" "1" "0000TEST\r\n");
    CHECK_INT(vt_rip_icons(t) - before, 1); /* the icon path ran */
    CHECK_INT(icn_reads, 1);
    CHECK_INT(icn_frees, 1);
    CHECK(!strcmp(icn_last, "TEST.ICN"));
    for (x = 0; x < 8; x++) {
        CHECK_INT(rpx(t, 4 + x, 3), row0[x]);
        CHECK_INT(rpx(t, 4 + x, 4), 15);
    }
    CHECK_INT(rpx(t, 12, 3), 0);
    CHECK(canvas_sum(t) == ICN_GOLD);
    /* 1P (clipboard from the icon) copy at 20,3 then xor over itself clears it; and mode 4 not */
    h_put(t, "!|1P0K0300" "0\r\n");
    CHECK_INT(rpx(t, 20, 3), 5);
    CHECK_INT(rpx(t, 27, 4), 15);
    h_put(t, "!|1P0K0301" "0\r\n");
    CHECK_INT(rpx(t, 20, 3), 0);
    CHECK_INT(rpx(t, 27, 4), 0);
    h_put(t, "!|1P0K0304" "0\r\n");
    CHECK_INT(rpx(t, 20, 3), 10); /* not 5 */
    /* 1C the icon box 4,3..11,4 into the clipboard, 1P at 30,10 */
    h_put(t, "!|1C0403" "0B04" "0\r\n");
    h_put(t, "!|1P0U0A00" "0\r\n");
    CHECK_INT(rpx(t, 30, 10), 5);
    CHECK_INT(rpx(t, 37, 11), 15);
    /* 1G moves the box 4,3..11,4 to row 20 */
    h_put(t, "!|1G0403" "0B04" "00" "0K\r\n");
    CHECK_INT(rpx(t, 4, 20), 5);
    CHECK_INT(rpx(t, 11, 21), 15);
    vt_free(t);
}

static void icon_names_that_climb_out_are_refused(void)
{
    static const char *const bad[] = { "!|1I0403000" "0000..\r\n", "!|1I0403000" "0000A/TEST\r\n",
                                       "!|1I0403000" "0000C:TEST\r\n", "!|1I0403000" "0000..\\TEST\r\n",
                                       "!|1I0403000" "0000.\r\n" };
    vt_term *t = rip_term();
    long before = vt_rip_icons(t);
    unsigned long blank;
    int i;
    vt_set_image_decoder(t, icn_decode, icn_release);
    vt_set_file_reader(t, icn_read, 0);
    h_put(t, "\033[!");
    h_put(t, "!|c0F\r\n");
    blank = canvas_sum(t);
    icn_reads = 0;
    for (i = 0; i < (int)(sizeof(bad) / sizeof(bad[0])); i++)
        h_put(t, bad[i]);
    CHECK_INT(icn_reads, 0); /* the host file hook was never asked */
    CHECK_INT(vt_rip_icons(t) - before, 0);
    CHECK(canvas_sum(t) == blank);
    /* no release function: no read either */
    vt_free(t);
    t = rip_term();
    vt_set_file_reader(t, icn_read, 0);
    h_put(t, "\033[!");
    icn_reads = 0;
    h_put(t, "!|1I040300" "0" "0000TEST\r\n");
    CHECK_INT(icn_reads, 0);
    vt_free(t);
}

static void detection_and_switch(void)
{
    vt_term *t = rip_term();
    h_reply_clear();
    h_put(t, "\033[0!");
    CHECK_INT(h_reply_len, 14);
    h_reply_clear();
    h_put(t, "\033["); /* split anywhere */
    CHECK_INT(h_reply_len, 0);
    h_put(t, "!");
    CHECK_INT(h_reply_len, 14);
    /* ESC [ 1 ! : RIP off, the command line is text */
    h_reply_clear();
    h_put(t, "\033[1!");
    CHECK_INT(h_reply_len, 0);
    h_put(t, "!|c0F|L00001E00\r\n");
    CHECK_INT(vt_rip_commands(t), 0);
    CHECK_INT(vt_images(t), 0);
    CHECK(strstr(h_screen(t), "!|c0F") != 0);
    /* ESC [ 2 ! : on again */
    h_put(t, "\033[2!" "!|c0F|L00001E00\r\n");
    CHECK_INT(vt_rip_commands(t), 2);
    CHECK_INT(vt_images(t), 1);
    CHECK_INT(rpx(t, 50, 0), 15);
    vt_free(t);
    /* an escape that is not ours reaches the engine whole */
    t = rip_term();
    h_put(t, "a\033[1mb\033[0m");
    CHECK_STR(h_screen(t), "ab");
    vt_free(t);
}

static void decstr_still_resets(void)
{
    vt_term *t = rip_term(), *u = h_new(80, 25, VT_XTERM);
    /* insert mode on, soft reset, then overwrite: only a DECSTR that reached the engine turns insert off */
    const char *s = "\033[4h" "\033[!p" "ab\rcd";
    h_put(t, s);
    h_put(u, s);
    CHECK_STR(h_screen(t), h_screen(u));
    CHECK(strncmp(h_screen(t), "cd", 2) == 0);
    vt_free(t);
    vt_free(u);
}

static void rip_off_is_byte_identical(void)
{
    vt_term *t = h_new(80, 25, VT_XTERM), *u = h_new(80, 25, VT_XTERM);
    const char *s = "\033[!" "!|c0F|L00001E00\r\n" "x\033[2!y\r\n";
    h_reply_clear();
    h_put(t, s);
    CHECK_INT(h_reply_len, 0); /* RIP not set: no detection answer */
    h_put(u, s);
    vt_set_rip(u, 1);
    vt_set_rip(u, 0); /* set and unset leaves nothing behind */
    h_put(u, s);
    CHECK_INT(vt_rip_commands(t), 0);
    CHECK_INT(vt_rip_commands(u), 0);
    CHECK_INT(vt_images(t), 0);
    CHECK_INT(vt_images(u), 0);
    vt_free(t);
    vt_free(u);
}

/* ---- RIP-AUTO: vt_set_rip(t, 2) ---- */

static vt_term *auto_term(void)
{
    vt_term *t = h_new(80, 25, VT_XTERM);
    vt_set_cell_pixels(t, 8, 14);
    vt_set_rip(t, 2);
    return t;
}

static void auto_plain_text_is_rip_off(void)
{
    vt_term *t = auto_term(), *u = h_new(80, 25, VT_XTERM);
    /* a `!` line start that is not `!|`, a `!` alone at a chunk end, text, a colour sequence */
    const char *s = "hello\r\n" "!bang\r\n" "\033[31mred\033[0m\r\n" "a!|b\r\n" "!";
    h_reply_clear();
    h_put(t, s);
    h_put(t, "x\r\n");
    h_put(u, s);
    h_put(u, "x\r\n");
    CHECK_STR(h_screen(t), h_screen(u));
    CHECK_INT(vt_rip_active(t), 0);
    CHECK_INT(vt_rip_commands(t), 0);
    CHECK_INT(vt_images(t), 0);
    CHECK_INT(h_reply_len, 0);
    vt_free(t);
    vt_free(u);
}

static void auto_query_turns_on(void)
{
    vt_term *t = auto_term();
    h_reply_clear();
    CHECK_INT(vt_rip_active(t), 0);
    h_put(t, "\033[!");
    CHECK_INT(h_reply_len, 14);
    CHECK(memcmp(h_reply, "RIPSCRIP015410", 14) == 0);
    CHECK_INT(vt_rip_active(t), 1);
    h_put(t, "\033[1!");
    CHECK_INT(vt_rip_active(t), 0);
    h_reply_clear();
    h_put(t, "\033[0!");
    CHECK_INT(h_reply_len, 14);
    CHECK_INT(vt_rip_active(t), 1);
    h_put(t, "\033[1!" "\033[2!");
    CHECK_INT(vt_rip_active(t), 1);
    vt_free(t);
}

/* the reachability test for Auto: vt_write-level bytes, the sentinel moves, numbers asserted */
static void auto_valid_line_turns_on_and_draws(void)
{
    vt_term *t = auto_term();
    vt_image_view v;
    CHECK_INT(vt_rip_commands(t), 0);
    h_put(t, "ok\r\n" "!|c0F|L00001E00|S010C|B0A0A1414\r\n" "after\r\n");
    CHECK_INT(vt_rip_active(t), 1);
    CHECK_INT(vt_rip_commands(t), 4);
    CHECK_INT(vt_images(t), 1);
    CHECK(view0(t, &v));
    CHECK_INT(v.w, 640);
    CHECK_INT(rpx(t, 40, 40), 12); /* bar corner, S010C fill colour 12 */
    vt_free(t);
    /* split at every byte, and a CRLF continuation */
    t = auto_term();
    {
        const char *s = "!|c0F|L0000\\\r\n1E00|B0A0A1414\r\n";
        size_t i;
        for (i = 0; s[i]; i++) {
            char b[2];
            b[0] = s[i];
            b[1] = 0;
            h_put(t, b);
        }
    }
    CHECK_INT(vt_rip_active(t), 1);
    CHECK_INT(vt_rip_commands(t), 3);
    vt_free(t);
}

static void auto_bad_line_is_text(void)
{
    vt_term *t = auto_term(), *u = h_new(80, 25, VT_XTERM);
    const char *lines[] = { "!|grep foo bar\r\n", "!|1|\r\n", "!|c0F|zz\r\n", "!|zzzz\r\n", "!|\r\n", "!|c0\r\n", 0 };
    int i;
    for (i = 0; lines[i]; i++) {
        h_put(t, lines[i]);
        h_put(u, lines[i]);
        CHECK_INT(vt_rip_active(t), 0);
        CHECK_STR(h_screen(t), h_screen(u));
    }
    CHECK_INT(vt_rip_commands(t), 0);
    CHECK_INT(vt_images(t), 0);
    CHECK(strstr(h_screen(t), "!|grep foo bar") != 0);
    /* a line longer than the hold buffer is text too, even when it would parse */
    {
        static char big[6000];
        memset(big, 'c', sizeof(big));
        big[0] = '!';
        big[1] = '|';
        big[5990] = '\r';
        big[5991] = '\n';
        big[5992] = 0;
        h_put(t, big);
        h_put(u, big);
        CHECK_INT(vt_rip_active(t), 0);
        CHECK_STR(h_screen(t), h_screen(u));
    }
    /* and a good line afterwards still turns RIP on */
    h_put(t, "!|c0F|L00001E00\r\n");
    CHECK_INT(vt_rip_active(t), 1);
    vt_free(t);
    vt_free(u);
}

static void on_mode_is_active_at_once(void)
{
    vt_term *t = rip_term();
    CHECK_INT(vt_rip_active(t), 1);
    h_put(t, "!|zzzz\r\n"); /* On does not hold or judge lines */
    CHECK_INT(vt_rip_active(t), 1);
    h_put(t, "\033[1!");
    CHECK_INT(vt_rip_active(t), 0);
    vt_free(t);
    t = h_new(80, 25, VT_XTERM);
    CHECK_INT(vt_rip_active(t), 0);
    vt_free(t);
}

static void canvas_lifecycle(void)
{
    vt_term *t = rip_draw("!|c0F|X0505\r\n");
    long serial0, serial1;
    vt_image_view v;
    CHECK_INT(vt_images(t), 1);
    CHECK_INT(rpx(t, 5, 5), 15);
    CHECK(view0(t, &v));
    serial0 = v.serial;
    h_put(t, "!|X0606\r\n");
    CHECK(view0(t, &v));
    serial1 = v.serial;
    CHECK(serial1 != serial0); /* drawing makes it a new picture for the renderer's cache */
    h_put(t, "text only\r\n");
    CHECK(view0(t, &v));
    CHECK_INT(v.serial, serial1); /* no drawing, no new picture */
    CHECK_INT(vt_images(t), 1);
    CHECK_INT(vt_image_memory(t), 224000);
    /* the screen clear drops the placements and the store frees the picture: the adapter
     * notices and the next command opens a fresh, blank canvas */
    h_put(t, "\033[2J\r\n"); /* a command starts a line: the clear leaves the cursor mid-line */
    CHECK_INT(vt_images(t), 0);
    CHECK_INT(vt_image_memory(t), 0);
    h_put(t, "!|X0707\r\n");
    CHECK_INT(vt_images(t), 1);
    CHECK_INT(rpx(t, 7, 7), 15);
    CHECK_INT(rpx(t, 5, 5), 0);
    vt_free(t);
}

/* RIP-VIEW-BG0: index 0 is palette entry 0 on a RIP canvas, "unset" only on a sixel */
static void index_zero_is_a_colour_for_rip_not_for_sixel(void)
{
    vt_term *t = rip_draw("!|a0000|c00|L00000A00\r\n"); /* entry 0 -> master 0 stays black: control */
    vt_image_view v;
    vt_free(t);
    t = rip_draw("!|a000K|c00|L00000A00\r\n"); /* entry 0 -> master 20 (brown), a line in colour 0 */
    CHECK(view0(t, &v));
    CHECK_INT(rpx(t, 5, 0), 0);
    CHECK_INT(v.bg0, 0);
    CHECK_INT(v.kitty, 0);
    CHECK(v.pal[0] == rip_ega_rgb(20));
    vt_free(t);
    t = rip_term();
    h_put(t, "\033Pq\"1;1;4;6#1;2;100;0;0#1~~~~\033\\");
    CHECK(vt_row_image(t, 0, 0, &v));
    CHECK_INT(v.bg0, 1);
    vt_free(t);
}

static void palette_through_the_terminal(void)
{
    vt_term *t = rip_draw("!|a0C0K|c0C|X0101\r\n"); /* index 12 gets master 20 (RIP-0: 6 is brown 0x14, here 12) */
    vt_image_view v;
    CHECK(view0(t, &v));
    CHECK(v.pal[12] == rip_ega_rgb(20));
    CHECK_INT(rpx(t, 1, 1), 12);
    vt_free(t);
}

/* each declared drawing knob moves the output */
static void knob_colour_and_write_mode(void)
{
    vt_term *t = rip_draw("!|c05|L00000A00\r\n");
    CHECK_INT(rpx(t, 5, 0), 5);
    vt_free(t);
    t = rip_draw("!|c0F|L00000A00\r\n"); /* copy twice stays */
    h_put(t, "!|L00000A00\r\n");
    CHECK_INT(rpx(t, 5, 0), 15);
    h_put(t, "!|W01|L00000A00\r\n"); /* XOR with itself clears */
    CHECK_INT(rpx(t, 5, 0), 0);
    h_put(t, "!|W01|L00000A00\r\n"); /* and again sets */
    CHECK_INT(rpx(t, 5, 0), 15);
    vt_free(t);
}

static void knob_line_style_and_thickness(void)
{
    vt_term *t = rip_draw("!|c0F|L00000F00\r\n");
    CHECK_INT(rrow(t, 0, 0, 15, 15), 16); /* solid */
    vt_free(t);
    t = rip_draw("!|c0F|=0100000" "1|L00000F00\r\n");
    CHECK_INT(rrow(t, 0, 0, 15, 15), 8); /* dotted 0xCCCC: two on, two off (UNVERIFIED pattern) */
    CHECK_INT(rpx(t, 0, 0), 15);
    CHECK_INT(rpx(t, 2, 0), 0);
    vt_free(t);
    t = rip_draw("!|c0F|=0402Z3" "01|L00000F00\r\n"); /* user pattern 0x0F0F */
    CHECK_INT(rrow(t, 0, 0, 15, 15), 8);
    CHECK_INT(rpx(t, 0, 0), 0);
    CHECK_INT(rpx(t, 4, 0), 15);
    vt_free(t);
    t = rip_draw("!|c0F|L00050F05\r\n");
    CHECK_INT(rcount(t, 15), 16);
    vt_free(t);
    t = rip_draw("!|c0F|=00000003|L00050F05\r\n"); /* thickness 3: three rows */
    CHECK_INT(rcount(t, 15), 48);
    CHECK_INT(rpx(t, 5, 4), 15);
    CHECK_INT(rpx(t, 5, 6), 15);
    CHECK_INT(rpx(t, 5, 3), 0);
    vt_free(t);
}

static void knob_fill_style(void)
{
    vt_term *t = rip_draw("!|S010C|B00000707\r\n");
    CHECK_INT(rcount(t, 12), 64); /* solid */
    vt_free(t);
    t = rip_draw("!|S000C|B00000707\r\n");
    CHECK_INT(rcount(t, 12), 0); /* empty: background */
    vt_free(t);
    t = rip_draw("!|S020C|B00000707\r\n");
    CHECK_INT(rcount(t, 12), 32); /* line fill (UNVERIFIED table) */
    vt_free(t);
    t = rip_draw("!|S030C|B00000707\r\n");
    CHECK_INT(rcount(t, 12), 8); /* light slash: one pixel a row (UNVERIFIED table) */
    vt_free(t);
    t = rip_draw("!|s7300000000000000" "0C|B00000707\r\n"); /* user pattern: top row only */
    CHECK_INT(rcount(t, 12), 8);
    CHECK_INT(rrow(t, 0, 0, 7, 12), 8);
    vt_free(t);
}

static void knob_geometry(void)
{
    vt_term *t = rip_draw("!|c0F|C2S2S0K\r\n"); /* circle, centre (100,100) radius 20 */
    CHECK_INT(rpx(t, 120, 100), 15);
    CHECK_INT(rpx(t, 80, 100), 15);
    CHECK_INT(rpx(t, 100, 80), 15);
    CHECK_INT(rpx(t, 100, 120), 15);
    CHECK_INT(rpx(t, 100, 100), 0);
    vt_free(t);
    t = rip_draw("!|c0F|A2S2S002I0K\r\n"); /* arc 0 to 90 degrees, counterclockwise from three o'clock */
    CHECK_INT(rpx(t, 120, 100), 15);
    CHECK_INT(rpx(t, 100, 80), 15);
    CHECK_INT(rpx(t, 80, 100), 0);
    CHECK_INT(rpx(t, 100, 120), 0);
    vt_free(t);
    t = rip_draw("!|c0F|S010C|I2S2S002I0K\r\n"); /* pie slice: filled, centre set */
    CHECK_INT(rpx(t, 100, 100), 15); /* the outline runs through the centre */
    CHECK_INT(rpx(t, 110, 90), 12);
    CHECK_INT(rpx(t, 90, 110), 0);
    vt_free(t);
    t = rip_draw("!|c0F|O2S2S00000K0A\r\n"); /* oval, radii 20 and 10 */
    CHECK_INT(rpx(t, 120, 100), 15);
    CHECK_INT(rpx(t, 100, 90), 15);
    CHECK_INT(rpx(t, 100, 89), 0);
    CHECK_INT(rpx(t, 100, 100), 0);
    vt_free(t);
    t = rip_draw("!|c0F|S010C|o2S2S0K0A\r\n"); /* filled oval */
    CHECK_INT(rpx(t, 100, 100), 12);
    CHECK_INT(rpx(t, 119, 100), 12);
    CHECK_INT(rpx(t, 100, 89), 0);
    vt_free(t);
    t = rip_draw("!|c0F|Z0A0K0K0K0U0K140K0A\r\n"); /* a Bezier whose control points lie on the line */
    CHECK_INT(rrow(t, 20, 10, 40, 15), 31);
    vt_free(t);
    t = rip_draw("!|c0F|Z0A140A0A140A14140A\r\n"); /* ends (10,40) (40,40), pulled up to y 17 at x 25 */
    CHECK_INT(rpx(t, 10, 40), 15);
    CHECK_INT(rpx(t, 40, 40), 15);
    CHECK_INT(rpx(t, 25, 40), 0);
    CHECK_INT(rpx(t, 25, 17) + rpx(t, 25, 18), 15); /* exactly one of the two rows (UNVERIFIED rounding) */
    vt_free(t);
}

static void knob_polygon_rect_flood(void)
{
    vt_term *t = rip_draw("!|c0F|S010C|p030A0A140A0A14\r\n"); /* filled triangle (10,10) (40,10) (10,40) */
    CHECK_INT(rpx(t, 15, 15), 12);
    CHECK_INT(rpx(t, 35, 35), 0);
    CHECK_INT(rpx(t, 10, 10), 15);
    vt_free(t);
    t = rip_draw("!|c0F|P030A0A140A0A14\r\n"); /* the same, outline only */
    CHECK_INT(rpx(t, 15, 15), 0);
    CHECK_INT(rpx(t, 25, 10), 15);
    CHECK_INT(rpx(t, 10, 25), 15);
    vt_free(t);
    t = rip_draw("!|c0F|l030A0A140A0A14\r\n"); /* the polyline is not closed */
    CHECK_INT(rpx(t, 25, 10), 15);
    CHECK_INT(rpx(t, 10, 25), 0);
    vt_free(t);
    t = rip_draw("!|c0F|R0A0A0U0U\r\n"); /* rectangle outline, then fill it */
    CHECK_INT(rpx(t, 20, 20), 0);
    CHECK_INT(rpx(t, 10, 20), 15);
    CHECK_INT(rpx(t, 30, 30), 15);
    h_put(t, "!|S010C|F0K0K0F\r\n");
    CHECK_INT(rpx(t, 20, 20), 12);
    CHECK_INT(rpx(t, 11, 11), 12);
    CHECK_INT(rpx(t, 10, 10), 15); /* the border stays */
    CHECK_INT(rpx(t, 5, 5), 0);    /* outside stays */
    h_put(t, "!|E\r\n");
    CHECK_INT(rcount(t, 12) + rcount(t, 15), 0);
    vt_free(t);
}

/* ---- RIP-5: text and text windows ---- */

static int popcount8(int ch)
{
    int r, b, n = 0;
    for (r = 0; r < 8; r++)
        for (b = 0; b < 8; b++)
            n += (rip_font8_row(ch, r) >> b) & 1;
    return n;
}

static long bbox_extent(vt_term *t, int idx, int *x0, int *y0, int *x1, int *y1)
{
    int x, y;
    long n = 0;
    *x0 = *y0 = 9999;
    *x1 = *y1 = -1;
    for (y = 0; y < 350; y++)
        for (x = 0; x < 640; x++)
            if (rpx(t, x, y) == idx) {
                n++;
                if (x < *x0) *x0 = x;
                if (x > *x1) *x1 = x;
                if (y < *y0) *y0 = y;
                if (y > *y1) *y1 = y;
            }
    return n;
}

static void text_font_scale_direction(void)
{
    vt_term *t;
    int x0, y0, x1, y1, a = popcount8('A');
    CHECK(a > 8 && a < 56);
    CHECK_INT(rip_font8_row('A', 0), 0x30); /* font8x8_basic A row 0 is 0x0C, bit-reversed to MSB-left */
    /* size 1, 2, 3: the lit pixels grow with the square of the size, the box with the size */
    t = rip_draw("!|c0F|Y00000100|@0A0AA\r\n");
    CHECK_INT(bbox_extent(t, 15, &x0, &y0, &x1, &y1), a);
    CHECK(x0 >= 10 && x1 <= 17 && y0 >= 10 && y1 <= 17);
    vt_free(t);
    t = rip_draw("!|c0F|Y00000200|@0A0AA\r\n");
    CHECK_INT(bbox_extent(t, 15, &x0, &y0, &x1, &y1), 4 * a);
    CHECK(x0 >= 10 && x1 <= 25 && y0 >= 10 && y1 <= 25);
    vt_free(t);
    t = rip_draw("!|c0F|Y00000300|@0A0AA\r\n");
    CHECK_INT(bbox_extent(t, 15, &x0, &y0, &x1, &y1), 9 * a);
    vt_free(t);
    /* a string advances 8 x size per character across (direction 0) */
    t = rip_draw("!|c0F|Y00000100|@0A0AAA\r\n");
    CHECK_INT(bbox_extent(t, 15, &x0, &y0, &x1, &y1), 2 * a);
    CHECK(x1 >= 18 && x1 <= 25 && y1 <= 17);
    vt_free(t);
    /* direction 1: the same pixels, the string climbs the screen */
    t = rip_draw("!|c0F|Y00010100|@0A32A\r\n");
    CHECK_INT(bbox_extent(t, 15, &x0, &y0, &x1, &y1), a);
    vt_free(t);
    t = rip_draw("!|c0F|Y00010100|@0A32AA\r\n");
    CHECK_INT(bbox_extent(t, 15, &x0, &y0, &x1, &y1), 2 * a);
    CHECK(y1 - y0 >= 9); /* two glyphs one above the other */
    CHECK(x1 - x0 <= 7);
    vt_free(t);
    /* T draws at the current position (m 0A0A) and advances it */
    t = rip_draw("!|c0F|Y00000100|m0A0A|TAB\r\n");
    CHECK_INT(bbox_extent(t, 15, &x0, &y0, &x1, &y1), popcount8('A') + popcount8('B'));
    vt_free(t);
    /* size is clamped: 0 draws as 1, a huge size as 10 */
    t = rip_draw("!|c0F|Y00000000|@0A0AA\r\n");
    CHECK_INT(bbox_extent(t, 15, &x0, &y0, &x1, &y1), a);
    vt_free(t);
    t = rip_draw("!|c0F|Y000010ZZ|@0A0AA\r\n");
    CHECK_INT(bbox_extent(t, 15, &x0, &y0, &x1, &y1), 100 * a);
    vt_free(t);
}

static void viewport_clips_and_resets(void)
{
    vt_term *t;
    /* a bar 36 x 36 inside a 10 x 10 viewport: 100 pixels */
    t = rip_draw("!|v00000909|S010F|B00000Z0Z\r\n");
    CHECK_INT(rcount(t, 15), 100);
    CHECK_INT(rpx(t, 9, 9), 15);
    CHECK_INT(rpx(t, 10, 9), 0);
    CHECK_INT(rpx(t, 9, 10), 0);
    /* E erases the viewport only */
    h_put(t, "!|v00000Z0Z|S0107|B00000Z0Z|v00000909|E\r\n");
    CHECK_INT(rcount(t, 7), 36 * 36 - 100);
    /* the corners swap and clamp */
    h_put(t, "!|v0909000 0\r\n");
    vt_free(t);
    t = rip_draw("!|v09090000|S010F|B00000Z0Z\r\n");
    CHECK_INT(rcount(t, 15), 100);
    /* text clips to the viewport too */
    h_put(t, "!|*\r\n!|v00000303|c0E|Y00000200|@0000A\r\n");
    CHECK(rcount(t, 14) > 0 && rcount(t, 14) < 4 * popcount8('A'));
    /* * puts the viewport back to the whole canvas and clears it */
    h_put(t, "!|*\r\n!|S010F|B00000Z0Z\r\n");
    CHECK_INT(rcount(t, 15), 36 * 36);
    vt_free(t);
}

/* ---- RIP-3b: Hershey stroke fonts ---- */

static void stroke_fonts_goldens(void)
{
    int f, zi, d, nsum = 0;
    unsigned long seen[16];
    char cmd[128];
    vt_term *t;
    for (f = 1; f <= 4; f++)
        for (zi = 0; zi < 2; zi++)
            for (d = 0; d < 2; d++) {
                unsigned long sum, b;
                int k;
                t = rip_term();
                vt_set_rip_stroke(t, 1);
                stroke_fixture(cmd, sizeof cmd, f, d, zi + 1);
                h_put(t, cmd);
                sum = canvas_sum(t);
                if (getenv("RIP_PRINT_GOLD"))
                    printf("GOLD %d %d %d %lu\n", f, zi, d, sum);
                CHECK(rcount(t, 15) > 20);
                CHECK(sum == stroke_gold[f - 1][zi][d]);
                for (k = 0; k < nsum; k++)
                    CHECK(seen[k] != sum); /* style, size and direction each move the output */
                seen[nsum++] = sum;
                vt_free(t);
                /* option off (the default): the bitmap, whatever the style number */
                t = rip_term();
                stroke_fixture(cmd, sizeof cmd, f, d, zi + 1);
                h_put(t, cmd);
                b = canvas_sum(t);
                if (f == 1) {
                    if (getenv("RIP_PRINT_GOLD"))
                        printf("BGOLD %d %d %lu\n", zi, d, b);
                    CHECK(b == bitmap_gold[zi][d]);
                }
                CHECK(b != sum); /* the option moves the output */
                vt_free(t);
            }
    /* every style draws the same bitmap with the option off */
    {
        unsigned long a, b;
        t = rip_term();
        stroke_fixture(cmd, sizeof cmd, 1, 0, 1);
        h_put(t, cmd);
        a = canvas_sum(t);
        vt_free(t);
        t = rip_term();
        stroke_fixture(cmd, sizeof cmd, 4, 0, 1);
        h_put(t, cmd);
        b = canvas_sum(t);
        vt_free(t);
        CHECK(a == b);
    }
}

static void text_window_scrolls_text_not_canvas(void)
{
    vt_term *t = rip_term();
    vt_image_view v;
    char line[40];
    int i, k;
    long before;
    h_put(t, "\033[!" "!|S010C|B0A0A1414\r\n" "!|w0005270A10\r\n");
    before = rcount(t, 12);
    CHECK_INT(before, 961);
    for (i = 0; i < 40; i++) {
        snprintf(line, sizeof line, "line%d\r\n", i);
        h_put(t, line);
    }
    CHECK_STR(h_row(t, 5), "line35");
    CHECK_STR(h_row(t, 9), "line39");
    CHECK_STR(h_row(t, 4), ""); /* above the window: untouched */
    CHECK_STR(h_row(t, 11), "");
    CHECK_INT(rcount(t, 12), before);
    CHECK_INT(rpx(t, 10, 10), 12);
    CHECK_INT(vt_images(t), 1);
    /* every grid row still shows its own tile of the canvas */
    for (k = 0; k < 25; k++) {
        CHECK(vt_row_image(t, k, 0, &v));
        CHECK_INT(v.py, k * 14);
    }
    /* and text outside the window scrolls nothing: below it a line stays */
    h_put(t, "!|g0007\r\n");
    vt_free(t);
}

static void text_window_commands_and_wrap(void)
{
    vt_term *t = rip_term();
    h_put(t, "\033[!" "!|c0F|L00001E00\r\n" "!|w0003270700\r\n"); /* rows 3..7, wrap off */
    h_put(t, "ab\r\n");
    CHECK_STR(h_row(t, 3), "ab");
    h_put(t, "!|g0402\r\n" "X\r\n");
    CHECK(strstr(h_row(t, 5), "X") != 0);
    CHECK_INT(h_cell(t, 4, 5)->ch, 'X');
    h_put(t, "!|H\r\n" "Y");
    CHECK_INT(h_cell(t, 0, 3)->ch, 'Y'); /* H: home of the window */
    h_put(t, "!|g0001\r\n" "abcdef\r\n" "!|g0301\r\n" "!|>\r\n");
    CHECK_STR(h_row(t, 4), "abc"); /* > erases to the end of the line */
    h_put(t, "!|e\r\n");
    CHECK_STR(h_row(t, 3), "");
    CHECK_STR(h_row(t, 4), "");
    CHECK_INT(rpx(t, 10, 0), 15); /* e leaves the canvas */
    /* wrap off: a long line stays on one row; wrap on: it continues below */
    h_put(t, "!|g0000\r\n");
    h_put(t, "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789");
    CHECK(h_row(t, 4)[0] == 0);
    vt_free(t);
    t = rip_term();
    h_put(t, "\033[!" "!|w0003270710\r\n");
    h_put(t, "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\r\n");
    CHECK(h_row(t, 4)[0] != 0);
    /* * resets the window and the grid */
    h_put(t, "!|*\r\n" "z");
    CHECK_STR(h_screen(t), "z");
    vt_free(t);
}

/* ---- RIP-6: mouse fields and buttons ---- */

static void m2(char *p, int v)
{
    const char *d = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    p[0] = d[v / 36];
    p[1] = d[v % 36];
}

static void mouse_limit_overlap_click_key(void)
{
    vt_term *t = rip_term();
    vt_u8 out[VT_RIP_OUT_MAX];
    char cmd[64];
    int i;
    /* fixture 07: a button with a hotkey and two overlapping fields */
    h_put(t, "\033[!" "!|*\r\n" "!|1K\r\n"
             "!|1B1O0K010274030F080F080700010E07000000\r\n"
             "!|1U0K0K28141T00<>Alpha<>alpha^M\r\n"
             "!|1M002S2S4G3W1000000region^M\r\n"
             "!|1M013W3C5K4G1000000overlap^M\r\n");
    CHECK_INT(vt_rip_mouse_fields(t), 3);
    CHECK_INT(vt_rip_click(t, 50, 30, out), 6); /* the button's centre */
    CHECK(memcmp(out, "alpha\r", 6) == 0);
    CHECK_INT(vt_rip_click(t, 20, 20, out), 6); /* its corner, inclusive */
    CHECK_INT(vt_rip_click(t, 81, 30, out), 0); /* just outside */
    CHECK_INT(vt_rip_click(t, 110, 110, out), 7);
    CHECK(memcmp(out, "region\r", 7) == 0);
    CHECK_INT(vt_rip_click(t, 150, 130, out), 8); /* in both: the last defined wins */
    CHECK(memcmp(out, "overlap\r", 8) == 0);
    /* the hotkey (1T = 65 = 'A') returns the same bytes, in either case */
    CHECK_INT(vt_rip_key(t, 'A', out), 6);
    CHECK(memcmp(out, "alpha\r", 6) == 0);
    CHECK_INT(vt_rip_key(t, 'a', out), 6);
    CHECK_INT(vt_rip_key(t, 'Q', out), 0);
    /* a different cell size than the picture's: the click maps through the cell */
    vt_set_cell_pixels(t, 16, 28);
    CHECK_INT(vt_rip_click(t, 100, 60, out), 6);
    vt_set_cell_pixels(t, 8, 14);
    /* 1K kills them all */
    h_put(t, "!|1K\r\n");
    CHECK_INT(vt_rip_mouse_fields(t), 0);
    CHECK_INT(vt_rip_click(t, 50, 30, out), 0);
    CHECK_INT(vt_rip_key(t, 'A', out), 0);
    /* the 129th field is rejected */
    for (i = 0; i < 129; i++) {
        strcpy(cmd, "!|1M");
        m2(cmd + 4, 0);
        m2(cmd + 6, (i % 16) * 4);
        m2(cmd + 8, (i / 16) * 4);
        m2(cmd + 10, (i % 16) * 4 + 2);
        m2(cmd + 12, (i / 16) * 4 + 2);
        strcpy(cmd + 14, "1000000");
        cmd[21] = 'a' + (char)(i % 26);
        cmd[22] = 0;
        strcat(cmd, "\r\n");
        h_put(t, cmd);
    }
    CHECK_INT(vt_rip_mouse_fields(t), 128);
    CHECK_INT(vt_rip_click(t, 2, 2, out), 1); /* the first still answers */
    CHECK(out[0] == 'a');
    /* field 128 (i = 127) is at column 15, row 7 -> x 60, y 28 */
    CHECK_INT(vt_rip_click(t, 61, 29, out), 1);
    CHECK_INT(out[0], 'a' + 127 % 26);
    /* field 129 (i = 128) would be at column 0, row 8 -> x 0, y 32: nothing there */
    CHECK_INT(vt_rip_click(t, 1, 33, out), 0);
    vt_free(t);
    /* RIP off: no answer */
    t = h_new(80, 25, VT_XTERM);
    CHECK_INT(vt_rip_click(t, 50, 30, out), 0);
    CHECK_INT(vt_rip_key(t, 'A', out), 0);
    vt_free(t);
}

static void mouse_expansion(void)
{
    unsigned char out[64];
    int n;
    n = rip_expand((const unsigned char *)"a^Mb^[c^a^A", 11, out, 64);
    CHECK_INT(n, 7);
    CHECK(memcmp(out, "a\rb\033c\001\001", 7) == 0);
    /* a backquote and a $NAME$ variable are no-ops (no variable is defined) */
    n = rip_expand((const unsigned char *)"x$FOO$y`z", 9, out, 64);
    CHECK_INT(n, 3);
    CHECK(memcmp(out, "xyz", 3) == 0);
    /* a $ that closes nothing is kept, the output is capped */
    n = rip_expand((const unsigned char *)"a$b c", 5, out, 64);
    CHECK_INT(n, 5);
    n = rip_expand((const unsigned char *)"abcdef", 6, out, 3);
    CHECK_INT(n, 3);
}

/* the pixels of the Alpha button drawn with the given 1B fields (as one string of the fixture's shape) */
static vt_term *button_with(const char *b1)
{
    char cmd[200];
    vt_term *t = rip_term();
    snprintf(cmd, sizeof cmd, "\033[!" "!|*\r\n" "!|1B%s000000\r\n" "!|1U0K0K28141T00<>Alpha<>alpha^M\r\n", b1);
    h_put(t, cmd);
    return t;
}

static void button_knobs_move_output(void)
{
    vt_term *t;
    vt_u8 out[VT_RIP_OUT_MAX];
    long bright, dark, surf, label;
    /* wid hgt orient flags bev dfore dback bright dark surface grp flags2 uline corner */
    t = button_with("000001" "0000" "03" "0C" "08" "0E" "09" "0A" "00" "00" "00" "00");
    bright = rcount(t, 14);
    dark = rcount(t, 9);
    surf = rcount(t, 10);
    label = rcount(t, 12);
    CHECK(bright > 0 && dark > 0 && surf > 0 && label > 0);
    CHECK_INT(rpx(t, 20, 20), 14);   /* the top left corner is bright */
    CHECK_INT(rpx(t, 80, 40), 9);    /* the bottom right is dark */
    CHECK_INT(rpx(t, 24, 24), 10);   /* inside the bevel: the surface */
    CHECK_INT(rpx(t, 19, 20), 0);    /* outside: untouched */
    vt_free(t);
    /* bevel size 0: no bevel; 3 -> 1: fewer bevel pixels */
    t = button_with("000001" "0000" "00" "0C" "08" "0E" "09" "0A" "00" "00" "00" "00");
    CHECK_INT(rcount(t, 14), 0);
    CHECK_INT(rcount(t, 9), 0);
    CHECK(rcount(t, 10) > surf);
    vt_free(t);
    t = button_with("000001" "0000" "01" "0C" "08" "0E" "09" "0A" "00" "00" "00" "00");
    CHECK(rcount(t, 14) > 0 && rcount(t, 14) < bright);
    vt_free(t);
    /* each colour field moves its own pixels */
    t = button_with("000001" "0000" "03" "01" "08" "02" "03" "04" "00" "00" "00" "00");
    CHECK_INT(rcount(t, 12), 0);
    CHECK_INT(rcount(t, 1), label);
    CHECK_INT(rcount(t, 2), bright);
    CHECK_INT(rcount(t, 3), dark);
    CHECK(rcount(t, 4) >= surf);
    vt_free(t);
    /* wid and hgt override the button's own corner: 10 x 10 from (20,20) */
    t = button_with("0A0A01" "0000" "03" "0C" "08" "0E" "09" "0A" "00" "00" "00" "00");
    CHECK_INT(rpx(t, 30, 30), 9);
    CHECK_INT(rpx(t, 31, 30), 0);
    CHECK_INT(vt_rip_click(t, 25, 25, out), 6);
    CHECK_INT(vt_rip_click(t, 50, 30, out), 0);
    vt_free(t);
}

static void mouse_clear_flag(void)
{
    vt_term *t = rip_term();
    vt_u8 out[VT_RIP_OUT_MAX];
    h_put(t, "\033[!" "!|c0F|S010F|B0A0A1414\r\n" "!|w0005270A10\r\n" "!|1M000A0A1414" "1100000bye\r\n");
    CHECK_INT(rcount(t, 15), 31 * 31);
    CHECK_INT(vt_rip_click(t, 15, 15, out), 3);
    CHECK(memcmp(out, "bye", 3) == 0);
    CHECK_INT(rcount(t, 15), 0);        /* clr: the windows and canvas reset first */
    CHECK_INT(vt_rip_mouse_fields(t), 0);
    h_put(t, "z");
    CHECK(strstr(h_row(t, 0), "z") != 0); /* the text window is the whole grid again */
    vt_free(t);
    /* clr = 0 keeps the canvas and the fields */
    t = rip_term();
    h_put(t, "\033[!" "!|c0F|S010F|B0A0A1414\r\n" "!|1M000A0A1414" "1000000bye\r\n");
    CHECK_INT(vt_rip_click(t, 15, 15, out), 3);
    CHECK_INT(rcount(t, 15), 31 * 31);
    CHECK_INT(vt_rip_mouse_fields(t), 1);
    vt_free(t);
}

void suite_rip(void)
{
    fixture_01_line_grammar();
    fixture_02_continuation();
    fixture_03_escapes();
    text_lines_pass_through();
    malformed_commands_are_counted();
    level_and_pair_lists();
    palette_fixture_05();
    palette_commands_move_output();
    ega_master_levels();
    canvas_dirty_and_serial();
    canvas_placement();
    reach_rip_through_vt_write();
    auto_plain_text_is_rip_off();
    auto_query_turns_on();
    auto_valid_line_turns_on_and_draws();
    auto_bad_line_is_text();
    on_mode_is_active_at_once();
    detection_and_switch();
    decstr_still_resets();
    rip_off_is_byte_identical();
    canvas_lifecycle();
    palette_through_the_terminal();
    index_zero_is_a_colour_for_rip_not_for_sixel();
    knob_colour_and_write_mode();
    knob_line_style_and_thickness();
    knob_fill_style();
    knob_geometry();
    knob_polygon_rect_flood();
    text_font_scale_direction();
    stroke_fonts_goldens();
    viewport_clips_and_resets();
    text_window_scrolls_text_not_canvas();
    text_window_commands_and_wrap();
    mouse_limit_overlap_click_key();
    mouse_expansion();
    button_knobs_move_output();
    mouse_clear_flag();
    icons_and_clipboard();
    icon_names_that_climb_out_are_refused();
}
