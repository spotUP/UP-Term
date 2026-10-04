/* The window's input policy (render/vtinput): wheel, buttons, motion,
 * clicks, console keys, paste -- what render/vtwin.c does with each event. */
#include "harness.h"
#include "../render/vtinput.h"

/* an 80 x 24 text area of 8 x 8 cells, 4 pixels in, 11 down (a window's
 * border and title bar) */
static vti_geom geom(void)
{
    vti_geom g;
    g.ox = 4;
    g.oy = 11;
    g.cw = 8;
    g.ch = 8;
    g.cols = 80;
    g.rows = 24;
    return g;
}

static const char *wheel(vt_term *t, int view, int up, int px, int py, int *lines)
{
    static char buf[40];
    vti_geom g = geom();
    int n = vti_wheel(&g, t, view, up, 0, px, py, (vt_u8 *)buf, lines);
    buf[n] = 0;
    return buf;
}

/* gap #1: the report named window pixels as cells (tmux picked the wrong pane,
 * and past pixel 222 the legacy form fell back to UP-Term's scrollback) */
static void wheel_reports_name_the_cell_under_the_pointer(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    int lines, x = -1, y = -1;
    vti_geom g = geom();
    CHECK_INT(vti_cell_at(&g, 4 + 8 * 30 + 3, 11 + 8 * 10 + 7, &x, &y), 1);
    CHECK_INT(x, 30);
    CHECK_INT(y, 10);
    CHECK_INT(vti_cell_at(&g, 3, 20, &x, &y), 0);
    CHECK_STR(wheel(t, 0, 1, 300, 200, &lines), "");        /* no mouse mode: the scrollback */
    CHECK_INT(lines, VTI_WHEEL_LINES);
    h_put(t, "\033[?1000h\033[?1006h");
    /* pixel (300, 200) is cell (37, 23): SGR says 38;24 */
    CHECK_STR(wheel(t, 0, 1, 300, 200, &lines), "\033[<64;38;24M");
    CHECK_STR(wheel(t, 0, 0, 4, 11, &lines), "\033[<65;1;1M");
    h_put(t, "\033[?1006l");
    /* the legacy form: pixel 600 is cell 74, within one byte (the pixel was not) */
    CHECK_STR(wheel(t, 0, 1, 600, 12, &lines), "\033[M`k!");
    CHECK_STR(wheel(t, 0, 1, 2, 2, &lines), "\033[M`!!");     /* over the border: the nearest cell */
    CHECK_STR(wheel(t, 5, 1, 300, 200, &lines), "");         /* a scrolled-back view keeps it */
    CHECK_INT(lines, VTI_WHEEL_LINES);
    CHECK_STR(wheel(t, 5, 0, 300, 200, &lines), "");
    CHECK_INT(lines, -VTI_WHEEL_LINES);
    vt_free(t);
}

/* gap #8: less, man, git log on the alternate screen scroll with the wheel
 * once the program sets ?1007 (xterm's alternateScroll; off until asked) */
static void wheel_on_the_alternate_screen_sends_cursor_keys(void)
{
    vt_term *t = h_new(80, 24, VT_XTERM);
    int lines;
    h_put(t, "\033[?1049h");
    CHECK_STR(wheel(t, 0, 1, 300, 200, &lines), "");             /* xterm's default: off */
    h_reply_clear();
    h_put(t, "\033[?1007$p");
    CHECK_STR(h_reply, "\033[?1007;2$y");
    h_put(t, "\033[?1007h");
    h_reply_clear();
    h_put(t, "\033[?1007$p");
    CHECK_STR(h_reply, "\033[?1007;1$y");
    CHECK_STR(wheel(t, 0, 1, 300, 200, &lines), "\033[A\033[A\033[A");
    CHECK_STR(wheel(t, 0, 0, 300, 200, &lines), "\033[B\033[B\033[B");
    h_put(t, "\033[?1h");                                          /* DECCKM: as the keys say it */
    CHECK_STR(wheel(t, 0, 0, 300, 200, &lines), "\033OB\033OB\033OB");
    h_put(t, "\033[?1000h\033[?1006h");                            /* a mouse mode wins */
    CHECK_STR(wheel(t, 0, 1, 4, 11, &lines), "\033[<64;1;1M");
    h_put(t, "\033[?1000l\033[?1049l");
    CHECK_STR(wheel(t, 0, 1, 300, 200, &lines), "");             /* the main screen: scrollback */
    CHECK_INT(lines, VTI_WHEEL_LINES);
    h_put(t, "\033[?1049h\033c");                                  /* RIS: off again */
    h_put(t, "\033[?1049h");
    CHECK_STR(wheel(t, 0, 1, 300, 200, &lines), "");
    vt_free(t);
}

void suite_input(void)
{
    wheel_reports_name_the_cell_under_the_pointer();
    wheel_on_the_alternate_screen_sends_cursor_keys();
}
