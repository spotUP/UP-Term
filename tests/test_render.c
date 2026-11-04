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
#include "../render/emoji.h"
#include "../render/outline.h"
#include "../render/vtwin.h"

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

/* The engine damages the window during vtwin_attach (vt_set_personality resets it) before bind runs vr_init:
 * sel_drop_if_hit asks a renderer with no terminal yet. Before 2026-10-25 vr_selection read the terminal's
 * scroll count first, a read of NULL + 0x56 that MuForce reported on every new window. */
static void selection_of_a_renderer_not_yet_initialised_is_none_and_reads_no_terminal(void)
{
    vr_render r;
    int ax = -1, ay = -1, bx = -1, by = -1;
    memset(&r, 0, sizeof r);
    CHECK_INT(vr_selection(&r, &ax, &ay, &bx, &by), 0);
    CHECK_INT(ax, -1);
}


/* A drag selection exists only while the left button is down and the window is active (render/vtwin.c). The
 * release of a press that opened the Amarchy text menu's window went to that window: the terminal kept
 * dragging, and every pointer move extended the selection for good. */
static vtwin dw;
static int dh_raw(void *u) { (void)u; return 0; }
static void dh_input(void *u, const vt_u8 *b, long n) { (void)u; (void)b; (void)n; }
static const vtwin_host dhost = { 0, dh_input, 0, 0, dh_raw, 0, 0, 0, 0, 0 };

static void drag_open(void)
{
    vt_callbacks cb;
    memset(&dw, 0, sizeof dw);
    memset(&cb, 0, sizeof(cb));
    cb.damage = cb_damage;
    cb.scroll = cb_scroll;
    gf_open(COLS * CW, ROWS * CH, CW, CH, 6);
    vtwin_init(&dw, &dhost, 0);
    term = vt_new(COLS, ROWS, 100, &cb, &dw.r);
    dw.t = term;
    dw.win = gf_window();
    vr_init(&dw.r, gf_window(), gf_font(), term, VT_ENC_LATIN1);
    vt_feed(term, (const vt_u8 *)"alpha beta gamma delta\r\nline two\r\n", 34);
    vt_flush(term);
}

static void drag_close(void)
{
    vr_free(&dw.r);
    vt_free(term);
    gf_close();
}

/* the pixel in the middle of cell (cx, cy) */
#define PX(cx) (dw.r.ox + (cx) * CW + 1)
#define PY(cy) (dw.r.oy + (cy) * CH + 1)

static int sel_end(int *bx, int *by)
{
    int ax, ay;
    return vr_selection(&dw.r, &ax, &ay, bx, by);
}

static void a_drag_ends_when_the_left_button_is_up_or_the_window_goes_inactive(void)
{
    int bx = -1, by = -1;
    drag_open();

    /* round trip: the press, then moves WITH the left button, extend */
    vtwin_mouse(&dw, 0, SELECTDOWN, 0, PX(2), PY(0), 10, 0);
    CHECK_INT(dw.dragging, 1);
    vtwin_mouse(&dw, 1, 0, IEQUALIFIER_LEFTBUTTON, PX(6), PY(0), 10, 1);
    CHECK_INT(sel_end(&bx, &by), 1);
    CHECK_INT(bx, 6);
    CHECK_INT(dw.dragging, 1);

    /* the release went elsewhere: a move without the button ends the drag, the selection stays, no extension */
    vtwin_mouse(&dw, 1, 0, 0, PX(12), PY(0), 10, 2);
    CHECK_INT(dw.dragging, 0);
    CHECK_INT(sel_end(&bx, &by), 1);
    CHECK_INT(bx, 6);
    vtwin_mouse(&dw, 1, 0, IEQUALIFIER_LEFTBUTTON, PX(15), PY(0), 10, 3);
    CHECK_INT(sel_end(&bx, &by), 1); /* no drag any more: even a move with the bit set extends nothing */
    CHECK_INT(bx, 6);

    /* the window goes inactive mid-drag: the drag ends, a tick and a move do not extend */
    vtwin_mouse(&dw, 0, SELECTDOWN, 0, PX(1), PY(1), 20, 0);
    vtwin_mouse(&dw, 1, 0, IEQUALIFIER_LEFTBUTTON, PX(4), PY(1), 20, 1);
    CHECK_INT(dw.dragging, 1);
    CHECK_INT(sel_end(&bx, &by), 1);
    CHECK_INT(bx, 4);
    vtwin_focus(&dw, 0);
    CHECK_INT(dw.dragging, 0);
    dw.win->MouseX = PX(9);
    dw.win->MouseY = PY(1);
    vtwin_tick(&dw);
    vtwin_mouse(&dw, 1, 0, IEQUALIFIER_LEFTBUTTON, PX(9), PY(1), 20, 2);
    CHECK_INT(sel_end(&bx, &by), 1);
    CHECK_INT(bx, 4);

    /* the tick's poll follows the pointer only while the window is active */
    vtwin_focus(&dw, 1);
    dw.dragging = 1;
    dw.drag_x = 4;
    dw.drag_y = 1;
    dw.inactive = 1;
    vtwin_tick(&dw);
    CHECK_INT(sel_end(&bx, &by), 1);
    CHECK_INT(bx, 4);
    dw.inactive = 0;
    vtwin_tick(&dw);
    CHECK_INT(sel_end(&bx, &by), 1);
    CHECK_INT(bx, 9); /* active and dragging: the poll extends */
    drag_close();
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

/* ---- R4: antialiased outline glyphs ---- */

/* A glyph of UP-Term's own engine (render/ttf.c): coverage rising left to right across the cell, the same on every
 * row, and the mask the engine makes of it (set where coverage is at least half). */
static const UBYTE aa_level[CW] = { 0, 40, 64, 100, 128, 170, 192, 255 };
static UBYTE aa_cov[CW * CH], aa_mask[2 * CH];
static vo_font *aa_font = (vo_font *)aa_level; /* any pointer: the fake's vo_* never look at it */

static void aa_glyph(void)
{
    int x, y;
    for (y = 0; y < CH; y++) {
        aa_mask[2 * y] = aa_mask[2 * y + 1] = 0;
        for (x = 0; x < CW; x++) {
            aa_cov[y * CW + x] = aa_level[x];
            if (aa_level[x] >= 128)
                aa_mask[2 * y] |= (UBYTE)(0x80 >> x);
        }
    }
    gf_outline(aa_mask, 2, aa_cov);
    vr_set_outline(&rn, aa_font);
}

/* a snowman (U+2603, not in the Latin-1 font: the outline font's glyph) at the top left, the cursor hidden: in white
 * on black (direct colours), or in the screen's text pen 1 on its background pen 0 */
#define AA_SNOWMAN "\x1b[?25l\x1b[H\x1b[38;2;255;255;255;48;2;0;0;0m\xe2\x98\x83\x1b[m"
#define AA_SNOWMAN_PENS "\x1b[?25l\x1b[H\x1b[m\xe2\x98\x83"

/* the cell's pixels are all one of its first column's and its last column's: 1-bit, no shade */
static int aa_two_colours(void)
{
    int x, y, bad = 0;
    for (y = 0; y < CH; y++)
        for (x = 0; x < CW; x++)
            bad |= gf_pixel(x, y) != gf_pixel(0, 0) && gf_pixel(x, y) != gf_pixel(CW - 1, 0);
    return !bad && gf_pixel(0, 0) != gf_pixel(CW - 1, 0);
}

/* R4.2: on a true-colour screen the coverage is written through the text-over-background table (WriteLUTPixelArray):
 * each pixel its own blend, edge pixels between the two colours; n_aa (the /state sentinel) counts the glyph. With
 * vr_set_antialias(0), as the profile's outline-antialias = off reaches it, the mask alone. */
static void an_outline_glyph_is_antialiased_on_a_true_colour_screen_and_not_when_off(void)
{
    ULONG aa0, out0;
    int x;
    open_term_in(0, VT_XTERM);
    gf_truecolor(1);
    vr_free(&rn);
    vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
    vr_redraw(&rn);
    CHECK(rn.cgx != 0);
    CHECK_INT(rn.aa_on, 1); /* the default */
    aa_glyph();
    aa0 = rn.n_aa;
    out0 = rn.n_outline;
    gf_clear_log();
    pass(AA_SNOWMAN);
    CHECK_INT((int)(rn.n_aa - aa0), 1);
    CHECK_INT((int)(rn.n_outline - out0), 1);
    CHECK(gf_count(GF_CGXLUT) >= 1);
    for (x = 0; x < CW; x++)
        CHECK_INT(gf_pixel(x, CH / 2), GF_CGX | (int)ce_mix(0xFFFFFF, 0, aa_level[x]));
    CHECK_INT(gf_pixel(4, 0), GF_CGX | 0x808080); /* half coverage: half way */
    vr_set_antialias(&rn, 0);
    aa0 = rn.n_aa;
    out0 = rn.n_outline;
    gf_clear_log();
    vr_redraw(&rn);
    CHECK_INT((int)(rn.n_aa - aa0), 0);
    CHECK_INT((int)(rn.n_outline - out0), 1);
    CHECK_INT(gf_count(GF_CGXLUT), 0); /* the mask in the text colour: no table of shades */
    close_term();
}

/* R4.3: on a palette screen with greys, each pixel takes the nearest of background, the three shade pens and the
 * text pen; on one without (no pen nearer a shade than black or white), the 1-bit mask as before. */
static void an_outline_glyph_uses_the_shade_pens_a_palette_has_and_the_mask_when_none(void)
{
    static const unsigned long greys[8] = { 0x000000, 0xFFFFFF, 0x404040, 0x808080, 0xC0C0C0, 0xFF0000, 0x00FF00, 0x0000FF };
    static const unsigned long bright[8] = { 0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0x00FFFF, 0xFF00FF };
    static const int want[CW] = { 0, 2, 2, 3, 3, 4, 4, 1 };
    ULONG aa0;
    int x;
    planar_depth = 3;
    planar_left = 0;
    test_pal = greys;
    test_npal = 8;
    open_term_in(0, VT_XTERM);
    aa_glyph();
    aa0 = rn.n_aa;
    pass(AA_SNOWMAN_PENS);
    CHECK_INT((int)(rn.n_aa - aa0), 1);
    for (x = 0; x < CW; x++)
        CHECK_INT(gf_pixel(x, CH / 2), want[x]);
    close_term();
    test_pal = bright;
    open_term_in(0, VT_XTERM);
    aa_glyph();
    aa0 = rn.n_aa;
    pass(AA_SNOWMAN_PENS);
    CHECK_INT((int)(rn.n_aa - aa0), 0);
    CHECK(aa_two_colours());
    CHECK_INT(gf_pixel(CW - 1, 0), 1);
    CHECK_INT(gf_pixel(0, 0), 0);
    close_term();
    planar_depth = 0;
    test_npal = 0;
}

/* R6.A1/A2, R6.5: over a dithered cell background (a screen of few pens: two pens in a tile) an antialiased glyph's
 * edge pixels are the pen nearest fg mixed over the pen UNDER the pixel, not over the cell's mean colour. The glyph
 * is drawn twice: over the dither (blend_dest on, the default: n_aa_tile counts the glyph) and with
 * vr_set_blend_dest(0), the profile's outline-blend-dest = off (the shades of the mean background: n_aa_tile 0).
 * Each is judged by its error against the ideal colour ce_mix(fg, the pen the tile has there, coverage). */
#define AB_DRAW "\x1b[?25l\x1b[H\x1b[38;5;231;48;5;244m\xe2\x98\x83\x1b[m"

static unsigned long ab_rgb(int pen)
{
    return wb16[pen];
}

/* rows 0 and 1 of the cell (the two phases of the tile): the squared RGB distance of what is drawn to the ideal */
static long ab_err(const int *tile, int *sum)
{
    int x, y, k;
    long e = 0;
    for (y = 0; y < 2; y++)
        for (x = 0; x < CW; x++) {
            unsigned long ideal = ce_mix(0xFFFFFF, ab_rgb(tile[y * CW + x]), aa_level[x]),
                          got = ab_rgb(gf_pixel(x, y));
            for (k = 0; k < 3; k++) {
                long d = (long)((ideal >> (16 - 8 * k)) & 255) - (long)((got >> (16 - 8 * k)) & 255);
                e += d * d;
            }
        }
    *sum = (int)e;
    return e;
}

static void an_antialiased_edge_over_a_dither_is_blended_with_the_pen_under_it_and_the_knob_moves_it(void)
{
    int bgp[2 * CW], on[2 * CW], off[2 * CW], x, y, e_on = 0, e_off = 0, n_on = 0, n_off = 0, diff = 0;
    ULONG t0, a0;
    planar_depth = 4;
    planar_left = 0;
    test_pal = wb16;
    test_npal = 16;
    open_term_in(0, VT_XTERM);
    CHECK_INT(rn.blend_dest, 1); /* the default */
    aa_glyph();
    pass("\x1b[?25l\x1b[H\x1b[38;5;231;48;5;244m \x1b[m"); /* the tile alone */
    for (y = 0; y < 2; y++)
        for (x = 0; x < CW; x++)
            bgp[y * CW + x] = gf_pixel(x, y);
    CHECK(bgp[0] != bgp[3] || bgp[0] != bgp[CW]); /* two pens: a dither */
    t0 = rn.n_aa_tile;
    a0 = rn.n_aa;
    pass(AB_DRAW);
    n_on = (int)(rn.n_aa_tile - t0);
    CHECK_INT((int)(rn.n_aa - a0), 1);
    CHECK_INT(n_on, 1); /* the sentinel: the tile path drew it */
    for (y = 0; y < 2; y++)
        for (x = 0; x < CW; x++)
            on[y * CW + x] = gf_pixel(x, y);
    ab_err(bgp, &e_on);
    vr_set_blend_dest(&rn, 0);
    pass("\x1b[H\x1b[38;5;231;48;5;244m \x1b[m");
    t0 = rn.n_aa_tile;
    pass(AB_DRAW);
    n_off = (int)(rn.n_aa_tile - t0);
    CHECK_INT(n_off, 0);
    for (y = 0; y < 2; y++)
        for (x = 0; x < CW; x++) {
            off[y * CW + x] = gf_pixel(x, y);
            diff += off[y * CW + x] != on[y * CW + x];
        }
    ab_err(bgp, &e_off);
    CHECK(diff > 0); /* the knob moves the output */
    CHECK(e_on < e_off); /* and the blend over the pen underneath is nearer the ideal */
    /* row 0, x 0..7: the tile pens 3 0 3 3 ... (bgp) with the glyph's coverage levels 0 40 64 100 128 170 192 255 */
    for (x = 0; x < CW; x++) {
        static const int want_on[CW] = { 3, 3, 6, 6, 6, 7, 7, 2 };
        CHECK_INT(on[x], want_on[x]);
    }
    /* row 1 has the other tile phase: the same coverage over the other pens gives other shades */
    CHECK(memcmp(on, on + CW, sizeof(int) * CW) != 0);
    vr_set_blend_dest(&rn, 1);
    close_term();
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

/* NS3.4 (owner 2026-10-19): a pane without the keys draws its block cursor hollow -- the cell's edge in the
 * cursor's fill, its inside the cell as it was -- and the pane with them a solid block. The swap redraws the
 * cursor at once, a hidden cursor (?25l) stays hidden through it, and the off phase gives the cell back. */
static void an_unfocused_pane_draws_a_hollow_cursor_and_the_focused_one_a_solid_block(void)
{
    int x, y, edge_ok = 1, inside_bg = 1, solid = 1, back = 1, untouched = 1;
    int snap[CW * CH];
    open_term(0);
    pass("\x1b[?25l\x1b[2J\x1b[1;1H");
    for (y = 0; y < CH; y++)
        for (x = 0; x < CW; x++)
            snap[y * CW + x] = gf_pixel(x, y);
    pass("\x1b[?25h");
    for (y = 0; y < CH; y++)
        for (x = 0; x < CW; x++)
            solid &= gf_pixel(x, y) != snap[y * CW + x]; /* an empty cell's block: every pixel the fill */
    CHECK(solid);
    vr_set_cursor_hollow(&rn, 1);
    CHECK_INT(rn.cursor_drawn, 1);
    for (y = 0; y < CH; y++)
        for (x = 0; x < CW; x++) {
            int edge = x == 0 || y == 0 || x == CW - 1 || y == CH - 1;
            if (edge)
                edge_ok &= gf_pixel(x, y) != snap[y * CW + x];
            else
                inside_bg &= gf_pixel(x, y) == snap[y * CW + x];
        }
    CHECK(edge_ok);
    CHECK(inside_bg);
    vr_cursor_off(&rn);
    for (y = 0; y < CH; y++)
        for (x = 0; x < CW; x++)
            back &= gf_pixel(x, y) == snap[y * CW + x];
    CHECK(back);
    vr_cursor_on(&rn);
    vr_set_cursor_hollow(&rn, 0); /* the keys came back: solid again */
    solid = 1;
    for (y = 0; y < CH; y++)
        for (x = 0; x < CW; x++)
            solid &= gf_pixel(x, y) != snap[y * CW + x];
    CHECK(solid);
    /* a hidden cursor: losing and getting the focus draws nothing */
    pass("\x1b[?25l");
    vr_set_cursor_hollow(&rn, 1);
    vr_set_cursor_hollow(&rn, 0);
    CHECK_INT(rn.cursor_drawn, 0);
    for (y = 0; y < CH; y++)
        for (x = 0; x < CW; x++)
            untouched &= gf_pixel(x, y) == snap[y * CW + x];
    CHECK(untouched);
    close_term();
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
    CHECK(gf_count(GF_CGXPIX) >= 1);
    for (i = 0; i < 8; i++)
        CHECK_INT(gf_pixel(i % 4, i / 4), GF_CGX | want[i]);
    close_term();
}


/* K30: a kitty picture with z<0 lies under the text: the image first, the
 * glyph over it (its Text after the image in the log), and the cells
 * without text show the picture. Before K30 the picture blanked the cells
 * and covered the X. */
static void k30_open_truecolour(void)
{
    open_term_in(0, VT_XTERM);
    gf_truecolor(1);
    vr_free(&rn);
    vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
    vt_set_cell_pixels(term, CW, CH);
    vr_redraw(&rn);
}

static int k30_last_text_after_image(const char *want)
{
    int i, img = -1, txt = -1;
    for (i = 0; i < gf_n; i++) {
        if (gf_log[i].kind == GF_CGXPIX)
            img = i;
        if (gf_log[i].kind == GF_TEXT && !strcmp(gf_log[i].str, want))
            txt = i;
    }
    return img >= 0 && txt > img;
}

static void k30_text_lies_over_a_picture_below(void)
{
    int under;
    k30_open_truecolour();
    pass("\x1b[1;1HX");
    under = gf_pixel(3, 3);
    gf_clear_log();
    /* red green blue white / black yellow cyan magenta, 2 cells wide */
    pass("\x1b[1;1H\x1b_Ga=T,f=24,s=4,v=2,c=2,r=1,z=-1,C=1;/wAAAP8AAAD/////AAAA//8AAP///wD/\x1b\\");
    CHECK(gf_count(GF_CGXPIX) >= 1);
    CHECK(k30_last_text_after_image("X")); /* the glyph over the image */
    CHECK_INT(gf_pixel(3, 3), under);      /* the fake paints the X's cell whole */
    CHECK((gf_pixel(12, 1) & GF_CGX) != 0); /* the blank cell beside it: the picture */
    close_term();
}

/* K32: cells of U+10EEEE with fg 38;5;7 (image 7) and the row/column marks
 * (U+0305 = 0, U+030D = 1) draw the four quarters of a 2 x 2 cell virtual
 * placement (red green / blue white) in place of a glyph; a placeholder of an
 * unknown image (fg 8) is a blank. The sentinel vt_kitty_placeholders shows
 * the engine resolved the four cells for the renderer. Before K32 the cells
 * drew the replacement glyph. */
#define K32_PH "\xF4\x8E\xBB\xAE"
#define K32_M0 "\xCC\x85"
#define K32_M1 "\xCC\x8D"
static void k32_placeholder_cells_draw_their_tiles(void)
{
    unsigned long n0;
    k30_open_truecolour();
    pass("\x1b_Ga=T,U=1,i=7,f=24,s=2,v=2,c=2,r=2;/wAAAP8AAAD/////\x1b\\");
    n0 = vt_kitty_placeholders(term);
    gf_clear_log();
    pass("\x1b[1;1H\x1b[38;5;7m" K32_PH K32_M0 K32_M0 K32_PH K32_M0 K32_M1 "\x1b[2;1H" K32_PH K32_M1 K32_M0 K32_PH K32_M1 K32_M1);
    CHECK_INT((int)(vt_kitty_placeholders(term) - n0) >= 4, 1);
    CHECK_INT(gf_pixel(0, 0), GF_CGX | 0xFF0000);
    CHECK_INT(gf_pixel(CW - 1, CH - 1), GF_CGX | 0xFF0000);
    CHECK_INT(gf_pixel(CW, 0), GF_CGX | 0x00FF00);
    CHECK_INT(gf_pixel(2 * CW - 1, CH - 1), GF_CGX | 0x00FF00);
    CHECK_INT(gf_pixel(0, CH), GF_CGX | 0x0000FF);
    CHECK_INT(gf_pixel(CW, CH), GF_CGX | 0xFFFFFF);
    CHECK_INT(gf_pixel(2 * CW - 1, 2 * CH - 1), GF_CGX | 0xFFFFFF);
    /* no such image: a blank, no glyph and no picture */
    pass("\x1b[4;1H\x1b[38;5;8m" K32_PH K32_M0 K32_M0);
    CHECK_INT(gf_pixel(0, 3 * CH) & GF_CGX, 0);
    close_term();
}

static void k30_far_below_stays_under_a_coloured_background(void)
{
    int bg;
    k30_open_truecolour();
    /* a green text cell under the picture, and a control one clear of it (both drawn by Text(), whose
     * JAM2 background pen the fake keeps in gf_bg) */
    pass("\x1b[1;1H\x1b[42mA\x1b[0m\x1b[1;5H\x1b[42mA\x1b[0m");
    pass("\x1b[1;1H\x1b_Ga=T,f=24,s=4,v=2,c=2,r=1,z=-2000000000,C=1;/wAAAP8AAAD/////AAAA//8AAP///wD/\x1b\\\x1b[5;1H"); /* the cursor away from the cells */
    bg = gf_bg(4 * CW + 3, 3);
    CHECK_INT(gf_bg(3, 3), bg);               /* the green cell keeps its background */
    CHECK((gf_pixel(3, 3) & GF_CGX) == 0);    /* and takes no picture */
    CHECK((gf_pixel(12, 1) & GF_CGX) != 0);   /* the default cell shows the picture */
    close_term();
}

/* ---- NS1.1 / NS1.2: the viewport rectangle ---- */

/* a hash of every graphics call since gf_clear_log: what the renderer did, not what the canvas shows */
static unsigned long log_hash(void)
{
    unsigned long h = 5381;
    int i;
    for (i = 0; i < gf_n; i++) {
        const gf_call *g = &gf_log[i];
        h = h * 33 + (unsigned long)g->kind;
        h = h * 33 + (unsigned long)g->a;
        h = h * 33 + (unsigned long)g->b;
        h = h * 33 + (unsigned long)g->c;
        h = h * 33 + (unsigned long)g->d;
        h = h * 33 + g->drmd;
    }
    return h & 0xFFFFFFFFUL;
}

#define RW 200
#define RH 120
/* a window of RW x RH pixels with borders 4, 11, 18, 2 (inside 178 x 107), the grid in a rect: x y w h
 * (-1 x: no rect call at all, the default). The scenario: a full repaint, 12 lines scrolling the grid,
 * a reverse-video line, the cursor moved. Returns the hash of the calls it made. */
static unsigned long rect_scenario(int x, int y, int w, int h, int first_call_only_layout)
{
    vt_callbacks cb;
    unsigned long hash;
    int i;
    memset(&cb, 0, sizeof(cb));
    cb.damage = cb_damage;
    cb.scroll = cb_scroll;
    cb.colors = cb_colors;
    gf_open(RW, RH, CW, CH, 6);
    if (planar_depth)
        gf_planar(planar_depth, planar_left, 10);
    gf_borders(4, 11, 18, 2);
    term = vt_new(COLS, ROWS, 100, &cb, &rn);
    vt_set_personality(term, VT_XTERM);
    vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
#ifndef RECT_ABSENT
    if (x >= 0)
        vr_set_rect(&rn, (WORD)x, (WORD)y, (WORD)w, (WORD)h);
#endif
    vr_layout(&rn);
    vt_resize(term, rn.cols, rn.rows);
    gf_clear_log();
    vr_redraw(&rn);
    for (i = 0; i < 12; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %02d\r\n", i);
        pass(buf);
    }
    pass("\x1b[7mreverse\x1b[0m\x1b[3;5H");
    (void)first_call_only_layout;
    hash = log_hash();
    return hash;
}

/* The default rectangle is the whole inner window and the renderer's calls are what they were before
 * rects existed (the hash is of the pre-NS1.1 renderer's log for the same scenario); a rect that is
 * exactly the inner area makes the same calls. */
static void the_default_rect_makes_the_calls_the_renderer_always_made(void)
{
    unsigned long def, full;
    int x0, y0, x1, y1;
    def = rect_scenario(-1, 0, 0, 0, 0);
    CHECK_INT(gf_clip(&x0, &y0, &x1, &y1), 0);
    CHECK_INT(def, 1292548310UL); /* recorded from the renderer before NS1.1 (git 8774b8d7) */
    close_term();
#ifndef RECT_ABSENT
    full = rect_scenario(0, 11 - 11, 178, 107, 0);
    CHECK_INT(full, def);
    close_term();
#else
    (void)full;
#endif
    CHECK_INT(gf_regions(), 0);
}

#ifndef RECT_ABSENT
/* Every pixel outside [x0..x1] x [y0..y1] is untouched (0, or the border's mark); the strip inside beside the grid is painted. */
static int pixels_outside(int x0, int y0, int x1, int y1)
{
    int x, y, n = 0;
    for (x = 0; x < RW; x++)
        for (y = 0; y < RH; y++)
            if ((x < x0 || x > x1 || y < y0 || y > y1) && gf_pixel(x, y) != 0 && gf_pixel(x, y) != GF_BORDER) {
                /* a planar screen marks the borders with the last pen, not GF_BORDER (gf_border_hits counts those) */
                if (planar_depth && (x < 4 || x >= RW - 18 || y < 11 || y >= RH - 2))
                    continue;
                n++;
            }
    return n;
}

/* A rect offset inside a larger window: nothing is drawn outside it -- text, fills, scrolls, the padding
 * strips -- with no clip region keeping it there. The inner area starts at (4, 11): the rect at
 * (20, 9) of it is the window's (24, 20), 100 x 52 pixels: 12 x 6 cells and strips of 4 pixels. */
static void a_rect_inside_a_larger_window_keeps_every_draw_inside_it(void)
{
    int cx0, cy0, cx1, cy1, i, ok = 1;
    const int rx0 = 24, ry0 = 20, rx1 = 24 + 100 - 1, ry1 = 20 + 52 - 1;
    rect_scenario(20, 9, 100, 52, 0);
    CHECK_INT(rn.cols, 12);
    CHECK_INT(rn.rows, 6);
    CHECK_INT(rn.ox, rx0);
    CHECK_INT(rn.oy, ry0);
    CHECK_INT(pixels_outside(rx0, ry0, rx1, ry1), 0);
    CHECK_INT(gf_border_hits(), 0);
    /* the text is there: the last line of text sits on the grid's top rows, and the padding is painted */
    CHECK(gf_count(GF_TEXT) > 0);
    CHECK(gf_pixel(rx1, ry0) != 0);
    CHECK(gf_pixel(rx0, ry1) != 0);
    CHECK_INT(gf_cell(0, 0) != ' ', 0); /* cell (0, 0) is of the window, not of the rect: nothing there */
    /* every call lands inside the rect */
    for (i = 0; i < gf_n; i++) {
        const gf_call *g = &gf_log[i];
        if (g->kind == GF_TEXT)
            ok &= g->a >= rx0 && g->b >= ry0 && g->a < rx1 + 1 && g->b < ry1 + 1;
        if (g->kind == GF_RECT)
            ok &= g->a >= rx0 && g->b >= ry0 && g->c <= rx1 && g->d <= ry1;
        if (g->kind == GF_SCROLL)
            ok &= g->c >= ry0 && g->d <= ry1;
    }
    CHECK(ok);
    CHECK(gf_count(GF_SCROLL) > 0);
    /* a new default background repaints the strips (pad_fill); coloured cells at the edges are extended
     * into them (pad_extend_now); a full repaint (vr_redraw) fills the rectangle: none leaves it */
    vr_set_pad_extend(&rn, 1);
    pass("\x1b]11;rgb:20/40/60\x07\x1b[41mabc\x1b[2;1H\x1b[42mxyz\x1b[6;12H\x1b[44mq");
    pass("");
    CHECK_INT(pixels_outside(rx0, ry0, rx1, ry1), 0);
    vr_redraw(&rn);
    CHECK_INT(pixels_outside(rx0, ry0, rx1, ry1), 0);
    CHECK_INT(gf_border_hits(), 0);
    /* NS1.2: no clip region on the layer. It belongs to the window, so a pane installing one would
     * clip the other panes of the window (the phase 0 spike, 2026-10-19) */
    CHECK_INT(gf_clip(&cx0, &cy0, &cx1, &cy1), 0);
    CHECK_INT(gf_regions(), 0);
    /* the inset moves the rect down 3 pixels: nothing beside it or below it */
    vr_set_inset(&rn, 12);
    vr_redraw(&rn);
    CHECK_INT(pixels_outside(rx0, ry0, rx1, 11 + 12 + 52 - 1), 0); /* the old rows keep what was drawn there */
    CHECK_INT(gf_clip(&cx0, &cy0, &cx1, &cy1), 0);
    vr_reset_rect(&rn);
    CHECK_INT(gf_regions(), 0);
    close_term();
}

/* The same rect with the planar/direct paths on (gf_planar: raw scrolls, planar text and fills, pad strips,
 * the direct cursor) at a non-zero screen left, and the rect's own left not 0 (the second pane of a split):
 * a wrapping long line, 40 scrolling lines, cursor moves, colours at the edges, a full redraw. Nothing
 * leaves the rect. */
static void a_planar_rect_keeps_every_draw_inside_it(int depth, int left, int rect_x)
{
    int i;
    const int rx0 = 4 + rect_x, ry0 = 20, rx1 = rx0 + 100 - 1, ry1 = 20 + 52 - 1;
    planar_depth = depth;
    planar_left = left;
    rect_scenario(rect_x, 9, 100, 52, 0);
    CHECK(rn.ox >= rx0 && rn.ox <= rx0 + 7); /* a VTCON_DIRECT build starts the text on a byte boundary: up to 7 pixels in */
    CHECK_INT(pixels_outside(rx0, ry0, rx1, ry1), 0);
    vr_set_pad_extend(&rn, 1);
    pass("\x1b]11;rgb:20/40/60\x07\x1b[41mabc\x1b[2;1H\x1b[42mxyz\x1b[6;12H\x1b[44mq");
    pass("\x1b[H\x1b[0mThis is a long line that must wrap around the pane edge several times over and over again\r\n");
    for (i = 0; i < 40; i++) {
        char buf[48];
        snprintf(buf, sizeof(buf), "\x1b[3%dmscroll %02d\r\n", i % 8, i);
        pass(buf);
    }
    pass("\x1b[1;1H\x1b[2;7H\x1b[6;12Hz\x1b[3;3H\x1b[7mrev\x1b[0m\x1b[2J\x1b[H\x1b[41mred line\r\n");
    pass("\x1b[6;1H\r\n\r\nend");
    CHECK_INT(pixels_outside(rx0, ry0, rx1, ry1), 0);
    vr_redraw(&rn);
    CHECK_INT(pixels_outside(rx0, ry0, rx1, ry1), 0);
    CHECK_INT(gf_border_hits(), 0);
    CHECK(gf_count(GF_SCROLL) + gf_count(GF_BLT) > 0); /* ScrollRaster, or the direct build's BltBitMap */
    CHECK_INT(gf_regions(), 0);
    close_term();
    planar_depth = 0;
    planar_left = 0;
}
#endif

/* A renderer after vr_free is inert: asked for its selection it has none,
 * and it does not ask the term it no longer holds (rig, split panes: a
 * NULL read in vr_selection as a new pane's engine came up, 2026-10-20). */
static void an_inert_renderer_has_no_selection(void)
{
    int ax, ay, bx, by;
    open_term(0);
    vr_select(&rn, 1, 0, 0, 3, 0);
    CHECK(vr_selection(&rn, &ax, &ay, &bx, &by));
    vr_free(&rn);
    CHECK(!vr_selection(&rn, &ax, &ay, &bx, &by));
    vt_free(term);
    gf_close();
}

/* "sometimes characters are not visible, they draw black on black, maybe in low color screenmodes only?" (owner
 * 2026-10-24): a CON: window's text in pen 4 (SGR 34) on a 4-colour screen draws as pen 0, the background's pen.
 * Every pixel of the cell came out pen 0; the glyph must show in another pen. */
static int cell_pixels_not_in_pen(int col, int row, int pen)
{
    int x, y, n = 0;
    for (y = row * CH; y < row * CH + CH; y++)
        for (x = col * CW; x < col * CW + CW; x++)
            n += gf_pixel(x, y) != pen;
    return n;
}

static void amiga_text_pen_past_a_4_colour_screen_is_not_drawn_in_the_background(void)
{
    planar_depth = 2;
    planar_left = 0;
    open_term_in(0, VT_AMIGA);
    pass("\x1b[34mH\x1b[35mH");
    CHECK(cell_pixels_not_in_pen(0, 0, 0) > 0); /* pen 4 is pen 0 here: drawn in the text pen instead */
    CHECK(cell_pixels_not_in_pen(1, 0, 0) > 0); /* pen 5 is pen 1, the text pen: drawn as it is */
    close_term();
    planar_depth = 0;
}

/* Paste: a line break in the clipboard reaches the program as a Return. A lone LF was dropped, so a multi-line
 * paste arrived as one joined line; CR LF is one Return, a lone CR (old Mac text) is one too. */
extern const char *stub_clip_text;
static char pst_keys[16];
static int pst_n;
static void pst_pasted(void *u, const vt_u8 *b, int n, long key)
{
    (void)u; (void)b; (void)n;
    if (pst_n < (int)sizeof pst_keys)
        pst_keys[pst_n++] = key == VT_KEY_RETURN ? 'R' : 'c';
}
static const vtwin_host phost = { 0, dh_input, 0, pst_pasted, dh_raw, 0, 0, 0, 0, 0 };

static void pst_run(const char *clip, const char *want)
{
    pst_n = 0;
    stub_clip_text = clip;
    vtwin_paste(&dw);
    pst_keys[pst_n] = 0;
    CHECK_STR(pst_keys, want);
}

static void a_pasted_line_break_reaches_the_program_as_one_return(void)
{
    drag_open();
    dw.host = &phost;
    pst_run("a\nb", "cRc");        /* LF */
    pst_run("a\r\nb", "cRc");      /* CR LF: one Return, not two */
    pst_run("a\rb", "cRc");        /* lone CR */
    pst_run("a\n\nb", "cRRc");     /* a blank line stays */
    pst_run("a\r\n\r\nb", "cRRc");
    pst_run("a\033b", "cc");       /* controls are still dropped */
    stub_clip_text = 0;
    drag_close();
}

/* ANSI music reaches the player through the console window's own callbacks
 * (render/vtwin.c vtwin_callbacks, as vtwin_attach fills them): the stub
 * below stands in for render/vtmusic.c, which needs audio.device. */
static int mus_calls, mus_n, mus_closes;
static vt_note mus_notes[VT_MUSIC_NOTES];
void vtmusic_play(const vt_note *notes, int n, int background);
void vtmusic_close(void);
void vtmusic_play(const vt_note *notes, int n, int background)
{
    (void)background;
    mus_calls++;
    if (n > VT_MUSIC_NOTES)
        n = VT_MUSIC_NOTES;
    memcpy(mus_notes, notes, sizeof(vt_note) * (size_t)n);
    mus_n = n;
}
void vtmusic_close(void) { mus_closes++; }

static void ansi_music_written_to_a_console_window_reaches_the_player(void)
{
    static const char s[] = "\x1b[MT120 L4 O3 C\x0e";
    vt_callbacks cb;
    int x, y;
    memset(&dw, 0, sizeof dw);
    gf_open(COLS * CW, ROWS * CH, CW, CH, 6);
    vtwin_init(&dw, &dhost, 0);
    vtwin_callbacks(&cb);
    CHECK(cb.music != 0);
    term = vt_new(COLS, ROWS, 100, &cb, &dw);
    vt_set_personality(term, VT_PCANSI);
    dw.t = term;
    dw.win = gf_window();
    vr_init(&dw.r, gf_window(), gf_font(), term, VT_ENC_LATIN1);
    mus_calls = mus_n = 0;
    vtwin_write(&dw, (const vt_u8 *)s, (long)sizeof s - 1);
    CHECK_INT(mus_calls, 1);
    CHECK_INT(mus_n, 1);
    CHECK_INT(mus_notes[0].chz, 52300);
    CHECK_INT(mus_notes[0].ms, 500);
    CHECK_INT(mus_notes[0].on_ms, 437);
    CHECK_INT(vt_note_period(mus_notes[0].chz, 16), 423);
    CHECK_STR(h_screen(term), "");
    vt_cursor(term, &x, &y);
    CHECK_INT(x, 0);
    drag_close();
}

/* K37: the 8-bit golden. Truecolor on deep screens (K38-K41) must not change what an 8-bit screen shows, so
 * this pins today's output first: four streams (a 64x64 f=32 gradient by a=T, the same by a=p c=4, an a=f frame
 * with a 4x4 rectangle and a=a c=2, a 4x4 sixel) on gf_planar(8) (AGA) and on the chunky 8-bit screen
 * (gf_truecolor(0), not planar). Per stream an FNV-1a hash of the window's pens (gf_pixel of every pixel) and
 * of every WriteChunkyPixels buffer (gf_chunky_hash). The constants were captured on commit a6de061d (the engine
 * and renderer sources are byte-identical on this branch's base). A change to the 8-bit output moves a hash. */
extern char *getenv(const char *); /* stdlib.h and the NDK headers clash over struct timeval */
static unsigned long k37_fnv(unsigned long h, unsigned long v)
{
    return ((h ^ (v & 0xFFUL)) * 16777619UL) & 0xFFFFFFFFUL;
}

static void k37_b64(char *o, const unsigned char *s, int n)
{
    static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i;
    for (i = 0; i < n; i += 3) {
        unsigned long v = (unsigned long)s[i] << 16 | (i + 1 < n ? (unsigned long)s[i + 1] << 8 : 0) |
                          (i + 2 < n ? s[i + 2] : 0);
        *o++ = t[(v >> 18) & 63];
        *o++ = t[(v >> 12) & 63];
        *o++ = i + 1 < n ? t[(v >> 6) & 63] : '=';
        *o++ = i + 2 < n ? t[v & 63] : '=';
    }
    *o = 0;
}

/* send `n` raw bytes as the data of one kitty command (keys first), in chunks of 3072 bytes (m=1 ... m=0) */
static void k37_send(const char *keys, const unsigned char *raw, int n)
{
    static char cmd[8192];
    char b[4200];
    int off = 0;
    do {
        int len = n - off > 3072 ? 3072 : n - off, more = off + len < n;
        k37_b64(b, raw + off, len);
        if (off == 0)
            snprintf(cmd, sizeof cmd, "\x1b_G%s,m=%d;%s\x1b\\", keys, more, b);
        else
            snprintf(cmd, sizeof cmd, "\x1b_Gm=%d;%s\x1b\\", more, b);
        pass(cmd);
        off += len;
    } while (off < n);
}

static void k37_gradient(unsigned char *px)
{
    int x, y;
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            unsigned char *p = px + (y * 64 + x) * 4;
            p[0] = (unsigned char)(4 * x);
            p[1] = (unsigned char)(4 * y);
            p[2] = (unsigned char)(2 * (x + y));
            p[3] = x == 0 ? 127 : x == 1 ? 128 : 255;
        }
}

static void k37_scene(int which)
{
    static unsigned char px[64 * 64 * 4];
    unsigned char rect[4 * 4 * 3], root[8 * 8 * 3];
    int i;
    k37_gradient(px);
    switch (which) {
    case 0:
        pass("\x1b[1;1H");
        k37_send("a=T,f=32,s=64,v=64,i=1", px, sizeof px);
        break;
    case 1:
        k37_send("a=t,f=32,s=64,v=64,i=2", px, sizeof px);
        pass("\x1b[1;1H\x1b_Ga=p,i=2,c=4\x1b\\");
        break;
    case 2:
        for (i = 0; i < 8 * 8 * 3; i++)
            root[i] = (unsigned char)(i * 7);
        for (i = 0; i < 4 * 4 * 3; i += 3) {
            rect[i] = 255;
            rect[i + 1] = 0;
            rect[i + 2] = (unsigned char)(i * 5);
        }
        pass("\x1b[1;1H");
        k37_send("a=T,f=24,s=8,v=8,i=3", root, sizeof root);
        k37_send("a=f,i=3,c=1,f=24,s=4,v=4,x=2,y=2", rect, sizeof rect);
        pass("\x1b_Ga=a,i=3,c=2\x1b\\");
        break;
    default:
        pass("\x1b[1;1H\x1bPq\"1;1;4;4#1;2;100;0;0#2;2;0;100;0#1~~~~-#2~~~~\x1b\\");
        break;
    }
}

/* mode: 0 = chunky 8-bit screen, 1 = planar 8 planes (AGA). Fills *pens and *chunky. */
static void k37_run(int planar, int which, unsigned long *pens, unsigned long *chunky, int *calls)
{
    int x, y;
    unsigned long h = 2166136261UL;
    planar_depth = planar ? 8 : 0;
    planar_left = 0;
    open_term_in(0, VT_XTERM);
    vt_set_cell_pixels(term, CW, CH);
    gf_chunky_hash = 2166136261UL;
    gf_chunky_n = 0;
    k37_scene(which);
    for (y = 0; y < ROWS * CH; y++)
        for (x = 0; x < COLS * CW; x++) {
            int v = gf_pixel(x, y);
            h = k37_fnv(k37_fnv(k37_fnv(h, (unsigned long)v), (unsigned long)v >> 8), (unsigned long)v >> 16);
        }
    *pens = h;
    *chunky = gf_chunky_hash;
    *calls = gf_chunky_n;
    close_term();
    planar_depth = 0;
}

static void k37_eight_bit_golden(void)
{
    /* [planar][scene] captured on a6de061d: pens hash, chunky hash. The chunky hashes of the kitty scenes 0-2 were
     * recaptured for S56-UD2-EARTH-OVER-PHOTO: a kitty run now skips its index-0 (transparent) pixels, so the buffers
     * are written in runs of the painted pixels; the pens hashes are unchanged. */
    static const unsigned long gold[2][4][2] = {
        { { 1594208197UL, 2497176488UL }, { 3441995717UL, 1537239861UL }, { 1184631749UL, 461891154UL }, { 584822725UL, 4290872673UL } },
        { { 2071090437UL, 2497176488UL }, { 3456382213UL, 1537239861UL }, { 987634949UL, 461891154UL }, { 412095749UL, 4290872673UL } },
    };
    int m, s;
    for (m = 0; m < 2; m++)
        for (s = 0; s < 4; s++) {
            unsigned long pens, chunky;
            int calls;
            k37_run(m, s, &pens, &chunky, &calls);
            if (getenv("K37_PRINT"))
                printf("K37 planar=%d scene=%d pens=%luUL chunky=%luUL calls=%d\n", m, s, pens, chunky, calls);
            CHECK(calls >= 1); /* the picture reached WriteChunkyPixels: the 8-bit path ran */
            CHECK(pens == gold[m][s][0]);
            CHECK(chunky == gold[m][s][1]);
        }
}

/* K40: RGB pictures on a deep screen. 64x64 gradient (r = 4x, g = 4y, b = 2(x + y)) with the alpha of column 0 given. */
static void k40_gradient(unsigned char *px, int alpha0)
{
    int x, y;
    k37_gradient(px);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            px[(y * 64 + x) * 4 + 3] = x == 0 ? (unsigned char)alpha0 : 255;
}

static void k40_open(int alpha_real)
{
    gf_cgx_alpha(alpha_real);
    open_term_in(0, VT_XTERM);
    gf_truecolor(1);
    vr_free(&rn);
    vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
    vt_set_cell_pixels(term, CW, CH);
    vr_redraw(&rn);
}

static int k40_blend(int s, int d, int a) { return (s * a + d * (255 - a)) / 255; }

static int k40_distinct(int x0, int x1)
{
    static unsigned char seen[1 << 21];
    int x, y, n = 0;
    memset(seen, 0, sizeof seen);
    for (y = 0; y < 64; y++)
        for (x = x0; x < x1; x++) {
            int v = gf_pixel(x, y) & 0xFFFFFF;
            if (!seen[v >> 3] || !(seen[v >> 3] & (1 << (v & 7)))) {
                seen[v >> 3] |= (unsigned char)(1 << (v & 7));
                n++;
            }
        }
    return n;
}

/* The one reachability test: the byte stream -> engine RGBA picture -> img_run -> WritePixelArray. Every pixel of an
 * opaque 4096-colour gradient reaches the screen exact, drawn straight from the view's pixels (SrcMod 256). */
static void k40_opaque_rgb_picture_reaches_the_screen_exact(void)
{
    static unsigned char px[64 * 64 * 4];
    int x, y, bad = 0;
    k40_open(1);
    CHECK_INT((int)vt_kitty_rgb_made(term), 0);
    k40_gradient(px, 255);
    pass("\x1b[1;1H");
    gf_clear_log();
    k37_send("a=T,f=32,s=64,v=64,i=1", px, sizeof px);
    CHECK(rn.n_img_rgb_runs >= 1);
    CHECK(vt_kitty_rgb_made(term) >= 1);
    CHECK(gf_count(GF_CGXPIX) >= 1);
    CHECK_INT(gf_count(GF_CGXLUT), 0);
    CHECK_INT(gf_count(GF_CGXALPHA), 0); /* opaque: no blend needed */
    CHECK_INT((int)gf_lastpix.smod, 256);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            if (gf_pixel(x, y) != (GF_CGX | ((4 * x) << 16) | ((4 * y) << 8) | (2 * (x + y))))
                bad++;
    CHECK_INT(bad, 0);
    CHECK_INT(k40_distinct(0, 64), 4096);
    close_term();
}

/* Translucent: column 0 at alpha 128 over the background is the driver's blend; column 1.. exact. 4033 colours:
 * 4032 exact + the blended column's 64 distinct colours are not in it: count the exact columns only. */
static void k40_translucent_pixels_blend_over_the_background(void)
{
    static unsigned char px[64 * 64 * 4];
    int y, bg, bad = 0;
    k40_open(1);
    pass("\x1b[1;1H");
    bg = gf_pixel(0, 0);
    gf_clear_log();
    k40_gradient(px, 128);
    k37_send("a=T,f=32,s=64,v=64,i=1", px, sizeof px);
    CHECK(gf_count(GF_CGXALPHA) >= 1);
    CHECK_INT(gf_count(GF_CGXPIX), 0);
    CHECK_INT((int)rn.n_img_rgb_soft_blend, 0);
    CHECK_INT((int)gf_lastpix.ga, (int)0xFFFFFFFFUL);
    for (y = 0; y < 64; y++) {
        int want = GF_CGX | (k40_blend(0, bg >> 16 & 255, 128) << 16 | k40_blend(4 * y, bg >> 8 & 255, 128) << 8
                            | k40_blend(2 * y, bg & 255, 128));
        if (!(bg & GF_CGX)) /* the fake treats an unfilled pixel as black */
            want = GF_CGX | (k40_blend(0, 0, 128) << 16 | k40_blend(4 * y, 0, 128) << 8 | k40_blend(2 * y, 0, 128));
        if (gf_pixel(0, y) != want)
            bad++;
        if (gf_pixel(1, y) != (GF_CGX | (4 << 16) | ((4 * y) << 8) | (2 * (1 + y))))
            bad++;
    }
    CHECK_INT(bad, 0);
    CHECK_INT(k40_distinct(1, 64), 4032);
    close_term();
}

/* The software blend's per-channel rule, as the renderer states it: (src*a + dst*(255-a) + 127) / 255. */
static int k40_soft(int s, int d, int a) { return (s * a + d * (255 - a) + 127) / 255; }

/* No working WritePixelArrayAlpha (the rig's P96 42.7): the software blend. Column 0 at alpha 128 over the background
 * is (src*128 + dst*127 + 127) / 255 per channel, computed here: source (0, 4y, 2y) over 0x204060 at y = 8 gives
 * r (0*128 + 32*127 + 127) / 255 = 16, g (32*128 + 64*127 + 127) / 255 = 48, b (16*128 + 96*127 + 127) / 255 = 56,
 * so 0x103038. The run is read once and the alpha call is never made. */
static void k40_soft_blend_gives_the_exact_blended_pixel(void)
{
    static unsigned char px[64 * 64 * 4];
    int y, bad = 0, bg, reads;
    k40_open(0);
    pass("\x1b[1;1H");
    vr_cgx_fill(0, gf_window()->RPort, 0, 0, 64, 64, 0x204060);
    bg = gf_pixel(0, 0);
    CHECK_INT(bg, GF_CGX | 0x204060);
    CHECK_INT(k40_soft(0, 0x20, 128), 0x10);
    CHECK_INT(k40_soft(32, 0x40, 128), 48);
    CHECK_INT(k40_soft(16, 0x60, 128), 56);
    gf_clear_log();
    k40_gradient(px, 128);
    k37_send("a=T,f=32,s=64,v=64,i=1", px, sizeof px);
    reads = gf_count(GF_CGXREAD);
    CHECK_INT(gf_count(GF_CGXALPHA), 0);
    CHECK(rn.n_img_rgb_soft_blend >= 1);
    CHECK_INT(reads, (int)rn.n_img_rgb_soft_blend); /* exactly one read per soft-blended run */
    CHECK_INT(gf_count(GF_CGXPIX), reads);          /* and one write of it */
    CHECK_INT(gf_pixel(0, 8), GF_CGX | 0x103038); /* source (0, 32, 16): r 16, g (32*128+64*127+127)/255 = 48, b 56 */
    for (y = 8; y < 64; y++) { /* row 0..7 is the cursor row, repainted with a pen before the run: the fake reads black */
        int want = GF_CGX | (k40_soft(0, 0x20, 128) << 16 | k40_soft(4 * y, 0x40, 128) << 8 | k40_soft(2 * y, 0x60, 128));
        if (gf_pixel(0, y) != want)
            bad++;
        if (gf_pixel(1, y) != (GF_CGX | (4 << 16) | ((4 * y) << 8) | (2 * (1 + y))))
            bad++;
    }
    CHECK_INT(bad, 0);
    close_term();
    gf_cgx_alpha(1);
}

/* The two ends of the rule: alpha 0 keeps the destination exactly, alpha 255 gives the source exactly. */
static void k40_soft_blend_ends_are_exact(void)
{
    static unsigned char px[64 * 64 * 4];
    int y, bad = 0;
    k40_open(0);
    pass("\x1b[1;1H");
    vr_cgx_fill(0, gf_window()->RPort, 0, 0, 64, 64, 0x204060);
    gf_clear_log();
    k40_gradient(px, 0);
    k37_send("a=T,f=32,s=64,v=64,i=1", px, sizeof px);
    CHECK(rn.n_img_rgb_soft_blend >= 1);
    for (y = 8; y < 64; y++) { /* rows 0..7: the cursor row's pen background reads black, still untouched below */
        if (gf_pixel(0, y) != (GF_CGX | 0x204060)) /* a = 0: the background, untouched */
            bad++;
        if (gf_pixel(7, y) != (GF_CGX | (28 << 16) | ((4 * y) << 8) | (2 * (7 + y)))) /* a = 255: the source */
            bad++;
    }
    CHECK_INT(bad, 0);
    close_term();
    gf_cgx_alpha(1);
}

/* Redraw order: a z<0 picture is drawn before the text in the log (the glyph over it), a z>=0 picture after;
 * a damage of the second cell row alone redraws that row's slice (SrcY 8, 8 rows). */
static int k40_after(enum gf_kind k, const char *txt, int want_text_first)
{
    int i, ki = -1, ti = -1;
    for (i = 0; i < gf_n; i++) {
        if (gf_log[i].kind == k)
            ki = i;
        if (gf_log[i].kind == GF_TEXT && gf_log[i].str[0] == txt[0])
            ti = i;
    }
    return ki >= 0 && ti >= 0 && (want_text_first ? ti < ki : ki < ti);
}

static void k40_redraw_order_and_partial_damage(void)
{
    static unsigned char px[64 * 64 * 4];
    k40_open(1);
    pass("\x1b[1;1HX");
    gf_clear_log();
    k40_gradient(px, 128);
    pass("\x1b[1;1H");
    k37_send("a=T,f=32,s=64,v=64,i=1,z=-1,C=1,c=8,r=8", px, sizeof px);
    CHECK(k40_after(GF_CGXALPHA, "X", 0)); /* under: the picture first, the glyph over it */
    CHECK(gf_count(GF_CGXALPHA) >= 1);
    gf_clear_log();
    vr_damage(&rn, 0, 1, COLS, 2); /* the second cell row only */
    CHECK_INT(gf_count(GF_CGXALPHA), 1); /* only the damaged row's slice */
    close_term();
    k40_open(1);
    pass("\x1b[1;1HX");
    gf_clear_log();
    k37_send("a=T,f=32,s=64,v=64,i=2,z=1,C=1,c=8,r=8", px, sizeof px);
    gf_clear_log();
    vr_damage(&rn, 0, 0, COLS - 1, ROWS - 1);
    vr_redraw(&rn);
    CHECK(k40_after(GF_CGXALPHA, "X", 1)); /* over: the glyph first */
    close_term();
}

/* Order with text: a z<0 picture is drawn before the glyph, which stays over it. A z>=0 kitty picture lies over its
 * cells, which keep their text and background (O6, K40d): the glyph and the cell background are drawn first and the
 * picture blends with what the rastport then holds. Row 0 is the cursor row (pen-painted, the fake reads it as
 * black); row 1 holds a blank with an RGB blue background (a FillPixelArray, read back as its colour), row 2 an X.
 * Column 0 (alpha 128), pixel row 12: source (0, 48, 24) over the blue (0, 0, 238). Before K40d the picture blanked
 * the cells to the default colours and the blend read black there. */
static void k40_soft_blend_reads_the_text_under_a_front_picture(void)
{
    static unsigned char px[64 * 64 * 4];
    k40_open(0);
    pass("\x1b[1;1HX");
    k40_gradient(px, 128);
    pass("\x1b[1;1H");
    gf_clear_log();
    k37_send("a=T,f=32,s=64,v=64,i=1,z=-1,C=1,c=8,r=8", px, sizeof px);
    CHECK(k40_after(GF_CGXPIX, "X", 0));
    CHECK(!(gf_pixel(5, 5) & GF_CGX)); /* a glyph pixel (the fake paints the glyph code), not the picture's colour */
    CHECK(rn.n_img_rgb_soft_blend >= 1);
    close_term();
    k40_open(0);
    pass("\x1b[2;1H\x1b[48;2;0;0;238m \x1b[3;1HX\x1b[0m\x1b[1;1H");
    gf_clear_log();
    k37_send("a=T,f=32,s=64,v=64,i=2,z=1,C=1,c=8,r=8", px, sizeof px);
    gf_clear_log();
    vr_damage(&rn, 0, 0, COLS - 1, ROWS - 1);
    vr_redraw(&rn);
    CHECK(gf_count(GF_CGXREAD) >= 1);
    CHECK(k40_after(GF_CGXPIX, "X", 1)); /* the glyph under the picture is drawn, before it */
    CHECK_INT(gf_pixel(0, 12),
              GF_CGX | (k40_soft(0, 0, 128) << 16) | (k40_soft(48, 0, 128) << 8) | k40_soft(24, 238, 128));
    /* the cursor row: the blend over what the read returned (black) */
    CHECK_INT(gf_pixel(0, 5), GF_CGX | (k40_soft(0, 0, 128) << 16) | (k40_soft(20, 0, 128) << 8) | k40_soft(10, 0, 128));
    close_term();
    gf_cgx_alpha(1);
}

/* Scrollback: an opaque RGB picture scrolled three rows into the history redraws exact through the RGB path when the
 * view goes back (its sixty-four rows come from the history rows and the live ones), with no indexed LUT draw. */
static void k40_picture_scrolled_into_history_redraws_exact(void)
{
    static unsigned char px[64 * 64 * 4];
    int x, y, bad = 0, runs;
    k40_open(1);
    k40_gradient(px, 255);
    pass("\x1b[1;1H");
    k37_send("a=T,f=32,s=64,v=64,i=1", px, sizeof px);
    pass("\x1b[8;1H\r\n\r\n\r\n"); /* the picture moves three rows up, into the history */
    vr_set_view(&rn, 0);
    runs = (int)rn.n_img_rgb_runs;
    gf_clear_log();
    vr_set_view(&rn, 3); /* the view back: the picture is whole again */
    CHECK((int)rn.n_img_rgb_runs > runs);
    CHECK(gf_count(GF_CGXPIX) >= 1);
    CHECK_INT(gf_count(GF_CGXLUT), 0);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++)
            if (gf_pixel(x, y) != (GF_CGX | ((4 * x) << 16) | ((4 * y) << 8) | (2 * (x + y))))
                bad++;
    CHECK_INT(bad, 0);
    close_term();
}

/* vr_cgx_has_alpha's slot check on a fake library block: a short table (older V41) and slots that are not JMPs
 * give 0 whatever the bytes hold; a -216 with its own target gives 1; the rig's one-stub table gives 0. */
static void k40_alpha_detection_checks_the_table_size_and_the_jmp_opcodes(void)
{
    static unsigned char blk[512];
    unsigned char *base = blk + 256;
    int i;
    for (i = 204; i <= 228; i += 6) { /* every slot a JMP to the same stub */
        base[-i] = 0x4E; base[-i + 1] = 0xF9;
        base[-i + 2] = 0x00; base[-i + 3] = 0xC0; base[-i + 4] = 0x00; base[-i + 5] = 0x10;
    }
    CHECK_INT(vr_cgx_slots_have_alpha(base, 240), 0);
    base[-216 + 4] = 0x20; /* -216 has its own target */
    CHECK_INT(vr_cgx_slots_have_alpha(base, 240), 1);
    CHECK_INT(vr_cgx_slots_have_alpha(base, 200), 0);
    CHECK_INT(vr_cgx_slots_have_alpha(base, 227), 0);
    CHECK_INT(vr_cgx_slots_have_alpha(base, 228), 1);
    base[-222] = 0; /* a slot that is no JMP */
    CHECK_INT(vr_cgx_slots_have_alpha(base, 240), 0);
}

/* An 8-bit screen keeps today's indexed pictures: no RGB picture is made. */
static void k40_eight_bit_screen_stays_indexed(void)
{
    static unsigned char px[64 * 64 * 4];
    open_term_in(0, VT_XTERM);
    gf_truecolor(0);
    vr_free(&rn);
    vr_init(&rn, gf_window(), gf_font(), term, VT_ENC_LATIN1);
    vt_set_cell_pixels(term, CW, CH);
    k40_gradient(px, 255);
    k37_send("a=T,f=32,s=64,v=64,i=1", px, sizeof px);
    CHECK_INT((int)vt_kitty_rgb_made(term), 0);
    CHECK_INT((int)rn.n_img_rgb_runs, 0);
    CHECK_INT(gf_count(GF_CGXPIX), 0);
    close_term();
}

/* S56-UD2-EARTH-OVER-PHOTO: on the palette path (no RGB screen) a kitty picture's alpha-0 pixels are index 0, which
 * painted the background opaque over a z<0 picture beneath (black corners round the UPDemo globe). Kitty composes
 * over what is beneath: those pixels never paint. A 32x32 red picture with 4x4 transparent corners (z=0) over a 64x64
 * blue photo (z=-1): the corners keep the photo's pen, the red body is red, the photo outside the picture is blue. */
static void s56_transparent_kitty_pixels_keep_the_picture_beneath_on_the_palette_path(void)
{
    static unsigned char photo[64 * 64 * 4], globe[32 * 32 * 4];
    int x, y, i;
    for (i = 0; i < 64 * 64; i++) {
        photo[i * 4] = 0;
        photo[i * 4 + 1] = 0;
        photo[i * 4 + 2] = 255;
        photo[i * 4 + 3] = 255;
    }
    for (y = 0; y < 32; y++)
        for (x = 0; x < 32; x++) {
            unsigned char *q = globe + (y * 32 + x) * 4;
            int corner = (x < 4 || x >= 28) && (y < 4 || y >= 28);
            q[0] = 255;
            q[1] = 255;
            q[2] = 0;
            q[3] = corner ? 0 : 255;
        }
    k40_open(1);
    vt_set_image_rgb(term, 0); /* the palette path: pictures stored as indices, drawn through the colour table */
    pass("\x1b[1;1H");
    k37_send("a=T,f=32,s=64,v=64,i=1,z=-1,C=1,c=8,r=8", photo, sizeof photo);
    pass("\x1b[3;3H");
    k37_send("a=T,f=32,s=32,v=32,i=2,z=0,C=1,c=4,r=4", globe, sizeof globe);
    pass("\x1b[8;20H"); /* the cursor cell away from the corners */
    CHECK_INT((int)vt_kitty_rgb_made(term), 0); /* the palette path */
    CHECK(rn.n_img_runs >= 1);
    CHECK(gf_count(GF_CGXLUT) >= 1);
    CHECK_INT(gf_pixel(4, 4), gf_pixel(60, 60));   /* the photo, away from the globe */
    CHECK(gf_pixel(4, 4) != 32);                   /* the photo is painted, not the background */
    CHECK(gf_pixel(32, 32) != 32);                 /* the globe's body is painted too */
    CHECK_INT(gf_pixel(16, 16), gf_pixel(4, 4));   /* top left corner: the photo */
    CHECK_INT(gf_pixel(47, 16), gf_pixel(4, 4));   /* top right */
    CHECK_INT(gf_pixel(16, 47), gf_pixel(4, 4));   /* bottom left */
    CHECK_INT(gf_pixel(47, 47), gf_pixel(4, 4));   /* bottom right */
    close_term();
}

/* K36-ANIM-BLINK: a looping kitty animation switches frames on the clock; the repaint of the picture's rows
 * must never leave more than the one cell row being drawn showing the background. Before the fix draw_rows_now
 * painted the text (background) of every damaged row first and only then the images of all of them, top down:
 * a screen refresh between the two showed the upper rows of the new frame and the rest blank (the owner's
 * "the picture blinks, only its top fifth drawn"). The log of the redraw is walked: a fill or text over an image
 * row blanks it, an image write over it draws it; at most one row may be blank at any call. */
static int k36_max_blank_rows(int img_rows)
{
    int blank[16] = { 0 }, i, y, most = 0, n;
    for (i = 0; i < gf_n; i++) {
        const gf_call *g = &gf_log[i];
        long y0, y1;
        int img = g->kind == GF_CGXLUT || g->kind == GF_CGXPIX;
        if (g->a >= 16) /* right of the picture's two cells */
            continue;
        if (g->kind == GF_RECT) {
            y0 = g->b;
            y1 = g->d + 1;
        } else if (g->kind == GF_CGXFILL || img) {
            y0 = g->b;
            y1 = g->b + g->d;
        } else if (g->kind == GF_TEXT) {
            y0 = g->b;
            y1 = g->b + CH;
        } else {
            continue;
        }
        for (y = 0; y < img_rows; y++)
            if (y0 < (y + 1) * CH && y1 > y * CH)
                blank[y] = !img;
        for (n = 0, y = 0; y < img_rows; y++)
            n += blank[y];
        if (n > most)
            most = n;
    }
    return most;
}

static void k36_a_frame_switch_never_shows_the_picture_half_drawn(void)
{
    static unsigned char fr[3][16 * 32 * 3];
    int f, i, loop, worst = 0;
    unsigned long sw0;
    k30_open_truecolour();
    for (f = 0; f < 3; f++)
        for (i = 0; i < 16 * 32 * 3; i += 3) {
            fr[f][i] = (unsigned char)(f == 0 ? 255 : 0);
            fr[f][i + 1] = (unsigned char)(f == 1 ? 255 : 0);
            fr[f][i + 2] = (unsigned char)(f == 2 ? 255 : 0);
        }
    pass("\x1b[1;1H");
    k37_send("a=T,f=24,s=16,v=32,i=1,q=2", fr[0], sizeof fr[0]); /* 2 x 4 cells */
    k37_send("a=f,f=24,s=16,v=32,i=1,q=2", fr[1], sizeof fr[1]);
    k37_send("a=f,f=24,s=16,v=32,i=1,q=2", fr[2], sizeof fr[2]);
    pass("\x1b_Ga=a,i=1,r=1,z=90,q=2\x1b\\\x1b_Ga=a,i=1,r=2,z=90,q=2\x1b\\\x1b_Ga=a,i=1,r=3,z=90,q=2\x1b\\");
    pass("\x1b[6;1H\x1b_Ga=a,i=1,s=3,q=2\x1b\\");
    sw0 = vt_kitty_frame_switches(term);
    CHECK_INT(vt_anim_tick(term, 0), 90000);
    for (loop = 0; loop < 7; loop++) { /* two loops and more, the wraps 3 -> 1 among them */
        gf_clear_log();
        vt_anim_tick(term, 90000);
        pass("");
        CHECK(gf_count(GF_CGXPIX) + gf_count(GF_CGXLUT) >= 1); /* the new frame was drawn */
        i = k36_max_blank_rows(4);
        if (i > worst)
            worst = i;
    }
    CHECK_INT((int)(vt_kitty_frame_switches(term) - sw0), 7); /* the sentinel: the clock switched every time */
    CHECK_INT(gf_pixel(0, 31), GF_CGX | 0x00FF00); /* 7 switches from frame 1: frame 2, its last row drawn */
    CHECK_INT(worst, 1);
    close_term();
}

/* A z >= 0 kitty picture of several cell rows lies over the text of every row it covers. Each row's slice must
 * be the last thing drawn in its band: no glyph of a lower row may follow (and cover) the picture. */
static void a_front_picture_spanning_rows_is_not_overdrawn_by_the_text_of_its_lower_rows(void)
{
    static unsigned char px[16 * 32 * 3];
    int i, y, last_img[4], last_txt[4], drawn = 0, bad = 0;
    k30_open_truecolour();
    for (i = 0; i < (int)sizeof px; i += 3) {
        px[i] = 255;
        px[i + 1] = px[i + 2] = 0;
    }
    pass("\x1b[2;1HAAAA\x1b[3;1HBBBB\x1b[4;1HCCCC\x1b[5;1HDDDD\x1b[2;1H");
    pass("");
    k37_send("a=T,f=24,s=16,v=32,i=1,z=1,C=1,c=2,r=4,q=2", px, sizeof px); /* 2 x 4 cells over rows 2..5 */
    gf_clear_log();
    vr_damage(&rn, 0, 0, COLS - 1, ROWS - 1);
    vr_redraw(&rn);
    for (y = 0; y < 4; y++)
        last_img[y] = last_txt[y] = -1;
    for (i = 0; i < gf_n; i++) {
        const gf_call *g = &gf_log[i];
        int img = g->kind == GF_CGXLUT || g->kind == GF_CGXPIX;
        if (g->a >= 16 || (!img && g->kind != GF_TEXT))
            continue;
        for (y = 0; y < 4; y++) {
            long band = (long)(1 + y) * CH;
            if (g->b >= band && g->b < band + CH)
                *(img ? &last_img[y] : &last_txt[y]) = i;
        }
    }
    for (y = 0; y < 4; y++) {
        drawn += last_img[y] >= 0;
        bad += last_txt[y] > last_img[y];
    }
    CHECK_INT(drawn, 4); /* every row of the picture was drawn */
    CHECK_INT(bad, 0);   /* and none of them was covered by text afterwards */
    close_term();
}

/* IMAGE-ASPECT-HIRES: handler open_window reads the screen (vtwin_set_screen) before vtwin_attach creates the
 * engine, so attach must hand the pixel aspect to the new engine. On 640x256 hires (1:2 pixels) a kitty picture
 * placed with no cell counts halves its height (7 -> 4); an unknown aspect leaves it as sent. */
static int attach_picture_height(int known)
{
    vtwin aw;
    vt_image_view v;
    char seq[200];
    int h = -1, i;
    memset(&aw, 0, sizeof aw);
    gf_open(COLS * CW, ROWS * CH, CW, CH, 6);
    vtwin_init(&aw, &dhost, 0);
    aw.aspect_known = known;
    aw.res_x = 22;
    aw.res_y = 44;
    aw.font = gf_font();
    if (vtwin_attach(&aw, gf_window())) {
        vt_set_cell_pixels(aw.t, 8, 16);
        strcpy(seq, "\033_Ga=T,f=24,s=4,v=7,q=2;");
        for (i = 0; i < 112; i++)
            strcat(seq, "A"); /* 84 zero bytes: a 4 x 7 RGB picture */
        strcat(seq, "\033\\");
        vt_feed(aw.t, (const vt_u8 *)seq, (long)strlen(seq));
        vt_flush(aw.t);
        if (vt_row_image(aw.t, 0, 0, &v))
            h = v.h;
        vr_free(&aw.r);
        vt_free(aw.t);
    }
    gf_close();
    return h;
}

static void vtwin_attach_hands_the_screens_pixel_aspect_to_the_new_engine(void)
{
    CHECK_INT(attach_picture_height(1), 4);
    CHECK_INT(attach_picture_height(0), 7);
}

int main(void)
{
    vtwin_attach_hands_the_screens_pixel_aspect_to_the_new_engine();
    an_inert_renderer_has_no_selection();
    the_default_rect_makes_the_calls_the_renderer_always_made();
#ifndef RECT_ABSENT
    a_rect_inside_a_larger_window_keeps_every_draw_inside_it();
    a_planar_rect_keeps_every_draw_inside_it(4, 16, 20);
    a_planar_rect_keeps_every_draw_inside_it(4, 0, 40);
    a_planar_rect_keeps_every_draw_inside_it(2, 24, 0);
#endif
    a_pasted_line_break_reaches_the_program_as_one_return();
    pen_matches_on_a_prepared_palette_are_the_bare_palettes_picks();
    a_second_window_on_the_same_palette_matches_no_colour_again();
    a_kitty_image_draws_its_pixels_on_the_canvas();
    k30_text_lies_over_a_picture_below();
    k30_far_below_stays_under_a_coloured_background();
    k32_placeholder_cells_draw_their_tiles();
    k37_eight_bit_golden();
    k36_a_frame_switch_never_shows_the_picture_half_drawn();
    a_front_picture_spanning_rows_is_not_overdrawn_by_the_text_of_its_lower_rows();
    k40_opaque_rgb_picture_reaches_the_screen_exact();
    k40_translucent_pixels_blend_over_the_background();
    k40_soft_blend_gives_the_exact_blended_pixel();
    k40_soft_blend_ends_are_exact();
    k40_redraw_order_and_partial_damage();
    k40_soft_blend_reads_the_text_under_a_front_picture();
    k40_picture_scrolled_into_history_redraws_exact();
    k40_alpha_detection_checks_the_table_size_and_the_jmp_opcodes();
    k40_eight_bit_screen_stays_indexed();
    s56_transparent_kitty_pixels_keep_the_picture_beneath_on_the_palette_path();
    the_cursor_pass_counts_its_gfx_calls_and_unchanged_cells();
    scrollback_view_move_blits_and_draws_only_the_exposed_rows();
    cursor_glyph_is_drawn_at_the_cursor_cell_from_the_grid_row();
    the_default_cursor_is_the_cell_with_its_colours_swapped_not_its_pens_complemented();
    an_unfocused_pane_draws_a_hollow_cursor_and_the_focused_one_a_solid_block();
    direct_colour_background_in_the_amiga_personality_is_drawn_in_its_colour();
    con_window_draws_pen_zero_with_its_own_ground();
    amiga_text_pen_past_a_4_colour_screen_is_not_drawn_in_the_background();
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
    an_antialiased_edge_over_a_dither_is_blended_with_the_pen_under_it_and_the_knob_moves_it();
    a_dithered_cell_shows_its_colour_closer_than_the_old_single_pen();
    a_dithered_frame_composed_and_drawn_in_place_has_the_same_pixels();
    screens_of_more_than_16_pens_are_not_dithered();
    a_half_block_of_two_colours_sharing_a_pen_is_drawn_in_that_pen();
    drawing_never_touches_the_window_borders_at_an_odd_inner_width();
    the_strips_beside_the_grid_and_blank_cells_show_the_window_background();
    dither_off_draws_the_nearest_single_pen_and_on_dithers_again();
    padding_extend_gives_the_strips_the_colour_of_the_edge_cells();
    an_outline_glyph_is_antialiased_on_a_true_colour_screen_and_not_when_off();
    an_outline_glyph_uses_the_shade_pens_a_palette_has_and_the_mask_when_none();
    selection_of_a_renderer_not_yet_initialised_is_none_and_reads_no_terminal();
    a_drag_ends_when_the_left_button_is_up_or_the_window_goes_inactive();
    ansi_music_written_to_a_console_window_reaches_the_player();
    printf("render: %d checks, %d failures\n", h_checks, h_failures);
    return h_failures ? 1 : 0;
}
