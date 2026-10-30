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
static long shrink_at, regrow_at;     /* the user drags the size gadget: 60 x 20 at shrink_at, 80 x 24 at regrow_at (0: never) */
static int next_key;
static const char *inject;            /* typed in at keys_left == 0, instead of next_key */
static char back[256];                /* the terminal's replies, read back as input */
static int back_len, back_pos;
static int layout_cols[8], n_layout;  /* DECCOLM widths the terminal was asked for */
static int saw_image, saw_link, saw_wide, saw_text, saw_slow, saw_rapid;
static const char *needle;            /* saw_text: the output had it */
static char cap[4 << 20];             /* capturing: everything the demo wrote */
static long cap_len;
static int capturing;

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
    if (capturing && cap_len + len <= (long)sizeof(cap)) {
        memcpy(cap + cap_len, buf, (size_t)len);
        cap_len += len;
    }
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
    if (shrink_at && now >= shrink_at) {
        shrink_at = 0;
        vt_resize(t, 60, 20);
    } else if (regrow_at && now >= regrow_at && !shrink_at) {
        regrow_at = 0;
        vt_resize(t, 80, 24);
    }
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
    shrink_at = regrow_at = 0;
    needle = 0;
    back_len = back_pos = 0;
    n_layout = 0;
    saw_image = saw_link = saw_wide = saw_text = saw_slow = saw_rapid = 0;
    updemo_colours(UPDEMO_AUTO);
    capturing = 0;
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

/* a window made too small waits for a bigger one; Q ends the demo from there */
static void a_window_made_too_small_waits_and_q_ends_it(void)
{
    long all;
    start(80, 24);
    updemo_run(&io, 80, 24, 0, 12);
    all = bytes;
    vt_free(t);
    start(80, 24);
    vt_resize(t, 60, 20); /* before the first scene's question */
    inject = "q";
    keys_left = 30;
    CHECK_INT(updemo_run(&io, 80, 24, 0, 12), 0);
    CHECK(bytes < all / 10);
    CHECK(!(vt_modes(t) & VT_MODE_ALT_SCREEN));
    vt_free(t);
}

/* the reflow scene asks the user to resize the window: made smaller than the demo needs and then
 * bigger again, the demo waits and goes on with the next scene (UPDEMO-REFLOW-RESIZE) */
static void a_resize_in_the_reflow_scene_does_not_end_the_demo(void)
{
    int s = scene_index("reflow");
    long frames, ticks;
    CHECK(s >= 0);
    start(80, 24);
    shrink_at = 6;
    regrow_at = 400;
    updemo_run(&io, 80, 24, s, 12);
    updemo_stats(s + 1, &frames, &ticks);
    CHECK(frames > 0); /* the scene after the reflow one ran */
    CHECK(vt_cols(t) == 80);
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

/* ---- the 16-colour version (W-D3) ---- */

static int cap_has(const char *s)
{
    long i, k = (long)strlen(s);
    for (i = 0; i + k <= cap_len; i++)
        if (!memcmp(cap + i, s, (size_t)k))
            return 1;
    return 0;
}

/* every SGR (ESC [ params m) of the capture: 1 when one has a parameter 38, 48 or 58 */
static int sgr_has_param_in(const int *bad, int nbad)
{
    long i = 0;
    while (i + 2 < cap_len) {
        if (cap[i] == 27 && cap[i + 1] == '[') {
            long j = i + 2, start = j;
            int hit = 0;
            while (j < cap_len && ((cap[j] >= '0' && cap[j] <= '9') || cap[j] == ';' || cap[j] == ':'))
                j++;
            if (j < cap_len && cap[j] == 'm') {
                long q = start;
                while (q <= j) {
                    long e = q;
                    int v = 0, k;
                    while (e < j && cap[e] != ';' && cap[e] != ':') {
                        v = v * 10 + (cap[e] - '0');
                        e++;
                    }
                    for (k = 0; k < nbad; k++)
                        if (e > q && v == bad[k])
                            hit = 1;
                    q = e + 1;
                }
            }
            if (hit)
                return 1;
            i = j;
        } else
            i++;
    }
    return 0;
}

static int cap_extended_colour(void)
{
    static const int bad[3] = { 38, 48, 58 };
    return sgr_has_param_in(bad, 3);
}

static void play_alone(int scene, int colours)
{
    start(80, 24);
    capturing = 1;
    cap_len = 0;
    updemo_colours(colours);
    updemo_only(1);
    CHECK_INT(updemo_run(&io, 80, 24, scene, 120), 0);
    updemo_only(0);
    capturing = 0;
}

/* The point of the 16-colour version: no scene -- text or effect -- sends a 256-colour or true-colour SGR
 * (38;5, 48;5, 38;2, 48;2, 58;5) or touches the palette (OSC 4, 10, 11, 104, 110, 111), and the engine knows
 * every sequence. The full version of the same scene does (so the scan can fail). */
static void ansi_mode_sends_only_the_16_colours_in_every_scene(void)
{
    const char *kinds[16];
    long counts[16];
    int s;
    for (s = 0; s < updemo_scenes(); s++) {
        play_alone(s, UPDEMO_ANSI);
        if (cap_extended_colour() || cap_has("\033]4") || cap_has("\033]10;") || cap_has("\033]11;") ||
            cap_has("\033]104") || cap_has("\033]110") || cap_has("\033]111")) {
            CHECK_STR(updemo_scene_name(s), "a scene with no 256-colour or palette sequence");
        }
        CHECK(updemo_ansi());
        CHECK_INT(vt_unhandled(t, kinds, counts, 16), 0);
        vt_free(t);
    }
    play_alone(scene_index("colours"), UPDEMO_FULL);
    CHECK(cap_has("\033[48;5;"));
    CHECK(cap_has("\033[38;2;"));
    CHECK(cap_extended_colour());
    CHECK(!updemo_ansi());
    vt_free(t);
    play_alone(scene_index("palette cycling"), UPDEMO_FULL);
    CHECK(cap_has("\033]4;"));
    vt_free(t);
}

/* the effects draw their gradients with the shade blocks U+2591-2593 (and the full block), in SGR 30-37 / 90-97 / 40-47 / 100-107 */
static void ansi_effects_draw_gradients_from_shade_blocks(void)
{
    static const char *const effects[] = { "copper bars", "plasma", "fire", "rotozoomer", "sine scroller", "colours" };
    unsigned i;
    for (i = 0; i < sizeof(effects) / sizeof(effects[0]); i++) {
        play_alone(scene_index(effects[i]), UPDEMO_ANSI);
        CHECK_STR(effects[i], effects[i]);
        CHECK(cap_has("\342\226\221") || cap_has("\342\226\222") || cap_has("\342\226\223"));
        CHECK(cap_has("\033[9") || cap_has("\033[10") || cap_has("\033[3") || cap_has("\033[4"));
        vt_free(t);
    }
    play_alone(scene_index("plasma"), UPDEMO_ANSI);
    CHECK(cap_has("\342\226\222") && cap_has("\342\226\221") && cap_has("\342\226\223"));
    CHECK(cap_has("\342\226\210") || cap_has("m "));
    vt_free(t);
}

/* palette cycling in the 16-colour version moves colour numbers: the picture is redrawn each frame and the
 * renderer is told of no palette change */
static void ansi_palette_cycling_changes_colour_indices_not_the_palette(void)
{
    static vt_callbacks cb;
    long frames, tk;
    cb.colors = told;
    cb.reply = reply;
    t = vt_new(80, 24, 100, &cb, 0);
    now = bytes = 0;
    next_key = -1;
    inject = 0;
    needle = 0;
    colours_told = 0;
    back_len = back_pos = 0;
    updemo_colours(UPDEMO_ANSI);
    capturing = 1;
    cap_len = 0;
    CHECK_INT(updemo_run(&io, 80, 24, scene_index("palette cycling"), 40), 0);
    capturing = 0;
    updemo_stats(scene_index("palette cycling"), &frames, &tk);
    CHECK(frames >= 10);
    CHECK_INT(colours_told, 0);
    CHECK(cap_has("\033[91m") || cap_has("\033[101m") || cap_has("\033[41m"));
    updemo_colours(UPDEMO_AUTO);
    vt_free(t);
}

/* each ramp maps a value range monotonically: the step never goes down as the value rises, it starts at 0 and
 * reaches the last step; the bright steps get brighter (luminance never drops); every colour is one of the 16 */
static int lum16(int c)
{
    static const unsigned long rgb[16] = {
        0x000000UL, 0xCD0000UL, 0x00CD00UL, 0xCDCD00UL, 0x0000EEUL, 0xCD00CDUL, 0x00CDCDUL, 0xE5E5E5UL,
        0x7F7F7FUL, 0xFF0000UL, 0x00FF00UL, 0xFFFF00UL, 0x5C5CFFUL, 0xFF00FFUL, 0x00FFFFUL, 0xFFFFFFUL
    };
    return (int)((299 * ((rgb[c] >> 16) & 255) + 587 * ((rgb[c] >> 8) & 255) + 114 * (rgb[c] & 255)) / 1000);
}

static void the_shade_ramps_map_a_value_range_monotonically(void)
{
    int k;
    CHECK(updemo_ramp_count() >= 8);
    for (k = 0; k < updemo_ramp_count(); k++) {
        int v, f, b, sh, prev = -1, prev_lum = -1, last = 0, rising = k != 0; /* the rainbow is a ring: hues, not brightness */
        for (v = 0; v < 256; v++) {
            int step = updemo_ramp_step(k, v, &f, &b, &sh), lum;
            CHECK(step >= prev);
            CHECK(f >= 0 && f < 16 && b >= 0 && b < 16 && sh >= 0 && sh <= 3);
            CHECK(sh || f == b);              /* a solid step is one colour */
            lum = (lum16(b) * (4 - sh) + lum16(f) * sh) / 4;
            if (rising)
                CHECK(lum >= prev_lum);
            prev = step;
            prev_lum = lum;
            last = step;
        }
        CHECK_INT(updemo_ramp_step(k, 0, &f, &b, &sh), 0);
        CHECK(last >= 12);                    /* an in-between step for every quarter between its colours */
        CHECK_INT(updemo_ramp_step(k, 256, &f, &b, &sh), -1);
        CHECK_INT(updemo_ramp_step(updemo_ramp_count(), 0, &f, &b, &sh), -1);
    }
    /* the fire runs black, red, bright red, yellow, white */
    {
        int f, b, sh;
        updemo_ramp_step(1, 0, &f, &b, &sh);
        CHECK_INT(b, 0);
        updemo_ramp_step(1, 255, &f, &b, &sh);
        CHECK_INT(b, 15);
    }
}

/* the colour count XTGETTCAP answers */
static void the_colour_count_is_read_from_the_xtgettcap_answer(void)
{
    static const char a16[] = "\033P1+r636F6C6F7273=3136\033\\";
    static const char a256[] = "\033P1+r636F6C6F7273=323536\033\\";
    static const char no[] = "\033P0+r636F6C6F7273\033\\";
    CHECK_INT(updemo_colour_count(a16, (long)sizeof(a16) - 1), 16);
    CHECK_INT(updemo_colour_count(a256, (long)sizeof(a256) - 1), 256);
    CHECK_INT(updemo_colour_count(no, (long)sizeof(no) - 1), 0);
    CHECK_INT(updemo_colour_count("", 0), 0);
    CHECK_INT(updemo_colour_count("\033P1+r636F6C6F7273=ZZ\033\\", 22), 0);
}

/* a window that says it has 16 colours (or fewer) gets the 16-colour version by itself, 256 the full one, a
 * terminal that does not answer the full one; ANSI and FULL force */
static int plays_ansi(int told, int mode)
{
    int r;
    start(80, 24);
    if (told >= 0)
        vt_set_screen_colors(t, told);
    updemo_colours(mode);
    updemo_only(1);
    CHECK_INT(updemo_run(&io, 80, 24, scene_index("plasma"), 12), 0);
    updemo_only(0);
    r = updemo_ansi();
    vt_free(t);
    return r;
}

static void the_demo_picks_the_version_from_the_colours_the_window_reports(void)
{
    CHECK_INT(plays_ansi(16, UPDEMO_AUTO), 1);
    CHECK_INT(plays_ansi(8, UPDEMO_AUTO), 1);
    CHECK_INT(plays_ansi(2, UPDEMO_AUTO), 1);
    CHECK_INT(plays_ansi(32, UPDEMO_AUTO), 0);
    CHECK_INT(plays_ansi(256, UPDEMO_AUTO), 0);
    CHECK_INT(plays_ansi(-1, UPDEMO_AUTO), 0);     /* not told: the entry's 256 */
    CHECK_INT(plays_ansi(256, UPDEMO_ANSI), 1);    /* ANSI forces */
    CHECK_INT(plays_ansi(16, UPDEMO_FULL), 0);     /* FULL forces */
    CHECK_INT(plays_ansi(-1, UPDEMO_ANSI), 1);
}

/* UPDemo ANSI, FULL, BENCH ANSI n, in any order and case */
static void the_arguments_ansi_and_full_force_a_version(void)
{
    char *a1[] = { "UPDemo", "ANSI" }, *a2[] = { "UPDemo", "full", "5" }, *a3[] = { "UPDemo", "BENCH", "ANSI", "17" };
    char *a4[] = { "UPDemo", "bench" }, *a5[] = { "UPDemo", "3" }, *a6[] = { "UPDemo" }, *a7[] = { "UPDemo", "17", "Bench", "Ansi" };
    updemo_args a;
    updemo_parse_args(2, a1, &a);
    CHECK_INT(a.colours, UPDEMO_ANSI);
    CHECK_INT(a.bench, 0);
    CHECK_INT(a.scene, 0);
    updemo_parse_args(3, a2, &a);
    CHECK_INT(a.colours, UPDEMO_FULL);
    CHECK_INT(a.scene, 5);
    updemo_parse_args(4, a3, &a);
    CHECK_INT(a.colours, UPDEMO_ANSI);
    CHECK_INT(a.bench, 1);
    CHECK_INT(a.scene, 17);
    updemo_parse_args(2, a4, &a);
    CHECK_INT(a.bench, 1);
    CHECK_INT(a.colours, UPDEMO_AUTO);
    updemo_parse_args(2, a5, &a);
    CHECK_INT(a.scene, 3);
    CHECK_INT(a.bench, 0);
    updemo_parse_args(1, a6, &a);
    CHECK_INT(a.colours, UPDEMO_AUTO);
    CHECK_INT(a.scene, 0);
    updemo_parse_args(4, a7, &a);
    CHECK_INT(a.scene, 17);
    CHECK_INT(a.bench, 1);
    CHECK_INT(a.colours, UPDEMO_ANSI);
}

/* BENCH keeps working in both versions: the same eleven scenes, frames in each */
static void bench_works_in_the_16_colour_version(void)
{
    long frames, tk;
    int s, n = 0, mode;
    for (mode = UPDEMO_ANSI; mode <= UPDEMO_FULL; mode++) {
        n = 0;
        start(80, 24);
        updemo_colours(mode);
        updemo_bench(1);
        CHECK_INT(updemo_run(&io, 80, 24, 0, 6), 0);
        updemo_bench(0);
        CHECK_INT(updemo_ansi(), mode == UPDEMO_ANSI);
        for (s = 0; s < updemo_scenes(); s++) {
            updemo_stats(s, &frames, &tk);
            CHECK_INT(frames > 0, updemo_scene_benchmarked(s));
            n += frames > 0;
        }
        CHECK_INT(n, 11);
        vt_free(t);
    }
}

/* U+2591-2593 anywhere in what the last play_alone sent */
static int cap_has_shade(void)
{
    return cap_has("\342\226\221") || cap_has("\342\226\222") || cap_has("\342\226\223");
}

/* SOLID: every scene -- text or effect -- is the 16-colour version with no shade block, and no 256-colour
 * SGR or palette sequence either */
static void solid_sends_no_shade_blocks_in_any_scene(void)
{
    int s;
    for (s = 0; s < updemo_scenes(); s++) {
        play_alone(s, UPDEMO_SOLID);
        if (cap_has_shade() || cap_extended_colour() || cap_has("\033]4") || cap_has("\033]10;") ||
            cap_has("\033]11;") || cap_has("\033]104") || cap_has("\033]110") || cap_has("\033]111")) {
            CHECK_STR(updemo_scene_name(s), "a scene with no shade block, 256-colour or palette sequence");
        }
        CHECK(updemo_ansi());
        vt_free(t);
    }
    /* the shaded version of the same effect does draw them (so the scan can fail), the solid one draws colours */
    play_alone(scene_index("plasma"), UPDEMO_ANSI);
    CHECK(cap_has_shade());
    vt_free(t);
    play_alone(scene_index("plasma"), UPDEMO_SOLID);
    CHECK(!cap_has_shade());
    CHECK(cap_has("\033[4") || cap_has("\033[10"));
    vt_free(t);
}

/* UPDemo SOLID is the 16-colour version even when the window says it has 256 colours */
static void solid_implies_ansi_on_a_256_colour_window(void)
{
    CHECK_INT(plays_ansi(256, UPDEMO_SOLID), 1);
    CHECK_INT(plays_ansi(-1, UPDEMO_SOLID), 1);
    CHECK_INT(plays_ansi(16, UPDEMO_SOLID), 1);
}

/* SOLID, ANSI, FULL, BENCH and the scene number in any order; ReadArgs' values go to the same settings */
static void the_argument_solid_parses_in_any_order_and_readargs_values_make_the_same_settings(void)
{
    char *a1[] = { "UPDemo", "SOLID" }, *a2[] = { "UPDemo", "3", "solid", "bench" }, *a3[] = { "UPDemo", "BENCH", "Solid", "17" };
    char *a4[] = { "UPDemo", "solid", "ANSI" }, *a5[] = { "UPDemo", "FULL", "SOLID" }, *a6[] = { "UPDemo", "ANSI", "FULL" };
    updemo_args a;
    updemo_parse_args(2, a1, &a);
    CHECK_INT(a.colours, UPDEMO_SOLID);
    CHECK_INT(a.bench, 0);
    updemo_parse_args(4, a2, &a);
    CHECK_INT(a.colours, UPDEMO_SOLID);
    CHECK_INT(a.scene, 3);
    CHECK_INT(a.bench, 1);
    updemo_parse_args(4, a3, &a);
    CHECK_INT(a.colours, UPDEMO_SOLID);
    CHECK_INT(a.scene, 17);
    CHECK_INT(a.bench, 1);
    updemo_parse_args(3, a4, &a);
    CHECK_INT(a.colours, UPDEMO_SOLID);
    updemo_parse_args(3, a5, &a);
    CHECK_INT(a.colours, UPDEMO_SOLID);
    updemo_parse_args(3, a6, &a);
    CHECK_INT(a.colours, UPDEMO_FULL); /* the later of the two */
    /* the Amiga's template SCENE/N,ANSI/S,FULL/S,SOLID/S,BENCH/S */
    updemo_args_make(&a, 17, 1, 0, 0, 1);
    CHECK_INT(a.scene, 17);
    CHECK_INT(a.bench, 1);
    CHECK_INT(a.colours, UPDEMO_SOLID);
    updemo_args_make(&a, 0, 0, 1, 0, 0);
    CHECK_INT(a.colours, UPDEMO_ANSI);
    CHECK_INT(a.scene, 0);
    updemo_args_make(&a, 0, 0, 0, 1, 0);
    CHECK_INT(a.colours, UPDEMO_FULL);
    updemo_args_make(&a, 0, 0, 0, 0, 0);
    CHECK_INT(a.colours, UPDEMO_AUTO);
}

/* BENCH SOLID runs the same eleven scenes and frames each, without shade blocks */
static void bench_works_in_the_solid_version(void)
{
    long frames, tk;
    int s, n = 0;
    start(80, 24);
    capturing = 1;
    cap_len = 0;
    updemo_colours(UPDEMO_SOLID);
    updemo_bench(1);
    CHECK_INT(updemo_run(&io, 80, 24, 0, 6), 0);
    updemo_bench(0);
    capturing = 0;
    CHECK(updemo_ansi());
    CHECK(!cap_has_shade());
    for (s = 0; s < updemo_scenes(); s++) {
        updemo_stats(s, &frames, &tk);
        CHECK_INT(frames > 0, updemo_scene_benchmarked(s));
        n += frames > 0;
    }
    CHECK_INT(n, 11);
    vt_free(t);
}

/* the 16-colour version's status line says what it plays */
static void the_16_colour_captions_say_so(void)
{
    start(80, 24);
    updemo_colours(UPDEMO_ANSI);
    needle = "A plasma in the 16 ANSI colours";
    updemo_only(1);
    CHECK_INT(updemo_run(&io, 80, 24, scene_index("plasma"), 6), 0);
    updemo_only(0);
    CHECK(saw_text);
    needle = 0;
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
    a_window_made_too_small_waits_and_q_ends_it();
    a_resize_in_the_reflow_scene_does_not_end_the_demo();
    a_frame_the_terminal_draws_in_20_ms_still_comes_25_times_a_second();
    palette_cycling_tells_the_renderer_once_a_frame();
    ansi_mode_sends_only_the_16_colours_in_every_scene();
    ansi_effects_draw_gradients_from_shade_blocks();
    ansi_palette_cycling_changes_colour_indices_not_the_palette();
    the_shade_ramps_map_a_value_range_monotonically();
    the_colour_count_is_read_from_the_xtgettcap_answer();
    the_demo_picks_the_version_from_the_colours_the_window_reports();
    the_arguments_ansi_and_full_force_a_version();
    solid_sends_no_shade_blocks_in_any_scene();
    solid_implies_ansi_on_a_256_colour_window();
    the_argument_solid_parses_in_any_order_and_readargs_values_make_the_same_settings();
    bench_works_in_the_solid_version();
    bench_works_in_the_16_colour_version();
    the_16_colour_captions_say_so();
}
