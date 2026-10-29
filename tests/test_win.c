#include <string.h>
#include "harness.h"
#include "../amarchy/win_core.h"

static const win_screen scr = { 640, 256, 11, 0 };

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

/* P3-A1..A3: work area, bottom reserve, strut, snap. 800x600, title 14. */
static const win_screen s0 = { 800, 600, 14, 0 };
static const win_screen s16 = { 800, 600, 14, 16 };

static win_box free_box(int w, int h)
{
    win_box b;
    memset(&b, 0, sizeof b);
    b.width = w; b.height = h; b.minw = 1; b.minh = 1; b.screen = 1;
    return b;
}

static void snap_is(win_arg a, int gap, const win_screen *s, int l, int t, int w, int h)
{
    win_box b = free_box(300, 200);
    win_snap(&b, a, gap, s);
    CHECK_INT(b.left, l);
    CHECK_INT(b.top, t);
    CHECK_INT(b.width, w);
    CHECK_INT(b.height, h);
}

static void work_area_and_fullscreen(void)
{
    int x, y, w, h;
    win_box b = free_box(10, 10);
    win_work_area(&s0, 4, &x, &y, &w, &h);
    CHECK_INT(x, 4); CHECK_INT(y, 18); CHECK_INT(w, 792); CHECK_INT(h, 578);
    win_work_area(&s16, 4, &x, &y, &w, &h);
    CHECK_INT(h, 562);
    CHECK_INT(y, 18);
    win_fullscreen(&b, &s0);
    CHECK_INT(b.height, 586);
    b = free_box(10, 10);
    win_fullscreen(&b, &s16);
    CHECK_INT(b.top, 14);
    CHECK_INT(b.height, 570);
}

/* P3-B1: win_move stops at the bar (bottom 16 on the 800x600 screen), as fullscreen does */
static void move_stops_at_bar(void)
{
    win_box b = box(100, 100, 200, 100);
    win_screen s16 = s0;
    s16.bottom = 16;
    win_move(&b, WA_DOWN, 5000, &s16);
    CHECK_INT(b.top, 484);                             /* 600 - 16 - 100 */
    CHECK_INT(win_move(&b, WA_DOWN, 5, &s16), 0);
    win_move(&b, WA_DOWN, 5000, &s0);
    CHECK_INT(b.top, 500);                             /* no bar: the bottom pixel row */
}

static void strut_and_ours(void)
{
    win_box bar = free_box(800, 16), part = free_box(400, 16), mid = free_box(800, 16);
    bar.top = 584; part.top = 584; mid.top = 300;
    CHECK_INT(win_strut(&bar, &s0), 16);
    CHECK_INT(win_strut(&part, &s0), 0);               /* not full width */
    CHECK_INT(win_strut(&mid, &s0), 0);                /* not at the bottom */
    bar.top = 0; bar.height = 600;
    CHECK_INT(win_strut(&bar, &s0), 0);                /* fills the screen from the top: not a bar */
    CHECK(win_is_amarchy("Amarchy"));
    CHECK(win_is_amarchy("Amarchy bar"));
    CHECK(!win_is_amarchy("Amarchy bar2"));
    CHECK(!win_is_amarchy("Shell"));
    CHECK(!win_is_amarchy(0));
}

static void snap_parse(void)
{
    win_cmd c;
    static const char *const ok[] = { "snap left", "snap right", "snap up", "snap down", "snap tl", "snap tr", "snap bl",
                                      "snap br", "snap center", 0 };
    int i;
    for (i = 0; ok[i]; i++)
        CHECK(win_parse(ok[i], &c) && c.verb == WV_SNAP);
    CHECK(win_parse("snap tl", &c) && c.arg == WA_TL);
    CHECK(win_parse("snap center", &c) && c.arg == WA_CENTER);
    CHECK(!win_parse("snap", &c));
    CHECK(!win_parse("snap middle", &c));
    CHECK(!win_parse("snap left 5", &c));
}

static void snap_boxes(void)
{
    win_box b;
    snap_is(WA_LEFT, 0, &s0, 0, 14, 400, 586);
    snap_is(WA_RIGHT, 0, &s0, 400, 14, 400, 586);
    snap_is(WA_UP, 0, &s0, 0, 14, 800, 293);
    snap_is(WA_DOWN, 0, &s0, 0, 307, 800, 293);
    snap_is(WA_TL, 0, &s0, 0, 14, 400, 293);
    snap_is(WA_TR, 0, &s0, 400, 14, 400, 293);
    snap_is(WA_BL, 0, &s0, 0, 307, 400, 293);
    snap_is(WA_BR, 0, &s0, 400, 307, 400, 293);
    snap_is(WA_CENTER, 0, &s0, 250, 207, 300, 200);
    snap_is(WA_LEFT, 4, &s0, 4, 18, 394, 578);
    snap_is(WA_RIGHT, 4, &s0, 402, 18, 394, 578);
    snap_is(WA_UP, 4, &s0, 4, 18, 792, 287);
    snap_is(WA_DOWN, 4, &s0, 4, 309, 792, 287);
    snap_is(WA_BR, 4, &s0, 402, 309, 394, 287);
    snap_is(WA_UP, 0, &s16, 0, 14, 800, 285);          /* bottom 16: the work area ends at 584 */
    snap_is(WA_DOWN, 0, &s16, 0, 299, 800, 285);
    snap_is(WA_UP, 4, &s16, 4, 18, 792, 279);
    snap_is(WA_DOWN, 4, &s16, 4, 301, 792, 279);
    snap_is(WA_CENTER, 0, &s16, 250, 207 - 8, 300, 200);
    {
        static const win_screen odd = { 801, 601, 14, 0 };
        snap_is(WA_RIGHT, 0, &odd, 400, 14, 401, 587);  /* the remainder pixel goes right and down */
        snap_is(WA_DOWN, 0, &odd, 0, 307, 801, 294);
    }
    b = free_box(300, 200);
    b.maxw = 300;
    win_snap(&b, WA_LEFT, 0, &s0);                     /* Max wins over the half */
    CHECK_INT(b.width, 300);
    CHECK_INT(b.height, 586);
    b = free_box(300, 200);
    b.minw = 500;
    win_snap(&b, WA_TL, 0, &s0);                       /* Min wins over the quarter */
    CHECK_INT(b.width, 500);
    b = free_box(300, 200);
    CHECK_INT(win_snap(&b, WA_LEFT, 0, &s0), 1);
    CHECK_INT(win_snap(&b, WA_LEFT, 0, &s0), 0);       /* already there: no change */
    CHECK_INT(win_snap(&b, WA_NEXT, 0, &s0), 0);       /* not a zone */
}

/* P3-E4: every knob of snap moves the host output: the nine zones give nine distinct boxes, the gap changes every zone's
 * box, and the bar reserve (bottom) changes every zone's box. */
static void snap_knobs_each_move_the_box(void)
{
    static const win_arg zone[] = { WA_LEFT, WA_RIGHT, WA_UP, WA_DOWN, WA_TL, WA_TR, WA_BL, WA_BR, WA_CENTER };
    static const win_screen bar = { 800, 600, 14, 16 };
    win_box b[9];
    int i, j;
    for (i = 0; i < 9; i++) {
        win_box g4 = free_box(300, 200), bb = free_box(300, 200);
        b[i] = free_box(300, 200);
        win_snap(&b[i], zone[i], 0, &s0);
        win_snap(&g4, zone[i], 4, &s0);
        win_snap(&bb, zone[i], 0, &bar);
        if (zone[i] != WA_CENTER)                           /* centre keeps the size: no gap */
            CHECK(memcmp(&b[i], &g4, sizeof b[i]) != 0);   /* gap 0 vs 4 */
        CHECK(memcmp(&b[i], &bb, sizeof b[i]) != 0);       /* bottom 0 vs 16 */
    }
    for (i = 0; i < 9; i++)
        for (j = i + 1; j < 9; j++)
            CHECK(memcmp(&b[i], &b[j], sizeof b[i]) != 0);  /* zone */
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
    work_area_and_fullscreen();
    strut_and_ours();
    move_stops_at_bar();
    snap_parse();
    snap_boxes();
    snap_knobs_each_move_the_box();
}
