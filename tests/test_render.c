/* The Amiga renderer's draw wiring on the host (ledger R8): render/amiga_render.c itself, compiled
 * against the NDK's structs and a fake graphics.library (tests/render_host/gfx_fake.c) that logs every
 * call and draws Text, RectFill and ScrollRaster into a pixel canvas. A real vt_term feeds it through
 * the callbacks vtwin.c gives the engine. The planar fast paths run on gf_planar's bitplanes (W-T4);
 * not covered: the DIRECT blit, vtwin's own frame loop.
 *
 * Its own binary (build/render_host, `make test ONLY=render`): the NDK headers and the engine's host
 * build cannot share a translation unit (struct timeval). */
#include "render_host/gfx_fake.h"
#include "../render/amiga_render.h"
#include "harness.h"
#include "../render/pen_match.h"

#define COLS 20
#define ROWS 8
#define CW 8
#define CH 8

static vr_render rn;
static vt_term *term;
/* open_term_in's screen: planar (gf_planar) of this depth with the window at screen x planar_left, y 10; 0: the
 * chunky screen */
static int planar_depth, planar_left;
/* open_term_in's colour map: gf_palette of these test_npal entries (0: the fake's default model) */
static const unsigned long *test_pal;
static int test_npal;

static void cb_damage(void *u, int x0, int y0, int x1, int y1)
{
    vr_damage((vr_render *)u, x0, y0, x1, y1);
}

static void cb_scroll(void *u, int top, int bot, int n)
{
    vr_scroll((vr_render *)u, top, bot, n);
}

/* vtwin_render's pass (render/vtwin.c): mask begin, cursor off, the engine's flush (composed off screen when a
 * ?2026 frame ended), cursor on, mask end */
static void pass(const char *s)
{
    vr_mask_begin(&rn);
    vr_cursor_off(&rn);
    vt_feed(term, (const vt_u8 *)s, (long)strlen(s));
    vr_compose_begin(&rn);
    vt_flush(term);
    vr_compose_end(&rn);
    vr_cursor_on(&rn);
    vr_mask_end(&rn);
}

/* a window of COLS x ROWS cells, `lines` lines "L00", "L01"... written live, the cursor on the
 * (empty) last row; the live top row shows line lines - ROWS + 1 */
static void open_term_in(int lines, enum vt_personality pers)
{
    vt_callbacks cb;
    char buf[16];
    int i;
    memset(&cb, 0, sizeof(cb));
    cb.damage = cb_damage;
    cb.scroll = cb_scroll;
    gf_open(COLS * CW, ROWS * CH, CW, CH, 6);
    if (planar_depth)
        gf_planar(planar_depth, planar_left, 10);
    if (test_npal)
        gf_palette(test_pal, test_npal);
    term = vt_new(COLS, ROWS, 100, &cb, &rn);
    vt_set_personality(term, pers);
    vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
    vr_redraw(&rn);
    for (i = 0; i < lines; i++) {
        snprintf(buf, sizeof(buf), "L%02d\r\n", i);
        pass(buf);
    }
    /* output that keeps coming scrolls ahead of the grid (jump scroll); vtwin settles it once the output
     * has been quiet (settle), and the next empty frame ends the step */
    vr_mask_begin(&rn);
    vr_cursor_off(&rn);
    vr_settle(&rn);
    vr_cursor_on(&rn);
    vr_mask_end(&rn);
    pass("");
}

static void open_term(int lines)
{
    open_term_in(lines, VT_XTERM);
}

static void close_term(void)
{
    vr_free(&rn);
    vt_free(term);
    gf_close();
}

/* what the window shows with the view `view` lines back, after open_term(lines) */
static const char *expect(int lines, int view)
{
    static char out[COLS * ROWS + ROWS + 1];
    char row[16];
    int y, n = 0, top = lines - (ROWS - 1) - view;
    for (y = 0; y < ROWS; y++) {
        if (top + y < lines)
            snprintf(row, sizeof(row), "L%02d", top + y);
        else
            row[0] = 0;
        n += snprintf(out + n, sizeof(out) - (size_t)n, "%-*s%s", COLS, row, y + 1 < ROWS ? "|" : "");
    }
    return out;
}

/* the calls since the last gf_clear_log drew no text outside the pixel rows [y0, y1) */
static int text_only_in(int y0, int y1)
{
    int i;
    for (i = 0; i < gf_n; i++)
        if (gf_log[i].kind == GF_TEXT && (gf_log[i].b < y0 || gf_log[i].b >= y1))
            return 0;
    return 1;
}

/* a rectangle fill that covers the whole window is a vr_redraw */
static int filled_whole_window(void)
{
    int i;
    for (i = 0; i < gf_n; i++)
        if (gf_log[i].kind == GF_RECT && gf_log[i].a <= 0 && gf_log[i].b <= 0 && gf_log[i].c >= COLS * CW - 1 &&
            gf_log[i].d >= ROWS * CH - 1)
            return 1;
    return 0;
}

/* S1: moving the scrollback view blits the text with ScrollRaster and draws only the rows it exposed. */
static void scrollback_view_move_blits_and_draws_only_the_exposed_rows(void)
{
    const gf_call *s;
    open_term(30);
    CHECK_STR(gf_screen(), expect(30, 0)); /* live: the harness shows what the grid holds */
    vr_set_view(&rn, 5);                   /* from live: a full redraw */
    CHECK_STR(gf_screen(), expect(30, 5));

    /* 5 -> 7: the text moves down two rows, rows 0 and 1 are new */
    gf_clear_log();
    vr_set_view(&rn, 7);
    CHECK_INT(gf_count(GF_SCROLL), 1);
    s = gf_find(GF_SCROLL, 0);
    CHECK(s != 0);
    if (s)
        CHECK_INT(s->b, -2 * CH);
    CHECK(text_only_in(0, 2 * CH));
    CHECK(gf_count(GF_TEXT) > 0);
    CHECK(!filled_whole_window());
    CHECK_STR(gf_screen(), expect(30, 7));

    /* 7 -> 4: up three rows, rows 5..7 are new */
    gf_clear_log();
    vr_set_view(&rn, 4);
    CHECK_INT(gf_count(GF_SCROLL), 1);
    s = gf_find(GF_SCROLL, 0);
    if (s)
        CHECK_INT(s->b, 3 * CH);
    CHECK(text_only_in(5 * CH, ROWS * CH));
    CHECK(!filled_whole_window());
    CHECK_STR(gf_screen(), expect(30, 4));

    /* back to live: a full redraw (the cursor is on the live screen), still the right rows */
    vr_set_view(&rn, 0);
    CHECK_STR(gf_screen(), expect(30, 0));
    close_term();
}

/* G5: with jump scroll the cursor's glyph is the grid row's (screen row + jump), at the cursor's screen cell. */
static void cursor_glyph_is_drawn_at_the_cursor_cell_from_the_grid_row(void)
{
    const gf_call *t, *f;
    open_term(12);
    pass("\r\n");   /* a pass that scrolls: the next pass moves further (jump_step) */
    vr_mask_begin(&rn);
    vr_cursor_off(&rn);
    vt_feed(term, (const vt_u8 *)"\r\nABCD\x1b[2D", 10); /* scrolls one row; the cursor is on the C */
    vt_flush(term);
    CHECK_INT(rn.jump, 2);                                  /* the screen is two rows ahead of the grid */
    gf_clear_log();
    vr_cursor_on(&rn);
    vr_mask_end(&rn);
    CHECK_INT(rn.cursor_x, 2);
    CHECK_INT(rn.cursor_y, ROWS - 1 - 2);                   /* grid row 7, screen row 5 */
    f = gf_find(GF_RECT, 0);
    t = gf_find(GF_TEXT, 0);
    CHECK_INT(gf_count(GF_TEXT), 1);
    CHECK(f != 0 && t != 0);
    if (f && t) {
        CHECK(f->a == 2 * CW && f->b == 5 * CH && f->c == 3 * CW - 1 && f->d == 6 * CH - 1); /* the cell filled */
        CHECK(t->a == 2 * CW && t->b == 5 * CH);                                               /* the glyph in it */
        CHECK_STR(t->str, "C");
    }
    close_term();
}

/* The pen a cell's background is drawn in (the text's Text() or the blank's fill) after `s`, in a window of that personality. */
static int bg_pen_after(enum vt_personality pers, const char *s)
{
    int pen;
    open_term_in(0, pers);
    pass(s);
    pen = gf_bgpen(0, 0);
    close_term();
    return pen;
}

/* nvim in a CON: window (the Amiga personality): "the background was blue but all typed text had grey". A
 * program that sends a direct colour (SGR 48;2, nvim does with TERM=vtcon) got the pen NUMBER of its blue byte
 * on a text cell, where a blank cell and every other personality draw the colour. */
static void direct_colour_background_in_the_amiga_personality_is_drawn_in_its_colour(void)
{
    const char *blue = "\x1b[48;2;0;0;170mhi";
    int want = bg_pen_after(VT_XTERM, blue);
    CHECK(want != 170);                                      /* the pen of the colour is not its blue byte */
    CHECK_INT(bg_pen_after(VT_AMIGA, blue), want);
    CHECK_INT(bg_pen_after(VT_AMIGA, "\x1b[48;2;0;0;1mhi"), bg_pen_after(VT_XTERM, "\x1b[48;2;0;0;1mhi"));
    CHECK(bg_pen_after(VT_AMIGA, "\x1b[48;2;0;0;1mhi") != 1); /* not pen 1 either */
    /* and the Amiga personality's own colours stay screen pens */
    CHECK_INT(bg_pen_after(VT_AMIGA, "\x1b[44mhi"), 4);
    CHECK_INT(bg_pen_after(VT_AMIGA, "\x1b[48;5;9mhi"), 9);
}

/* NewShell in a CON: window on Workbench (owner, 1.6 on a Replay): "still grey". The window ground is
 * light grey on black, but the Amiga personality drew its default background, and the global background a
 * Shell prompt sets with CSI >0m, as screen pen 0: the Workbench grey. Pen 0 is the window's background and
 * pen 1 its text, so both draw with the window's ground; pens 2-7 stay screen pens. */
static void con_window_draws_pen_zero_with_its_own_ground(void)
{
    int pen;
    open_term_in(0, VT_AMIGA);
    vr_set_defaults(&rn, 0xC0C0C0UL, 0x000000UL);
    CHECK(rn.pen_default_bg != 0); /* the ground is a pen of its own, not the Workbench grey */
    pass("hi");
    CHECK_INT(gf_bgpen(0, 0), (int)rn.pen_default_bg);
    pass("\r\n\x9b>00m\x9b" "01;00m1.SYS:> ");
    CHECK_INT(gf_bgpen(0, 1), (int)rn.pen_default_bg);
    CHECK_INT(gf_bgpen(COLS - 1, 1), (int)rn.pen_default_bg);
    pass("\r\n\x9b" "40mA\x9b" "41mB\x9b" "42mC");
    CHECK_INT(gf_bgpen(0, 2), (int)rn.pen_default_bg);
    CHECK_INT(gf_bgpen(1, 2), (int)rn.pen_default_fg);
    CHECK_INT(gf_bgpen(2, 2), 2);
    pen = gf_bgpen(0, 2);
    CHECK(pen != 0);
    close_term();
}

/* ---- S3: the fill paths (S2's FillPixelArray, and the cells joined into one fill) ---- */

#define UPPER "\xe2\x96\x80"
#define PAINT "\x1b[38;5;196;48;5;21m"
#define OTHER "\x1b[38;5;46;48;5;226m"

static int snap[COLS * CW * ROWS * CH];

static void snapshot(void)
{
    int x, y;
    for (y = 0; y < ROWS * CH; y++)
        for (x = 0; x < COLS * CW; x++)
            snap[y * COLS * CW + x] = gf_pixel(x, y);
}

/* the fills a pass of `s` makes: FillPixelArray calls on a true-colour screen (the cursor is a RectFill),
 * else RectFills less the cursor's own (what an empty pass makes) */
static int fills_of(const char *s)
{
    int base, n;
    gf_clear_log();
    pass("");
    base = gf_count(GF_RECT);
    gf_clear_log();
    pass(s);
    n = rn.cgx ? gf_count(GF_CGXFILL) : gf_count(GF_RECT) - base;
    return n;
}

static int same_as_snapshot(void)
{
    int x, y;
    for (y = 0; y < ROWS * CH; y++)
        for (x = 0; x < COLS * CW; x++)
            if (snap[y * COLS * CW + x] != gf_pixel(x, y))
                return 0;
    return 1;
}

/* the row drawn in one pass, and the same cells one pass each (a lone cell is never joined) */
static void row_joined_equals_row_cell_by_cell(int truecolor, int *fills_joined, int *fills_alone)
{
    static const char *const cells[] = { PAINT UPPER, PAINT UPPER, PAINT UPPER, OTHER UPPER, OTHER UPPER, PAINT UPPER };
    char all[256] = "";
    int i;
    open_term_in(0, VT_XTERM);
    if (truecolor) {
        gf_truecolor(1);
        vr_free(&rn);
        vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
        vr_redraw(&rn);
    }
    for (i = 0; i < 6; i++)
        strcat(all, cells[i]);
    *fills_joined = fills_of(all);
    snapshot();
    close_term();
    open_term_in(0, VT_XTERM);
    if (truecolor) {
        gf_truecolor(1);
        vr_free(&rn);
        vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
        vr_redraw(&rn);
    }
    *fills_alone = 0;
    for (i = 0; i < 6; i++) {
        char at[16];
        snprintf(at, sizeof(at), "\x1b[1;%dH", i + 1);
        pass(at);
        *fills_alone += fills_of(cells[i]);
    }
    CHECK(same_as_snapshot());
    close_term();
}

/* UPDemo's plasma: ~22 ms a frame of per-cell RectFill. Neighbouring half blocks of one look are one pair of fills
 * (the background, the band): 3 runs here are 6 fills, not 12, and the pixels are those of cell by cell. */
static void neighbouring_half_blocks_of_one_colour_are_one_fill_with_the_same_pixels(void)
{
    int joined, alone;
    row_joined_equals_row_cell_by_cell(0, &joined, &alone);
    CHECK_INT(joined, 6);
    CHECK_INT(alone, 12);
}

static void the_same_joining_on_a_true_colour_screen(void)
{
    int joined, alone;
    row_joined_equals_row_cell_by_cell(1, &joined, &alone);
    CHECK_INT(joined, 6);
    CHECK_INT(alone, 12);
}

/* S2: a direct colour on a true-colour screen with no pen to spare is filled by FillPixelArray (cybergraphics
 * LVO -150), not by a scratch pen reloaded with SetRGB32 (84 us a call on the 68060: 70-80 % of UPDemo's
 * colour scenes). The fake's OpenLibrary used to return 0, so this path was never reached. */
static void direct_colours_on_a_true_colour_screen_are_filled_by_fillpixelarray(void)
{
    open_term_in(0, VT_XTERM);
    gf_truecolor(1);
    vr_free(&rn);
    vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
    vr_redraw(&rn);
    CHECK(rn.cgx != 0);
    gf_clear_log();
    pass("\x1b[48;2;200;100;50m   \x1b[48;2;1;2;3m  ");
    CHECK(gf_count(GF_CGXFILL) >= 2);
    CHECK_INT(gf_count(GF_SETRGB), 0);
    CHECK_INT(gf_pixel(0, 0), GF_CGX | 0xC86432);
    CHECK_INT(gf_pixel(3 * CW - 1, CH - 1), GF_CGX | 0xC86432);
    CHECK_INT(gf_pixel(3 * CW, 0), GF_CGX | 0x010203);
    CHECK_INT(gf_pixel(5 * CW, 0) & GF_CGX, 0); /* the rest of the row is not that colour */
    /* a half block in two direct colours: both parts */
    gf_clear_log();
    pass("\x1b[2;1H\x1b[38;2;9;8;7;48;2;70;80;90m" UPPER);
    CHECK_INT(gf_count(GF_SETRGB), 0);
    CHECK_INT(gf_pixel(0, CH), GF_CGX | 0x090807);
    CHECK_INT(gf_pixel(0, CH + CH - 1), GF_CGX | 0x46505A);
    close_term();
    /* and on a palette screen it is not used */
    open_term_in(0, VT_XTERM);
    gf_clear_log();
    pass("\x1b[48;2;200;100;50m   ");
    CHECK_INT(gf_count(GF_CGXFILL), 0);
    CHECK(gf_count(GF_RECT) > 0);
    close_term();
}

/* W-T13 (and W-T11, the colours of rig 2's split-screen scene): a 24-bit Workbench whose colour map is used up
 * (Directory Opus and the Workbench's icons hold every entry: ObtainPen finds no scratch pen, ObtainBestPen
 * exact finds none) drew every window in the nearest of the 32 pens that are left (colour 240 and 48;2;88;88;88
 * purple). An RTG screen shows a colour without a pen: the cells' colours are FillPixelArray's, exact; only the
 * glyphs take the nearest pen, over a background filled first (not JAM2, which would put the nearest pen round
 * every glyph). */
static void direct_colours_are_exact_on_a_true_colour_screen_whose_colour_map_is_used_up(void)
{
    open_term_in(0, VT_XTERM);
    gf_truecolor(2);
    vr_free(&rn);
    vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
    vr_redraw(&rn);
    CHECK(rn.truecolor != 0); /* the RTG path ran: no scratch pen, FillPixelArray's colour */
    CHECK(rn.cgx != 0);
    CHECK(rn.scratch[0] < 0 && rn.scratch[1] < 0);
    gf_clear_log();
    pass("\x1b[48;2;200;100;50m   \x1b[48;5;240m  \x1b[48;2;88;88;88m ");
    CHECK_INT(gf_count(GF_CGXFILL), 2); /* colour 240 is #585858: it and the direct 88;88;88 are one fill */
    CHECK_INT(gf_count(GF_SETRGB), 0);
    CHECK_INT(gf_pixel(0, 0), GF_CGX | 0xC86432);
    CHECK_INT(gf_pixel(3 * CW - 1, CH - 1), GF_CGX | 0xC86432);
    CHECK_INT(gf_pixel(3 * CW, 0), GF_CGX | 0x585858); /* colour 240 */
    CHECK_INT(gf_pixel(5 * CW, 0), GF_CGX | 0x585858);
    CHECK_INT(gf_pixel(6 * CW - 1, CH - 1), GF_CGX | 0x585858);
    CHECK_INT(gf_pixel(7 * CW, 0) & GF_CGX, 0);
    /* text in a direct colour: no pen holds it, so the run is drawn in a bitmap of its own and written through a
     * two-colour table (WriteLUTPixelArray) -- the glyph cells the text colour, the others the cell's background.
     * The fake draws a character as a solid cell. */
    gf_clear_log();
    pass("\x1b[2;1H\x1b[38;2;255;175;255;48;2;70;80;90mA B\x1b[m");
    CHECK_INT(gf_count(GF_CGXLUT), 1);
    CHECK_INT(gf_count(GF_CGXFILL), 0);
    CHECK_INT(gf_count(GF_SETRGB), 0);
    if (gf_find(GF_CGXLUT, 0)) {
        CHECK_INT(gf_find(GF_CGXLUT, 0)->c, 3 * CW);
        CHECK_INT(gf_find(GF_CGXLUT, 0)->d, CH);
    }
    CHECK_INT(gf_pixel(0, CH), GF_CGX | 0xFFAFFF);
    CHECK_INT(gf_pixel(CW - 1, CH + CH - 1), GF_CGX | 0xFFAFFF);
    CHECK_INT(gf_pixel(CW, CH), GF_CGX | 0x46505A);
    CHECK_INT(gf_pixel(2 * CW, CH + CH - 1), GF_CGX | 0xFFAFFF);
    /* a palette colour on the window's own background: the pen's colour from the colour map is the other entry */
    gf_clear_log();
    pass("\x1b[3;1H\x1b[38;5;240mx\x1b[m");
    CHECK_INT(gf_count(GF_CGXLUT), 1);
    CHECK_INT(gf_pixel(0, 2 * CH), GF_CGX | 0x585858);
    /* a box-drawing cell (a rule line, the tmux status line's pane border) in a direct colour: the same */
    gf_clear_log();
    pass("\x1b[5;1H\x1b[38;5;34m\xe2\x94\x80\x1b[m");
    CHECK_INT(gf_count(GF_CGXLUT), 1);
    CHECK_INT(gf_pixel(CW / 2, 4 * CH + CH / 2), GF_CGX | 0x00AF00); /* the arm's middle row, colour 34 */
    CHECK_INT(gf_pixel(CW / 2, 4 * CH) & GF_CGX, GF_CGX);          /* the cell's other pixels: the background */
    /* the 16 ANSI colours are direct colours here too: no pen of the colour map to take */
    gf_clear_log();
    pass("\x1b[6;1H\x1b[42m  \x1b[m");
    CHECK_INT(gf_count(GF_CGXFILL), 1);
    CHECK_INT(gf_pixel(0, 5 * CH), GF_CGX | (int)vt_palette_rgb(term, 2));
    /* a glyph in a pen colour (the default colours) is still drawn through the pens */
    gf_clear_log();
    pass("\x1b[4;1H\x1b[48;2;1;2;3mx\x1b[m");
    CHECK_INT(gf_count(GF_CGXLUT), 0);
    CHECK_INT(gf_count(GF_CGXFILL), 1); /* the background, filled first; the glyph in the pen */
    close_term();
}

/* W-T3: a frame of UPDemo's effects (half blocks in 256 colours, every row) */
static const char *effect_frame(int phase)
{
    static char buf[8192];
    int x, y, n = 0;
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "\x1b[H");
    for (y = 0; y < ROWS - 1; y++)
        for (x = 0; x < COLS; x++)
            n += snprintf(buf + n, sizeof(buf) - (size_t)n, "\x1b[%d;%dH\x1b[38;5;%d;48;5;%dm" UPPER, y + 1, x + 1,
                          16 + (x + y + phase) % 200, 16 + (x * 3 + y + phase) % 200);
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "\x1b[m");
    return buf;
}

static char framebuf[8300];

static const char *synced(const char *s)
{
    snprintf(framebuf, sizeof(framebuf), "\x1b[?2026h%s\x1b[?2026l", s);
    return framebuf;
}

/* W-T3 (owner: UPDemo's fire, plasma and rotozoomer visibly redraw row by row): a frame the program ends with ?2026
 * reset is drawn off screen and reaches the window in one blit -- nothing of it shows while it is drawn -- with the
 * pixels it has drawn in place. */
static void a_synchronized_frame_reaches_the_window_in_one_blit_after_it_is_drawn(void)
{
    int composed, x, y, same;
    /* the frame drawn in place: the reference pixels */
    open_term_in(0, VT_XTERM);
    pass("\x1b[?25l");
    pass(effect_frame(0));
    pass(effect_frame(7));
    snapshot();
    close_term();
    open_term_in(0, VT_XTERM);
    pass("\x1b[?25l");
    pass(synced(effect_frame(0)));
    vr_mask_begin(&rn);
    vr_cursor_off(&rn);
    vt_feed(term, (const vt_u8 *)synced(effect_frame(7)), (long)strlen(framebuf));
    gf_clear_log();
    composed = vr_compose_begin(&rn);
    CHECK(composed);
    vt_flush(term);
    /* drawn, not shown: the window still has the frame before */
    same = 1;
    for (y = 0; y < (ROWS - 1) * CH; y++)
        for (x = 0; x < COLS * CW; x++)
            if (gf_pixel(x, y) != snap[y * COLS * CW + x])
                same = 0;
    CHECK(!same);
    CHECK_INT(gf_count(GF_BLT), 0);
    if (composed)
        vr_compose_end(&rn);
    vr_cursor_on(&rn);
    vr_mask_end(&rn);
    CHECK_INT(gf_count(GF_BLT), 1); /* every row of the frame, one blit */
    CHECK(gf_count(GF_RECT) + gf_count(GF_CGXFILL) > 0);
    CHECK(same_as_snapshot());
    close_term();
}

/* Output with no ?2026 frame in it is drawn in place, as before: no back bitmap, no blit. */
static void output_without_a_synchronized_frame_is_drawn_in_place(void)
{
    open_term_in(0, VT_XTERM);
    gf_clear_log();
    pass(effect_frame(3));
    pass("text");
    CHECK_INT(gf_count(GF_ALLOC), 0);
    CHECK_INT(gf_count(GF_BLT), 0);
    /* nor a frame still open (the hold is vtwin's; this pass only must not compose it) */
    pass("\x1b[?2026hmore");
    CHECK_INT(gf_count(GF_BLT), 0);
    pass("end\x1b[?2026l");
    CHECK(gf_count(GF_BLT) > 0); /* that frame ended now: composed */
    close_term();
}

/* A frame that scrolls (text, newlines at the bottom, more text) comes out as drawn in place: what was composed
 * before the scroll reaches the window first, the scroll moves it there. The same with no memory for the back
 * bitmap: drawn in place. */
static void a_synchronized_frame_that_scrolls_shows_what_in_place_drawing_shows(void)
{
    static const char frame[] = "\x1b[8;1Haaaa\r\nbbbb\r\ncccc\x1b[1;5H" PAINT UPPER UPPER "\x1b[mdd";
    int k;
    for (k = 0; k < 2; k++) {
        open_term_in(12, VT_XTERM);
        pass(frame);
        snapshot();
        close_term();
        open_term_in(12, VT_XTERM);
        gf_no_back = k;
        gf_clear_log();
        pass(synced(frame));
        CHECK_INT(gf_count(GF_BLT) > 0, !k);
        CHECK(same_as_snapshot());
        close_term();
    }
}

/* C1 (scidemo-lite on the A1200: a page-down came in two steps, the second a quarter second later): each
 * ?2026 frame the program ends is shown as the grid has it. Page-downs one after another scrolled ahead of the
 * grid (jump scroll, its step doubling while passes keep scrolling) and left blank rows at the bottom for vtwin's
 * settle to blit down later; a jump left by output before the frame is settled in it too. */
static void a_synchronized_frame_that_scrolls_is_not_left_for_settle(void)
{
    int k;
    open_term(12);
    pass("\x1b[?25l");         /* no cursor: a pass draws it, vr_redraw does not */
    for (k = 0; k < 4; k++) {   /* a page-down: the screen scrolls by three rows, then the new rows */
        pass(synced("\x1b[10;1H\r\n\r\n\r\nPgDn"));
        CHECK_INT(rn.jump, 0);
        snapshot();
        vr_redraw(&rn);          /* the grid, drawn in full */
        CHECK(same_as_snapshot());
    }
    pass("\r\n");               /* plain output that scrolls, pass after pass: jump scroll */
    pass("\r\n");
    CHECK(rn.jump > 0);
    pass(synced("\r\nEnd"));
    CHECK_INT(rn.jump, 0);
    snapshot();
    vr_redraw(&rn);
    CHECK(same_as_snapshot());
    close_term();
}

static int texts_of(char ch)
{
    int i, n = 0;
    for (i = 0; i < gf_n; i++)
        n += gf_log[i].kind == GF_TEXT && strchr(gf_log[i].str, ch) != 0; /* (both due: one run) */
    return n;
}

/* W-T2: blinking in time, not in frames. A slow (SGR 5) and a rapid (SGR 6) cell over 3 s of frames 20 ms apart
 * and 160 ms apart (the frame clock follows what a frame costs, pace.h): the slow one changes 6 times either way
 * (500 ms a phase), the rapid one every 150 ms (one change a frame at most). Counted in frames (every 10th and 3rd),
 * the rates moved with the clock: 15 and 50 changes at 20 ms, 1 and 6 at 160 ms. */
static void slow_and_rapid_blink_keep_their_rates_whatever_the_frame_interval(void)
{
    static const unsigned long step[] = { 20000UL, 160000UL };
    int k;
    for (k = 0; k < 2; k++) {
        int slow = 0, fast = 0, f, frames = (int)(3040000UL / step[k]);
        BYTE so, fo;
        open_term_in(0, VT_XTERM);
        pass("\x1b[?25l\x1b[5mS\x1b[6mR\x1b[m");
        CHECK(rn.has_blink);
        so = rn.blink_slow_off;
        fo = rn.blink_fast_off;
        gf_clear_log();
        for (f = 0; f < frames; f++) {
            vr_blink_tick(&rn, step[k]);
            slow += rn.blink_slow_off != so;
            fast += rn.blink_fast_off != fo;
            so = rn.blink_slow_off;
            fo = rn.blink_fast_off;
        }
        CHECK_INT(slow, 6);
        CHECK(fast >= 19 && fast <= 20);
        /* and the window: each cell drawn again at each change of its own rate */
        CHECK_INT(texts_of('S'), slow);
        CHECK_INT(texts_of('R'), fast);
        close_term();
    }
}

/* ---- W-T4: a planar (AGA) window's synchronized frame ---- */

/* An effect frame with text: half blocks over the top rows, lower half and full blocks and blanks between them, a
 * run of plain text (the painter) and a row of a colour a character (the planar cell path where the text is on a
 * byte boundary) */
static const char *planar_frame(int phase, const char *extra)
{
    static char buf[8300];
    snprintf(buf, sizeof(buf),
             "%s\x1b[6;3H\x1b[38;5;%d;48;5;50m\xe2\x96\x84\xe2\x96\x88  \xe2\x96\x84\x1b[44m \xe2\x96\x80"
             "\x1b[7;1Hplain text %d\x1b[8;1H\x1b[31ma\x1b[32mb\x1b[33mc\x1b[34md\x1b[35me\x1b[m%s",
             effect_frame(phase), 100 + phase, phase, extra);
    return buf;
}

/* the frames drawn in place on a planar screen of `depth` planes, the window at screen x `left`: the reference */
static void planar_reference(int depth, int left, const char *extra)
{
    planar_depth = depth;
    planar_left = left;
    open_term_in(0, VT_XTERM);
    pass("\x1b[?25l");
    pass(planar_frame(0, ""));
    pass(planar_frame(7, extra));
    snapshot();
    close_term();
}

/* W-T4 (owner: "AGA is important"; UPDemo's effects swept down an AGA window row by row, 20-54 ms a frame on the
 * 68060): on a planar screen the frame is composed by the CPU in planes of its own -- nothing reaches the window
 * while it is drawn, no RectFill -- and reaches the window in one copy, the pixels those of the frame drawn in
 * place; at any bit phase of the window, 4 and 8 planes. Before W-T4 vr_compose_begin refused planar windows. */
static void a_planar_synchronized_frame_reaches_the_window_in_one_copy_with_the_in_place_pixels(void)
{
    static const int depths[] = { 4, 8 }, lefts[] = { 0, 4, 13 };
    static int before[COLS * CW * ROWS * CH];
    int d, l, x, y, composed, same;
    unsigned long copies;
    for (d = 0; d < 2; d++)
        for (l = 0; l < 3; l++) {
            planar_reference(depths[d], lefts[l], "");
            open_term_in(0, VT_XTERM);
            pass("\x1b[?25l");
            pass(synced(planar_frame(0, "")));
            for (y = 0; y < ROWS * CH; y++)
                for (x = 0; x < COLS * CW; x++)
                    before[y * COLS * CW + x] = gf_pixel(x, y);
            vr_mask_begin(&rn);
            vr_cursor_off(&rn);
            vt_feed(term, (const vt_u8 *)synced(planar_frame(7, "")), (long)strlen(framebuf));
            gf_clear_log();
            copies = rn.n_copies;
            composed = vr_compose_begin(&rn);
            CHECK(composed);
            vt_flush(term);
            same = 1;
            for (y = 0; y < ROWS * CH; y++)
                for (x = 0; x < COLS * CW; x++)
                    if (gf_pixel(x, y) != before[y * COLS * CW + x])
                        same = 0;
            CHECK(same);                     /* drawn, not shown */
            CHECK_INT(gf_count(GF_RECT), 0); /* the CPU drew it, not the blitter */
            CHECK_INT(gf_count(GF_TEXT), 0);
            if (composed)
                vr_compose_end(&rn);
            vr_cursor_on(&rn);
            vr_mask_end(&rn);
            CHECK_INT(rn.n_copies - copies, 1);
            CHECK(same_as_snapshot());
            close_term();
        }
    planar_depth = 0;
}

/* The copy goes behind the beam: with the beam on the window's rows it waits for the next frame's top (WaitTOF)
 * first, so every row is shown new in the same frame; with the beam below them or well above, it copies at once. */
static void a_planar_frame_is_copied_once_the_beam_has_left_its_rows(void)
{
    static const long beam[] = { 44 + 10 + 20, 300, 20 };
    static const int waits[] = { 1, 0, 0 };
    int k;
    planar_depth = 4;
    planar_left = 0;
    for (k = 0; k < 3; k++) {
        open_term_in(0, VT_XTERM);
        pass("\x1b[?25l");
        gf_beam = beam[k];
        gf_clear_log();
        rn.n_copies = 0;
        pass(synced(planar_frame(k, "")));
        CHECK_INT(gf_count(GF_TOF), waits[k]);
        CHECK_INT(rn.n_copies, 1);
        close_term();
    }
    planar_depth = 0;
}

/* What only graphics.library draws (box drawing: lines) in a frame, or another window over this one: that frame
 * is drawn in place, through the RastPort and its layer -- the pixels the same as in place. */
static void a_planar_frame_with_lines_or_a_covered_window_is_drawn_in_place(void)
{
    static const char box[] = "\x1b[3;3H\xe2\x94\x80\xe2\x94\x82\x1b[m";
    int k;
    for (k = 0; k < 2; k++) {
        planar_reference(4, 4, k ? "" : box);
        open_term_in(0, VT_XTERM);
        pass("\x1b[?25l");
        pass(synced(planar_frame(0, "")));
        gf_cover(k);
        rn.n_copies = 0;
        gf_clear_log();
        pass(synced(planar_frame(7, k ? "" : box)));
        CHECK_INT(rn.n_copies, 0);
        CHECK(gf_count(GF_RECT) > 0);
        gf_cover(0);
        CHECK(same_as_snapshot());
        close_term();
    }
    planar_depth = 0;
}

/* upkg browse on an AGA window (owner, 1.4): the list came up over the banner it replaced -- the banner's pixels
 * stayed in every blank cell. The list's frame clears the alternate screen and starts with a box corner: the planar
 * pass, begun over the whole grid, counted the window blank while its cells went to the planes only, gave up at the
 * corner (graphics.library draws it) and drew the frame again in place -- skipping the blank cells as if the window
 * were blank already. In place, the screen shows only the new frame. */
static void a_planar_frame_drawn_in_place_after_all_clears_the_screen_it_replaced(void)
{
    static const char frame[] = "\x1b[?1049h\x1b[?2026h\x1b[H\x1b[2J\x1b[36m\xe2\x95\x94\xe2\x95\x90\x1b[m\x1b[5;3Hlist\x1b[?2026l";
    char full[ROWS * (COLS + 2) + 1];
    int y;
    planar_depth = 4;
    planar_left = 4;
    open_term_in(0, VT_XTERM);
    pass("\x1b[?25l");
    pass(frame);
    snapshot();
    close_term();
    open_term_in(0, VT_XTERM);
    pass("\x1b[?25l");
    full[0] = 0;
    for (y = 0; y < ROWS; y++) {
        memset(full + strlen(full), 'B', COLS);
        full[(y + 1) * COLS + y * 2] = 0;
        strcat(full, y < ROWS - 1 ? "\r\n" : "");
    }
    pass(full); /* the banner: every cell drawn */
    rn.n_copies = 0;
    pass(frame);
    CHECK_INT(rn.n_copies, 0); /* drawn in place: the corner needs graphics.library */
    CHECK(same_as_snapshot());
    close_term();
    planar_depth = 0;
}


/* ---- W-T7: 256 colours on a screen of few pens ---- */

/* rig 1's 16-colour Workbench (AGA, 4 planes) as read 2026-10-13: pen 6 is the grey #d2d2e1 */
static const unsigned long wb16[16] = { 0xaaaaaa, 0x000000, 0xffffff, 0x6688bb, 0x00ff9f, 0xf0f0ff, 0xd2d2e1, 0xebebeb,
                                        0x4473ff, 0xffff00, 0x7f007f, 0xefaf4f, 0xee4444, 0x55dd55, 0x0044dd, 0xee9900 };

static unsigned long cube_rgb(int i)
{
    static const int lv[6] = { 0, 95, 135, 175, 215, 255 };
    i -= 16;
    return ((unsigned long)lv[i / 36] << 16) | ((unsigned long)lv[(i / 6) % 6] << 8) | (unsigned long)lv[i % 6];
}

/* the entry the screen's own nearest gives (the fake's ObtainBestPen: plain RGB distance), the match before W-T7 */
static int old_pen(unsigned long t)
{
    int i, k = 0;
    long bd = -1;
    for (i = 0; i < 16; i++) {
        long dr = (long)((wb16[i] >> 16) & 255) - (long)((t >> 16) & 255);
        long dg = (long)((wb16[i] >> 8) & 255) - (long)((t >> 8) & 255), db = (long)(wb16[i] & 255) - (long)(t & 255);
        long d = dr * dr + dg * dg + db * db;
        if (bd < 0 || d < bd) {
            bd = d;
            k = i;
        }
    }
    return k;
}

/* the cube colours the old match drew in the grey pen */
static int grey_colours(int *out)
{
    int i, n = 0;
    for (i = 16; i < 232; i++)
        if (old_pen(cube_rgb(i)) == 6)
            out[n++] = i;
    return n;
}

static double lin_of(int c)
{
    double v = c / 255.0;
    double t = (v + 0.055) / 1.055, p = 1;
    int k;
    if (v <= 0.04045)
        return v / 12.92;
    /* t^2.4 = t^2 * t^0.4, t^0.4 by Newton on x^5 = t^2 */
    {
        double x = t, y = t * t;
        for (k = 0; k < 40; k++)
            x = x - (x * x * x * x * x - y) / (5 * x * x * x * x);
        p = t * t * x;
    }
    return p;
}

static double cbrt_n(double v)
{
    double x = v > 1 ? v : 1;
    int k;
    if (v <= 0)
        return 0;
    for (k = 0; k < 60; k++)
        x = x - (x * x * x - v) / (3 * x * x);
    return x;
}

/* a colour in linear light (r, g, b) as OKLab (B. Ottosson, 2020) */
static void oklab(const double *c, double *lab)
{
    double l = cbrt_n(0.4122214708 * c[0] + 0.5363325363 * c[1] + 0.0514459929 * c[2]);
    double m = cbrt_n(0.2119034982 * c[0] + 0.6806995451 * c[1] + 0.1073969566 * c[2]);
    double s = cbrt_n(0.0883024619 * c[0] + 0.2817188376 * c[1] + 0.6299787005 * c[2]);
    lab[0] = 0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s;
    lab[1] = 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s;
    lab[2] = 0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s;
}

/* how far (OKLab) a cell's pixels, averaged as light, look from colour t; *grey: its pixels in pen 6 */
static double cell_error(int col, int row, unsigned long t, int *grey)
{
    double sum[3] = { 0, 0, 0 }, want[3], a[3], b[3];
    int x, y, k, n = 0;
    for (y = 0; y < CH; y++)
        for (x = 0; x < CW; x++) {
            int pen = gf_pixel(col * CW + x, row * CH + y);
            *grey += pen == 6;
            for (k = 0; k < 3; k++)
                sum[k] += lin_of((int)((wb16[pen & 15] >> (16 - 8 * k)) & 255));
            n++;
        }
    for (k = 0; k < 3; k++) {
        sum[k] /= n;
        want[k] = lin_of((int)((t >> (16 - 8 * k)) & 255));
    }
    oklab(sum, a);
    oklab(want, b);
    return (a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]);
}

/* the pen of the first glyph pixel of cell (col, row) that is not the background's */
static int text_pen(int col, int row, int bg)
{
    int x, y, pen;
    for (y = 0; y < CH; y++)
        for (x = 0; x < CW; x++)
            if ((pen = gf_pixel(col * CW + x, row * CH + y)) != bg)
                return pen;
    return bg;
}

/* W-T7 (owner, 2026-10-13: "grey" -- 26-31 % of UPDemo's rotozoomer, 24-26 % of its plasma in pen 6 of a
 * 16-colour Workbench): the light cube colours whose nearest pen by plain distance is the grey #d2d2e1. Drawn as
 * backgrounds they are now mixes of two pens, at most a quarter of their pixels grey (before: every one); drawn
 * as text, a saturated one (chroma 120 or more) takes a pen of its hue, never the grey; and the window gives back
 * every pen it held. */
/* pm_match on a palette made ready once (pm_palette: linear light and each pair's distance worked out ahead) picks
 * what the match before it picked from the bare palette, for all 240 xterm colours past the first 16: the hashes
 * are the old code's (render/pen_match.h at 182b22ed). The palette switched on every call is rebuilt each time,
 * never matched on the one before. */
static void pen_matches_on_a_prepared_palette_are_the_bare_palettes_picks(void)
{
    static pm_palette pp;
    static const int lv[6] = { 0, 95, 135, 175, 215, 255 };
    unsigned long alt[16], h = 5381, h2 = 5381, t;
    pm_pick p;
    int i;
    for (i = 0; i < 16; i++)
        alt[i] = wb16[i];
    alt[6] = 0xc02020;
    alt[9] = 0x2020c0;
    for (i = 16; i < 256; i++) {
        t = i >= 232 ? (unsigned long)(8 + 10 * (i - 232)) * 0x010101
                     : ((unsigned long)lv[(i - 16) / 36] << 16) | ((unsigned long)lv[(i - 16) / 6 % 6] << 8) |
                           (unsigned long)lv[(i - 16) % 6];
        pm_palette_set(&pp, wb16, 16);
        pm_match(&pp, 0xFFFF, t, 1, &p);
        h = (h * 33 ^ (unsigned long)(p.solid * 1000000 + p.a * 10000 + p.b * 100 + p.lev)) & 0xffffffffUL;
        pm_palette_set(&pp, alt, 16);
        pm_match(&pp, 0xFFFF, t, 1, &p);
        h2 = (h2 * 33 ^ (unsigned long)(p.solid * 1000000 + p.a * 10000 + p.b * 100 + p.lev)) & 0xffffffffUL;
    }
    CHECK(h == 0x04827ca3UL);
    CHECK(h2 == 0x6d3453c1UL);
    /* and the level a pick names is that many pixels of the 8 x 8 tile in pen b */
    for (i = 0, t = 0; i < 8; i++) {
        unsigned char v = pm_pattern(p.lev, i);
        for (; v; v &= (unsigned char)(v - 1))
            t++;
    }
    CHECK_INT((int)t, p.lev);
}

static void light_cube_colours_on_a_16_colour_workbench_are_not_drawn_in_its_grey_pen(void)
{
    int cols[64], n = grey_colours(cols), i, grey = 0, sat = 0;
    char buf[64];
    CHECK(n >= 20);
    planar_depth = 4;
    planar_left = 0;
    test_pal = wb16;
    test_npal = 16;
    open_term_in(0, VT_XTERM);
    pass("\x1b[?25l\x1b[2J");
    for (i = 0; i < n; i++) {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH\x1b[48;5;%dm \x1b[m", 1 + i / COLS, 1 + i % COLS, cols[i]);
        pass(buf);
    }
    for (i = 0; i < n; i++) {
        int g = 0;
        cell_error(i % COLS, i / COLS, cube_rgb(cols[i]), &g);
        grey += g;
    }
    CHECK(grey * 4 <= n * CW * CH);
    pass("\x1b[2J\x1b[H");
    for (i = 0; i < n; i++) {
        unsigned long t = cube_rgb(cols[i]);
        int r = (int)(t >> 16), g = (int)((t >> 8) & 255), b = (int)(t & 255);
        int mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
        if (mx - mn < 120)
            continue;
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH\x1b[38;5;%dmW\x1b[m", 1 + i / COLS, 1 + i % COLS, cols[i]);
        pass(buf);
        CHECK(text_pen(i % COLS, i / COLS, 0) != 6);
        sat++;
    }
    CHECK(sat >= 8);
    close_term();
    CHECK_INT(gf_pens_held, 0);
    planar_depth = 0;
    test_npal = 0;
}

/* the cube colours 16.. as backgrounds with a W in the next colour, cell by cell, on a 16-colour screen of
 * palette pal; a hash of the pixels they came out as */
extern ULONG vr_pick_matches; /* amiga_render.c */

static unsigned long paint_cube_cells(const unsigned long *pal, int n)
{
    char buf[64];
    unsigned long h = 2166136261UL;
    int i, x, y;
    planar_depth = 4;
    planar_left = 0;
    test_pal = pal;
    test_npal = 16;
    open_term_in(0, VT_XTERM);
    pass("\x1b[?25l\x1b[2J");
    for (i = 0; i < n; i++) {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH\x1b[48;5;%dm\x1b[38;5;%dmW\x1b[m", 1 + i / COLS, 1 + i % COLS, 16 + i,
                 16 + (i + 1) % 216);
        pass(buf);
    }
    for (i = 0; i < n; i++)
        for (y = 0; y < CH; y++)
            for (x = 0; x < CW; x++)
                h = (h ^ (unsigned long)gf_pixel((i % COLS) * CW + x, (i / COLS) * CH + y)) * 16777619UL & 0xffffffffUL;
    close_term();
    CHECK_INT(gf_pens_held, 0);
    planar_depth = 0;
    test_npal = 0;
    return h;
}

/* The match of a colour to the pens of a 16-colour screen (about 24 ms on a 68020) is made once per palette for
 * the whole program: a second window on the same palette draws the same pixels without matching anything again;
 * another palette matches anew. */
static void a_second_window_on_the_same_palette_matches_no_colour_again(void)
{
    static unsigned long pal_a[16], pal_b[16];
    unsigned long h1, h2, m0, m1;
    int i;
    for (i = 0; i < 16; i++)
        pal_a[i] = pal_b[i] = wb16[i];
    pal_a[6] = 0x305070; /* a palette no other test paints on: the first window starts with nothing matched */
    pal_b[6] = 0x703050;
    m0 = vr_pick_matches;
    h1 = paint_cube_cells(pal_a, 60);
    m1 = vr_pick_matches;
    CHECK(m1 - m0 >= 20);
    h2 = paint_cube_cells(pal_a, 60);
    CHECK_INT((int)(vr_pick_matches - m1), 0);
    CHECK(h1 == h2);
    m1 = vr_pick_matches;
    paint_cube_cells(pal_b, 60);
    CHECK(vr_pick_matches - m1 >= 20);
}

/* The same cells: each one's pixels, averaged as light, closer to its colour than the single grey pen they were
 * drawn in before -- by OKLab, a perceptual distance the renderer does not use itself: on the whole at most
 * half the old error, and nearly every cell closer on its own. */
static void a_dithered_cell_shows_its_colour_closer_than_the_old_single_pen(void)
{
    int cols[64], n = grey_colours(cols), i, closer = 0, g = 0;
    double now = 0, before = 0, e, o;
    char buf[64];
    double old_lin[3], lab_o[3], lab_t[3], want[3];
    int k;
    planar_depth = 4;
    planar_left = 0;
    test_pal = wb16;
    test_npal = 16;
    open_term_in(0, VT_XTERM);
    pass("\x1b[?25l\x1b[2J");
    for (i = 0; i < n; i++) {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH\x1b[48;5;%dm \x1b[m", 1 + i / COLS, 1 + i % COLS, cols[i]);
        pass(buf);
    }
    for (i = 0; i < n; i++) {
        unsigned long t = cube_rgb(cols[i]);
        e = cell_error(i % COLS, i / COLS, t, &g);
        for (k = 0; k < 3; k++) {
            old_lin[k] = lin_of((int)((wb16[6] >> (16 - 8 * k)) & 255));
            want[k] = lin_of((int)((t >> (16 - 8 * k)) & 255));
        }
        oklab(old_lin, lab_o);
        oklab(want, lab_t);
        o = (lab_o[0] - lab_t[0]) * (lab_o[0] - lab_t[0]) + (lab_o[1] - lab_t[1]) * (lab_o[1] - lab_t[1]) +
            (lab_o[2] - lab_t[2]) * (lab_o[2] - lab_t[2]);
        now += e;
        before += o;
        closer += e < o;
    }
    CHECK(now * 2 <= before);
    CHECK(closer * 5 >= n * 4);
    close_term();
    planar_depth = 0;
    test_npal = 0;
}

/* A frame of dithered half blocks, blanks, full blocks and a quadrant (the generic fill) */
static const char *dither_frame(int phase)
{
    static char buf[4096];
    int n = 0, y, x;
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "\x1b[H");
    for (y = 0; y < 5; y++)
        for (x = 0; x < COLS; x++) {
            int a = 147 + (x + y + phase) % 72, b = 153 + (x * 3 + phase) % 66;
            n += snprintf(buf + n, sizeof(buf) - (size_t)n, "\x1b[%d;%dH\x1b[38;5;%d;48;5;%dm%s", y + 1, x + 1, a, b,
                          (x + y) % 4 == 0 ? " " : (x + y) % 4 == 1 ? "\xe2\x96\x88" : "\xe2\x96\x80");
        }
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "\x1b[7;2H\x1b[38;5;219;48;5;117m\xe2\x96\x98\xe2\x96\x9f\x1b[m");
    return buf;
}

/* The fast path keeps its pixels: a synchronized frame of dithered cells composed by the CPU in its planes (no
 * RectFill, no template, one copy) shows what the same frame drawn in place shows, pixel for pixel, at any bit
 * phase of the window. */
static void a_dithered_frame_composed_and_drawn_in_place_has_the_same_pixels(void)
{
    static const int lefts[] = { 0, 4, 13 };
    int l, composed;
    unsigned long copies;
    test_pal = wb16;
    test_npal = 16;
    for (l = 0; l < 3; l++) {
        planar_depth = 4;
        planar_left = lefts[l];
        open_term_in(0, VT_XTERM);
        pass("\x1b[?25l");
        pass(dither_frame(0));
        pass(dither_frame(5));
        CHECK(gf_count(GF_OTHER) > 0); /* in place: the pattern's templates */
        snapshot();
        close_term();
        open_term_in(0, VT_XTERM);
        pass("\x1b[?25l");
        pass(synced(dither_frame(0)));
        vr_mask_begin(&rn);
        vr_cursor_off(&rn);
        vt_feed(term, (const vt_u8 *)synced(dither_frame(5)), (long)strlen(framebuf));
        gf_clear_log();
        copies = rn.n_copies;
        composed = vr_compose_begin(&rn);
        CHECK(composed);
        vt_flush(term);
        CHECK_INT(gf_count(GF_RECT), 0);
        CHECK_INT(gf_count(GF_OTHER), 0);
        if (composed)
            vr_compose_end(&rn);
        vr_cursor_on(&rn);
        vr_mask_end(&rn);
        CHECK_INT(rn.n_copies - copies, 1);
        CHECK(same_as_snapshot());
        close_term();
    }
    planar_depth = 0;
    test_npal = 0;
}

/* 256 pens (an 8-plane screen) and true colour are as before: no dither, each cell one pen -- the screen's own
 * nearest on the full map of 256. */
static void screens_of_more_than_16_pens_are_not_dithered(void)
{
    int cols[64], n = grey_colours(cols), i, x, y, uniform = 1;
    char buf[64];
    planar_depth = 8;
    planar_left = 0;
    test_pal = wb16;
    test_npal = 16;
    open_term_in(0, VT_XTERM);
    pass("\x1b[?25l\x1b[2J");
    gf_clear_log();
    for (i = 0; i < n; i++) {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH\x1b[48;5;%dm \x1b[m", 1 + i / COLS, 1 + i % COLS, cols[i]);
        pass(buf);
    }
    CHECK_INT(gf_count(GF_OTHER), 0); /* no pattern templates */
    for (i = 0; i < n; i++)
        for (y = 0; y < CH; y++)
            for (x = 0; x < CW; x++)
                if (gf_pixel((i % COLS) * CW + x, (i / COLS) * CH + y) != 6)
                    uniform = 0;
    CHECK(uniform);
    close_term();
    planar_depth = 0;
    test_npal = 0;
    open_term(0);
    gf_truecolor(1);
    vr_free(&rn);
    vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
    vr_redraw(&rn);
    gf_clear_log();
    pass("\x1b[48;5;219m \x1b[38;2;255;175;255m\xe2\x96\x88\x1b[m");
    CHECK_INT(gf_count(GF_OTHER), 0);
    CHECK(gf_count(GF_CGXFILL) >= 2); /* the colours themselves */
    close_term();
}

/* Neighbouring colours that share a pen (on 16 pens 226 #ffff00 and 190 #d7ff00 are both pen 9) as an upper half
 * block: the whole cell in that pen. The rule that keeps text visible on its own background's pen gave the block's
 * top half the default text pen -- a quarter of UPDemo's plasma on the rig's Workbench in its grey (W-T7). */
static void a_half_block_of_two_colours_sharing_a_pen_is_drawn_in_that_pen(void)
{
    int x, y, other = 0, k;
    planar_depth = 4;
    test_pal = wb16;
    test_npal = 16;
    for (k = 0; k < 2; k++) {
        planar_left = 0;
        open_term_in(0, VT_XTERM);
        pass("\x1b[?25l\x1b[2J");
        pass(k ? synced("\x1b[2;2H\x1b[38;5;226;48;5;190m\xe2\x96\x80\x1b[m")
               : "\x1b[2;2H\x1b[38;5;226;48;5;190m\xe2\x96\x80\x1b[m");
        for (y = 0; y < CH; y++)
            for (x = 0; x < CW; x++)
                other += gf_pixel(CW + x, CH + y) != 9;
        close_term();
    }
    CHECK_INT(other, 0);
    planar_depth = 0;
    test_npal = 0;
}

/* ---- W-T10: nothing drawn over the window's borders ---- */

/* W-T10 (owner: "something black draws over the right shell window border"; rig 1, UPDemo's headings): a window
 * with borders whose inner width is not a whole number of cells -- 21 columns and 5 pixels over -- shows double-size
 * rows (whose last cell is half in the grid: the engine gives such a row (21 + 1) / 2 cells), the cursor on that half
 * cell, half blocks, dithered colours and text up to the last column; drawn in place and as a synchronized frame, on
 * a chunky screen and a planar one at two bit phases. No pixel of the borders may change: the double-size row's last
 * cell was filled and drawn 2 cells wide, 3 pixels past the inner box into the right border. */
static void drawing_never_touches_the_window_borders_at_an_odd_inner_width(void)
{
    static const char frame[] =
        "\x1b[H\x1b#6ABCDEFGHIJK"
        "\x1b[2;1H\x1b#3Heading line\x1b[3;1H\x1b#4Heading line"
        "\x1b[4;1H\x1b[38;5;196;48;5;21m" UPPER UPPER UPPER UPPER UPPER UPPER UPPER UPPER UPPER UPPER UPPER UPPER
        UPPER UPPER UPPER UPPER UPPER UPPER UPPER UPPER UPPER
        "\x1b[5;1H\x1b[48;5;172m                     \x1b[m"
        "\x1b[6;1H\x1b[4mtext to the last colum\x1b[m"
        "\x1b[7;1H\x1b#6\x1b[44mblue double row\x1b[m";
    static const int depths[] = { 0, 4, 4 }, lefts[] = { 0, 0, 3 };
    vt_callbacks cb;
    int m, k;
    for (m = 0; m < 3; m++)
        for (k = 0; k < 2; k++) {
            memset(&cb, 0, sizeof(cb));
            cb.damage = cb_damage;
            cb.scroll = cb_scroll;
            /* borders 4, 11, 18, 2; inside 173 x 67: 21 x 8 cells and 5 x 3 pixels over */
            gf_open(4 + 173 + 18, 11 + 67 + 2, CW, CH, 6);
            if (depths[m]) {
                gf_planar(depths[m], lefts[m], 10);
                gf_palette(wb16, 16);
            }
            gf_borders(4, 11, 18, 2);
            term = vt_new(21, 8, 100, &cb, &rn);
            vt_set_personality(term, VT_XTERM);
            vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
            vr_redraw(&rn);
            pass(k ? synced(frame) : frame);
            pass(k ? synced("\x1b[1;11H") : "\x1b[1;11H"); /* the cursor on the double-size row's half cell */
            pass("\x1b[7;11H");
            CHECK_INT(gf_border_hits(), 0);
            close_term();
        }
}

/* ---- W-T12: the window has one background ---- */

/* vtwin's colours callback (render/vtwin.c cb_colors): a program changed the palette or the defaults (OSC 4, 10, 11) */
static void cb_colors(void *u)
{
    vr_palette_changed((vr_render *)u);
    vr_set_defaults((vr_render *)u, vt_default_color(term, 0), vt_default_color(term, 1));
}

/* Pixels of the window's inside that do not show `bg`: the strips beside the grid (above, left, right of the last
 * whole cell, below the last whole row), and the cells holding default blanks past each row's used width (not the
 * cursor's). */
static int off_background(int bl, int bt, int iw, int ih, int bg)
{
    int x, y, n = 0, cx, cy;
    vt_cursor(term, &cx, &cy);
    for (y = 0; y < ih; y++)
        for (x = 0; x < iw; x++) {
            int col = x / CW, row = y / CH, ncells;
            if (col < vt_cols(term) && row < vt_rows(term)) {
                const vt_cell *c = vt_row(term, row, &ncells);
                if (col < vt_row_used(term, row) || (c && col < ncells && c[col].bg != VT_COLOR_DEFAULT) ||
                    (col == cx && row == cy))
                    continue;
            }
            n += gf_bg(bl + x, bt + y) != bg;
        }
    return n;
}

static void feed(const char *s)
{
    vt_feed(term, (const vt_u8 *)s, (long)strlen(s));
}

/* W-T12 (owner: "the grey bg is visible around the white bg at some window sizes", "it looks like we paint over the
 * bg"): a window whose inside is not a whole number of cells, its default background changed by OSC 11 (white on a
 * window opened black), then cleared, a region scrolled, a line erased; then reverse video, a region scroll in it
 * and a redraw (what a resize and a refresh do); an Amiga-dialect window given a global background pen. Every pixel
 * of the inside outside the cells, and every default blank cell, shows the window's background -- in place and as a
 * synchronized frame, on a chunky screen and a planar one. On old code a true-colour window kept the strips and the
 * blank cells in the old background (the default blank's look was cached before OSC 11 was read), and a redraw
 * filled the window with the default background pen whatever reverse video or the global background said. */
static void the_strips_beside_the_grid_and_blank_cells_show_the_window_background(void)
{
    static const int depths[] = { 0, 4 };
    static const char *const steps[] = {
        "\x1b]11;#ffffff\x07\x1b[2J\x1b[Hheader",
        "\x1b[3;6r\x1b[6;1H\nA\nB\nC\x1b[r",
        "\x1b[7;1H\x1b[44mblue\x1b[K\x1b[m\x1b[8;1Hx\x1b[K",
        "\x1b[?5h",
        "\x1b[2;5r\x1b[5;1H\nD\nE\x1b[r",
    };
    vt_callbacks cb;
    int m, k, i;
    for (m = 0; m < 2; m++)
        for (k = 0; k < 3; k++) {
            memset(&cb, 0, sizeof(cb));
            cb.damage = cb_damage;
            cb.scroll = cb_scroll;
            cb.colors = cb_colors;
            /* borders 4, 11, 18, 2; inside 173 x 67: 21 x 8 cells and 5 x 3 pixels over */
            gf_open(4 + 173 + 18, 11 + 67 + 2, CW, CH, 6);
            if (depths[m]) {
                gf_planar(depths[m], 0, 10);
                gf_palette(wb16, 16);
            }
            gf_borders(4, 11, 18, 2);
            term = vt_new(21, 8, 100, &cb, &rn);
            vt_set_personality(term, VT_XTERM);
            vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
            /* vtwin's settings(): the window's own defaults (an XCON: window: light grey on black) */
            vr_set_defaults(&rn, 0xC0C0C0UL, 0x000000UL);
            vt_set_default_colors(term, 0xC0C0C0UL, 0x000000UL, 0xC0C0C0UL);
            vr_redraw(&rn);
            pass("hello\r\nworld");
            CHECK_INT(off_background(4, 11, 173, 67, (int)rn.pen_default_bg), 0);
            for (i = 0; i < 5; i++) {
                if (k == 2) {
                    pass(steps[i]); /* read inside the pass: OSC 11 after the cursor went off */
                } else {
                    /* as vtwin has it: the write read first (the colours callback at its end), drawn at the next
                     * frame, which takes the cursor off first */
                    feed(k ? synced(steps[i]) : steps[i]);
                    pass("");
                }
                CHECK_INT(off_background(4, 11, 173, 67, (int)(i < 3 ? rn.pen_default_bg : rn.pen_default_fg)), 0);
            }
            vr_redraw(&rn); /* vtwin_resize, vtwin_refresh */
            CHECK_INT(off_background(4, 11, 173, 67, (int)rn.pen_default_fg), 0);
            CHECK_INT(gf_border_hits(), 0);
            close_term();
            /* the Amiga dialect: the window's background is the global background pen (SGR >3), a form feed clears */
            gf_open(4 + 173 + 18, 11 + 67 + 2, CW, CH, 6);
            if (depths[m]) {
                gf_planar(depths[m], 0, 10);
                gf_palette(wb16, 16);
            }
            gf_borders(4, 11, 18, 2);
            term = vt_new(21, 8, 100, &cb, &rn);
            vt_set_personality(term, VT_AMIGA);
            vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
            vr_redraw(&rn);
            pass(k == 1 ? synced("\x1b[>3m\x0chello") : "\x1b[>3m\x0chello");
            CHECK_INT(off_background(4, 11, 173, 67, 3), 0);
            pass(k == 1 ? synced("\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\nscrolled") : "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\nscrolled");
            CHECK_INT(off_background(4, 11, 173, 67, 3), 0);
            vr_redraw(&rn);
            CHECK_INT(off_background(4, 11, 173, 67, 3), 0);
            CHECK_INT(gf_border_hits(), 0);
            close_term();
        }
}

/* ---- W-T9: the dither is a setting ---- */

/* the cells of the grey cube colours, 0 uniform or 1 mixed of two or more pens */
static int mixed_cells(int n)
{
    int i, x, y, mixed = 0;
    for (i = 0; i < n; i++) {
        int first = gf_pixel((i % COLS) * CW, (i / COLS) * CH), diff = 0;
        for (y = 0; y < CH; y++)
            for (x = 0; x < CW; x++)
                diff |= gf_pixel((i % COLS) * CW + x, (i / COLS) * CH + y) != first;
        mixed += diff;
    }
    return mixed;
}

/* W-T9 (owner 2026-10-14: "make it an option"): vr_set_dither(0), as the profile's dither = off and /dither off
 * reach it through vtwin's settings(), makes every colour on a 16-pen screen one pen again -- no pattern is drawn --
 * while a half block of two colours sharing a pen still keeps that pen (W-T7's same-pen rule); the setting turned
 * back on, with the same redraw a live change gets, dithers the cells again. */
static void dither_off_draws_the_nearest_single_pen_and_on_dithers_again(void)
{
    int cols[64], n = grey_colours(cols), i, x, y, other = 0;
    char buf[64];
    planar_depth = 4;
    planar_left = 0;
    test_pal = wb16;
    test_npal = 16;
    open_term_in(0, VT_XTERM);
    CHECK_INT(rn.dither_on, 1); /* the default */
    pass("\x1b[?25l\x1b[2J");
    for (i = 0; i < n; i++) {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH\x1b[48;5;%dm \x1b[m", 1 + i / COLS, 1 + i % COLS, cols[i]);
        pass(buf);
    }
    CHECK(mixed_cells(n) * 2 >= n); /* on: most of them two pens */
    vr_set_dither(&rn, 0);
    vr_redraw(&rn);
    gf_clear_log();
    vr_redraw(&rn);
    CHECK_INT(gf_count(GF_OTHER), 0); /* no pattern template */
    CHECK_INT(mixed_cells(n), 0);     /* off: each cell one pen */
    pass("\x1b[2;2H\x1b[38;5;226;48;5;190m\xe2\x96\x80\x1b[m");
    for (y = 0; y < CH; y++)
        for (x = 0; x < CW; x++)
            other |= gf_pixel(CW + x, CH + y) != 9;
    CHECK_INT(other, 0); /* the block's two colours share pen 9: all of the cell */
    vr_set_dither(&rn, 1);
    vr_redraw(&rn);
    CHECK(mixed_cells(n) * 2 >= n);
    close_term();
    CHECK_INT(gf_pens_held, 0);
    planar_depth = 0;
    test_npal = 0;
}

/* ---- W-T14: padding colour ---- */

/* The window's inside is 173 x 67 pixels under borders 4, 11, 18, 2: 21 x 8 cells of 8 x 8 and strips 5 pixels
 * right and 3 below; lx, ly shift the grid (an Amiga console's offsets) so there is a strip left of and above it. */
#define PAD_BL 4
#define PAD_BT 11
#define PAD_IW 173
#define PAD_IH 67

/* what the strip pixel (cx, cy) -- relative to the grid's corner, outside it -- should show: the pixel of the edge
 * cell next to it at the same place in the cell (a pattern of two pens continues past the grid, anchored to the cells) */
static int pad_expect(int lx, int ly, int cx, int cy)
{
    int col = cx < 0 ? 0 : cx / CW, row = cy < 0 ? 0 : cy / CH;
    int ox = ((cx % CW) + CW) % CW, oy = ((cy % CH) + CH) % CH;
    col = col > 20 ? 20 : col;
    row = row > 7 ? 7 : row;
    return gf_bg(PAD_BL + lx + col * CW + ox, PAD_BT + ly + row * CH + oy);
}

/* a blank cell's first pixel, clamped to the grid */
static int pad_cell_pen(int lx, int ly, int col, int row)
{
    return pad_expect(lx, ly, col * CW, row * CH);
}

/* Pixels of the strips that do not show what they should: the edge cell's look (extend), else `bg` */
static int pad_wrong(int lx, int ly, int extend, int bg)
{
    int x, y, n = 0, gw = 21 * CW, gh = 8 * CH;
    for (y = 0; y < PAD_IH; y++)
        for (x = 0; x < PAD_IW; x++) {
            int cx = x - lx, cy = y - ly;
            if (cx >= 0 && cx < gw && cy >= 0 && cy < gh)
                continue; /* a cell */
            n += gf_bg(PAD_BL + x, PAD_BT + y) != (extend ? pad_expect(lx, ly, cx, cy) : bg);
        }
    return n;
}

/* the screen the strips are checked against: every edge a different colour (blue left column, red third row, green
 * last row, yellow top-right cell, magenta last column of the fifth row) */
static const char *pad_scene(int phase)
{
    return phase ? "\x1b[?25l\x1b[46m\x1b[2J\x1b[2;1H\x1b[43m\x1b[K\x1b[7;1H\x1b[45m\x1b[K\x1b[8;1H\x1b[41m\x1b[K\x1b[m"
                 : "\x1b[?25l\x1b[44m\x1b[2J\x1b[3;1H\x1b[41m\x1b[K\x1b[8;1H\x1b[42m\x1b[K\x1b[1;21H\x1b[43m \x1b[5;21H\x1b[45m \x1b[m";
}

/* W-T14 (owner 2026-10-14, Ghostty's window-padding-color = extend): with vr_set_pad_extend(1) every pixel of the
 * strips -- right of the last whole column, below the last whole row, and the offsets above and left of the grid --
 * shows the pen of the cell next to it, corners included; with it off (the default) all of them the window's
 * background. The strips follow output that changes the edge cells (a scroll, an erase), a synchronized frame drawn
 * composed, and the setting turned off and on again by the live redraw; on a chunky and a planar screen. */
static void padding_extend_gives_the_strips_the_colour_of_the_edge_cells(void)
{
    static const int depths[] = { 0, 4 };
    static const int shifts[][2] = { { 0, 0 }, { 5, 3 } };
    int m, k, sh, phase;
    for (m = 0; m < 2; m++)
        for (sh = 0; sh < 2; sh++)
            for (k = 0; k < 2; k++) {
                int lx = shifts[sh][0], ly = shifts[sh][1];
                vt_callbacks cb;
                memset(&cb, 0, sizeof(cb));
                cb.damage = cb_damage;
                cb.scroll = cb_scroll;
                cb.colors = cb_colors;
                gf_open(PAD_BL + PAD_IW + 18, PAD_BT + PAD_IH + 2, CW, CH, 6);
                if (depths[m]) {
                    gf_planar(depths[m], 0, 10);
                    gf_palette(wb16, 16);
                }
                gf_borders(PAD_BL, PAD_BT, 18, 2);
                term = vt_new(21, 8, 100, &cb, &rn);
                vt_set_personality(term, VT_XTERM);
                vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
                rn.lay_x = (WORD)lx;
                rn.lay_y = (WORD)ly;
                vr_layout(&rn);
                CHECK_INT(rn.cols, 21);
                CHECK_INT(rn.rows, 8);
                vr_redraw(&rn);
                CHECK_INT(rn.pad_extend, 0); /* the default: the background */
                for (phase = 0; phase < 2; phase++) {
                    pass(k ? synced(pad_scene(phase)) : pad_scene(phase));
                    CHECK(pad_cell_pen(lx, ly, 0, 2) != (int)rn.pen_default_bg); /* the scene has colours */
                    CHECK_INT(pad_wrong(lx, ly, 0, (int)rn.pen_default_bg), 0);
                    vr_set_pad_extend(&rn, 1);
                    vr_redraw(&rn); /* what vtwin_apply_settings does */
                    CHECK_INT(pad_wrong(lx, ly, 1, 0), 0);
                    pass(k ? synced("\x1b[8;1H\n\x1b[1;1H\x1b[44m\x1b[K\x1b[8;1H\x1b[42m\x1b[K\x1b[m")
                           : "\x1b[8;1H\n\x1b[1;1H\x1b[44m\x1b[K\x1b[8;1H\x1b[42m\x1b[K\x1b[m");
                    CHECK_INT(pad_wrong(lx, ly, 1, 0), 0); /* the strips followed the scroll and the erase */
                    vr_set_pad_extend(&rn, 0);
                    vr_redraw(&rn);
                    CHECK_INT(pad_wrong(lx, ly, 0, (int)rn.pen_default_bg), 0);
                }
                CHECK_INT(gf_border_hits(), 0);
                close_term();
            }
}

/* Q3-27: the default cursor (cursor-color = inverse) is the cell with its colours swapped, not its pen numbers
 * complemented: on a 4-plane screen the pen of (pen ^ 15) is whatever the palette holds there (a blue block on the
 * 16-colour Workbench). The cursor's fill is the cell's text pen, its glyph the cell's background pen; and the
 * blink's off phase gives the cell back. */
static void the_default_cursor_is_the_cell_with_its_colours_swapped_not_its_pens_complemented(void)
{
    int x, y, k, differs = 0;
    int snap[CW * CH];
    planar_depth = 4;
    planar_left = 0;
    test_pal = wb16;
    test_npal = 16;
    open_term_in(0, VT_XTERM);
    for (k = 0; k < 8; k++) {
        char buf[64];
        int fg, bg, ok = 1, glyph = 0, fills = 0;
        snprintf(buf, sizeof(buf), "\x1b[?25l\x1b[2J\x1b[1;1H\x1b[%dmW\x1b[m\x1b[1;1H", 40 + k);
        pass(buf);
        bg = gf_pixel(0, 0);
        fg = text_pen(0, 0, bg);
        CHECK(fg != bg);
        for (y = 0; y < CH; y++)
            for (x = 0; x < CW; x++) {
                snap[y * CW + x] = gf_pixel(x, y);
                ok &= snap[y * CW + x] == fg || snap[y * CW + x] == bg;
            }
        if (!ok)
            continue; /* a dithered background (W-T9): two pens, no one swap to compare */
        differs += (bg ^ 15) != fg; /* COMPLEMENT would draw another pen than the text's */
        pass("\x1b[?25h");
        CHECK_INT(rn.cursor_drawn, 1);
        for (y = 0; y < CH; y++)
            for (x = 0; x < CW; x++) {
                int pen = gf_pixel(x, y), was_glyph = snap[y * CW + x] == fg;
                glyph += was_glyph;
                fills += !was_glyph;
                ok &= pen == (was_glyph ? bg : fg);
            }
        CHECK(glyph > 0 && fills > 0);
        CHECK(ok);
        vr_cursor_off(&rn);
        for (y = 0; y < CH; y++)
            for (x = 0; x < CW; x++)
                ok &= gf_pixel(x, y) == snap[y * CW + x];
        CHECK(ok);
    }
    CHECK(differs >= 3);
    /* the blink repaints the cursor's cell and no other pixel of the window */
    {
        static int all[COLS * CW * ROWS * CH];
        int changed = 0, cell_changed = 0;
        pass("\x1b[?25l\x1b[2J\x1b[1;1H\x1b[42mWW\x1b[m\x1b[2;3Hxy\x1b[2;4H");
        for (y = 0; y < ROWS * CH; y++)
            for (x = 0; x < COLS * CW; x++)
                all[y * COLS * CW + x] = gf_pixel(x, y);
        pass("\x1b[?25h");
        for (y = 0; y < ROWS * CH; y++)
            for (x = 0; x < COLS * CW; x++)
                if (all[y * COLS * CW + x] != gf_pixel(x, y)) {
                    if (x / CW == 3 && y / CH == 1)
                        cell_changed++;
                    else
                        changed++;
                }
        CHECK(cell_changed > 0);
        CHECK_INT(changed, 0);
        vr_cursor_off(&rn);
        for (y = 0; y < ROWS * CH; y++)
            for (x = 0; x < COLS * CW; x++)
                changed += all[y * COLS * CW + x] != gf_pixel(x, y);
        CHECK_INT(changed, 0);
    }
    close_term();
    planar_depth = 0;
    test_npal = 0;
}

/* PROF counters of the cursor pass (ledger "cursor pass"): the Text and RectFill of the cursor's draw are counted
 * as the fake sees them, and a cursor cell drawn again unchanged is counted. */
extern ULONG vr_cp[12], vr_cp_pass;
/* pass(): the counters run in the two cursor blocks only, as vtwin_render sets them */
static void pass_counted(const char *s)
{
    vr_mask_begin(&rn);
    vr_cp_pass = 1;
    vr_cursor_off(&rn);
    vr_cp_pass = 0;
    vt_feed(term, (const vt_u8 *)s, (long)strlen(s));
    vt_flush(term);
    vr_cp_pass = 1;
    vr_cursor_on(&rn);
    vr_cp_pass = 0;
    vr_mask_end(&rn);
}
static void the_cursor_pass_counts_its_gfx_calls_and_unchanged_cells(void)
{
    int k;
    open_term(12);
    pass("AB"); /* the cursor is on the cell after B */
    for (k = 0; k < 12; k++)
        vr_cp[k] = 0;
    pass_counted("");
    CHECK_INT((int)vr_cp[8], 1);
    pass_counted("");  /* nothing changed: the same cell again */
    CHECK_INT((int)vr_cp[8], 2);
    CHECK_INT((int)vr_cp[9], 1);
    CHECK((int)vr_cp[3] >= 1);                /* off: the cell repainted by draw_rows */
    CHECK((int)vr_cp[5] >= 1);                /* on: the cell filled */
    pass_counted("C"); /* the cursor moved */
    CHECK_INT((int)vr_cp[8], 3);
    CHECK_INT((int)vr_cp[9], 1);
    /* the on block's Text and RectFill as the fake logs them */
    vr_mask_begin(&rn);
    vr_cursor_off(&rn);
    gf_clear_log();
    for (k = 0; k < 12; k++)
        vr_cp[k] = 0;
    vr_cp_pass = 1;
    vr_cursor_on(&rn);
    vr_cp_pass = 0;
    vr_mask_end(&rn);
    CHECK_INT((int)vr_cp[4], gf_count(GF_TEXT));
    CHECK_INT((int)vr_cp[5], gf_count(GF_RECT));
    close_term();
}

/* K11: a kitty image (a=T, f=24 raw RGB, 4x2 pixels) fed through the engine reaches the window as its pixels:
 * the renderer's image run on a true-colour screen (WriteLUTPixelArray, here a GF_CGXLUT call) paints each
 * pixel's colour -- the palette cube holds the 0 and 255 levels exactly. The engine's kitty_images_placed
 * counter (vt_kitty_placed) shows the command ran. */
static void a_kitty_image_draws_its_pixels_on_the_canvas(void)
{
    static const int want[8] = { 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF, 0x000000, 0xFFFF00, 0x00FFFF, 0xFF00FF };
    int i;
    open_term_in(0, VT_XTERM);
    gf_truecolor(1);
    vr_free(&rn);
    vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
    vt_set_cell_pixels(term, CW, CH);
    vr_redraw(&rn);
    CHECK(rn.cgx != 0);
    CHECK_INT((int)vt_kitty_placed(term), 0);
    gf_clear_log();
    /* red green blue white / black yellow cyan magenta */
    pass("\x1b_Ga=T,f=24,s=4,v=2;/wAAAP8AAAD/////AAAA//8AAP///wD/\x1b\\");
    CHECK_INT((int)vt_kitty_placed(term), 1);
    CHECK(gf_count(GF_CGXLUT) >= 1);
    for (i = 0; i < 8; i++)
        CHECK_INT(gf_pixel(i % 4, i / 4), GF_CGX | want[i]);
    close_term();
}

int main(void)
{
    pen_matches_on_a_prepared_palette_are_the_bare_palettes_picks();
    a_second_window_on_the_same_palette_matches_no_colour_again();
    a_kitty_image_draws_its_pixels_on_the_canvas();
    the_cursor_pass_counts_its_gfx_calls_and_unchanged_cells();
    scrollback_view_move_blits_and_draws_only_the_exposed_rows();
    cursor_glyph_is_drawn_at_the_cursor_cell_from_the_grid_row();
    the_default_cursor_is_the_cell_with_its_colours_swapped_not_its_pens_complemented();
    direct_colour_background_in_the_amiga_personality_is_drawn_in_its_colour();
    con_window_draws_pen_zero_with_its_own_ground();
    direct_colours_on_a_true_colour_screen_are_filled_by_fillpixelarray();
    direct_colours_are_exact_on_a_true_colour_screen_whose_colour_map_is_used_up();
    neighbouring_half_blocks_of_one_colour_are_one_fill_with_the_same_pixels();
    the_same_joining_on_a_true_colour_screen();
    slow_and_rapid_blink_keep_their_rates_whatever_the_frame_interval();
    a_synchronized_frame_reaches_the_window_in_one_blit_after_it_is_drawn();
    output_without_a_synchronized_frame_is_drawn_in_place();
    a_synchronized_frame_that_scrolls_shows_what_in_place_drawing_shows();
    a_synchronized_frame_that_scrolls_is_not_left_for_settle();
    a_planar_synchronized_frame_reaches_the_window_in_one_copy_with_the_in_place_pixels();
    a_planar_frame_is_copied_once_the_beam_has_left_its_rows();
    a_planar_frame_with_lines_or_a_covered_window_is_drawn_in_place();
    a_planar_frame_drawn_in_place_after_all_clears_the_screen_it_replaced();
    light_cube_colours_on_a_16_colour_workbench_are_not_drawn_in_its_grey_pen();
    a_dithered_cell_shows_its_colour_closer_than_the_old_single_pen();
    a_dithered_frame_composed_and_drawn_in_place_has_the_same_pixels();
    screens_of_more_than_16_pens_are_not_dithered();
    a_half_block_of_two_colours_sharing_a_pen_is_drawn_in_that_pen();
    drawing_never_touches_the_window_borders_at_an_odd_inner_width();
    the_strips_beside_the_grid_and_blank_cells_show_the_window_background();
    dither_off_draws_the_nearest_single_pen_and_on_dithers_again();
    padding_extend_gives_the_strips_the_colour_of_the_edge_cells();
    printf("render: %d checks, %d failures\n", h_checks, h_failures);
    return h_failures ? 1 : 0;
}
