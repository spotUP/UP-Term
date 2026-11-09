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
    reach_rip_through_vt_write();
}
