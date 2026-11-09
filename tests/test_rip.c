/* RIPscrip: RIP-1 byte parser against the fixtures in tests/rip/ (goldens are the
 * .dump files beside them), and the RIP-3 canvas and palette. Run from the
 * repository root. */
#include <stdio.h>
#include <stdlib.h>
#include "harness.h"
#include "../engine/rip.h"

/* RIP-2 lands the vt adapter; until then the reachability test below is
 * expected-pending (it still drives vt_write and reads the sentinel). */
#define RIP2_LANDED 0

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

/* RIP-1 reachability, expected-pending until RIP-2: bytes written to a
 * terminal reach the RIP parser (sentinel: the commands it handed on). */
static void reach_rip_through_vt_write(void)
{
    vt_term *t = h_new(40, 10, VT_XTERM);
    vt_set_rip(t, 1);
    h_put(t, "!|c0F|L0A0A5050\r\n");
    h_put(t, "!|c0C|R0A0A5050\r\n");
    if (!RIP2_LANDED) {
        printf("  PENDING RIP-2: vt_write does not reach the parser (commands=%ld)\n", vt_rip_commands(t));
        return;
    }
    CHECK_INT(vt_rip_commands(t), 4);
    CHECK_STR(h_screen(t), "");
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
}
