/* The scrollback keeps its lines packed (W23, engine/vtengine.c "the
 * scrollback, packed"): a line that scrolls off is stored as its text and
 * style runs and decoded into cells again when read. These tests drive it
 * through the public API only -- vt_write, the scroll, vt_row -- and hold
 * the memory it saves with a bound that fails if it climbs. */
#include "harness.h"
#include "../render/vtinput.h"
#include <stdlib.h>

/* v in decimal at b; the bytes written (C89 has no snprintf) */
static int num(char *b, long v)
{
    char d[12];
    int n = 0, k = 0;
    do {
        d[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
        b[k++] = d[--n];
    return k;
}

/* Bytes one line of `cols` cells took before W23: the grid line itself,
 * moved into the ring (line_new: a header and cols cells). The host's
 * header is what vt_line is on the host (16 bytes); 68k has the same. */
#define OLD_LINE_BYTES(cols) (16L + (long)(cols) * 16L)

typedef struct {
    vt_cell c[300];
    int n, wrapped, marks;
} saved_row;

static void save_row(vt_term *t, int row, saved_row *s)
{
    const vt_cell *c = vt_row(t, row, &s->n);
    memcpy(s->c, c, (size_t)s->n * sizeof(vt_cell));
    s->wrapped = vt_row_wrapped(t, row);
    s->marks = vt_row_marks(t, row);
}

/* Scrollback row `row` is exactly the saved row: every cell, its flags,
 * and what vt_row_used promises about its tail. */
static int same_row(vt_term *t, int row, const saved_row *s)
{
    int n, x, used;
    const vt_cell *c = vt_row(t, row, &n);
    if (!c || n != s->n || vt_row_wrapped(t, row) != s->wrapped || vt_row_marks(t, row) != s->marks)
        return 0;
    for (x = 0; x < n; x++)
        if (memcmp(&c[x], &s->c[x], sizeof(vt_cell)))
            return 0;
    used = vt_row_used(t, row);
    for (x = used; x < n; x++)
        if (c[x].ch != ' ' || c[x].fg != VT_COLOR_DEFAULT || c[x].bg != VT_COLOR_DEFAULT || c[x].attr ||
            c[x].width != 1 || c[x].deco || c[x].ext || c[x].pad)
            return 0;
    return used >= 0 && used <= n;
}

/* Each case is written on the top row of a cleared screen, saved, then
 * scrolled off by newlines; the newest scrollback line must give back the
 * same cells. */
static const char *const cases[] = {
    "plain ascii text, the common case",
    "trailing blanks of the default style   ",
    "caf\xc3\xa9 \xc3\xbc" "ber \xc3\xb1",                       /* Latin-1: the two-byte code */
    "\033[1;3;4mbold italic under\033[0m plain \033[1mbold again",
    "\033[4:3mcurly\033[21mdouble \033[4:4mdots\033[4:5mdash\033[0m \033[53mover\033[73msup\033[0m"
    "\033[74msub\033[51mframe\033[52menc\033[0m",
    "\033[2mfaint\033[5mblink\033[6mrapid\033[7minv\033[8mconc\033[9mstrike\033[0m",
    "\033[60mideo\033[62mover\033[64mstress\033[0m",
    "\033[38;5;196;48;5;21m256\033[38;2;1;2;3;48;2;250;251;252mrgb\033[39mdflt\033[49mboth\033[0m",
    "\033[58;5;9;4munder colour\033[58;2;10;20;30mrgb ul\033[11mfont1\033[20mfraktur\033[0m",
    "\033]8;;http://example.com/a\033\\link\033]8;;\033\\ between \033]8;id=x;file:///b\033\\two\033]8;;\033\\",
    "\xe4\xb8\xad\xe6\x96\x87 wide \xe4\xb8\x8a",                  /* CJK: width 2 and its right half */
    "\xf0\x9f\x98\x80 grin \xf0\x9f\x91\x8d\xf0\x9f\x8f\xbd",      /* beyond the BMP: cluster entries */
    "e\xcc\x81 n\xcc\x83 combined",                                 /* combining marks: cluster entries */
    "\xe2\x94\x8c\xe2\x94\x80\xe2\x94\x90\xe2\x94\x82\xe2\x96\x88 box",  /* U+2500s: two-byte code */
    "\xee\x82\xa0 \xef\xbf\xbd private use",                        /* high BMP: three-byte code */
    "\033[44mblue\033[K",                                           /* BCE: a coloured tail */
    "\033[41m   \033[0m spaces with a colour are text",
    "\033#6double width",                                           /* DEC line size */
    "\033]133;A\033\\prompt$ \033]133;B\033\\cmd",                  /* OSC 133 marks */
    "\033[1m0123456789012345678901234567890123456789",              /* exactly full: no tail */
    "\033[7m\033[2J",                                               /* inverse erase */
    "",                                                             /* an empty line */
};

static void every_kind_of_cell_comes_back_from_the_scrollback(void)
{
    vt_term *t = h_new(40, 3, VT_XTERM);
    saved_row s;
    int i;
    for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        h_put(t, "\033[0m\033[H\033[2J\033[H"); /* ED 2 keeps the history as it is */
        h_put(t, cases[i]);
        save_row(t, 0, &s);
        h_put(t, "\033[0m\r\n\n\n"); /* row 0 leaves the top */
        h_checks++;
        if (!same_row(t, -1, &s)) {
            h_failures++;
            printf("  FAIL %s:%d: case %d did not come back whole\n", __FILE__, __LINE__, i);
        }
    }
    vt_free(t);
}

/* What the cells name -- a link, a cluster's code points, an underline
 * colour -- reads the same from a decoded scrollback line. */
static void links_clusters_and_styles_resolve_from_the_scrollback(void)
{
    vt_term *t = h_new(30, 3, VT_XTERM);
    const vt_cell *c;
    vt_u32 cp[VT_CLUSTER_CPS];
    int n;
    h_put(t, "\033]8;;http://example.com/x\033\\L\033]8;;\033\\ e\xcc\x81 \033[58;2;1;2;3;4mU\033[0m\r\n\n\n");
    c = vt_row(t, -1, &n);
    CHECK(c != 0);
    CHECK_STR(vt_cell_link(t, &c[0]) ? vt_cell_link(t, &c[0]) : "(none)", "http://example.com/x");
    CHECK(vt_cell_link(t, &c[1]) == 0);
    CHECK_INT(vt_cell_text(t, &c[2], cp), 2);
    CHECK_INT(cp[0], 'e');
    CHECK_INT(cp[1], 0x301);
    CHECK_INT(vt_cell_underline_color(t, &c[4]), VT_RGB(1, 2, 3));
    vt_free(t);
}

/* A wrapped line, a wide glyph that did not fit at the margin (the padding
 * blank before it), and a line wider than 255 cells (runs split at 255):
 * row flags and cells all come back. */
static void wraps_padding_and_long_runs_come_back(void)
{
    vt_term *t = h_new(10, 2, VT_XTERM);
    vt_term *w = h_new(300, 2, VT_XTERM);
    saved_row a, b;
    int i;
    h_put(t, "123456789\xe4\xb8\xad" "x"); /* the wide glyph wraps, a padding blank at column 9 */
    save_row(t, 0, &a);
    save_row(t, 1, &b);
    CHECK(a.wrapped);
    h_put(t, "\r\n\n");
    CHECK(same_row(t, -2, &a));
    CHECK(same_row(t, -1, &b));
    h_put(w, "\033[1m");
    for (i = 0; i < 300; i++)
        h_put(w, i % 7 ? "a" : "b");
    save_row(w, 0, &a);
    h_put(w, "\033[0m\r\n\n");
    CHECK(same_row(w, -1, &a));
    vt_free(t);
    vt_free(w);
}

/* The table sweeps read the packed lines: a style or cluster entry a
 * scrollback line names is kept (and renumbered with it) when the screen
 * runs out of entries. */
static void table_sweeps_keep_what_the_scrollback_names(void)
{
    vt_term *t = h_new(20, 3, VT_XTERM);
    const vt_cell *c;
    char buf[64];
    int i, n;
    h_put(t, "\033[58;5;1;4mA\033[58;5;2mB\033[0m\r\n\n\n"); /* two style entries, now in the history */
    for (i = 0; i < 600; i++) { /* entries made and dropped on screen: the table fills and is swept */
        int k = 0;
        memcpy(buf, "\033[H\033[58;2;", 11);
        k = 11;
        k += num(buf + k, i & 255);
        buf[k++] = ';';
        k += num(buf + k, i >> 8);
        memcpy(buf + k, ";7mx\033[0m", 9);
        h_put(t, buf);
    }
    c = vt_row(t, -1, &n);
    CHECK(c != 0);
    CHECK_INT(vt_cell_underline_color(t, &c[0]), 1);
    CHECK_INT(vt_cell_underline_color(t, &c[1]), 2);
    vt_free(t);
}

/* Reachability (the one test of the wiring): text written with vt_write,
 * scrolled off by the screen, read back with vt_row -- and the sentinel
 * that it went through the packed store: the scrollback holds exactly the
 * text's bytes as payload, no cells. */
static void a_scrolled_line_is_packed_and_read_back(void)
{
    vt_term *t = h_new(80, 4, VT_XTERM);
    int i;
    for (i = 0; i < 10; i++)
        h_put(t, "hello, packed scrollback\r\n");
    CHECK_INT(vt_scrollback_lines(t), 7);
    CHECK_STR(h_row(t, -1), "hello, packed scrollback");
    CHECK_STR(h_row(t, -7), "hello, packed scrollback");
    /* 24 characters a line and nothing else -- 2 bytes more (a run's count
     * and flags) on a line the reset drew whole, whose cells are not known
     * to be plain text alone */
    CHECK(vt_count_sb_payload(t) >= 7L * 24L && vt_count_sb_payload(t) <= 7L * 26L);
    vt_free(t);
}

/* What reads the history as text -- the window's Find, a selection's copy,
 * a double-click's word, a Ctrl + click's link -- reads the packed lines: a
 * styled line with wide, combined and linked text, wrapped over seven rows
 * (more than the VT_SBC lines the decode cache holds), is found across its
 * wraps, copied back to the byte, and its word and link resolve. */
#define SB_TEXT "red \xe4\xb8\xad\xe6\x96\x87 e\xcc\x81t\xc3\xa9 link needle-in-the-hay tail end of it and more words"

static void find_copy_and_word_read_the_packed_history(void)
{
    vt_term *t = h_new(10, 3, VT_XTERM);
    char buf[256];
    const vt_cell *c;
    int n, x0 = -1, x1 = -1, y0 = 0, y1 = 0;
    long top;
    h_put(t, "\033[31mred \xe4\xb8\xad\xe6\x96\x87 e\xcc\x81t\xc3\xa9 \033]8;;http://x/\033\\link\033]8;;\033\\ "
             "\033[1;44mneedle-in-the-hay\033[0m tail end of it and more words\r\n\n\n");
    CHECK_INT(vt_scrollback_lines(t), 7);
    top = -(long)vt_scrollback_lines(t);
    CHECK_INT(vt_find(t, "needle-in-the-hay", top), top); /* a match on a wrapped line: its first row */
    CHECK_INT(vt_find(t, "HAY TAIL END", top), top);      /* across a wrap, case folded */
    CHECK_INT(vt_find(t, "t\xc3\xa9 link", top), top);    /* Latin-1 in a styled run */
    vti_line(t, -4, &y0, &y1);                              /* triple-click: the whole line */
    CHECK_INT(y0, top);
    CHECK_INT(y1, -1);
    CHECK(vt_copy_text(t, 0, y0, 9, y1, buf, sizeof(buf)) > 0);
    CHECK_STR(buf, SB_TEXT);
    CHECK(vti_word(t, 4, -6, &x0, &x1));                   /* "t\xc3\xa9 link ne": the link */
    CHECK_INT(x0, 3);
    CHECK_INT(x1, 6);
    c = vt_row(t, -6, &n);
    CHECK(c != 0 && n == 10);
    CHECK_STR(c && vt_cell_link(t, &c[4]) ? vt_cell_link(t, &c[4]) : "(none)", "http://x/");
    vt_free(t);
}

static void feed_file(vt_term *t, const char *path, int times)
{
    static vt_u8 data[1 << 16];
    FILE *f = fopen(path, "rb");
    long len;
    int i;
    if (!f) {
        h_checks++;
        h_failures++;
        printf("  FAIL cannot open %s\n", path);
        return;
    }
    len = (long)fread(data, 1, sizeof(data), f);
    fclose(f);
    for (i = 0; i < times; i++)
        vt_write(t, data, len);
}

/* What the scrollback holds, measured (the live bytes it gives back when
 * cleared), printed against what the same lines took before W23. The 68k
 * figure is an estimate from the payload: a 14-byte header a line, the
 * record rounded to 4 (2 bytes on average), 12 bytes a 2048-byte block. */
static long measure(const char *what, vt_term *t)
{
    long lines = vt_scrollback_lines(t), payload = vt_count_sb_payload(t), held = vt_count_live;
    long old = lines * OLD_LINE_BYTES(vt_cols(t));
    long m68k = (payload + lines * 16L) * 2060L / 2048L;
    vt_clear_scrollback(t);
    held -= vt_count_live;
    printf("  %-24s %3ld lines: text+runs %6ld B, held %6ld B (68k ~%6ld B); before W23 %6ld B: %.1fx"
           " (68k ~%.1fx)\n", what, lines, payload, held, m68k, old, (double)old / (double)held,
           (double)old / (double)m68k);
    return held;
}

/* The bound (the sentinel): 500 lines of 78 characters of plain text, as
 * `cat` of a source file leaves them, held in under SB_PLAIN_BOUND bytes
 * on the host. Before W23 they took 500 x (16 + 80 x 16) = 648,000. */
#define SB_PLAIN_BOUND 60000L

static void plain_text_scrollback_stays_under_its_bound(void)
{
    vt_term *t = vt_new(80, 24, 500, 0, 0);
    char line[80];
    long held;
    int i, x;
    for (i = 0; i < 800; i++) {
        for (x = 0; x < 78; x++)
            line[x] = (char)('a' + (i + x) % 26);
        line[78] = '\r';
        line[79] = '\n';
        vt_write(t, (const vt_u8 *)line, 80);
    }
    CHECK_INT(vt_scrollback_lines(t), 500);
    CHECK_INT(vt_count_sb_payload(t), 500L * 78L); /* the characters alone: plain text costs no runs */
    held = measure("plain text, 78 columns", t);
    CHECK(held > 0);
    CHECK(held <= SB_PLAIN_BOUND);
    vt_free(t);
}

/* The other two the owner asked for, measured and printed; held to a
 * quarter of the cells at most. */
static void measured_ratios_for_real_output(void)
{
    static const struct { const char *name, *path; int times; } s[] = {
        { "coloured ls -l", "tests/streams/ls-color.80x24.bin", 60 },
        { "Claude Code session", "tests/streams/claude-session.80x24.bin", 40 },
    };
    int i;
    for (i = 0; i < 2; i++) {
        vt_term *t = vt_new(80, 24, 500, 0, 0);
        long lines;
        feed_file(t, s[i].path, s[i].times);
        lines = vt_scrollback_lines(t);
        CHECK(lines > 100);
        CHECK(measure(s[i].name, t) * 4 < lines * OLD_LINE_BYTES(80));
        vt_free(t);
    }
}

/* A reflow lays the history out again a packed line at a time: the
 * history is never all in cells at once (626 KB at 500 x 77), and the text
 * survives a narrow-and-back. */
static void reflow_streams_the_history(void)
{
    vt_term *t = vt_new(80, 24, 500, 0, 0);
    char line[200];
    long n0, n1, base, peak;
    char *a, *b;
    int i, x;
    vt_set_reflow(t, 1);
    for (i = 0; i < 600; i++) {
        int len = 20 + (i * 37) % 140; /* some lines wrap */
        int k = 0;
        line[k++] = '\033';
        line[k++] = '[';
        line[k++] = '3';
        line[k++] = (char)('0' + i % 8);
        line[k++] = 'm';
        for (x = 0; x < len; x++)
            line[k++] = (char)('A' + (i + x) % 26);
        line[k++] = '\r';
        line[k++] = '\n';
        vt_write(t, (const vt_u8 *)line, k);
    }
    n0 = vt_copy_text(t, 0, -vt_scrollback_lines(t), 79, 23, 0, 0);
    a = (char *)malloc((size_t)n0 + 1);
    vt_copy_text(t, 0, -vt_scrollback_lines(t), 79, 23, a, n0 + 1);
    base = vt_count_live;
    vt_count_peak = base;
    vt_resize(t, 61, 24);
    peak = vt_count_peak - base;
    printf("  reflow 80 -> 61 columns with %d history lines: %ld bytes at the peak above the window\n",
           vt_scrollback_lines(t), peak);
    CHECK(peak < 120000L); /* both histories packed, a few rows of cells -- not 500 rows of cells */
    vt_resize(t, 80, 24);
    n1 = vt_copy_text(t, 0, -vt_scrollback_lines(t), 79, 23, 0, 0);
    b = (char *)malloc((size_t)n1 + 1);
    vt_copy_text(t, 0, -vt_scrollback_lines(t), 79, 23, b, n1 + 1);
    /* the oldest lines may have gone (a narrower history is taller): the
     * newer text is the old text's tail */
    CHECK(n1 > 1000 && n1 <= n0);
    CHECK(n1 <= n0 && !memcmp(a + (n0 - n1), b, (size_t)n1));
    free(a);
    free(b);
    vt_free(t);
}

void suite_sbpack(void)
{
    every_kind_of_cell_comes_back_from_the_scrollback();
    links_clusters_and_styles_resolve_from_the_scrollback();
    wraps_padding_and_long_runs_come_back();
    table_sweeps_keep_what_the_scrollback_names();
    find_copy_and_word_read_the_packed_history();
    a_scrolled_line_is_packed_and_read_back();
    plain_text_scrollback_stays_under_its_bound();
    measured_ratios_for_real_output();
    reflow_streams_the_history();
}
