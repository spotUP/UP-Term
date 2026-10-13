/* The Amiga renderer's draw wiring on the host (ledger R8): render/amiga_render.c itself, compiled
 * against the NDK's structs and a fake graphics.library (tests/render_host/gfx_fake.c) that logs every
 * call and draws Text, RectFill and ScrollRaster into a pixel canvas. A real vt_term feeds it through
 * the callbacks vtwin.c gives the engine. Not covered: the planar fast path and the DIRECT blit
 * (they need a real bitmap), vtwin's own frame loop.
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

static void cb_damage(void *u, int x0, int y0, int x1, int y1)
{
    vr_damage((vr_render *)u, x0, y0, x1, y1);
}

static void cb_scroll(void *u, int top, int bot, int n)
{
    vr_scroll((vr_render *)u, top, bot, n);
}

/* vtwin_render's pass (render/vtwin.c): mask begin, cursor off, the engine's flush, cursor on, mask end */
static void pass(const char *s)
{
    vr_mask_begin(&rn);
    vr_cursor_off(&rn);
    vt_feed(term, (const vt_u8 *)s, (long)strlen(s));
    vt_flush(term);
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

int main(void)
{
    scrollback_view_move_blits_and_draws_only_the_exposed_rows();
    cursor_glyph_is_drawn_at_the_cursor_cell_from_the_grid_row();
    direct_colour_background_in_the_amiga_personality_is_drawn_in_its_colour();
    direct_colours_on_a_true_colour_screen_are_filled_by_fillpixelarray();
    printf("render: %d checks, %d failures\n", h_checks, h_failures);
    return h_failures ? 1 : 0;
}
