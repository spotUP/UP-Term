/* demo/updemo through the engine: every scene's output is sequences the
 * terminal knows, and the screen comes back as it was. The demo
 * reads the terminal's answers (the window's size) and mouse reports:
 * the engine's replies go back in as input here. */
#include <string.h>
#include "harness.h"
#include "../engine/vtengine.h"
#include "../demo/updemo.h"

static vt_term *t;
static long now, keys_left, bytes;
static int next_key;
static const char *inject;            /* typed in at keys_left == 0, instead of next_key */
static char back[256];                /* the terminal's replies, read back as input */
static int back_len, back_pos;
static int layout_cols[8], n_layout;  /* DECCOLM widths the terminal was asked for */
static int saw_image, saw_link, saw_wide, saw_text, saw_slow, saw_rapid;
static const char *needle;            /* saw_text: the output had it */

static void scan(void)
{
    int y, x, n;
    if (vt_images(t))
        saw_image = 1;
    for (y = 0; y < vt_rows(t); y++) {
        const vt_cell *c = vt_row(t, y, &n);
        for (x = 0; x < n; x++) {
            if (c[x].attr & VT_ATTR_BLINK) {
                if (c[x].attr & VT_ATTR_RAPID)
                    saw_rapid = 1;
                else
                    saw_slow = 1;
            }
            if (c[x].width == 2)
                saw_wide = 1;
            if (c[x].ext && vt_cell_link(t, &c[x]))
                saw_link = 1;
        }
    }
}

static void out(void *user, const char *buf, long len)
{
    (void)user;
    bytes += len;
    if (needle && len >= (long)strlen(needle)) {
        long i, k = (long)strlen(needle);
        for (i = 0; i + k <= len && !saw_text; i++)
            if (!memcmp(buf + i, needle, (size_t)k))
                saw_text = 1;
    }
    vt_write(t, (const unsigned char *)buf, len);
    scan();
}

static void reply(void *user, const vt_u8 *buf, long len)
{
    (void)user;
    if (back_pos == back_len)
        back_pos = back_len = 0;
    if (back_len + len <= (long)sizeof(back)) {
        memcpy(back + back_len, buf, (size_t)len);
        back_len += (int)len;
    }
}

static void layout(void *user, int which, int value)
{
    (void)user;
    if (which == VT_LAYOUT_COLUMNS && n_layout < 8)
        layout_cols[n_layout++] = value;
}

/* a virtual clock: the wait passes at once */
static int key(void *user, int wait_ms)
{
    (void)user;
    if (back_pos < back_len)
        return (unsigned char)back[back_pos++];
    now += wait_ms / 20 + 1;
    if (inject) {
        int c;
        if (keys_left-- > 0)
            return -1;
        c = (unsigned char)*inject++;
        if (!*inject)
            inject = 0;
        return c;
    }
    if (next_key >= 0 && keys_left-- == 0)
        return next_key;
    return -1;
}

static long ticks(void *user)
{
    (void)user;
    return now;
}

static updemo_io io = { out, key, ticks, 0 };

static char line[200];

static const char *row_text(int row)
{
    int n, i;
    const vt_cell *c = vt_row(t, row, &n);
    if (!c)
        return "";
    for (i = 0; i < n && i < (int)sizeof(line) - 1; i++)
        line[i] = (char)c[i].ch;
    while (i && line[i - 1] == ' ')
        i--;
    line[i] = 0;
    return line;
}

static void start(int cols, int rows)
{
    static vt_callbacks cb;
    cb.reply = reply;
    cb.layout = layout;
    t = vt_new(cols, rows, 100, &cb, 0);
    vt_write(t, (const unsigned char *)"before the demo\r\n$ ", 19);
    now = bytes = 0;
    next_key = -1;
    inject = 0;
    needle = 0;
    back_len = back_pos = 0;
    n_layout = 0;
    saw_image = saw_link = saw_wide = saw_text = saw_slow = saw_rapid = 0;
}

static int scene_index(const char *name)
{
    int s;
    for (s = 0; s < updemo_scenes(); s++)
        if (!strcmp(updemo_scene_name(s), name))
            return s;
    return -1;
}

/* ONE route (owner 2026-10-13: "why is there still two different routes in the demo?"): the program has one scene
 * list, the captioned one. UPDemo, UPDemo n and Help > Demo tour (/demo) are all updemo_run over it; there is no
 * second player, so no second list can hold scenes the first lacks. */
static void there_is_one_scene_list_with_the_effects_at_its_end(void)
{
    CHECK_INT(updemo_scenes(), 24);
    CHECK_INT(scene_index("styles"), 1);        /* the tour's scene, in the list a bare UPDemo plays */
    CHECK_INT(scene_index("copper bars"), 16);  /* the show's scenes follow the tour's */
    CHECK_INT(scene_index("the end"), 23);
}

/* a bare UPDemo plays the whole list from its first scene: the status line shows the caption and "1/24" */
static void a_bare_run_starts_at_the_first_scene_with_its_caption(void)
{
    start(80, 24);
    needle = "Demo 1/24  Text styles, double-size lines, box drawing";
    CHECK_INT(updemo_run(&io, 80, 24, 0, 6), 0);
    CHECK(saw_text);
    needle = 0;
    vt_free(t);
}

/* UPDemo 2 starts at the second scene of the same list */
static void a_start_number_picks_a_scene_of_the_one_list(void)
{
    start(80, 24);
    needle = "Demo 2/24  Every text style a program can ask for";
    CHECK_INT(updemo_run(&io, 80, 24, 1, 6), 0);
    CHECK(saw_text);
    needle = "Demo 1/24";
    saw_text = 0;
    vt_free(t);
    start(80, 24);
    needle = "Demo 1/24";
    CHECK_INT(updemo_run(&io, 80, 24, 1, 6), 0);
    CHECK(!saw_text);   /* scene 1 was skipped */
    needle = 0;
    vt_free(t);
}

/* UPDemo BENCH: the benchmarked scenes of the same list, the effects among them */
static void bench_picks_the_effect_scenes_of_the_one_list(void)
{
    long frames, tk;
    int s, n = 0;
    start(80, 24);
    updemo_bench(1);
    CHECK_INT(updemo_run(&io, 80, 24, 0, 6), 0);
    updemo_bench(0);
    for (s = 0; s < updemo_scenes(); s++) {
        updemo_stats(s, &frames, &tk);
        CHECK_INT(frames > 0, updemo_scene_benchmarked(s));
        n += frames > 0;
    }
    CHECK_INT(n, 11);
    CHECK(updemo_scene_benchmarked(scene_index("plasma")));
    CHECK(!updemo_scene_benchmarked(scene_index("tmux")));
    vt_free(t);
}

/* the biggest window the buffers take, and a bigger one (clamped) */
static void big_windows_are_drawn_within_bounds(void)
{
    start(132, 60);
    CHECK_INT(updemo_run(&io, 132, 60, 0, 6), 0);
    CHECK(!(vt_modes(t) & VT_MODE_ALT_SCREEN)); /* the reflow scene draws on the main screen */
    vt_free(t);
    start(200, 80);
    CHECK_INT(updemo_run(&io, 200, 80, 0, 6), 0);
    vt_free(t);
}

static void a_small_window_is_refused_untouched(void)
{
    start(60, 20);
    CHECK_INT(updemo_run(&io, 60, 20, 0, 6), 1);
    CHECK_INT(bytes, 0);
    CHECK_INT(updemo_run(&io, 60, 20, 0, 6), 1);
    CHECK_INT(bytes, 0);
    vt_free(t);
}

/* ---- the tour (Help > Demo tour) ---- */

/* Every scene of the tour plays, with what it is there to show reaching
 * the terminal: a sixel image, a hyperlink, wide characters. What the
 * scenes changed is put back: the mouse, the palette, the screen. */
static void the_demo_plays_every_scene_and_puts_the_terminal_back(void)
{
    const char *kinds[16];
    long counts[16], frames, tk;
    int s, all_ran = 1;
    start(80, 24);
    CHECK_INT(updemo_run(&io, 80, 24, 0, 90), 0); /* long enough for each scene to reach its point */
    CHECK_INT(vt_unhandled(t, kinds, counts, 16), 0);
    if (vt_unhandled(t, kinds, counts, 16))
        CHECK_STR(kinds[0], "");
    for (s = 0; s < updemo_scenes(); s++) {
        updemo_stats(s, &frames, &tk);
        if (frames < 1) {
            all_ran = 0;
            CHECK_STR(updemo_scene_name(s), "a scene that drew no frame");
        }
    }
    CHECK(all_ran);
    CHECK(saw_image);
    CHECK(saw_link);
    CHECK(saw_wide);
    CHECK(!(vt_modes(t) & VT_MODE_ALT_SCREEN));
    CHECK(vt_modes(t) & VT_MODE_CURSOR_VISIBLE);
    CHECK(!(vt_modes(t) & (VT_MODE_MOUSE_NORMAL | VT_MODE_MOUSE_SGR)));
    CHECK_INT(vt_palette_rgb(t, 1), vt_palette_rgb(0, 1)); /* the themes' colours went back */
    CHECK_INT(vt_cols(t), 80);
    /* the reflow scene drew on the main screen: the shell's lines went into
     * the scrollback, not away */
    CHECK(vt_scrollback_lines(t) >= 2);
    vt_free(t);
}

/* W24 (the owner: the emoji rule does not apply to the demo): the Unicode
 * scene shows emoji, two cells wide */
static void the_unicode_scene_shows_emoji(void)
{
    start(80, 24);
    needle = "\360\237\230\200";   /* U+1F600 */
    saw_text = 0;
    CHECK_INT(updemo_run(&io, 80, 24, 0, 90), 0);
    CHECK(saw_text);
    needle = 0;
    vt_free(t);
}

/* W-T2 (owner: the styles scene's two blink speeds blink at the same rate): its "rapid blink" line is SGR 5 then 6,
 * so a terminal that knows 6 blinks it fast and one that does not still blinks it. It sent 6;5: the 5 after the 6
 * asked for the slow rate again (ECMA-48 8.3.117: 5 and 6 are alternatives, the last one counts). */
static void the_styles_scene_shows_slow_and_rapid_blink(void)
{
    start(80, 24);
    updemo_only(1);
    CHECK_INT(updemo_run(&io, 80, 24, scene_index("styles"), 150), 0);
    updemo_only(0);
    CHECK(saw_slow);
    CHECK(saw_rapid);
    vt_free(t);
}

static void q_ends_the_demo_and_any_other_key_does_not(void)
{
    long all;
    start(80, 24);
    updemo_run(&io, 80, 24, 0, 12);
    all = bytes;
    vt_free(t);
    start(80, 24);
    next_key = 'q';
    keys_left = 3;
    CHECK_INT(updemo_run(&io, 80, 24, 0, 0), 0);
    CHECK(bytes < all / 4);
    CHECK(!(vt_modes(t) & VT_MODE_ALT_SCREEN));
    CHECK_STR(row_text(0), "before the demo");
    vt_free(t);
    start(80, 24);
    inject = "\033"; /* Esc */
    keys_left = 3;
    CHECK_INT(updemo_run(&io, 80, 24, 0, 0), 0);
    CHECK(bytes < all / 4);
    vt_free(t);
}

/* W-D2 (owner 2026-10-14: the demo must not exit on a key; a key skips to the next scene). Every scene runs 40
 * ticks; `seq` is typed after 3 polls, in scene `first`. The frames and ticks each scene got say where the demo went. */
static long fr[24], tk[24];

static int run_keys(int first, const char *seq)
{
    int s, rc;
    start(80, 24);
    inject = seq;
    keys_left = 3;
    rc = updemo_run(&io, 80, 24, first, 40);
    for (s = 0; s < 24; s++)
        updemo_stats(s, &fr[s], &tk[s]);
    return rc;
}

static void a_key_goes_to_the_next_scene_and_the_demo_keeps_running(void)
{
    const char *seq[] = { " ", "\r", "\n", "\033[C", "\033OC" };
    int i;
    for (i = 0; i < 5; i++) {
        CHECK_INT(run_keys(1, seq[i]), 0);
        CHECK(tk[1] < 40);                       /* cut short */
        CHECK(tk[2] >= 40);                      /* the next scene ran its time out */
        CHECK(fr[23] > 0);                       /* and the demo went on to the last scene */
        vt_free(t);
    }
}

static void left_and_b_go_back_a_scene(void)
{
    const char *seq[] = { "b", "B", "\033[D", "\033OD" };
    int i;
    for (i = 0; i < 4; i++) {
        CHECK_INT(run_keys(2, seq[i]), 0);
        CHECK(fr[1] > 0); /* the scene before was played again */
        CHECK(tk[1] >= 40);
        CHECK(fr[23] > 0);
        vt_free(t);
    }
}

static void q_esc_and_ctrl_c_end_the_demo(void)
{
    const char *seq[] = { "q", "Q", "\033", "\003" };
    int i;
    for (i = 0; i < 4; i++) {
        CHECK_INT(run_keys(1, seq[i]), 0);
        CHECK_INT((int)fr[2], 0); /* nothing after the scene it was typed in */
        CHECK(!(vt_modes(t) & VT_MODE_ALT_SCREEN));
        CHECK_STR(row_text(0), "before the demo");
        vt_free(t);
    }
}

/* ESC [ A is a cursor key, not an Esc followed by a bracket; other keys and replies do nothing */
static void an_arrow_key_sequence_is_not_taken_as_esc_and_a_mouse_report_is_no_key(void)
{
    const char *seq[] = { "\033[A", "\033[B", "\033OA", "\033[1;5C", "x", "\033[<0;10;12M", "\033[<0;10;12m", "\033[M \"#" };
    int i;
    for (i = 0; i < 8; i++) {
        CHECK_INT(run_keys(1, seq[i]), 0);
        CHECK(tk[1] >= 40);  /* the scene was not left */
        CHECK(fr[23] > 0);
        vt_free(t);
    }
}

/* a click arrives as an SGR mouse report: the mouse scene shows it, and
 * the tour goes on */
static void the_mouse_scene_shows_a_click(void)
{
    int s = scene_index("mouse");
    CHECK(s >= 0);
    start(80, 24);
    inject = "\033[<0;10;12M";
    keys_left = 2;
    needle = "button 1 pressed at column 10, row 12";
    CHECK_INT(updemo_run(&io, 80, 24, s, 0), 0);
    CHECK(saw_text);
    CHECK(!(vt_modes(t) & VT_MODE_MOUSE_SGR));
    vt_free(t);
}

/* DECCOLM from 80 columns, and back to 80 however the scene ends */
static void the_resize_scene_leaves_the_width_as_it_was(void)
{
    int s = scene_index("resize");
    CHECK(s >= 0);
    start(80, 24);
    CHECK_INT(updemo_run(&io, 80, 24, s, 12), 0); /* ends before the scene's own 80 */
    CHECK(n_layout >= 2);
    CHECK_INT(layout_cols[0], 132);
    CHECK_INT(layout_cols[n_layout - 1], 80);
    vt_free(t);
    start(100, 30); /* not 80 wide: the scene only tells */
    CHECK_INT(updemo_run(&io, 100, 30, s, 12), 0);
    CHECK_INT(n_layout, 0);
    vt_free(t);
}

/* the tour asks the size before each scene: a window made too small ends it */
static void a_window_made_too_small_ends_the_demo(void)
{
    long all;
    start(80, 24);
    updemo_run(&io, 80, 24, 0, 12);
    all = bytes;
    vt_free(t);
    start(80, 24);
    vt_resize(t, 60, 20); /* before the first scene's question */
    CHECK_INT(updemo_run(&io, 80, 24, 0, 12), 0);
    CHECK(bytes < all / 10);
    CHECK(!(vt_modes(t) & VT_MODE_ALT_SCREEN));
    vt_free(t);
}

/* ---- what a frame costs the terminal (ledger S2) ---- */

/* A console like ours: the output is drawn when the program looks for a
 * key (WaitForChar draws what is pending first), the drawing takes
 * draw_ticks, and a wait of w ms then lasts w ms. */
static long draw_ticks, colours_told;
static int drawn;

static void out_slow(void *user, const char *buf, long len)
{
    (void)user;
    vt_write(t, (const unsigned char *)buf, len);
    drawn = 0;
}

static int key_slow(void *user, int wait_ms)
{
    (void)user;
    if (!drawn) {
        now += draw_ticks;
        drawn = 1;
    }
    now += wait_ms / 20;
    return -1;
}

static void told(void *user)
{
    (void)user;
    colours_told++;
}

static long fps_of(int s)
{
    long frames, tk;
    updemo_stats(s, &frames, &tk);
    return tk > 0 ? frames * 50 / tk : 0;
}

/* The 25 frames a second hold when a frame takes the terminal 20 ms to
 * draw: the frame's time is read after the drawing, not before it, so
 * the drawing is part of the 40 ms and not added to them (plasma came at
 * 12 frames a second on a 68060 that draws one in 22 ms). */
static void a_frame_the_terminal_draws_in_20_ms_still_comes_25_times_a_second(void)
{
    updemo_io slow = { out_slow, key_slow, ticks, 0 };
    start(80, 24);
    draw_ticks = 1;
    CHECK_INT(updemo_run(&slow, 80, 24, scene_index("plasma"), 100), 0); /* plasma onwards, two seconds each */
    CHECK_INT(fps_of(scene_index("plasma")), 25);                         /* plasma */
    CHECK_INT(fps_of(scene_index("palette cycling")), 25);                         /* palette cycling */
    vt_free(t);
    start(80, 24);
    draw_ticks = 3; /* 60 ms a frame: as fast as the terminal draws, no wait on top */
    CHECK_INT(updemo_run(&slow, 80, 24, scene_index("plasma"), 100), 0);
    CHECK_INT(fps_of(scene_index("plasma")), 16);
    vt_free(t);
}

/* Palette cycling sets 32 colours a frame (OSC 4 each): the renderer
 * hears of it once a frame, not 32 times -- each time it gave its pens
 * back and obtained them again. */
static void palette_cycling_tells_the_renderer_once_a_frame(void)
{
    static vt_callbacks cb;
    long frames, tk;
    cb.colors = told;
    t = vt_new(80, 24, 100, &cb, 0);
    now = bytes = 0;
    next_key = -1;
    inject = 0;
    needle = 0;
    colours_told = 0;
    CHECK_INT(updemo_run(&io, 80, 24, scene_index("palette cycling"), 40), 0); /* then the end */
    updemo_stats(scene_index("palette cycling"), &frames, &tk);
    CHECK(frames >= 10);
    CHECK(colours_told >= frames);     /* every frame's colours reached it */
    CHECK(colours_told <= frames + 2); /* once a frame, and the OSC 104 after each scene */
    vt_free(t);
}

void suite_updemo(void)
{
    big_windows_are_drawn_within_bounds();
    a_small_window_is_refused_untouched();
    the_demo_plays_every_scene_and_puts_the_terminal_back();
    there_is_one_scene_list_with_the_effects_at_its_end();
    a_bare_run_starts_at_the_first_scene_with_its_caption();
    a_start_number_picks_a_scene_of_the_one_list();
    bench_picks_the_effect_scenes_of_the_one_list();
    the_unicode_scene_shows_emoji();
    the_styles_scene_shows_slow_and_rapid_blink();
    q_ends_the_demo_and_any_other_key_does_not();
    a_key_goes_to_the_next_scene_and_the_demo_keeps_running();
    left_and_b_go_back_a_scene();
    q_esc_and_ctrl_c_end_the_demo();
    an_arrow_key_sequence_is_not_taken_as_esc_and_a_mouse_report_is_no_key();
    the_mouse_scene_shows_a_click();
    the_resize_scene_leaves_the_width_as_it_was();
    a_window_made_too_small_ends_the_demo();
    a_frame_the_terminal_draws_in_20_ms_still_comes_25_times_a_second();
    palette_cycling_tells_the_renderer_once_a_frame();
}
