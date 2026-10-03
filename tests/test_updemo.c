/* demo/updemo through the engine: every scene's output is sequences the
 * terminal knows, and the screen comes back as it was. */
#include <string.h>
#include "harness.h"
#include "../engine/vtengine.h"
#include "../demo/updemo.h"

static vt_term *t;
static long now, keys_left, bytes;
static int next_key;

static void out(void *user, const char *buf, long len)
{
    (void)user;
    bytes += len;
    vt_write(t, (const unsigned char *)buf, len);
}

/* a virtual clock: the wait passes at once */
static int key(void *user, int wait_ms)
{
    (void)user;
    now += wait_ms / 20 + 1;
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
    t = vt_new(cols, rows, 100, &cb, 0);
    vt_write(t, (const unsigned char *)"before the demo\r\n$ ", 19);
    now = bytes = 0;
    next_key = -1;
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

void suite_updemo(void)
{
    CHECK_INT(updemo_scenes(), 11);
    every_scene_runs_and_the_screen_comes_back();
    big_windows_are_drawn_within_bounds();
    a_small_window_is_refused_untouched();
    q_quits_and_the_palette_is_the_terminals_again();
}
