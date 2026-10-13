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

#define COLS 20
#define ROWS 8
#define CW 8
#define CH 8

static vr_render rn;
static vt_term *term;
/* open_term_in's screen: planar (gf_planar) of this depth with the window at screen x planar_left, y 10; 0: the
 * chunky screen */
static int planar_depth, planar_left;

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
    int composed;
    vr_mask_begin(&rn);
    vr_cursor_off(&rn);
    vt_feed(term, (const vt_u8 *)s, (long)strlen(s));
    composed = vr_compose_begin(&rn);
    vt_flush(term);
    if (composed)
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

int main(void)
{
    scrollback_view_move_blits_and_draws_only_the_exposed_rows();
    cursor_glyph_is_drawn_at_the_cursor_cell_from_the_grid_row();
    direct_colour_background_in_the_amiga_personality_is_drawn_in_its_colour();
    direct_colours_on_a_true_colour_screen_are_filled_by_fillpixelarray();
    neighbouring_half_blocks_of_one_colour_are_one_fill_with_the_same_pixels();
    the_same_joining_on_a_true_colour_screen();
    slow_and_rapid_blink_keep_their_rates_whatever_the_frame_interval();
    a_synchronized_frame_reaches_the_window_in_one_blit_after_it_is_drawn();
    output_without_a_synchronized_frame_is_drawn_in_place();
    a_synchronized_frame_that_scrolls_shows_what_in_place_drawing_shows();
    a_planar_synchronized_frame_reaches_the_window_in_one_copy_with_the_in_place_pixels();
    a_planar_frame_is_copied_once_the_beam_has_left_its_rows();
    a_planar_frame_with_lines_or_a_covered_window_is_drawn_in_place();
    printf("render: %d checks, %d failures\n", h_checks, h_failures);
    return h_failures ? 1 : 0;
}
