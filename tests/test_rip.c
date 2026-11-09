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
    detection_and_switch();
    decstr_still_resets();
    rip_off_is_byte_identical();
    canvas_lifecycle();
    palette_through_the_terminal();
    knob_colour_and_write_mode();
    knob_line_style_and_thickness();
    knob_fill_style();
    knob_geometry();
    knob_polygon_rect_flood();
}
