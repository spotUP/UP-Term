/* demo/updemo through the engine: every scene's output is sequences the
 * terminal knows, and the screen comes back as it was. The tour also
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
static int saw_image, saw_link, saw_wide, saw_text;
static const char *needle;            /* saw_text: the output had it */

static void scan(void)
{
    int y, x, n;
    if (vt_images(t))
        saw_image = 1;
    for (y = 0; y < vt_rows(t) && (!saw_link || !saw_wide); y++) {
        const vt_cell *c = vt_row(t, y, &n);
        for (x = 0; x < n; x++) {
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
    saw_image = saw_link = saw_wide = saw_text = 0;
}

static int tour_scene(const char *name)
{
    int s;
    for (s = 0; s < updemo_tour_scenes(); s++)
        if (!strcmp(updemo_tour_scene_name(s), name))
            return s;
    return -1;
}

static void every_scene_runs_and_the_screen_comes_back(void)
{
    const char *kinds[16];
    long counts[16];
    int x, y;
    start(80, 24);
    CHECK_INT(updemo_run(&io, 80, 24, 0, 12), 0);
    CHECK(bytes > 20000);                       /* the scenes drew */
    CHECK_INT(vt_unhandled(t, kinds, counts, 16), 0);
    if (vt_unhandled(t, kinds, counts, 16))
        CHECK_STR(kinds[0], "");                /* names the first sequence the engine dropped */
    CHECK(!(vt_modes(t) & VT_MODE_ALT_SCREEN));
    CHECK(vt_modes(t) & VT_MODE_CURSOR_VISIBLE);
    CHECK_STR(row_text(0), "before the demo");
    CHECK_STR(row_text(1), "$");
    CHECK_STR(row_text(23), "");                /* the status line was on the other screen */
    vt_cursor(t, &x, &y);
    CHECK_INT(x, 2);
    CHECK_INT(y, 1);
    vt_free(t);
}

/* the biggest window the buffers take, and a bigger one (clamped) */
static void big_windows_are_drawn_within_bounds(void)
{
    start(132, 60);
    CHECK_INT(updemo_run(&io, 132, 60, 0, 6), 0);
    CHECK_STR(row_text(0), "before the demo");
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
    CHECK_INT(updemo_tour(&io, 60, 20, 0, 6), 1);
    CHECK_INT(bytes, 0);
    vt_free(t);
}

static void q_quits_and_the_palette_is_the_terminals_again(void)
{
    long all;
    start(80, 24);
    updemo_run(&io, 80, 24, 0, 12);
    all = bytes;
    vt_free(t);
    start(80, 24);
    next_key = 'q';
    keys_left = 3; /* the fourth frame's key */
    CHECK_INT(updemo_run(&io, 80, 24, 9, 0), 0); /* from the palette scene */
    CHECK(bytes < all / 4);
    CHECK(!(vt_modes(t) & VT_MODE_ALT_SCREEN));
    CHECK_INT(vt_palette_rgb(t, 20), vt_palette_rgb(0, 20)); /* OSC 104 took the scene's colours back */
    vt_free(t);
}

/* ---- the tour (Help > Demo tour) ---- */

/* Every scene of the tour plays, with what it is there to show reaching
 * the terminal: a sixel image, a hyperlink, wide characters. What the
 * scenes changed is put back: the mouse, the palette, the screen. */
static void the_tour_plays_every_scene_and_puts_the_terminal_back(void)
{
    const char *kinds[16];
    long counts[16], frames, tk;
    int s, all_ran = 1;
    start(80, 24);
    CHECK_INT(updemo_tour(&io, 80, 24, 0, 90), 0); /* long enough for each scene to reach its point */
    CHECK_INT(vt_unhandled(t, kinds, counts, 16), 0);
    if (vt_unhandled(t, kinds, counts, 16))
        CHECK_STR(kinds[0], "");
    for (s = 0; s < updemo_tour_scenes(); s++) {
        updemo_stats(s, &frames, &tk);
        if (frames < 1) {
            all_ran = 0;
            CHECK_STR(updemo_tour_scene_name(s), "a scene that drew no frame");
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

static void any_key_ends_the_tour(void)
{
    long all;
    start(80, 24);
    updemo_tour(&io, 80, 24, 0, 12);
    all = bytes;
    vt_free(t);
    start(80, 24);
    next_key = 'x';
    keys_left = 3;
    CHECK_INT(updemo_tour(&io, 80, 24, 0, 0), 0);
    CHECK(bytes < all / 4);
    CHECK(!(vt_modes(t) & VT_MODE_ALT_SCREEN));
    CHECK_STR(row_text(0), "before the demo");
    vt_free(t);
    start(80, 24);
    inject = "\033[A"; /* a cursor key is a key too */
    keys_left = 3;
    CHECK_INT(updemo_tour(&io, 80, 24, 0, 0), 0);
    CHECK(bytes < all / 4);
    vt_free(t);
}

/* a click arrives as an SGR mouse report: the mouse scene shows it, and
 * the tour goes on */
static void the_mouse_scene_shows_a_click(void)
{
    int s = tour_scene("mouse");
    CHECK(s >= 0);
    start(80, 24);
    inject = "\033[<0;10;12M";
    keys_left = 2;
    needle = "button 1 pressed at column 10, row 12";
    CHECK_INT(updemo_tour(&io, 80, 24, s, 0), 0);
    CHECK(saw_text);
    CHECK(!(vt_modes(t) & VT_MODE_MOUSE_SGR));
    vt_free(t);
}

/* DECCOLM from 80 columns, and back to 80 however the scene ends */
static void the_resize_scene_leaves_the_width_as_it_was(void)
{
    int s = tour_scene("resize");
    CHECK(s >= 0);
    start(80, 24);
    CHECK_INT(updemo_tour(&io, 80, 24, s, 12), 0); /* ends before the scene's own 80 */
    CHECK(n_layout >= 2);
    CHECK_INT(layout_cols[0], 132);
    CHECK_INT(layout_cols[n_layout - 1], 80);
    vt_free(t);
    start(100, 30); /* not 80 wide: the scene only tells */
    CHECK_INT(updemo_tour(&io, 100, 30, s, 12), 0);
    CHECK_INT(n_layout, 0);
    vt_free(t);
}

/* the tour asks the size before each scene: a window made too small ends it */
static void a_window_made_too_small_ends_the_tour(void)
{
    long all;
    start(80, 24);
    updemo_tour(&io, 80, 24, 0, 12);
    all = bytes;
    vt_free(t);
    start(80, 24);
    vt_resize(t, 60, 20); /* before the first scene's question */
    CHECK_INT(updemo_tour(&io, 80, 24, 0, 12), 0);
    CHECK(bytes < all / 10);
    CHECK(!(vt_modes(t) & VT_MODE_ALT_SCREEN));
    vt_free(t);
}

void suite_updemo(void)
{
    CHECK_INT(updemo_scenes(), 11);
    CHECK_INT(updemo_tour_scenes(), 20);
    every_scene_runs_and_the_screen_comes_back();
    big_windows_are_drawn_within_bounds();
    a_small_window_is_refused_untouched();
    q_quits_and_the_palette_is_the_terminals_again();
    the_tour_plays_every_scene_and_puts_the_terminal_back();
    any_key_ends_the_tour();
    the_mouse_scene_shows_a_click();
    the_resize_scene_leaves_the_width_as_it_was();
    a_window_made_too_small_ends_the_tour();
}
