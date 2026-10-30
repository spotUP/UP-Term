/* The Amarchy bar core (amarchy/bar_core.c; roadmap phase 2 R2.4) and the geometry of the agents panel that hangs
 * from it (amarchy/menu_geom.c anchored layout). The bar's drawing and IDCMP loop (amarchy/bar.c) are the rig
 * script's (tools/rig/amarchy_bar_rig.py). */
#include <string.h>
#include "../amarchy/bar_core.h"
#include "../amarchy/menu_geom.h"
#include "harness.h"

static void test_layout_groups(void)
{
    bar b;
    int a, c, r1, r2;
    bar_init(&b, 640, 4, 2);
    a = bar_add(&b, 100, BAR_LEFT, "ws", 16);
    c = bar_add(&b, 101, BAR_LEFT, "clock", 40);
    r1 = bar_add(&b, 102, BAR_RIGHT, "ram", 24);
    r2 = bar_add(&b, BAR_SEG_AGENTS, BAR_RIGHT, "Agents", 48);
    bar_layout(&b, 640);
    CHECK_INT(b.seg[a].x, 0);
    CHECK_INT(b.seg[a].w, 24);
    CHECK_INT(b.seg[c].x, 26);                          /* after the first plus the gap */
    CHECK_INT(b.seg[r2].x + b.seg[r2].w, 640);           /* the last right segment ends at the edge */
    CHECK_INT(b.seg[r2].w, 56);
    CHECK_INT(b.seg[r1].x + b.seg[r1].w, b.seg[r2].x - 2);
    /* a screen of another width moves only the right group */
    bar_layout(&b, 320);
    CHECK_INT(b.seg[a].x, 0);
    CHECK_INT(b.seg[r2].x + b.seg[r2].w, 320);
}

/* Shown segments in order of x: none overlaps another and none starts left of 0 (the 640 px rig bar showed the clock
 * on top of "Workbench" and a negative x broke the rig's struct pack). */
static int bar_clean(const bar *b)
{
    int i, j;
    for (i = 0; i < b->n; i++) {
        if (b->seg[i].hidden)
            continue;
        if (b->seg[i].x < 0)
            return 0;
        for (j = i + 1; j < b->n; j++)
            if (!b->seg[j].hidden && b->seg[i].x < b->seg[j].x + b->seg[j].w && b->seg[j].x < b->seg[i].x + b->seg[i].w)
                return 0;
    }
    return 1;
}

static void test_narrow_bar_drops_right_segments_by_priority(void)
{
    bar b;
    int wide, i;
    int net, cpu, ram, ag, sys, clk, scr;
    static const int widths[] = { 640, 560, 480, 400 };
    bar_init(&b, 640, 4, 2);
    scr = bar_add(&b, BAR_SEG_SCREEN0, BAR_LEFT, "Workbench", 90);
    bar_add(&b, BAR_SEG_MORE, BAR_LEFT, "", 0);
    clk = bar_add(&b, BAR_SEG_CLOCK, BAR_CENTRE, "Thu 14:05", 90);
    cpu = bar_add(&b, BAR_SEG_CPU, BAR_RIGHT, "CPU 12%", 56);
    net = bar_add(&b, BAR_SEG_NET, BAR_RIGHT, "Net emu", 64);
    ram = bar_add(&b, BAR_SEG_RAM, BAR_RIGHT, "C 1.2M F 61M", 120);
    ag = bar_add(&b, BAR_SEG_AGENTS, BAR_RIGHT, "Agents", 48);
    sys = bar_add(&b, BAR_SEG_SYSTEM, BAR_RIGHT, "System", 48);
    /* plenty of room: nothing drops */
    bar_layout(&b, 900);
    CHECK_INT(b.seg[net].hidden, 0);
    CHECK_INT(bar_clean(&b), 1);
    /* 560 px: Network goes first, the rest stays */
    bar_layout(&b, 560);
    CHECK_INT(b.seg[net].hidden, 1);
    CHECK_INT(b.seg[net].w, 0);
    CHECK_INT(b.seg[cpu].hidden, 0);
    CHECK_INT(b.seg[ram].hidden, 0);
    CHECK_INT(bar_clean(&b), 1);
    CHECK_INT(bar_hit(&b, 0), scr);
    /* then CPU, then RAM; Clock, System and the screen never drop, nothing overlaps at any width */
    bar_layout(&b, 480);
    CHECK_INT(b.seg[net].hidden, 1);
    CHECK_INT(b.seg[cpu].hidden, 1);
    CHECK_INT(b.seg[ram].hidden, 0);
    CHECK_INT(bar_clean(&b), 1);
    bar_layout(&b, 400);
    CHECK_INT(b.seg[ram].hidden, 1);
    CHECK_INT(b.seg[clk].hidden, 0);
    CHECK_INT(b.seg[sys].hidden, 0);
    CHECK_INT(b.seg[scr].hidden, 0);
    for (i = 0; i < 4; i++) {
        bar_layout(&b, widths[i]);
        CHECK_INT(bar_clean(&b), 1);
    }
    /* the room comes back: everything is shown again */
    wide = 900;
    bar_layout(&b, wide);
    CHECK_INT(b.seg[net].hidden, 0);
    CHECK_INT(b.seg[ram].hidden, 0);
    CHECK_INT(b.seg[ag].hidden, 0);
    CHECK_INT(bar_clean(&b), 1);
}

/* Clock + System + the first screen entry do not fit (a long screen title on a narrow bar): the entry is clipped to
 * the room left, never drawn into the clock or System, and no segment starts left of 0. */
static void test_very_narrow_bar_clips_first_screen_without_overlap(void)
{
    bar b;
    int scr, clk, sys, w;
    bar_init(&b, 300, 4, 2);
    scr = bar_add(&b, BAR_SEG_SCREEN0, BAR_LEFT, "Workbench with a very long screen title", 200);
    bar_add(&b, BAR_SEG_MORE, BAR_LEFT, "", 0);
    clk = bar_add(&b, BAR_SEG_CLOCK, BAR_CENTRE, "Thu 14:05", 90);
    bar_add(&b, BAR_SEG_CPU, BAR_RIGHT, "CPU 12%", 56);
    bar_add(&b, BAR_SEG_RAM, BAR_RIGHT, "C 1.2M F 61M", 120);
    sys = bar_add(&b, BAR_SEG_SYSTEM, BAR_RIGHT, "System", 48);
    for (w = 300; w >= 220; w -= 20) {
        bar_layout(&b, w);
        CHECK_INT(bar_clean(&b), 1);
        CHECK_INT(b.seg[clk].hidden, 0);
        CHECK_INT(b.seg[sys].hidden, 0);
        CHECK_INT(b.seg[scr].hidden, 0);
        CHECK_INT(b.seg[scr].x + b.seg[scr].w <= b.seg[clk].x, 1);
    }
    bar_layout(&b, 300);
    CHECK_INT(b.seg[scr].w < 208, 1);                    /* clipped, not drawn at its full width */
    CHECK_INT(b.seg[scr].w > 0, 1);
}

/* Invariant: no two shown segment rects intersect at any width at which the centre group and the undroppable right
 * segments (System) fit side by side. Giving way, in order: right segments by rank (Network, CPU, RAM, Agents, then
 * the bar.conf tray segments), the
 * screens list down to one entry (+N), that entry clipped to the room left, and last that entry hidden when the
 * centre and right groups leave no room at all (the plan says the first screen never drops, but an overlap is worse;
 * the first screen stays at every width that leaves it any room). Sweep: every width from 320 to the minimum. */
static void test_bar_never_overlaps_across_width_sweep(void)
{
    bar b;
    int scr, clk, sys, w, minw, room_seen = 0, hidden_seen = 0;
    bar_init(&b, 320, 4, 2);
    scr = bar_add(&b, BAR_SEG_SCREEN0, BAR_LEFT, "Workbench with a very long screen title", 200);
    bar_add(&b, BAR_SEG_MORE, BAR_LEFT, "", 0);
    clk = bar_add(&b, BAR_SEG_CLOCK, BAR_CENTRE, "Thu 14:05", 90);
    bar_add(&b, BAR_SEG_CPU, BAR_RIGHT, "CPU 12%", 56);
    sys = bar_add(&b, BAR_SEG_SYSTEM, BAR_RIGHT, "System", 48);
    bar_add(&b, 310, BAR_RIGHT, "Tray one", 80);          /* a tray segment from a bar.conf run row */
    bar_layout(&b, 1000);
    minw = b.seg[clk].w + b.seg[sys].w + b.gap;                             /* clock + System side by side */
    for (w = 320; w >= minw; w--) {
        bar_layout(&b, w);
        CHECK_INT(bar_clean(&b), 1);
        CHECK_INT(b.seg[clk].hidden, 0);
        CHECK_INT(b.seg[sys].hidden, 0);
        if (b.seg[scr].hidden)
            hidden_seen++;
        else {
            room_seen++;
            CHECK_INT(b.seg[scr].x + b.seg[scr].w <= b.seg[clk].x, 1);
        }
    }
    CHECK(room_seen > 0);
    CHECK(hidden_seen > 0);                               /* the sweep reaches the no-room case */
}

/* AM-BAR-COLUMNS: every dynamic segment owns a fixed slot (its widest text). Whatever length one segment's text takes,
 * the other slots keep their x, nothing overlaps and nothing reaches past the screen width, at every width. The clock
 * slot holds the wider of the short and the long format (BAR-CLOCK-ALT-CLIP): the flipped text is never cut. */
static void test_bar_slots_keep_x_when_text_length_changes(void)
{
    static const char *txt[] = { "", "5%", "CPU 5%", "CPU 100%", "C 1.2M F 61M", "C 4096M F 4096M", "eth0 up", "Fri 30 Oct 2026", "Fri 00:00" };
    const int ids[] = { BAR_SEG_CLOCK, BAR_SEG_CPU, BAR_SEG_RAM, BAR_SEG_NET };
    const int slot[] = { 15 * 8, 8 * 8, 15 * 8, 10 * 8 };
    int w, a, v, i, j, ntxt = (int)(sizeof txt / sizeof txt[0]);
    for (w = 1000; w >= 760; w -= 40) {
        bar b;
        int base_x[4], idx[4];
        bar_init(&b, w, 4, 12);
        bar_add(&b, BAR_SEG_SCREEN0, BAR_LEFT, "Workbench", 72);
        bar_add(&b, BAR_SEG_MORE, BAR_LEFT, "", 0);
        idx[0] = bar_add(&b, BAR_SEG_CLOCK, BAR_CENTRE, "", 0);
        idx[1] = bar_add(&b, BAR_SEG_CPU, BAR_RIGHT, "", 0);
        idx[2] = bar_add(&b, BAR_SEG_RAM, BAR_RIGHT, "", 0);
        idx[3] = bar_add(&b, BAR_SEG_NET, BAR_RIGHT, "", 0);
        bar_add(&b, BAR_SEG_SYSTEM, BAR_RIGHT, "System", 48);
        for (i = 0; i < 4; i++)
            bar_set_slot(&b, ids[i], slot[i]);
        bar_layout(&b, w);
        for (i = 0; i < 4; i++)
            base_x[i] = b.seg[idx[i]].x;
        for (a = 0; a < 4; a++)
            for (v = 0; v < ntxt; v++) {
                int tw = (int)strlen(txt[v]) * 8;
                if (tw > slot[a])
                    continue;
                bar_set_text(&b, ids[a], txt[v], tw);
                bar_layout(&b, w);
                CHECK_INT(bar_clean(&b), 1);
                for (j = 0; j < b.n; j++)
                    CHECK_INT(b.seg[j].hidden || b.seg[j].x + b.seg[j].w <= w, 1);
                for (i = 0; i < 4; i++)
                    CHECK_INT(b.seg[idx[i]].x, base_x[i]);
            }
        CHECK_INT(b.seg[idx[0]].w, slot[0] + 8);        /* the clock slot is the long form's width plus padding */
    }
}

static void test_hit(void)
{
    bar b;
    int r;
    bar_init(&b, 640, 4, 2);
    bar_add(&b, 100, BAR_LEFT, "ws", 16);
    r = bar_add(&b, BAR_SEG_AGENTS, BAR_RIGHT, "Agents", 48);
    bar_layout(&b, 640);
    CHECK_INT(bar_hit(&b, 0), 0);
    CHECK_INT(bar_hit(&b, 23), 0);
    CHECK_INT(bar_hit(&b, 24), -1);                      /* past the segment: the gap */
    CHECK_INT(bar_hit(&b, 300), -1);
    CHECK_INT(bar_hit(&b, b.seg[r].x - 1), -1);
    CHECK_INT(bar_hit(&b, b.seg[r].x), r);
    CHECK_INT(bar_hit(&b, 639), r);
    CHECK_INT(bar_hit(&b, 640), -1);
    CHECK_INT(bar_hit(&b, -1), -1);
}

static void test_click_dispatch(void)
{
    bar b;
    int id = -1;
    bar_init(&b, 640, 4, 2);
    bar_add(&b, 100, BAR_LEFT, "ws", 16);
    bar_add(&b, BAR_SEG_AGENTS, BAR_RIGHT, "Agents", 48);
    bar_layout(&b, 640);
    CHECK_INT(bar_click(&b, 630, BAR_BTN_LEFT, &id), BAR_ACT_PANEL);
    CHECK_INT(id, BAR_SEG_AGENTS);
    CHECK_INT(bar_click(&b, 630, BAR_BTN_RIGHT, &id), BAR_ACT_LAUNCH_AGENT);
    CHECK_INT(bar_click(&b, 630, 9, &id), BAR_ACT_NONE);  /* another button */
    CHECK_INT(bar_click(&b, 5, BAR_BTN_LEFT, &id), BAR_ACT_NONE);   /* a segment with no action yet */
    CHECK_INT(id, 100);
    CHECK_INT(bar_click(&b, 300, BAR_BTN_LEFT, &id), BAR_ACT_NONE);
    CHECK_INT(id, 0);
}

static void test_dirty_only_changed(void)
{
    bar b;
    int i, drawn = 0;
    bar_init(&b, 640, 4, 2);
    bar_add(&b, 100, BAR_LEFT, "ws", 16);
    bar_add(&b, BAR_SEG_AGENTS, BAR_RIGHT, "Agents", 48);
    bar_layout(&b, 640);
    for (i = bar_take_dirty(&b, 0); i >= 0; i = bar_take_dirty(&b, i + 1))
        drawn++;
    CHECK_INT(drawn, 2);                                 /* first paint: everything */
    CHECK_INT(bar_take_dirty(&b, 0), -1);
    CHECK_INT(bar_set_text(&b, 100, "ws", 16), 0);       /* same value: no redraw */
    bar_layout(&b, 640);                                 /* same width: nothing moved */
    CHECK_INT(bar_take_dirty(&b, 0), -1);
    CHECK_INT(bar_set_text(&b, 100, "ws2", 24), 1);
    CHECK_INT(bar_take_dirty(&b, 0), 0);
    CHECK_INT(bar_take_dirty(&b, 0), -1);
    CHECK_INT(bar_set_text(&b, 999, "x", 8), 0);         /* unknown id */
}

/* The panel opens upward from the bar's right end: right edge on the screen's, bottom edge on the bar's top. */
static void test_panel_anchor(void)
{
    mg_font f = { 8, 8 };
    mg_screen s = { 640, 256, 22, 44, 0, 0, 0, 0 };
    mg_geom g;
    int ax, ay;
    bar_panel_anchor(640, 246, &ax, &ay);
    CHECK_INT(ax, 640);
    CHECK_INT(ay, 246);
    s.anchored = 1;
    s.ax = ax;
    s.ay = ay;
    mg_layout(&g, &f, 120, 6, &s);
    CHECK_INT(g.x + g.w, 640);
    CHECK_INT(g.y + g.h, 246);
    CHECK(g.y >= 0 && g.y < 246);
    /* unanchored stays centred */
    s.anchored = 0;
    mg_layout(&g, &f, 120, 6, &s);
    CHECK_INT(g.x, (640 - 120) / 2);
}

static void test_panel_anchor_clamped(void)
{
    mg_font f = { 8, 8 };
    mg_screen s = { 640, 256, 22, 44, 0, 1, 640, 246 };
    mg_geom g;
    mg_layout(&g, &f, 120, 40, &s);                      /* more rows than fit */
    CHECK(g.y >= 0);
    CHECK(g.y + g.h <= 246);
    s.ay = 20;                                           /* the bar's top is above the menu's height */
    mg_layout(&g, &f, 120, 6, &s);
    CHECK_INT(g.y, 0);
    s.ax = 50;                                           /* the right end is nearer the left edge than the width */
    s.ay = 246;
    mg_layout(&g, &f, 200, 6, &s);
    CHECK_INT(g.x, 0);
    s.ax = 900;                                          /* beyond the screen: the edge */
    mg_layout(&g, &f, 120, 6, &s);
    CHECK_INT(g.x + g.w, 640);
}

/* A right click on the Agents segment launches whatever window is active: the press is read from the input stream with
 * the pointer's screen position, so only the rectangle and the segment decide (bar_press_act). */
static void test_press_over_agents_launches(void)
{
    bar b;
    bar_init(&b, 640, 4, 2);
    bar_add(&b, BAR_SEG_AGENTS, BAR_RIGHT, "Agents", 48);
    bar_layout(&b, 640);
    CHECK_INT(bar_press_act(&b, 630, 250, 246, 10, BAR_BTN_RIGHT), BAR_ACT_LAUNCH_AGENT);
    CHECK_INT(bar_press_act(&b, 630, 250, 246, 10, BAR_BTN_LEFT), BAR_ACT_PANEL);
    CHECK_INT(bar_press_act(&b, 630, 245, 246, 10, BAR_BTN_RIGHT), BAR_ACT_NONE);   /* above the bar: the window under it */
    CHECK_INT(bar_press_act(&b, 630, 256, 246, 10, BAR_BTN_RIGHT), BAR_ACT_NONE);   /* below it */
    CHECK_INT(bar_press_act(&b, 100, 250, 246, 10, BAR_BTN_RIGHT), BAR_ACT_NONE);   /* in the bar, off the segment */
    CHECK_INT(bar_press_act(&b, 640, 250, 246, 10, BAR_BTN_RIGHT), BAR_ACT_NONE);   /* outside the width */
}

/* The re-raise decision: only a foreign BACKDROP layer in front that touches the bar's rectangle raises the bar.
 * A normal window over the bar (Directory Opus' lister, a full-size window with its size gadget) stays in front of
 * it (BAR-BEHIND); our own windows, layers clear of the bar and an empty list do not raise either. */
/* BAR-CLICK: the stock Workbench root window (WFLG_WBENCHWINDOW, plain layer, full width) sits in front of the bar's
 * backdrop layer; it must count as the desktop or the poll never raises the bar and Workbench takes every click. */
static void test_desktop(void)
{
    CHECK_INT(bar_is_desktop(0x2220327fUL, 0, 799, 800), 1);          /* rig: stock Workbench root window */
    CHECK_INT(bar_is_desktop(0x00000100UL, 0, 799, 800), 1);          /* WFLG_BACKDROP (Directory Opus) */
    CHECK_INT(bar_is_desktop(0x02000000UL, 40, 300, 800), 0);         /* a drawer window */
    CHECK_INT(bar_is_desktop(0x0000327fUL, 0, 799, 800), 0);          /* a normal full-width window */
}

static void test_covered(void)
{
    const bar_lyr wb_over = { 0, 16, 639, 255, 0, 1 };         /* Workbench's backdrop window over the bar */
    const bar_lyr wb_above = { 0, 16, 639, 245, 0, 1 };
    const bar_lyr wb_edge = { 0, 16, 639, 246, 0, 1 };         /* row 246 is the bar's top row */
    const bar_lyr win_over = { 100, 200, 400, 255, 0, 0 };     /* a normal window on the bar: no raise */
    const bar_lyr win_full = { 0, 16, 639, 255, 0, 0 };        /* full size, size gadget in the bar's rows */
    const bar_lyr panel = { 300, 100, 639, 245, 1, 0 };
    const bar_lyr menu_on_bar = { 0, 240, 639, 255, 1, 1 };
    bar_lyr list[3];
    CHECK_INT(bar_covered(0, 246, 639, 255, 0, 0), 0);
    CHECK_INT(bar_covered(0, 246, 639, 255, &wb_over, 1), 1);
    CHECK_INT(bar_covered(0, 246, 639, 255, &wb_above, 1), 0);
    CHECK_INT(bar_covered(0, 246, 639, 255, &wb_edge, 1), 1);
    CHECK_INT(bar_covered(0, 246, 639, 255, &win_over, 1), 0);
    CHECK_INT(bar_covered(0, 246, 639, 255, &win_full, 1), 0);
    CHECK_INT(bar_covered(0, 246, 639, 255, &panel, 1), 0);
    CHECK_INT(bar_covered(0, 246, 639, 255, &menu_on_bar, 1), 0);
    list[0] = panel;
    list[1] = win_over;
    list[2] = wb_over;          /* behind a normal window and still over the bar: raise (the poll runs on layers) */
    CHECK_INT(bar_covered(0, 246, 639, 255, list, 3), 1);
    CHECK_INT(bar_covered(0, 246, 639, 255, list, 2), 0);
}

/* The full bar of the plan (R2.4): screens left (tw 60 each), clock centre, CPU/RAM/network/Agents right. */
static int full_bar(bar *b, int nscr, int width, int clock_tw)
{
    int i;
    bar_init(b, width, 4, 2);
    for (i = 0; i < nscr; i++)
        bar_add(b, BAR_SEG_SCREEN0 + i, BAR_LEFT, "Scr", 60);
    bar_add(b, BAR_SEG_MORE, BAR_LEFT, "", 0);
    bar_add(b, 300, BAR_CENTRE, "Mon 14:05", clock_tw);
    bar_add(b, 301, BAR_RIGHT, "CPU", 40);
    bar_add(b, 302, BAR_RIGHT, "RAM", 100);
    bar_add(b, 303, BAR_RIGHT, "Net", 60);
    bar_add(b, BAR_SEG_AGENTS, BAR_RIGHT, "Agents", 48);
    bar_layout(b, width);
    return nscr;
}

static int shown(const bar *b)
{
    int i, n = 0;
    for (i = 0; i < b->n; i++)
        if (BAR_IS_SCREEN(b->seg[i].id) && !b->seg[i].hidden)
            n++;
    return n;
}

static const bar_seg *seg_id(const bar *b, int id)
{
    int i;
    for (i = 0; i < b->n; i++)
        if (b->seg[i].id == id)
            return &b->seg[i];
    return 0;
}

static void test_centre(void)
{
    bar b;
    const bar_seg *c;
    int w;
    for (w = 640; w <= 1280; w += (w == 640 ? 160 : 480)) {         /* 640, 800, 1280 */
        full_bar(&b, 2, w, 72);
        c = seg_id(&b, 300);
        CHECK_INT(shown(&b), 2);
        CHECK_INT(seg_id(&b, BAR_SEG_MORE)->w, 0);
        if (w == 1280)
            CHECK_INT(c->x, (1280 - c->w) / 2);                      /* centred when nothing touches it */
        CHECK(c->x >= seg_id(&b, BAR_SEG_SCREEN0 + 1)->x + seg_id(&b, BAR_SEG_SCREEN0 + 1)->w);
        CHECK(c->x + c->w <= seg_id(&b, 301)->x);
    }
    /* pushed right: 7 screens at 800 with a narrow clock cut to 6 + marker, which end past the ideal centre */
    full_bar(&b, 7, 800, 20);
    c = seg_id(&b, 300);
    CHECK_INT(shown(&b), 6);
    CHECK(c->x > (800 - c->w) / 2);
    CHECK_INT(c->x, seg_id(&b, BAR_SEG_MORE)->x + seg_id(&b, BAR_SEG_MORE)->w + 2);
}

static void test_cut(void)
{
    bar b;
    int n;
    /* 640, 6 screens: the room is right group start (346) - centre (80) - gap = 264 */
    full_bar(&b, 6, 640, 72);
    n = shown(&b);
    CHECK_INT(n, 3);
    CHECK_STR(seg_id(&b, BAR_SEG_MORE)->text, "+3");
    CHECK_INT(seg_id(&b, BAR_SEG_MORE)->w, 2 * 8 + 8);               /* "+3" at cw 8 plus padding */
    CHECK(seg_id(&b, BAR_SEG_MORE)->x == seg_id(&b, BAR_SEG_SCREEN0 + 2)->x + 68 + 2);
    CHECK_INT(bar_hit(&b, 3 * 70 + 30), -1);                         /* hidden entries are never hit */
    CHECK(seg_id(&b, BAR_SEG_SCREEN0 + 5)->hidden);
    CHECK_INT(seg_id(&b, 300)->w, 80);                               /* centre and right never cut */
    CHECK_INT(seg_id(&b, BAR_SEG_AGENTS)->x + seg_id(&b, BAR_SEG_AGENTS)->w, 640);
    /* everything fits: no cut, no marker */
    full_bar(&b, 3, 800, 72);
    CHECK_INT(shown(&b), 3);
    CHECK(seg_id(&b, BAR_SEG_MORE)->hidden);
    /* cut by exactly one entry: 5 screens at 640 do not fit (350), 4 do not (276 + marker), so test with a narrower clock */
    full_bar(&b, 5, 640, 20);
    CHECK_INT(shown(&b), 4);
    CHECK_STR(seg_id(&b, BAR_SEG_MORE)->text, "+1");
    /* all but one: a tiny width keeps the first entry and counts the rest (the tray segments 301..303 drop first) */
    full_bar(&b, 6, 200, 72);
    CHECK_INT(shown(&b), 1);
    CHECK_STR(seg_id(&b, BAR_SEG_MORE)->text, "+5");
    CHECK(!seg_id(&b, BAR_SEG_SCREEN0)->hidden);
    /* knob: the character width moves the marker */
    b.cw = 10;
    bar_layout(&b, 200);
    CHECK_INT(seg_id(&b, BAR_SEG_MORE)->w, 2 * 10 + 8);
    /* growing the bar again brings the entries back */
    bar_layout(&b, 1280);
    CHECK_INT(shown(&b), 6);
    CHECK(seg_id(&b, BAR_SEG_MORE)->hidden);
}

static void test_fmt_bytes(void)
{
    char o[16];
    bar_fmt_bytes(o, sizeof o, 0UL);               CHECK_STR(o, "0K");
    bar_fmt_bytes(o, sizeof o, 1023UL);            CHECK_STR(o, "1K");
    bar_fmt_bytes(o, sizeof o, 1024UL);            CHECK_STR(o, "1K");
    bar_fmt_bytes(o, sizeof o, 512UL * 1024UL);    CHECK_STR(o, "512K");
    bar_fmt_bytes(o, sizeof o, 1048575UL);         CHECK_STR(o, "1.0M");           /* rounds up into megabytes */
    bar_fmt_bytes(o, sizeof o, 1101004UL);         CHECK_STR(o, "1.0M");           /* 1.05 MB */
    bar_fmt_bytes(o, sizeof o, 1258291UL);         CHECK_STR(o, "1.2M");
    bar_fmt_bytes(o, sizeof o, 61UL * 1048576UL);  CHECK_STR(o, "61M");
    bar_fmt_bytes(o, sizeof o, 10UL * 1048576UL - 1UL); CHECK_STR(o, "10M");
    bar_fmt_bytes(o, sizeof o, 4294967295UL);      CHECK_STR(o, "4096M");          /* 4 GB - 1, no overflow */
    bar_fmt_bytes(o, 4, 61UL * 1048576UL);         CHECK_STR(o, "61M");
    bar_fmt_bytes(o, 3, 61UL * 1048576UL);         CHECK_STR(o, "61");             /* cap respected */
}

static void test_fmt_time(void)
{
    static const char *const day[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    char o[BAR_TEXT], want[BAR_TEXT];
    int d;
    for (d = 0; d < 7; d++) {
        bar_fmt_time(o, sizeof o, BAR_CLOCK_FMT, d, 29, 9, 2026, 14, 5);
        strcpy(want, day[d]);
        strcat(want, " 14:05");
        CHECK_STR(o, want);
    }
    bar_fmt_time(o, sizeof o, BAR_CLOCK_FMT, 1, 29, 9, 2026, 0, 0);           CHECK_STR(o, "Mon 00:00");   /* midnight */
    bar_fmt_time(o, sizeof o, BAR_CLOCK_FMT, 1, 29, 9, 2026, 23, 59);         CHECK_STR(o, "Mon 23:59");
    bar_fmt_time(o, sizeof o, BAR_DATE_FMT, 1, 29, 9, 2026, 14, 5);           CHECK_STR(o, "Mon 29 Oct 2026");
    bar_fmt_time(o, sizeof o, BAR_DATE_FMT, 4, 1, 0, 2027, 0, 0);             CHECK_STR(o, "Thu 01 Jan 2027");
    bar_fmt_time(o, sizeof o, "%H:%M", 1, 29, 9, 2026, 14, 5);                CHECK_STR(o, "14:05");        /* bar.conf clock knob */
    bar_fmt_time(o, sizeof o, "100%% %q %", 1, 29, 9, 2026, 14, 5);           CHECK_STR(o, "100% %q %");
    bar_fmt_time(o, 6, BAR_DATE_FMT, 1, 29, 9, 2026, 14, 5);                  CHECK_STR(o, "Mon 2");        /* cap */
}

static void test_fmt_misc(void)
{
    char o[BAR_TEXT];
    bar_fmt_ram(o, sizeof o, 1258291UL, 61UL * 1048576UL);
    CHECK_STR(o, "C 1.2M F 61M");
    bar_fmt_ram(o, sizeof o, 0UL, 4294967295UL);
    CHECK_STR(o, "C 0K F 4096M");
    CHECK_INT(bar_fmt_percent(100, 100), 0);
    CHECK_INT(bar_fmt_percent(0, 100), 100);
    CHECK_INT(bar_fmt_percent(75, 100), 25);
    CHECK_INT(bar_fmt_percent(0, 0), 0);
    CHECK_INT(bar_fmt_percent(5, 3), 0);
    bar_fmt_screen(o, sizeof o, "Workbench Screen");     CHECK_STR(o, "Workbench");          /* first word whole */
    bar_fmt_screen(o, sizeof o, "UP-Term");              CHECK_STR(o, "UP-Term");
    bar_fmt_screen(o, sizeof o, "  Mui screen");         CHECK_STR(o, "Mui");
    bar_fmt_screen(o, sizeof o, "");                     CHECK_STR(o, "");
    bar_fmt_screen(o, sizeof o, "Amarchy-Workspace-2");  CHECK_STR(o, "Amarchy-Workspace-2");   /* no 8 character cut */
    bar_fmt_screen(o, 6, "Workbench");                   CHECK_STR(o, "Workb");                 /* cap */
    CHECK_INT(bar_cpu_pct(0, 50, 1000000UL), 100);
    CHECK_INT(bar_cpu_pct(50000000UL, 50, 1000000UL), 0);
    CHECK_INT(bar_cpu_pct(25000000UL, 50, 1000000UL), 50);
    CHECK_INT(bar_cpu_pct(4000000000UL, 50, 1000000UL), 0);           /* more than calibrated: idle */
    CHECK_INT(bar_cpu_pct(37500000UL, 50, 1000000UL), 25);
    CHECK_INT(bar_cpu_pct(100, 0, 1000000UL), 0);
    CHECK_INT(bar_cpu_pct(100, 50, 0), 0);
    CHECK_INT(bar_cpu_pct(0, 99999UL, 2000000UL), 100);               /* clamped, no overflow */
    CHECK_INT((int)bar_cpu_cal(0, 50000000UL, 50), 1000000);
    CHECK_INT((int)bar_cpu_cal(1000000UL, 20000000UL, 50), 1000000);  /* a busy sample never lowers it */
    CHECK_INT((int)bar_cpu_cal(1000000UL, 60000000UL, 50), 1200000);
    CHECK_INT((int)bar_cpu_cal(7UL, 5, 0), 7);
    bar_fmt_cpu(o, sizeof o, 12);   CHECK_STR(o, "CPU 12%");
    bar_fmt_cpu(o, sizeof o, 0);    CHECK_STR(o, "CPU 0%");
    bar_fmt_cpu(o, sizeof o, 100);  CHECK_STR(o, "CPU 100%");
    bar_fmt_cpu(o, sizeof o, 250);  CHECK_STR(o, "CPU 100%");
    bar_fmt_cpu(o, sizeof o, -3);   CHECK_STR(o, "CPU 0%");
    CHECK_INT(bar_net_kind("UAE bsdsocket.library 4.1"), BAR_NET_EMULATED);
    CHECK_INT(bar_net_kind("bsdsocket.library 4.364 (1.1.2019)"), BAR_NET_STACK);
    CHECK_INT(bar_net_kind(""), BAR_NET_STACK);
    CHECK_INT(bar_net_kind(0), BAR_NET_STACK);
    bar_fmt_net(o, sizeof o, BAR_NET_EMULATED, 0);  CHECK_STR(o, "Network emulated");
    bar_fmt_net(o, sizeof o, BAR_NET_NONE, 0);      CHECK_STR(o, "No network");
    bar_fmt_net(o, sizeof o, BAR_NET_STACK, "eth0"); CHECK_STR(o, "No network");   /* a stack with no answering interface */
    /* R2.10: IFQ_State (SM_Offline 0, SM_Online 1, SM_Down 2, SM_Up 3) to the segment text */
    CHECK_INT(bar_net_state_kind(3), BAR_NET_UP);
    CHECK_INT(bar_net_state_kind(1), BAR_NET_UP);
    CHECK_INT(bar_net_state_kind(2), BAR_NET_DOWN);
    CHECK_INT(bar_net_state_kind(0), BAR_NET_DOWN);
    CHECK_INT(bar_net_state_kind(-1), BAR_NET_STACK);
    CHECK_INT(bar_net_state_kind(9), BAR_NET_STACK);
    bar_fmt_net(o, sizeof o, bar_net_state_kind(3), "eth0");  CHECK_STR(o, "eth0 up");
    bar_fmt_net(o, sizeof o, bar_net_state_kind(2), "eth0");  CHECK_STR(o, "eth0 down");
    bar_fmt_net(o, sizeof o, BAR_NET_UP, 0);                  CHECK_STR(o, "Network up");
    bar_fmt_net(o, sizeof o, BAR_NET_DOWN, "averyveryverylongInterfaceName0"); CHECK(strlen(o) < BAR_TEXT && strstr(o, " down"));
}

static int x_of(const bar *b, int id)
{
    int i;
    for (i = 0; i < b->n; i++)
        if (b->seg[i].id == id)
            return b->seg[i].x + 1;
    return -1;
}

static void test_tray_click(void)
{
    bar b;
    int id;
    bar_init(&b, 400, 4, 4);
    bar_add(&b, BAR_SEG_SYSTEM, BAR_RIGHT, "System", 48);
    bar_add(&b, BAR_SEG_TRAY0, BAR_RIGHT, "Files", 40);
    bar_add(&b, BAR_SEG_TRAY0 + 1, BAR_RIGHT, "Edit", 32);
    bar_layout(&b, 400);
    CHECK_INT(bar_click(&b, x_of(&b, BAR_SEG_TRAY0 + 1), BAR_BTN_LEFT, &id), BAR_ACT_RUN);
    CHECK_INT(id, BAR_SEG_TRAY0 + 1);
    CHECK_INT(bar_click(&b, x_of(&b, BAR_SEG_TRAY0), BAR_BTN_RIGHT, &id), BAR_ACT_NONE);
    CHECK_INT(bar_click(&b, x_of(&b, BAR_SEG_TRAY0), BAR_BTN_LEFT, &id), BAR_ACT_RUN);
    CHECK_INT(id, BAR_SEG_TRAY0);
}

static void test_clock_system_screens(void)
{
    bar b;
    char o[160];
    int wd, d, m, y, id, tw3[3] = {40, 24, 48}, tw1[1] = {40};
    const char *n3[3] = {"Workbenc", "UP-Term", "Ambient"}, *n1[1] = {"Workbenc"};
    bar_days_to_date(17833L, &wd, &d, &m, &y);
    CHECK_INT(wd, 4); CHECK_INT(d, 29); CHECK_INT(m, 9); CHECK_INT(y, 2026);          /* Thu 29 Oct 2026 */
    bar_days_to_date(16860L, &wd, &d, &m, &y);
    CHECK_INT(d, 29); CHECK_INT(m, 1); CHECK_INT(y, 2024);                             /* leap day */
    bar_days_to_date(0L, &wd, &d, &m, &y);
    CHECK_INT(wd, 0); CHECK_INT(d, 1); CHECK_INT(m, 0); CHECK_INT(y, 1978);
    bar_init(&b, 800, 4, 2);
    bar_add(&b, BAR_SEG_MORE, BAR_LEFT, "", 0);
    bar_add(&b, BAR_SEG_CLOCK, BAR_CENTRE, "Mon 14:05", 72);
    bar_add(&b, BAR_SEG_RAM, BAR_RIGHT, "C 1.2M F 61M", 96);
    bar_add(&b, BAR_SEG_SYSTEM, BAR_RIGHT, "System", 48);
    CHECK_INT(bar_set_screens(&b, 3, n3, tw3, 1), 1);
    CHECK_INT(bar_set_screens(&b, 3, n3, tw3, 1), 0);                                  /* same list: nothing to redraw */
    CHECK_INT(b.seg[b.n - 1].id, BAR_SEG_MORE);                                         /* marker last */
    bar_layout(&b, 800);
    bar_dump(&b, o, sizeof o);
    CHECK(strstr(o, "201@") && strstr(o, "*=UP-Term"));                                 /* accent on the front screen only */
    CHECK_INT(bar_set_screens(&b, 3, n3, tw3, 2), 1);                                   /* front moves */
    bar_dump(&b, o, sizeof o);
    CHECK(strstr(o, "*=Ambient") && !strstr(o, "*=UP-Term"));
    CHECK_INT(bar_set_screens(&b, 1, n1, tw1, 0), 1);                                   /* two screens closed */
    CHECK_INT(b.n, 5);
    bar_dump(&b, o, sizeof o);
    CHECK(!strstr(o, "UP-Term") && strstr(o, "Workbenc"));
    bar_layout(&b, 800);
    /* clicks: system opens the menu, clock flips, a screen entry fronts that screen, right buttons do nothing */
    CHECK_INT(bar_click(&b, x_of(&b, BAR_SEG_SYSTEM), BAR_BTN_LEFT, &id), BAR_ACT_SYSTEM);
    CHECK_INT(id, BAR_SEG_SYSTEM);
    CHECK_INT(bar_click(&b, x_of(&b, BAR_SEG_SYSTEM), BAR_BTN_RIGHT, &id), BAR_ACT_NONE);
    CHECK_INT(bar_click(&b, x_of(&b, BAR_SEG_CLOCK), BAR_BTN_LEFT, &id), BAR_ACT_CLOCK);
    CHECK_INT(bar_click(&b, x_of(&b, BAR_SEG_CLOCK), BAR_BTN_RIGHT, &id), BAR_ACT_NONE);
    CHECK_INT(bar_click(&b, x_of(&b, BAR_SEG_SCREEN0), BAR_BTN_LEFT, &id), BAR_ACT_SCREEN);
    CHECK_INT(id, BAR_SEG_SCREEN0);
    CHECK_INT(bar_click(&b, x_of(&b, BAR_SEG_RAM), BAR_BTN_LEFT, &id), BAR_ACT_NONE);         /* RAM has no action */
}

/* A left click on the network segment opens the "network" panel: bar_click gives BAR_ACT_NET, bar_act_node names the node, and
 * the opener (bar.c act_panel) is reached with it. The counter proves the panel path ran; the right button does nothing. */
static int panels_opened;
static char panel_node[24];
static void open_panel(const char *node) { panels_opened++; strcpy(panel_node, node); }

static void test_bar_net_click_opens_panel(void)
{
    bar b;
    char rows[BAR_NET_ROWS][BAR_TEXT], cmd[64];
    int id = -1, act;
    const char *node;
    bar_init(&b, 800, 4, 2);
    bar_add(&b, BAR_SEG_NET, BAR_RIGHT, "replayeth up", 96);
    bar_layout(&b, 800);
    panels_opened = 0;
    act = bar_click(&b, x_of(&b, BAR_SEG_NET), BAR_BTN_LEFT, &id);
    CHECK_INT(act, BAR_ACT_NET);
    CHECK_INT(id, BAR_SEG_NET);
    if ((node = bar_act_node(act)) != 0)               /* the same two lines as bar.c's click switch */
        open_panel(node);
    CHECK_INT(panels_opened, 1);
    CHECK_STR(panel_node, "network");
    CHECK_INT(bar_click(&b, x_of(&b, BAR_SEG_NET), BAR_BTN_RIGHT, &id), BAR_ACT_NONE);
    CHECK(bar_act_node(BAR_ACT_NONE) == 0 && bar_act_node(BAR_ACT_CLOCK) == 0);
    CHECK_STR(bar_act_node(BAR_ACT_PANEL), "agents");
    CHECK_STR(bar_act_node(BAR_ACT_SYSTEM), "system");
    /* the panel's rows */
    CHECK_INT(bar_net_rows(rows, BAR_NET_ROWS, BAR_NET_UP, "replayeth", 0xC0A8004AUL), 3);
    CHECK_STR(rows[0], "Interface: replayeth");
    CHECK_STR(rows[1], "Address: 192.168.0.74");
    CHECK_STR(rows[2], "State: up");
    bar_net_rows(rows, BAR_NET_ROWS, BAR_NET_DOWN, "replayeth", 0);
    CHECK_STR(rows[1], "Address: none");
    CHECK_STR(rows[2], "State: down");
    bar_net_rows(rows, BAR_NET_ROWS, BAR_NET_STACK, "", 0);
    CHECK_STR(rows[2], "State: no network");
    /* the Up and Down command */
    CHECK_INT(bar_net_cmd(cmd, sizeof cmd, "replayeth", 0), 1);
    CHECK_STR(cmd, "ConfigureNetInterface replayeth DOWN");
    CHECK_INT(bar_net_cmd(cmd, sizeof cmd, "replayeth", 1), 1);
    CHECK_STR(cmd, "ConfigureNetInterface replayeth UP");
    CHECK_INT(bar_net_cmd(cmd, sizeof cmd, "", 1), 0);
    CHECK_INT(bar_net_cmd(cmd, sizeof cmd, "a;b", 1), 0);
    CHECK_INT(bar_net_cmd(cmd, 10, "replayeth", 1), 0);
}

void suite_bar(void)
{
    test_layout_groups();
    test_narrow_bar_drops_right_segments_by_priority();
    test_very_narrow_bar_clips_first_screen_without_overlap();
    test_bar_never_overlaps_across_width_sweep();
    test_bar_slots_keep_x_when_text_length_changes();
    test_hit();
    test_click_dispatch();
    test_dirty_only_changed();
    test_panel_anchor();
    test_panel_anchor_clamped();
    test_press_over_agents_launches();
    test_desktop();
    test_covered();
    test_centre();
    test_cut();
    test_fmt_bytes();
    test_fmt_time();
    test_fmt_misc();
    test_clock_system_screens();
    test_tray_click();
    test_bar_net_click_opens_panel();
}
