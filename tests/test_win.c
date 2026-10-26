#include <string.h>
#include "harness.h"
#include "../amarchy/win_core.h"

static const win_screen scr = { 640, 256, 11 };

static win_box box(int l, int t, int w, int h)
{
    win_box b;
    memset(&b, 0, sizeof b);
    b.left = l; b.top = t; b.width = w; b.height = h;
    b.minw = 100; b.minh = 50; b.maxw = 500; b.maxh = 200; b.screen = 1;
    return b;
}

static void parse_verbs(void)
{
    win_cmd c;
    CHECK(win_parse("move left", &c) && c.verb == WV_MOVE && c.arg == WA_LEFT && c.pixels == WIN_STEP);
    CHECK(win_parse("size down 7", &c) && c.verb == WV_SIZE && c.arg == WA_DOWN && c.pixels == 7);
    CHECK(win_parse("focus prev", &c) && c.verb == WV_FOCUS && c.arg == WA_PREV);
    CHECK(win_parse("cycle next", &c) && c.verb == WV_CYCLE && c.arg == WA_NEXT);
    CHECK(win_parse("screen workbench", &c) && c.verb == WV_SCREEN && c.arg == WA_WORKBENCH);
    CHECK(win_parse("title hide", &c) && c.verb == WV_TITLE && c.arg == WA_HIDE);
    CHECK(win_parse("fullscreen", &c) && c.verb == WV_FULLSCREEN);
    CHECK(!win_parse("move left 0", &c) && c.verb == WV_NONE);
    CHECK(!win_parse("move left 12345", &c));
    CHECK(!win_parse("zip now", &c) && !win_parse("", &c) && !win_parse("move", &c));
}

static void move_steps_and_clamps(void)
{
    win_box b = box(100, 100, 200, 100);
    CHECK_INT(win_move(&b, WA_LEFT, WIN_STEP, &scr), 1);
    CHECK_INT(b.left, 68);
    CHECK_INT(win_move(&b, WA_DOWN, 10, &scr), 1);
    CHECK_INT(b.top, 110);
    win_move(&b, WA_LEFT, 500, &scr);                 /* stops at the left edge */
    CHECK_INT(b.left, 0);
    CHECK_INT(win_move(&b, WA_LEFT, 5, &scr), 0);
    win_move(&b, WA_RIGHT, 5000, &scr);               /* right edge: screen width minus window width */
    CHECK_INT(b.left, 440);
    win_move(&b, WA_UP, 5000, &scr);
    CHECK_INT(b.top, 0);
    win_move(&b, WA_DOWN, 5000, &scr);
    CHECK_INT(b.top, 156);
    CHECK_INT(b.width, 200);                           /* a move never resizes */
}

static void size_stops_at_limits(void)
{
    win_box b = box(100, 10, 200, 100);
    win_size(&b, WA_RIGHT, WIN_STEP, &scr);
    CHECK_INT(b.width, 232);
    win_size(&b, WA_RIGHT, 1000, &scr);                /* max width 500 */
    CHECK_INT(b.width, 500);
    win_size(&b, WA_LEFT, 1000, &scr);                 /* min width 100 */
    CHECK_INT(b.width, 100);
    win_size(&b, WA_DOWN, 1000, &scr);                 /* max height 200 */
    CHECK_INT(b.height, 200);
    win_size(&b, WA_UP, 1000, &scr);                   /* min height 50 */
    CHECK_INT(b.height, 50);
    CHECK_INT(win_size(&b, WA_UP, 5, &scr), 0);
    b = box(400, 200, 100, 50);                        /* the screen limits what the maxima allow */
    win_size(&b, WA_RIGHT, 400, &scr);
    CHECK_INT(b.width, 240);
    win_size(&b, WA_DOWN, 400, &scr);
    CHECK_INT(b.height, 56);
    b = box(0, 0, 200, 100);
    b.maxw = 0;                                        /* no Max: the screen */
    win_size(&b, WA_RIGHT, 2000, &scr);
    CHECK_INT(b.width, 640);
}

static void fullscreen_terminal_window(void)
{
    CHECK_INT(win_is_fullscreen_terminal(1, 0, 0, 640, 256, &scr), 1);
    CHECK_INT(win_is_fullscreen_terminal(0, 0, 0, 640, 256, &scr), 0);   /* not backdrop and borderless */
    CHECK_INT(win_is_fullscreen_terminal(1, 0, 0, 600, 256, &scr), 0);   /* not the screen's size */
}

static void full_width_and_screen(void)
{
    win_box b = box(50, 60, 200, 100);
    b.maxw = 0; b.maxh = 0;
    CHECK_INT(win_fullwidth(&b, &scr), 1);
    CHECK(b.left == 0 && b.width == 640 && b.top == 60 && b.height == 100);
    CHECK_INT(win_fullwidth(&b, &scr), 0);
    CHECK_INT(win_fullscreen(&b, &scr), 1);
    CHECK(b.left == 0 && b.top == 11 && b.width == 640 && b.height == 245);   /* below the 11 pixel title bar */
    CHECK_INT(win_fullscreen(&b, &scr), 0);
    b = box(50, 60, 200, 100);                         /* the window's own maximum still holds */
    win_fullscreen(&b, &scr);
    CHECK(b.width == 500 && b.height == 200 && b.top == 11);
}

static void save_restore_ring(void)
{
    win_ring r;
    win_box b = box(1, 2, 3, 4), out;
    long i;
    win_ring_clear(&r);
    CHECK_INT(win_restore(&r, 77, &out), 0);
    win_save(&r, 77, &b);
    CHECK_INT(win_restore(&r, 77, &out), 1);
    CHECK(out.left == 1 && out.top == 2 && out.width == 3 && out.height == 4);
    b.left = 9;
    win_save(&r, 77, &b);                              /* same id: replaced, not added */
    CHECK_INT(r.n, 1);
    win_restore(&r, 77, &out);
    CHECK_INT(out.left, 9);
    for (i = 1; i <= 7; i++)                           /* 77 plus ids 1..7: eight windows */
        win_save(&r, i, &b);
    CHECK_INT(r.n, 8);
    CHECK_INT(win_restore(&r, 77, &out), 1);
    win_save(&r, 8, &b);                               /* the 9th: the oldest (77) is dropped */
    CHECK_INT(r.n, 8);
    CHECK_INT(win_restore(&r, 77, &out), 0);
    CHECK_INT(win_restore(&r, 1, &out), 1);
    CHECK_INT(win_restore(&r, 8, &out), 1);
    win_save(&r, 1, &b);                               /* saving again makes it the newest: 2 goes next */
    win_save(&r, 9, &b);
    CHECK_INT(win_restore(&r, 2, &out), 0);
    CHECK_INT(win_restore(&r, 1, &out), 1);
}

static void direction_search(void)
{
    win_box w[6];
    w[0] = box(200, 100, 100, 50);                     /* the target */
    w[1] = box(0, 100, 100, 50);                       /* left, straight */
    w[2] = box(0, 200, 100, 50);                       /* left, lower */
    w[3] = box(400, 100, 100, 50);                     /* right */
    w[4] = box(200, 0, 100, 50);                       /* above */
    w[5] = box(210, 100, 100, 50);                     /* beside it on another screen */
    w[5].screen = 2;
    CHECK_INT(win_pick(w, 6, 0, WA_LEFT), 1);          /* nearest in line beats the lower one */
    CHECK_INT(win_pick(w, 6, 0, WA_RIGHT), 3);
    CHECK_INT(win_pick(w, 6, 0, WA_UP), 4);
    CHECK_INT(win_pick(w, 6, 0, WA_DOWN), 2);          /* the only one below, even if off to the left */
    CHECK_INT(win_pick(w, 6, 3, WA_RIGHT), -1);        /* nothing there */
    CHECK_INT(win_pick(w, 6, 5, WA_LEFT), -1);         /* alone on its screen: never the target, never others */
    CHECK_INT(win_pick(w, 6, 0, WA_NEXT), 1);
    CHECK_INT(win_pick(w, 6, 0, WA_PREV), 4);          /* wraps past the other screen's box */
    CHECK_INT(win_pick(w, 6, 4, WA_NEXT), 0);          /* skips w[5] on the way round */
    CHECK_INT(win_pick(w, 6, 9, WA_LEFT), -1);
}

static void ticket_needs_unchanged_box(void)
{
    win_box taken = box(40, 40, 320, 120), now = taken;
    CHECK_INT(win_ticket_valid(1, &taken, &now), 1);   /* nothing moved: the menu's target stands */
    now.left = 72;
    CHECK_INT(win_ticket_valid(1, &taken, &now), 0);   /* moved since the menu opened */
    now = taken;
    now.top = 41;
    CHECK_INT(win_ticket_valid(1, &taken, &now), 0);
    now = taken;
    now.width = 321;
    CHECK_INT(win_ticket_valid(1, &taken, &now), 0);
    now = taken;
    now.height = 119;
    CHECK_INT(win_ticket_valid(1, &taken, &now), 0);
    now = taken;
    CHECK_INT(win_ticket_valid(0, &taken, &now), 0);   /* window gone or ours: refused whatever the box */
}

void suite_win(void)
{
    parse_verbs();
    move_steps_and_clamps();
    size_stops_at_limits();
    full_width_and_screen();
    fullscreen_terminal_window();
    save_restore_ring();
    direction_search();
    ticket_needs_unchanged_box();
}
